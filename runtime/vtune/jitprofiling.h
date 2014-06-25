/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __JITPROFILING_H__
#define __JITPROFILING_H__

/**
 * @brief JIT Profiling APIs
 *
 * The JIT Profiling API is used to report information about just-in-time
 * generated code that can be used by performance tools. The user inserts
 * calls in the code generator to report information before JIT-compiled
 * code goes to execution. This information is collected at runtime and used
 * by tools like Intel(R) VTune(TM) Amplifier to display performance metrics
 * associated with JIT-compiled code.
 *
 * These APIs can be used to\n
 * - **Profile trace-based and method-based JIT-compiled
 * code**. Some examples of environments that you can profile with these APIs:
 * dynamic JIT compilation of JavaScript code traces, JIT execution in OpenCL(TM)
 * software technology, Java/.NET managed execution environments, and custom
 * ISV JIT engines.
 * @code
 * #include <jitprofiling.h>
 *
 * if (iJIT_IsProfilingActive != iJIT_SAMPLING_ON) {
 *     return;
 * }
 *
 * iJIT_Method_Load jmethod = {0};
 * jmethod.method_id = iJIT_GetNewMethodID();
 * jmethod.method_name = "method_name";
 * jmethod.class_file_name = "class_name";
 * jmethod.source_file_name = "source_file_name";
 * jmethod.method_load_address = code_addr;
 * jmethod.method_size = code_size;
 *
 * iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED, (void*)&jmethod);
 * iJIT_NotifyEvent(iJVM_EVENT_TYPE_SHUTDOWN, NULL);
 * @endcode
 *
 *  * Expected behavior:
 *    * If any iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED event overwrites an
 *      already reported method, then such a method becomes invalid and its
 *      memory region is treated as unloaded. VTune Amplifier displays the metrics
 *      collected by the method until it is overwritten.
 *    * If supplied line number information contains multiple source lines for
 *      the same assembly instruction (code location), then VTune Amplifier picks up
 *      the first line number.
 *    * Dynamically generated code can be associated with a module name.
 *      Use the iJIT_Method_Load_V2 structure.\n
 *      Clarification of some cases:
 *        * If you register a function with the same method ID multiple times,
 *          specifying different module names, then the VTune Amplifier picks up
 *          the module name registered first. If you want to distinguish the same
 *          function between different JIT engines, supply different method IDs for
 *          each function. Other symbolic information (for example, source file)
 *          can be identical.
 *
 * - **Analyze split functions** (multiple joint or disjoint code regions
 * belonging to the same function) **including re-JIT**
 * with potential overlapping of code regions in time, which is common in
 * resource-limited environments.
 * @code
 * #include <jitprofiling.h>
 *
 * unsigned int method_id = iJIT_GetNewMethodID();
 *
 * iJIT_Method_Load a = {0};
 * a.method_id = method_id;
 * a.method_load_address = 0x100;
 * a.method_size = 0x20;
 *
 * iJIT_Method_Load b = {0};
 * b.method_id = method_id;
 * b.method_load_address = 0x200;
 * b.method_size = 0x30;
 *
 * iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED, (void*)&a);
 * iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED, (void*)&b);
 * @endcode
 *
 *  * Expected behaviour:
 *      * If a iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED event overwrites an
 *        already reported method, then such a method becomes invalid and
 *        its memory region is treated as unloaded.
 *      * All code regions reported with the same method ID are considered as
 *        belonging to the same method. Symbolic information (method name,
 *        source file name) will be taken from the first notification, and all
 *        subsequent notifications with the same method ID will be processed
 *        only for line number table information. So, the VTune Amplifier will map
 *        samples to a source line using the line number table from the current
 *        notification while taking the source file name from the very first one.\n
 *        Clarification of some cases:\n
 *          * If you register a second code region with a different source file
 *          name and the same method ID, then this information will be saved and
 *          will not be considered as an extension of the first code region, but
 *          VTune Amplifier will use the source file of the first code region and map
 *          performance metrics incorrectly.
 *          * If you register a second code region with the same source file as
 *          for the first region and the same method ID, then the source file will be
 *          discarded but VTune Amplifier will map metrics to the source file correctly.
 *          * If you register a second code region with a null source file and
 *          the same method ID, then provided line number info will be associated
 *          with the source file of the first code region.
 *
 * - **Explore inline functions** including multi-level hierarchy of
 * nested inline methods which shows how performance metrics are distributed through them.
 * @code
 * #include <jitprofiling.h>
 *
 *  //                                    method_id   parent_id
 *  //   [-- c --]                          3000        2000
 *  //                  [---- d -----]      2001        1000
 *  //  [---- b ----]                       2000        1000
 *  // [------------ a ----------------]    1000         n/a
 *
 * iJIT_Method_Load a = {0};
 * a.method_id = 1000;
 *
 * iJIT_Method_Inline_Load b = {0};
 * b.method_id = 2000;
 * b.parent_method_id = 1000;
 *
 * iJIT_Method_Inline_Load c = {0};
 * c.method_id = 3000;
 * c.parent_method_id = 2000;
 *
 * iJIT_Method_Inline_Load d = {0};
 * d.method_id = 2001;
 * d.parent_method_id = 1000;
 *
 * iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED, (void*)&a);
 * iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_INLINE_LOAD_FINISHED, (void*)&b);
 * iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_INLINE_LOAD_FINISHED, (void*)&c);
 * iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_INLINE_LOAD_FINISHED, (void*)&d);
 * @endcode
 *
 *  * Requirements:
 *      * Each inline (iJIT_Method_Inline_Load) method should be associated
 *        with two method IDs: one for itself; one for its immediate parent.
 *      * Address regions of inline methods of the same parent method cannot
 *        overlap each other.
 *      * Execution of the parent method must not be started until it and all
 *        its inline methods are reported.
 *  * Expected behaviour:
 *      * In case of nested inline methods an order of
 *        iJVM_EVENT_TYPE_METHOD_INLINE_LOAD_FINISHED events is not important.
 *      * If any event overwrites either inline method or top parent method,
 *        then the parent, including inline methods, becomes invalid and its memory
 *        region is treated as unloaded.
 *
 * **Life time of allocated data**\n
 * The client sends an event notification to the agent with event-specific
 * data, which is a structure. The pointers in the structure refer to memory
 * allocated by the client, which responsible for releasing it. The pointers are
 * used by the iJIT_NotifyEvent method to copy client's data in a trace file,
 * and they are not used after the iJIT_NotifyEvent method returns.
 *
 */

