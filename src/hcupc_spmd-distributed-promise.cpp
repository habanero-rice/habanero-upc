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
#include "hcupc_spmd-distributed-promise.h"
#include <unordered_map>

namespace hupcpp {

std::unordered_map<int, dpromise_t*> distributed_promise_guid_map;

// used only for distributed promise related operations and hence not available to users
template <typename T>
inline void _asyncAfter(int p, upcxx::event *e, T lambda) {
	assert(p != MYTHREAD);
	auto lambda2 = [=]() {
		auto lambda_dest = [lambda]() {
			hupcpp::async([lambda]() {
				lambda();
			});
		};
		upcxx::global_ptr<decltype(lambda_dest)> remote_lambda = upcxx::allocate<decltype(lambda_dest)>(MYTHREAD, 1);
		memcpy(remote_lambda.raw_ptr(), &lambda_dest, sizeof(decltype(lambda_dest)));
		upcxx::async_after(p, e, NULL)(async_at_received<decltype(lambda_dest)>, remote_lambda);
	};
	allocate_comm_task<decltype(lambda2)>(lambda2);
}

template <typename T>
inline upcxx::event* _asyncCopy(upcxx::global_ptr<T> src, upcxx::global_ptr<T> dst, size_t count) {
	// event allocation taken out of lambda to implement the distributed promise
	// as in case of remote put we will use this event to launch
	// an upcxx::async_after
	upcxx::event* e = get_upcxx_event(); // thread-safe
	HASSERT(e != NULL);
	auto lambda = [=]() {
		upcxx::async_copy(src, dst, count, e);
		upcxx::async_after(MYTHREAD, e, NULL)(async_after_asyncCopy, (void*)NULL);
	};
	allocate_comm_task<decltype(lambda)>(lambda);
	return e;
}

/*
 * This is a collective call and hence to be executed by all
 * the communication workers
 */
promise_t* __promiseHandle( int gid, int hid, size_t size) {
	/*
	 * We are calling upcxx::allocate hence we do it only
	 * on the comm_worker
	 */
	HASSERT(size > 0);
	HASSERT(hid>=0 && hid<THREADS);

	upcxx::shared_array<dpromise_t> *promise_array = new upcxx::shared_array<dpromise_t>();
	(*promise_array).init(THREADS, 1);
	dpromise_t *myindex_promise_array = (dpromise_t *) &((*promise_array)[MYTHREAD]);

	// store the dpromise_t in hash-table with guid as hash-key
	distributed_promise_guid_map.insert({gid, myindex_promise_array});

	// initialize the local promise_t equivalent pointer for this dpromise_t
	promise_t* promise = &(myindex_promise_array->the_promise);
	hclib::promise_init(promise);	// does not allocate but only initializes
	promise->kind = (hid == MYTHREAD) ? PROMISE_KIND_DISTRIBUTED_OWNER : PROMISE_KIND_DISTRIBUTED_REMOTE;

	// initialize structure variables
	myindex_promise_array->global_id = gid;
	myindex_promise_array->home_rank = hid;
	myindex_promise_array->count = size/sizeof(char);
	myindex_promise_array->putRequestedFromHome = false;
	myindex_promise_array->put_initiator_rank = -1;

	// allocate the datum in global address space
	myindex_promise_array->datum = new upcxx::shared_array<char>();
	(*myindex_promise_array->datum).init(THREADS*(myindex_promise_array->count), myindex_promise_array->count);	// block cyclic allocation of array
	const int kind = promise->kind;
	HASSERT(kind == PROMISE_KIND_DISTRIBUTED_OWNER || kind == PROMISE_KIND_DISTRIBUTED_REMOTE);
	hupcpp::barrier();

	return promise;
}

/*
 * This function is launched at home node to do
 * a local promise_put
 */
inline void PROMISE_PUT_home(int guid, int source) {
	// 1. dereference the guid, i.e., find the dpromise_t from the guid value
	std::unordered_map<int, dpromise_t*>::const_iterator gotit = distributed_promise_guid_map.find(guid);	// O(1) on an average
	HASSERT(gotit != distributed_promise_guid_map.end());
	dpromise_t* remote_dpromise_array = gotit->second;

	// 2. copy the content from global address space to private memory
	const int count_remote = remote_dpromise_array->count;
	char* data_remote = new char[count_remote];
	const int remote_startIndex = MYTHREAD * count_remote;
	std::memcpy(data_remote, (char*)(&((*remote_dpromise_array->datum)[remote_startIndex])), sizeof(char)*count_remote);
	// 3. Store which remote node gave me the put data. This is to ensure
	// that I should cancel out all put_back on this same distributed promise from the source node
	assert(remote_dpromise_array->put_initiator_rank == -1);
	remote_dpromise_array->put_initiator_rank = source;
	// 4. perform a local promise_put
	promise_t* promise_remote = &remote_dpromise_array->the_promise;
	hclib::promise_put(promise_remote, data_remote);
}

/*
 * This function is launched at remote node which
 * launches a put on dest node and then forces
 * a local promise_put as well on the dest node
 */
inline void PROMISE_PUT_remote(dpromise_t* myindex_dpromise_array, int dest) {
	const int count = myindex_dpromise_array->count;
	const int my_startIndex = MYTHREAD * count;
	const int dest_startIndex = dest * count;
	const int guid = myindex_dpromise_array->global_id;
	// 3. Launch an asynchronous copy task to copy the datum to remote node
	upcxx::global_ptr<char> src_start_address =  &((*myindex_dpromise_array->datum)[my_startIndex]);
	upcxx::global_ptr<char> dest_start_address = &((*myindex_dpromise_array->datum)[dest_startIndex]);
	upcxx::event *e = _asyncCopy(src_start_address, dest_start_address, count);
	int me = MYTHREAD;
	// 4. launch an asyncAfter at dest node such that once this asyncCopy completes, it can copy
	// the content of datum from its global address space to its private memory and perform a local promise_put
	_asyncAfter(dest, e, [me, guid]() {
		PROMISE_PUT_home(guid, me);
	});
}

/*
 * Copy the put data from the private memory of a UPC++ place into
 * the corresponding global address space of this UPC++ place.
 */
inline void copy_datum_from_private_to_globalAddress(promise_t* promise, void* data) {
	dpromise_t *myindex_dpromise_array = (dpromise_t *) promise;
	assert(myindex_dpromise_array->put_initiator_rank == -1);
	myindex_dpromise_array->put_initiator_rank = MYTHREAD;
	const int count = myindex_dpromise_array->count;
	const int my_startIndex = MYTHREAD * count;
	std::memcpy((char*)(&((*myindex_dpromise_array->datum)[my_startIndex])), data, sizeof(char)*count);
	__sync_bool_compare_and_swap(&(myindex_dpromise_array->putRequestedFromHome), false, true);	// mark put data is available
}

void PROMISE_PUT(promise_t* promise, void* data) {
	int kind = promise->kind;
	// 1. do a local put in all cases
	// incase the user breaks the single-assignment rule, an error will be thrown here
	hclib::promise_put(promise, data);
	switch (kind) {
	case PROMISE_KIND_SHARED:
		// intra-node promise implementation --> nothing else to do
		break;

	case PROMISE_KIND_DISTRIBUTED_OWNER:
	{
		// 2. copy the data from my private memory to shared memory in my portion of global address space
		copy_datum_from_private_to_globalAddress(promise, data);
		break;
	}

	case PROMISE_KIND_DISTRIBUTED_REMOTE:
	{
		// 2. copy the data from my private memory to shared memory in my portion of global address space
		copy_datum_from_private_to_globalAddress(promise, data);

		// 3. Initiate the put on remote node (home node)
		dpromise_t *myindex_dpromise_array = (dpromise_t *) promise;
		const int dest = myindex_dpromise_array->home_rank;
		PROMISE_PUT_remote(myindex_dpromise_array, dest);
		break;
	}

	default:
		printf("ERROR (PROMISE_PUT): Unknown promise kind \n");
		HASSERT(0);
	}
}

void* PROMISE_GET(promise_t* promise) {
	return hclib::promise_get(promise);
}

/*
 * If this is done on a dpromise_t then it has to be
 * a collective call
 */
void PROMISE_FREE(promise_t* promise) {
	int kind = promise->kind;
	switch (kind) {
	case PROMISE_KIND_SHARED:
	{
		// intra-node promise implementation
		hclib::promise_free(promise);
		break;
	}
	case PROMISE_KIND_DISTRIBUTED_OWNER:
	case PROMISE_KIND_DISTRIBUTED_REMOTE:
	{
		// TODO: Free upcxx::shared_array
#if 0
		hupcpp::barrier();
		// 2. copy the data from my private memory to shared memory in my portion of global address space
		dpromise_t *myindex_dpromise_array = (dpromise_t *) promise;
#endif
		hupcpp::barrier();
		break;
	}
	default:
		printf("ERROR (PROMISE_FREE): Unknown promise kind \n");
		assert(0);
	}
}

int __promiseGetHome(promise_t* promise) {
	int kind = promise->kind;
	int home = -1;
	switch (kind) {
	case PROMISE_KIND_SHARED:
	{
		// intra-node promise implementation
		home = MYTHREAD;
		break;
	}
	case PROMISE_KIND_DISTRIBUTED_OWNER:
	case PROMISE_KIND_DISTRIBUTED_REMOTE:
	{
		// TODO: Free upcxx::shared_array
		dpromise_t *dpromise = (dpromise_t *) promise;
		home = dpromise->home_rank;
		break;
	}
	default:
		printf("ERROR (__promiseGetHome): Unknown promise kind \n");
		assert(0);
	}
	return home;
}

/*
 * Executes at home node to perform put at place "dest" for
 * the data associated with the distributed promise id guid.
 */
inline void PROMISE_PUT_callback(int dest, int guid) {
	// 1. dereference the guid, i.e., find the dpromise_t from the guid value
	std::unordered_map<int, dpromise_t*>::const_iterator gotit = distributed_promise_guid_map.find(guid);	// O(1) on an average
	HASSERT(gotit != distributed_promise_guid_map.end());
	dpromise_t* myindex_dpromise_array = gotit->second;
	promise_t* the_promise = &(myindex_dpromise_array->the_promise);
	hupcpp::asyncAwait(the_promise, [=]() {
		// 2. When control is here, it means the promise is ready with the put data
		if(myindex_dpromise_array->put_initiator_rank != dest) { // Don't have the do a put_back if the requestor node is the source of the actual put
			PROMISE_PUT_remote(myindex_dpromise_array, dest);
		}
	});
}

/*
 * This function is called from hupcpp::asyncAwait function.
 * This is called from inside the hclib runtime and the list of
 * promise is passed to check if any of the promise in the list is "distributed promise" rather than a "promise".
 * In case of distributed promise, special treatment is ensured here in this function
 */
void dpromise_register_callback(promise_t** promise_list) {
	int index = 0;
	while (promise_list[index] != NULL) {
		promise_t* promise = promise_list[index++];
		int kind = promise->kind;
		switch(kind) {
		case PROMISE_KIND_SHARED:
			continue;
		case PROMISE_KIND_DISTRIBUTED_OWNER:
			/*
			 * No special treatment here as this distributed promise is actually owned by this place itself.
			 * The put can be either done locally or by remote place. In either case the promise_put
			 * is always called at this place.
			 */
			continue;
		case PROMISE_KIND_DISTRIBUTED_REMOTE:
		{
			/*
			 * This place is not the home node of this distributed promise. The put can be done either by this
			 * place itself or by another place. (TODO: Optimizations possible if this place is doing the remote put)
			 *
			 * As home node is someone else, this place should inform the home node that its waiting on the
			 * distributed promise value. When the home node receives the promise_put, it will then send the data back to this
			 * place such that this asyncAwait gets scheduled.
			 */

			// 1. Check if the put data is already available
			if(PROMISE_GET(promise) != NULL) continue;

			dpromise_t *myindex_dpromise_array = (dpromise_t *) promise;
			// 2. If the put is already requested from home node, do not register any new callback
			if(!__sync_bool_compare_and_swap(&(myindex_dpromise_array->putRequestedFromHome), false, true)) continue;

			// 3. Schedule the callback function on home node for promise_put at my place
			myindex_dpromise_array->putRequestedFromHome = true;
			const int dpromise_guid = myindex_dpromise_array->global_id;
			const int dest = myindex_dpromise_array->home_rank;
			const int me = MYTHREAD;
			asyncAt(dest, [dpromise_guid, me]() {
				PROMISE_PUT_callback(me, dpromise_guid);
			});
			continue;
		}
		default:
			printf("ERROR (dpromise_register_callback): Unknown promise kind \n");
			assert(0);
		}
	}
}

}
