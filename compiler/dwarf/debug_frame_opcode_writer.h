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

#ifndef ART_COMPILER_DWARF_DEBUG_FRAME_OPCODE_WRITER_H_
#define ART_COMPILER_DWARF_DEBUG_FRAME_OPCODE_WRITER_H_

#include "enums.h"
#include "writer.h"

namespace art {
namespace dwarf {

// Writer for .debug_frame opcodes (DWARF-3)
// The writer is very light-weight, however it will do following for you:
//  * Choose the most compact encoding of a given opcode
//  * Keep track of current state and convert absolute values to deltas
//  * Divide by header-defined factors as appropriate
class DebugFrameOpCodeWriter : protected Writer {
 public:
  // To save space, DWARF divides most offests by header-defined factors.
  // They are used in integer divisions, so let's make them constants.
  // We usually subtract from stack base pointer, so making the factor
  // negative makes the encoded values positive and thus easier to encode.
  static constexpr int kDataAlignmentFactor = -4;
  static constexpr int kCodeAlignmentFactor = 1;

  // Advance the program counter to given location.
  // The other opcode functions take PC as an explicit argument,
  // so you should not need to call this function manually.
  void AdvancePC(int absolute_pc) {
    DCHECK_GE(absolute_pc, current_pc_);
    int delta = FactorCodeOffset(absolute_pc - current_pc_);
    if (delta != 0) {
      if (delta <= 0x3F) {
        PushUint8(DW_CFA_advance_loc | delta);
      } else if (delta <= UINT8_MAX) {
        PushUint8(DW_CFA_advance_loc1);
        PushUint8(delta);
      } else if (delta <= UINT16_MAX) {
        PushUint8(DW_CFA_advance_loc2);
        PushUint16(delta);
      } else {
        PushUint8(DW_CFA_advance_loc4);
        PushUint32(delta);
      }
    }
    current_pc_ = absolute_pc;
  }

  // Common helper - specify register spill relative to current stack pointer
  void RelOffset(int pc, int reg, int offset) {
    Offset(pc, reg, offset - current_cfa_offset_);
  }

  // Common helper - increase stack frame size by given delta
  void AdjustCFAOffset(int pc, int delta) {
    DefCFAOffset(pc, current_cfa_offset_ + delta);
  }

  void Nop() {
    PushUint8(DW_CFA_nop);
  }

  void Offset(int pc, int reg, int offset) {
    AdvancePC(pc);
    int factored_offset = FactorDataOffset(offset);  // May change sign
    if (factored_offset >= 0) {
      if (0 <= reg && reg <= 0x3F) {
        PushUint8(DW_CFA_offset | reg);
        PushUleb128(factored_offset);
      } else {
        PushUint8(DW_CFA_offset_extended);
        PushUleb128(reg);
        PushUleb128(factored_offset);
      }
    } else {
      uses_dwarf3_features_ = true;
      PushUint8(DW_CFA_offset_extended_sf);
      PushUleb128(reg);
      PushSleb128(factored_offset);
    }
  }

  void Restore(int pc, int reg) {
    AdvancePC(pc);
    if (0 <= reg && reg <= 0x3F) {
      PushUint8(DW_CFA_restore | reg);
    } else {
      PushUint8(DW_CFA_restore_extended);
      PushUleb128(reg);
    }
  }

  void Undefined(int pc, int reg) {
    AdvancePC(pc);
    PushUint8(DW_CFA_undefined);
    PushUleb128(reg);
  }

  void SameValue(int pc, int reg) {
    AdvancePC(pc);
    PushUint8(DW_CFA_same_value);
    PushUleb128(reg);
  }

  void Register(int pc, int reg, int new_reg) {
    AdvancePC(pc);
    PushUint8(DW_CFA_register);
    PushUleb128(reg);
    PushUleb128(new_reg);
  }

