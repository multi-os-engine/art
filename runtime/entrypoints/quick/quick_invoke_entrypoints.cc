/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include "callee_save_frame.h"
#include "dex_instruction-inl.h"
#include "entrypoints/entrypoint_utils.h"
#include "mirror/art_method-inl.h"
#include "mirror/class-inl.h"
#include "mirror/dex_cache-inl.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"

namespace art {

template<InvokeType type, bool access_check>
uint64_t artInvokeCommon(uint32_t method_idx, mirror::Object* this_object,
                         mirror::ArtMethod* caller_method,
                         Thread* self, mirror::ArtMethod** sp) {
  mirror::ArtMethod* method = FindMethodFast(method_idx, this_object, caller_method, access_check,
                                             type);
  if (UNLIKELY(method == NULL)) {
    FinishCalleeSaveFrameSetup(self, sp, Runtime::kRefsAndArgs);
    method = FindMethodFromCode<type, access_check>(method_idx, this_object, caller_method, self);
    if (UNLIKELY(method == NULL)) {
      CHECK(self->IsExceptionPending());
      return 0;  // failure
    }
  }
  DCHECK(!self->IsExceptionPending());
  const void* code = method->GetEntryPointFromQuickCompiledCode();

  // When we return, the caller will branch to this address, so it had better not be 0!
  if (kIsDebugBuild && UNLIKELY(code == NULL)) {
      MethodHelper mh(method);
      LOG(FATAL) << "Code was NULL in method: " << PrettyMethod(method)
                 << " location: " << mh.GetDexFile().GetLocation();
  }
#ifdef __LP64__
  UNIMPLEMENTED(FATAL);
  return 0;
#else
  uint32_t method_uint = reinterpret_cast<uint32_t>(method);
  uint64_t code_uint = reinterpret_cast<uint32_t>(code);
  uint64_t result = ((code_uint << 32) | method_uint);
  return result;
#endif
}

// Explicit template declarations of artInvokeCommon for all invoke types.
#define EXPLICIT_ART_INVOKE_COMMON_TEMPLATE_DECL(_type, _access_check)                 \
  template SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)                                 \
  uint64_t artInvokeCommon<_type, _access_check>(uint32_t method_idx,                  \
                                                 mirror::Object* this_object,          \
                                                 mirror::ArtMethod* caller_method,     \
                                                 Thread* self, mirror::ArtMethod** sp)

#define EXPLICIT_ART_INVOKE_COMMON_TYPED_TEMPLATE_DECL(_type) \
    EXPLICIT_ART_INVOKE_COMMON_TEMPLATE_DECL(_type, false);   \
    EXPLICIT_ART_INVOKE_COMMON_TEMPLATE_DECL(_type, true)

EXPLICIT_ART_INVOKE_COMMON_TYPED_TEMPLATE_DECL(kStatic);
EXPLICIT_ART_INVOKE_COMMON_TYPED_TEMPLATE_DECL(kDirect);
EXPLICIT_ART_INVOKE_COMMON_TYPED_TEMPLATE_DECL(kVirtual);
EXPLICIT_ART_INVOKE_COMMON_TYPED_TEMPLATE_DECL(kSuper);
EXPLICIT_ART_INVOKE_COMMON_TYPED_TEMPLATE_DECL(kInterface);

#undef EXPLICIT_ART_INVOKE_COMMON_TYPED_TEMPLATE_DECL
#undef EXPLICIT_ART_INVOKE_COMMON_TEMPLATE_DECL

// See comments in runtime_support_asm.S
extern "C" uint64_t artInvokeInterfaceTrampolineWithAccessCheck(uint32_t method_idx,
                                                                mirror::Object* this_object,
                                                                mirror::ArtMethod* caller_method,
                                                                Thread* self,
                                                                mirror::ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return artInvokeCommon<kInterface, true>(method_idx, this_object, caller_method, self, sp);
}


extern "C" uint64_t artInvokeDirectTrampolineWithAccessCheck(uint32_t method_idx,
                                                             mirror::Object* this_object,
                                                             mirror::ArtMethod* caller_method,
                                                             Thread* self,
                                                             mirror::ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return artInvokeCommon<kDirect, true>(method_idx, this_object, caller_method, self, sp);
}

extern "C" uint64_t artInvokeStaticTrampolineWithAccessCheck(uint32_t method_idx,
                                                             mirror::Object* this_object,
                                                             mirror::ArtMethod* caller_method,
                                                             Thread* self,
                                                             mirror::ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return artInvokeCommon<kStatic, true>(method_idx, this_object, caller_method, self, sp);
}

extern "C" uint64_t artInvokeSuperTrampolineWithAccessCheck(uint32_t method_idx,
                                                            mirror::Object* this_object,
                                                            mirror::ArtMethod* caller_method,
                                                            Thread* self,
                                                            mirror::ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return artInvokeCommon<kSuper, true>(method_idx, this_object, caller_method, self, sp);
}

extern "C" uint64_t artInvokeVirtualTrampolineWithAccessCheck(uint32_t method_idx,
                                                              mirror::Object* this_object,
                                                              mirror::ArtMethod* caller_method,
                                                              Thread* self,
                                                              mirror::ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  return artInvokeCommon<kVirtual, true>(method_idx, this_object, caller_method, self, sp);
}

}  // namespace art
