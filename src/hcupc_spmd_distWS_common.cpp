/* Copyright (c) 2015, Rice University

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

1.  Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
2.  Redistributions in binary form must reproduce the above
     copyright notice, this list of conditions and the following
     disclaimer in the documentation and/or other materials provided
     with the distribution.
3.  Neither the name of Rice University
     nor the names of its contributors may be used to endorse or
     promote products derived from this software without specific
     prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 */

/*
 * hcupc_spmd_distWS_common.cpp
 *
 *      Author: Vivek Kumar (vivekk@rice.edu)
 */

#include "hcupc_spmd-internal.h"
#include "hcupc_spmd_common_methods.h"
#include "hcupc_spmd-commTask.h"
#include "hcupc_spmd-atomics.h"
#include <cmath>

/**
 * Two different types of distributed work-stealing algorithms are available
 * a) BaselineWS and b) SuccessOnlyWS
 *
 * BaselineWS involves inter-place failed steal attempts, extra idle time, extra inter-place messages. SuccessOnlyWS
 * removes inter-place failed steals, reduces idle time and also reduces total inter-place message exchanges
 *
 * To use BaselineWS set this environment varialbe before launching the runtime:
 * export HCPP_DIST_WS_BASELINE=1
 * SuccessOnlyWS is the default implementation
 */

/*
 * ------- Runtime control flags -------- Start
 */

/*
 * Available options:
 * 		__VS_RR__		:	HabaneroUPC++ Distributed Workstealing (Round Robin)
 * 	 	__VS_RAND__		:	HabaneroUPC++ Distributed Workstealing (Rand)
 * 	 	__VS_DISTANCE__	:	HabaneroUPC++ Distributed Workstealing (Edison Euclidian Distance)
 * 	 	__VS_DIST_HPT__ :	HabaneroUPC++ Distributed Workstealing (Distributed HPT)
 *
 */
#define __VS_RAND__

/*
 * ------- Runtime control flags -------- End
 */

namespace hupcpp {
static int last_steal;	// place id of last victim
}

#include "hcupc_spmd-random.h"

//#define HCPP_DEBUG

/*
 * End ------- Runtime control flags --------
 */

