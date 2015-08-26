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

counter_t	total_success_steals=0;
counter_t	total_tasks_stolen=0;
counter_t	total_failed_steals=0;
counter_t	total_received_stealRequests_when_idle=0;
counter_t	total_cyclic_steals=0;
counter_t	total_asyncany_rdma_probes=0;

int		total_steal_1 = 0;
int		total_steal_2 = 0;
int		total_steal_3 = 0;
int		total_steal_4 = 0;
int		total_steal_4Plus = 0;

extern volatile int idle_workers;	// counts computation workers only

/*
 * only for statistics
 */
inline void contacted_victims_statistics(int victims_contacted) {
	if(victims_contacted) {
		switch(victims_contacted) {
		case 1:
			total_steal_1++;
			break;
		case 2:
			total_steal_2++;
			break;
		case 3:
			total_steal_3++;
			break;
		case 4:
			total_steal_4++;
			break;
		default:
			total_steal_4Plus++;
			break;
		}
	}
}

/*
 * only for statistics
 */
void get_totalAsyncAny_stats(counter_t *tasksStolen, counter_t* successSteals, counter_t* failSteals, counter_t* recvStealReqsWhenFree, counter_t* cyclic_steals, counter_t* rdma_probes) {
	*tasksStolen = total_tasks_stolen;
	*successSteals = total_success_steals;
	*failSteals = total_failed_steals;
	*recvStealReqsWhenFree = total_received_stealRequests_when_idle;
	*cyclic_steals = total_cyclic_steals;
	*rdma_probes = total_asyncany_rdma_probes;
	total_cyclic_steals = 0;
	total_asyncany_rdma_probes = 0;
	total_failed_steals = 0;
	total_success_steals = 0;
	total_tasks_stolen = 0;
	total_received_stealRequests_when_idle = 0;
}

void increment_outgoing_tasks() {
	total_tasks_stolen++;
}

void get_steal_stats(int* s1, int* s2, int* s3, int* s4, int* s4P) {
	*s1 = total_steal_1;
	*s2 = total_steal_2;
	*s3 = total_steal_3;
	*s4 = total_steal_4;
	*s4P = total_steal_4Plus;
}

double totalTasksStolenInOneShot = -1; // may be fraction as well (getenv)

upcxx::shared_array<int> workAvail;
upcxx::shared_array<upcxx::shared_lock> asyncsInFlightCountLock;
upcxx::shared_array<int> asyncsInFlight;

#define NOT_WORKING      -1  /* work_available */
#define NONE_WORKING     -2  /* result of probe sequence */

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

/*
 * only for statistics
 */
inline void check_cyclicSteals(int v) {
	int h=head, t=tail;
	while(h!=t) {
		if(v==queued_thieves[++h % THREADS]) {
			total_cyclic_steals++;
			return;
		}
	}
}

inline void mark_victimContacted(int v) {
	HASSERT(contacted_victims[v] == POPPED_ENTRY);
	contacted_victims[v] = QUEUED_ENTRY;
	check_cyclicSteals(v);
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
	if(out_of_work) total_received_stealRequests_when_idle++;
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

	if(MYTHREAD == 0) {
#ifdef DIST_WS
		cout << "---------DIST_WS_RUNTIME_INFO-----------" << endl;
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

#ifdef DIST_WS
	// initialize random victim selection routines
	initialize_last_stolen();
	vsinit();
#endif

	workAvail.init(THREADS);

	asyncsInFlight.init(THREADS);
	asyncsInFlightCountLock.init(THREADS);
	new (asyncsInFlightCountLock[MYTHREAD].raw_ptr()) upcxx::shared_lock();

	workAvail[MYTHREAD] = NOT_WORKING;
	asyncsInFlight[MYTHREAD] = 0;

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

/*
 * victim uses to this to publish its load info in global address space
 * to help thief in: a) finding correct victim; and b) termination detection.
 */
void publish_local_load_info() {
#ifdef DIST_WS
	const int total_aany = hcpp::totalAsyncAnyAvailable();
	workAvail[MYTHREAD] = (total_aany>0) ? total_aany : totalPendingLocalAsyncs();
#else
	workAvail[MYTHREAD] = totalPendingLocalAsyncs();
#endif
}

int total_asyncs_inFlight() {
	int pending_asyncs = 0;
	for(int i=0; i<THREADS; i++) pending_asyncs += asyncsInFlight[i];
	return pending_asyncs;
}

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
#ifdef DIST_WS // There are other functions too here depending on CRT, but using ifdef only here to pass build when using OCR
bool serve_pending_distSteal_request() {
	// if im here then means I have tasks available
	out_of_work = false;
	while(!idle_workers && thieves_waiting) {
		// First make sure, we still have tasks, hence try local steal first
		int i=0;
		hcpp::remoteAsyncAny_task tasks[5];

		for( ; i<totalTasksStolenInOneShot; i++) {
			// Steal task from computation workers
			if(i>0 && idle_workers) break;
			bool success = hcpp::steal_fromComputeWorkers_forDistWS(&tasks[i]);
			if(!success) break;	// never return from here as it will be bug in case HCPP_STEAL_N>1
			increment_outgoing_tasks();
		}

		if(i > 0) {
			// Increment outgoing task counter
			increment_task_in_flight_self();

			// pop a thief
			const int thief = pop_thief();
			HASSERT(thief >= 0);
			// use upcxx::async to send task to requestor
#ifdef HCPP_DEBUG
			cout <<MYTHREAD << ": Sending " << i << " tasks to " << thief << endl;
#endif
			total_success_steals++;
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
#endif

inline void mark_myPlace_asIdle() {
	out_of_work = true;
	workAvail[MYTHREAD] = NOT_WORKING;
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
 * S.: Lifeline-based global load balancing. In: PPoPP. pp. 201Ð212 (2011)
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
		total_asyncany_rdma_probes++;

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
			cout <<MYTHREAD << ": Sending steal request to " << v << endl;
#endif
			victims_contacted++;

			while(waiting_for_returnAsync) {
				upcxx::advance();
#ifdef HCPP_DEBUG
				cout <<MYTHREAD << ": Waiting for return async"<< endl;
#endif
			}
		}
	} /*for */

	contacted_victims_statistics(victims_contacted);

	return victims_contacted>0;
}
}