/**
 * @defgroup jitapi JIT Profiling
 * @ingroup internal
 * @{
 */

/**
 * @brief Enumerator for the types of notifications
 */
typedef enum iJIT_jvm_event
{
    iJVM_EVENT_TYPE_SHUTDOWN = 2,               /**<\brief Send this to shutdown the agent.
                                                 * Use NULL for event data. */

    iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED = 13,  /**<\brief Send when dynamic code is
                                                 * JIT compiled and loaded into
                                                 * memory by the JIT engine, but
                                                 * before the code is executed.
                                                 * Use iJIT_Method_Load as event
                                                 * data. */
/** @cond exclude_from_documentation */
    iJVM_EVENT_TYPE_METHOD_UNLOAD_START,    /**<\brief Send when compiled dynamic
                                             * code is being unloaded from memory.
                                             * Use iJIT_Method_Load as event data.*/
/** @endcond */

    iJVM_EVENT_TYPE_METHOD_UPDATE,   /**<\brief Send to provide new content for
                                      * a previously reported dynamic code.
                                      * The previous content will be invalidated
                                      * starting from the time of the notification.
                                      * Use iJIT_Method_Load as event data but
                                      * required fields are following:
                                      * - method_id    identify the code to update.
                                      * - method_load_address    specify start address
                                      *                          within identified code range
                                      *                          where update should be started.
                                      * - method_size            specify length of updated code
                                      *                          range. */


    iJVM_EVENT_TYPE_METHOD_INLINE_LOAD_FINISHED, /**<\brief Send when an inline dynamic
                                                  * code is JIT compiled and loaded
                                                  * into memory by the JIT engine,
                                                  * but before the parent code region
                                                  * starts executing.
                                                  * Use iJIT_Method_Inline_Load as event data.*/

/** @cond exclude_from_documentation */
    /* Legacy stuff. Do not use it. */
    iJVM_EVENT_TYPE_ENTER_NIDS = 19,
    iJVM_EVENT_TYPE_LEAVE_NIDS,
/** @endcond */

    iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED_V2  /**<\brief Send when a dynamic code is
                                              * JIT compiled and loaded into
                                              * memory by the JIT engine, but
                                              * before the code is executed.
                                              * Use iJIT_Method_Load_V2 as event
                                              * data. */
} iJIT_JVM_EVENT;

