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
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "hclib.h"
#include "rt-hclib-def.h"
#include "runtime-support.h"
#include "runtime-hclib.h"
#ifdef HAVE_PHASER
#include "phased.h"
#endif

int get_worker_count() {
  return get_nb_workers();
}

/**
 * @file This file should only contain implementations of user-level defined HCLIB functions.
 */

static int runtime_on = 0;
static async_task_t * root_async_task = NULL;

// To support separate finish for COMM_HC async
static async_task_t * root_comm_async_task = NULL;

void hclib_init(int * argc, char ** argv) {
    if (runtime_on) {
        assert(0 && "hc_init called twice in a row");
    }

    // Start the underlying runtime
    runtime_init(argc, argv);
    runtime_on = 1;

    // Create the root activity: abstraction to make code more uniform
    root_async_task = allocate_async_task(NULL);
    set_current_async(root_async_task);

    // COMM_HC
    root_comm_async_task = allocate_async_task(NULL);

    // Create and check in the implicit finish scope.
    start_finish();

    // Go back to user code
}

void hclib_finalize() {
    // Checkout of the implicit finish scope
    if (runtime_on) {
        // Current worker is executing the async root task
        // Check out of the implicit finish scope
        end_finish();
        // Deallocate the root activity
        free(root_async_task);
        // Shutdown the underlying runtime
        runtime_finalize();
        runtime_on = 0;
    } else {
        assert(0 && "hc_finalize called without any matching hc_init");
    }
}

/**
 * @brief start_finish implementation is decoupled in two functions.
 * The main concern is that the implicit finish is executed by the
 * library thread. So we cannot assume it is part of the underlying runtime.
 * Hence, we cannot use get/set current_async. Decoupling allows to feed
 * the function with the root dummy async_task we maintain in a static.
 */
void start_finish() {
    finish_t * finish = allocate_finish();
    assert(finish && "malloc failed");
    async_task_t * async_task = get_current_async();
    finish->counter = 1;
    finish->accumulators = NULL;
#if CHECKED_EXECUTION
    // The owner of this finish scope is the worker executing this code.
    finish->owner = get_worker_id_hc();
#endif
    // The parent of this finish scope is the finish scope the
    // async is currently executing in.
    finish->parent = async_task->current_finish;
    // The async task has now entered a new finish scope
    async_task->current_finish = finish;
}

static finish_t* latest_comp_finish = NULL;

int totalPendingLocalAsyncs() {
	assert(latest_comp_finish != NULL);
	return (latest_comp_finish->counter - 1);
}

void start_finish_comm() {
	//
	async_task_t * async_task_tmp = get_current_async();
	latest_comp_finish = async_task_tmp->current_finish;
	//
    finish_t * finish = allocate_finish();
    assert(finish && "malloc failed");
    /*
     * WARNING: We currently assume no nesting of finishes.
     * This is not a restriction!! If nesting is to be allowed
     * then following code needs changes.
     */
    async_task_t * async_task = root_comm_async_task;
    finish->counter = 1;
    finish->accumulators = NULL;
#if CHECKED_EXECUTION
    // The owner of this finish scope is the worker executing this code.
    finish->owner = get_worker_id_hc();
#endif
    // The parent of this finish scope is the finish scope the
    // async is currently executing in.
    finish->parent = async_task->current_finish;
    // The async task has now entered a new finish scope
    async_task->current_finish = finish;
}


void end_finish_comm_looper() {
    //Note: Important to store the current async in a local as the
    //      help-protocol may execute a new async and change the
    //      worker's currently executed async.
    async_task_t * async = root_comm_async_task;
    finish_t * current_finish = async->current_finish;
    // If we still have children executing
    if (current_finish->counter > 1) {
        // Notify the runtime we are waiting for them
        // Note: there's a race on counter. help_finish may
        //       start and realize the finish scope is done.
         help_finish(current_finish);
    }
}

void end_finish_comm_free() {
    //Note: Important to store the current async in a local as the
    //      help-protocol may execute a new async and change the
    //      worker's currently executed async.
    async_task_t * async = root_comm_async_task;
    finish_t * current_finish = async->current_finish;
    latest_comp_finish = NULL;
    //TODO careful with that, async has not been restored
    // Notify this finish listeners, accumulators may be registered on this
    end_finish_notify(current_finish);

    // Pop current finish to its parent
    async->current_finish = current_finish->parent;
    // Don't need this finish scope anymore
    deallocate_finish(current_finish);
}

void end_finish() {
    //Note: Important to store the current async in a local as the
    //      help-protocol may execute a new async and change the
    //      worker's currently executed async.
    async_task_t * async = get_current_async();
    finish_t * current_finish = async->current_finish;
    // If we still have children executing
    if (current_finish->counter > 1) {
        // Notify the runtime we are waiting for them
        // Note: there's a race on counter. help_finish may
        //       start and realize the finish scope is done.
         help_finish(current_finish);
    }
    //TODO careful with that, async has not been restored
    // Notify this finish listeners, accumulators may be registered on this
    end_finish_notify(current_finish);

    // Pop current finish to its parent
    async->current_finish = current_finish->parent;
    // Restore worker's currently executing async
    //TODO not need that I think
    set_current_async(async);
    // Don't need this finish scope anymore
    deallocate_finish(current_finish);
}

void async(asyncFct_t fct_ptr, void * arg,
           struct ddf_st ** ddf_list, struct _phased_t * phased_clause, int property) {
#if CHECKED_EXECUTION
    assert(runtime_on);
#endif
    // Populate the async definition
    async_t async_def;
    async_def.fct_ptr = fct_ptr;
    async_def.arg = arg;
    async_def.ddf_list = ddf_list;
    #ifdef HAVE_PHASER
        async_def.phased_clause = phased_clause;
    #endif

    // Build the new async task and associate with the definition
    async_task_t * async_task = allocate_async_task(&async_def);
    schedule_async(async_task, get_current_finish(), property);
}

void async_comm(asyncFct_t fct_ptr, void * arg,
           struct ddf_st ** ddf_list, struct _phased_t * phased_clause, int property) {
#if CHECKED_EXECUTION
    assert(runtime_on);
#endif
    // Populate the async definition
    async_t async_def;
    async_def.fct_ptr = fct_ptr;
    async_def.arg = arg;
    async_def.ddf_list = ddf_list;
    #ifdef HAVE_PHASER
        async_def.phased_clause = phased_clause;
    #endif

    // Build the new async task and associate with the definition
    async_task_t * async_task = allocate_async_task(&async_def);
    schedule_async(async_task, root_comm_async_task->current_finish, property);
}

void forasync(void* forasync_fct, void * argv, struct ddf_st ** ddf_list, struct _phased_t * phased_clause,
            struct _accumed_t * accumed, int dim, loop_domain_t * domain, forasync_mode_t mode) {
	forasync_lib_interface(forasync_fct,argv,ddf_list,phased_clause,accumed,dim,domain,mode);
}
