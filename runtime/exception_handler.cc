/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include "exception_handler.h"

#include "arch/context.h"
#include "dex_instruction.h"
#include "entrypoints/entrypoint_utils.h"
#include "entrypoints/runtime_asm_entrypoints.h"
#include "handle_scope-inl.h"
#include "mirror/art_method-inl.h"
#include "mirror/class-inl.h"
#include "mirror/class_loader.h"
#include "mirror/throwable.h"
#include "quick_exception_handler.h"
#include "verifier/method_verifier.h"

namespace art {

bool CatchBlockStackVisitor::VisitFrame() {
  mirror::ArtMethod* method = GetMethod();
  exception_handler_->SetHandlerFrameDepth(GetFrameDepth());
  if (method == nullptr) {
    // This is the upcall, we remember the frame and last pc so that we may long jump to them.
    exception_handler_->SetHandlerQuickFramePc(GetCurrentQuickFramePc());
    exception_handler_->SetHandlerQuickFrame(GetCurrentQuickFrame());
    uint32_t next_dex_pc;
    mirror::ArtMethod* next_art_method;
    bool has_next = GetNextMethodAndDexPc(&next_art_method, &next_dex_pc);
    // Report the method that did the down call as the handler.
    exception_handler_->SetHandlerDexPc(next_dex_pc);
    exception_handler_->SetHandlerMethod(next_art_method);
    if (!has_next) {
      // No next method? Check exception handler is set up for the unhandled exception handler
      // case.
      DCHECK_EQ(0U, exception_handler_->GetHandlerDexPc());
      DCHECK(nullptr == exception_handler_->GetHandlerMethod());
    }
    return false;  // End stack walk.
  }
  if (method->IsRuntimeMethod()) {
    // Ignore callee save method.
    DCHECK(method->IsCalleeSaveMethod());
    return true;
  }
  StackHandleScope<1> hs(self_);

  bool found_catch = false;
  if (method->IsOptimized(sizeof(void*))) {
    found_catch = LookForCatchOptimizing(hs.NewHandle(method));
  } else {
    found_catch = LookForCatch(hs.NewHandle(method));
  }

  // We return true to continue walking stack - thus only do it in case no catch is found.
  return !found_catch;
}

bool CatchBlockStackVisitor::LookForCatch(Handle<mirror::ArtMethod> method) {
  uint32_t dex_pc = DexFile::kDexNoIndex;
  if (!method->IsNative()) {
    dex_pc = GetDexPc();
  }
  if (dex_pc != DexFile::kDexNoIndex) {
    bool clear_exception = false;
    StackHandleScope<1> hs(Thread::Current());
    Handle<mirror::Class> to_find(hs.NewHandle((*exception_)->GetClass()));
    uint32_t found_dex_pc = mirror::ArtMethod::FindCatchBlock(method, to_find, dex_pc,
                                                              &clear_exception);
    exception_handler_->SetClearException(clear_exception);
    if (found_dex_pc != DexFile::kDexNoIndex) {
      exception_handler_->SetHandlerMethod(method.Get());
      exception_handler_->SetHandlerDexPc(found_dex_pc);
      exception_handler_->SetHandlerQuickFramePc(method->ToNativeQuickPc(found_dex_pc));
      exception_handler_->SetHandlerQuickFrame(GetCurrentQuickFrame());
      return true;  // End stack walk.
    }
  }
  return false;  // Continue stack walk.
}


bool CatchBlockStackVisitor::LookForCatchOptimizing(Handle<mirror::ArtMethod> method) {
  DCHECK_EQ(method->IsOptimized(sizeof(void*)), true);

  CodeInfo code_info = method->GetOptimizedCodeInfo();
  StackMap stack_map = code_info.GetStackMapForNativePcOffset(GetNativePcOffset());

  // Only need to look in this method if current location has catch information.
  if (stack_map.HasCatchInfo(code_info)) {
    CatchInfo catch_info = code_info.GetCatchInfoOf(stack_map);
    uint16_t number_of_catches = catch_info.GetNumberOfCatches();
    DCHECK_NE(number_of_catches, 0u);

    // Set aside the exception while we resolve its type.
    Thread* self = Thread::Current();
    StackHandleScope<1> hs(self);
    Handle<mirror::Throwable> exception(hs.NewHandle(self->GetException()));
    self->ClearException();

    Handle<mirror::Class> exception_type(hs.NewHandle((*exception_)->GetClass()));

    // Now walk through all of the catches to check if current exception type is handled there.
    uint16_t catch_idx;
    for (catch_idx = 0u; catch_idx != number_of_catches; catch_idx++) {
      uint16_t type_idx = catch_info.GetTypeIndexCaught(catch_idx);

      // Handle the catch-all case.
      if (type_idx == DexFile::kDexNoIndex16) {
        break;
      }

      // Now check that this exception type applies.
      mirror::Class* handled_exception_type = method->GetClassFromTypeIndex(type_idx, true);
      if (UNLIKELY(handled_exception_type == nullptr)) {
        // Now have a NoClassDefFoundError as exception. Ignore in case the exception class was
        // removed by a pro-guard like tool.
        // Note: this is not RI behavior. RI would have failed when loading the class.
        self->ClearException();
        // Delete any long jump context as this routine is called during a stack walk which will
        // release its in use context at the end.
        delete self->GetLongJumpContext();
        LOG(WARNING) << "Unresolved exception class when finding catch block: "
          << DescriptorToDot(method->GetTypeDescriptorFromTypeIdx(type_idx));
      } else if (handled_exception_type->IsAssignableFrom(exception_type.Get())) {
        break;
      }
    }

    // Put the exception back.
    if (exception.Get() != nullptr) {
      self->SetException(exception.Get());
    }

    // If an appropriate catch was found, prepare exception for being handled there.
    if (catch_idx != number_of_catches) {
      uint16_t type_idx = catch_info.GetTypeIndexCaught(catch_idx);
      // TODO Check if this is right:
      uintptr_t native_pc = reinterpret_cast<uintptr_t>(method->GetQuickOatCodePointer(sizeof(void*))) +
          catch_info.GetNativePcOffset(code_info, type_idx);

      exception_handler_->SetClearException(catch_info.ClearsException(type_idx));
      exception_handler_->SetHandlerMethod(method.Get());
      exception_handler_->SetHandlerDexPc(catch_info.GetDexPc(code_info, type_idx));
      exception_handler_->SetHandlerQuickFramePc(native_pc);
      exception_handler_->SetHandlerQuickFrame(GetCurrentQuickFrame());
      return true;  // End stack walk.
    }
  }

  // There is no catch block in this method so continue walking.
  return false;
}
}  // namespace art
