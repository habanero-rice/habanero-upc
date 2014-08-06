/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#include "debug.h"
#include "ocr-comp-platform.h"
#include "ocr-runtime.h"
#include "ocr-types.h"
#include "ocr-worker.h"
#include "worker/hc/hc-worker.h"
#include "policy-domain/hc/hc-policy.h"

#include <pthread.h>
#include <stdio.h>

#include "worker/hc/hc-comm-worker.h"

//debug: Used for external timing
// extern volatile int START_COMMUNICATION;

void hcCommLoopWorker(ocrPolicyDomain_t * pd, ocrWorker_t * worker) {
    // Build and cache a take context
    ocrPolicyCtx_t * orgCtx = getCurrentWorkerContext();
    ocrPolicyCtx_t * ctx = orgCtx->clone(orgCtx, PD_MSG_EDT_TAKE);
    // ctx->type = PD_MSG_EDT_TAKE;
    // Entering the worker loop
    while(worker->fctPtrs->isRunning(worker)) {
        // while(!START_COMMUNICATION);

        while(worker->fctPtrs->isRunning(worker)) {
            ocrGuid_t taskGuid;
            u32 count;
            pd->takeEdt(pd, NULL, &count, &taskGuid, ctx);
            // remove this when we can take a bunch and make sure there's
            // an agreement whether it's the callee or the caller that
            // allocates the taskGuid array
            ASSERT(count <= 1);
            if (count != 0) {
                ocrTask_t* task = NULL;
                deguidify(pd, taskGuid, (u64*)&(task), NULL);
                worker->fctPtrs->execute(worker, task, taskGuid, NULL_GUID);
                task->fctPtrs->destruct(task);
            }
        }
    }
    ctx->destruct(ctx);
}

ocrWorkerFactory_t * newOcrWorkerFactoryHcComm(ocrParamList_t * perType) {
    ocrWorkerFactory_t * derived = newOcrWorkerFactoryHc(perType);
    ocrWorkerFactory_t* base = (ocrWorkerFactory_t*) derived;
    base->workerFcts.loop = hcCommLoopWorker;
    return base;
}

