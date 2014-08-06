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

#ifndef HCLIB_H_
#define HCLIB_H_

/**
 * @file Interface to HCLIB
 */

/**
 * @defgroup HClib Finish/Async/Forasync
 * @brief Core API, Finish/Async/Forasync for structured parallelism.
 *
 * @{
 **/

#include "accumulator.h"
#include "ddf.h"

//TODO make this conditional in phased.h
struct _phased_t;

int get_worker_count();
int get_worker_id_hc();
void end_finish_comm_free();
void end_finish_comm_looper();
void start_finish_comm();
int totalPendingLocalAsyncs();


/**
 * @brief Initialize the HClib runtime.
 * Implicitly defines a global finish scope.
 */
void hclib_init(int * argc, char ** argv);

/**
 * @brief Finalize execution of the HClib runtime.
 * Ends the global finish scope and waits for all asyncs to terminate.
 */
void hclib_finalize();


//
// Default async arguments
//

/** @brief No properties defined. */
#define NO_PROP 0
/** @brief No arguments provided. */
#define NO_ARG NULL
/** @brief To satisfy a DDF with a 'NULL' value. */
#define NO_DATUM NULL
/** @brief No DDF argument provided. */
#define NO_DDF NULL
/** @brief No phaser argument provided. */
#define NO_PHASER NULL
/** @brief To indicate an async must register with all phasers. */
#define PHASER_TRANSMIT_ALL ((int) 0x1) 
/** @brief No accumulator argument provided. */
#define NO_ACCUM NULL


//
// Async definition and API
//

/**
 * @brief Function prototype executable by an async.
 * @param[in] arg           Arguments to the function
 */
typedef void (*asyncFct_t) (void * arg);

// forward declaration for phased clause defined in phased.h
struct _phased_t;

/**
 * @brief Spawn a new task asynchronously.
 * @param[in] fct_ptr           The function to execute
 * @param[in] arg               Argument to the async
 * @param[in] ddf_list          The list of DDFs the async depends on
 * @param[in] phased_clause     Phased clause to specify which phasers the async registers on
 * @param[in] property          Flag to pass information to the runtime
 */
void async(asyncFct_t fct_ptr, void * arg,
           struct ddf_st ** ddf_list, struct _phased_t * phased_clause, int property);

void async_comm(asyncFct_t fct_ptr, void * arg,
           struct ddf_st ** ddf_list, struct _phased_t * phased_clause, int property);
//
// Forasync definition and API
//

/** @brief forasync mode to control chunking strategy. */
typedef int forasync_mode_t;

/** @brief Forasync mode to recursively chunk the iteration space. */
#define FORASYNC_MODE_RECURSIVE 1
/** @brief Forasync mode to perform static chunking of the iteration space. */
#define FORASYNC_MODE_FLAT 0

/**
 * @brief Function prototype for a 1-dimension forasync.
 * @param[in] arg               Argument to the loop iteration
 * @param[in] index             Current iteration index
 */
typedef void (*forasync1D_Fct_t) (void * arg,int index);

/**
 * @brief Function prototype for a 2-dimensions forasync.
 * @param[in] arg               Argument to the loop iteration
 * @param[in] index_outer       Current outer iteration index
 * @param[in] index_inner       Current inner iteration index
 */
typedef void (*forasync2D_Fct_t) (void * arg,int index_outer,int index_inner);

/**
 * @brief Function prototype for a 3-dimensions forasync.
 * @param[in] arg               Argument to the loop iteration
 * @param[in] index_outer       Current outer iteration index
 * @param[in] index_mid         Current intermediate iteration index
 * @param[in] index_inner       Current inner iteration index
 */
typedef void (*forasync3D_Fct_t) (void * arg,int index_outer,int index_mid,int index_inner);

/** @struct loop_domain_t
 * @brief Describe loop domain when spawning a forasync.
 * @param[in] low       Lower bound for the loop
 * @param[in] high      Upper bound for the loop
 * @param[in] stride    Stride access
 * @param[in] tile      Tile size for chunking
 */
typedef struct _loop_domain_t {
    int low;
    int high;
    int stride;
    int tile;
} loop_domain_t;

/**
 * @brief Parallel for loop 'forasync' (up to 3 dimensions).
 *
 * Execute iterations of a loop in parallel. The loop domain allows 
 * to specify bounds as well as tiling information. Tiling of size one,
 * is equivalent to spawning each individual iteration as an async.
 *
 * @param[in] forasync_fct      The function pointer to execute.
 * @param[in] argv              Argument to the function
 * @param[in] ddf_list          DDFs dependences 
 * @param[in] phased_clause     Phasers registration
 * @param[in] accumed           Accumulators registration
 * @param[in] dim               Dimension of the loop
 * @param[in] domain            Loop domains to iterate over (array of size 'dim').
 * @param[in] mode              Forasync mode to control chunking strategy (flat chunking or recursive).
 */
void forasync(void* forasync_fct, void * argv, struct ddf_st ** ddf_list, struct _phased_t * phased_clause, 
            struct _accumed_t * accumed, int dim, loop_domain_t * domain, forasync_mode_t mode);

/**
 * @brief starts a new finish scope
 */
void start_finish();

/**
 * @brief ends the current finish scope
 */
void end_finish();

/**
 * @}
 */

#endif /* HCLIB_H_ */
