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

inline void ddf_put(DDF_t * ddf, void * datum) {
	hcpp::ddf_put(ddf, datum);
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
