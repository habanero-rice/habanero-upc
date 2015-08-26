namespace hupcpp {
/*
 * The comm_async_task structure is similar to hcpp::crt_async_task, but redefining locally
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
	inline void init_comm_async_task(comm_async_generic_framePtr fp, size_t arg_sz, void *async_args) {
		HASSERT(arg_sz <= MAX_COMM_ASYNC_ARG_SIZE);
		this->_fp = fp;
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

// runs at destination
template <typename T>
inline void async_at_received(upcxx::global_ptr<T> remote_lambda) {
	const size_t source = remote_lambda.where();
	HASSERT(source != MYTHREAD);
	upcxx::global_ptr<T> my_lambda = upcxx::allocate<T>(MYTHREAD, 1);
	upcxx::copy(remote_lambda, my_lambda, 1);

	(*((T*)(my_lambda.raw_ptr())))();

	upcxx::deallocate(my_lambda);
	upcxx::deallocate(remote_lambda);
	queue_source_place_of_remoteTask(source);
}

// runs at source
template <typename T>
inline void asyncAt(int p, T lambda) {
	HASSERT(p != MYTHREAD);
	auto lambda2 = [=]() {
		auto lambda_dest = [lambda]() {
			hupcpp::async([lambda]() {
				lambda();
			});
		};
		upcxx::global_ptr<decltype(lambda_dest)> remote_lambda = upcxx::allocate<decltype(lambda_dest)>(MYTHREAD, 1);
		memcpy(remote_lambda.raw_ptr(), &lambda_dest, sizeof(decltype(lambda_dest)));
		upcxx::async(p, NULL)(async_at_received<decltype(lambda_dest)>, remote_lambda);
	};
	allocate_comm_task<decltype(lambda2)>(lambda2);
}

inline void async_after_asyncCopy(void* ddf) {
	queue_source_place_of_remoteTask(MYTHREAD);
	if(ddf) {
		register_incoming_async_ddf(ddf);
	}
}

template <typename T>
inline void asyncCopy(upcxx::global_ptr<T> src, upcxx::global_ptr<T> dst, size_t count, DDF_t* ddf=NULL) {
	auto lambda = [=]() {
		upcxx::event* e = get_upcxx_event();
		HASSERT(e != NULL);
		upcxx::async_copy(src, dst, count, e);
		upcxx::async_after(MYTHREAD, e, NULL)(async_after_asyncCopy, (void*)ddf);
	};
	allocate_comm_task<decltype(lambda)>(lambda);
}

// runs at source
#ifdef DIST_WS
template <typename T>
inline void asyncAny(T lambda) {
	hcupc_asyncAny<T>(lambda);
}

inline void unwrap_asyncAny_task(hcpp::remoteAsyncAny_task task) {
	(*task._fp)(task._args);
}

inline void unwrap_n_asyncAny_tasks(const hcpp::remoteAsyncAny_task* tasks, int count) {
	for(int j=0; j<count; j++) {
		hcpp::remoteAsyncAny_task tmp;
		memcpy((void*)&tmp, (void*)&tasks[j], sizeof(hcpp::remoteAsyncAny_task));
		hcpp::async([tmp]() {
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
