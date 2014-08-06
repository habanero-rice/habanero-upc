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

#include "rt-accumulator.h"
#include "ddf.h"
#include "runtime-hclib.h"

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

//
// Data-structure Types Implementation
//

 typedef struct accum_short_t {
    accum_impl_t impl;
    short result;
    void (*open)(struct accum_short_t * accum, short init);
    void (*put)(struct accum_short_t * accum, short v);
    short (*get)(struct accum_short_t * accum);
    void (*op)(short * res, short cur, short new);
 } accum_short_t;

void accum_op_short_max(short * res, short cur, short new) {
    if(new > cur) {
        *res = new;
    }
}

void accum_op_short_min(short * res, short cur, short new) {
    if(new < cur) {
        *res = new;
    }
}

void accum_op_short_plus(short * res, short cur, short new) {
    *res = cur + new;
}

void accum_op_short_prod(short * res, short cur, short new) {
    *res = cur * new;
}

void accum_op_short_land(short * res, short cur, short new) {
    *res = cur && new;
}

void accum_op_short_band(short * res, short cur, short new) {
    *res = cur & new;
}

void accum_op_short_lor(short * res, short cur, short new) {
    *res = cur || new;
}

void accum_op_short_bor(short * res, short cur, short new) {
    *res = cur | new;
}

void accum_op_short_lxor(short * res, short cur, short new) {
    *res = !cur != !new;
}

void accum_op_short_bxor(short * res, short cur, short new) {
    *res = cur ^ new;
}

 typedef struct accum_int_t {
    accum_impl_t impl;
    int result;
    void (*open)(struct accum_int_t * accum, int init);
    void (*put)(struct accum_int_t * accum, int v);
    int (*get)(struct accum_int_t * accum);
    void (*op)(int * res, int cur, int new);
 } accum_int_t;

void accum_op_int_max(int * res, int cur, int new) {
    if(new > cur) {
        *res = new;
    }
}

void accum_op_int_min(int * res, int cur, int new) {
    if(new < cur) {
        *res = new;
    }
}

void accum_op_int_plus(int * res, int cur, int new) {
    *res = cur + new;
}

void accum_op_int_prod(int * res, int cur, int new) {
    *res = cur * new;
}

void accum_op_int_land(int * res, int cur, int new) {
    *res = cur && new;
}

void accum_op_int_band(int * res, int cur, int new) {
    *res = cur & new;
}

void accum_op_int_lor(int * res, int cur, int new) {
    *res = cur || new;
}

void accum_op_int_bor(int * res, int cur, int new) {
    *res = cur | new;
}

void accum_op_int_lxor(int * res, int cur, int new) {
    *res = !cur != !new;
}

void accum_op_int_bxor(int * res, int cur, int new) {
    *res = cur ^ new;
}

 typedef struct accum_long_t {
    accum_impl_t impl;
    long result;
    void (*open)(struct accum_long_t * accum, long init);
    void (*put)(struct accum_long_t * accum, long v);
    long (*get)(struct accum_long_t * accum);
    void (*op)(long * res, long cur, long new);
 } accum_long_t;

void accum_op_long_max(long * res, long cur, long new) {
    if(new > cur) {
        *res = new;
    }
}

void accum_op_long_min(long * res, long cur, long new) {
    if(new < cur) {
        *res = new;
    }
}

void accum_op_long_plus(long * res, long cur, long new) {
    *res = cur + new;
}

void accum_op_long_prod(long * res, long cur, long new) {
    *res = cur * new;
}

void accum_op_long_land(long * res, long cur, long new) {
    *res = cur && new;
}

void accum_op_long_band(long * res, long cur, long new) {
    *res = cur & new;
}

void accum_op_long_lor(long * res, long cur, long new) {
    *res = cur || new;
}

void accum_op_long_bor(long * res, long cur, long new) {
    *res = cur | new;
}

void accum_op_long_lxor(long * res, long cur, long new) {
    *res = !cur != !new;
}

