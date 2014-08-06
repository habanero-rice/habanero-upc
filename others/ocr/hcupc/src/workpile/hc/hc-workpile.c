/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-macros.h"
#include "ocr-policy-domain-getter.h"
#include "ocr-policy-domain.h"
#include "ocr-workpile.h"
#include "workpile/hc/hc-workpile.h"


/******************************************************/
/* OCR-HC WorkPile                                    */
/******************************************************/

static void hcWorkpileDestruct ( ocrWorkpile_t * base ) {
    ocrWorkpileHc_t* derived = (ocrWorkpileHc_t*) base;
    dequeDestroy(derived->deque);
    free(derived);
}

static void hcWorkpileStart(ocrWorkpile_t *base, ocrPolicyDomain_t *PD) {
}

static void hcWorkpileStop(ocrWorkpile_t *base) {
}

static ocrGuid_t hcWorkpilePop ( ocrWorkpile_t * base, ocrCost_t *cost ) {
    ocrWorkpileHc_t* derived = (ocrWorkpileHc_t*) base;
    return (ocrGuid_t) dequePop(derived->deque);
}

static void hcWorkpilePush (ocrWorkpile_t * base, ocrGuid_t g ) {
    ocrWorkpileHc_t* derived = (ocrWorkpileHc_t*) base;
    dequePush(derived->deque, (void *)g);
}

static ocrGuid_t hcWorkpileSteal ( ocrWorkpile_t * base, ocrCost_t *cost ) {
    ocrWorkpileHc_t* derived = (ocrWorkpileHc_t*) base;
    return (ocrGuid_t) dequeSteal(derived->deque);
}

static ocrWorkpile_t * newWorkpileHc(ocrWorkpileFactory_t * factory, ocrParamList_t *perInstance) {
    ocrWorkpileHc_t* derived = (ocrWorkpileHc_t*) checkedMalloc(derived, sizeof(ocrWorkpileHc_t));
    ocrWorkpile_t * base = (ocrWorkpile_t *) derived;
    ocrMappable_t * module_base = (ocrMappable_t *) base;
    module_base->mapFct = NULL;
    base->fctPtrs = &(factory->workpileFcts);
    derived->deque = (deque_t *) checkedMalloc(derived->deque, sizeof(deque_t));
    dequeInit(derived->deque, (void *) NULL_GUID);
    return base;
}


/******************************************************/
/* OCR-HC WorkPile Factory                            */
/******************************************************/

static void destructWorkpileFactoryHc(ocrWorkpileFactory_t * factory) {
    free(factory);
}

ocrWorkpileFactory_t * newOcrWorkpileFactoryHc(ocrParamList_t *perType) {
    ocrWorkpileFactoryHc_t* derived = (ocrWorkpileFactoryHc_t*) checkedMalloc(derived, sizeof(ocrWorkpileFactoryHc_t));
    ocrWorkpileFactory_t* base = (ocrWorkpileFactory_t*) derived;
    base->instantiate = newWorkpileHc;
    base->destruct =  destructWorkpileFactoryHc;
    base->workpileFcts.destruct = hcWorkpileDestruct;
    base->workpileFcts.start = hcWorkpileStart;
    base->workpileFcts.stop = hcWorkpileStop;
    base->workpileFcts.pop = hcWorkpilePop;
    base->workpileFcts.push = hcWorkpilePush;
    base->workpileFcts.steal = hcWorkpileSteal;
    return base;
}


/******************************************************/
/* Semi Concurrent Deque                              */
/******************************************************/

static void semiConcDequeDestruct ( ocrWorkpile_t * base ) {
    ocrWorkpileSemiConcDeque_t* derived = (ocrWorkpileSemiConcDeque_t*) base;
    semiConcDequeDestroy(derived->deque);
    free(derived);
}

static void semiConcDequeStart(ocrWorkpile_t *base, ocrPolicyDomain_t *PD) {
}

static void semiConcDequeStop(ocrWorkpile_t *base) {
}

static ocrGuid_t semiConcDequePop ( ocrWorkpile_t * base, ocrCost_t *cost ) {
    ocrWorkpileSemiConcDeque_t * derived = (ocrWorkpileSemiConcDeque_t*) base;
    return (ocrGuid_t) semiConcDequeNonLockedPop(derived->deque);
}

static void semiConcDequePush (ocrWorkpile_t * base, ocrGuid_t g ) {
    ocrWorkpileSemiConcDeque_t* derived = (ocrWorkpileSemiConcDeque_t*) base;
    semiConcDequeLockedPush(derived->deque, (void *)g);
}

static ocrGuid_t semiConcDequeSteal ( ocrWorkpile_t * base, ocrCost_t *cost ) {
    ocrWorkpileSemiConcDeque_t* derived = (ocrWorkpileSemiConcDeque_t*) base;
    return (ocrGuid_t) semiConcDequeNonLockedPop(derived->deque);
}

static ocrWorkpile_t * newWorkpileSemiConcDeque(ocrWorkpileFactory_t * factory, ocrParamList_t *perInstance) {
    ocrWorkpileSemiConcDeque_t* derived = (ocrWorkpileSemiConcDeque_t*) checkedMalloc(derived, sizeof(ocrWorkpileSemiConcDeque_t));
    ocrWorkpile_t * base = (ocrWorkpile_t *) derived;
    ocrMappable_t * module_base = (ocrMappable_t *) base;
    module_base->mapFct = NULL;
    base->fctPtrs = &(factory->workpileFcts);
    derived->deque = (semiConcDeque_t *) checkedMalloc(derived->deque, sizeof(semiConcDeque_t));
    semiConcDequeInit(derived->deque, (void *) NULL_GUID);
    return base;
}

/******************************************************/
/* Semi Concurrent Deque Factory                      */
/******************************************************/

static void destructWorkpileFactorySemiConcDeque(ocrWorkpileFactory_t * factory) {
    free(factory);
}

ocrWorkpileFactory_t * newOcrWorkpileFactorySemiConcDeque(ocrParamList_t *perType) {
    ocrWorkpileFactorySemiConcDeque_t* derived = (ocrWorkpileFactorySemiConcDeque_t*) checkedMalloc(derived, sizeof(ocrWorkpileFactorySemiConcDeque_t));
    ocrWorkpileFactory_t* base = (ocrWorkpileFactory_t*) derived;
    base->instantiate = newWorkpileSemiConcDeque;
    base->destruct = destructWorkpileFactorySemiConcDeque;
    base->workpileFcts.destruct = semiConcDequeDestruct;
    base->workpileFcts.start = semiConcDequeStart;
    base->workpileFcts.stop = semiConcDequeStop;
    base->workpileFcts.pop = semiConcDequePop;
    base->workpileFcts.push = semiConcDequePush;
    base->workpileFcts.steal = semiConcDequeSteal;
    return base;
}
