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

#ifndef ART_RUNTIME_ARCH_X86_X86_OPCODES_H_
#define ART_RUNTIME_ARCH_X86_X86_OPCODES_H_

#include <stdint.h>

namespace art {

// An x86 instruction consisting of a possible group id and a set of 3 operands.  Each
// operand is a bit mask containing the encoding bits for the operand (or 0 for empty).
class X86Instruction {
 public:
  uint32_t group_id_;
  uint32_t operands_[3];
};

class X86Group {
 public:
  uint32_t id_;
  uint32_t opcode_;
  X86Instruction instructions_[8];
};

static constexpr uint32_t kX86Instruction = 0xfe;
static constexpr uint32_t kX86OpReg = 0;
static constexpr uint32_t kX86UndefinedOpcode = 0xff;

// Instruction operand formats.
// This is in the bottom 5 bits of the operand.
static constexpr uint32_t kX86Op_A = 1;
static constexpr uint32_t kX86Op_B = 2;
static constexpr uint32_t kX86Op_C = 3;
static constexpr uint32_t kX86Op_D = 4;
static constexpr uint32_t kX86Op_E = 5;
static constexpr uint32_t kX86Op_F = 6;
static constexpr uint32_t kX86Op_G = 7;
static constexpr uint32_t kX86Op_H = 8;
static constexpr uint32_t kX86Op_I = 9;
static constexpr uint32_t kX86Op_J = 10;
static constexpr uint32_t kX86Op_L = 11;
static constexpr uint32_t kX86Op_M = 12;
static constexpr uint32_t kX86Op_N = 13;
static constexpr uint32_t kX86Op_O = 14;
static constexpr uint32_t kX86Op_P = 15;
static constexpr uint32_t kX86Op_Q = 16;
static constexpr uint32_t kX86Op_R = 17;
static constexpr uint32_t kX86Op_S = 18;
static constexpr uint32_t kX86Op_U = 19;
static constexpr uint32_t kX86Op_V = 21;
static constexpr uint32_t kX86Op_W = 22;
static constexpr uint32_t kX86Op_X = 23;
static constexpr uint32_t kX86Op_Y = 24;

// Operand format bits - upper 27 bits (as a bitmask).
static constexpr uint32_t kX86Op_a = 1 << 6;
static constexpr uint32_t kX86Op_b = 1 << 7;
static constexpr uint32_t kX86Op_c = 1 << 8;
static constexpr uint32_t kX86Op_d = 1 << 9;
static constexpr uint32_t kX86Op_v = 1 << 10;
static constexpr uint32_t kX86Op_w = 1 << 11;
static constexpr uint32_t kX86Op_p = 1 << 12;
static constexpr uint32_t kX86Op_z = 1 << 13;
static constexpr uint32_t kX86Op_q = 1 << 14;
static constexpr uint32_t kX86Op_s = 1 << 15;
static constexpr uint32_t kX86Op_1 = 1 << 16;
static constexpr uint32_t kX86Op_n = 1 << 17;
static constexpr uint32_t kX86Op_y = 1 << 18;

extern X86Instruction x86_a2_one_byte[];
extern X86Instruction x86_a3_two_byte[];
extern X86Group x86_groups[];

}       // namespace art
#endif  // ART_RUNTIME_ARCH_X86_X86_OPCODES_H_
