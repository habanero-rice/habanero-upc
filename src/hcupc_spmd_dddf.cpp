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
#include "hcupc_spmd-dddf.h"
#include <unordered_map>

namespace hupcpp {

std::unordered_map<int, DDDF_t*> dddf_guid_map;

// used only for DDDF related operations and hence not available to users
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
	// event allocation taken out of lambda to implement the DDDF
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
DDF_t* __ddfHandle( int gid, int hid, size_t size) {
	/*
	 * We are calling upcxx::allocate hence we do it only
	 * on the comm_worker
	 */
	HASSERT(size > 0);
	HASSERT(hid>=0 && hid<THREADS);

	upcxx::shared_array<DDDF_t> *dddf_array = new upcxx::shared_array<DDDF_t>();
	(*dddf_array).init(THREADS, 1);
	DDDF_t *myindex_dddf_array = (DDDF_t *) &((*dddf_array)[MYTHREAD]);

	// store the DDDF_t in hash-table with guid as hash-key
	dddf_guid_map.insert({gid, myindex_dddf_array});

	// initialize the local DDF_t equivalent pointer for this DDDF_t
	DDF_t* ddf = &(myindex_dddf_array->theDDF);
	hclib::ddf_init(ddf);	// does not allocate but only initializes
	ddf->kind = (hid == MYTHREAD) ? DDF_KIND_DISTRIBUTED_OWNER : DDF_KIND_DISTRIBUTED_REMOTE;

	// initialize structure variables
	myindex_dddf_array->global_id = gid;
	myindex_dddf_array->home_rank = hid;
	myindex_dddf_array->count = size/sizeof(char);
	myindex_dddf_array->putRequestedFromHome = false;
	myindex_dddf_array->put_initiator_rank = -1;

	// allocate the datum in global address space
	myindex_dddf_array->datum = new upcxx::shared_array<char>();
	(*myindex_dddf_array->datum).init(THREADS*(myindex_dddf_array->count), myindex_dddf_array->count);	// block cyclic allocation of array
	const int kind = ddf->kind;
	HASSERT(kind == DDF_KIND_DISTRIBUTED_OWNER || kind == DDF_KIND_DISTRIBUTED_REMOTE);
	hupcpp::barrier();

	return ddf;
}

/*
 * This function is launched at home node to do
 * a local ddf_put
 */
inline void DDF_PUT_home(int guid, int source) {
	// 1. dereference the guid, i.e., find the DDDF_t from the guid value
	std::unordered_map<int, DDDF_t*>::const_iterator gotit = dddf_guid_map.find(guid);	// O(1) on an average
	HASSERT(gotit != dddf_guid_map.end());
	DDDF_t* remote_dddf_array = gotit->second;

	// 2. copy the content from global address space to private memory
	const int count_remote = remote_dddf_array->count;
	char* data_remote = new char[count_remote];
	const int remote_startIndex = MYTHREAD * count_remote;
	std::memcpy(data_remote, (char*)(&((*remote_dddf_array->datum)[remote_startIndex])), sizeof(char)*count_remote);
	// 3. Store which remote node gave me the put data. This is to ensure
	// that I should cancel out all put_back on this same DDDF from the source node
	assert(remote_dddf_array->put_initiator_rank == -1);
	remote_dddf_array->put_initiator_rank = source;
	// 4. perform a local ddf_put
	DDF_t* ddf_remote = &remote_dddf_array->theDDF;
	hclib::ddf_put(ddf_remote, data_remote);
}

/*
 * This function is launched at remote node which
 * launches a put on dest node and then forces
 * a local ddf_put as well on the dest node
 */
inline void DDF_PUT_remote(DDDF_t* myindex_dddf_array, int dest) {
	const int count = myindex_dddf_array->count;
	const int my_startIndex = MYTHREAD * count;
	const int dest_startIndex = dest * count;
	const int guid = myindex_dddf_array->global_id;
	// 3. Launch an asynchronous copy task to copy the datum to remote node
	upcxx::global_ptr<char> src_start_address =  &((*myindex_dddf_array->datum)[my_startIndex]);
	upcxx::global_ptr<char> dest_start_address = &((*myindex_dddf_array->datum)[dest_startIndex]);
	upcxx::event *e = _asyncCopy(src_start_address, dest_start_address, count);
	int me = MYTHREAD;
	// 4. launch an asyncAfter at dest node such that once this asyncCopy completes, it can copy
	// the content of datum from its global address space to its private memory and perform a local ddf_put
	_asyncAfter(dest, e, [me, guid]() {
		DDF_PUT_home(guid, me);
	});
}

/*
 * Copy the put data from the private memory of a UPC++ place into
 * the corresponding global address space of this UPC++ place.
 */
inline void copy_datum_from_private_to_globalAddress(DDF_t* ddf, void* data) {
	DDDF_t *myindex_dddf_array = (DDDF_t *) ddf;
	assert(myindex_dddf_array->put_initiator_rank == -1);
	myindex_dddf_array->put_initiator_rank = MYTHREAD;
	const int count = myindex_dddf_array->count;
	const int my_startIndex = MYTHREAD * count;
	std::memcpy((char*)(&((*myindex_dddf_array->datum)[my_startIndex])), data, sizeof(char)*count);
	__sync_bool_compare_and_swap(&(myindex_dddf_array->putRequestedFromHome), false, true);	// mark put data is available
}

void DDF_PUT(DDF_t* ddf, void* data) {
	int kind = ddf->kind;
	// 1. do a local put in all cases
	// incase the user breaks the single-assignment rule, an error will be thrown here
	hclib::ddf_put(ddf, data);
	switch (kind) {
	case DDF_KIND_SHARED:
		// intra-node DDF implementation --> nothing else to do
		break;

	case DDF_KIND_DISTRIBUTED_OWNER:
	{
		// 2. copy the data from my private memory to shared memory in my portion of global address space
		copy_datum_from_private_to_globalAddress(ddf, data);
		break;
	}

	case DDF_KIND_DISTRIBUTED_REMOTE:
	{
		// 2. copy the data from my private memory to shared memory in my portion of global address space
		copy_datum_from_private_to_globalAddress(ddf, data);

		// 3. Initiate the put on remote node (home node)
		DDDF_t *myindex_dddf_array = (DDDF_t *) ddf;
		const int dest = myindex_dddf_array->home_rank;
		DDF_PUT_remote(myindex_dddf_array, dest);
		break;
	}

	default:
		printf("ERROR (DDF_PUT): Unknown DDF kind \n");
		HASSERT(0);
	}
}

void* DDF_GET(DDF_t* ddf) {
	return hclib::ddf_get(ddf);
}

/*
 * If this is done on a DDDF_t then it has to be
 * a collective call
 */
void DDF_FREE(DDF_t* ddf) {
	int kind = ddf->kind;
	switch (kind) {
	case DDF_KIND_SHARED:
	{
		// intra-node DDF implementation
		hclib::ddf_free(ddf);
		break;
	}
	case DDF_KIND_DISTRIBUTED_OWNER:
	case DDF_KIND_DISTRIBUTED_REMOTE:
	{
		// TODO: Free upcxx::shared_array
#if 0
		hupcpp::barrier();
		// 2. copy the data from my private memory to shared memory in my portion of global address space
		DDDF_t *myindex_dddf_array = (DDDF_t *) ddf;
#endif
		hupcpp::barrier();
		break;
	}
	default:
		printf("ERROR (DDF_FREE): Unknown DDF kind \n");
		assert(0);
	}
}

int __ddfGetHome(DDF_t* ddf) {
	int kind = ddf->kind;
	int home = -1;
	switch (kind) {
	case DDF_KIND_SHARED:
	{
		// intra-node DDF implementation
		home = MYTHREAD;
		break;
	}
	case DDF_KIND_DISTRIBUTED_OWNER:
	case DDF_KIND_DISTRIBUTED_REMOTE:
	{
		// TODO: Free upcxx::shared_array
		DDDF_t *dddf = (DDDF_t *) ddf;
		home = dddf->home_rank;
		break;
	}
	default:
		printf("ERROR (__ddfGetHome): Unknown DDF kind \n");
		assert(0);
	}
	return home;
}

/*
 * Executes at home node to perform put at place "dest" for
 * the data associated with the DDDF id guid.
 */
inline void DDF_PUT_callback(int dest, int guid) {
	// 1. dereference the guid, i.e., find the DDDF_t from the guid value
	std::unordered_map<int, DDDF_t*>::const_iterator gotit = dddf_guid_map.find(guid);	// O(1) on an average
	HASSERT(gotit != dddf_guid_map.end());
	DDDF_t* myindex_dddf_array = gotit->second;
	DDF_t* theDDF = &(myindex_dddf_array->theDDF);
	hupcpp::asyncAwait(theDDF, [=]() {
		// 2. When control is here, it means the DDF is ready with the put data
		if(myindex_dddf_array->put_initiator_rank != dest) { // Don't have the do a put_back if the requestor node is the source of the actual put
			DDF_PUT_remote(myindex_dddf_array, dest);
		}
	});
}

/*
 * This function is called from hupcpp::asyncAwait function.
 * This is called from inside the hcpp runtime and the list of
 * ddf is passed to check if any of the ddf in the list is "DDDF" rather than a "DDF".
 * In case of DDDF, special treatment is ensured here in this function
 */
void dddf_register_callback(DDF_t** ddf_list) {
	int index = 0;
	while (ddf_list[index] != NULL) {
		DDF_t* ddf = ddf_list[index++];
		int kind = ddf->kind;
		switch(kind) {
		case DDF_KIND_SHARED:
			continue;
		case DDF_KIND_DISTRIBUTED_OWNER:
			/*
			 * No special treatment here as this DDDF is actually owned by this place itself.
			 * The put can be either done locally or by remote place. In either case the ddf_put
			 * is always called at this place.
			 */
			continue;
		case DDF_KIND_DISTRIBUTED_REMOTE:
		{
			/*
			 * This place is not the home node of this DDDF. The put can be done either by this
			 * place itself or by another place. (TODO: Optimizations possible if this place is doing the remote put)
			 *
			 * As home node is someone else, this place should inform the home node that its waiting on the
			 * DDDF value. When the home node receives the ddf_put, it will then send the data back to this
			 * place such that this asyncAwait gets scheduled.
			 */

			// 1. Check if the put data is already available
			if(DDF_GET(ddf) != NULL) continue;

			DDDF_t *myindex_dddf_array = (DDDF_t *) ddf;
			// 2. If the put is already requested from home node, do not register any new callback
			if(!__sync_bool_compare_and_swap(&(myindex_dddf_array->putRequestedFromHome), false, true)) continue;

			// 3. Schedule the callback function on home node for ddf_put at my place
			myindex_dddf_array->putRequestedFromHome = true;
			const int dddf_guid = myindex_dddf_array->global_id;
			const int dest = myindex_dddf_array->home_rank;
			const int me = MYTHREAD;
			asyncAt(dest, [dddf_guid, me]() {
				DDF_PUT_callback(me, dddf_guid);
			});
			continue;
		}
		default:
			printf("ERROR (dddf_register_callback): Unknown DDF kind \n");
			assert(0);
		}
	}
}

}
