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

#ifndef ART_COMPILER_DWARF_DEBUG_LINE_OPCODE_WRITER_H_
#define ART_COMPILER_DWARF_DEBUG_LINE_OPCODE_WRITER_H_

#include "enums.h"
#include "writer.h"

namespace art {
namespace dwarf {

// Writer for the .debug_line opcodes (DWARF-3)
// The writer is very light-weight, however it will do following for you:
//  * Choose the most compact encoding of a given opcode
//  * Keep track of current state and convert absolute values to deltas
//  * Divide by header-defined factors as appropriate
class DebugLineOpCodeWriter : protected Writer {
 public:
  static constexpr int kOpcodeBase = 13;
  static constexpr bool kDefaultIsStmt = true;
  static constexpr int kLineBase = -5;
  static constexpr int kLineRange = 14;

  void AddRow() {
    PushUint8(DW_LNS_copy);
  }

  void AdvancePC(uint64_t absolute_address) {
    DCHECK_GE(absolute_address, current_address_);
    if (absolute_address != current_address_) {
      uint64_t delta = FactorCodeOffset(absolute_address - current_address_);
      if (delta <= INT32_MAX) {
        PushUint8(DW_LNS_advance_pc);
        PushUleb128(static_cast<int>(delta));
        current_address_ = absolute_address;
      } else {
        SetAddress(absolute_address);
      }
    }
  }

  void AdvanceLine(int absolute_line) {
    int delta = absolute_line - current_line_;
    if (delta != 0) {
      PushUint8(DW_LNS_advance_line);
      PushSleb128(delta);
      current_line_ = absolute_line;
    }
  }

  void SetFile(int file) {
    if (current_file_ != file) {
      PushUint8(DW_LNS_set_file);
      PushUleb128(file);
      current_file_ = file;
    }
  }

  void SetColumn(int column) {
    PushUint8(DW_LNS_set_column);
    PushUleb128(column);
  }

  void NegateStmt() {
    PushUint8(DW_LNS_negate_stmt);
  }

  void SetBasicBlock() {
    PushUint8(DW_LNS_set_basic_block);
  }

  void SetPrologueEnd() {
    uses_dwarf3_features_ = true;
    PushUint8(DW_LNS_set_prologue_end);
  }

  void SetEpilogueBegin() {
    uses_dwarf3_features_ = true;
    PushUint8(DW_LNS_set_epilogue_begin);
  }

  void SetISA(int isa) {
    uses_dwarf3_features_ = true;
    PushUint8(DW_LNS_set_isa);
    PushUleb128(isa);
  }

  void EndSequence() {
    PushUint8(0);
    PushUleb128(1);
    PushUint8(DW_LNE_end_sequence);
  }

  // Uncoditionally set address using the long encoding.
  // This gives the linker opportunity to relocate the address.
  void SetAddress(uint64_t absolute_address) {
    DCHECK_GE(absolute_address, current_address_);
    FactorCodeOffset(absolute_address);  // check if it is factorable
    PushUint8(0);
    if (use_64bit_address_) {
      PushUleb128(1 + 8);
      PushUint8(DW_LNE_set_address);
      PushUint64(absolute_address);
    } else {
      PushUleb128(1 + 4);
      PushUint8(DW_LNE_set_address);
      PushUint32(absolute_address);
    }
    current_address_ = absolute_address;
  }

  void DefineFile(const char* filename,
                  int directory_index,
                  int modification_time,
                  int file_size) {
    int size = 1 +
               strlen(filename) + 1 +
               UnsignedLeb128Size(directory_index) +
               UnsignedLeb128Size(modification_time) +
               UnsignedLeb128Size(file_size);
    PushUint8(0);
    PushUleb128(size);
    size_t start = data_->size();
    PushUint8(DW_LNE_define_file);
    PushString(filename);
    PushUleb128(directory_index);
    PushUleb128(modification_time);
    PushUleb128(file_size);
    DCHECK_EQ(start + size, data_->size());
  }

  // Compact address and line opcode.
  void AddRow(uint64_t absolute_address, int absolute_line) {
    DCHECK_GE(absolute_address, current_address_);

    // If the address is definitely too far, use the long encoding
    uint64_t delta_address = FactorCodeOffset(absolute_address - current_address_);
    if (delta_address > 0xFF) {
      AdvancePC(absolute_address);
      delta_address = 0;
    }

    // If the line is definitely too far, use the long encoding
    int delta_line = absolute_line - current_line_;
    if (!(kLineBase <= delta_line && delta_line < kLineBase + kLineRange)) {
      AdvanceLine(absolute_line);
      delta_line = 0;
    }

    // Both address and line should be reasonable now.  Use the short encoding
    int opcode = kOpcodeBase + (delta_line - kLineBase) +
                 (static_cast<int>(delta_address) * kLineRange);
    if (opcode > 0xFF) {
      // If the address is still too far, try to increment it by const amount
      int const_advance = (0xFF - kOpcodeBase) / kLineRange;
      opcode -= (kLineRange * const_advance);
      if (opcode <= 0xFF) {
        PushUint8(DW_LNS_const_add_pc);
      } else {
        // Give up and use long encoding for address
        AdvancePC(absolute_address);
        // Still use the opcode to do line advance and copy
        opcode = kOpcodeBase + (delta_line - kLineBase);
      }
    }
    DCHECK(kOpcodeBase <= opcode && opcode <= 0xFF);
    PushUint8(opcode);  // "special opcode"
    current_line_ = absolute_line;
    current_address_ = absolute_address;
  }

  int code_factor_bits() {
    return code_factor_bits_;
  }

  uint64_t current_address() {
    return current_address_;
  }

  int current_file() {
    return current_file_;
  }

  int current_line() {
    return current_line_;
  }

  using Writer::data;

  DebugLineOpCodeWriter(bool use64bitAddress, int codeFactorBits)
      : Writer(&opcodes_),
        uses_dwarf3_features_(false),
        use_64bit_address_(use64bitAddress),
        code_factor_bits_(codeFactorBits),
        current_address_(0),
        current_file_(1),
        current_line_(1) {
  }

 protected:
  uint64_t FactorCodeOffset(uint64_t offset) const {
    DCHECK_GE(code_factor_bits_, 0);
    DCHECK_EQ((offset >> code_factor_bits_) << code_factor_bits_, offset);
    return offset >> code_factor_bits_;
  }

  std::vector<uint8_t> opcodes_;
  bool uses_dwarf3_features_;
  bool use_64bit_address_;
  int code_factor_bits_;
  uint64_t current_address_;
  int current_file_;
  int current_line_;
};

}  // namespace dwarf
}  // namespace art

#endif  // ART_COMPILER_DWARF_DEBUG_LINE_OPCODE_WRITER_H_
