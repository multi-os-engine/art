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

#ifndef ART_COMPILER_DEX_QUICK_X86_X86_REGISTERS_H_
#define ART_COMPILER_DEX_QUICK_X86_X86_REGISTERS_H_

namespace art {

// Offset to distingish FP regs.
#define X86_FP_REG_OFFSET 32

enum X86NativeRegisterPool {
  r0     = 0,
  rAX    = r0,
  r1     = 1,
  rCX    = r1,
  r2     = 2,
  rDX    = r2,
  r3     = 3,
  rBX    = r3,
  r4sp   = 4,
  rX86_SP    = r4sp,
  r4sib_no_index = r4sp,
  r5     = 5,
  rBP    = r5,
  r5sib_no_base = r5,
  r6     = 6,
  rSI    = r6,
  r7     = 7,
  rDI    = r7,
#ifndef TARGET_REX_SUPPORT
  rRET   = 8,  // fake return address register for core spill mask.
#else
  r8     = 8,
  r9     = 9,
  r10    = 10,
  r11    = 11,
  r12    = 12,
  r13    = 13,
  r14    = 14,
  r15    = 15,
  rRET   = 16,  // fake return address register for core spill mask.
#endif
  fr0  =  0 + X86_FP_REG_OFFSET,
  fr1  =  1 + X86_FP_REG_OFFSET,
  fr2  =  2 + X86_FP_REG_OFFSET,
  fr3  =  3 + X86_FP_REG_OFFSET,
  fr4  =  4 + X86_FP_REG_OFFSET,
  fr5  =  5 + X86_FP_REG_OFFSET,
  fr6  =  6 + X86_FP_REG_OFFSET,
  fr7  =  7 + X86_FP_REG_OFFSET,
  fr8  =  8 + X86_FP_REG_OFFSET,
  fr9  =  9 + X86_FP_REG_OFFSET,
  fr10 = 10 + X86_FP_REG_OFFSET,
  fr11 = 11 + X86_FP_REG_OFFSET,
  fr12 = 12 + X86_FP_REG_OFFSET,
  fr13 = 13 + X86_FP_REG_OFFSET,
  fr14 = 14 + X86_FP_REG_OFFSET,
  fr15 = 15 + X86_FP_REG_OFFSET,
};

}  // namespace art

#endif  // ART_COMPILER_DEX_QUICK_X86_X86_REGISTERS_H_