void accum_op_long_bxor(long * res, long cur, long new) {
    *res = cur ^ new;
}

 typedef struct accum_float_t {
    accum_impl_t impl;
    float result;
    void (*open)(struct accum_float_t * accum, float init);
    void (*put)(struct accum_float_t * accum, float v);
    float (*get)(struct accum_float_t * accum);
    void (*op)(float * res, float cur, float new);
 } accum_float_t;

void accum_op_float_max(float * res, float cur, float new) {
    if(new > cur) {
        *res = new;
    }
}

void accum_op_float_min(float * res, float cur, float new) {
    if(new < cur) {
        *res = new;
    }
}

void accum_op_float_plus(float * res, float cur, float new) {
    *res = cur + new;
}

void accum_op_float_prod(float * res, float cur, float new) {
    *res = cur * new;
}

 typedef struct accum_double_t {
    accum_impl_t impl;
    double result;
    void (*open)(struct accum_double_t * accum, double init);
    void (*put)(struct accum_double_t * accum, double v);
    double (*get)(struct accum_double_t * accum);
    void (*op)(double * res, double cur, double new);
 } accum_double_t;

void accum_op_double_max(double * res, double cur, double new) {
    if(new > cur) {
        *res = new;
    }
}

void accum_op_double_min(double * res, double cur, double new) {
    if(new < cur) {
        *res = new;
    }
}

void accum_op_double_plus(double * res, double cur, double new) {
    *res = cur + new;
}

void accum_op_double_prod(double * res, double cur, double new) {
    *res = cur * new;
}

//
// Lazy Implementation
//

typedef struct accum_short_lazy_t {
    accum_short_t base;
    short * localAccums;
} accum_short_lazy_t;


void accum_destroy_short_lazy(accum_impl_t * accum) {
    free(((accum_short_lazy_t *) accum)->localAccums);
    free(accum);
}

void accum_close_short_lazy(accum_impl_t * accum) {
    accum_short_t * accum_short = ((accum_short_t *) accum);
    short * result = &(accum_short->result);
    short * localAccums = ((accum_short_lazy_t *) accum)->localAccums;
    int i = 0;
    int nb_w = get_nb_workers();
    // Sequential reduce of local accumulators
    while (i < nb_w) {
        accum_short->op(result, *result, localAccums[i]);
        i++;
    }
}

accum_t * accum_transmit_short_lazy(accum_impl_t * accum, void * params, int properties) {
    // No-op
    return (accum_t *) accum;
}

void accum_open_short_lazy(accum_short_t * accum, short init) {
    accum->result = init;
    int nb_w = get_nb_workers();
    // Initialize local accumulators for each worker
    accum_short_lazy_t * accum_lazy = (accum_short_lazy_t *) accum;
    short * localAccums = malloc(sizeof(short) * nb_w);
    int i = 0;
    while (i < nb_w) {
        localAccums[i] = init;
        i++;
    }
    accum_lazy->localAccums = localAccums;
}

void accum_put_short_lazy(accum_short_t * accum, short v) {
    int wid = get_worker_id_hc();
    accum_short_lazy_t * accum_lazy = (accum_short_lazy_t *) accum;
    return accum->op(&(accum_lazy->localAccums[wid]), accum_lazy->localAccums[wid], v);
}

short accum_get_short_lazy(accum_short_t * accum) {
    return accum->result;
}

typedef struct accum_int_lazy_t {
    accum_int_t base;
    int * localAccums;
} accum_int_lazy_t;


void accum_destroy_int_lazy(accum_impl_t * accum) {
    free(((accum_int_lazy_t *) accum)->localAccums);
    free(accum);
}

void accum_close_int_lazy(accum_impl_t * accum) {
    accum_int_t * accum_int = ((accum_int_t *) accum);
    int * result = &(accum_int->result);
    int * localAccums = ((accum_int_lazy_t *) accum)->localAccums;
    int i = 0;
    int nb_w = get_nb_workers();
    // Sequential reduce of local accumulators
    while (i < nb_w) {
        accum_int->op(result, *result, localAccums[i]);
        i++;
    }
}

accum_t * accum_transmit_int_lazy(accum_impl_t * accum, void * params, int properties) {
    // No-op
    return (accum_t *) accum;
}

