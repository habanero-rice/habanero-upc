/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef OCR_POLICY_DOMAIN_H_
#define OCR_POLICY_DOMAIN_H_

#include "ocr-allocator.h"
#include "ocr-comp-target.h"
#include "ocr-datablock.h"
#include "ocr-event.h"
#include "ocr-guid.h"
#include "ocr-mappable.h"
#include "ocr-mem-target.h"
#include "ocr-scheduler.h"
#include "ocr-sync.h"
#include "ocr-task.h"
#include "ocr-tuning.h"
#include "ocr-types.h"
#include "ocr-worker.h"
#include "ocr-workpile.h"

/****************************************************/
/* PARAMETER LISTS                                  */
/****************************************************/

typedef struct _paramListPolicyDomainFact_t {
    ocrParamList_t base;
} paramListPolicyDomainFact_t;

typedef struct _paramListPolicyDomainInst_t {
    ocrParamList_t base;
} paramListPolicyDomainInst_t;


/******************************************************/
/* OCR POLICY DOMAIN INTERFACE                        */
/******************************************************/

typedef enum {
    PD_MSG_INVAL       = 0, /**< Invalid message */
    PD_MSG_EDT_READY   = 1, /**< An EDT is now fully "satisfied" */
    PD_MSG_EDT_SATISFY = 2, /**< Partial EDT satisfaction */
    PD_MSG_DB_DESTROY  = 3, /**< A DB destruction has been requested */
    PD_MSG_EDT_TAKE    = 4, /**< Take EDTs (children of the PD make this call) */
    PD_MSG_DB_TAKE     = 5, /**< Take DBs (children of the PD make this call) */
    PD_MSG_EDT_STEAL   = 6, /**< Steal EDTs (from non-children PD) */
    PD_MSG_DB_STEAL    = 7, /**< Steal DBs (from non-children PD) */
    PD_MSG_GUID_REL    = 8, /**< Release a GUID */
    PD_MSG_EDT_GIVE    = 9, 
    PD_MSG_TAKE_MY_WORK=10, 
    PD_MSG_GIVE_ME_WORK=11, 
    PD_MSG_PICKUP_EDT  =12, 
    PD_MSG_MSG_TAKE    =13, 
    PD_MSG_MSG_GIVE    =14, 
    PD_MSG_INJECT_EDT  =15, 
} ocrPolicyMsgType_t;


struct _ocrPolicyDomain_t;

/**
 * @brief Structure describing a "message" that is used to communicate between
 * policy domains in an asynchronous manner
 *
 * Policy domains can communicate between each other using either
 * a synchronous method where the requester (source) policy domain executes
 * the code of the requestee (destination) policy domain directly or
 * asynchronously where the source domain sends the equivalent of a message
 * to the destination domain and then `waits' for a response. The first
 * way requires synchronization and homogeneous runtime cores and is the
 * most natural model on x86 while the second model is more natural in a
 * slave-master framework where the slaves are not allowed to execute
 * code written for the master.
 *
 * This struct is meant to be extended (either one per implementation
 * or optionally one per implementation and type).
 *
 * Note that this structure is also used for synchronous messages
 * so that the caller knows who finally responded to its request.
 */
typedef struct _ocrPolicyCtx_t {
    ocrGuid_t sourcePD;      /**< Source policy domain */
    struct _ocrPolicyDomain_t * PD; /**< current policy domain */
    ocrGuid_t sourceObj;     /**< Source object (worker for example) in the source PD */
    u64       sourceId;      /**< Internal ID to the source PD */
    ocrGuid_t destPD;        /**< Destination policy domain (after all eventual hops) */
    ocrGuid_t destObj;       /**< Responding object (after all eventual hops) */
    ocrPolicyMsgType_t type; /**< Type of message */
    struct _ocrPolicyCtx_t * (*clone)(struct _ocrPolicyCtx_t *self, ocrPolicyMsgType_t type);
    void (*destruct)(struct _ocrPolicyCtx_t *self);
} ocrPolicyCtx_t;


typedef struct _ocrPolicyCtxFactory_t {
    ocrPolicyCtx_t * (*instantiate)(struct _ocrPolicyCtxFactory_t *factory, ocrParamList_t *perInstance);
    void (*destruct)(struct _ocrPolicyCtxFactory_t *self);
} ocrPolicyCtxFactory_t;

