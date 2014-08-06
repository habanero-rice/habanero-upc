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
#include <assert.h>

#include "hclib.h"
#include "rt-hclib-def.h"
#include "runtime-support.h"
#include "runtime-hclib.h"
#ifdef HAVE_PHASER
#include "phased.h"
#endif

#define DEBUG_FORASYNC 0

void forasync1D_runner(void * forasync_arg) {

    forasync1D_t * forasync = (forasync1D_t *) forasync_arg;
    async_t * user = *((async_t **) forasync_arg);
    forasync1D_Fct_t user_fct_ptr = (forasync1D_Fct_t) user->fct_ptr;
    void * user_arg = (void *) user->arg;
    loop_domain_t loop0 = forasync->loop0;
    int i=0;
    for(i=loop0.low; i<loop0.high; i+=loop0.stride) {
        (*user_fct_ptr)(user_arg, i);
    }
}

void forasync2D_runner(void * forasync_arg) {
    forasync2D_t * forasync = (forasync2D_t *) forasync_arg;
    async_t * user = *((async_t **) forasync_arg);
    forasync2D_Fct_t user_fct_ptr = (forasync2D_Fct_t) user->fct_ptr;
    void * user_arg = (void *) user->arg;
    loop_domain_t loop0 = forasync->loop0;
    loop_domain_t loop1 = forasync->loop1;
    int i=0,j=0;
    for(i=loop0.low; i<loop0.high; i+=loop0.stride) {
        for(j=loop1.low; j<loop1.high; j+=loop1.stride) {
            (*user_fct_ptr)(user_arg, i, j);
        }
    }
}

void forasync3D_runner(void * forasync_arg) {
    forasync3D_t * forasync = (forasync3D_t *) forasync_arg;
    async_t * user = *((async_t **) forasync_arg);
    forasync3D_Fct_t user_fct_ptr = (forasync3D_Fct_t) user->fct_ptr;
    void * user_arg = (void *) user->arg;
    loop_domain_t loop0 = forasync->loop0;
    loop_domain_t loop1 = forasync->loop1;
    loop_domain_t loop2 = forasync->loop2;
    int i=0,j=0,k=0;
    for(i=loop0.low; i<loop0.high; i+=loop0.stride) {
        for(j=loop1.low; j<loop1.high; j+=loop1.stride) {
            for(k=loop2.low; k<loop2.high; k+=loop2.stride) {
                (*user_fct_ptr)(user_arg, i, j, k);
            }
        }
    }
#if DEBUG_FORASYNC
    printf("forasync spawned %d\n", nb_spawn);
#endif
}

void forasync1D_recursive(void * forasync_arg) {
    forasync1D_t * forasync = (forasync1D_t *) forasync_arg;
    loop_domain_t loop0 = forasync->loop0;
    int high0 = loop0.high;
    int low0 = loop0.low;
    int stride0 = loop0.stride;
    int tile0 = loop0.tile;

    //split the range into two, spawn a new task for the first half and recurse on the rest  
    forasync1D_task_t * new_forasync_task = NULL;
     if((high0-low0) > tile0) {
        int mid = (high0+low0)/2;
        // upper-half
        new_forasync_task = allocate_forasync1D_task();
        new_forasync_task->task.forasync_task.def.fct_ptr = forasync1D_recursive;
        new_forasync_task->task.forasync_task.def.arg = &(new_forasync_task->def);
        new_forasync_task->def.base.user = forasync->base.user;
        loop_domain_t new_loop0 = {mid, high0, stride0, tile0};
        new_forasync_task->def.loop0 = new_loop0;
        // update lower-half
        forasync->loop0.high = mid;
        // delegate scheduling to the underlying runtime
        //TODO can we make this a special async to avoid a get_current_finish ?
        schedule_async((async_task_t*)new_forasync_task, get_current_finish(), NO_PROP);
        //continue to work on the half task 
        forasync1D_recursive(forasync_arg); 
     } else { 
        //compute the tile
        forasync1D_runner(forasync_arg);
     }
}