/** @cond exclude_from_documentation */
/* Legacy stuff. Do not use it. */
typedef enum _iJIT_ModeFlags
{
    iJIT_NO_NOTIFICATIONS          = 0x0000,
    iJIT_BE_NOTIFY_ON_LOAD         = 0x0001,
    iJIT_BE_NOTIFY_ON_UNLOAD       = 0x0002,
    iJIT_BE_NOTIFY_ON_METHOD_ENTRY = 0x0004,
    iJIT_BE_NOTIFY_ON_METHOD_EXIT  = 0x0008

} iJIT_ModeFlags;
/** @endcond */

/**
 * @brief Enumerator for the agent's mode
 */
typedef enum _iJIT_IsProfilingActiveFlags
{
    iJIT_NOTHING_RUNNING           = 0x0000,    /**<\brief The agent is not running;
                                                 * iJIT_NotifyEvent calls will
                                                 * not be processed. */
    iJIT_SAMPLING_ON               = 0x0001,    /**<\brief The agent is running and
                                                 * ready to process notifications. */

/** @cond exclude_from_documentation */
    /* Legacy. Call Graph is running */
    iJIT_CALLGRAPH_ON              = 0x0002
/** @endcond */

} iJIT_IsProfilingActiveFlags;

/** @cond exclude_from_documentation */
/* Legacy stuff. Do not use it. */
typedef enum _iJDEnvironmentType
{
    iJDE_JittingAPI = 2

} iJDEnvironmentType;

typedef struct _iJIT_Method_Id
{
    unsigned int method_id;

} *piJIT_Method_Id, iJIT_Method_Id;

typedef struct _iJIT_Method_NIDS
{
    unsigned int method_id;     /**<\brief Unique method ID */
    unsigned int stack_id;      /**<\brief NOTE: no need to fill this field,
                                 * it's filled by VTune Amplifier */
    char*  method_name;         /**<\brief Method name (just the method, without the class) */

} *piJIT_Method_NIDS, iJIT_Method_NIDS;
/** @endcond */

/**
 * @brief Description of a single entry in the line number information of a code region.
 * @details A table of line number entries gives information about how the reported code region
 * is mapped to source file.
 * Intel(R) VTune(TM) Amplifier uses line number information to attribute
 * the samples (virtual address) to a line number. \n
 * It is acceptable to report different code addresses for the same source line:
 * @code
 *   Offset LineNumber
 *      1       2
 *      12      4
 *      15      2
 *      18      1
 *      21      30
 *
 *  VTune Amplifier constructs the following table using the client data
 *
 *   Code subrange  Line number
 *      0-1             2
 *      1-12            4
 *      12-15           2
 *      15-18           1
 *      18-21           30
 * @endcode
 */
typedef struct _LineNumberInfo
{
    unsigned int Offset;     /**<\brief Offset from the begining of the code region. */
    unsigned int LineNumber; /**<\brief Matching source line number offset (from beginning of source file). */

} *pLineNumberInfo, LineNumberInfo;

/**
 * @brief Description of a JIT-compiled method
 * @details When you use the iJIT_Method_Load structure to describe
 *  the JIT compiled method, use iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED
 *  as an event type to report it.
 */