void accum_open_int_lazy(accum_int_t * accum, int init) {
    accum->result = init;
    int nb_w = get_nb_workers();
    // Initialize local accumulators for each worker
    accum_int_lazy_t * accum_lazy = (accum_int_lazy_t *) accum;
    int * localAccums = malloc(sizeof(int) * nb_w);
    int i = 0;
    while (i < nb_w) {
        localAccums[i] = init;
        i++;
    }
    accum_lazy->localAccums = localAccums;
}

void accum_put_int_lazy(accum_int_t * accum, int v) {
    int wid = get_worker_id_hc();
    accum_int_lazy_t * accum_lazy = (accum_int_lazy_t *) accum;
    return accum->op(&(accum_lazy->localAccums[wid]), accum_lazy->localAccums[wid], v);
}

int accum_get_int_lazy(accum_int_t * accum) {
    return accum->result;
}

typedef struct accum_long_lazy_t {
    accum_long_t base;
    long * localAccums;
} accum_long_lazy_t;


void accum_destroy_long_lazy(accum_impl_t * accum) {
    free(((accum_long_lazy_t *) accum)->localAccums);
    free(accum);
}

void accum_close_long_lazy(accum_impl_t * accum) {
    accum_long_t * accum_long = ((accum_long_t *) accum);
    long * result = &(accum_long->result);
    long * localAccums = ((accum_long_lazy_t *) accum)->localAccums;
    int i = 0;
    int nb_w = get_nb_workers();
    // Sequential reduce of local accumulators
    while (i < nb_w) {
        accum_long->op(result, *result, localAccums[i]);
        i++;
    }
}

accum_t * accum_transmit_long_lazy(accum_impl_t * accum, void * params, int properties) {
    // No-op
    return (accum_t *) accum;
}

void accum_open_long_lazy(accum_long_t * accum, long init) {
    accum->result = init;
    int nb_w = get_nb_workers();
    // Initialize local accumulators for each worker
    accum_long_lazy_t * accum_lazy = (accum_long_lazy_t *) accum;
    long * localAccums = malloc(sizeof(long) * nb_w);
    int i = 0;
    while (i < nb_w) {
        localAccums[i] = init;
        i++;
    }
    accum_lazy->localAccums = localAccums;
}

void accum_put_long_lazy(accum_long_t * accum, long v) {
    int wid = get_worker_id_hc();
    accum_long_lazy_t * accum_lazy = (accum_long_lazy_t *) accum;
    return accum->op(&(accum_lazy->localAccums[wid]), accum_lazy->localAccums[wid], v);
}

long accum_get_long_lazy(accum_long_t * accum) {
    return accum->result;
}

typedef struct accum_float_lazy_t {
    accum_float_t base;
    float * localAccums;
} accum_float_lazy_t;


void accum_destroy_float_lazy(accum_impl_t * accum) {
    free(((accum_float_lazy_t *) accum)->localAccums);
    free(accum);
}

void accum_close_float_lazy(accum_impl_t * accum) {
    accum_float_t * accum_float = ((accum_float_t *) accum);
    float * result = &(accum_float->result);
    float * localAccums = ((accum_float_lazy_t *) accum)->localAccums;
    int i = 0;
    int nb_w = get_nb_workers();
    // Sequential reduce of local accumulators
    while (i < nb_w) {
        accum_float->op(result, *result, localAccums[i]);
        i++;
    }
}

accum_t * accum_transmit_float_lazy(accum_impl_t * accum, void * params, int properties) {
    // No-op
    return (accum_t *) accum;
}

void accum_open_float_lazy(accum_float_t * accum, float init) {
    accum->result = init;
    int nb_w = get_nb_workers();
    // Initialize local accumulators for each worker
    accum_float_lazy_t * accum_lazy = (accum_float_lazy_t *) accum;
    float * localAccums = malloc(sizeof(float) * nb_w);
    int i = 0;
    while (i < nb_w) {
        localAccums[i] = init;
        i++;
    }
    accum_lazy->localAccums = localAccums;
}

void accum_put_float_lazy(accum_float_t * accum, float v) {
    int wid = get_worker_id_hc();
    accum_float_lazy_t * accum_lazy = (accum_float_lazy_t *) accum;
    return accum->op(&(accum_lazy->localAccums[wid]), accum_lazy->localAccums[wid], v);
}