/**
 * @brief A policy domain is OCR's way of dividing up the runtime in scalable
 * chunks
 *
 * Each policy domain contains the following:
 *     - 0 or more 'schedulers' which both schedule tasks and place data
 *     - 0 or more 'workers' on which EDTs are executed
 *     - 0 or more 'target computes' on which 'workers' are mapped and run
 *     - 0 or more 'workpiles' from which EDTs are taken to be scheduled
 *       on the workers. Note that the exact content of workpiles is
 *       left up to the scheduler managing them
 *     - 0 or more 'allocators' from which DBs are allocated/freed
 *     - 0 or more 'target memories' which are managed by the
 *       allocators
 *
 * A policy domain will directly respond to requests emanating from user
 * code to:
 *     - create EDTs (using ocrEdtCreate())
 *     - create DBs (using ocrDbCreate())
 *     - create GUIDs
 *
 * It will also respond to requests from the runtime to:
 *     - destroy and reallocate a data-block (callback stemming from user
 *       actions on data-blocks using ocrDbDestroy())
 *     - destroy an EDT (callback stemming from user actions on EDTs
 *       using ocrEdtDestroy())
 *     - partial or complete satisfaction of dependences (callback
 *       stemming from user actions on EDTs using ocrEventSatisfy())
 *     - take EDTs stemming from idling workers for example
 *
 * Finally, it will respond to requests from other policy domains to:
 *     - take (steal) EDTs and DBs (data load-balancing)
 */
