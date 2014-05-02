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

#include "method_frame_info_arm64.h"

#include "base/macros.h"
#include "primitive.h"
#include "stack_indirect_reference_table.h"

namespace art {
namespace arm64 {

constexpr size_t kFramePointerSize = 8;

MethodFrameInfo Arm64JniMethodFrameInfo(bool is_static, const char* shorty) {
  constexpr uint32_t core_spill_mask =
      1 << X19 | 1 << X20 | 1 << X21 | 1 << X22 | 1 << X23 | 1 << X24 |
      1 << X25 | 1 << X26 | 1 << X27 | 1 << X28 | 1 << X29 | 1 << LR;
  constexpr uint32_t fp_spill_mask =
      1 << D8 | 1 << D9 | 1 << D10 | 1 << D11 | 1 << D12 | 1 << D13 |
      1 << D14 | 1 << D15;

  // Method*, LR and callee save area size, local reference segment state.
  COMPILE_ASSERT((core_spill_mask & (1u << LR)) != 0u, core_spills_must_contain_lr);
  constexpr size_t frame_data_size =
      (1 + POPCOUNT(core_spill_mask) + POPCOUNT(fp_spill_mask)) * kFramePointerSize +
      sizeof(uint32_t);
  // References plus 2 words for SIRT header
  size_t ref_count = 1u;  // First argument is object or class reference.
  for (const char* param = shorty + 1; *param != 0; ++param) {
    if (*param == 'L') {
      ++ref_count;
    }
  }
  size_t sirt_size = StackIndirectReferenceTable::GetAlignedSirtSizeTarget(kFramePointerSize,
                                                                           ref_count);
  // Plus return value spill area size
  size_t return_value_size = Primitive::ComponentSize(Primitive::GetType(shorty[0]));
  if (return_value_size >= 1 && return_value_size < 4) {
    return_value_size = 4;
  }
  size_t frame_size = RoundUp(frame_data_size + sirt_size + return_value_size, kStackAlignment);

  return MethodFrameInfo(frame_size, core_spill_mask, fp_spill_mask);
}

}  // namespace arm64
}  // namespace art