float accum_get_float_lazy(accum_float_t * accum) {
    return accum->result;
}

typedef struct accum_double_lazy_t {
    accum_double_t base;
    double * localAccums;
} accum_double_lazy_t;


void accum_destroy_double_lazy(accum_impl_t * accum) {
    free(((accum_double_lazy_t *) accum)->localAccums);
    free(accum);
}

void accum_close_double_lazy(accum_impl_t * accum) {
    accum_double_t * accum_double = ((accum_double_t *) accum);
    double * result = &(accum_double->result);
    double * localAccums = ((accum_double_lazy_t *) accum)->localAccums;
    int i = 0;
    int nb_w = get_nb_workers();
    // Sequential reduce of local accumulators
    while (i < nb_w) {
        accum_double->op(result, *result, localAccums[i]);
        i++;
    }
}

accum_t * accum_transmit_double_lazy(accum_impl_t * accum, void * params, int properties) {
    // No-op
    return (accum_t *) accum;
}

void accum_open_double_lazy(accum_double_t * accum, double init) {
    accum->result = init;
    int nb_w = get_nb_workers();
    // Initialize local accumulators for each worker
    accum_double_lazy_t * accum_lazy = (accum_double_lazy_t *) accum;
    double * localAccums = malloc(sizeof(double) * nb_w);
    int i = 0;
    while (i < nb_w) {
        localAccums[i] = init;
        i++;
    }
    accum_lazy->localAccums = localAccums;
}

void accum_put_double_lazy(accum_double_t * accum, double v) {
    int wid = get_worker_id_hc();
    accum_double_lazy_t * accum_lazy = (accum_double_lazy_t *) accum;
    return accum->op(&(accum_lazy->localAccums[wid]), accum_lazy->localAccums[wid], v);
}

double accum_get_double_lazy(accum_double_t * accum) {
    return accum->result;
}

//
// API Implementation
//

accum_t * accum_create_short(accum_op_t op, accum_mode_t mode, short init) {
    accum_short_t * accum_short = NULL;
    if(mode == ACCUM_MODE_SEQ) {
        assert(0);
        // accum_short = (accum_short_t *) malloc(sizeof(accum_short_t));
        // accum_impl_t * accum_impl = (accum_impl_t *) accum_short;
        // accum_impl->close = accum_close_short_seq;
        // accum_impl->transmit = accum_transmit_short_seq;
        // accum_impl->destroy = accum_destroy_short_seq;
        // accum_short->open = accum_open_short_seq;
        // accum_short->get = accum_get_short_seq;
        // accum_short->put = accum_put_short_seq;
    } else if(mode == ACCUM_MODE_REC) {
        assert(0);
        // accum_short_rec_t * accum_rec = malloc(sizeof(accum_short_rec_t));
        // //TODO parameterize degree and contributors
        // accum_rec->degree = 2;
        // accum_rec->children = NULL;
        // accum_short = (accum_short_t *) accum_rec;
        // accum_impl_t * accum_impl = (accum_impl_t *) accum_short;
        // accum_impl->close = accum_close_short_rec;
        // accum_impl->transmit = accum_transmit_short_rec;
        // accum_impl->destroy = accum_destroy_short_rec;
        // accum_short->open = accum_open_short_rec;
        // accum_short->get = accum_get_short_rec;
        // accum_short->put = accum_put_short_rec;
    } else if(mode == ACCUM_MODE_LAZY) {
        accum_short_lazy_t * accum_lazy = malloc(sizeof(accum_short_lazy_t));
        //TODO parameterize degree and contributors
        accum_lazy->localAccums = NULL;
        accum_short = (accum_short_t *) accum_lazy;
        accum_impl_t * accum_impl = (accum_impl_t *) accum_short;
        accum_impl->close = accum_close_short_lazy;
        accum_impl->transmit = accum_transmit_short_lazy;
        accum_impl->destroy = accum_destroy_short_lazy;
        accum_short->open = accum_open_short_lazy;
        accum_short->get = accum_get_short_lazy;
        accum_short->put = accum_put_short_lazy;
    } else {
        assert(0 && "error: short accumulator mode not implemented");
    }

    switch(op) {
        case ACCUM_OP_NONE:
            // Used internally
            break;
        case ACCUM_OP_MAX:
            accum_short->op = accum_op_short_max;
            break;
        case ACCUM_OP_MIN:
            accum_short->op = accum_op_short_min;
            break;
        case ACCUM_OP_PLUS:
            accum_short->op = accum_op_short_plus;
            break;
        case ACCUM_OP_PROD:
            accum_short->op = accum_op_short_prod;
            break;
        case ACCUM_OP_LAND:
            accum_short->op = accum_op_short_land;
            break;
        case ACCUM_OP_BAND:
            accum_short->op = accum_op_short_band;
            break;
        case ACCUM_OP_LOR:
            accum_short->op = accum_op_short_lor;
            break;
        case ACCUM_OP_BOR:
            accum_short->op = accum_op_short_bor;
            break;
        case ACCUM_OP_LXOR:
            accum_short->op = accum_op_short_lxor;
            break;
        case ACCUM_OP_BXOR:
            accum_short->op = accum_op_short_bxor;
            break;
        default:
            assert(0 && "error: short accumulator operation not implemented");
    }

    // Open the accumulator
    accum_short->open(accum_short, init);
    return (accum_t *) accum_short;
}

