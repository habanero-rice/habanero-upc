/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __HC_WORKER_H__
#define __HC_WORKER_H__

#include "ocr-types.h"
#include "ocr-utils.h"
#include "ocr-worker.h"

typedef struct {
    ocrWorkerFactory_t base;
} ocrWorkerFactoryHc_t;

typedef struct _paramListWorkerHcInst_t {
    paramListWorkerInst_t base;
    u32 workerId;
    ocrWorkerType_t type;
} paramListWorkerHcInst_t;

typedef struct {
    ocrWorker_t worker;
    // The HC implementation relies on integer ids to
    // map workers, schedulers and workpiles together
    u32 id;
    // Flag the worker checksto now if he's running
    bool run;
    // reference to the EDT this worker is currently executing
    ocrGuid_t currentEDTGuid;
} ocrWorkerHc_t;

ocrWorkerFactory_t* newOcrWorkerFactoryHc(ocrParamList_t *perType);

#endif /* __HC_WORKER_H__ */
