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

#include "art_method.h"

namespace art {

// Assembly stub that does the final part of the up-call into Java.
extern "C" void art_quick_invoke_stub_internal(ArtMethod*, uint32_t*, uint32_t, Thread* self,
                                               JValue* result, uint32_t, uint32_t*, uint32_t*);

template <bool kIsStatic>
static void quick_invoke_reg_setup(ArtMethod* method, uint32_t* args, uint32_t args_size,
                                   Thread* self, JValue* result, const char* shorty) {
  // Note: We do not follow o32 ABI in quick code.
  constexpr uint32_t kMaxNumberOfFpArgs = 2;
  uint32_t core_reg_args[4];  // $a0 ~ $a3
  uint32_t fpu_reg_args[4];   // $f12 ~ $f15 (R2) or $f12 & $f14 (R6)
  uint32_t gpr_index = 1;     // Reserve a0 for ArtMethod*.
  uint32_t fpr_index = 0;
  uint32_t arg_index = 0;
  const uint32_t result_in_float = (shorty[0] == 'F' || shorty[0] == 'D') ? 1 : 0;

  if (!kIsStatic) {
    // Copy receiver for non-static methods.
    core_reg_args[gpr_index++] = args[arg_index++];
  }

  for (uint32_t shorty_index = 1; shorty[shorty_index] != '\0'; ++shorty_index, ++arg_index) {
    char arg_type = shorty[shorty_index];
    switch (arg_type) {
      case 'D':
        if (fpr_index < kMaxNumberOfFpArgs) {
          fpu_reg_args[2 * fpr_index] = args[arg_index];
          fpu_reg_args[2 * fpr_index++ + 1] = args[arg_index + 1];
        }
        arg_index++;
        break;
      case 'F':
        if (fpr_index < kMaxNumberOfFpArgs) {
          fpu_reg_args[2 * fpr_index++] = args[arg_index];
        }
        break;
      case 'J':
        if (gpr_index < arraysize(core_reg_args)) {
          core_reg_args[gpr_index++] = args[arg_index];
        }
        ++arg_index;
        FALLTHROUGH_INTENDED;  // Fall-through to take of the high part.
      default:
        if (gpr_index < arraysize(core_reg_args)) {
          core_reg_args[gpr_index++] = args[arg_index];
        }
        break;
    }
  }

  art_quick_invoke_stub_internal(method, args, args_size, self, result, result_in_float,
      core_reg_args, fpu_reg_args);
}

// Called by art::ArtMethod::Invoke to do entry into a non-static method.
// TODO: migrate into an assembly implementation as with ARM64.
extern "C" void art_quick_invoke_stub(ArtMethod* method, uint32_t* args, uint32_t args_size,
                                      Thread* self, JValue* result, const char* shorty) {
  quick_invoke_reg_setup<false>(method, args, args_size, self, result, shorty);
}

// Called by art::ArtMethod::Invoke to do entry into a static method.
// TODO: migrate into an assembly implementation as with ARM64.
extern "C" void art_quick_invoke_static_stub(ArtMethod* method, uint32_t* args,
                                             uint32_t args_size, Thread* self, JValue* result,
                                             const char* shorty) {
  quick_invoke_reg_setup<true>(method, args, args_size, self, result, shorty);
}

}  // namespace art
