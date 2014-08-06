/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __SCHEDULER_ALL_H__
#define __SCHEDULER_ALL_H__

#include "ocr-scheduler.h"
#include "ocr-utils.h"

typedef enum _schedulerType_t {
    schedulerHc_id,
    schedulerHcPlaced_id,
    schedulerHcComm_id,
    schedulerFsimXE_id,
    schedulerFsimCE_id,
    schedulerMax_id
} schedulerType_t;

const char * scheduler_types[] = {
    "HC",
    "HC_Placed",
    "COMM_HC",
    "XE",
    "CE",
    NULL
};

#include "scheduler/hc/hc-scheduler.h"
#include "scheduler/hc/hc-comm-scheduler.h"

inline ocrSchedulerFactory_t * newSchedulerFactory(schedulerType_t type, ocrParamList_t *perType) {
    switch(type) {
    case schedulerHc_id:
        return newOcrSchedulerFactoryHc(perType);
    case schedulerHcComm_id:
        return newOcrSchedulerFactoryHcComm(perType);
    case schedulerFsimXE_id:
    case schedulerFsimCE_id:
    case schedulerHcPlaced_id:
    default:
        ASSERT(0);
    }
    return NULL;
}

#endif /* __SCHEDULER_ALL_H__ */