accum_t * accum_create_int(accum_op_t op, accum_mode_t mode, int init) {
    accum_int_t * accum_int = NULL;
    if(mode == ACCUM_MODE_SEQ) {
        assert(0);
        // accum_int = (accum_int_t *) malloc(sizeof(accum_int_t));
        // accum_impl_t * accum_impl = (accum_impl_t *) accum_int;
        // accum_impl->close = accum_close_int_seq;
        // accum_impl->transmit = accum_transmit_int_seq;
        // accum_impl->destroy = accum_destroy_int_seq;
        // accum_int->open = accum_open_int_seq;
        // accum_int->get = accum_get_int_seq;
        // accum_int->put = accum_put_int_seq;
    } else if(mode == ACCUM_MODE_REC) {
        assert(0);
        // accum_int_rec_t * accum_rec = malloc(sizeof(accum_int_rec_t));
        // //TODO parameterize degree and contributors
        // accum_rec->degree = 2;
        // accum_rec->children = NULL;
        // accum_int = (accum_int_t *) accum_rec;
        // accum_impl_t * accum_impl = (accum_impl_t *) accum_int;
        // accum_impl->close = accum_close_int_rec;
        // accum_impl->transmit = accum_transmit_int_rec;
        // accum_impl->destroy = accum_destroy_int_rec;
        // accum_int->open = accum_open_int_rec;
        // accum_int->get = accum_get_int_rec;
        // accum_int->put = accum_put_int_rec;
    } else if(mode == ACCUM_MODE_LAZY) {
        accum_int_lazy_t * accum_lazy = malloc(sizeof(accum_int_lazy_t));
        //TODO parameterize degree and contributors
        accum_lazy->localAccums = NULL;
        accum_int = (accum_int_t *) accum_lazy;
        accum_impl_t * accum_impl = (accum_impl_t *) accum_int;
        accum_impl->close = accum_close_int_lazy;
        accum_impl->transmit = accum_transmit_int_lazy;
        accum_impl->destroy = accum_destroy_int_lazy;
        accum_int->open = accum_open_int_lazy;
        accum_int->get = accum_get_int_lazy;
        accum_int->put = accum_put_int_lazy;
    } else {
        assert(0 && "error: int accumulator mode not implemented");
    }

    switch(op) {
        case ACCUM_OP_NONE:
            // Used internally
            break;
        case ACCUM_OP_MAX:
            accum_int->op = accum_op_int_max;
            break;
        case ACCUM_OP_MIN:
            accum_int->op = accum_op_int_min;
            break;
        case ACCUM_OP_PLUS:
            accum_int->op = accum_op_int_plus;
            break;
        case ACCUM_OP_PROD:
            accum_int->op = accum_op_int_prod;
            break;
        case ACCUM_OP_LAND:
            accum_int->op = accum_op_int_land;
            break;
        case ACCUM_OP_BAND:
            accum_int->op = accum_op_int_band;
            break;
        case ACCUM_OP_LOR:
            accum_int->op = accum_op_int_lor;
            break;
        case ACCUM_OP_BOR:
            accum_int->op = accum_op_int_bor;
            break;
        case ACCUM_OP_LXOR:
            accum_int->op = accum_op_int_lxor;
            break;
        case ACCUM_OP_BXOR:
            accum_int->op = accum_op_int_bxor;
            break;
        default:
            assert(0 && "error: int accumulator operation not implemented");
    }

    // Open the accumulator
    accum_int->open(accum_int, init);
    return (accum_t *) accum_int;
}
accum_t * accum_create_long(accum_op_t op, accum_mode_t mode, long init) {
    accum_long_t * accum_long = NULL;
    if(mode == ACCUM_MODE_SEQ) {
        assert(0);
        // accum_long = (accum_long_t *) malloc(sizeof(accum_long_t));
        // accum_impl_t * accum_impl = (accum_impl_t *) accum_long;
        // accum_impl->close = accum_close_long_seq;
        // accum_impl->transmit = accum_transmit_long_seq;
        // accum_impl->destroy = accum_destroy_long_seq;
        // accum_long->open = accum_open_long_seq;
        // accum_long->get = accum_get_long_seq;
        // accum_long->put = accum_put_long_seq;
    } else if(mode == ACCUM_MODE_REC) {
        assert(0);
        // accum_long_rec_t * accum_rec = malloc(sizeof(accum_long_rec_t));
        // //TODO parameterize degree and contributors
        // accum_rec->degree = 2;
        // accum_rec->children = NULL;
        // accum_long = (accum_long_t *) accum_rec;
        // accum_impl_t * accum_impl = (accum_impl_t *) accum_long;
        // accum_impl->close = accum_close_long_rec;
        // accum_impl->transmit = accum_transmit_long_rec;
        // accum_impl->destroy = accum_destroy_long_rec;
        // accum_long->open = accum_open_long_rec;
        // accum_long->get = accum_get_long_rec;
        // accum_long->put = accum_put_long_rec;
    } else if(mode == ACCUM_MODE_LAZY) {
        accum_long_lazy_t * accum_lazy = malloc(sizeof(accum_long_lazy_t));
        //TODO parameterize degree and contributors
        accum_lazy->localAccums = NULL;
        accum_long = (accum_long_t *) accum_lazy;
        accum_impl_t * accum_impl = (accum_impl_t *) accum_long;
        accum_impl->close = accum_close_long_lazy;
        accum_impl->transmit = accum_transmit_long_lazy;
        accum_impl->destroy = accum_destroy_long_lazy;
        accum_long->open = accum_open_long_lazy;
        accum_long->get = accum_get_long_lazy;
        accum_long->put = accum_put_long_lazy;
    } else {
        assert(0 && "error: long accumulator mode not implemented");
    }

    switch(op) {
        case ACCUM_OP_NONE:
            // Used internally
            break;
        case ACCUM_OP_MAX:
            accum_long->op = accum_op_long_max;
            break;
        case ACCUM_OP_MIN:
            accum_long->op = accum_op_long_min;
            break;
        case ACCUM_OP_PLUS:
            accum_long->op = accum_op_long_plus;
            break;
        case ACCUM_OP_PROD:
            accum_long->op = accum_op_long_prod;
            break;
        case ACCUM_OP_LAND:
            accum_long->op = accum_op_long_land;
            break;
        case ACCUM_OP_BAND:
            accum_long->op = accum_op_long_band;
            break;
        case ACCUM_OP_LOR:
            accum_long->op = accum_op_long_lor;
            break;
        case ACCUM_OP_BOR:
            accum_long->op = accum_op_long_bor;
            break;
        case ACCUM_OP_LXOR:
            accum_long->op = accum_op_long_lxor;
            break;
        case ACCUM_OP_BXOR:
            accum_long->op = accum_op_long_bxor;
            break;
        default:
            assert(0 && "error: long accumulator operation not implemented");
    }

    // Open the accumulator
    accum_long->open(accum_long, init);
    return (accum_t *) accum_long;
}
accum_t * accum_create_float(accum_op_t op, accum_mode_t mode, float init) {
    accum_float_t * accum_float = NULL;
    if(mode == ACCUM_MODE_SEQ) {
        assert(0);
        // accum_float = (accum_float_t *) malloc(sizeof(accum_float_t));
        // accum_impl_t * accum_impl = (accum_impl_t *) accum_float;
        // accum_impl->close = accum_close_float_seq;
        // accum_impl->transmit = accum_transmit_float_seq;
        // accum_impl->destroy = accum_destroy_float_seq;
        // accum_float->open = accum_open_float_seq;
        // accum_float->get = accum_get_float_seq;
        // accum_float->put = accum_put_float_seq;
    } else if(mode == ACCUM_MODE_REC) {
        assert(0);
        // accum_float_rec_t * accum_rec = malloc(sizeof(accum_float_rec_t));
        // //TODO parameterize degree and contributors
        // accum_rec->degree = 2;
        // accum_rec->children = NULL;
        // accum_float = (accum_float_t *) accum_rec;
        // accum_impl_t * accum_impl = (accum_impl_t *) accum_float;
        // accum_impl->close = accum_close_float_rec;
        // accum_impl->transmit = accum_transmit_float_rec;
        // accum_impl->destroy = accum_destroy_float_rec;
        // accum_float->open = accum_open_float_rec;
        // accum_float->get = accum_get_float_rec;
        // accum_float->put = accum_put_float_rec;
    } else if(mode == ACCUM_MODE_LAZY) {
        accum_float_lazy_t * accum_lazy = malloc(sizeof(accum_float_lazy_t));
        //TODO parameterize degree and contributors
        accum_lazy->localAccums = NULL;
        accum_float = (accum_float_t *) accum_lazy;
        accum_impl_t * accum_impl = (accum_impl_t *) accum_float;
        accum_impl->close = accum_close_float_lazy;
        accum_impl->transmit = accum_transmit_float_lazy;
        accum_impl->destroy = accum_destroy_float_lazy;
        accum_float->open = accum_open_float_lazy;
        accum_float->get = accum_get_float_lazy;
        accum_float->put = accum_put_float_lazy;
    } else {
        assert(0 && "error: float accumulator mode not implemented");
    }

    switch(op) {
        case ACCUM_OP_NONE:
            // Used internally
            break;
        case ACCUM_OP_MAX:
            accum_float->op = accum_op_float_max;
            break;
        case ACCUM_OP_MIN:
            accum_float->op = accum_op_float_min;
            break;
        case ACCUM_OP_PLUS:
            accum_float->op = accum_op_float_plus;
            break;
        case ACCUM_OP_PROD:
            accum_float->op = accum_op_float_prod;
            break;
        default:
            assert(0 && "error: float accumulator operation not implemented");
    }

    // Open the accumulator
    accum_float->open(accum_float, init);
    return (accum_t *) accum_float;
}
accum_t * accum_create_double(accum_op_t op, accum_mode_t mode, double init) {
    accum_double_t * accum_double = NULL;
    if(mode == ACCUM_MODE_SEQ) {
        assert(0);
        // accum_double = (accum_double_t *) malloc(sizeof(accum_double_t));
        // accum_impl_t * accum_impl = (accum_impl_t *) accum_double;
        // accum_impl->close = accum_close_double_seq;
        // accum_impl->transmit = accum_transmit_double_seq;
        // accum_impl->destroy = accum_destroy_double_seq;
        // accum_double->open = accum_open_double_seq;
        // accum_double->get = accum_get_double_seq;
        // accum_double->put = accum_put_double_seq;
    } else if(mode == ACCUM_MODE_REC) {
        assert(0);
        // accum_double_rec_t * accum_rec = malloc(sizeof(accum_double_rec_t));
        // //TODO parameterize degree and contributors
        // accum_rec->degree = 2;
        // accum_rec->children = NULL;
        // accum_double = (accum_double_t *) accum_rec;
        // accum_impl_t * accum_impl = (accum_impl_t *) accum_double;
        // accum_impl->close = accum_close_double_rec;
        // accum_impl->transmit = accum_transmit_double_rec;
        // accum_impl->destroy = accum_destroy_double_rec;
        // accum_double->open = accum_open_double_rec;
        // accum_double->get = accum_get_double_rec;
        // accum_double->put = accum_put_double_rec;
    } else if(mode == ACCUM_MODE_LAZY) {
        accum_double_lazy_t * accum_lazy = malloc(sizeof(accum_double_lazy_t));
        //TODO parameterize degree and contributors
        accum_lazy->localAccums = NULL;
        accum_double = (accum_double_t *) accum_lazy;
        accum_impl_t * accum_impl = (accum_impl_t *) accum_double;
        accum_impl->close = accum_close_double_lazy;
        accum_impl->transmit = accum_transmit_double_lazy;
        accum_impl->destroy = accum_destroy_double_lazy;
        accum_double->open = accum_open_double_lazy;
        accum_double->get = accum_get_double_lazy;
        accum_double->put = accum_put_double_lazy;
    } else {
        assert(0 && "error: double accumulator mode not implemented");
    }

    switch(op) {
        case ACCUM_OP_NONE:
            // Used internally
            break;
        case ACCUM_OP_MAX:
            accum_double->op = accum_op_double_max;
            break;
        case ACCUM_OP_MIN:
            accum_double->op = accum_op_double_min;
            break;
        case ACCUM_OP_PLUS:
            accum_double->op = accum_op_double_plus;
            break;
        case ACCUM_OP_PROD:
            accum_double->op = accum_op_double_prod;
            break;
        default:
            assert(0 && "error: double accumulator operation not implemented");
    }

    // Open the accumulator
    accum_double->open(accum_double, init);
    return (accum_t *) accum_double;
}
//
// User API Interface Implementation
//

