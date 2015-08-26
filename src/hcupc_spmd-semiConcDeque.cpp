#include "hcupc_spmd-internal.h"
#include "hcupc_spmd_common_methods.h"
#include "hcupc_spmd-commTask.h"
#include "hcupc_spmd-atomics.h"

namespace hupcpp {

/******************************************************/
/* Semi Concurrent DEQUE                              */
/******************************************************/

#define SEMI_CONCURRENT_DEQUE_SIZE		50
volatile comm_async_task DEQUE[SEMI_CONCURRENT_DEQUE_SIZE][sizeof(comm_async_task)];
volatile int deqHead;
volatile int deqTail;
volatile int lock;

void semiConcDequeInit() {
	deqHead = 0;
	deqTail = 0;
	lock = 0;
}

void comm_task_push(comm_async_task* entry) {
	bool success = false;
	while (!success) {
		int size = deqTail - deqHead;
		if (SEMI_CONCURRENT_DEQUE_SIZE == size) {
			HASSERT("SEMI_CONCURRENT_DEQUE full, increase deque's size" && 0);
		}

		if (hc_cas(&lock, 0, 1) ) {
			success = true;
			const int index = deqTail % SEMI_CONCURRENT_DEQUE_SIZE;
			memcpy((void*)DEQUE[index], entry, sizeof(comm_async_task));
			hc_mfence();
			++deqTail;
			lock= 0;
		}
	}
}

bool comm_task_pop(comm_async_task* buff) {
	if ((deqTail - deqHead) > 0) {
		const int index = deqHead % SEMI_CONCURRENT_DEQUE_SIZE;
		memcpy(buff, (void*)DEQUE[index], sizeof(comm_async_task));
		++deqHead;
		return true;
	}
	return false;
}

}
