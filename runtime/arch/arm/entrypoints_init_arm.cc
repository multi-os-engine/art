/*
 * Copyright (C) 2012 The Android Open Source Project
 * Copyright 2014-2016 Intel Corporation
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

#include "entrypoints/jni/jni_entrypoints.h"
#include "entrypoints/quick/quick_alloc_entrypoints.h"
#include "entrypoints/quick/quick_default_externs.h"
#include "entrypoints/quick/quick_entrypoints.h"
#include "entrypoints/entrypoint_utils.h"
#include "entrypoints/math_entrypoints.h"
#include "entrypoints/runtime_asm_entrypoints.h"
#include "interpreter/interpreter.h"

#ifdef MOE
#include <math.h>
#include "arm_eaabi_forward.cc"
#include "thread.h"
#endif

namespace art {

// Cast entrypoints.
extern "C" uint32_t artIsAssignableFromCode(const mirror::Class* klass,
                                            const mirror::Class* ref_class);

#ifdef MOE
static uint32_t art_is_assignable_from_code(const mirror::Class* klass,
                                            const mirror::Class* ref_class) {
    Thread* self;
    __asm__ __volatile__("mov %0, r9" : "=r"(self));
    uint32_t ret = artIsAssignableFromCode(klass, ref_class);
    __asm__ __volatile__("mov r9, %0" : : "r"(self));
    return ret;
}
#endif


// Used by soft float.
// Single-precision FP arithmetics.
extern "C" float fmodf(float a, float b);              // REM_FLOAT[_2ADDR]
// Double-precision FP arithmetics.
extern "C" double fmod(double a, double b);            // REM_DOUBLE[_2ADDR]

// Used by hard float.
extern "C" float art_quick_fmodf(float a, float b);    // REM_FLOAT[_2ADDR]
extern "C" double art_quick_fmod(double a, double b);  // REM_DOUBLE[_2ADDR]

// Integer arithmetics.
extern "C" int __aeabi_idivmod(int32_t, int32_t);  // [DIV|REM]_INT[_2ADDR|_LIT8|_LIT16]

// Long long arithmetics - REM_LONG[_2ADDR] and DIV_LONG[_2ADDR]
extern "C" int64_t __aeabi_ldivmod(int64_t, int64_t);

#ifdef MOE
  static uint32_t art_jni_method_start(Thread* self) {
    uint32_t ret = JniMethodStart(self);
    __asm__ __volatile__("mov r9, %0" : : "r"(self));
    return ret;
  }
  static uint32_t art_jni_method_start_synchronized(jobject to_lock, Thread* self) {
    uint32_t ret = JniMethodStartSynchronized(to_lock, self);
    __asm__ __volatile__("mov r9, %0" : : "r"(self));
    return ret;
  }

  static void art_jni_method_end(uint32_t saved_local_ref_cookie, Thread* self) {
    JniMethodEnd(saved_local_ref_cookie, self);
    __asm__ __volatile__("mov r9, %0" : : "r"(self));
  }

  static void art_jni_method_end_synchronized(uint32_t saved_local_ref_cookie, jobject locked, Thread* self) {
    JniMethodEndSynchronized(saved_local_ref_cookie, locked, self);
    __asm__ __volatile__("mov r9, %0" : : "r"(self));
  }

  static mirror::Object* art_jni_method_end_with_reference(jobject result, uint32_t saved_local_ref_cookie, Thread* self) {
    mirror::Object* ret = JniMethodEndWithReference(result, saved_local_ref_cookie, self);
    __asm__ __volatile__("mov r9, %0" : : "r"(self));
    return ret;
  }

  static mirror::Object* art_jni_method_end_with_reference_synchronized(jobject result, uint32_t saved_local_ref_cookie, jobject locked, Thread* self) {
     mirror::Object* ret = JniMethodEndWithReferenceSynchronized(result, saved_local_ref_cookie, locked, self);
    __asm__ __volatile__("mov r9, %0" : : "r"(self));
    return ret;
  }
    
  static void art_read_barrier_jni(mirror::CompressedReference<mirror::Object>* handle_on_stack, Thread* self) {
    ReadBarrierJni(handle_on_stack, self);
    __asm__ __volatile__("mov r9, %0" : : "r"(self));
  }
  
  static mirror::Object* art_read_barrier_slow(mirror::Object* ref, mirror::Object* obj, uint32_t offset) {
    mirror::Object* ret = artReadBarrierSlow(ref, obj, offset);
    __asm__ __volatile__("mov r9, %0" : : "r"(Thread::Current()));
    return ret;
  }
  
  static void* art_memcpy(void* dst, const void* src, size_t size) {
    void* ret = memcpy(dst, src, size);
    __asm__ __volatile__("mov r9, %0" : : "r"(Thread::Current()));
    return ret;
  }
#endif

void InitEntryPoints(JniEntryPoints* jpoints, QuickEntryPoints* qpoints) {
  // JNI
  jpoints->pDlsymLookup = art_jni_dlsym_lookup_stub;

  // Alloc
  ResetQuickAllocEntryPoints(qpoints);

  // Cast
#ifndef MOE
  qpoints->pInstanceofNonTrivial = artIsAssignableFromCode;
#else
  qpoints->pInstanceofNonTrivial = art_is_assignable_from_code;
#endif
  qpoints->pCheckCast = art_quick_check_cast;

  // DexCache
  qpoints->pInitializeStaticStorage = art_quick_initialize_static_storage;
  qpoints->pInitializeTypeAndVerifyAccess = art_quick_initialize_type_and_verify_access;
  qpoints->pInitializeType = art_quick_initialize_type;
  qpoints->pResolveString = art_quick_resolve_string;

  // Field
  qpoints->pSet8Instance = art_quick_set8_instance;
  qpoints->pSet8Static = art_quick_set8_static;
  qpoints->pSet16Instance = art_quick_set16_instance;
  qpoints->pSet16Static = art_quick_set16_static;
  qpoints->pSet32Instance = art_quick_set32_instance;
  qpoints->pSet32Static = art_quick_set32_static;
  qpoints->pSet64Instance = art_quick_set64_instance;
  qpoints->pSet64Static = art_quick_set64_static;
  qpoints->pSetObjInstance = art_quick_set_obj_instance;
  qpoints->pSetObjStatic = art_quick_set_obj_static;
  qpoints->pGetByteInstance = art_quick_get_byte_instance;
  qpoints->pGetBooleanInstance = art_quick_get_boolean_instance;
  qpoints->pGetShortInstance = art_quick_get_short_instance;
  qpoints->pGetCharInstance = art_quick_get_char_instance;
  qpoints->pGet32Instance = art_quick_get32_instance;
  qpoints->pGet64Instance = art_quick_get64_instance;
  qpoints->pGetObjInstance = art_quick_get_obj_instance;
  qpoints->pGetByteStatic = art_quick_get_byte_static;
  qpoints->pGetBooleanStatic = art_quick_get_boolean_static;
  qpoints->pGetShortStatic = art_quick_get_short_static;
  qpoints->pGetCharStatic = art_quick_get_char_static;
  qpoints->pGet32Static = art_quick_get32_static;
  qpoints->pGet64Static = art_quick_get64_static;
  qpoints->pGetObjStatic = art_quick_get_obj_static;

  // Array
  qpoints->pAputObjectWithNullAndBoundCheck = art_quick_aput_obj_with_null_and_bound_check;
  qpoints->pAputObjectWithBoundCheck = art_quick_aput_obj_with_bound_check;
  qpoints->pAputObject = art_quick_aput_obj;
  qpoints->pHandleFillArrayData = art_quick_handle_fill_data;

  // JNI
#ifndef MOE
  qpoints->pJniMethodStart = JniMethodStart;
  qpoints->pJniMethodStartSynchronized = JniMethodStartSynchronized;
  qpoints->pJniMethodEnd = JniMethodEnd;
  qpoints->pJniMethodEndSynchronized = JniMethodEndSynchronized;
  qpoints->pJniMethodEndWithReference = JniMethodEndWithReference;
  qpoints->pJniMethodEndWithReferenceSynchronized = JniMethodEndWithReferenceSynchronized;
#else
  qpoints->pJniMethodStart = art_jni_method_start;
  qpoints->pJniMethodStartSynchronized = art_jni_method_start_synchronized;
  qpoints->pJniMethodEnd = art_jni_method_end;
  qpoints->pJniMethodEndSynchronized = art_jni_method_end_synchronized;
  qpoints->pJniMethodEndWithReference = art_jni_method_end_with_reference;
  qpoints->pJniMethodEndWithReferenceSynchronized = art_jni_method_end_with_reference_synchronized;
#endif
  qpoints->pQuickGenericJniTrampoline = art_quick_generic_jni_trampoline;

  // Locks
  qpoints->pLockObject = art_quick_lock_object;
  qpoints->pUnlockObject = art_quick_unlock_object;

  // Math
  qpoints->pIdivmod = __aeabi_idivmod;
  qpoints->pLdiv = __aeabi_ldivmod;
  qpoints->pLmod = __aeabi_ldivmod;  // result returned in r2:r3
  qpoints->pLmul = art_quick_mul_long;
  qpoints->pShlLong = art_quick_shl_long;
  qpoints->pShrLong = art_quick_shr_long;
  qpoints->pUshrLong = art_quick_ushr_long;
  if (kArm32QuickCodeUseSoftFloat) {
    qpoints->pFmod = fmod;
    qpoints->pFmodf = fmodf;
    qpoints->pD2l = art_d2l;
    qpoints->pF2l = art_f2l;
    qpoints->pL2f = art_l2f;
  } else {
    qpoints->pFmod = art_quick_fmod;
    qpoints->pFmodf = art_quick_fmodf;
    qpoints->pD2l = art_quick_d2l;
    qpoints->pF2l = art_quick_f2l;
    qpoints->pL2f = art_quick_l2f;
  }

  // Intrinsics
  qpoints->pIndexOf = art_quick_indexof;
  qpoints->pStringCompareTo = art_quick_string_compareto;
#ifndef MOE
  qpoints->pMemcpy = memcpy;
#else
  qpoints->pMemcpy = art_memcpy;
#endif

  // Invocation
  qpoints->pQuickImtConflictTrampoline = art_quick_imt_conflict_trampoline;
  qpoints->pQuickResolutionTrampoline = art_quick_resolution_trampoline;
  qpoints->pQuickToInterpreterBridge = art_quick_to_interpreter_bridge;
  qpoints->pInvokeDirectTrampolineWithAccessCheck =
      art_quick_invoke_direct_trampoline_with_access_check;
  qpoints->pInvokeInterfaceTrampolineWithAccessCheck =
      art_quick_invoke_interface_trampoline_with_access_check;
  qpoints->pInvokeStaticTrampolineWithAccessCheck =
      art_quick_invoke_static_trampoline_with_access_check;
  qpoints->pInvokeSuperTrampolineWithAccessCheck =
      art_quick_invoke_super_trampoline_with_access_check;
  qpoints->pInvokeVirtualTrampolineWithAccessCheck =
      art_quick_invoke_virtual_trampoline_with_access_check;

  // Thread
  qpoints->pTestSuspend = art_quick_test_suspend;

  // Throws
  qpoints->pDeliverException = art_quick_deliver_exception;
  qpoints->pThrowArrayBounds = art_quick_throw_array_bounds;
  qpoints->pThrowDivZero = art_quick_throw_div_zero;
  qpoints->pThrowNoSuchMethod = art_quick_throw_no_such_method;
  qpoints->pThrowNullPointer = art_quick_throw_null_pointer_exception;
  qpoints->pThrowStackOverflow = art_quick_throw_stack_overflow;

  // Deoptimization from compiled code.
  qpoints->pDeoptimize = art_quick_deoptimize_from_compiled_code;

  // Read barrier
#ifndef MOE
  qpoints->pReadBarrierJni = ReadBarrierJni;
  qpoints->pReadBarrierSlow = artReadBarrierSlow;
#else
  qpoints->pReadBarrierJni = art_read_barrier_jni;
  qpoints->pReadBarrierSlow = art_read_barrier_slow;
#endif
}

}  // namespace art