  void RememberState() {
    // Note that we do not need to advance the PC
    PushUint8(DW_CFA_remember_state);
  }

  void RestoreState(int pc) {
    AdvancePC(pc);
    PushUint8(DW_CFA_restore_state);
  }

  void DefCFA(int pc, int reg, int offset) {
    AdvancePC(pc);
    if (offset >= 0) {
      PushUint8(DW_CFA_def_cfa);
      PushUleb128(reg);
      PushUleb128(offset);  // non-factored
    } else {
      uses_dwarf3_features_ = true;
      PushUint8(DW_CFA_def_cfa_sf);
      PushUleb128(reg);
      PushSleb128(FactorDataOffset(offset));
    }
    current_cfa_offset_ = offset;
  }

  void DefCFARegister(int pc, int reg) {
    AdvancePC(pc);
    PushUint8(DW_CFA_def_cfa_register);
    PushUleb128(reg);
  }

  void DefCFAOffset(int pc, int offset) {
    if (current_cfa_offset_ != offset) {
      AdvancePC(pc);
      if (offset >= 0) {
        PushUint8(DW_CFA_def_cfa_offset);
        PushUleb128(offset);  // non-factored
      } else {
        uses_dwarf3_features_ = true;
        PushUint8(DW_CFA_def_cfa_offset_sf);
        PushSleb128(FactorDataOffset(offset));
      }
      current_cfa_offset_ = offset;
    }
  }

  void ValOffset(int pc, int reg, int offset) {
    AdvancePC(pc);
    int factored_offset = FactorDataOffset(offset);  // May change sign
    if (factored_offset >= 0) {
      uses_dwarf3_features_ = true;
      PushUint8(DW_CFA_val_offset);
      PushUleb128(reg);
      PushUleb128(factored_offset);
    } else {
      uses_dwarf3_features_ = true;
      PushUint8(DW_CFA_val_offset_sf);
      PushUleb128(reg);
      PushSleb128(factored_offset);
    }
  }

  void DefCFAExpression(int pc, void* expr, int expr_size) {
    AdvancePC(pc);
    uses_dwarf3_features_ = true;
    PushUint8(DW_CFA_def_cfa_expression);
    PushUleb128(expr_size);
    PushData(expr, expr_size);
  }

  void Expression(int pc, int reg, void* expr, int expr_size) {
    AdvancePC(pc);
    uses_dwarf3_features_ = true;
    PushUint8(DW_CFA_expression);
    PushUleb128(reg);
    PushUleb128(expr_size);
    PushData(expr, expr_size);
  }

  void ValExpression(int pc, int reg, void* expr, int expr_size) {
    AdvancePC(pc);
    uses_dwarf3_features_ = true;
    PushUint8(DW_CFA_val_expression);
    PushUleb128(reg);
    PushUleb128(expr_size);
    PushData(expr, expr_size);
  }

  int current_cfa_offset() const {
    return current_cfa_offset_;
  }

  void set_current_cfa_offset(int offset) {
    current_cfa_offset_ = offset;
  }

  using Writer::data;

  DebugFrameOpCodeWriter()
      : Writer(&opcodes_),
        current_cfa_offset_(0),
        current_pc_(0),
        uses_dwarf3_features_(false) {
  }

 protected:
  int FactorDataOffset(int offset) const {
    DCHECK_EQ(offset % kDataAlignmentFactor, 0);
    return offset / kDataAlignmentFactor;
  }

  int FactorCodeOffset(int offset) const {
    DCHECK_EQ(offset % kCodeAlignmentFactor, 0);
    return offset / kCodeAlignmentFactor;
  }

  std::vector<uint8_t> opcodes_;
  int current_cfa_offset_;
  int current_pc_;
  bool uses_dwarf3_features_;
};

}  // namespace dwarf
}  // namespace art

#endif  // ART_COMPILER_DWARF_DEBUG_FRAME_OPCODE_WRITER_H_
