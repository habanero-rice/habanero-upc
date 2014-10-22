#include "hcupc_spmd-internal.h"
#include "hcupc_spmd_common_methods.h"

namespace hcpp {

typedef struct finish_spmd_t {
	volatile int          out_counter;  // remote asyncs launched, updated by all computation asyncs
	volatile int   		  in_counter;  // remote asyncs received.
	bool                  done;
	pthread_mutex_t 	  copy_lock;
	struct finish_spmd_t  *parent;
	inline finish_spmd_t() {
		out_counter = 0;
		in_counter = 0;
		done = false;
		parent = NULL;
		int ret = pthread_mutex_init(&copy_lock, NULL);
		HCPP_ASSERT(ret == 0);
	}
	~finish_spmd_t() {
		pthread_mutex_destroy(&copy_lock);
	}
} finish_spmd_t;

static finish_spmd_t* current_finish_scope = NULL;
static int hc_workers;
static bool hc_workers_initialized = false;

void initialize_hcWorker() {
	hc_workers = get_worker_count();
	assert(hc_workers > 0);
}

int barrier() {
	int rv;
	gasnet_barrier_notify(0, GASNET_BARRIERFLAG_ANONYMOUS);
	while ((rv=gasnet_barrier_try(0, GASNET_BARRIERFLAG_ANONYMOUS))
			== GASNET_ERR_NOT_READY) {
	}
	assert(rv == GASNET_OK);
	return UPCXX_SUCCESS;
}

void upcxx_copy_lock() {
	HCPP_ASSERT(current_finish_scope != NULL);
	pthread_mutex_lock(&(current_finish_scope->copy_lock));
}

void upcxx_copy_unlock() {
	HCPP_ASSERT(current_finish_scope != NULL);
	pthread_mutex_unlock(&(current_finish_scope->copy_lock));
}

async_task * advance_in_task_queue_hcpp(queue_t *inq, int max_dispatched)
{
	async_task *task = NULL;
	int num_dispatched = 0;

	gasnet_AMPoll(); // make progress in GASNet

	// Execute tasks in the async queue
	while (!queue_is_empty(inq)) {
		// dequeue an async task
		gasnet_hsl_lock(&in_task_queue_lock);
		task = (async_task *)queue_dequeue(inq);
		gasnet_hsl_unlock(&in_task_queue_lock);

		HCPP_ASSERT (task != NULL);
		HCPP_ASSERT (task->_callee == myrank());

		num_dispatched++;
		if (num_dispatched >= max_dispatched) break;
	}; // end of while (!queue_is_empty(inq))

	return task;
} // end of poll_in_task_queue;

int advance_out_task_queue_hcpp(queue_t *outq, int max_dispatched)
{
	async_task *task;
	int num_dispatched = 0;

	// Execute tasks in the async queue
	while (!queue_is_empty(outq)) {
		// dequeue an async task
		gasnet_hsl_lock(&out_task_queue_lock);
		task = (async_task *)queue_dequeue(outq);
		gasnet_hsl_unlock(&out_task_queue_lock);
		HCPP_ASSERT (task != NULL);
		HCPP_ASSERT (task->_callee != myrank());

		// remote async task
		// Send AM "there" to request async task execution
		GASNET_SAFE(gasnet_AMRequestMedium0(task->_callee, ASYNC_AM,
				task, task->nbytes()));
		delete task;
		num_dispatched++;
		if (num_dispatched >= max_dispatched) break;
	} // end of while (!queue_is_empty(outq))

	return num_dispatched;
} // end of poll_out_task_queue()

void* advance_upcxx()
{
	advance_out_task_queue_hcpp(out_task_queue, MAX_DISPATCHED_OUT);
	void* task = advance_in_task_queue_hcpp(in_task_queue, 1);
	// check outstanding events
	if (!outstanding_events.empty()) {
		for (std::list<event*>::iterator it = outstanding_events.begin();
				it != outstanding_events.end(); ++it) {
			// cerr << "Number of outstanding_events: " << outstanding_events.size() << endl;
			event *e = (*it);
			HCPP_ASSERT(e != NULL);
			// fprintf(stderr, "P %u Advance event: %p\n", myrank(), e);
			e->test();
			break;
		}
	}
	return task;
} // advance()

void execute_upcxx(void *t) {
	HCPP_ASSERT(t != NULL);
	async_task* task = (async_task*) t;
	if (task->_fp) {
		(*task->_fp)(task->_args);
	}
}

void execute_upcxx_ack(void *t) {
	HCPP_ASSERT(t != NULL);
	async_task* task = (async_task*) t;
	if (task->_ack != NULL) {
		if (task->_caller == myrank()) {
			// local event acknowledgment
			task->_ack->decref(); // need to enqueue callback tasks
		} else {
			// send an ack message back to the caller of the async task
			async_done_am_t am;
			am.ack_event = task->_ack;
			GASNET_SAFE(gasnet_AMRequestMedium0(task->_caller,
					ASYNC_DONE_AM,
					&am,
					sizeof(am)));
		}
	}
}

static __inline__ int hcpp_atomic_inc(volatile int *ptr) {
	unsigned char c;
	__asm__ __volatile__(

			"lock       ;\n"
			"incl %0; sete %1"
			: "+m" (*(ptr)), "=qm" (c)
			  : : "memory"
	);
	return c!= 0;
}

void register_incoming_async() {
	HCPP_ASSERT(current_finish_scope != NULL);
	//register this async_at receive by incrementing counter
	hcpp_atomic_inc(&(current_finish_scope->in_counter));
}

void register_incoming_async_ddf(void* d) {
	if(d) {
		struct ddf_st * ddf = (struct ddf_st *) d;
		int* tmp = new int;
		*tmp = 10;
		ddf_put(ddf, (void*)tmp);
	}
	HCPP_ASSERT(current_finish_scope != NULL);
	//register this async_at receive by incrementing counter
	hcpp_atomic_inc(&(current_finish_scope->in_counter));
}

void register_outgoing_async() {
	HCPP_ASSERT(current_finish_scope != NULL);
	//register this async_at by incrementing counter
	hcpp_atomic_inc(&(current_finish_scope->out_counter));
}

void async_wrapper(void *args)
{
	std::function<void()> *lambda = (std::function<void()> *)args;
	(*lambda)();
	delete lambda;
}

void async_comm_hclib(std::function<void()> &&lambda)
{
	std::function<void()> * copy_of_lambda = new std::function<void()> (lambda);
	int PROP = (hc_workers == 1) ? NO_PROP : ASYNC_COMM;
	::async_comm(&async_wrapper, (void *)copy_of_lambda, NO_DDF, NO_PHASER, PROP);
}

void async_hclib(std::function<void()> &&lambda)
{
	std::function<void()> * copy_of_lambda = new std::function<void()> (lambda);
	::async(&async_wrapper, (void *)copy_of_lambda, NO_DDF, NO_PHASER, NO_PROP);
}

finish_spmd_t* allocate_finish_spmd() {
	if(!hc_workers_initialized) {
		initialize_hcWorker();
		hc_workers_initialized = true;
	}
	finish_spmd_t* f = new finish_spmd_t;
	f->parent = current_finish_scope;
	current_finish_scope = f;
	hcpp::barrier();
	return f;
}

void  deallocate_finish_spmd() {
	finish_spmd_t* f = current_finish_scope;
	HCPP_ASSERT(f->done);
	current_finish_scope = current_finish_scope->parent;
	delete(f);
}

void hcpp_finish_barrier(finish_spmd_t* f) {
	while (true) {
		// Poke Habanero runtime (my out_deque) to pop tasks (would push to upcxx queue)
		::end_finish_comm_looper();

		// Poke upcxx to push/pull tasks from upcxx queue
		void* task = advance_upcxx();

		if(task) {
			// send the ack ahead of time. It does not matter.
			execute_upcxx_ack(task);
			// This task should only be executed by the computation worker hence
			// wrap it as an async and rely on runtime to put in my in_deque.
			if(hc_workers > 1) {
				async_hclib([task]() {
					execute_upcxx(task);
					delete ((async_task*)task);
					//register_incoming_async();
				});
			}
			else {
				// do locally
				execute_upcxx(task);
				delete ((async_task*)task);
			}
		}

		// Not sure if this is required. Used this becase its used along with upcxx::advance in upcxx::async_wait
		gasnet_wait_syncnbi_all();

		int work_left_local = f->in_counter - (f->out_counter + totalPendingLocalAsyncs());
		int work_left_global;
		upcxx::upcxx_reduce<int>(&work_left_local, &work_left_global, 1, 0, UPCXX_SUM, UPCXX_INT);
		upcxx::upcxx_bcast(&work_left_global, &work_left_global, sizeof(int), 0);

		if(work_left_global==0) break;
	}
}

void finish_spmd(std::function<void()> lambda) {
	::start_finish();
	finish_spmd_t* f = allocate_finish_spmd();
	::start_finish_comm();
	lambda();
	HCPP_ASSERT(!f->done);
	hcpp_finish_barrier(f);
	::end_finish_comm_free();
	f->done = true;
	deallocate_finish_spmd();
	::end_finish();
}

}

