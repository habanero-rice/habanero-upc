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
 *      Author: Vivek Kumar (vivekk@rice.edu)
 */

#include "hcupc_spmd-internal.h"
#include "hcupc_spmd_common_methods.h"
#include "hcupc_spmd-commTask.h"
#include "hcupc_spmd-atomics.h"
#include  <mutex>

namespace hupcpp {

static int hc_workers;
static bool hc_workers_initialized = false;
volatile int* current_finish_counter = NULL;

extern "C" {
extern void (*hclib_distributed_future_register_callback)(
        hclib::promise_t** promise_list);
}

void launch(int *argc, char ***argv, std::function<void()> lambda) {
    hclib_distributed_future_register_callback = dpromise_register_callback;

    hclib::launch(argc, *argv, [=]() {
        hclib::finish([=] {
            hclib::async_comm([=] {
                upcxx::init(argc, argv);
                fprintf(stderr, "Initializing %d upcxx %p\n", upcxx::global_myrank(), pthread_self());
            });
        });

        hupcpp::showStatsHeader();
        hclib::finish([=] {
            hclib::async([=] {
                lambda();
            });
        });
        hupcpp::showStatsFooter();

        hclib::finish([=] {
            hclib::async_comm([=] {
                fprintf(stderr, "Finalizing %d %p\n", upcxx::global_myrank(), pthread_self());
                upcxx::finalize();
            });
        });
    });
}

void initialize_hcWorker() {
    finish([=] {
        async_comm([=] {
            initialize_distws_set_of_thieves();
        });
    });
	hc_workers = numWorkers();
	assert(hc_workers > 0);
	semiConcDequeInit();
	hc_workers_initialized = true;
}

/*
 * upcxx::event management for async_copy
 */
typedef struct event_list {
	upcxx::event e;
	event_list* next;
} event_list;

static event_list* event_list_head = NULL;
static event_list* event_list_curr;

// c++11 mutex to ensure thread-safety while
// creating and upcxx::event
std::mutex event_mutex;

/*
 * Ensure this is thread safe as multiple computation
 * workers call this routine concurrently
 */
upcxx::event* get_upcxx_event() {
	event_list *el = new event_list;
	el->next = NULL;

	event_mutex.lock();
	if(!event_list_head) {
		event_list_head = event_list_curr = el;
	}
	else {
		event_list_curr->next = el;
		event_list_curr = el;
	}
	event_mutex.unlock();

	return &(el->e);
}

/*
 * upcxx::event management for async_copy
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
 * Olivier, S., Prins, J.: Scalable dynamic load balancing using upc. In: ICPP. pp.123�131 (2008)
 */
static upcxx::shared_array<upcxx::atomic<unsigned int > > cb_count;
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
static void cb_init() {
    cb_count.init(1);
    // cb_count = 0;
#ifdef DIST_WS
	hclib::registerHCUPC_callback(&idle_workers);
	idle_workers_threshold = hc_workers > 12 ? 2 : 0;	// heuristic for 24 core edison node only !!!
#endif
}

/*
 * termination detection helper function
 */
inline int cbarrier_inc() {
    global_ptr<upcxx::atomic<unsigned int> > obj = &cb_count[0];
    unsigned int curr_count = upcxx::fetch_add(obj, 1);
	if (curr_count + 1 == upcxx::global_ranks()) {
		return TERM;
	} else {
		return NO_TERM;
	}
}

/* cbarrier_dec allows thread to exit barrier wait, but not after
 * barrier has been achieved
 */
inline int cbarrier_dec() {
	int status = TERM;
    global_ptr<upcxx::atomic<unsigned int> > obj = &cb_count[0];
    if (upcxx::fetch_add(obj, 0) < upcxx::global_ranks()) {
		status = NO_TERM;
        upcxx::fetch_add(obj, -1);
	}
	return status;
}

inline int cbarrier_test() {
    global_ptr<upcxx::atomic<unsigned int> > obj = &cb_count[0];
	return  (upcxx::fetch_add(obj, 0) == upcxx::global_ranks())? TERM : NO_TERM;
}

/*
 * To enable portability across OCR and CRT we using a hcupc
 * level push rather than calling asyncComm directly and relying
 * on intra-node work-stealing runtime.
 */
void send_taskto_comm_worker(comm_async_task* task) {
	// increment the finish counter
	HASSERT(current_finish_counter);
	hclib_atomic_inc(current_finish_counter);
	// push the task to deque maintained in hcupc
	comm_task_push(task);
}

/*
 * This is called by comm worker in each iteration
 * of the termination detection loop.
 */
inline void pop_execute_comm_task() {
	bool success = true;
	while (success) {
		comm_async_task task;
		success = comm_task_pop(&task);
		if(success) {
			if(task.involve_communication) {
				/*
				 * By default all communication worker tasks involves communication.
				 * There are some special cases where we want some task to be executed
				 * only by communication worker but it does not involves communication and
				 * hence should not increment task in flight counter.
				 */
				// increment outgoing task counter
				increment_task_in_flight_self();
				// statistics -- increment outgoing task counter
				increment_outgoing_tasks();
			}
			// execute the task
			(task._fp)(task._args);
			// decrement finish counter
			HASSERT(current_finish_counter);
			hclib_atomic_dec(current_finish_counter);
		}
	}
}

#ifdef DIST_WS

const static char* baseline_distWS = getenv("HCLIB_DIST_WS_BASELINE");

void hclib_finish_barrier_distWS() {
	int status = NO_TERM;

	hupcpp::barrier();
	const bool singlePlace = upcxx::global_ranks() == 1;

	while (status != TERM) {
		pop_execute_comm_task();

		upcxx::advance();
		bool tasksReceived = received_tasks_from_victim();
		if (tasksReceived) {
			decrement_tasks_in_flight_count();
		}
		bool tryTermination = true;

		if (!initiate_global_steal()) {
            // 1. If i have extra tasks then I should feed others
			serve_pending_distSteal_request();
		} else {
            // 2. If my workers are asking global steals then I should try dist ws
			bool success = search_tasks_globally();
			if (success) tryTermination = false;
		}

		if (tryTermination && total_pending_local_asyncs() == 0 &&
                total_asyncs_in_flight() == 0) {
            // 3. If I failed stealing and my workers are idle, I should try termination
			if (singlePlace) break;
			status = cbarrier_inc();
			while (status != TERM) {
                const bool isTrue = (received_tasks_from_victim() ||
                        detect_work()) && cbarrier_dec() != TERM;
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

	HASSERT(total_pending_local_asyncs() == 0);

	hupcpp::barrier();
}

void hclib_finish_barrier_baseline_distWS() {
	int status = NO_TERM;

	hupcpp::barrier();
	const bool singlePlace = upcxx::global_ranks() == 1;

	while (status != TERM) {
		pop_execute_comm_task();

		upcxx::advance();
		bool tryTermination = true;

		// 1. If i have extra tasks then I should feed others
		if(!initiate_global_steal()) {
			serve_pending_distSteal_request_baseline();
		}

		// 2. If my workers are asking global steals then I should try dist ws
		else {
			bool success = search_tasks_globally_baseline();
			if(success) tryTermination = false;
		}

		// 3. If I failed stealing and my workers are idle, I should try termination
		if(tryTermination && total_pending_local_asyncs() == 0 && total_asyncs_in_flight() == 0) {
			if(singlePlace) break;
			status = cbarrier_inc();
			while (status != TERM) {
				const bool isTrue = detect_work() && cbarrier_dec() != TERM;
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

	HASSERT(total_pending_local_asyncs() == 0);

	hupcpp::barrier();
}

void hclib_finish_barrier() {
	if(baseline_distWS) {
		hclib_finish_barrier_baseline_distWS();
	}
	else {
		hclib_finish_barrier_distWS();
	}
}

#else

void hclib_finish_barrier() {
	int status = NO_TERM;

	hupcpp::barrier();
	const bool singlePlace = upcxx::global_ranks() == 1;

	while (status != TERM) {
        fprintf(stderr, "WAITING %d %p\n", upcxx::global_myrank(), pthread_self());
		pop_execute_comm_task();

		publish_local_load_info();

		upcxx::advance();

		bool tasksReceived = received_tasks_from_victim();
		if (tasksReceived) {
			decrement_tasks_in_flight_count();
		}

		if (total_pending_local_asyncs() == 0 && total_asyncs_in_flight() == 0) {
			if (singlePlace) break;
			status = cbarrier_inc();
			while (status != TERM) {
				const bool isTrue = (received_tasks_from_victim() || detect_work()) && cbarrier_dec() != TERM;
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
    fprintf(stderr, "DONE WAITING\n");

	HASSERT(total_pending_local_asyncs() == 0);

	hupcpp::barrier();
}

#endif

void finish_spmd(std::function<void()> lambda) {
    HASSERT(hc_workers_initialized);
    hclib::finish([=] {
            async_comm([=] {
                cb_init();
            });
        });
    start_finish_spmd_timer();

    hupcpp::barrier();

    /*
     * Start a finish scope, the only "special" thing about start_finish_special
     * is that it returns a pointer to the task counter for the created finish
     * scope which we save here. Otherwise, it is a regular start_finish call.
     */
    current_finish_counter = hclib::start_finish_special();

    // launch async tasks in immediate scope of the finish_spmd in user program
    lambda();

    /*
     * The termination detection is the job of communication worker.
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
    async_comm([=] {
        fprintf(stderr, "hclib_finish_barrier %d %p\n", upcxx::global_myrank(), pthread_self());
        hclib_finish_barrier();
    });

    /*
     * this finish will end only if all the local and remote tasks terminates
     */
    fprintf(stderr, "Before final end finish\n");
    hclib::end_finish();
    fprintf(stderr, "After final end finish\n");
    free_upcxx_event_list();
    end_finish_spmd_timer();
}

}