typedef struct _ocrPolicyDomain_t {
    ocrMappable_t module;
    ocrGuid_t guid;                             /**< GUID for this policy */

    u64 schedulerCount;                         /**< Number of schedulers */
    u64 workerCount;                            /**< Number of workers */
    u64 computeCount;                           /**< Number of target computate nodes */
    u64 workpileCount;                          /**< Number of workpiles */
    u64 allocatorCount;                         /**< Number of allocators */
    u64 memoryCount;                            /**< Number of target memory nodes */

    ocrScheduler_t  ** schedulers;              /**< All the schedulers */
    ocrWorker_t     ** workers;                 /**< All the workers */
    ocrCompTarget_t ** computes;                /**< All the target compute nodes */
    ocrWorkpile_t   ** workpiles;               /**< All the workpiles */
    ocrAllocator_t  ** allocators;              /**< All the allocators */
    ocrMemTarget_t  ** memories;                /**< All the target memory nodes */

    ocrTaskFactory_t  * taskFactory;            /**< Factory to produce tasks (EDTs) in this policy domain */
    ocrTaskTemplateFactory_t  * taskTemplateFactory; /**< Factory to produce task templates in this policy domain */
    ocrDataBlockFactory_t * dbFactory;          /**< Factory to produce data-blocks in this policy domain */
    ocrEventFactory_t * eventFactory;           /**< Factory to produce events in this policy domain */

    ocrPolicyCtxFactory_t * contextFactory;     /**< Factory to produce the contexts (used for communicating
                                                 * between policy domains) */

    ocrGuidProvider_t *guidProvider;            /**< GUID generator for this policy domain */

    ocrLockFactory_t *lockFactory;              /**< Factory for locks */
    ocrAtomic64Factory_t *atomicFactory;        /**< Factory for atomics */

    ocrCost_t *costFunction;                    /**< Cost function used to determine
                                                 * what to schedule/steal/take/etc.
                                                 * Currently a placeholder for future
                                                 * objective driven scheduling */

    /**
     * @brief Destroys (and frees any associated memory) this
     * policy domain
     *
     * Call when the policy domain has stopped executing to free
     * any remaining memory.
     *
     * @param self                This policy domain
     */
    void (*destruct)(struct _ocrPolicyDomain_t *self);

    /**
     * @brief Starts this policy domain
     *
     * This starts the portion of OCR that manages the resources contained
     * in this policy domain.
     *
     * @param self                This policy domain
     */
    void (*start)(struct _ocrPolicyDomain_t *self);

    /**
     * @brief Stops this policy domain
     *
     * This stops the portion of OCR that manages the resources
     * contained in this policy domain. The policy domain will also stop
     * responding to requests from other policy domains.
     *
     * @param self                This policy domain
     */
    void (*stop)(struct _ocrPolicyDomain_t *self);

    /**
     * @brief Finish the execution of the policy domain
     *
     * Ask the policy domain to wrap up currently executing
     * task and shutdown workers. This is typically something
     * ocrShutdown would call.
     *
     * @param self                This policy domain
     */
    void (*finish)(struct _ocrPolicyDomain_t *self);

    /**
     * @brief Request the allocation of memory (a data-block)
     *
     * This call will be triggered by user code when a data-block
     * needs to be allocated
     *
     * @param self              This policy domain
     * @param guid              Contains the DB GUID on return for synchronous
     *                          calls
     * @param ptr               Contains the address for accessing this DB on
     *                          return for synchronous calls
     * @param size              Size of the DB requested
     * @param hint              Hint concerning where to allocate
     * @param context           Context for this call. This will be updated
     *                          as the call progresses (with the destination
     *                          information for example)
     *
     * If this policy domain implements synchronous calls, this call will only
     * return once the entire call has been serviced. In the asynchronous case,
     * this call will return a value indicating asynchronous processing and the
     * policy domain will be notified when the call returns.
     *
     * @return:
     *     - 0 if the call completed successfully synchronously
     *     - 255 if the call is being processed asynchronously
     *     - TODO
     */
    u8 (*allocateDb)(struct _ocrPolicyDomain_t *self, ocrGuid_t *guid,
                     void** ptr, u64 size, u16 properties,
                     ocrGuid_t affinity, ocrInDbAllocator_t allocator,
                     ocrPolicyCtx_t *context);

    /**
     * @brief Request the creation of a task metadata (EDT)
     *
     * This call will be triggered by user code when an EDT
     * is created
     *
     * @todo Improve description to be more like allocateDb
     *
     * @todo Add something about templates here and potentially
     * known dependences so that it can be optimally placed
     */
    u8 (*createEdt)(struct _ocrPolicyDomain_t *self, ocrGuid_t *guid,
                    ocrTaskTemplate_t * edtTemplate, u32 paramc, u64* paramv,
                    u32 depc, u16 properties, ocrGuid_t affinity,
                    ocrGuid_t * outputEvent, ocrPolicyCtx_t *context);

    /**
     * @brief Request the creation of a task-template metadata
     */
    u8 (*createEdtTemplate)(struct _ocrPolicyDomain_t *self, ocrGuid_t *guid,
                           ocrEdt_t funcPtr, u32 paramc, u32 depc, u16 properties, ocrPolicyCtx_t *context);

    /**
     * @brief Request the creation of an event
     */
    u8 (*createEvent)(struct _ocrPolicyDomain_t *self, ocrGuid_t *guid,
                      ocrEventTypes_t type, bool takesArg, ocrPolicyCtx_t *context);
    /**
     * @brief Inform the policy domain of an event that does not require any
     * further processing
     *
     * These events are informational to the policy domain (runtime) and include
     * things like dependence satisfaction, destruction of a DB, etc. The context
     * will encode the type of event and any additional information
     *
     * This call will return immediately (whether in asynchronous or
     * synchronous mode)
     */
    void (*inform)(struct _ocrPolicyDomain_t *self, ocrGuid_t obj,
                   const ocrPolicyCtx_t *context);

    /**
     * @brief Gets a GUID and associates the value 'val' with
     * it
     *
     * This is used by the runtime to obtain new GUIDs
     *
     * @todo Write description, behaves as other functions
     */
    u8 (*getGuid)(struct _ocrPolicyDomain_t *self, ocrGuid_t *guid, u64 val,
                  ocrGuidKind type, ocrPolicyCtx_t *context);

    u8 (*getInfoForGuid)(struct _ocrPolicyDomain_t *self, ocrGuid_t guid, u64* val,
                         ocrGuidKind* type, ocrPolicyCtx_t *context);

    /**
     * @brief Take one or more EDTs to execute
     *
     * This call is called by either workers within this PD that need work or by
     * other PDs that are trying to "steal" work from here
     *
     * On a synchronous return, the number of EDTs taken and a pointer
     * to an array of GUIDs for them will be returned. On an asynchronous return,
     * count will be 0 and the GUIDs pointer will be invalid; the information
     * will have to come from the context
     *
     * @param self        This policy domain
     * @param cost      An optional cost function provided by the taker
     * @param count     On return contains the number of EDTs taken (synchronous calls)
     *                  Warning: no assumption here whether memory is allocated in caller or callee
     * @param edts      On return contains the EDTs taken (synchronous calls)
     *                  Warning: no assumption here whether memory is allocated in caller or callee
     * @param context   Context for this call
     *
     * @return:
     *     - 0 if the call completed successfully synchronously
     *     - 255 if the call is being processed asynchronously
     *     - TODO
     */
    u8 (*takeEdt)(struct _ocrPolicyDomain_t *self, ocrCost_t *cost, u32 *count,
                  ocrGuid_t *edts, ocrPolicyCtx_t *context);

    /**
     * @brief Same as takeEdt but for data-blocks
     *
     * @todo Add full description
     */
    u8 (*takeDb)(struct _ocrPolicyDomain_t *self, ocrCost_t *cost, u32 *count,
                 ocrGuid_t *dbs, ocrPolicyCtx_t *context);

    /**
     * @brief A function called by other policy domains to hand-off
     * EDTs; this is in essence, the opposite of takeEdt
     *
     * @todo Give more detail
     * @todo Is this needed?
     */
    u8 (*giveEdt)(struct _ocrPolicyDomain_t *self, u32 count, ocrGuid_t *edts,
                  ocrPolicyCtx_t *context);

    /**
     * @brief Same as giveEdt() but for data-blocks
     */
    u8 (*giveDb)(struct _ocrPolicyDomain_t *self, u32 count, ocrGuid_t *dbs,
                 ocrPolicyCtx_t *context);

    /**
     * @brief Inform the policy domain a worker is waiting for an event to complete
     */
    u8 (*waitForEvent) (struct _ocrPolicyDomain_t *self, ocrGuid_t workerGuid,
                       ocrGuid_t yieldingEdtGuid, ocrGuid_t eventToYieldForGuid,
                       ocrGuid_t * returnGuid, ocrPolicyCtx_t *context);

    /**
     * @brief Called for asynchronous calls when the call has been
     * processed.
     *
     * @param self        This policy domain
     * @param context   Context for the call (indicating what it is a response to)
     */
    void (*processResponse)(struct _ocrPolicyDomain_t *self, ocrPolicyCtx_t *context);

    /**
     * @brief Gets a lock to use
     */
    ocrLock_t* (*getLock)(struct _ocrPolicyDomain_t *self, ocrPolicyCtx_t *context);

    /**
     * @brief Gets an atomic to use
     */
    ocrAtomic64_t* (*getAtomic64)(struct _ocrPolicyDomain_t *self, ocrPolicyCtx_t *context);

    /**
     * @brief Gets a context for this policy domain
     *
     * This returns a new instance of a context. Another method of getting a
     * context is to call getCurrentWorkerContext() which will return
     * a copy of the cached worker's context. This function returns
     * an empty context whereas the getCurrentWorkerContext() function
     * returns a pre-filled out one (with the PD and source object
     * information filled out)
     *
     * @param self          This policy domain
     */
    ocrPolicyCtx_t* (*getContext)(struct _ocrPolicyDomain_t *self);

    struct _ocrPolicyDomain_t** neighbors;
    u64 neighborCount;

} ocrPolicyDomain_t;

