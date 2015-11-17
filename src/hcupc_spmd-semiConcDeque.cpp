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

namespace hupcpp {

/******************************************************/
/* Semi Concurrent DEQUE                              */
/******************************************************/

#define SEMI_CONCURRENT_DEQUE_SIZE		8192
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
			assert("SEMI_CONCURRENT_DEQUE full, increase deque's size" && 0);
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