void forasync2D_recursive(void * forasync_arg) {
    forasync2D_t * forasync = (forasync2D_t *) forasync_arg;
    loop_domain_t loop0 = forasync->loop0;
    int high0 = loop0.high;
    int low0 = loop0.low;
    int stride0 = loop0.stride;
    int tile0 = loop0.tile;
    loop_domain_t loop1 = forasync->loop1;
    int high1 = loop1.high;
    int low1 = loop1.low;
    int stride1 = loop1.stride;
    int tile1 = loop1.tile;

    //split the range into two, spawn a new task for the first half and recurse on the rest  
    forasync2D_task_t * new_forasync_task = NULL;
     if((high0-low0) > tile0) {
        int mid = (high0+low0)/2;
        // upper-half
        new_forasync_task = allocate_forasync2D_task();
        new_forasync_task->task.forasync_task.def.fct_ptr = forasync2D_recursive;
        new_forasync_task->task.forasync_task.def.arg = &(new_forasync_task->def);
        new_forasync_task->def.base.user = forasync->base.user;
        loop_domain_t new_loop0 = {mid, high0, stride0, tile0};;
        new_forasync_task->def.loop0 = new_loop0;
        new_forasync_task->def.loop1 = loop1;
        // update lower-half
        forasync->loop0.high = mid;
     } else if((high1-low1) > tile1) {
        int mid = (high1+low1)/2;
        // upper-half
        new_forasync_task = allocate_forasync2D_task();
        new_forasync_task->task.forasync_task.def.fct_ptr = forasync2D_recursive;
        new_forasync_task->task.forasync_task.def.arg = &(new_forasync_task->def);
        new_forasync_task->def.base.user = forasync->base.user;
        new_forasync_task->def.loop0 = loop0;
        loop_domain_t new_loop1 = {mid, high1, stride1, tile1};
        new_forasync_task->def.loop1 = new_loop1;
        // update lower-half
        forasync->loop1.high = mid;
     }
     // recurse
     if(new_forasync_task != NULL) {
        // delegate scheduling to the underlying runtime
        //TODO can we make this a special async to avoid a get_current_async ?
        schedule_async((async_task_t*)new_forasync_task, get_current_finish(), NO_PROP);
        //continue to work on the half task 
        forasync2D_recursive(forasync_arg); 
     } else { //compute the tile
        forasync2D_runner(forasync_arg);
     }
}

void forasync3D_recursive(void * forasync_arg) {
    forasync3D_t * forasync = (forasync3D_t *) forasync_arg;
    loop_domain_t loop0 = forasync->loop0;
    int high0 = loop0.high;
    int low0 = loop0.low;
    int stride0 = loop0.stride;
    int tile0 = loop0.tile;
    loop_domain_t loop1 = forasync->loop1;
    int high1 = loop1.high;
    int low1 = loop1.low;
    int stride1 = loop1.stride;
    int tile1 = loop1.tile;
    loop_domain_t loop2 = forasync->loop2;
    int high2 = loop2.high;
    int low2 = loop2.low;
    int stride2 = loop2.stride;
    int tile2 = loop2.tile;

    //split the range into two, spawn a new task for the first half and recurse on the rest  
    forasync3D_task_t * new_forasync_task = NULL;
     if((high0-low0) > tile0) {
        int mid = (high0+low0)/2;
        // upper-half
        new_forasync_task = allocate_forasync3D_task();
        new_forasync_task->task.forasync_task.def.fct_ptr = forasync3D_recursive;
        new_forasync_task->task.forasync_task.def.arg = &(new_forasync_task->def);
        new_forasync_task->def.base.user = forasync->base.user;
        loop_domain_t new_loop0 = {mid, high0, stride0, tile0};
        new_forasync_task->def.loop0 = new_loop0;
        new_forasync_task->def.loop1 = loop1;
        new_forasync_task->def.loop2 = loop2;
        // update lower-half
        forasync->loop0.high = mid;
     } else if((high1-low1) > tile1) {
        int mid = (high1+low1)/2;
        // upper-half
        new_forasync_task = allocate_forasync3D_task();
        new_forasync_task->task.forasync_task.def.fct_ptr = forasync3D_recursive;
        new_forasync_task->task.forasync_task.def.arg = &(new_forasync_task->def);
        new_forasync_task->def.base.user = forasync->base.user;
        new_forasync_task->def.loop0 = loop0;
        loop_domain_t new_loop1 = {mid, high1, stride1, tile1};
        new_forasync_task->def.loop1 = new_loop1;
        new_forasync_task->def.loop2 = loop2;
        // update lower-half
        forasync->loop1.high = mid;
     } else if((high2-low2) > tile2) {
        int mid = (high2+low2)/2;
        // upper-half
        new_forasync_task = allocate_forasync3D_task();
        new_forasync_task->task.forasync_task.def.fct_ptr = forasync3D_recursive;
        new_forasync_task->task.forasync_task.def.arg = &(new_forasync_task->def);
        new_forasync_task->def.base.user = forasync->base.user;
        new_forasync_task->def.loop0 = loop0;
        new_forasync_task->def.loop1 = loop1;
        loop_domain_t new_loop2 = {mid, high2, stride2, tile2};
        new_forasync_task->def.loop2 = new_loop2;
        // update lower-half
        forasync->loop2.high = mid;
     }
     // recurse
     if(new_forasync_task != NULL) {
        // delegate scheduling to the underlying runtime
        //TODO can we make this a special async to avoid a get_current_async ?
        schedule_async((async_task_t*)new_forasync_task, get_current_finish(), NO_PROP);
        //continue to work on the half task 
        forasync3D_recursive(forasync_arg); 
     } else { //compute the tile
        forasync3D_runner(forasync_arg);
     }
}

