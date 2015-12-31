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
 * export HCLIB_DIST_WS_BASELINE=1
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
#define __VS_RR__

/*
 * ------- Runtime control flags -------- End
 */

namespace hupcpp {
static int last_steal;	// place id of last victim
}

#include "hcupc_spmd-random.h"

//#define HCLIB_DEBUG

/*
 * End ------- Runtime control flags --------
 */

namespace hupcpp {

extern volatile int idle_workers;	// counts computation workers only

double totalTasksStolenInOneShot = -1; // may be fraction as well (getenv)

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

upcxx::shared_array<upcxx::shared_lock> workAvailLock;
upcxx::shared_array<int> workAvail;
upcxx::shared_array<bool> waitForTaskFromVictim;

upcxx::shared_array<upcxx::shared_lock> reqLock;
upcxx::shared_array<int> req_thread;

upcxx::shared_array<upcxx::shared_lock> asyncsInFlightCountLock;
upcxx::shared_array<int> asyncsInFlight;

#define REQ_AVAILABLE	-1	 /* req_thread */
#define REQ_UNAVAILABLE  -2  /* req_thread */
#define NOT_WORKING      -1  /* work_available */
#define NONE_WORKING     -2  /* result of probe sequence */

#define LOCK_WORK_AVAIL(i)				workAvailLock[i].get().lock()
#define UNLOCK_WORK_AVAIL(i)			workAvailLock[i].get().unlock()
#define LOCK_WORK_AVAIL_SELF			(&workAvailLock[MYTHREAD])->lock()
#define UNLOCK_WORK_AVAIL_SELF			(&workAvailLock[MYTHREAD])->unlock()

#define LOCK_REQ(i)			 			reqLock[i].get().lock()
#define UNLOCK_REQ(i)		 			reqLock[i].get().unlock()
#define LOCK_REQ_SELF		 			(&reqLock[MYTHREAD])->lock()
#define UNLOCK_REQ_SELF		 			(&reqLock[MYTHREAD])->unlock()

#define INCREMENT_TASK_IN_FLIGHT_SELF   {(&asyncsInFlightCountLock[MYTHREAD])->lock(); asyncsInFlight[MYTHREAD] = asyncsInFlight[MYTHREAD]+1;   (&asyncsInFlightCountLock[MYTHREAD])->unlock(); }
#define DECREMENT_TASK_IN_FLIGHT(i)     {asyncsInFlightCountLock[i].get().lock(); asyncsInFlight[i] = asyncsInFlight[i]-1;      asyncsInFlightCountLock[i].get().unlock(); }

/*
 * Used only at the source
 */
void increment_task_in_flight_self() {
	(&asyncsInFlightCountLock[MYTHREAD])->lock();
	asyncsInFlight[MYTHREAD] = asyncsInFlight[MYTHREAD]+1;
	(&asyncsInFlightCountLock[MYTHREAD])->unlock();
}

/*
 * Used only at the source. This is used only when asyncCopy is invoked
 */
inline void decrement_task_in_flight_self(int tasks) {
	(&asyncsInFlightCountLock[MYTHREAD])->lock();
	asyncsInFlight[MYTHREAD] = asyncsInFlight[MYTHREAD]-tasks;
	(&asyncsInFlightCountLock[MYTHREAD])->unlock();
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
	check_cyclicSteals(v, head, tail, queued_thieves);
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
	return ((tail - THREADS) == head);
}

inline int size_thief_queue() {
	return std::abs(head - tail);
}

inline bool empty_thief_queue() {
	return head==tail;
}

/*
 * Victim will queue the thief id for every steal request.
 */
inline void queue_thief(int i) {
	HASSERT(!full_thief_queue());
	thieves_waiting = true;
	check_if_out_of_work_stats(out_of_work);
	queued_thieves[++tail % THREADS] = i;
}

inline int pop_thief() {
	if(empty_thief_queue()) {
		HASSERT(!thieves_waiting);
		return -1;
	}
	const int p = queued_thieves[++head % THREADS];
	HASSERT(p>=0 && p<THREADS);
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
	HASSERT(head_rto < THREADS);
	const int index = head_rto++;
	*victim = index;
	*tasks = received_tasks_origin[index];
	received_tasks_origin[index] = 0; // reset
	if(head_rto == THREADS) {
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
			if(victim != MYTHREAD) {
				decrement_task_in_flight(victim, tasks);
			}
			else {
				decrement_task_in_flight_self(tasks);
			}
		}
	}
}

