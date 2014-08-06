/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include <assert.h>

#include "policy-domain/hc/hc-dist-policy.h"
#include "policy-domain/hc/hc-policy-ext.h"

//
// !! Warning, nasty hardcoding !!
//
// - comm scheduler MUST be scheduler 0
//

#ifdef _COMM_WORKER_AS_MASTER
#define COMM_SCHEDULER_ID 0
#define COMP_SCHEDULER_ID 1
#else
#define COMM_SCHEDULER_ID 1
#define COMP_SCHEDULER_ID 0
#endif

static bool isHcCommEdt(ocrPolicyDomain_t * pd, ocrGuid_t edtGuid) {
    ocrTask_t * task;
    deguidify(pd, edtGuid, (u64*)&task, NULL);
    ocrTaskTemplate_t * taskTemplate;
    deguidify(pd, task->templateGuid, (u64*)&taskTemplate, NULL);
    return (taskTemplate->properties == EDT_PROP_COMM);
}

static u8 hcDistTakeEdt(ocrPolicyDomain_t *self, ocrCost_t *cost, u32 *count,
                ocrGuid_t *edts, ocrPolicyCtx_t *context) {
    //TODO does count is a request or a wish ?
    //TODO check the type of current worker, if comp-worker, dispatch to comp scheduler
    ocrWorker_t *worker = NULL;
    deguidify(self, getCurrentWorkerContext()->sourceObj, (u64*)&worker, NULL);

    ocrScheduler_t * targetScheduler = ((worker->type == COMM_WORKER) ? self->schedulers[COMM_SCHEDULER_ID] : self->schedulers[COMP_SCHEDULER_ID]);
    targetScheduler->fctPtrs->takeEdt(targetScheduler, cost, count, edts, context);
    // When takeEdt is successful, it means there either was
    // work in the current worker's workpile, or that the scheduler
    // did work-stealing across workpiles.
    return 0;
}


static u8 hcDistGiveEdt(ocrPolicyDomain_t *self, u32 count, ocrGuid_t *edts, ocrPolicyCtx_t *context) {
    assert(count == 1);
    // Dispatch EDT to the correct scheduler
    ocrScheduler_t * targetScheduler = (isHcCommEdt(self, edts[0]) ? 
                            self->schedulers[COMM_SCHEDULER_ID] : self->schedulers[COMP_SCHEDULER_ID]);
    targetScheduler->fctPtrs->giveEdt(targetScheduler, count, edts, context);
    return 0;
}

ocrPolicyDomain_t * newPolicyDomainHcDist(ocrPolicyDomainFactory_t * policy,
                                      u64 schedulerCount, u64 workerCount, u64 computeCount,
                                      u64 workpileCount, u64 allocatorCount, u64 memoryCount,
                                      ocrTaskFactory_t *taskFactory, ocrTaskTemplateFactory_t *taskTemplateFactory,
                                      ocrDataBlockFactory_t *dbFactory, ocrEventFactory_t *eventFactory,
                                      ocrPolicyCtxFactory_t *contextFactory, ocrGuidProvider_t *guidProvider,
                                      ocrLockFactory_t* lockFactory, ocrAtomic64Factory_t* atomicFactory,
                                      ocrCost_t *costFunction, ocrParamList_t *perInstance) {
    ocrPolicyDomain_t * pd = newPolicyDomainHc(policy, 
                                schedulerCount, workerCount, computeCount,
                                workpileCount, allocatorCount, memoryCount, taskFactory, 
                                taskTemplateFactory, dbFactory, eventFactory, contextFactory,
                                guidProvider, lockFactory, atomicFactory, costFunction, perInstance);
    pd->takeEdt = hcDistTakeEdt;
    pd->giveEdt = hcDistGiveEdt;
    return pd;
}

ocrPolicyDomainFactory_t * newPolicyDomainFactoryHcDist(ocrParamList_t *perType) {
    ocrPolicyDomainFactory_t* base = newPolicyDomainFactoryHc(perType);
    base->instantiate = newPolicyDomainHcDist;
    return base;
}