typedef struct _iJIT_Method_Load
{
    unsigned int method_id; /**<\brief Unique method ID.
                             *  Method ID cannot be smaller than 999.
                             *  You must either use the API function
                             *  iJIT_GetNewMethodID to get a valid and unique
                             *  method ID, or else manage ID uniqueness
                             *  and correct range by yourself.\n
                             *  You must use the same method ID for all code
                             *  regions of the same method, otherwise different
                             *  method IDs specify different methods. */

    char* method_name; /**<\brief The name of the method. It can be optionally
                        *  prefixed with its class name and appended with
                        *  its complete signature. Can't be NULL. */

    void* method_load_address; /**<\brief The start virtual address of the method code
                                *  region. If NULL, data provided with
                                *  event are not accepted. */

    unsigned int method_size; /**<\brief The code size of the method in memory.
                               *  If 0, then data provided with the event are not
                               *  accepted. */

    unsigned int line_number_size; /**<\brief The number of entries in the line number
                                    *  table.0 if none. */

    pLineNumberInfo line_number_table; /**<\brief Pointer to the line numbers info
                                        *  array. Can be NULL if
                                        *  line_number_size is 0. See
                                        *  LineNumberInfo Structure for a
                                        *  description of a single entry in
                                        *  the line number info array */

    unsigned int class_id; /**<\brief This field is obsolete. */

    char* class_file_name; /**<\brief Class name. Can be NULL.*/

    char* source_file_name; /**<\brief Source file name. Can be NULL.*/

    void* user_data; /**<\brief This field is obsolete. */

    unsigned int user_data_size; /**<\brief This field is obsolete. */

    iJDEnvironmentType  env; /**<\brief This field is obsolete. */

} *piJIT_Method_Load, iJIT_Method_Load;

#pragma pack(push, 8)
/**
 * @brief Description of a JIT-compiled method
 * @details When you use the iJIT_Method_Load_V2 structure to describe
 *  the JIT compiled method, use iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED_V2
 *  as an event type to report it.
 */
typedef struct _iJIT_Method_Load_V2
{
    unsigned int method_id; /**<\brief Unique method ID.
                             *  Method ID cannot be smaller than 999.
                             *  You must either use the API function
                             *  iJIT_GetNewMethodID to get a valid and unique
                             *  method ID, or else manage ID uniqueness
                             *  and correct range by yourself.\n
                             *  You must use the same method ID for all code
                             *  regions of the same method, otherwise different
                             *  method IDs specify different methods. */

    char* method_name; /**<\brief The name of the method. It can be optionally
                        *  prefixed with its class name and appended with
                        *  its complete signature. Can't be  NULL. */

    void* method_load_address; /**<\brief The start virtual address of the method code
                                *  region. If NULL, then data provided with the
                                *  event are not accepted. */

    unsigned int method_size; /**<\brief The code size of the method in memory.
                               *  If 0, then data provided with the event are not
                               *  accepted. */

    unsigned int line_number_size; /**<\brief The number of entries in the line number
                                    *  table. 0 if none. */

    pLineNumberInfo line_number_table; /**<\brief Pointer to the line numbers info
                                        *  array. Can be NULL if
                                        *  line_number_size is 0. See
                                        *  LineNumberInfo Structure for a
                                        *  description of a single entry in
                                        *  the line number info array. */

    char* class_file_name; /**<\brief Class name. Can be NULL. */

    char* source_file_name; /**<\brief Source file name. Can be NULL. */

    char* module_name; /**<\brief Module name. Can be NULL.
                           The module name can be useful for distinguishing among
                           different JIT engines. VTune Amplifier will display
                           reported methods grouped by specific module. */

} *piJIT_Method_Load_V2, iJIT_Method_Load_V2;
#pragma pack(pop)

/**
 * @brief Description of an inline JIT-compiled method
 * @details When you use the_iJIT_Method_Inline_Load structure to describe
 *  the JIT compiled method, use iJVM_EVENT_TYPE_METHOD_INLINE_LOAD_FINISHED
 *  as an event type to report it.
 */
