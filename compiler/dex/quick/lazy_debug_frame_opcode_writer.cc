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

#include "lazy_debug_frame_opcode_writer.h"
#include "mir_to_lir.h"

namespace art {
namespace dwarf {

const std::vector<uint8_t>* LazyDebugFrameOpCodeWriter::Patch() {
  if (!patched) {
    patched = true;
    // Move our data buffer to temporary variable
    std::vector<uint8_t> old_opcodes;
    old_opcodes.swap(opcodes_);
    // Refill our data buffer with patched opcodes
    data_->reserve(old_opcodes.size() + advances.size() + 4);
    size_t pos = 0;
    for (auto advance : advances) {
      DCHECK_GE(advance.pos, pos);
      // Copy old data up to the point when advance was issued
      for (; pos < advance.pos; ++pos) {
        PushUint8(old_opcodes[pos]);
      }
      if (NEXT_LIR(advance.last_lir_insn) == nullptr) {
        // This may happen if there is no slow-path code after return.
        return data_;  // Ignore the rest of opcodes.
      }
      // Insert the advance command with its final offset
      DebugFrameOpCodeWriter::AdvancePC(NEXT_LIR(advance.last_lir_insn)->offset);
    }
    // Copy the final segment
    for (; pos < old_opcodes.size(); ++pos) {
      PushUint8(old_opcodes[pos]);
    }
  }
  return data_;
}

}  // namespace dwarf
}  // namespace art
