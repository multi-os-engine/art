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

#ifndef ART_COMPILER_DEX_QUICK_LAZY_DEBUG_FRAME_OPCODE_WRITER_H_
#define ART_COMPILER_DEX_QUICK_LAZY_DEBUG_FRAME_OPCODE_WRITER_H_

#include "base/arena_allocator.h"
#include "base/arena_containers.h"
#include "dwarf/debug_frame_opcode_writer.h"

namespace art {
struct LIR;
namespace dwarf {

// When we are generating the CFI code, we do not know the instuction offsets,
// this class stores the LIR references and patches the instruction stream later.
class LazyDebugFrameOpCodeWriter FINAL
    : private DebugFrameOpCodeWriter<ArenaAllocatorAdapter<uint8_t>> {
  typedef DebugFrameOpCodeWriter<ArenaAllocatorAdapter<uint8_t>> Base;
 public:
  // This method is implicitely called the by opcode writers.
  virtual void ImplicitlyAdvancePC() OVERRIDE {
    DCHECK_EQ(patched_, false);
    DCHECK_EQ(this->current_pc_, 0);
    advances_.push_back({this->data()->size(), *last_lir_insn_});
  }

  // The register was unspilled.
  void Restore(Reg reg) {
    if (enable_writes_) {
      Base::Restore(reg);
    }
  }

  // Custom alias - unspill many registers based on bitmask.
  void Restores(Reg reg_base, uint32_t reg_mask) {
    if (enable_writes_) {
      Base::Restores(reg_base, reg_mask);
    }
  }

  // Remember the state of register spills.
  void RememberState() {
    if (enable_writes_) {
      Base::RememberState();
    }
  }

  // Restore the state of register spills.
  void RestoreState() {
    if (enable_writes_) {
      Base::RestoreState();
    }
  }

  // Set the frame pointer (CFA) to (stack_pointer + offset).
  void DefCFAOffset(int offset) {
    if (enable_writes_) {
      Base::DefCFAOffset(offset);
    }
    this->current_cfa_offset_ = offset;
  }

  // The stack size was increased by given delta.
  void AdjustCFAOffset(int delta) {
    DefCFAOffset(this->current_cfa_offset_ + delta);
  }

  // The register was spilled to (stack_pointer + offset).
  void RelOffset(Reg reg, int offset) {
    if (enable_writes_) {
      Base::RelOffset(reg, offset);
    }
  }

  // Custom alias - spill many registers based on bitmask.
  void RelOffsets(Reg reg_base, int offset, uint32_t reg_mask, int reg_size) {
    if (enable_writes_) {
      Base::RelOffsets(reg_base, offset, reg_mask, reg_size);
    }
  }

  using Base::current_cfa_offset;
  using Base::set_current_cfa_offset;

  const ArenaVector<uint8_t>* Patch();

  explicit LazyDebugFrameOpCodeWriter(LIR** last_lir_insn, bool enable_writes,
                                      ArenaAllocator* allocator)
    : Base(allocator->Adapter()),
      last_lir_insn_(last_lir_insn),
      enable_writes_(enable_writes),
      advances_(allocator->Adapter()),
      patched_(false) {
  }

 private:
  typedef struct {
    size_t pos;
    LIR* last_lir_insn;
  } Advance;

  LIR** last_lir_insn_;
  bool enable_writes_;
  ArenaVector<Advance> advances_;
  bool patched_;

  DISALLOW_COPY_AND_ASSIGN(LazyDebugFrameOpCodeWriter);
};

}  // namespace dwarf
}  // namespace art

#endif  // ART_COMPILER_DEX_QUICK_LAZY_DEBUG_FRAME_OPCODE_WRITER_H_