void forasync1D_flat(void * forasync_arg) {
    forasync1D_t * forasync = (forasync1D_t *) forasync_arg;
    loop_domain_t loop0 = forasync->loop0;
    int high0 = loop0.high;
    int stride0 = loop0.stride;
    int tile0 = loop0.tile;
    int nb_chunks = (int) (high0/tile0);
    int size = tile0*nb_chunks;
    finish_t * current_finish = get_current_finish();
    int low0;
    for(low0 = loop0.low; low0<size; low0+=tile0) {
        #if DEBUG_FORASYNC
            printf("Scheduling Task %d %d\n",low0,(low0+tile0));
        #endif
        //TODO block allocation ?
        forasync1D_task_t * new_forasync_task = allocate_forasync1D_task();
        new_forasync_task->task.forasync_task.def.fct_ptr = forasync1D_runner;
        new_forasync_task->task.forasync_task.def.arg = &(new_forasync_task->def);
        new_forasync_task->def.base.user = forasync->base.user;
        loop_domain_t new_loop0 = {low0, low0+tile0, stride0, tile0};
        new_forasync_task->def.loop0 = new_loop0;
        schedule_async((async_task_t*)new_forasync_task, current_finish, NO_PROP);
    }
    // handling leftover
    if (size < high0) {
        #if DEBUG_FORASYNC
            printf("Scheduling Task %d %d\n",low0,high0);
        #endif
        forasync1D_task_t * new_forasync_task = allocate_forasync1D_task();
        new_forasync_task->task.forasync_task.def.fct_ptr = forasync1D_runner;
        new_forasync_task->task.forasync_task.def.arg = &(new_forasync_task->def);
        new_forasync_task->def.base.user = forasync->base.user;
        loop_domain_t new_loop0 = {low0, high0, loop0.stride, loop0.tile};
        new_forasync_task->def.loop0 = new_loop0;
        schedule_async((async_task_t*)new_forasync_task, current_finish, NO_PROP);
    }
}

void forasync2D_flat(void * forasync_arg) {
    forasync2D_t * forasync = (forasync2D_t *) forasync_arg;
    loop_domain_t loop0 = forasync->loop0;
    loop_domain_t loop1 = forasync->loop1;
    finish_t * current_finish = get_current_finish();
    int low0, low1;
    for(low0=loop0.low; low0<loop0.high; low0+=loop0.tile) {
        int high0 = (low0+loop0.tile)>loop0.high?loop0.high:(low0+loop0.tile);
        #if DEBUG_FORASYNC
            printf("Scheduling Task Loop1 %d %d\n",low0,high0);
        #endif
        for(low1=loop1.low; low1<loop1.high; low1+=loop1.tile) {
            int high1 = (low1+loop1.tile)>loop1.high?loop1.high:(low1+loop1.tile);
            #if DEBUG_FORASYNC
                printf("Scheduling Task %d %d\n",low1,high1);
            #endif
            forasync2D_task_t * new_forasync_task = allocate_forasync2D_task();
            new_forasync_task->task.forasync_task.def.fct_ptr = forasync2D_runner;
            new_forasync_task->task.forasync_task.def.arg = &(new_forasync_task->def);
            new_forasync_task->def.base.user = forasync->base.user;
            loop_domain_t new_loop0 = {low0, high0, loop0.stride, loop0.tile};
            new_forasync_task->def.loop0 = new_loop0;
            loop_domain_t new_loop1 = {low1, high1, loop1.stride, loop1.tile};
            new_forasync_task->def.loop1 = new_loop1;
            schedule_async((async_task_t*)new_forasync_task, current_finish, NO_PROP);
        }
    }
}

