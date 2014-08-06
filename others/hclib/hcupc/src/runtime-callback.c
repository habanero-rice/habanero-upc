/* Copyright (c) 2013, Rice University

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

/**
 * @file This file contains the interface to perform call-backs
 * from the runtime implementation to the HCLIB.
 */

#include <stdio.h>

#include "runtime-callback.h"
#include "runtime-hclib.h"

/**
 * @brief Async is starting execution.
 */
inline static void async_run_start(async_task_t * async_task) {
    // Currently nothing to do here

    // Note: not to be mistaken with the 'async'
    //       function that create the async and
    //       check in the finish scope.
}

/**
 * @brief Async is done executing.
 */
inline static void async_run_end(async_task_t * async_task) {
    // Async has ran, checkout from its finish scope
    async_check_out_finish(async_task);
    #ifdef HAVE_PHASER
    async_drop_phasers(async_task);
    #endif
}

/**
 * @brief Call-back to checkout from a finish scope.
 * Note: Just here because of the ocr-based implementation
 */
void rtcb_check_out_finish(finish_t * finish) {
    check_out_finish(finish);
}

/**
 * @brief Call-back for the underlying runtime to run an async.
 */
void rtcb_async_run(async_task_t * async_task) {
    async_run_start(async_task);
    // Call the targeted function with its arguments
    async_t async_def = async_task->def;
    ((asyncFct_t)(async_def.fct_ptr))(async_def.arg);
    async_run_end(async_task);
    deallocate_async_task(async_task);
}
