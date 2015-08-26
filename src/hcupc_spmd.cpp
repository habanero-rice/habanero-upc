#include "hcupc_spmd-internal.h"
#include "hcupc_spmd_common_methods.h"
#include "hcupc_spmd-commTask.h"
#include "hcupc_spmd-atomics.h"

namespace hupcpp {

static int hc_workers;
static bool hc_workers_initialized = false;
volatile int* current_finish_counter = NULL;

void init(int * argc, char *** argv) {
        hcpp::init(argc, *argv);
        upcxx::init(argc, argv);
        hupcpp::showStatsHeader();
}

void finalize() {
        hupcpp::showStatsFooter();
        upcxx::finalize();
        hcpp::finalize();
}

void initialize_hcWorker() {
	initialize_distws_setOfThieves();
	hc_workers = numWorkers();
	assert(hc_workers > 0);
	semiConcDequeInit();
	hc_workers_initialized = true;
}

/*
 * upcxx::event management for asyncCopy
 */
typedef struct event_list {
	upcxx::event e;
	event_list* next;
} event_list;

static event_list* event_list_head = NULL;
static event_list* event_list_curr;

upcxx::event* get_upcxx_event() {
	event_list *el = new event_list;
	el->next = NULL;

	if(!event_list_head) {
		event_list_head = event_list_curr = el;
	}
	else {
		event_list_curr->next = el;
		event_list_curr = el;
	}
	return &(el->e);
}

/*
 * upcxx::event management for asyncCopy
 */
inline void free_upcxx_event_list() {
	while(event_list_head) {
		event_list* el = event_list_head;
		event_list_head = el->next;
		delete el;
	}
	event_list_curr = event_list_head = NULL;
}

/*
 * This barrier is different from upcxx::barrier. Here we do not
 * call upcxx::advance() as in upcxx::barrier().
 */
int barrier() {
	int rv;
	gasnet_barrier_notify(0, GASNET_BARRIERFLAG_ANONYMOUS);
	while ((rv=gasnet_barrier_try(0, GASNET_BARRIERFLAG_ANONYMOUS))
			== GASNET_ERR_NOT_READY) {
	}
	assert(rv == GASNET_OK);
	return UPCXX_SUCCESS;
}

/*
 * We use cancellable barrier for global termination detection.
 * This is adapted from:
 * Olivier, S., Prins, J.: Scalable dynamic load balancing using upc. In: ICPP. pp.123Ð131 (2008)
 */
upcxx::shared_var<int> cb_cancel;
upcxx::shared_var<int> cb_count;
upcxx::shared_var<int> cb_done;
upcxx::shared_lock cb_lock;
bool workAvailInit = false;
#define CB_LOCK		cb_lock.lock()
#define CB_UNLOCK	cb_lock.unlock()
#define NO_TERM   0
#define TERM      1

volatile int idle_workers;	// counts computation workers only
static int idle_workers_threshold = 0;

inline bool initiate_global_steal() {
	return idle_workers > idle_workers_threshold;
}

/*
 * termination detection helper function
 */
void cb_init() {
	CB_LOCK;
	cb_count = 0;
	cb_cancel = 0;
	cb_done = 0;
	CB_UNLOCK;
#ifdef DIST_WS
	hcpp::registerHCUPC_callback(&idle_workers);
	idle_workers_threshold = hc_workers > 12 ? 2 : 0;	// heuristic for 24 core edison node only !!!
#endif
}

/*
 * termination detection helper function
 */
inline int cbarrier_inc() {
	CB_LOCK;
	cb_count = cb_count + 1;
	CB_UNLOCK;
	if (cb_count == THREADS) {
		return TERM;
	}
	else {
		return NO_TERM;
	}
}

/* cbarrier_dec allows thread to exit barrier wait, but not after
 * barrier has been achieved
 */
inline int cbarrier_dec() {
	int status = TERM;
	CB_LOCK;
	if (cb_count < THREADS) {
		status = NO_TERM;
		cb_count = cb_count - 1;
	}
	CB_UNLOCK;
	return status;
}

inline int cbarrier_test() {
	return  (cb_count == THREADS)? TERM : NO_TERM;
}

/*
 * To enable portability across OCR and CRT we using a hcupc
 * level push rather than calling asyncComm directly and relying
 * on intra-node work-stealing runtime.
 */
void send_taskto_comm_worker(comm_async_task* task) {
	// increment the finish counter
	HASSERT(current_finish_counter);
	hcpp_atomic_inc(current_finish_counter);
	// push the task to deque maintained in hcupc
	comm_task_push(task);
}

/*
 * This is called by comm worker in each iteration
 * of the termination detection loop.
 */
inline void pop_execute_comm_task() {
	bool success = true;
	while(success) {
		comm_async_task task;
		success = comm_task_pop(&task);
		if(success) {
			// increment outgoing task counter
			increment_task_in_flight_self();
			// statistics -- increment outgoing task counter
			increment_outgoing_tasks();
			// execute the task
			(task._fp)(task._args);
			// decrement finish counter
			HASSERT(current_finish_counter);
			hcpp_atomic_dec(current_finish_counter);
		}
	}
}

#ifdef DIST_WS
//#define BASELINE_IMP

void hcpp_finish_barrier() {
	int status = NO_TERM;

	hupcpp::barrier();
	const bool singlePlace = THREADS == 1;

	while (status != TERM) {
		pop_execute_comm_task();

		upcxx::advance();
#ifndef BASELINE_IMP
		bool tasksReceived = received_tasks_from_victim();
		if(tasksReceived) {
			decrement_tasks_in_flight_count();
		}
#endif
		bool tryTermination = true;

		// 1. If i have extra tasks then I should feed others
		if(!initiate_global_steal()) {
			serve_pending_distSteal_request();
		}

		// 2. If my workers are asking global steals then I should try dist ws
		else {
			bool success = search_tasks_globally();
			if(success) tryTermination = false;
		}

		// 3. If I failed stealing and my workers are idle, I should try termination
		if(tryTermination && totalPendingLocalAsyncs() == 0 && total_asyncs_inFlight() == 0) {
			if(singlePlace) break;
			status = cbarrier_inc();
			while (status != TERM) {
#ifdef BASELINE_IMP
				const bool isTrue = detectWork() && cbarrier_dec() != TERM;
#else
				const bool isTrue = (received_tasks_from_victim() || detectWork()) && cbarrier_dec() != TERM;
#endif
				if (isTrue) {
					status = NO_TERM;
					break;
				}
				else {
					upcxx::advance();	// only when this place is sure all other place are idle
				}
				status = cbarrier_test();
			}
		}
	}

	HASSERT(totalPendingLocalAsyncs() == 0);

	hupcpp::barrier();
}

#else

void hcpp_finish_barrier() {
	int status = NO_TERM;

	hupcpp::barrier();
	const bool singlePlace = THREADS == 1;

	while (status != TERM) {
		pop_execute_comm_task();

		publish_local_load_info();

		upcxx::advance();

		bool tasksReceived = received_tasks_from_victim();
		if(tasksReceived) {
			decrement_tasks_in_flight_count();
		}

		if(totalPendingLocalAsyncs() == 0 && total_asyncs_inFlight() == 0) {
			if(singlePlace) break;
			status = cbarrier_inc();
			while (status != TERM) {
				const bool isTrue = (received_tasks_from_victim() || detectWork()) && cbarrier_dec() != TERM;
				if (isTrue) {
					status = NO_TERM;
					break;
				}
				else {
					upcxx::advance();	// only when this place is sure all other place are idle
				}
				status = cbarrier_test();
			}
		}
	}

	HASSERT(totalPendingLocalAsyncs() == 0);

	hupcpp::barrier();
}

#endif

void finish_spmd(std::function<void()> lambda) {
	HASSERT(hc_workers_initialized);
	cb_init();
	/*
	 * we support distributed workstealing only if each
	 * place has a communication worker, i.e. hc_workers > 1
	 */
	hupcpp::barrier();
	const bool comm_worker = true;//hc_workers > 1 && THREADS > 1;

	current_finish_counter = hcpp::start_finish_special();

	// launch async tasks in immediate scopr of the finish_spmd in user program
	lambda();

	if(comm_worker) {
		/*
		 * The termiantion detection is the job of communication worker.
		 * This is also treated as an async task and hence we wrap the
		 * termination detection task in an async_comm such that it
		 * gets scheduled only at out_deq of communication worker. It
		 * has while loop, which the comm worker loops upon once it pops
		 * this task from its out_deq.
		 *
		 * We never allow any other/more task to queue at out_deq of comm worker
		 * as in this design it will never pop and execute. We maintain an auxiliary
		 * deque outside the OCR/CRT runtine i.e. inside hcupc_spmd and mark all the ocr's out_deq
		 * bounded task to this deque. Inside the termiantion detection phase the comm worker
		 * pops the tasks as necessary.
		 */
		auto comm_lambda_finish = [=] () {
			hcpp_finish_barrier();
		};
		asyncComm(comm_lambda_finish);
	}

	/*
	 * this finish will end only if all the local and remote tasks terminates
	 */
	end_finish();
	free_upcxx_event_list();
}

/*
 * NOTE: this is a collective routine
 */

void runtime_statistics(double duration) {
#ifdef HC_COMM_WORKER_STATS
	shared_array<int> push_outd_myPlace;
	shared_array<int> push_ind_myPlace;
	shared_array<int> steal_ind_myPlace;

	push_outd_myPlace.init(THREADS);
	push_ind_myPlace.init(THREADS);
	steal_ind_myPlace.init(THREADS);

	int c1, c2, c3;
	hcpp::gather_commWorker_Stats(&c1, &c2, &c3);

	push_outd_myPlace[MYTHREAD] = c1;
	push_ind_myPlace[MYTHREAD] = c2;
	steal_ind_myPlace[MYTHREAD] = c3;
#endif

	shared_array<counter_t> tasks_sendto_remotePlace;
	shared_array<counter_t> total_dist_successSteals;
	shared_array<counter_t> total_dist_failedSteals;
	shared_array<counter_t> total_recvStealReqsWhenFree;
	shared_array<counter_t> total_cyclic_steals;
	shared_array<counter_t> total_rdma_probes;

	tasks_sendto_remotePlace.init(THREADS);
	total_dist_successSteals.init(THREADS);
	total_dist_failedSteals.init(THREADS);
	total_recvStealReqsWhenFree.init(THREADS);
	total_cyclic_steals.init(THREADS);
	total_rdma_probes.init(THREADS);

	counter_t c4, c5, c6, c7, c8, c9;

	get_totalAsyncAny_stats(&c4, &c5, &c6, &c7, &c8, &c9);

	tasks_sendto_remotePlace[MYTHREAD] = c4;
	total_dist_successSteals[MYTHREAD] = c5;
	total_dist_failedSteals[MYTHREAD] = c6;
	total_recvStealReqsWhenFree[MYTHREAD] = c7;
	total_cyclic_steals[MYTHREAD] = c8;
	total_rdma_probes[MYTHREAD] = c9;

	int s1=0, s2=0, s3=0, s4=0, s4P=0;
	get_steal_stats(&s1, &s2, &s3, &s4, &s4P);

	shared_array<int> total_steal_1;
	shared_array<int> total_steal_2;
	shared_array<int> total_steal_3;
	shared_array<int> total_steal_4;
	shared_array<int> total_steal_4P;

	total_steal_1.init(THREADS);
	total_steal_2.init(THREADS);
	total_steal_3.init(THREADS);
	total_steal_4.init(THREADS);
	total_steal_4P.init(THREADS);

	total_steal_1[MYTHREAD] = s1;
	total_steal_2[MYTHREAD] = s2;
	total_steal_3[MYTHREAD] = s3;
	total_steal_4[MYTHREAD] = s4;
	total_steal_4P[MYTHREAD] = s4P;

	hupcpp::barrier();
#ifdef DIST_WS
	if(MYTHREAD == 0) {
#ifdef HC_COMM_WORKER_STATS
		int t1=0, t2=0, t3=0;
#endif
		counter_t t4=0, t5=0, t6=0, t7=0, t7b=0, t7c=0;
		int t8=0,t9=0,t10=0,t11=0,t12=0;
		for(int i=0; i<THREADS; i++) {
#ifdef HC_COMM_WORKER_STATS
			t1 += push_outd_myPlace[i];
			t2 += push_ind_myPlace[i];
			t3 += steal_ind_myPlace[i];
#endif
			t4 += tasks_sendto_remotePlace[i];
			t5 += total_dist_successSteals[i];
			t6 += total_dist_failedSteals[i];
			t7 += total_recvStealReqsWhenFree[i];
			t7b += total_cyclic_steals[i];
			t7c += total_rdma_probes[i];

			t8 += total_steal_1[i];
			t9 += total_steal_2[i];
			t10 += total_steal_3[i];
			t11 += total_steal_4[i];
			t12 += total_steal_4P[i];
		}

		printf("============================ MMTk Statistics Totals ============================\n");
#ifdef HC_COMM_WORKER_STATS
		printf("time.mu\ttotalPushOutDeq\ttotalPushInDeq\ttotalStealsInDeq\ttotalTasksSendToRemote\ttotalSuccessDistSteals\ttotalFailedDistSteals\ttotalIncomingStealReqsWhenIdle\ttotalCyclicSteals\ttotalRDMAasyncAnyProbes");
		printf("\tS1\tS2\tS3\tS4\tS4Plus\n");

		printf("%.3f\t%d\t%d\t%d\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu",duration,t1,t2, t3, t4, t5, t6, t7, t7b, t7c);
		printf("\t%d\t%d\t%d\t%d\t%d\n",t8,t9,t10,t11,t12);

#else
		printf("time.mu\ttotalTasksSendToRemote\ttotalSuccessDistSteals\ttotalFailedDistSteal\ttotalIncomingStealReqsWhenIdle\ttotalCyclicSteals\ttotalRDMAasyncAnyProbes");
		printf("\tS1\tS2\tS3\tS4\tS4Plus\n");

		printf("%.3f\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu",duration,t4, t5, t6, t7, t7b, t7c);
		printf("\t%d\t%d\t%d\t%d\t%d\n",t8,t9,t10,t11,t12);

#endif
		printf("Total time: %.3f ms\n",duration);
		printf("------------------------------ End MMTk Statistics -----------------------------\n");
		printf("===== TEST PASSED in %.3f msec =====\n",duration);
	}
#else // !DIST_WS
	if(MYTHREAD == 0) {
#ifdef HC_COMM_WORKER_STATS
		int t1=0, t2=0, t3=0;
#endif
		counter_t t4=0;
		for(int i=0; i<THREADS; i++) {
#ifdef HC_COMM_WORKER_STATS
			t1 += push_outd_myPlace[i];
			t2 += push_ind_myPlace[i];
			t3 += steal_ind_myPlace[i];
#endif
			t4 += tasks_sendto_remotePlace[i];
		}

		printf("============================ MMTk Statistics Totals ============================\n");
#ifdef HC_COMM_WORKER_STATS
		printf("time.mu\ttotalPushOutDeq\ttotalPushInDeq\ttotalStealsInDeq\ttotalTasksSendToRemote\n");
		printf("%.3f\t%d\t%d\t%d\t%llu\n",duration,t1,t2, t3, t4);

#else
		printf("time.mu\ttotalTasksSendToRemote\n");
		printf("%.3f\t%llu\n",duration,t4);

#endif
		printf("Total time: %.3f ms\n",duration);
		printf("------------------------------ End MMTk Statistics -----------------------------\n");
		printf("===== TEST PASSED in %.3f msec =====\n",duration);
	}
#endif // end DIST_WS
}

static double benchmark_start_time_stats = 0;

double mysecond() {
	struct timeval tv;
	gettimeofday(&tv, 0);
	return tv.tv_sec + ((double) tv.tv_usec / 1000000);
}

void showStatsHeader() {
	if(MYTHREAD == 0) {
		cout << endl;
		cout << "-----" << endl;
		cout << "mkdir timedrun fake" << endl;
		cout << endl;
	}
	initialize_hcWorker();
	if(MYTHREAD == 0) {
		cout << endl;
		cout << "-----" << endl;
	}
	benchmark_start_time_stats = mysecond();
}

void showStatsFooter() {
	double end = mysecond();
	HASSERT(benchmark_start_time_stats != 0);
	double dur = (end-benchmark_start_time_stats)*1000;	// timer is switched off before printing topology
	if(MYTHREAD == 0) {
		print_topology_information();
	}
	runtime_statistics(dur);
}
}

