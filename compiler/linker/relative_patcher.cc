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

#include "linker/relative_patcher.h"

#include "linker/arm/relative_patcher_thumb2.h"
#include "linker/arm64/relative_patcher_arm64.h"
#include "linker/x86/relative_patcher_x86.h"
#include "linker/x86_64/relative_patcher_x86_64.h"
#include "linker/mips/relative_patcher_mips.h"
#include "output_stream.h"

namespace art {
namespace linker {

std::unique_ptr<RelativePatcher> RelativePatcher::Create(
    InstructionSet instruction_set, const InstructionSetFeatures* features,
    RelativePatcherTargetProvider* provider) {
  return std::unique_ptr<RelativePatcher>(
              CreateRelativePatcher(instruction_set, provider, features));
}

bool RelativePatcher::WriteCodeAlignment(OutputStream* out, uint32_t aligned_code_delta) {
  static const uint8_t kPadding[] = {
      0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u
  };
  DCHECK_LE(aligned_code_delta, sizeof(kPadding));
  if (UNLIKELY(!out->WriteFully(kPadding, aligned_code_delta))) {
    return false;
  }
  size_code_alignment_ += aligned_code_delta;
  return true;
}

bool RelativePatcher::WriteRelCallThunk(OutputStream* out, const ArrayRef<const uint8_t>& thunk) {
  if (UNLIKELY(!out->WriteFully(thunk.data(), thunk.size()))) {
    return false;
  }
  size_relative_call_thunks_ += thunk.size();
  return true;
}

bool RelativePatcher::WriteMiscThunk(OutputStream* out, const ArrayRef<const uint8_t>& thunk) {
  if (UNLIKELY(!out->WriteFully(thunk.data(), thunk.size()))) {
    return false;
  }
  size_misc_thunks_ += thunk.size();
  return true;
}

}  // namespace linker
}  // namespace art
