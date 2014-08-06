/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef __OCR_TASK_H__
#define __OCR_TASK_H__

#include "ocr-edt.h"
#include "ocr-sync.h"
#include "ocr-types.h"
#include "ocr-utils.h"
#include "ocr-mappable.h"

#ifdef OCR_ENABLE_STATISTICS
#include "ocr-statistics.h"
#endif

struct _ocrTask_t;
struct _ocrTaskFcts_t;
struct _ocrTaskFactory_t;

/****************************************************/
/* PARAMETER LISTS                                  */
/****************************************************/
typedef struct _paramListTaskFact_t {
    ocrParamList_t base;
} paramListTaskFact_t;

typedef struct _paramListTaskTemplateFact_t {
    ocrParamList_t base;
} paramListTaskTemplateFact_t;

struct _ocrTaskTemplate_t;

/****************************************************/
/* OCR TASK TEMPLATE                                */
/****************************************************/

/*! \brief Abstract class to represent OCR task template functions
 *
 *  This class provides the interface to call operations on task
 *  templates
 */
typedef struct ocrTaskTemplateFcts_t {
    /*! \brief Virtual destructor for the Task interface
     */
    void (*destruct) (struct _ocrTaskTemplate_t* self);
} ocrTaskTemplateFcts_t;

/*! \brief Abstract class to represent OCR task templates.
 *
 */
typedef struct _ocrTaskTemplate_t {
    ocrGuid_t guid; /**< GUID for this task template */
#ifdef OCR_ENABLE_STATISTICS
    ocrStatsProcess_t statProcess;
#endif
    u32 paramc;
    u32 depc;
    u16 properties;
    ocrEdt_t executePtr;
    ocrTaskTemplateFcts_t *fctPtrs;
} ocrTaskTemplate_t;

/****************************************************/
/* OCR TASK TEMPLATE FACTORY                        */
/****************************************************/

typedef struct _ocrTaskTemplateFactory_t {
    ocrTaskTemplate_t* (*instantiate)(struct _ocrTaskTemplateFactory_t * factory, ocrEdt_t fctPtr,
                                      u32 paramc, u32 depc, u16 properties, ocrParamList_t *perInstance);

    /*! \brief Virtual destructor for the TaskTemplateFactory interface
     */
    void (*destruct)(struct _ocrTaskTemplateFactory_t * factory);

    ocrTaskTemplateFcts_t taskTemplateFcts;
} ocrTaskTemplateFactory_t;

/****************************************************/
/* OCR TASK                                         */
/****************************************************/

struct _ocrTask_t;

/*! \brief Abstract class to represent OCR tasks function pointers
 *
 *  This class provides the interface to call operations on task
 */
typedef struct _ocrTaskFcts_t {
    /*! \brief Virtual destructor for the Task interface
     */
    void (*destruct) (struct _ocrTask_t* self);
    /*! \brief Interface to execute the underlying computation of a task
     */
    void (*execute) (struct _ocrTask_t* self);
    /*! \brief Interface to schedule the underlying computation of a task
     */
    void (*schedule) (struct _ocrTask_t* self);
} ocrTaskFcts_t;

// ELS runtime size is one to support finish-edt
// ELS_USER_SIZE is defined by configure
#define ELS_RUNTIME_SIZE 1
#define ELS_SIZE (ELS_RUNTIME_SIZE + ELS_USER_SIZE)

/*! \brief Abstract class to represent OCR tasks.
 *
 *  This class provides the interface for the underlying implementation to conform.
 *  OCR tasks can be executed and can have their synchronization frontier furthered by Events.
 */
typedef struct _ocrTask_t {
    ocrGuid_t guid; /**< GUID for this task (EDT) */
#ifdef OCR_ENABLE_STATISTICS
    ocrStatsProcess_t statProcess;
#endif
    ocrGuid_t templateGuid; /**< GUID for the template of this task */
    u32 paramc;
    u64* paramv;
    u64 depc;
    // depv and the associated bookeeping are implementation specific
    ocrGuid_t outputEvent; // Event to notify when the EDT is done
    ocrGuid_t els[ELS_SIZE];
    struct _ocrTaskFcts_t * fctPtrs;
    ocrAtomic64_t* addedDepCounter;
} ocrTask_t;

/****************************************************/
/* OCR TASK FACTORY                                 */
/****************************************************/

/*! \brief Abstract factory class to create OCR tasks.
 *
 *  This class provides an interface to create Task instances with a non-static create function
 *  to allow runtime implementers to choose to have state in their derived TaskFactory classes.
 */
typedef struct _ocrTaskFactory_t {
    ocrMappable_t module;
    /*! \brief Instantiates a Task and returns its corresponding GUID
     *  \param[in]  routine A user defined function that represents the computation this Task encapsulates.
     *  \param[in]  worker_id   The Worker instance creating this Task instance
     *  \return GUID of the concrete Task that is created by this call
     *
     *  The signature of the interface restricts the user computation that can be assigned to a task as follows.
     *  The user defined computation should take a vector of GUIDs and its size as their inputs, which may be
     *  the GUIDs used to satisfy the Events enlisted in the dependence list.
     *
     */
    ocrTask_t* (*instantiate)(struct _ocrTaskFactory_t * factory, ocrTaskTemplate_t * edtTemplate,
                              u32 paramc, u64* paramv, u32 depc, u16 properties,
                              ocrGuid_t affinity, ocrGuid_t *outputEvent, ocrParamList_t *perInstance);

    /*! \brief Virtual destructor for the TaskFactory interface
     */
    void (*destruct)(struct _ocrTaskFactory_t * factory);

    ocrTaskFcts_t taskFcts;
} ocrTaskFactory_t;
#endif /* __OCR_TASK_H__ */

