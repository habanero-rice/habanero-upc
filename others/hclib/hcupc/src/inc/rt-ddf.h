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

#ifndef RT_DDF_H_
#define RT_DDF_H_

#include "rt-hclib-def.h"

/**
 * This file contains the runtime-level DDF interface
 */

// forward declaration
struct ddf_st;

/**
 * DDT data-structure to associate DDTs and DDFs.
 * This is exposed so that the runtime know the size of the struct.
 */
typedef struct ddt_st {
    // NULL-terminated list of DDFs the DDT is registered on
    struct ddf_st ** waitingFrontier;
    // This allows us to chain all DDTs waiting on a same DDF
    // Whenever a DDT wants to register on a DDF, and that DDF is
    // not ready, we chain the current DDT and the DDF's headDDTWaitList
    // and try to cas on the DDF's headDDTWaitList, with the current DDT.
    struct ddt_st * nextDDTWaitingOnSameDDF;
} ddt_t;

/**
 * Initialize a DDT
 */
void ddt_init(ddt_t * ddt, struct ddf_st ** ddf_list);

/**
 * Runtime interface to iterate over the frontier of a DDT.
 * If all DDFs are ready, returns 1, otherwise returns 0
 */
int iterate_ddt_frontier(ddt_t * ddt);

#endif /* RT_DDF_H_ */
