#include "hcupc_spmd-internal.h"
#include "hcupc_spmd_common_methods.h"
#define MAX_STEAL_QUEUE_SIZE 10

namespace hcpp {
int STEAL_QUEUE[MAX_STEAL_QUEUE_SIZE];
int STEAL_REQUEST = 24;	// upcxx::async_task->_arg_sz, which is always fixed for asyncAt with steal requests

void push_thiefID(int id) {

}

int pop_waiting_thief() {

	return 0;
}

bool is_steal_request(void* task) {
	// check if this asyncAt was a steal request from some thief
	if(((async_task*)task)->_arg_sz == STEAL_REQUEST) {
		// queue this request
		push_thiefID(((async_task*)task)->_caller);
		// increment termination detection counter
		register_incoming_async();
		return true;
	}
	return false;
}

}