short accum_get_short(accum_t * acc) {
    accum_short_t * acc_short = (accum_short_t *) acc;
    return acc_short->get(acc_short);
}

void accum_put_short(accum_t * acc, short v) {
    accum_short_t * acc_short = (accum_short_t *) acc;
    return acc_short->put(acc_short, v);
}

int accum_get_int(accum_t * acc) {
    accum_int_t * acc_int = (accum_int_t *) acc;
    return acc_int->get(acc_int);
}

void accum_put_int(accum_t * acc, int v) {
    accum_int_t * acc_int = (accum_int_t *) acc;
    return acc_int->put(acc_int, v);
}

long accum_get_long(accum_t * acc) {
    accum_long_t * acc_long = (accum_long_t *) acc;
    return acc_long->get(acc_long);
}

void accum_put_long(accum_t * acc, long v) {
    accum_long_t * acc_long = (accum_long_t *) acc;
    return acc_long->put(acc_long, v);
}

float accum_get_float(accum_t * acc) {
    accum_float_t * acc_float = (accum_float_t *) acc;
    return acc_float->get(acc_float);
}

void accum_put_float(accum_t * acc, float v) {
    accum_float_t * acc_float = (accum_float_t *) acc;
    return acc_float->put(acc_float, v);
}

double accum_get_double(accum_t * acc) {
    accum_double_t * acc_double = (accum_double_t *) acc;
    return acc_double->get(acc_double);
}

void accum_put_double(accum_t * acc, double v) {
    accum_double_t * acc_double = (accum_double_t *) acc;
    return acc_double->put(acc_double, v);
}

void accum_register(accum_t ** accs, int n) {
    int i = 0;
    finish_t * finish = get_current_finish();
    //TODO this goes away when we switch to static registration in start_finish
    assert((finish->accumulators == NULL) && "error: overwritting registered accumulators");
    accum_t ** accumulators = (accum_t **) malloc(sizeof(accum_t*) * (n+1));
    while (i < n) {
        accumulators[i] = accs[i];
        i++;
    }
    accumulators[i] = NULL;
    finish->accumulators = accumulators;
}

void accum_destroy(accum_t * acc) {
    accum_impl_t * acc_impl = (accum_impl_t *) acc;
    return acc_impl->destroy(acc_impl);
}