namespace hupcpp {

const static char* cyclic_steals_not_allowed = getenv("HCPP_CANCEL_CYCLIC_STEALS");
extern volatile int idle_workers;	// counts computation workers only

double totalTasksStolenInOneShot = -1; // may be fraction as well (getenv)

/*
 * GLB Specific
 */
int glb_max_rand_attempts = 1;
int current_glb_max_rand_attempts = 0;

static int* contacted_victims;
static int* queued_thieves;
static int task_transfer_threshold;
static int head=-1, tail=-1;
static bool thieves_waiting = false;
static bool out_of_work = false;	// only for statistics
#define QUEUED_ENTRY	-3
#define POPPED_ENTRY	-4

static int* received_tasks_origin;
static int head_rto=0;	//rto=received tasks origin
static int tasks_received = 0;

upcxx::shared_array<int> workAvail;
upcxx::shared_array<bool> waitForTaskFromVictim;

upcxx::shared_array<upcxx::shared_lock> reqLock;
upcxx::shared_array<int> req_thread;

upcxx::shared_array<upcxx::shared_lock> asyncsInFlightCountLock;
upcxx::shared_array<int> asyncsInFlight;

#define MAX_ASYNC_ANY_TASKS_SHIPMENT 5

#define REQ_AVAILABLE	-1	 /* req_thread */
#define REQ_UNAVAILABLE  -2  /* req_thread */
#define NOT_WORKING      -1  /* work_available */
#define NONE_WORKING     -2  /* result of probe sequence */

#define SUCCESS_STEAL 0
#define FAILED_STEAL 1

#define LOCK_REQ(i)			 			reqLock[i].get().lock()
#define UNLOCK_REQ(i)		 			reqLock[i].get().unlock()
#define LOCK_REQ_SELF		 			(&reqLock[upcxx::global_myrank()])->lock()
#define UNLOCK_REQ_SELF		 			(&reqLock[upcxx::global_myrank()])->unlock()

#define INCREMENT_TASK_IN_FLIGHT_SELF   {(&asyncsInFlightCountLock[upcxx::global_myrank()])->lock(); asyncsInFlight[upcxx::global_myrank()] = asyncsInFlight[upcxx::global_myrank()]+1;   (&asyncsInFlightCountLock[upcxx::global_myrank()])->unlock(); }
#define DECREMENT_TASK_IN_FLIGHT(i)     {asyncsInFlightCountLock[i].get().lock(); asyncsInFlight[i] = asyncsInFlight[i]-1;      asyncsInFlightCountLock[i].get().unlock(); }

#define ALLOC_ARRAY_SHARED_LOCK(x) { new (x[upcxx::global_myrank()].raw_ptr()) upcxx::shared_lock(upcxx::global_myrank()); }

/*
 * victim uses to this to publish its load info in global address space
 * to help thief in: a) finding correct victim; and b) termination detection.
 */
void publish_local_load_info() {
#ifdef DIST_WS
	const int total_aany = hcpp::totalAsyncAnyAvailable();
	workAvail[upcxx::global_myrank()] = (total_aany>0) ? total_aany : totalPendingLocalAsyncs();
#else
	workAvail[upcxx::global_myrank()] = totalPendingLocalAsyncs();
#endif
}

inline void mark_myPlace_asBusy() {
	const int me = upcxx::global_myrank();
	// don't need to do anything if I've already declared that I'm busy
	if(!out_of_work) return;

	out_of_work = false;
	publish_local_load_info();

	if(baseline_distWS || glb_distWS) {
		current_glb_max_rand_attempts = 0; // glb only
		LOCK_REQ_SELF;
		assert(req_thread[me] == REQ_UNAVAILABLE);
		req_thread[me] = REQ_AVAILABLE;
		UNLOCK_REQ_SELF;
	}
}

/*
 * Used only at the source. This is used only when asyncCopy is invoked
 */

void increment_task_in_flight_self() {
	INCREMENT_TASK_IN_FLIGHT_SELF;
}

inline void decrement_task_in_flight_self(int tasks) {
	(&asyncsInFlightCountLock[upcxx::global_myrank()])->lock();
	asyncsInFlight[upcxx::global_myrank()] = asyncsInFlight[upcxx::global_myrank()]-tasks;
	(&asyncsInFlightCountLock[upcxx::global_myrank()])->unlock();
}

/*
 * Used only at the destination
 */
inline void decrement_task_in_flight(int place, int tasks) {
	asyncsInFlightCountLock[place].get().lock();
	asyncsInFlight[place] = asyncsInFlight[place]-tasks;
	asyncsInFlightCountLock[place].get().unlock();
}

inline void mark_victimContacted(int v) {
	HASSERT(contacted_victims[v] == POPPED_ENTRY);
	contacted_victims[v] = QUEUED_ENTRY;
	bool ret = check_cyclicSteals(v, head, tail, queued_thieves);
	if (cyclic_steals_not_allowed && ret) {
		// This victim is already queued as Thief with me.
		// Remove this victim from queued_thief list
		int queued_thieves_tmp[upcxx::global_ranks()];
		int index=-1;
		while(head!=tail) {
			int t = queued_thieves[++head % upcxx::global_ranks()];
           		if(v!=t) {
                        	queued_thieves_tmp[++index] = t;
            		}
        	}
		head=-1;
		tail=index;
#ifdef HCPP_DEBUG
			cout <<upcxx::global_myrank() << ": Cyclic steals, queued_thieves -->" << v << " new head= "<< head << " tail = " << tail << endl;
#endif
		if(tail != -1) {
			memcpy(queued_thieves, queued_thieves_tmp, sizeof(int)*upcxx::global_ranks());
		}
		else thieves_waiting = false;
	}
}

inline void unmark_victimContacted(int v) {
	HASSERT(contacted_victims[v] == QUEUED_ENTRY);
	contacted_victims[v] = POPPED_ENTRY;
}

inline bool victim_already_contacted(int v) {
	return (contacted_victims[v] == QUEUED_ENTRY);
}

/*
 * "thief_queue":
 * 		Operated only by victims. They use this to queue ids of thieves
 * 		from whome they have received steal requests.
 */
inline bool full_thief_queue() {
	return ((tail - upcxx::global_ranks()) == head);
}

inline int size_thief_queue() {
	return std::abs(head - tail);
}

inline bool empty_thief_queue() {
	if(head==tail) {
                head = tail = -1;
                return true;
        }
        else return false;
}

/*
 * Victim will queue the thief id for every steal request.
 */
inline void queue_thief(int i) {
	HASSERT(!full_thief_queue());
	thieves_waiting = true;
	check_if_out_of_work_stats(out_of_work);
	queued_thieves[++tail % upcxx::global_ranks()] = i;
	// Check if I owe task from this thief, i.e., if this
	// thief is queued as one my victims
	if(cyclic_steals_not_allowed && victim_already_contacted(i)) {
#ifdef HCPP_DEBUG
			cout <<upcxx::global_myrank() << ": Cyclic steals, unmark_victimContacted -->" << i << endl;
#endif
		unmark_victimContacted(i);
	}
}

inline int pop_thief() {
	if(empty_thief_queue()) {
		HASSERT(!thieves_waiting);
		return -1;
	}
	const int p = queued_thieves[++head % upcxx::global_ranks()];
	HASSERT(p>=0 && p<upcxx::global_ranks());
	if(empty_thief_queue()) {
		thieves_waiting=false;
	}
	return p;
}

/*
 * "victim_queue":
 * 		Operated only by thief. Thief queues entry of victim_ids from whom
 * 		it has received tasks. These id are popped after upcxx::advance() and
 * 		tasks in flight counter at victim is decremented.
 */
void queue_source_place_of_remoteTask(int i) {
	out_of_work = false;	// ==> task received from victim
	tasks_received++;
	received_tasks_origin[i]++;
}

inline void pop_source_place_of_remoteTask(int* victim, int* tasks) {
	HASSERT(head_rto < upcxx::global_ranks());
	const int index = head_rto++;
	*victim = index;
	*tasks = received_tasks_origin[index];
	received_tasks_origin[index] = 0; // reset
	if(head_rto == upcxx::global_ranks()) {
		head_rto = 0; // reset
	}
	tasks_received -= *tasks;
}

/*
 * UPC++ place uses this to decrement outgoing task counter
 * at source place.
 */
void decrement_tasks_in_flight_count() {
	while(tasks_received > 0) {
		int victim, tasks;
		pop_source_place_of_remoteTask(&victim, &tasks);
		if(tasks > 0) {
			if(victim != upcxx::global_myrank()) {
				decrement_task_in_flight(victim, tasks);
			}
			else {
				decrement_task_in_flight_self(tasks);
			}
		}
	}
}

bool received_tasks_from_victim() {
	return (tasks_received>0);
}

/*
 * Distributed runtime initialization.
 */
void initialize_distws_setOfThieves() {
	if(getenv("HCPP_STEAL_N") != NULL) {
		totalTasksStolenInOneShot = std::stod(getenv("HCPP_STEAL_N"));
		HASSERT(totalTasksStolenInOneShot > 0);
		if(totalTasksStolenInOneShot>1) {
			HASSERT((totalTasksStolenInOneShot-(int)totalTasksStolenInOneShot) == 0);       // do not want something like 1.5
		}
	}
	else {
		totalTasksStolenInOneShot = 1;
	}

	task_transfer_threshold = 2 * numWorkers();	// heuristics

	if(upcxx::global_myrank() == 0) {
#ifdef DIST_WS
		cout << "---------DIST_WS_RUNTIME_INFO-----------" << endl;
		bool baseline = true;
		if(successonly_distWS) {
			printf(">>> HCPP_DIST_WS_SUCCESSONLY\t\t= true\n");
			baseline = false;
		}

		if(successonly_glb_distWS) {
			printf(">>> HCPP_DIST_WS_SUCCESSONLY_GLB\t\t= true\n");
			baseline = false;
		}

		if(glb_distWS) {
			printf(">>> HCPP_DIST_WS_GLB\t\t= true\n");
			assert(baseline && "Error.. Cannot enable both HCPP_DIST_WS_SUCCESSONLY and HCPP_DIST_WS_GLB");
			baseline = false;
			if(getenv("HCPP_DIST_WS_GLB_RAND")) {
				glb_max_rand_attempts = atoi(getenv("HCPP_DIST_WS_GLB_RAND"));
				assert(glb_max_rand_attempts <= (upcxx::global_ranks()-1) && "Error: HCPP_DIST_WS_GLB_RAND should be less than total number of Places");
				printf(">>> HCPP_DIST_WS_GLB_RAND\t\t= %d\n",glb_max_rand_attempts);
			}
		}

		if(baseline) {
			printf(">>> HCPP_DIST_WS_BASELINE\t\t= true\n");
		}

		if(getenv("HCPP_CANCEL_CYCLIC_STEALS")) {
			printf(">>> HCPP_CANCEL_CYCLIC_STEALS\t\t= true\n");
		}

		if(totalTasksStolenInOneShot < 1) {
			printf("WARNING (HCPP_STEAL_N): N should always be positive integer, setting it as 1\n");
			totalTasksStolenInOneShot = 1;
		}
		printf(">>> HCPP_STEAL_N\t\t= %f\n",totalTasksStolenInOneShot);
		printf(">>> %s\n",vsdescript());
		cout << "----------------------------------------" << endl;
#endif
		hcpp::display_runtime();
	}
	assert(totalTasksStolenInOneShot <= 5);
	stats_initTimelineEvents();
#ifdef DIST_WS
	// initialize random victim selection routines
	initialize_last_stolen();
	vsinit();
#endif

	workAvail.init(upcxx::global_ranks());
	waitForTaskFromVictim.init(upcxx::global_ranks());

	asyncsInFlight.init(upcxx::global_ranks());
	asyncsInFlightCountLock.init(upcxx::global_ranks());
	ALLOC_ARRAY_SHARED_LOCK(asyncsInFlightCountLock);

	req_thread.init(upcxx::global_ranks());
	reqLock.init(upcxx::global_ranks());
	ALLOC_ARRAY_SHARED_LOCK(reqLock);

	workAvail[upcxx::global_myrank()] = NOT_WORKING;
	req_thread[upcxx::global_myrank()] = REQ_UNAVAILABLE;
	asyncsInFlight[upcxx::global_myrank()] = 0;
	waitForTaskFromVictim[upcxx::global_myrank()] = false;

	contacted_victims = new int[upcxx::global_ranks()];
	queued_thieves = new int[upcxx::global_ranks()];
	received_tasks_origin = new int[upcxx::global_ranks()];
	for(int i=0; i<upcxx::global_ranks(); i++) {
		contacted_victims[i] = POPPED_ENTRY;
		queued_thieves[i] = POPPED_ENTRY;
		received_tasks_origin[i] = 0;
	}
}

/*
 * true if neighboring thread to the right is working
 */
int detectWork() {
	const int neighbor = (upcxx::global_myrank() + 1) % upcxx::global_ranks();
	return (NOT_WORKING != workAvail[neighbor]);
}

int total_asyncs_inFlight() {
	int pending_asyncs = 0;
	for(int i=0; i<upcxx::global_ranks(); i++) pending_asyncs += asyncsInFlight[i];
	return pending_asyncs;
}

/*
 * Runs at thief when victim sends asyncAny task to thief
 */
template <typename T>
void asyncAny_destination(upcxx::global_ptr<T> remote_lambda) {
	upcxx::global_ptr<T> my_lambda = upcxx::allocate<T>(upcxx::global_myrank(), 1);
	upcxx::copy(remote_lambda, my_lambda, 1);

	(*((T*)(my_lambda.raw_ptr())))();

	upcxx::deallocate(my_lambda);
	upcxx::deallocate(remote_lambda);
}

template <typename T>
void launch_upcxx_async(T* lambda, int dest) {
	upcxx::global_ptr<T> remote_lambda = upcxx::allocate<T>(upcxx::global_myrank(), 1);
	memcpy(remote_lambda.raw_ptr(), lambda, sizeof(T));
	upcxx::async(dest,NULL)(asyncAny_destination<T>, remote_lambda);
}

/*
 * Communication worker uses this routine to steal asyncAny from its
 * companion computation workers and then send them to remote thief
 */
inline hcpp::remoteAsyncAny_task* steal_from_my_computation_workers(int *count) {
	hcpp::remoteAsyncAny_task* tasks = (hcpp::remoteAsyncAny_task*) malloc(sizeof(hcpp::remoteAsyncAny_task) * MAX_ASYNC_ANY_TASKS_SHIPMENT);
	int i = 0;
	for( ; i<totalTasksStolenInOneShot; i++) {
		// Steal task from computation workers
		if(i>0 && idle_workers) break;
		bool success = hcpp::steal_fromComputeWorkers_forDistWS(&(tasks[i]));
		if(!success) break;	// never return from here as it will be bug in case HCPP_STEAL_N>1
		increment_outgoing_tasks();
	}

	if(i >0) {
		/*
		 * Now that the tasks are stolen from the computation workers, these tasks would be sent to
		 * remote thief for sure. This would be done by using one single upcxx::async and hence
		 * simply increment outgoing task counter
		 */
		INCREMENT_TASK_IN_FLIGHT_SELF;
	}

	*count = i;
	return tasks;
}

/*
 * Communication worker uses this routine to ship asyncAny tasks to a remote thief
 */
inline void ship_asyncAny_to_remote_thief(hcpp::remoteAsyncAny_task* asyncs, int count, int thief, bool baselineWS) {
	success_steals_stats(baselineWS);
	const int source = upcxx::global_myrank();
	hcpp::remoteAsyncAny_task tasks[MAX_ASYNC_ANY_TASKS_SHIPMENT];
	assert(count <= MAX_ASYNC_ANY_TASKS_SHIPMENT && "HCPP_STEAL_N doesn't mismatch");
	memcpy(tasks, asyncs, count*sizeof(hcpp::remoteAsyncAny_task));
	free(asyncs);

#ifdef HCPP_DEBUG
	cout <<upcxx::global_myrank() << ": (baseline=" << baselineWS << ") Sending " << count << " tasks to " << thief << endl;
#endif

	auto lambda_for_thief = [tasks, count, source, baselineWS]() {
		const int dest = upcxx::global_myrank();
		assert(source != dest);
		unwrap_n_asyncAny_tasks(tasks, count);
		if(baselineWS) {
			DECREMENT_TASK_IN_FLIGHT(source);
			// this flag is only modified by thief, but victim can also mark false if its out of work
			waitForTaskFromVictim[dest] = false;
		}
		else {
			queue_source_place_of_remoteTask(source);
			unmark_victimContacted(source);
		}
	};

	launch_upcxx_async<decltype(lambda_for_thief)>(&lambda_for_thief, thief);
	if(baselineWS) {
		req_thread[upcxx::global_myrank()] = REQ_AVAILABLE;
	}
	upcxx::advance();
}

/*
 * Victim function.
 * Victim pops pending steal requests. If its able to steal even
 * one asyncAny task from its computation worker, it will pop
 * a pending remote steal request and send asyncAny using upcxx::async.
 * At the end it will publish its local load.
 */
inline bool serve_pending_distSteal_request_asynchronous() {
	// if im here then means I have tasks available
	out_of_work = false;
	// First make sure, we still have tasks, hence try local steal first
	int count = 0;
	hcpp::remoteAsyncAny_task* tasks = steal_from_my_computation_workers(&count);

	if(count > 0) {
		// pop a thief
		const int thief = pop_thief();
		HASSERT(thief >= 0);
		ship_asyncAny_to_remote_thief(tasks, count, thief, false);
		return true;
	}

	return false;
}

bool serve_pending_distSteal_request_successonly() {
	bool success = true;
	// if im here then means I have tasks available
	while(!idle_workers && thieves_waiting) {
		success = serve_pending_distSteal_request_asynchronous();
		if(!success) break;
	}

	publish_local_load_info();
	return success;
}

inline int renewed_as_victim() {
	const int declared_load = hcpp::totalAsyncAnyAvailable();
	if(req_thread[upcxx::global_myrank()] == REQ_UNAVAILABLE) {
		// this will be true only in two conditions:
		// 1. this thread was a thief in previous iteration
		// 2. this thread just started with work
		if(declared_load) {
			req_thread[upcxx::global_myrank()] = REQ_AVAILABLE;
			//glb: we now handling this case when we return from this function
			//workAvail[upcxx::global_myrank()] = work;
		}
	}
	return declared_load;
}

inline bool serve_pending_distSteal_request_synchronous() {
	// if im here then means I have tasks available
	out_of_work = false;
	int requestor = req_thread[upcxx::global_myrank()];
	if (requestor >= 0) {
		int count = 0;
		hcpp::remoteAsyncAny_task* tasks = steal_from_my_computation_workers(&count);

		if(count > 0) {
			ship_asyncAny_to_remote_thief(tasks, count, requestor, true);
			return true;
		}
	}

	return false;
}

bool serve_pending_distSteal_request_baseline() {
	bool success = false;
	if(renewed_as_victim() > 0) {
		success = serve_pending_distSteal_request_synchronous();
	}
	// re-advertise correct work
	publish_local_load_info();
	return success;
}

bool serve_pending_distSteal_request_glb() {
	current_glb_max_rand_attempts = 0;
	out_of_work = false;

	bool success = true;
	if(renewed_as_victim() == 0) {
		publish_local_load_info();
		return false;
	}

	while(success) {
		if(req_thread[upcxx::global_myrank()]>=0) {
			success = serve_pending_distSteal_request_synchronous();
		}
		else if(!idle_workers && thieves_waiting) {
			success = serve_pending_distSteal_request_asynchronous();
		}
		else {
			success = false;
			break;
		}
	}

	publish_local_load_info();
	return success;
}

/**************************************
 *  Thief operated routines start here
 **************************************/

inline void mark_myPlace_asIdle() {
	const int me = upcxx::global_myrank();
	if(out_of_work) {
			return;
	}

	out_of_work = true;
	workAvail[me] = NOT_WORKING;

	if(baseline_distWS || glb_distWS) {
			LOCK_REQ_SELF;
			int pendingReq = req_thread[me];
			req_thread[me] = REQ_UNAVAILABLE;
			UNLOCK_REQ_SELF;

			if(pendingReq >= 0) {
				// invalidate steal request. this steal request has failed
				// This will get the thief out of the while loop in steal_from_victim()
				waitForTaskFromVictim[pendingReq] = false;
			}
	}
}

static bool waiting_for_returnAsync = false;

void receipt_of_stealRequest() {
	waiting_for_returnAsync = false;
}

/*
 * Thief's asyncAny task hunt function.
 * Thief will use RDMA messages to find a busy victim and
 * then uses upcxx::async to queue steal request at the victim.
 * If the victim runs out of asyncAny task, it does not deny the
 * request and eventually send asyncAny to thief.
 *
 * This is an improvement over:
 * Saraswat, V.A., Kambadur, P., Kodali, S., Grove, D., Krishnamoorthy,
 * S.: Lifeline-based global load balancing. In: PPoPP. pp. 201-212 (2011)
 */
inline bool attempt_steal_or_set_lifeline(int v, int me) {
	if(victim_already_contacted(v)) return false;

	int workProbe = workAvail[v];
	total_asyncany_rdma_probes_stats();

	if (workProbe > task_transfer_threshold) { // TODO: may not be good in case of HPT
		// try to queue for work
		mark_victimContacted(v);
		waiting_for_returnAsync = true;

		auto lambda = [me]() {
			queue_thief(me);
			upcxx::async(me, NULL)(receipt_of_stealRequest);
		};

		launch_upcxx_async<decltype(lambda)>(&lambda, v);

#ifdef HCPP_DEBUG
		cout <<upcxx::global_myrank() << ": Sending steal request to " << v << endl;
#endif

		while(waiting_for_returnAsync) {
			upcxx::advance();
#ifdef HCPP_DEBUG
			cout <<upcxx::global_myrank() << ": Waiting for return async"<< endl;
#endif
		}
		return true;
	}

	return false;
}

inline bool search_for_lifelines(bool glb) {
	const int me = upcxx::global_myrank();
	const int ranks = upcxx::global_ranks();
	int victims_contacted = 0;
	/*
	 * Hypercube neighbors:
	 * The lowBound and highBound in this case for loop as well as the function
	 * to calculate the victim id was contributed by Karthik S.M.
	 */
	const int upperBound = glb ? log(ranks) : ranks;
	int victim;
	/* check all other threads */
	for (int i = 0; i < upperBound && !received_tasks_from_victim(); i++) {
		if(glb) victim = ((int)(pow(2,i)+me)) % ranks;
		else victim = selectvictim();
		if(me == victim) continue;
		const bool success = attempt_steal_or_set_lifeline(victim, me);
		if(success) victims_contacted++;
	} /*for */

	contacted_victims_statistics(victims_contacted);

	return victims_contacted>0;
}

inline bool steal_from_victim_baseline(int victim) {
	// Poke upcxx to push/pull tasks from upcxx queue
	bool success = false;
	while(waitForTaskFromVictim[upcxx::global_myrank()]) {
		upcxx::advance();
	}
	return true;
}

inline int attempt_synchronous_steal(int v) {
	int workProbe = workAvail[v];
	const int me = upcxx::global_myrank();
	total_asyncany_rdma_probes_stats();

	if (workProbe == NOT_WORKING)
		return NOT_WORKING;

	if (workProbe > task_transfer_threshold) {
		// try to queue for work
		LOCK_REQ(v);
		bool success = (req_thread[v] == REQ_AVAILABLE);
		if (success) {
			waitForTaskFromVictim[me] = true;	// no lock is required to modify this, but we always want to do this before marking our id at victim
			req_thread[v] = me;
		}
		UNLOCK_REQ(v);

		if (success) {
#ifdef HCPP_DEBUG
			cout <<upcxx::me << ": Receiving tasks from " << v << endl;
#endif
			// once this function returns, then it means the work is received from victim
			success = steal_from_victim_baseline(v);
			if(success) {
				workAvail[me] = 0;
				req_thread[me] = REQ_AVAILABLE;
				out_of_work=false;
			}
			return SUCCESS_STEAL;
		}
		else {
#ifdef HCPP_DEBUG
			cout <<upcxx::me << ": Failed to steal from " << v << endl;
#endif
			record_failedSteal_timeline();
			return FAILED_STEAL;
		}
	}

	return NOT_WORKING;
}

inline bool search_tasks_globally_synchronous(bool glb) {
	for (int i = 1; i < upcxx::global_ranks(); i++) {
		int v = selectvictim();
		const int status = attempt_synchronous_steal(v);
		switch(status) {
		case SUCCESS_STEAL:
			if(glb) current_glb_max_rand_attempts = 0;
			return true;
		case FAILED_STEAL:
			if(glb) current_glb_max_rand_attempts++;
			/* Do nothing else */
			break;
		case NOT_WORKING:
			/* Do nothing */
			break;
		default:
			assert(0 && "Unexpected switch-case statement");
		}

		if(glb && (current_glb_max_rand_attempts >= glb_max_rand_attempts)) {
			return false;
		}

	} /*for */

	return false;
}

bool search_tasks_globally_successonly() {
	// show this thread as not working
	mark_myPlace_asIdle();
	// restart victim selection
	resetVictimArray();
	bool glb = successonly_distWS ? false : true;
	return search_for_lifelines(glb);
}

bool search_tasks_globally_baseline() {
	// show this thread as not working
	mark_myPlace_asIdle();
	// restart victim selection
	resetVictimArray();
	return search_tasks_globally_synchronous(false);
}

bool search_tasks_globally_glb() {
	// show this thread as not working
	mark_myPlace_asIdle();
	// restart victim selection
	resetVictimArray();

	/*
	 * When a rank becomes a thief, it would try "HCPP_DIST_WS_GLB_RAND" number of random
	 * victim selections first, followed by setting up lifelines
	 *
	 * However, in one search cycle it might be possible that it fails
	 * in both. In the next search cycles it should never try random selections
	 * and hence we return true to hint that random attempts are already done
	 */
	bool tryRandomVictim = (current_glb_max_rand_attempts < glb_max_rand_attempts);

	bool success = false;
	if(tryRandomVictim) {
		// Start with attempting steal from random victims
		success = search_tasks_globally_synchronous(true);
	}

	// Recheck--in any case don't attempt lifeline without attempting required number of rand attempts
	tryRandomVictim = (current_glb_max_rand_attempts < glb_max_rand_attempts);

	if(!success && !tryRandomVictim) {
		// Now try the lifeline approach
		return search_for_lifelines(true);
	}

	return success;
}

}
