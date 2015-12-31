namespace hupcpp {

#define PROMISE_HANDLE(i)           	__promiseHandle(i, PROMISE_HOME(i), PROMISE_SIZE(i))
#define PROMISE_GET_HOME(promise) 		__promiseGetHome(promise)

/*
 * hclib-promise.h also contains the exact enum values
 */
typedef enum distributed_promise_kind {
	PROMISE_KIND_UNKNOWN=0,
	PROMISE_KIND_SHARED,
	PROMISE_KIND_DISTRIBUTED_OWNER,
	PROMISE_KIND_DISTRIBUTED_REMOTE,
} distributed_promise_kind_t;

typedef struct _dpromise_t {
    promise_t the_promise;
    int global_id;
    int home_rank; /* home is where the put is */
    int put_initiator_rank;
	int count;
	bool putRequestedFromHome;
	upcxx::shared_array<char>* datum;
} dpromise_t;

promise_t* __promiseHandle(int gid, int hid, size_t count);
int __promiseGetHome(promise_t* promise);
void PROMISE_PUT(promise_t* promise, void* data);
void* PROMISE_GET(promise_t* promise);
void PROMISE_FREE(promise_t* promise);

}
