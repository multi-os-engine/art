/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "linker/mips/relative_patcher_mips_base.h"

namespace art {
namespace linker {

uint32_t MipsBaseRelativePatcher::ReserveSpace(
    uint32_t offset,
    const CompiledMethod* compiled_method ATTRIBUTE_UNUSED,
    MethodReference method_ref ATTRIBUTE_UNUSED) {
  return offset;  // No space reserved; no limit on relative call distance.
}

uint32_t MipsBaseRelativePatcher::ReserveSpaceEnd(uint32_t offset) {
  return offset;  // No space reserved; no limit on relative call distance.
}

uint32_t MipsBaseRelativePatcher::WriteThunks(OutputStream* out ATTRIBUTE_UNUSED, uint32_t offset) {
  return offset;  // No thunks added; no limit on relative call distance.
}

void MipsBaseRelativePatcher::PatchCall(std::vector<uint8_t>* code ATTRIBUTE_UNUSED,
                                        uint32_t literal_offset ATTRIBUTE_UNUSED,
                                        uint32_t patch_offset ATTRIBUTE_UNUSED,
                                        uint32_t target_offset ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL) << "PatchCall unimplemented on MIPS";
}

}  // namespace linker
}  // namespace art
