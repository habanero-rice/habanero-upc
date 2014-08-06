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

#ifndef RUNTIME_IMPL_H_
#define RUNTIME_IMPL_H_

#include "hclib.h"
#include "rt-hclib-def.h"

/**
 * @file This file contains the interface for HCLIB to
 * invoke the underlying runtime implementation.
 */

//
// Runtime life-cycle management
//

/**
 * @brief start the underlying runtime
 */
void runtime_init(int * argc, char ** argv);

/**
 * @brief shutdown the underlying runtime
 */
void runtime_finalize();

/*
 * @brief Retrieve the async task being currently executed
 */
async_task_t * get_current_async();



//
// Task conversion
//

/**
 * @brief Converts an async_task_t into a ddt_t
 * Warning: 'async_task' must be a data-driven task in the first place.
 */
struct ddt_st;

/**
 * @brief Convert a ddt into an async_task
 */
async_task_t * rt_ddt_to_async_task(struct ddt_st * ddt);

/**
 * @brief Convert an async task into a ddt
 */
struct ddt_st * rt_async_task_to_ddt(async_task_t * async_task);


//
// Task allocation
//

/**
 * @brief allocates an async task.
 * Rule 1: The runtime must return a valid pointer to an async_task_t.
 * Rule 2: The runtime may allocate a larger data-structure for its
 * own book-keeping as long as the Rule 1 is enforced.
 */
async_task_t * rt_allocate_async_task();
void rt_deallocate_async_task(async_task_t * async_task);

forasync1D_task_t * rt_allocate_forasync1D_task();
forasync2D_task_t * rt_allocate_forasync2D_task();
forasync3D_task_t * rt_allocate_forasync3D_task();

/**
 * @brief allocates a data-driven task and register a ddf_list
 */
async_task_t * rt_allocate_ddt(struct ddf_st ** ddf_list);


//
// Finish allocation
//

/**
 * @brief allocates a finish data-structure.
 * Rule 1: The runtime must return a valid pointer to a finish_t.
 * Rule 2: The runtime may allocate a larger data-structure for its
 * own book-keeping as long as the Rule 1 is enforced.
 */
finish_t * rt_allocate_finish();

/**
 * @brief deallocates a finish data-structure.
 */
void rt_deallocate_finish(finish_t *);


//
// Scheduling
//

/**
 * @brief delegate an async to the runtime for scheduling
 */
void rt_schedule_async(async_task_t * async);

/**
 * @brief To notify a finish scope counter reached zero
 */
void rt_finish_reached_zero(finish_t * finish);

/**
 * @brief Try to make progress when waiting for a finish scope to complete
 * Typically execute other asyncs on top of its own stack. Better know
 * what you're doing when calling this.
 */
void rt_help_finish(finish_t * finish);


//
// Worker info
//

/**
 * @brief The number of worker the underlying runtime dispose of
 */
int rt_get_nb_workers();

/**
 * @brief The identifier of the currently executing worker
 */
int rt_get_worker_id();

#endif /* RUNTIME_IMPL_H_ */
