#include "hcupc_spmd-internal.h"
#include "hcupc_spmd_common_methods.h"
#include "hcupc_spmd-commTask.h"
#include "hcupc_spmd-atomics.h"
#include <cmath>

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

//#define HCPP_DEBUG

/*
 * End ------- Runtime control flags --------
 */

namespace hupcpp {

counter_t			  total_success_steals=0;
counter_t			  total_tasks_stolen=0;
counter_t			  total_failed_steals=0;
counter_t			  total_asyncany_rdma_probes=0;

extern volatile int idle_workers;	// counts computation workers only

void get_totalAsyncAny_stats(counter_t *tasksStolen, counter_t* successSteals, counter_t* failSteals, counter_t* recvStealReqsWhenFree, counter_t* cyclic_steals, counter_t* rdma_probes) {
	*tasksStolen = total_tasks_stolen;
	*successSteals = total_success_steals;
	*failSteals = total_failed_steals;
	*rdma_probes = total_asyncany_rdma_probes;
	total_asyncany_rdma_probes = 0;
	*recvStealReqsWhenFree = 0;
	total_failed_steals = 0;
	total_success_steals = 0;
	total_tasks_stolen = 0;
}

double totalTasksStolenInOneShot = -1; // may be fraction as well (getenv)

upcxx::shared_array<upcxx::shared_lock> workAvailLock;
upcxx::shared_array<int> workAvail;
upcxx::shared_array<bool> waitForTaskFromVictim;

upcxx::shared_array<upcxx::shared_lock> reqLock;
upcxx::shared_array<int> req_thread;

upcxx::shared_array<upcxx::shared_lock> asyncsInFlightCountLock;
upcxx::shared_array<int> asyncsInFlight;

static int task_transfer_threshold;

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

void decrement_tasks_in_flight_count() {
}

bool received_tasks_from_victim() {
	return true;
}

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

	task_transfer_threshold = 2 * hcpp::numWorkers();	// heuristics

	if(MYTHREAD == 0) {
		cout << "---------DIST_WS_RUNTIME_INFO-----------" << endl;
		if(totalTasksStolenInOneShot < 1) {
			printf("WARNING (HCPP_STEAL_N): N should always be positive integer, setting it as 1\n");
			totalTasksStolenInOneShot = 1;
		}
		printf(">>> HCPP_STEAL_N\t\t= %f\n",totalTasksStolenInOneShot);
		printf(">>> %s\n",vsdescript());
		cout << "----------------------------------------" << endl;
		hcpp::display_runtime();
	}
	assert(totalTasksStolenInOneShot <= 5);

	// initialize random victim selection routines
	initialize_last_stolen();
	vsinit();

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

template <typename T>
void asyncAny_destination(upcxx::global_ptr<T> remote_lambda) {
	upcxx::global_ptr<T> my_lambda = upcxx::allocate<T>(MYTHREAD, 1);
	upcxx::copy(remote_lambda, my_lambda, 1);

	(*((T*)(my_lambda.raw_ptr())))();

	upcxx::deallocate(my_lambda);
	upcxx::deallocate(remote_lambda);
}

bool serve_pending_distSteal_request() {
	// if im here then means I have tasks available

	if(req_thread[MYTHREAD] == REQ_UNAVAILABLE) {
		// this will be true only in two conditions:
		// 1. this thread was a thief in previous iteration
		// 2. this thread just started with work
		const int work = hcpp::totalAsyncAnyAvailable();
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
		hcpp::remoteAsyncAny_task tasks[5];

		for( ; i<totalTasksStolenInOneShot; i++) {
			if(i>0 && idle_workers) break;
			// Steal task from computation workers
			bool success = hcpp::steal_fromComputeWorkers_forDistWS(&tasks[i]);
			if(!success) break;	// never return from here as it will be bug in case HCPP_STEAL_N>1
			total_tasks_stolen++;
		}

		if(i > 0) {
			// Increment outgoing task counter
			INCREMENT_TASK_IN_FLIGHT_SELF;


			// use upcxx::async to send task to requestor
#ifdef HCPP_DEBUG
			cout <<MYTHREAD << ": Sending " << i << " tasks to " << requestor << endl;
#endif
			total_success_steals++;
			const int source = MYTHREAD;
			auto lambda_for_thief = [tasks, i, source]() {
				const int dest = MYTHREAD;
				assert(source != dest);
				unwrap_n_asyncAny_tasks(tasks, i);
				DECREMENT_TASK_IN_FLIGHT(source);
				// this flag is only modified by thief, but victim can also mark false if its out of work
				waitForTaskFromVictim[dest] = false;
			};

			upcxx::global_ptr<decltype(lambda_for_thief)> remote_lambda = upcxx::allocate<decltype(lambda_for_thief)>(MYTHREAD, 1);
			memcpy(remote_lambda.raw_ptr(), &lambda_for_thief, sizeof(decltype(lambda_for_thief)));
			upcxx::async(requestor,NULL)(asyncAny_destination<decltype(lambda_for_thief)>, remote_lambda);
			req_thread[MYTHREAD] = REQ_AVAILABLE;
		}
	}

	// re-advertise correct work
	workAvail[MYTHREAD] = hcpp::totalAsyncAnyAvailable();

	return true;
}

inline void mark_myPlace_asIdle() {
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

inline int findwork() {
	int workDetected;

	do {
		workDetected = 0;
		/* check all other threads */
		resetVictimArray();

		for (int i = 1; i < THREADS; i++) {
			int v = selectvictim();
			int workProbe = workAvail[v];
			total_asyncany_rdma_probes++;

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
#ifdef HCPP_DEBUG
					cout <<MYTHREAD << ": Receiving tasks from " << v << endl;
#endif
					return v;
				}
				else {
#ifdef HCPP_DEBUG
					cout <<MYTHREAD << ": Failed to steal from " << v << endl;
#endif
					total_failed_steals++;
				}
			}

		} /*for */

	} while (workDetected);

	return NONE_WORKING;
}

inline bool steal_from_victim(int victim) {
	// Poke upcxx to push/pull tasks from upcxx queue
	bool success = false;
	while(waitForTaskFromVictim[MYTHREAD]) {
		upcxx::advance();
	}
	return true;
}

bool search_tasks_globally() {
	// show this thread as not working
	mark_myPlace_asIdle();

	bool success = false;
	while(true) {
		const int victim = findwork();
		if(victim >= 0) {
			// once this function returns, then it means the work is received from victim
			success = steal_from_victim(victim);
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

}
