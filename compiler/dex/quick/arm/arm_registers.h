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

#ifndef ART_COMPILER_DEX_QUICK_ARM_ARM_REGISTERS_H_
#define ART_COMPILER_DEX_QUICK_ARM_ARM_REGISTERS_H_

namespace art {

// Offset to distinguish DP FP regs.
#define ARM_FP_DOUBLE 64
// Offset to distingish FP regs.
#define ARM_FP_REG_OFFSET 32

enum ArmNativeRegisterPool {
  r0   = 0,
  r1   = 1,
  r2   = 2,
  r3   = 3,
  rARM_SUSPEND = 4,
  r5   = 5,
  r6   = 6,
  r7   = 7,
  r8   = 8,
  rARM_SELF  = 9,
  r10  = 10,
  r11  = 11,
  rARM_FP  = 11,
  r12  = 12,
  r13sp  = 13,
  rARM_SP  = 13,
  r14lr  = 14,
  rARM_LR  = 14,
  r15pc  = 15,
  rARM_PC  = 15,
  fr0  =  0 + ARM_FP_REG_OFFSET,
  fr1  =  1 + ARM_FP_REG_OFFSET,
  fr2  =  2 + ARM_FP_REG_OFFSET,
  fr3  =  3 + ARM_FP_REG_OFFSET,
  fr4  =  4 + ARM_FP_REG_OFFSET,
  fr5  =  5 + ARM_FP_REG_OFFSET,
  fr6  =  6 + ARM_FP_REG_OFFSET,
  fr7  =  7 + ARM_FP_REG_OFFSET,
  fr8  =  8 + ARM_FP_REG_OFFSET,
  fr9  =  9 + ARM_FP_REG_OFFSET,
  fr10 = 10 + ARM_FP_REG_OFFSET,
  fr11 = 11 + ARM_FP_REG_OFFSET,
  fr12 = 12 + ARM_FP_REG_OFFSET,
  fr13 = 13 + ARM_FP_REG_OFFSET,
  fr14 = 14 + ARM_FP_REG_OFFSET,
  fr15 = 15 + ARM_FP_REG_OFFSET,
  fr16 = 16 + ARM_FP_REG_OFFSET,
  fr17 = 17 + ARM_FP_REG_OFFSET,
  fr18 = 18 + ARM_FP_REG_OFFSET,
  fr19 = 19 + ARM_FP_REG_OFFSET,
  fr20 = 20 + ARM_FP_REG_OFFSET,
  fr21 = 21 + ARM_FP_REG_OFFSET,
  fr22 = 22 + ARM_FP_REG_OFFSET,
  fr23 = 23 + ARM_FP_REG_OFFSET,
  fr24 = 24 + ARM_FP_REG_OFFSET,
  fr25 = 25 + ARM_FP_REG_OFFSET,
  fr26 = 26 + ARM_FP_REG_OFFSET,
  fr27 = 27 + ARM_FP_REG_OFFSET,
  fr28 = 28 + ARM_FP_REG_OFFSET,
  fr29 = 29 + ARM_FP_REG_OFFSET,
  fr30 = 30 + ARM_FP_REG_OFFSET,
  fr31 = 31 + ARM_FP_REG_OFFSET,
  dr0 = fr0 + ARM_FP_DOUBLE,
  dr1 = fr2 + ARM_FP_DOUBLE,
  dr2 = fr4 + ARM_FP_DOUBLE,
  dr3 = fr6 + ARM_FP_DOUBLE,
  dr4 = fr8 + ARM_FP_DOUBLE,
  dr5 = fr10 + ARM_FP_DOUBLE,
  dr6 = fr12 + ARM_FP_DOUBLE,
  dr7 = fr14 + ARM_FP_DOUBLE,
  dr8 = fr16 + ARM_FP_DOUBLE,
  dr9 = fr18 + ARM_FP_DOUBLE,
  dr10 = fr20 + ARM_FP_DOUBLE,
  dr11 = fr22 + ARM_FP_DOUBLE,
  dr12 = fr24 + ARM_FP_DOUBLE,
  dr13 = fr26 + ARM_FP_DOUBLE,
  dr14 = fr28 + ARM_FP_DOUBLE,
  dr15 = fr30 + ARM_FP_DOUBLE,
};

}  // namespace art

#endif  // ART_COMPILER_DEX_QUICK_ARM_ARM_REGISTERS_H_
