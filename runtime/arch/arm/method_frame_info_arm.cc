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

#include "method_frame_info_arm.h"

#include "base/macros.h"
#include "primitive.h"
#include "stack_indirect_reference_table.h"

namespace art {
namespace arm {

constexpr size_t kFramePointerSize = 4;

MethodFrameInfo ArmJniMethodFrameInfo(bool is_static, const char* shorty) {
  constexpr uint32_t core_spills =
      1 << R5 | 1 << R6 | 1 << R7 | 1 << R8 | 1 << R10 | 1 << R11 | 1 << LR;

  // Method*, LR and callee save area size, local reference segment state.
  COMPILE_ASSERT((core_spills & (1u << LR)) != 0u, core_spills_must_contain_lr);
  constexpr size_t frame_data_size = (2 + POPCOUNT(core_spills)) * kFramePointerSize;
  // References plus 2 words for SIRT header
  size_t ref_count = 1u;  // First argument is object or class reference.
  for (const char* param = shorty + 1; *param != 0; ++param) {
    if (*param == 'L') {
      ++ref_count;
    }
  }
  const size_t sirt_size =
      StackIndirectReferenceTable::GetAlignedSirtSizeTarget(kFramePointerSize, ref_count);
  // Plus return value spill area size
  size_t return_value_size = Primitive::ComponentSize(Primitive::GetType(shorty[0]));
  if (return_value_size >= 1 && return_value_size < 4) {
    return_value_size = 4;
  }
  size_t frame_size = RoundUp(frame_data_size + sirt_size + return_value_size, kStackAlignment);

  return MethodFrameInfo(frame_size, core_spills, 0u);
}

}  // namespace arm
}  // namespace art
