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

#include "mirror/art_method.h"

using ::art::mirror::ArtMethod;
using ::art::JValue;
using ::art::Thread;
using ::art::RoundUp;

extern "C" void art_quick_invoke_stub_internal(ArtMethod*, uint32_t*, uint32_t, Thread* self,
                                               JValue* result, uint32_t, uint32_t*, uint32_t*);

template <bool is_static>
static void _art_quick_invoke_stub(ArtMethod* method, uint32_t* args, uint32_t args_size,
                                   Thread* self, JValue* result, const char* shorty) {
  // Note: We do not follow aapcs ABI in quick code for both softfp and hardfp.
  uint32_t core_reg_args[4];  // r0 ~ r3
  uint32_t fp_reg_args[16];  // s0 ~ s15 (d0 ~ d7)
  uint32_t gpr_index = 1;  // Reserve r0 for ArtMethod*.
  uint32_t fpr_index = 0;
  uint32_t fpr_double_index = 0;
  uint32_t arg_index = 0;
  uint32_t result_in_float = (*shorty == 'F' || *shorty == 'D') ? 1 : 0;

#ifdef ARM32_QUICKCODE_USE_SOFTFP
  result_in_float = 0;
#endif

  if (!is_static) {
    core_reg_args[gpr_index++] = args[arg_index++];
  }

  for (++shorty; *shorty; ++shorty, ++arg_index) {
    switch (*shorty) {
#ifdef ARM32_QUICKCODE_USE_SOFTFP
      case 'D':
        // Intentional fall-through.
#else
      case 'D':
        // Double should not overlap with float.
        fpr_double_index = std::max(fpr_double_index, RoundUp(fpr_index, 2));
        if (fpr_double_index < arraysize(fp_reg_args)) {
          fp_reg_args[fpr_double_index++] = args[arg_index];
          fp_reg_args[fpr_double_index++] = args[arg_index + 1];
        }
        ++arg_index;
        break;
      case 'F':
        // Float should not overlap with double.
        if (fpr_index % 2 == 0) {
          fpr_index = std::max(fpr_double_index, fpr_index);
        }
        if (fpr_index < arraysize(fp_reg_args)) {
          fp_reg_args[fpr_index++] = args[arg_index];
        }
        break;
#endif
      case 'J':
        if (gpr_index < arraysize(core_reg_args)) {
          core_reg_args[gpr_index++] = args[arg_index];
        }
        ++arg_index;
        // Intentional fall-through.
      default:
        if (gpr_index < arraysize(core_reg_args)) {
          core_reg_args[gpr_index++] = args[arg_index];
        }
        break;
    }
  }

  art_quick_invoke_stub_internal(method, args, args_size, self, result, result_in_float,
      core_reg_args, fp_reg_args);
}

extern "C" void art_quick_invoke_stub(ArtMethod* method, uint32_t* args, uint32_t args_size,
                                      Thread* self, JValue* result, const char* shorty) {
  _art_quick_invoke_stub<false>(method, args, args_size, self, result, shorty);
}

extern "C" void art_quick_invoke_static_stub(ArtMethod* method, uint32_t* args, uint32_t args_size,
                                             Thread* self, JValue* result, const char* shorty) {
  _art_quick_invoke_stub<true>(method, args, args_size, self, result, shorty);
}
