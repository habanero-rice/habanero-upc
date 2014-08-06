
#include "accumulator.h"

// Accum base implementation for all type of accumulators
 typedef struct accum_impl_t {
    accum_t base;
    // Close the accumulator, no more puts allowed, wrap-up any work left
    void (*close)(struct accum_impl_t * accum);
    // Transmit the accumulator to another context (finish, async etc..)
    accum_t * (*transmit)(struct accum_impl_t * accum, void * params, int properties);
    void (*destroy)(struct accum_impl_t * accum);
 } accum_impl_t;
