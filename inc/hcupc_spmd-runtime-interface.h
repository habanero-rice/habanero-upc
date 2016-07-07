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

#include "hcpp.h"

namespace hupcpp {

typedef	hcpp::ddf_t  DDF_t;
typedef	hcpp::ddt_t  DDT_t;

inline DDF_t* ddf_create() {
	return hcpp::ddf_create();
}

inline DDF_t ** ddf_create_n(size_t nb_ddfs, int null_terminated) {
	return hcpp::ddf_create_n(nb_ddfs, null_terminated);
}

inline void ddf_free(DDF_t * ddf) {
	hcpp::ddf_free(ddf);
}

inline void * ddf_get(DDF_t * ddf) {
	return hcpp::ddf_get(ddf);
}

#include "hcupc_spmd-asyncAwait.h"

inline int numWorkers() {
	return hcpp::numWorkers();
}

inline int totalPendingLocalAsyncs() {
	return hcpp::totalPendingLocalAsyncs() - 1;
}

inline int get_hc_wid() {
	return hcpp::get_hc_wid();
}

inline void register_incoming_async_ddf(void* d) {
	if(d) {
		DDF_t * ddf = (DDF_t *) d;
		int* tmp = new int;
		*tmp = 10;
		hcpp::ddf_put(ddf, (void*)tmp);
	}
}

inline void start_finish() {
	hcpp::start_finish();
}

inline void end_finish() {
	hcpp::end_finish();
}

template <typename T>
inline void asyncComm(T lambda) {
	hcpp::asyncComm<T>(lambda);
}

template <typename T>
inline void async(T lambda) {
	hcpp::async<T>(lambda);
}

#ifdef DIST_WS
template <typename T>
inline void hcupc_asyncAny(T lambda) {
	hcpp::asyncAny<T>(lambda);
}
#endif

inline void finish(std::function<void()> lambda) {
	hcpp::finish(lambda);
}

inline void* hcupc_malloc(size_t nbytes) {
	return HC_MALLOC(nbytes);
}

inline void hcupc_free(void* ptr) {
	HC_FREE(ptr);
}

typedef struct _loop_domain_t {
	int low;
	int high;
	int stride;
	int tile;
} loop_domain_t;

template <typename T>
inline void forasync1D(loop_domain_t* loop, T lambda, int mode=FORASYNC_MODE_RECURSIVE) {
	hcpp::forasync1D_internal<T>((hcpp::loop_domain_t*)loop, lambda, mode);
}

template <typename T>
inline void forasync2D(loop_domain_t* loop, T lambda, int mode=FORASYNC_MODE_RECURSIVE) {
	hcpp::forasync2D_internal<T>((hcpp::loop_domain_t*)loop, lambda, mode);
}

template <typename T>
inline void forasync3D(loop_domain_t* loop, T lambda, int mode=FORASYNC_MODE_RECURSIVE) {
	hcpp::forasync3D_internal<T>((hcpp::loop_domain_t*)loop, lambda, mode);
}
}
