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

#ifndef DDF_H_
#define DDF_H_

#include <stdlib.h>

/**
 * @file User Interface to HCLIB's Data-Driven Futures
 */

/**
 * @defgroup DDF Data-Driven Future
 * @brief Data-Driven Future API for data-flow like programming.
 *
 * @{
 **/

/**
 * @brief Opaque type for DDFs.
 */
struct ddf_st;

/**
 * @brief Allocate and initialize a DDF.
 * @return A DDF.
 */
struct ddf_st * ddf_create();

/**
 * @brief Allocate and initialize an array of DDFs.
 * @param[in] nb_ddfs 				Size of the DDF array
 * @param[in] null_terminated 		If true, create nb_ddfs-1 and set the last element to NULL.
 * @return A contiguous array of DDFs
 */
struct ddf_st ** ddf_create_n(size_t nb_ddfs, int null_terminated);

/**
 * @brief Destruct a DDF.
 * @param[in] ddf 				The DDF to destruct
 */
void ddf_free(struct ddf_st * ddf);

/**
 * @brief Get the value of a DDF.
 * @param[in] ddf 				The DDF to get a value from
 */
void * ddf_get(struct ddf_st * ddf);

/**
 * @brief Put a value in a DDF.
 * @param[in] ddf 				The DDF to get a value from
 * @param[in] datum 			The datum to be put in the DDF
 */
void ddf_put(struct ddf_st * ddf, void * datum);

/**
 * @}
 */

 #endif /* DDF_H_ */