bool received_tasks_from_victim() {
	return tasks_received>0;
}

/*
 * Distributed runtime initialization.
 */
void initialize_distws_setOfThieves() {
	if(getenv("HCLIB_STEAL_N") != NULL) {
		totalTasksStolenInOneShot = std::stod(getenv("HCLIB_STEAL_N"));
		HASSERT(totalTasksStolenInOneShot > 0);
		if(totalTasksStolenInOneShot>1) {
			HASSERT((totalTasksStolenInOneShot-(int)totalTasksStolenInOneShot) == 0);       // do not want something like 1.5
		}
	}
	else {
		totalTasksStolenInOneShot = 1;
	}

	task_transfer_threshold = 2 * numWorkers();	// heuristics

	if(MYTHREAD == 0) {
#ifdef DIST_WS
		cout << "---------DIST_WS_RUNTIME_INFO-----------" << endl;
		if(getenv("HCLIB_DIST_WS_BASELINE")) {
			printf(">>> HCLIB_DIST_WS_BASELINE\t\t= true\n");
		}
		else {
			printf(">>> HCLIB_DIST_WS_BASELINE\t\t= false\n");
		}
		if(totalTasksStolenInOneShot < 1) {
			printf("WARNING (HCLIB_STEAL_N): N should always be positive integer, setting it as 1\n");
			totalTasksStolenInOneShot = 1;
		}
		printf(">>> HCLIB_STEAL_N\t\t= %f\n",totalTasksStolenInOneShot);
		printf(">>> %s\n",vsdescript());
		cout << "----------------------------------------" << endl;
#endif
		hclib::display_runtime();
	}
	assert(totalTasksStolenInOneShot <= 5);
	stats_initTimelineEvents();
#ifdef DIST_WS
	// initialize random victim selection routines
	initialize_last_stolen();
	vsinit();
#endif

	workAvail.init(THREADS);
	workAvailLock.init(THREADS);
	new (workAvailLock[MYTHREAD].raw_ptr()) upcxx::shared_lock();
	waitForTaskFromVictim.init(THREADS);

	asyncsInFlight.init(THREADS);
	asyncsInFlightCountLock.init(THREADS);
	new (asyncsInFlightCountLock[MYTHREAD].raw_ptr()) upcxx::shared_lock();

	req_thread.init(THREADS);
	reqLock.init(THREADS);
	new (reqLock[MYTHREAD].raw_ptr()) upcxx::shared_lock();

	workAvail[MYTHREAD] = NOT_WORKING;
	req_thread[MYTHREAD] = REQ_UNAVAILABLE;
	asyncsInFlight[MYTHREAD] = 0;
	waitForTaskFromVictim[MYTHREAD] = false;

	contacted_victims = new int[THREADS];
	queued_thieves = new int[THREADS];
	received_tasks_origin = new int[THREADS];
	for(int i=0; i<THREADS; i++) {
		contacted_victims[i] = POPPED_ENTRY;
		queued_thieves[i] = POPPED_ENTRY;
		received_tasks_origin[i] = 0;
	}
}

/*
 * true if neighboring thread to the right is working
 */
int detectWork() {
	const int neighbor = (MYTHREAD + 1) % THREADS;
	return (NOT_WORKING != workAvail[neighbor]);
}

int total_asyncs_inFlight() {
	int pending_asyncs = 0;
	for(int i=0; i<THREADS; i++) pending_asyncs += asyncsInFlight[i];
	return pending_asyncs;
}

/*
 * victim uses to this to publish its load info in global address space
 * to help thief in: a) finding correct victim; and b) termination detection.
 */
void publish_local_load_info() {
#ifdef DIST_WS
	const int total_aany = hclib::totalAsyncAnyAvailable();
	workAvail[MYTHREAD] = (total_aany>0) ? total_aany : totalPendingLocalAsyncs();
#else
	workAvail[MYTHREAD] = totalPendingLocalAsyncs();
#endif
}

#ifdef DIST_WS
/*
 * Runs at thief when victim sends asyncAny task to thief
 */
