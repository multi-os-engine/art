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

#ifndef ART_COMPILER_DWARF_EXPRESSION_H_
#define ART_COMPILER_DWARF_EXPRESSION_H_

#include <cstddef>
#include <cstdint>

#include "dwarf/dwarf_constants.h"
#include "dwarf/writer.h"

namespace art {
namespace dwarf {
class Expression : private Writer<> {
 public:
  using Writer<>::data;
  using Writer<>::size;

  void Consts(int32_t value) {
    if (0 <= value && value < 32) {
      PushUint8(DW_OP_lit0 + value);
    } else {
      PushUint8(DW_OP_consts);
      PushSleb128(value);
    }
  }

  void Constu(uint32_t value) {
    if (value < 32) {
      PushUint8(DW_OP_lit0 + value);
    } else {
      PushUint8(DW_OP_constu);
      PushUleb128(value);
    }
  }

  void Reg(uint32_t dwarf_reg_num) {
    if (dwarf_reg_num < 32) {
      PushUint8(DW_OP_reg0 + dwarf_reg_num);
    } else {
      PushUint8(DW_OP_regx);
      PushUleb128(dwarf_reg_num);
    }
  }

  void Fbreg(int32_t stack_offset) {
    PushUint8(DW_OP_fbreg);
    PushSleb128(stack_offset);
  }

  void Piece(uint32_t num_bytes) {
    PushUint8(DW_OP_piece);
    PushUleb128(num_bytes);
  }

  void Deref() { PushUint8(DW_OP_deref); }

  void DerefSize(uint8_t num_bytes) {
    PushUint8(DW_OP_deref_size);
    PushUint8(num_bytes);
  }

  void Plus() { PushUint8(DW_OP_plus); }

  void PlusUconst(uint32_t offset) {
    PushUint8(DW_OP_plus_uconst);
    PushUleb128(offset);
  }

  void CallFrameCfa() { PushUint8(DW_OP_call_frame_cfa); }

  void PushObjectAddress() { PushUint8(DW_OP_push_object_address); }

  void StackValue() { PushUint8(DW_OP_stack_value); }

  explicit Expression(std::vector<uint8_t>* buffer) : Writer<>(buffer) {
    buffer->clear();
  }
};
}  // namespace dwarf
}  // namespace art

#endif  // ART_COMPILER_DWARF_EXPRESSION_H_
