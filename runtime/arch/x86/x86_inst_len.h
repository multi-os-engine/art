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

#ifndef ART_RUNTIME_ARCH_X86_X86_INST_LEN_H_
#define ART_RUNTIME_ARCH_X86_X86_INST_LEN_H_

#include "x86_opcodes.h"

namespace art {

class X86InstructionLengthCalculator {
 public:
  X86InstructionLengthCalculator() : rex_(0), rex_present_(false), modrm_extracted_(false),
  modrm_(0), op66_(false), pc_(nullptr) {
  }

  uint32_t Calculate(uint8_t* pc);

 private:
  bool ExtractOperandDisplacement(uint32_t format) {
    uint32_t type = format & 0x1f;
    if (type == kX86Op_E || type == kX86Op_M || type == kX86Op_W) {
      int mod = (modrm_ >> 6) & 0b11;
      switch (mod) {
        case 0b00: break;
        case 0b01: pc_ += 1; return true;
        case 0b10: pc_ += 4; return true;
        case 0b11:
          break;
      }
    }
    return false;
  }

  bool ExtractImmediate(uint32_t format) {
    uint32_t type = format & 0x1f;
    if (type == kX86Op_I || type == kX86Op_J) {
      if ((format & kX86Op_b) != 0) {
        pc_ += 1;
      } else if ((format & kX86Op_w) != 0) {
        pc_ += 2;
      } else if ((format & kX86Op_z) != 0) {
        pc_ += op66_ ? 2 : 4;
      } else if ((format & (kX86Op_q | kX86Op_d | kX86Op_v))  != 0) {
        pc_ += 4;
      }

      return true;
    }
    return false;
  }

  bool NeedsModrm(uint32_t format) {
    switch (format & 0x1f) {
      case kX86Op_C:
      case kX86Op_D:
      case kX86Op_G:
      case kX86Op_M:
      case kX86Op_N:
      case kX86Op_P:
      case kX86Op_Q:
      case kX86Op_R:
      case kX86Op_S:
      case kX86Op_U:
      case kX86Op_V:
      case kX86Op_W:
        return true;
    }
    return false;
  }

  void ExtractRowCol(uint8_t op, uint8_t* row, uint8_t* col) {
    *row = op >> 4;
    *col = op & 0b1111;
  }

  uint8_t rex_;
  bool rex_present_;
  bool modrm_extracted_;
  uint8_t modrm_;
  bool op66_;
  uint8_t* pc_;
};

}       // namespace art


#endif  // ART_RUNTIME_ARCH_X86_X86_INST_LEN_H_
