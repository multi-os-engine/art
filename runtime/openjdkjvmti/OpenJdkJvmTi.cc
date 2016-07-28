/* Copyright (C) 2016 The Android Open Source Project
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This file implements interfaces from the file jvm.h. This implementation
 * is licensed under the same terms as the file jvm.h.  The
 * copyright and license information for the file jvm.h follows.
 *
 * Copyright (c) 1997, 2011, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#include "../../libcore/ojluni/src/main/native/jvmti.h"  // TODO(narayan): fix it
#include "gc_root-inl.h"
#include "globals.h"
#include "ti/env.h"
#include "scoped_thread_state_change.h"

#pragma GCC diagnostic ignored "-Wunused-parameter"
namespace openjdkjvmti {

// A structure that is a jvmtiEnv with additional information for the runtime.
struct ArtJvmTiEnv : public jvmtiEnv {
  art::ti::Env art_env;
};


// Macro and constexpr to make error values less annoying to write.
#define ERR(e) JVMTI_ERROR_ ## e
static constexpr jvmtiError OK = JVMTI_ERROR_NONE;

// Special error code for unimplemented functions in JVMTI
static constexpr jvmtiError ERR(NOT_IMPLEMENTED) = JVMTI_ERROR_NOT_AVAILABLE;

class JvmtiFunctions {
 private:
  static art::ti::Env* AsArtEnv(jvmtiEnv* env) {
    return &(static_cast<ArtJvmTiEnv*>(env)->art_env);
  }

  static bool IsValidEnv(jvmtiEnv* env) {
    return env != nullptr && (art::kIsDebugBuild || AsArtEnv(env)->IsValid());
  }

 public:
  static jvmtiError Allocate(jvmtiEnv* env, jlong size, unsigned char** mem_ptr) {
    if (!IsValidEnv(env)) {
      return ERR(INVALID_ENVIRONMENT);
    }
    if (mem_ptr == nullptr) {
      return ERR(NULL_POINTER);
    }
    if (size < 0) {
      return ERR(ILLEGAL_ARGUMENT);
    } else if (size == 0) {
      *mem_ptr = nullptr;
      return OK;
    }
    *mem_ptr = static_cast<unsigned char*>(malloc(size));
    return (*mem_ptr != nullptr) ? OK : ERR(OUT_OF_MEMORY);
  }

  static jvmtiError Deallocate(jvmtiEnv* env, unsigned char* mem) {
    if (!IsValidEnv(env)) {
      return ERR(INVALID_ENVIRONMENT);
    }
    if (mem != nullptr) {
      free(mem);
    }
    return OK;
  }

  static jvmtiError GetThreadState(jvmtiEnv* env, jthread thread, jint* thread_state_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetCurrentThread(jvmtiEnv* env, jthread* thread_ptr) {
    art::ScopedObjectAccess soa(AsArtEnv(env));
    *thread_ptr = soa.AddLocalReference<jthread>(soa.Self()->GetPeer());
    return OK;
  }

  static jvmtiError GetAllThreads(jvmtiEnv* env, jint* threads_count_ptr, jthread** threads_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SuspendThread(jvmtiEnv* env, jthread thread) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SuspendThreadList(jvmtiEnv* env,
                                      jint request_count,
                                      const jthread* request_list,
                                      jvmtiError* results) {
    jvmtiCapabilities caps;
    if (GetCapabilities(env, &caps) != OK || !caps.can_suspend) {
      return ERR(MUST_POSSESS_CAPABILITY);
    } else if (request_count < 0) {
      return ERR(ILLEGAL_ARGUMENT);
    } else if (request_list == nullptr || results == nullptr) {
      return ERR(NULL_POINTER);
    } else {
      jvmtiError* final_result = results + request_count;
      for (; results != final_result; results++, request_list++) {
        *results = SuspendThread(env, *request_list);
      }
      return OK;
    }
  }

  static jvmtiError ResumeThread(jvmtiEnv* env, jthread thread) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError ResumeThreadList(jvmtiEnv* env,
                                    jint request_count,
                                    const jthread* request_list,
                                    jvmtiError* results) {
    jvmtiCapabilities caps;
    if (GetCapabilities(env, &caps) != OK || !caps.can_suspend) {
      return ERR(MUST_POSSESS_CAPABILITY);
    } else if (request_count < 0) {
      return ERR(ILLEGAL_ARGUMENT);
    } else if (request_list == nullptr || results == nullptr) {
      return ERR(NULL_POINTER);
    } else {
      jvmtiError* final_result = results + request_count;
      for (; results != final_result; results++, request_list++) {
        *results = ResumeThread(env, *request_list);
      }
      return OK;
    }
  }

  static jvmtiError StopThread(jvmtiEnv* env, jthread thread, jobject exception) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError InterruptThread(jvmtiEnv* env, jthread thread) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetThreadInfo(jvmtiEnv* env, jthread thread, jvmtiThreadInfo* info_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetOwnedMonitorInfo(jvmtiEnv* env,
                                        jthread thread,
                                        jint* owned_monitor_count_ptr,
                                        jobject** owned_monitors_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetOwnedMonitorStackDepthInfo(jvmtiEnv* env,
                                                  jthread thread,
                                                  jint* monitor_info_count_ptr,
                                                  jvmtiMonitorStackDepthInfo** monitor_info_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetCurrentContendedMonitor(jvmtiEnv* env, jthread thread, jobject* monitor_ptr) {
  // TODO
  return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError RunAgentThread(jvmtiEnv* env,
                                  jthread thread,
                                  jvmtiStartFunction proc,
                                  const void* arg,
                                  jint priority) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SetThreadLocalStorage(jvmtiEnv* env, jthread thread, const void* data) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetThreadLocalStorage(jvmtiEnv* env, jthread thread, void** data_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetTopThreadGroups(jvmtiEnv* env,
                                      jint* group_count_ptr,
                                      jthreadGroup** groups_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetThreadGroupInfo(jvmtiEnv* env,
                                      jthreadGroup group,
                                      jvmtiThreadGroupInfo* info_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetThreadGroupChildren(jvmtiEnv* env,
                                          jthreadGroup group,
                                          jint* thread_count_ptr,
                                          jthread** threads_ptr,
                                          jint* group_count_ptr,
                                          jthreadGroup** groups_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetStackTrace(jvmtiEnv* env,
                                  jthread thread,
                                  jint start_depth,
                                  jint max_frame_count,
                                  jvmtiFrameInfo* frame_buffer,
                                  jint* count_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetAllStackTraces(jvmtiEnv* env,
                                      jint max_frame_count,
                                      jvmtiStackInfo** stack_info_ptr,
                                      jint* thread_count_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetThreadListStackTraces(jvmtiEnv* env,
                                            jint thread_count,
                                            const jthread* thread_list,
                                            jint max_frame_count,
                                            jvmtiStackInfo** stack_info_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetFrameCount(jvmtiEnv* env, jthread thread, jint* count_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError PopFrame(jvmtiEnv* env, jthread thread) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetFrameLocation(jvmtiEnv* env,
                                    jthread thread,
                                    jint depth,
                                    jmethodID* method_ptr,
                                    jlocation* location_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError NotifyFramePop(jvmtiEnv* env, jthread thread, jint depth) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError ForceEarlyReturnObject(jvmtiEnv* env, jthread thread, jobject value) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError ForceEarlyReturnInt(jvmtiEnv* env, jthread thread, jint value) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError ForceEarlyReturnLong(jvmtiEnv* env, jthread thread, jlong value) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError ForceEarlyReturnFloat(jvmtiEnv* env, jthread thread, jfloat value) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError ForceEarlyReturnDouble(jvmtiEnv* env, jthread thread, jdouble value) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError ForceEarlyReturnVoid(jvmtiEnv* env, jthread thread) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError FollowReferences(jvmtiEnv* env,
                                    jint heap_filter,
                                    jclass klass,
                                    jobject initial_object,
                                    const jvmtiHeapCallbacks* callbacks,
                                    const void* user_data) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError IterateThroughHeap(jvmtiEnv* env,
                                      jint heap_filter,
                                      jclass klass,
                                      const jvmtiHeapCallbacks* callbacks,
                                      const void* user_data) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetTag(jvmtiEnv* env, jobject object, jlong* tag_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SetTag(jvmtiEnv* env, jobject object, jlong tag) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetObjectsWithTags(jvmtiEnv* env,
                                      jint tag_count,
                                      const jlong* tags,
                                      jint* count_ptr,
                                      jobject** object_result_ptr,
                                      jlong** tag_result_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError ForceGarbageCollection(jvmtiEnv* env) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError IterateOverObjectsReachableFromObject(
      jvmtiEnv* env,
      jobject object,
      jvmtiObjectReferenceCallback object_reference_callback,
      const void* user_data) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError IterateOverReachableObjects(jvmtiEnv* env,
                                                jvmtiHeapRootCallback heap_root_callback,
                                                jvmtiStackReferenceCallback stack_ref_callback,
                                                jvmtiObjectReferenceCallback object_ref_callback,
                                                const void* user_data) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError IterateOverHeap(jvmtiEnv* env,
                                    jvmtiHeapObjectFilter object_filter,
                                    jvmtiHeapObjectCallback heap_object_callback,
                                    const void* user_data) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError IterateOverInstancesOfClass(jvmtiEnv* env,
                                                jclass klass,
                                                jvmtiHeapObjectFilter object_filter,
                                                jvmtiHeapObjectCallback heap_object_callback,
                                                const void* user_data) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetLocalObject(jvmtiEnv* env,
                                  jthread thread,
                                  jint depth,
                                  jint slot,
                                  jobject* value_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetLocalInstance(jvmtiEnv* env, jthread thread, jint depth, jobject* value_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetLocalInt(jvmtiEnv* env,
                                jthread thread,
                                jint depth,
                                jint slot,
                                jint* value_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetLocalLong(jvmtiEnv* env,
                                jthread thread,
                                jint depth,
                                jint slot,
                                jlong* value_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetLocalFloat(jvmtiEnv* env,
                                  jthread thread,
                                  jint depth,
                                  jint slot,
                                  jfloat* value_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetLocalDouble(jvmtiEnv* env,
                                  jthread thread,
                                  jint depth,
                                  jint slot,
                                  jdouble* value_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SetLocalObject(jvmtiEnv* env,
                                  jthread thread,
                                  jint depth,
                                  jint slot,
                                  jobject value) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SetLocalInt(jvmtiEnv* env,
                                jthread thread,
                                jint depth,
                                jint slot,
                                jint value) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SetLocalLong(jvmtiEnv* env,
                                jthread thread,
                                jint depth,
                                jint slot,
                                jlong value) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SetLocalFloat(jvmtiEnv* env,
                                  jthread thread,
                                  jint depth,
                                  jint slot,
                                  jfloat value) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SetLocalDouble(jvmtiEnv* env,
                                  jthread thread,
                                  jint depth,
                                  jint slot,
                                  jdouble value) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SetBreakpoint(jvmtiEnv* env, jmethodID method, jlocation location) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError ClearBreakpoint(jvmtiEnv* env, jmethodID method, jlocation location) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SetFieldAccessWatch(jvmtiEnv* env, jclass klass, jfieldID field) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError ClearFieldAccessWatch(jvmtiEnv* env, jclass klass, jfieldID field) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SetFieldModificationWatch(jvmtiEnv* env, jclass klass, jfieldID field) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError ClearFieldModificationWatch(jvmtiEnv* env, jclass klass, jfieldID field) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetLoadedClasses(jvmtiEnv* env, jint* class_count_ptr, jclass** classes_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetClassLoaderClasses(jvmtiEnv* env,
                                          jobject initiating_loader,
                                          jint* class_count_ptr,
                                          jclass** classes_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetClassSignature(jvmtiEnv* env,
                                      jclass klass,
                                      char** signature_ptr,
                                      char** generic_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetClassStatus(jvmtiEnv* env, jclass klass, jint* status_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetSourceFileName(jvmtiEnv* env, jclass klass, char** source_name_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetClassModifiers(jvmtiEnv* env, jclass klass, jint* modifiers_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetClassMethods(jvmtiEnv* env,
                                    jclass klass,
                                    jint* method_count_ptr,
                                    jmethodID** methods_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetClassFields(jvmtiEnv* env,
                                  jclass klass,
                                  jint* field_count_ptr,
                                  jfieldID** fields_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetImplementedInterfaces(jvmtiEnv* env,
                                            jclass klass,
                                            jint* interface_count_ptr,
                                            jclass** interfaces_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetClassVersionNumbers(jvmtiEnv* env,
                                          jclass klass,
                                          jint* minor_version_ptr,
                                          jint* major_version_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetConstantPool(jvmtiEnv* env,
                                    jclass klass,
                                    jint* constant_pool_count_ptr,
                                    jint* constant_pool_byte_count_ptr,
                                    unsigned char** constant_pool_bytes_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError IsInterface(jvmtiEnv* env, jclass klass, jboolean* is_interface_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError IsArrayClass(jvmtiEnv* env,
                                jclass klass,
                                jboolean* is_array_class_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError IsModifiableClass(jvmtiEnv* env,
                                      jclass klass,
                                      jboolean* is_modifiable_class_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetClassLoader(jvmtiEnv* env, jclass klass, jobject* classloader_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetSourceDebugExtension(jvmtiEnv* env,
                                            jclass klass,
                                            char** source_debug_extension_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError RetransformClasses(jvmtiEnv* env, jint class_count, const jclass* classes) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError RedefineClasses(jvmtiEnv* env,
                                    jint class_count,
                                    const jvmtiClassDefinition* class_definitions) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetObjectSize(jvmtiEnv* env, jobject object, jlong* size_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetObjectHashCode(jvmtiEnv* env, jobject object, jint* hash_code_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetObjectMonitorUsage(jvmtiEnv* env,
                                          jobject object,
                                          jvmtiMonitorUsage* info_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetFieldName(jvmtiEnv* env,
                                jclass klass,
                                jfieldID field,
                                char** name_ptr,
                                char** signature_ptr,
                                char** generic_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetFieldDeclaringClass(jvmtiEnv* env,
                                          jclass klass,
                                          jfieldID field,
                                          jclass* declaring_class_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetFieldModifiers(jvmtiEnv* env,
                                      jclass klass,
                                      jfieldID field,
                                      jint* modifiers_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError IsFieldSynthetic(jvmtiEnv* env,
                                    jclass klass,
                                    jfieldID field,
                                    jboolean* is_synthetic_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetMethodName(jvmtiEnv* env,
                                  jmethodID method,
                                  char** name_ptr,
                                  char** signature_ptr,
                                  char** generic_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetMethodDeclaringClass(jvmtiEnv* env,
                                            jmethodID method,
                                            jclass* declaring_class_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetMethodModifiers(jvmtiEnv* env,
                                      jmethodID method,
                                      jint* modifiers_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetMaxLocals(jvmtiEnv* env,
                                jmethodID method,
                                jint* max_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetArgumentsSize(jvmtiEnv* env,
                                    jmethodID method,
                                    jint* size_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetLineNumberTable(jvmtiEnv* env,
                                      jmethodID method,
                                      jint* entry_count_ptr,
                                      jvmtiLineNumberEntry** table_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetMethodLocation(jvmtiEnv* env,
                                      jmethodID method,
                                      jlocation* start_location_ptr,
                                      jlocation* end_location_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetLocalVariableTable(jvmtiEnv* env,
                                          jmethodID method,
                                          jint* entry_count_ptr,
                                          jvmtiLocalVariableEntry** table_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetBytecodes(jvmtiEnv* env,
                                 jmethodID method,
                                 jint* bytecode_count_ptr,
                                 unsigned char** bytecodes_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError IsMethodNative(jvmtiEnv* env, jmethodID method, jboolean* is_native_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError IsMethodSynthetic(jvmtiEnv* env, jmethodID method, jboolean* is_synthetic_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError IsMethodObsolete(jvmtiEnv* env, jmethodID method, jboolean* is_obsolete_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SetNativeMethodPrefix(jvmtiEnv* env, const char* prefix) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SetNativeMethodPrefixes(jvmtiEnv* env, jint prefix_count, char** prefixes) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError CreateRawMonitor(jvmtiEnv* env, const char* name, jrawMonitorID* monitor_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError DestroyRawMonitor(jvmtiEnv* env, jrawMonitorID monitor) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError RawMonitorEnter(jvmtiEnv* env, jrawMonitorID monitor) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError RawMonitorExit(jvmtiEnv* env, jrawMonitorID monitor) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError RawMonitorWait(jvmtiEnv* env, jrawMonitorID monitor, jlong millis) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError RawMonitorNotify(jvmtiEnv* env, jrawMonitorID monitor) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError RawMonitorNotifyAll(jvmtiEnv* env, jrawMonitorID monitor) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SetJNIFunctionTable(jvmtiEnv* env, const jniNativeInterface* function_table) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetJNIFunctionTable(jvmtiEnv* env, jniNativeInterface** function_table) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SetEventCallbacks(jvmtiEnv* env,
                                      const jvmtiEventCallbacks* callbacks,
                                      jint size_of_callbacks) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SetEventNotificationMode(jvmtiEnv* env,
                                            jvmtiEventMode mode,
                                            jvmtiEvent event_type,
                                            jthread event_thread,
                                            ...) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GenerateEvents(jvmtiEnv* env, jvmtiEvent event_type) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetExtensionFunctions(jvmtiEnv* env,
                                          jint* extension_count_ptr,
                                          jvmtiExtensionFunctionInfo** extensions) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetExtensionEvents(jvmtiEnv* env,
                                       jint* extension_count_ptr,
                                       jvmtiExtensionEventInfo** extensions) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SetExtensionEventCallback(jvmtiEnv* env,
                                              jint extension_event_index,
                                              jvmtiExtensionEvent callback) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetPotentialCapabilities(jvmtiEnv* env, jvmtiCapabilities* capabilities_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError AddCapabilities(jvmtiEnv* env, const jvmtiCapabilities* capabilities_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError RelinquishCapabilities(jvmtiEnv* env,
                                           const jvmtiCapabilities* capabilities_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetCapabilities(jvmtiEnv* env, jvmtiCapabilities* capabilities_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetCurrentThreadCpuTimerInfo(jvmtiEnv* env, jvmtiTimerInfo* info_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetCurrentThreadCpuTime(jvmtiEnv* env, jlong* nanos_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetThreadCpuTimerInfo(jvmtiEnv* env, jvmtiTimerInfo* info_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetThreadCpuTime(jvmtiEnv* env, jthread thread, jlong* nanos_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetTimerInfo(jvmtiEnv* env, jvmtiTimerInfo* info_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetTime(jvmtiEnv* env, jlong* nanos_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetAvailableProcessors(jvmtiEnv* env, jint* processor_count_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError AddToBootstrapClassLoaderSearch(jvmtiEnv* env, const char* segment) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError AddToSystemClassLoaderSearch(jvmtiEnv* env, const char* segment) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetSystemProperties(jvmtiEnv* env, jint* count_ptr, char*** property_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetSystemProperty(jvmtiEnv* env, const char* property, char** value_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SetSystemProperty(jvmtiEnv* env, const char* property, const char* value) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetPhase(jvmtiEnv* env, jvmtiPhase* phase_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError DisposeEnvironment(jvmtiEnv* env) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SetEnvironmentLocalStorage(jvmtiEnv* env, const void* data) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetEnvironmentLocalStorage(jvmtiEnv* env, void** data_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetVersionNumber(jvmtiEnv* env, jint* version_ptr) {
    *version_ptr = JVMTI_VERSION;
    // TODO
    return OK;
  }

  static jvmtiError GetErrorName(jvmtiEnv* env, jvmtiError error,  char** name_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SetVerboseFlag(jvmtiEnv* env, jvmtiVerboseFlag flag, jboolean value) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetJLocationFormat(jvmtiEnv* env, jvmtiJlocationFormat* format_ptr) {
    // TODO
    return ERR(NOT_IMPLEMENTED);
  }
};
const jvmtiInterface_1 gJvmtiInterface = {
  nullptr,  // reserved1
  JvmtiFunctions::SetEventNotificationMode,
  nullptr,  // reserved3
  JvmtiFunctions::GetAllThreads,
  JvmtiFunctions::SuspendThread,
  JvmtiFunctions::ResumeThread,
  JvmtiFunctions::StopThread,
  JvmtiFunctions::InterruptThread,
  JvmtiFunctions::GetThreadInfo,
  JvmtiFunctions::GetOwnedMonitorInfo,  // 10
  JvmtiFunctions::GetCurrentContendedMonitor,
  JvmtiFunctions::RunAgentThread,
  JvmtiFunctions::GetTopThreadGroups,
  JvmtiFunctions::GetThreadGroupInfo,
  JvmtiFunctions::GetThreadGroupChildren,
  JvmtiFunctions::GetFrameCount,
  JvmtiFunctions::GetThreadState,
  JvmtiFunctions::GetCurrentThread,
  JvmtiFunctions::GetFrameLocation,
  JvmtiFunctions::NotifyFramePop,  // 20
  JvmtiFunctions::GetLocalObject,
  JvmtiFunctions::GetLocalInt,
  JvmtiFunctions::GetLocalLong,
  JvmtiFunctions::GetLocalFloat,
  JvmtiFunctions::GetLocalDouble,
  JvmtiFunctions::SetLocalObject,
  JvmtiFunctions::SetLocalInt,
  JvmtiFunctions::SetLocalLong,
  JvmtiFunctions::SetLocalFloat,
  JvmtiFunctions::SetLocalDouble,  // 30
  JvmtiFunctions::CreateRawMonitor,
  JvmtiFunctions::DestroyRawMonitor,
  JvmtiFunctions::RawMonitorEnter,
  JvmtiFunctions::RawMonitorExit,
  JvmtiFunctions::RawMonitorWait,
  JvmtiFunctions::RawMonitorNotify,
  JvmtiFunctions::RawMonitorNotifyAll,
  JvmtiFunctions::SetBreakpoint,
  JvmtiFunctions::ClearBreakpoint,
  nullptr,  // reserved40
  JvmtiFunctions::SetFieldAccessWatch,
  JvmtiFunctions::ClearFieldAccessWatch,
  JvmtiFunctions::SetFieldModificationWatch,
  JvmtiFunctions::ClearFieldModificationWatch,
  JvmtiFunctions::IsModifiableClass,
  JvmtiFunctions::Allocate,
  JvmtiFunctions::Deallocate,
  JvmtiFunctions::GetClassSignature,
  JvmtiFunctions::GetClassStatus,
  JvmtiFunctions::GetSourceFileName,  // 50
  JvmtiFunctions::GetClassModifiers,
  JvmtiFunctions::GetClassMethods,
  JvmtiFunctions::GetClassFields,
  JvmtiFunctions::GetImplementedInterfaces,
  JvmtiFunctions::IsInterface,
  JvmtiFunctions::IsArrayClass,
  JvmtiFunctions::GetClassLoader,
  JvmtiFunctions::GetObjectHashCode,
  JvmtiFunctions::GetObjectMonitorUsage,
  JvmtiFunctions::GetFieldName,  // 60
  JvmtiFunctions::GetFieldDeclaringClass,
  JvmtiFunctions::GetFieldModifiers,
  JvmtiFunctions::IsFieldSynthetic,
  JvmtiFunctions::GetMethodName,
  JvmtiFunctions::GetMethodDeclaringClass,
  JvmtiFunctions::GetMethodModifiers,
  nullptr,  // reserved67
  JvmtiFunctions::GetMaxLocals,
  JvmtiFunctions::GetArgumentsSize,
  JvmtiFunctions::GetLineNumberTable,  // 70
  JvmtiFunctions::GetMethodLocation,
  JvmtiFunctions::GetLocalVariableTable,
  JvmtiFunctions::SetNativeMethodPrefix,
  JvmtiFunctions::SetNativeMethodPrefixes,
  JvmtiFunctions::GetBytecodes,
  JvmtiFunctions::IsMethodNative,
  JvmtiFunctions::IsMethodSynthetic,
  JvmtiFunctions::GetLoadedClasses,
  JvmtiFunctions::GetClassLoaderClasses,
  JvmtiFunctions::PopFrame,  // 80
  JvmtiFunctions::ForceEarlyReturnObject,
  JvmtiFunctions::ForceEarlyReturnInt,
  JvmtiFunctions::ForceEarlyReturnLong,
  JvmtiFunctions::ForceEarlyReturnFloat,
  JvmtiFunctions::ForceEarlyReturnDouble,
  JvmtiFunctions::ForceEarlyReturnVoid,
  JvmtiFunctions::RedefineClasses,
  JvmtiFunctions::GetVersionNumber,
  JvmtiFunctions::GetCapabilities,
  JvmtiFunctions::GetSourceDebugExtension,  // 90
  JvmtiFunctions::IsMethodObsolete,
  JvmtiFunctions::SuspendThreadList,
  JvmtiFunctions::ResumeThreadList,
  nullptr,  // reserved94
  nullptr,  // reserved95
  nullptr,  // reserved96
  nullptr,  // reserved97
  nullptr,  // reserved98
  nullptr,  // reserved99
  JvmtiFunctions::GetAllStackTraces,  // 100
  JvmtiFunctions::GetThreadListStackTraces,
  JvmtiFunctions::GetThreadLocalStorage,
  JvmtiFunctions::SetThreadLocalStorage,
  JvmtiFunctions::GetStackTrace,
  nullptr,  // reserved105
  JvmtiFunctions::GetTag,
  JvmtiFunctions::SetTag,
  JvmtiFunctions::ForceGarbageCollection,
  JvmtiFunctions::IterateOverObjectsReachableFromObject,
  JvmtiFunctions::IterateOverReachableObjects,  // 110
  JvmtiFunctions::IterateOverHeap,
  JvmtiFunctions::IterateOverInstancesOfClass,
  nullptr,  // reserved113
  JvmtiFunctions::GetObjectsWithTags,
  JvmtiFunctions::FollowReferences,
  JvmtiFunctions::IterateThroughHeap,
  nullptr,  // reserved117
  nullptr,  // reserved118
  nullptr,  // reserved119
  JvmtiFunctions::SetJNIFunctionTable,  // 120
  JvmtiFunctions::GetJNIFunctionTable,
  JvmtiFunctions::SetEventCallbacks,
  JvmtiFunctions::GenerateEvents,
  JvmtiFunctions::GetExtensionFunctions,
  JvmtiFunctions::GetExtensionEvents,
  JvmtiFunctions::SetExtensionEventCallback,
  JvmtiFunctions::DisposeEnvironment,
  JvmtiFunctions::GetErrorName,
  JvmtiFunctions::GetJLocationFormat,
  JvmtiFunctions::GetSystemProperties,  // 130
  JvmtiFunctions::GetSystemProperty,
  JvmtiFunctions::SetSystemProperty,
  JvmtiFunctions::GetPhase,
  JvmtiFunctions::GetCurrentThreadCpuTimerInfo,
  JvmtiFunctions::GetCurrentThreadCpuTime,
  JvmtiFunctions::GetThreadCpuTimerInfo,
  JvmtiFunctions::GetThreadCpuTime,
  JvmtiFunctions::GetTimerInfo,
  JvmtiFunctions::GetTime,
  JvmtiFunctions::GetPotentialCapabilities,  // 140
  nullptr,  // reserved141
  JvmtiFunctions::AddCapabilities,
  JvmtiFunctions::RelinquishCapabilities,
  JvmtiFunctions::GetAvailableProcessors,
  JvmtiFunctions::GetClassVersionNumbers,
  JvmtiFunctions::GetConstantPool,
  JvmtiFunctions::GetEnvironmentLocalStorage,
  JvmtiFunctions::SetEnvironmentLocalStorage,
  JvmtiFunctions::AddToBootstrapClassLoaderSearch,
  JvmtiFunctions::SetVerboseFlag,  // 150
  JvmtiFunctions::AddToSystemClassLoaderSearch,
  JvmtiFunctions::RetransformClasses,
  JvmtiFunctions::GetOwnedMonitorStackDepthInfo,
  JvmtiFunctions::GetObjectSize,
  JvmtiFunctions::GetLocalInstance,
};


// Creates a jvmtiEnv and returns it with the art::ti::Env that is associated with it. new_art_ti
// is a pointer to the uninitialized memory for an art::ti::Env.
extern void CreateArtJvmTiEnv(void** new_jvmtiEnv, art::ti::Env** new_art_ti) {
  struct ArtJvmTiEnv* env = static_cast<ArtJvmTiEnv*>(
      art::ti::Env::AllocateForTiEnv(sizeof(ArtJvmTiEnv)));
  env->functions = &gJvmtiInterface;
  *new_jvmtiEnv = env;
  *new_art_ti = &env->art_env;
}

};  // namespace openjdkjvmti
