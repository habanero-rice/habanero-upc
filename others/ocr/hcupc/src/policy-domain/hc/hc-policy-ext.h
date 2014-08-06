/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __HC_POLICY_EXT_H__
#define __HC_POLICY_EXT_H__

#include "policy-domain/hc/hc-policy.h"


ocrPolicyDomain_t * newPolicyDomainHc(ocrPolicyDomainFactory_t * policy,
                                      u64 schedulerCount, u64 workerCount, u64 computeCount,
                                      u64 workpileCount, u64 allocatorCount, u64 memoryCount,
                                      ocrTaskFactory_t *taskFactory, ocrTaskTemplateFactory_t *taskTemplateFactory,
                                      ocrDataBlockFactory_t *dbFactory, ocrEventFactory_t *eventFactory,
                                      ocrPolicyCtxFactory_t *contextFactory, ocrGuidProvider_t *guidProvider,
                                      ocrLockFactory_t* lockFactory, ocrAtomic64Factory_t* atomicFactory,
                                      ocrCost_t *costFunction, ocrParamList_t *perInstance);

#endif /* __HC_POLICY_EXT_H__ */
