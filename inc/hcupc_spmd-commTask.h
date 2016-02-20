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


namespace hupcpp {
/*
 * The comm_async_task structure is similar to hclib::crt_async_task, but redefining locally
 * here to achieve portability across OCR and CRT
 */

/*
 * We just need to pack the function pointer and the pointer to
 * the heap allocated lambda. Due to this 16 bytes is sufficient
 * for our lambda based approach.
 */
#define MAX_COMM_ASYNC_ARG_SIZE 16
typedef void (*comm_async_generic_framePtr)(void*);

template <typename Function, typename T1>
struct comm_async_arguments1 {
	Function kernel;
	T1 arg1;

	comm_async_arguments1(Function k, T1 a1) :
		kernel(k), arg1(a1) { }
};

template<typename Function, typename T1>
void comm_wrapper1(void *args) {
	comm_async_arguments1<Function, T1> *a =
			(comm_async_arguments1<Function, T1> *)args;

	(*a->kernel)(a->arg1);
}

struct comm_async_task  {
	char _args[MAX_COMM_ASYNC_ARG_SIZE];
	comm_async_generic_framePtr _fp;
	bool involve_communication;
	inline void init_comm_async_task(comm_async_generic_framePtr fp, size_t arg_sz, void *async_args) {
		HASSERT(arg_sz <= MAX_COMM_ASYNC_ARG_SIZE);
		this->_fp = fp;
		/*
		 * By default all communication worker tasks involves communication.
		 * There are some special cases where we want some task to be executed
		 * only by communication worker but it does not involves communication and
		 * hence should not increment task in flight counter.
		 */
		this->involve_communication = true;
		memcpy(&this->_args, async_args, arg_sz);
	}

	inline comm_async_task() { };

	template<typename Function, typename T1>
	inline comm_async_task(Function kernel, const T1 &a1) {
		comm_async_arguments1<Function, T1> args(kernel, a1);
		init_comm_async_task(comm_wrapper1<Function, T1>, (size_t)sizeof(args), (void *)&args);
	}
};

void send_taskto_comm_worker(comm_async_task* entry);
bool comm_task_pop(comm_async_task* buff);
void comm_task_push(comm_async_task* entry);

template <typename T>
inline void execute_comm_task(T* lambda) {
	(*lambda)();
	hcupc_free((void*) lambda);
}

template <typename T>
inline void allocate_comm_task(T lambda) {
	const size_t nbytes = sizeof(T);
	T* task = (T*) hcupc_malloc(nbytes);
	memcpy(task, &lambda, nbytes);
	comm_async_task cb = comm_async_task(execute_comm_task<T>, task);
	send_taskto_comm_worker(&cb);
}

/**
 * This task is not meant to be used in user code. Only meant to
 * be used in runtime for special cases.
 *
 * By default all communication worker tasks involves communication.
 * There are some special cases where we want some task to be executed
 * only by communication worker but it does not involves communication and
 * hence should not increment task in flight counter.
 */
template <typename T>
inline void __intraPlace_asyncComm(T lambda) {
	const size_t nbytes = sizeof(T);
	T* task = (T*) hcupc_malloc(nbytes);
	memcpy(task, &lambda, nbytes);
	comm_async_task cb = comm_async_task(execute_comm_task<T>, task);
	cb.involve_communication = false;
	send_taskto_comm_worker(&cb);
}

// runs at destination
template <typename T>
inline void async_at_received(upcxx::global_ptr<T> remote_lambda) {
	const size_t source = remote_lambda.where();
	HASSERT(source != upcxx::global_myrank());
	upcxx::global_ptr<T> my_lambda = upcxx::allocate<T>(upcxx::global_myrank(), 1);
	upcxx::copy(remote_lambda, my_lambda, 1);

	(*((T*)(my_lambda.raw_ptr())))();

	upcxx::deallocate(my_lambda);
	upcxx::deallocate(remote_lambda);
	queue_source_place_of_remoteTask(source);
}

// runs at source
template <typename T>
inline void asyncAt(int p, T lambda) {
	HASSERT(p != upcxx::global_myrank());
	auto lambda2 = [=]() {
		auto lambda_dest = [lambda]() {
			hupcpp::async([lambda]() {
				lambda();
			});
		};
		upcxx::global_ptr<decltype(lambda_dest)> remote_lambda = upcxx::allocate<decltype(lambda_dest)>(upcxx::global_myrank(), 1);
		memcpy(remote_lambda.raw_ptr(), &lambda_dest, sizeof(decltype(lambda_dest)));
		upcxx::async(p, NULL)(async_at_received<decltype(lambda_dest)>, remote_lambda);
	};
	allocate_comm_task<decltype(lambda2)>(lambda2);
}

inline void async_after_async_copy(void* promise) {
	queue_source_place_of_remoteTask(upcxx::global_myrank());
	if(promise) {
		register_incoming_async_promise(promise);
	}
}

template <typename T>
inline void async_copy(upcxx::global_ptr<T> src, upcxx::global_ptr<T> dst, size_t count, promise_t* promise=NULL) {
	auto lambda = [=]() {
		upcxx::event* e = get_upcxx_event();
		HASSERT(e != NULL);
		upcxx::async_copy(src, dst, count, e);
		upcxx::async_after(upcxx::global_myrank(), e, NULL)(async_after_async_copy, (void*)promise);
	};
	allocate_comm_task<decltype(lambda)>(lambda);
}

// runs at source
#ifdef DIST_WS
template <typename T>
inline void asyncAny(T lambda) {
	hcupc_asyncAny<T>(lambda);
}

inline void unwrap_asyncAny_task(hclib::remoteAsyncAny_task task) {
	(*task._fp)(task._args);
}

inline void unwrap_n_asyncAny_tasks(const hclib::remoteAsyncAny_task* tasks, int count) {
	for(int j=0; j<count; j++) {
		hclib::remoteAsyncAny_task tmp;
		memcpy((void*)&tmp, (void*)&tasks[j], sizeof(hclib::remoteAsyncAny_task));
		hclib::async([tmp]() {
			unwrap_asyncAny_task(tmp);
		});
	}
}
#else
template <typename T>
inline void asyncAny(T lambda) {
        assert("Rebuild HabaneroUPC++ with --enable-distWS build options" && false);
}
#endif
}
