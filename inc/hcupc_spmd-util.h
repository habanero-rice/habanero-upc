#include "hcpp-utils.h"
#include <upcxx.h>
#include "hcupc_spmd_common_methods.h"

namespace hcpp {

// runs at destination
template <typename T>
void async_at_received(upcxx::global_ptr<T> remote_lambda) {
	upcxx_copy_lock();
	upcxx::global_ptr<T> my_lambda = upcxx::allocate<T>(upcxx::myrank(), 1);
	upcxx::copy(remote_lambda, my_lambda, 1);
	upcxx_copy_unlock();
	(*(T*)my_lambda)(); // execute the lambda
	upcxx::deallocate(my_lambda);
	upcxx::deallocate(remote_lambda);
	register_incoming_async();
}

// runs at source
template <typename T>
void asyncAt(int p, T lambda) {
	std::function<void()> * copy_of_upcxx_call_lambda;
	if(MYTHREAD != p) {
		register_outgoing_async();
		upcxx_copy_lock();
		upcxx::global_ptr<T> remote_lambda = upcxx::allocate<T>(MYTHREAD, 1);
		memcpy(remote_lambda.raw_ptr(), &lambda, sizeof(T));
		upcxx_copy_unlock();
		async_comm_hclib([=]() {
			upcxx::async(p)(async_at_received<T>, remote_lambda);
		});
	}
	else {
		async_comm_hclib(lambda);
	}
}

template <typename T>
void asyncCopy(upcxx::global_ptr<T> src, upcxx::global_ptr<T> dst, size_t count, DDF_t* ddf=NULL) {
	register_outgoing_async();
	async_comm_hclib([=]() {
		upcxx::event e;
		upcxx_copy_lock();
		upcxx::async_copy(src, dst, count, &e);
		upcxx_copy_unlock();
		upcxx::async_after(MYTHREAD, &e)(register_incoming_async_ddf, (void*)ddf);
	});
}

}
