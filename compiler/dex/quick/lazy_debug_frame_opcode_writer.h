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

#ifndef ART_COMPILER_DEX_QUICK_LAZY_DEBUG_FRAME_OPCODE_WRITER_H_
#define ART_COMPILER_DEX_QUICK_LAZY_DEBUG_FRAME_OPCODE_WRITER_H_

#include "dwarf/debug_frame_opcode_writer.h"

namespace art {
struct LIR;
namespace dwarf {

// When we are generating the CFI code, we do not know the instuction offests,
// this class stores the LIR references and patches the instruction stream later.
class LazyDebugFrameOpCodeWriter : protected DebugFrameOpCodeWriter {
 public:
  void Restore(int reg) {
    LazyAdvancePC();
    DebugFrameOpCodeWriter::Restore(0, reg);
  }

  void RememberState() {
    // No need to advance PC
    DebugFrameOpCodeWriter::RememberState();
  }

  void RestoreState() {
    LazyAdvancePC();
    DebugFrameOpCodeWriter::RestoreState(0);
  }

  void DefCFAOffset(int offset) {
    LazyAdvancePC();
    DebugFrameOpCodeWriter::DefCFAOffset(0, offset);
  }

  void RelOffset(int reg, int offset) {
    LazyAdvancePC();
    DebugFrameOpCodeWriter::RelOffset(0, reg, offset);
  }

  void AdjustCFAOffset(int delta) {
    LazyAdvancePC();
    DebugFrameOpCodeWriter::AdjustCFAOffset(0, delta);
  }

  using DebugFrameOpCodeWriter::current_cfa_offset;
  using DebugFrameOpCodeWriter::set_current_cfa_offset;

  const std::vector<uint8_t>* Patch();

  explicit LazyDebugFrameOpCodeWriter(LIR** last_lir_insn)
    : last_lir_insn_(last_lir_insn),
      patched(false) {
  }

 protected:
  typedef struct {
    size_t pos;
    LIR* last_lir_insn;
  } Advance;

  void LazyAdvancePC() {
    DCHECK_EQ(patched, false);
    DCHECK_EQ(current_pc_, 0);
    advances.push_back({data_->size(), *last_lir_insn_});
  }

  LIR** last_lir_insn_;
  std::vector<Advance> advances;
  bool patched;
};

}  // namespace dwarf
}  // namespace art

#endif  // ART_COMPILER_DEX_QUICK_LAZY_DEBUG_FRAME_OPCODE_WRITER_H_
