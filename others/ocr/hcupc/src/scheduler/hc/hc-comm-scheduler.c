/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include <assert.h>
#include "debug.h"
#include "ocr-macros.h"
#include "ocr-policy-domain-getter.h"
#include "ocr-policy-domain.h"
#include "scheduler/hc/hc-comm-scheduler.h"

static u8 hcCommSchedulerTake (ocrScheduler_t *self, struct _ocrCost_t *cost, u32 *count,
                            ocrGuid_t *edts,
                            ocrPolicyCtx_t *context) {
     // Only the comm-worker can take from this scheduler
#ifdef _COMM_WORKER_AS_MASTER
     assert(context->sourceId == 0); // the first is communication worker
#else
     assert(context->sourceId == context->PD->workerCount-1); // the last is communication worker
#endif
     // Try to pop, if no work, return.
#ifdef _COMM_WORKER_AS_MASTER
     ocrWorkpile_t * wp_to_pop = self->workpiles[0]; // the first one is the OUT queue (SEMI_CONC_DEQUE)
#else
     ocrWorkpile_t * wp_to_pop = self->workpiles[1]; // the last one is the OUT queue (SEMI_CONC_DEQUE)
#endif
     ocrGuid_t popped = wp_to_pop->fctPtrs->pop(wp_to_pop, cost);
     if ( NULL_GUID == popped ) {
         *count = 0;
     } else {
         // In this implementation we expect the caller to have
         // allocated memory for us since we can return at most one
         // guid (most likely store using the address of a local)
         *count = 1;
         *edts = popped;
     }
     return 0;
}

static u8 hcCommSchedulerGive (ocrScheduler_t* self, u32 count, ocrGuid_t* edts, ocrPolicyCtx_t * context ) {
	// Only the HC (not COMM_HC) workers can put ASYNC_COMM asyncs. This will always
#ifdef _COMM_WORKER_AS_MASTER
	// go into the SEMI_CONC_DEQUE, which is the first workpile
	ocrWorkpile_t * wp_to_push = self->workpiles[0];
#else
	// go into the SEMI_CONC_DEQUE, which is the last workpile
	ocrWorkpile_t * wp_to_push = self->workpiles[1];
#endif
     u32 i = 0;
     for ( ; i < count; ++i ) {
         wp_to_push->fctPtrs->push(wp_to_push,edts[i]);
     }
     return 0;
}

ocrSchedulerFactory_t * newOcrSchedulerFactoryHcComm(ocrParamList_t *perType) {
    ocrSchedulerFactory_t* hcCommScheduler = newOcrSchedulerFactoryHc(perType);
    hcCommScheduler->schedulerFcts.takeEdt = hcCommSchedulerTake;
    // Only comp-worker can give new task.
    // Nothing prevent a comm-worker from executing a comm-task that creates a new
    // edt but it's forbidden for now.
    hcCommScheduler->schedulerFcts.giveEdt = hcCommSchedulerGive;
    return hcCommScheduler;
}
