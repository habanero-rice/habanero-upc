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

#ifndef RUNTIME_HCLIB_H_
#define RUNTIME_HCLIB_H_

#include "rt-hclib-def.h"

/**
 * @file This file contains the HCLIB runtime implementation
 */

/*
 * Finish check in/out protocol for asyncs
 */
void async_check_in_finish(async_task_t * async_task);
void async_check_out_finish(async_task_t * async_task);

void check_in_finish(finish_t * finish);
void check_out_finish(finish_t * finish);

void async_drop_phasers(async_task_t * async_task);

/*
 * Current async accessors
 */
void set_current_async(async_task_t * async);
async_task_t * get_current_async();

/*
 * Finish utilities functions
 */
finish_t * get_current_finish();
void end_finish_notify(finish_t * current_finish);


/*
 * Allocators / Deallocators
 */

forasync1D_task_t * allocate_forasync1D_task();
forasync2D_task_t * allocate_forasync2D_task();
forasync3D_task_t * allocate_forasync3D_task();
void deallocate_forasync_task(forasync_task_t * forasync);

async_task_t * allocate_async_task(async_t * async_def);
void deallocate_async_task(async_task_t * async);

finish_t * allocate_finish();
void deallocate_finish(finish_t * finish);


/*
 * Schedule an async for execution, transmitting finish, phasers, etc..
 */

void schedule_async(async_task_t * async_task, finish_t * finish_scope, int property);

/*
 * Hand over an async to the underlying runtime.
 * The task must have been properly configured beforehand.
 */
void try_schedule_async(async_task_t * async_task);


/*
 * Workers info
 */
int get_nb_workers();
int get_worker_id_hc();


/**
 * @brief notifies the runtime end_finish is logically
 * blocked, waiting for children to finish.
 */
void help_finish(finish_t * finish);

#endif /* RUNTIME_HCLIB_H_ */
