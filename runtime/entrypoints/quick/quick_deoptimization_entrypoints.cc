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

#include "art_method-inl.h"
#include "base/logging.h"
#include "callee_save_frame.h"
#include "dex_file-inl.h"
#include "interpreter/interpreter.h"
#include "mirror/class-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/object-inl.h"
#include "quick_exception_handler.h"
#include "stack.h"
#include "thread.h"
#include "verifier/method_verifier.h"

namespace art {

extern "C" NO_RETURN void artDeoptimize(Thread* self) SHARED_REQUIRES(Locks::mutator_lock_) {
  ScopedQuickEntrypointChecks sqec(self);

  if (VLOG_IS_ON(deopt)) {
    LOG(INFO) << "Deopting:";
    self->Dump(LOG(INFO));
  }

  self->AssertHasDeoptimizationContext();
  self->SetException(Thread::GetDeoptimizationException());
  self->QuickDeliverException();
}

// Do single frame deoptimization.
// `from_code` whether it's triggered from compiled code as a result of HDeoptimize.
// `grp_result` integer return result.
// `fpr_result` floating-point return result.
NO_RETURN void artDeoptimizeSingleFrame(bool from_code,
                                        uint64_t gpr_result,
                                        uint64_t fpr_result)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  Thread* self = Thread::Current();
  ScopedQuickEntrypointChecks sqec(self);

  // Deopt logging will be in DeoptimizeSingleFrame. It is there to take advantage of the
  // specialized visitor that will show whether a method is Quick or Shadow.

  QuickExceptionHandler exception_handler(self, true);
  const char* shorty = exception_handler.DeoptimizeSingleFrame(from_code);
  exception_handler.UpdateInstrumentationStack();
  exception_handler.DeoptimizeSingleFrameArchDependentFixup();

  // Before deoptimizing to interpreter, we must push the deoptimization context.
  JValue return_value;
  if (shorty == nullptr || (shorty[0] != 'F' && shorty[0] != 'D')) {
    // int type.
    return_value.SetJ(gpr_result);
  } else {
    // float type.
    return_value.SetJ(fpr_result);
  }
  self->PushDeoptimizationContext(return_value,
                                  shorty != nullptr && shorty[0] == 'J',
                                  from_code,
                                  self->GetException());

  // We cannot smash the caller-saves, as we need the ArtMethod in a parameter register that would
  // be caller-saved. This has the downside that we cannot track incorrect register usage down the
  // line.
  exception_handler.DoLongJump(false);
}

extern "C" NO_RETURN void artDeoptimizeFromCompiledCode()
    SHARED_REQUIRES(Locks::mutator_lock_) {
  // We never deoptimize from compiled code with an invocation result.
  artDeoptimizeSingleFrame(true, 0, 0);
}

extern "C" NO_RETURN void artDeoptimizeWhenReturnedTo(uint64_t gpr_result, uint64_t fpr_result)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  artDeoptimizeSingleFrame(false, gpr_result, fpr_result);
}
}  // namespace art
