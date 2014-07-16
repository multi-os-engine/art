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

#include "base/macros.h"
#include "base/logging.h"
#include "globals.h"
#include "x86_inst_len.h"

namespace art {

uint32_t X86InstructionLengthCalculator::Calculate(uint8_t* pc) {
  rex_present_ = false;
  modrm_extracted_ = false;
  op66_ = false;

  const X86Instruction* inst = nullptr;
  uint8_t* start_pc = pc;
  pc_ = pc;

  uint8_t opcode = *pc_++;
  uint8_t row, column;
  uint32_t group_prefix = 0;

  // Check for prefixes.
  while (true) {
    bool prefix_present = true;
    switch (opcode) {
      case 0x66:
        op66_ = true;
        // Deliberate fall through.

      case 0xf0:
      case 0xf2:
      case 0xf3:
      case 0x67:
      case 0x26:
      case 0x36:
      case 0x64:
      case 0x65:
        group_prefix = opcode << 16;
        opcode = *pc_++;
        ExtractRowCol(opcode, &row, &column);
        break;

      default:
        prefix_present = false;
        ExtractRowCol(opcode, &row, &column);
    }
    if (!prefix_present) {
      break;
    }
  }

  // REX on x86_64
  if (x86_64_ && opcode >= 0x40 && opcode <= 0x4f) {
    rex_= opcode;
    rex_present_ = true;
    opcode = *pc_++;
    ExtractRowCol(opcode, &row, &column);
  }

  if (opcode >= 0xd8 && opcode <= 0xdf) {
    // x87 opcodes.  Not supported yet.
    UNIMPLEMENTED(FATAL) << "x87 opcodes are not implemented";
  }

  if (*pc == 0x0f) {
    // Two byte instructions.
    group_prefix |= 0x0f00;
    opcode = *pc_++;
    ExtractRowCol(opcode, &row, &column);
    uint8_t index = column < 8 ? row * 8 + column : (row + 16) * 8 + column - 8;
    inst = &x86_a3_two_byte[index];
  } else {
    // One byte instruction.  The table is divided into two halves.
    uint8_t index = column < 8 ? row * 8 + column : (row + 16) * 8 + column - 8;
    inst = &x86_a2_one_byte[index];
  }

  // We can't handle undefined opcodes.
  if (inst->group_id_ == kX86UndefinedOpcode) {
    LOG(FATAL) << "Undefined X86 opcode encountered: 0x" << std::hex << opcode;
  }

  // Check for a group of instructions.  If one is found, indirect it to get the
  // set of 8 instructions in the group.
  if (inst->group_id_ != kX86Instruction) {
    // Group present.
    bool group_found = false;
    for (int i = 0;  x86_groups[i].id_ != 0xff; ++i) {
      if (x86_groups[i].id_ == inst->group_id_ &&
          x86_groups[i].opcode_ == (group_prefix | opcode)) {
        modrm_ = *pc_++;
        modrm_extracted_ = true;
        inst = &x86_groups[i].instructions_[(modrm_ >> 3) & 0b111];
        group_found = true;
        break;
      }
    }

    if (!group_found) {
      LOG(FATAL) << "Unable to find x86 instruction group " << inst->group_id_ <<
        " with opcode " << (group_prefix | opcode);
    }
  }

  // Extract modrm if it hasn't already been extracted.
  if (!modrm_extracted_) {
    for (int i = 0; i < 3; ++i) {
      if (NeedsModrm(inst->operands_[i])) {
        modrm_ = *pc_++;
        modrm_extracted_ = true;
        break;
      }
    }
  }

  // Extract SIB if present.
  if (modrm_extracted_) {
    if ((modrm_ >> 6) != 0b11 && (modrm_ & 0b111) == 4) {
      ++pc_;     // SIB
    }
  }

  // Displacement.
  for (int i = 0; i < 3; ++i) {
    if (ExtractOperandDisplacement(inst->operands_[i])) {
      break;
    }
  }

  // Immediate.
  for (int i = 0; i < 3; ++i) {
    if (ExtractImmediate(inst->operands_[i])) {
      break;
    }
  }

  return pc_ - start_pc;
}


}       // namespace art
