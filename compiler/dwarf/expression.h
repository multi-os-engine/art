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

namespace art {
namespace dwarf {
  template<size_t BufferSize = 16>
  class Expression {
   public:
    void Consts(int32_t value) {
      if (0 <= value && value < 32) {
        *(ptr_++) = DW_OP_lit0 + value;
      } else {
        *(ptr_++) = DW_OP_consts;
        ptr_ = EncodeSignedLeb128(ptr_, value);
      }
    }

    void Constu(uint32_t value) {
      if (value < 32) {
        *(ptr_++) = DW_OP_lit0 + value;
      } else {
        *(ptr_++) = DW_OP_constu;
        ptr_ = EncodeUnsignedLeb128(ptr_, value);
      }
    }

    void Reg(uint32_t dwarf_reg_num) {
      if (dwarf_reg_num < 32) {
        *(ptr_++) = DW_OP_reg0 + dwarf_reg_num;
      } else {
        *(ptr_++) = DW_OP_regx;
        ptr_ = EncodeUnsignedLeb128(ptr_, dwarf_reg_num);
      }
    }

    void Fbreg(int32_t stack_offset) {
      *(ptr_++) = DW_OP_fbreg;
      ptr_ = EncodeSignedLeb128(ptr_, stack_offset);
    }

    void Piece(uint32_t num_bytes) {
      *(ptr_++) = DW_OP_piece;
      ptr_ = EncodeUnsignedLeb128(ptr_, num_bytes);
    }

    void Deref() { *(ptr_++) = DW_OP_deref; }

    void DerefSize(uint8_t num_bytes) {
      *(ptr_++) = DW_OP_deref_size;
      *(ptr_++) = num_bytes;
    }

    void Plus() { *(ptr_++) = DW_OP_plus; }

    void PlusUconst(uint32_t offset) {
      *(ptr_++) = DW_OP_plus_uconst;
      ptr_ = EncodeUnsignedLeb128(ptr_, offset);
    }

    void CallFrameCfa() { *(ptr_++) = DW_OP_call_frame_cfa; }

    void PushObjectAddress() { *(ptr_++) = DW_OP_push_object_address; }

    void StackValue() { *(ptr_++) = DW_OP_stack_value; }

    Expression() : ptr_(&buffer_[0]) {}
    ~Expression() {
      // Check that at least half of the buffer is free to be safe.
      DCHECK_LE(Size(), BufferSize / 2);
    }

    const uint8_t* Data() const { return &buffer_[0]; }
    size_t Size() const { return ptr_ - &buffer_[0]; }

   private:
    uint8_t buffer_[BufferSize];
    uint8_t* ptr_;  // Current position in the buffer.
  };
}  // namespace dwarf
}  // namespace art

#endif  // ART_COMPILER_DWARF_EXPRESSION_H_