template <typename T>
void asyncAny_destination(upcxx::global_ptr<T> remote_lambda) {
	upcxx::global_ptr<T> my_lambda = upcxx::allocate<T>(MYTHREAD, 1);
	upcxx::copy(remote_lambda, my_lambda, 1);

	(*((T*)(my_lambda.raw_ptr())))();

	upcxx::deallocate(my_lambda);
	upcxx::deallocate(remote_lambda);
}

template <typename T>
void launch_upcxx_async(T* lambda, int dest) {
	upcxx::global_ptr<T> remote_lambda = upcxx::allocate<T>(MYTHREAD, 1);
	memcpy(remote_lambda.raw_ptr(), lambda, sizeof(T));
	upcxx::async(dest,NULL)(asyncAny_destination<T>, remote_lambda);
}

/*
 * Victim function.
 * Victim pops pending steal requests. If its able to steal even
 * one asyncAny task from its computation worker, it will pop
 * a pending remote steal request and send asyncAny using upcxx::async.
 * At the end it will publish its local load.
 */
bool serve_pending_distSteal_request() {
	// if im here then means I have tasks available
	out_of_work = false;
	while(!idle_workers && thieves_waiting) {
		// First make sure, we still have tasks, hence try local steal first
		int i=0;
		hclib::remoteAsyncAny_task tasks[5];

		for( ; i<totalTasksStolenInOneShot; i++) {
			// Steal task from computation workers
			if(i>0 && idle_workers) break;
			bool success = hclib::steal_fromComputeWorkers_forDistWS(&tasks[i]);
			if(!success) break;	// never return from here as it will be bug in case HCLIB_STEAL_N>1
			increment_outgoing_tasks();
		}

		if(i > 0) {
			// Increment outgoing task counter
			increment_task_in_flight_self();

			// pop a thief
			const int thief = pop_thief();
			HASSERT(thief >= 0);
			// use upcxx::async to send task to requestor
#ifdef HCLIB_DEBUG
			cout <<MYTHREAD << ": Sending " << i << " tasks to " << thief << endl;
#endif
			success_steals_stats();
			const int source = MYTHREAD;
			auto lambda_for_thief = [tasks, i, source]() {
				const int dest = MYTHREAD;
				assert(source != dest);
				unwrap_n_asyncAny_tasks(tasks, i);
				//decrement_task_in_flight(source);
				queue_source_place_of_remoteTask(source);
				unmark_victimContacted(source);
			};

			launch_upcxx_async<decltype(lambda_for_thief)>(&lambda_for_thief, thief);
			upcxx::advance();
		}
		else {
			out_of_work = true;
			break; // cannot steal tasks to send remote
		}
	}

	// re-advertise correct work
	publish_local_load_info();

	return true;
}

bool serve_pending_distSteal_request_baseline() {
	// if im here then means I have tasks available

	if(req_thread[MYTHREAD] == REQ_UNAVAILABLE) {
		// this will be true only in two conditions:
		// 1. this thread was a thief in previous iteration
		// 2. this thread just started with work
		const int work = hclib::totalAsyncAnyAvailable();
		if(work) {
			req_thread[MYTHREAD] = REQ_AVAILABLE;
			workAvail[MYTHREAD] = work;
			return true;
		}
		return false;
	}

	int requestor = req_thread[MYTHREAD];
	if (requestor >= 0) {
		int i=0;
		hclib::remoteAsyncAny_task tasks[5];

		for( ; i<totalTasksStolenInOneShot; i++) {
			if(i>0 && idle_workers) break;
			// Steal task from computation workers
			bool success = hclib::steal_fromComputeWorkers_forDistWS(&tasks[i]);
			if(!success) break;	// never return from here as it will be bug in case HCLIB_STEAL_N>1
			increment_outgoing_tasks();
		}

		if(i > 0) {
			// Increment outgoing task counter
			INCREMENT_TASK_IN_FLIGHT_SELF;


			// use upcxx::async to send task to requestor
#ifdef HCLIB_DEBUG
			cout <<MYTHREAD << ": Sending " << i << " tasks to " << requestor << endl;
#endif
			success_steals_stats();
			const int source = MYTHREAD;
			auto lambda_for_thief = [tasks, i, source]() {
				const int dest = MYTHREAD;
				assert(source != dest);
				unwrap_n_asyncAny_tasks(tasks, i);
				DECREMENT_TASK_IN_FLIGHT(source);
				// this flag is only modified by thief, but victim can also mark false if its out of work
				waitForTaskFromVictim[dest] = false;
			};

			launch_upcxx_async<decltype(lambda_for_thief)>(&lambda_for_thief, requestor);
			req_thread[MYTHREAD] = REQ_AVAILABLE;
			upcxx::advance();
		}
	}

	// re-advertise correct work
	publish_local_load_info();

	return true;
}