/****************************************************/
/* OCR POLICY DOMAIN FACTORY                        */
/****************************************************/

typedef struct _ocrPolicyDomainFactory_t {
    ocrMappable_t module;
    /**
     * @brief Create a policy domain
     *
     * Allocates the required space for the policy domain
     * based on the counts passed as arguments. The 'schedulers',
     * 'workers', 'computes', 'workpiles', 'allocators' and 'memories'
     * data-structures must then be properly filled
     *
     * @param factory             This policy domain factory
     * @param configuration       An optional configuration
     * @param schedulerCount      The number of schedulers
     * @param workerCount         The number of workers
     * @param computeCount        The number of compute targets
     * @param workpileCount       The number of workpiles
     * @param allocatorCount      The number of allocators
     * @param memoryCount         The number of memory targets
     * @param taskFactory         The factory to use to generate EDTs
     * @param taskTemplateFactory The factory to use to generate EDTs templates
     * @param dbFactory           The factory to use to generate DBs
     * @param eventFactory        The factory to use to generate events
     * @param contextFactory      The factory to use to generate context
     * @param guidProvider        The provider of GUIDs for this policy domain
     * @param costFunction        The cost function used by this policy domain
     */

    ocrPolicyDomain_t * (*instantiate) (struct _ocrPolicyDomainFactory_t *factory,
                                        u64 schedulerCount, u64 workerCount, u64 computeCount,
                                        u64 workpileCount, u64 allocatorCount, u64 memoryCount,
                                        ocrTaskFactory_t *taskFactory,
                                        ocrTaskTemplateFactory_t *taskTemplateFactory, ocrDataBlockFactory_t *dbFactory,
                                        ocrEventFactory_t *eventFactory, ocrPolicyCtxFactory_t *contextFactory,
                                        ocrGuidProvider_t *guidProvider, ocrLockFactory_t *lockFactory,
                                        ocrAtomic64Factory_t *atomicFactory,
                                        ocrCost_t *costFunction, ocrParamList_t *perInstance);

    void (*destruct)(struct _ocrPolicyDomainFactory_t * factory);
} ocrPolicyDomainFactory_t;

#define __GUID_END_MARKER__
#include "ocr-guid-end.h"
#undef __GUID_END_MARKER__

#endif /* OCR_POLICY_DOMAIN_H_ */