typedef struct _iJIT_Method_Inline_Load
{
    unsigned int method_id; /**<\brief Unique method ID.
                             *  Method ID cannot be smaller than 999.
                             *  You must either use the API function
                             *  iJIT_GetNewMethodID to get a valid and unique
                             *  method ID, or else manage ID uniqueness
                             *  and correct range by yourself. */

    unsigned int parent_method_id; /**<\brief Unique immediate parent's method ID.
                                    *  Method ID may not be smaller than 999.
                                    *  You must either use the API function
                                    *  iJIT_GetNewMethodID to get a valid and unique
                                    *  method ID, or else manage ID uniqueness
                                    *  and correct range by yourself. */

    char* method_name; /**<\brief The name of the method. It can be optionally
                        *  prefixed with its class name and appended with
                        *  its complete signature. Can't be NULL. */

    void* method_load_address;  /** <\brief The virtual address on which the method
                                 *  is inlined. If NULL, then data provided with
                                 *  the event are not accepted. */

    unsigned int method_size; /**<\brief The code size of the method in memory.
                               *  If 0, then data provided with the event are not
                               *  accepted. */

    unsigned int line_number_size; /**<\brief The number of entries in the line number
                                    *  table. 0 if none. */

    pLineNumberInfo line_number_table; /**<\brief Pointer to the line numbers info
                                        *  array. Can be NULL if
                                        *  line_number_size is 0. See
                                        *  LineNumberInfo Structure for a
                                        *  description of a single entry in
                                        *  the line number info array */

    char* class_file_name; /**<\brief Class name. Can be NULL.*/

    char* source_file_name; /**<\brief Source file name. Can be NULL.*/

} *piJIT_Method_Inline_Load, iJIT_Method_Inline_Load;

/** @cond exclude_from_documentation */
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef CDECL
#  if defined WIN32 || defined _WIN32
#    define CDECL __cdecl
#  else /* defined WIN32 || defined _WIN32 */
#    if defined _M_IX86 || defined __i386__
#      define CDECL __attribute__ ((cdecl))
#    else  /* _M_IX86 || __i386__ */
#      define CDECL /* actual only on x86_64 platform */
#    endif /* _M_IX86 || __i386__ */
#  endif /* defined WIN32 || defined _WIN32 */
#endif /* CDECL */

#define JITAPI CDECL
/** @endcond */

/**
 * @brief Generates a new unique method ID.
 *
 * You must use this API to obtain unique and valid method IDs for methods or
 * traces reported to the agent if you don't have your own mechanism to generate
 * unique method IDs.
 *
 * @return a new unique method ID. When out of unique method IDs, this API
 * returns 0, which is not an accepted value.
 */
unsigned int JITAPI iJIT_GetNewMethodID(void);

/**
 * @brief Returns the current mode of the agent.
 *
 * @return iJIT_SAMPLING_ON, indicating that agent is running, or
 * iJIT_NOTHING_RUNNING if no agent is running.
 */
iJIT_IsProfilingActiveFlags JITAPI iJIT_IsProfilingActive(void);

/**
 * @brief Reports infomation about JIT-compiled code to the agent.
 *
 * The reported information is used to attribute samples obtained from any
 * Intel(R) VTune(TM) Amplifier collector. This API needs to be called
 * after JIT compilation and before the first entry into the JIT-compiled
 * code.
 *
 * @param[in] event_type - type of the data sent to the agent
 * @param[in] EventSpecificData - pointer to event-specific data
 *
 * @returns 1 on success, otherwise 0.
 */
int JITAPI iJIT_NotifyEvent(iJIT_JVM_EVENT event_type, void *EventSpecificData);

/** @cond exclude_from_documentation */
/*
 * Do not use these legacy APIs, which are here for backward compatibility
 * with Intel(R) VTune(TM) Performance Analyzer.
 */
typedef void (*iJIT_ModeChangedEx)(void *UserData, iJIT_ModeFlags Flags);
void JITAPI iJIT_RegisterCallbackEx(void *userdata,
                                    iJIT_ModeChangedEx NewModeCallBackFuncEx);
void JITAPI FinalizeThread(void);
void JITAPI FinalizeProcess(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */
/** @endcond */

/** @} jitapi group */

#endif /* __JITPROFILING_H__ */