inline void mark_myPlace_asIdle() {
	out_of_work = true;
	workAvail[MYTHREAD] = NOT_WORKING;
}

inline void mark_myPlace_asIdle_baseline() {
	if(workAvail[MYTHREAD] == NOT_WORKING && req_thread[MYTHREAD] == REQ_UNAVAILABLE) {
		return;
	}
	else {
		LOCK_REQ_SELF;
		workAvail[MYTHREAD] = NOT_WORKING;
		int pendingReq = req_thread[MYTHREAD];
		req_thread[MYTHREAD] = REQ_UNAVAILABLE;
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
 * S.: Lifeline-based global load balancing. In: PPoPP. pp. 201�212 (2011)
 */
bool search_tasks_globally() {
	// show this thread as not working
	mark_myPlace_asIdle();
	// restart victim selection
	resetVictimArray();

	const int me = MYTHREAD;
	int victims_contacted = 0;
	/* check all other threads */
	for (int i = 1; i < THREADS && !received_tasks_from_victim(); i++) {
		const int v = selectvictim();
		if(victim_already_contacted(v)) continue;

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

#ifdef HCLIB_DEBUG
			cout <<MYTHREAD << ": Sending steal request to " << v << endl;
#endif
			victims_contacted++;

			while(waiting_for_returnAsync) {
				upcxx::advance();
#ifdef HCLIB_DEBUG
				cout <<MYTHREAD << ": Waiting for return async"<< endl;
#endif
			}
		}
	} /*for */

	contacted_victims_statistics(victims_contacted);

	return victims_contacted>0;
}


inline int findwork_baseline() {
	int workDetected;

	do {
		workDetected = 0;
		/* check all other threads */
		resetVictimArray();

		for (int i = 1; i < THREADS; i++) {
			int v = selectvictim();
			int workProbe = workAvail[v];
			total_asyncany_rdma_probes_stats();

			if (workProbe != NOT_WORKING)
				workDetected = 1;

			if (workProbe > task_transfer_threshold) {
				// try to queue for work
				LOCK_REQ(v);
				bool success = (req_thread[v] == REQ_AVAILABLE);
				if (success) {
					waitForTaskFromVictim[MYTHREAD] = true;	// no lock is required to modify this, but we always want to do this before marking our id at victim
					req_thread[v] = MYTHREAD;
				}
				UNLOCK_REQ(v);

				if (success) {
#ifdef HCLIB_DEBUG
					cout <<MYTHREAD << ": Receiving tasks from " << v << endl;
#endif
					return v;
				}
				else {
#ifdef HCLIB_DEBUG
					cout <<MYTHREAD << ": Failed to steal from " << v << endl;
#endif
					record_failedSteal_timeline();
				}
			}

		} /*for */

	} while (workDetected);

	return NONE_WORKING;
}

inline bool steal_from_victim_baseline(int victim) {
	// Poke upcxx to push/pull tasks from upcxx queue
	bool success = false;
	while(waitForTaskFromVictim[MYTHREAD]) {
		upcxx::advance();
	}
	return true;
}

bool search_tasks_globally_baseline() {
	// show this thread as not working
	mark_myPlace_asIdle_baseline();

	bool success = false;
	while(true) {
		const int victim = findwork_baseline();
		if(victim >= 0) {
			// once this function returns, then it means the work is received from victim
			success = steal_from_victim_baseline(victim);
			if(success) {
				workAvail[MYTHREAD] = 0;
				req_thread[MYTHREAD] = REQ_AVAILABLE;
				//success steal, increment the counter
				break;	// start the fast path
			}
			else {
				// failed steal, increment the counter
				// keep searching
				continue;
			}
		}
		else {
			// probably all places are idle
			break;
		}
	}

	return success;
}
#endif

}
