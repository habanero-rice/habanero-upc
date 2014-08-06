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

#ifndef ACCUMULATOR_H_
#define ACCUMULATOR_H_

/**
 * @defgroup Accumulators Accumulators
 * @brief Accumulator API for reductions.
 *
 * @{
 **/

/**
 * @brief Runtime-supported accumulator operations.
 */
typedef enum {
    ACCUM_OP_NONE,
    ACCUM_OP_PLUS,
    ACCUM_OP_PROD,
    ACCUM_OP_MIN,
    ACCUM_OP_MAX,
    ACCUM_OP_LAND,
    ACCUM_OP_BAND,
    ACCUM_OP_LOR,
    ACCUM_OP_BOR,
    ACCUM_OP_LXOR,
    ACCUM_OP_BXOR
} accum_op_t;

/**
 * @brief Accumulators' reduction modes.
 * To specify the accumulation strategy an accumulator is backed by.
 */
typedef enum {
    ACCUM_MODE_SEQ,
    ACCUM_MODE_LAZY,
    ACCUM_MODE_REC
} accum_mode_t;

/**
 * @brief Opaque type for accumulators.
 */
typedef struct _accum_t {
} accum_t;

/**
 * @brief Accumed clause.
 * User-level data-structure to represent an accumulator clause.
 */
typedef struct _accumed_t {
    int count;
    accum_t ** accums;
} accumed_t;

/**
 * @brief Register a list of accumulators in the current finish scope.
 * @param[in] accs        The accumulator list to register
 * @param[in] v           The size of the list
 * Warning/Limitation: this overwrites currently registered accum
 */
void accum_register(accum_t ** accs, int n);

/**
 * @brief Allocate and initialize an 'int' accumulator.
 * @param[in] op         Operation to apply on reduction
 * @param[in] mode       Accumulator backend implementation
 * @param[in] init       Initial value of the accumulator
 * @return A contiguous array of DDFs
 */
accum_t * accum_create_int(accum_op_t op, accum_mode_t mode, int init);

/**
 * @brief Get the value of an accumulator.
 * @param[in] acc        The accumulator to get the value from
 * @return Current accumulator's value
 */
int accum_get_int(accum_t * acc);

/**
 * @brief Contribute a value to an accumulator.
 * @param[in] acc        The accumulator to put a value
 * @param[in] v          The value to accumulate
 */
void accum_put_int(accum_t * acc, int v);

/**
 * @brief Allocate and initialize a 'double' accumulator.
 * @param[in] op         Operation to apply on reduction
 * @param[in] mode       Accumulator backend implementation
 * @param[in] init       Initial value of the accumulator
 * @return A contiguous array of DDFs
 */
accum_t * accum_create_double(accum_op_t op, accum_mode_t mode, double init);

/**
 * @brief Get the value of an accumulator.
 * @param[in] acc        The accumulator to get the value from
 * @return Current accumulator's value
 */
double accum_get_double(accum_t * acc);

/**
 * @brief Contribute a value to an accumulator.
 * @param[in] acc        The accumulator to put a value
 * @param[in] v          The value to accumulate
 */
void accum_put_double(accum_t * acc, double v);

/**
 * @brief Destroy an accumulator.
 * @param[in] acc        The accumulator to destroy
 */
void accum_destroy(accum_t * acc);

/**
 * @}
 */

#endif /* ACCUMULATOR_H_ */
