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

#ifndef ART_RUNTIME_ARCH_ARM64_REGISTERS_ARM64_H_
#define ART_RUNTIME_ARCH_ARM64_REGISTERS_ARM64_H_

#include <iosfwd>

namespace art {
namespace arm64 {

#define LIST_0_TO_31(M)                                                                       \
  M(0) M(1) M(2) M(3) M(4) M(5) M(6) M(7) M(8) M(9) M(10) M(11) M(12) M(13) M(14) M(15) M(16) \
  M(17) M(18) M(19) M(20) M(21) M(22) M(23) M(24) M(25) M(26) M(27) M(28) M(29) M(30) M(31)

#define REG_ENUM(T, N) T##N,

// The enum register mappings are expected to be identical to VIXL register
// codes except for the stack pointer register.
// TODO: static asserts to check this.

// Values for GP XRegisters - 64bit registers.
// TODO: static asserts
enum XRegister {
#define X_REG_ENUM(N) REG_ENUM(X, N)
  LIST_0_TO_31(X_REG_ENUM)
#undef X_REG_ENUM
  SP,         // SP and XZR are encoded in instructions using the register
              // code 31, the context deciding which is used. We use a
              // differrent enum value to distinguish between the two.
  kNumberOfXRegisters,
  // Aliases.
  TR  = X18,  // ART Thread Register - Managed Runtime (Caller Saved Reg)
  ETR = X21,  // ART Thread Register - External Calls  (Callee Saved Reg)
  IP0 = X16,  // Used as scratch by VIXL.
  IP1 = X17,  // Used as scratch by ART JNI Assembler.
  FP  = X29,
  LR  = X30,
  XZR = X31,
  kNoRegister = -1,
};
std::ostream& operator<<(std::ostream& os, const XRegister& rhs);

// Values for GP WRegisters - 32bit registers.
enum WRegister {
#define W_REG_ENUM(N) REG_ENUM(W, N)
  LIST_0_TO_31(W_REG_ENUM)
#undef W_REG_ENUM
  WSP,  // See the comment for SP above.
  kNumberOfWRegisters,
  // Aliases.
  WZR = W31,
  kNoWRegister = -1,
};
std::ostream& operator<<(std::ostream& os, const WRegister& rhs);

// Values for FP DRegisters - double precision floating point.
enum DRegister {
#define D_REG_ENUM(N) REG_ENUM(D, N)
  LIST_0_TO_31(D_REG_ENUM)
#undef D_REG_ENUM
  kNumberOfDRegisters,
  kNoDRegister = -1,
};
std::ostream& operator<<(std::ostream& os, const DRegister& rhs);

// Values for FP SRegisters - single precision floating point.
enum SRegister {
#define S_REG_ENUM(N) REG_ENUM(S, N)
  LIST_0_TO_31(S_REG_ENUM)
#undef S_REG_ENUM
  kNumberOfSRegisters,
  kNoSRegister = -1,
};
std::ostream& operator<<(std::ostream& os, const SRegister& rhs);

#undef LIST_0_TO_31
#undef REG_ENUM

}  // namespace arm64
}  // namespace art

#endif  // ART_RUNTIME_ARCH_ARM64_REGISTERS_ARM64_H_