void forasync3D_flat(void * forasync_arg) {
    forasync3D_t * forasync = (forasync3D_t *) forasync_arg;
    loop_domain_t loop0 = forasync->loop0;
    loop_domain_t loop1 = forasync->loop1;
    loop_domain_t loop2 = forasync->loop2;
    finish_t * current_finish = get_current_finish();
    int low0, low1, low2;
    for(low0=loop0.low; low0<loop0.high; low0+=loop0.tile) {
        int high0 = (low0+loop0.tile)>loop0.high?loop0.high:(low0+loop0.tile);
        #if DEBUG_FORASYNC
            printf("Scheduling Task Loop1 %d %d\n",low0,high0);
        #endif
        for(low1=loop1.low; low1<loop1.high; low1+=loop1.tile) {
            int high1 = (low1+loop1.tile)>loop1.high?loop1.high:(low1+loop1.tile);
            #if DEBUG_FORASYNC
                printf("Scheduling Task Loop2 %d %d\n",low1,high1);
            #endif
            for(low2=loop2.low; low2<loop2.high; low2+=loop2.tile) {
                int high2 = (low2+loop2.tile)>loop2.high?loop2.high:(low2+loop2.tile);
                #if DEBUG_FORASYNC
                    printf("Scheduling Task %d %d\n",low2,high2);
                #endif
                forasync3D_task_t * new_forasync_task = allocate_forasync3D_task();
                new_forasync_task->task.forasync_task.def.fct_ptr = forasync3D_runner;
                new_forasync_task->task.forasync_task.def.arg = &(new_forasync_task->def);
                new_forasync_task->def.base.user = forasync->base.user;
                loop_domain_t new_loop0 = {low0, high0, loop0.stride, loop0.tile};
                new_forasync_task->def.loop0 = new_loop0;
                loop_domain_t new_loop1 = {low1, high1, loop1.stride, loop1.tile};
                new_forasync_task->def.loop1 = new_loop1;
                loop_domain_t new_loop2 = {low2, high2, loop2.stride, loop2.tile};
                new_forasync_task->def.loop2 = new_loop2;
                schedule_async((async_task_t*)new_forasync_task, current_finish, NO_PROP);
            }
        }
    }
}

static void forasync_internal(void* user_fct_ptr, void * user_arg,
                    accumed_t * accumed,
                    int dim, loop_domain_t * loop_domain, forasync_mode_t mode) {
    // All the sub-asyncs share async_def
    
    // The user loop code to execute
    async_t user_def;
    user_def.fct_ptr = user_fct_ptr;
    user_def.arg = user_arg;
    start_finish();
    if (accumed != NULL) {
        accum_register(accumed->accums, accumed->count);
    }

    assert(dim>0 && dim<4);
    // TODO put those somewhere as static
    asyncFct_t fct_ptr_rec[3] = {forasync1D_recursive, forasync2D_recursive, forasync3D_recursive};
    asyncFct_t fct_ptr_flat[3] = {forasync1D_flat, forasync2D_flat, forasync3D_flat};
    asyncFct_t * fct_ptr = (mode == FORASYNC_MODE_RECURSIVE) ? fct_ptr_rec : fct_ptr_flat;
    if (dim==1) {
        forasync1D_t forasync = {{&user_def}, loop_domain[0]};
        (fct_ptr[dim-1])((void *) &forasync);
    } else if(dim==2) {
        forasync2D_t forasync = {{&user_def}, loop_domain[0], loop_domain[1]};
        (fct_ptr[dim-1])((void *) &forasync);
    } else if(dim==3) {
        forasync3D_t forasync = {{&user_def}, loop_domain[0], loop_domain[1], loop_domain[2]};
        (fct_ptr[dim-1])((void *) &forasync);
    }

    end_finish();
}

//
//  forasync. runtime_type specifies the type of runtime (1 = recursive) (default = chunk)
void forasync(void* forasync_fct, void * argv, struct ddf_st ** ddf_list, struct _phased_t * phased_clause, 
            struct _accumed_t * accumed, int dim, loop_domain_t * domain, forasync_mode_t mode) {
    forasync_internal(forasync_fct, argv, accumed, dim, domain, mode);
}
