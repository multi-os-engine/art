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

#ifndef ART_COMPILER_UTILS_X86_64_MANAGED_REGISTER_X86_64_H_
#define ART_COMPILER_UTILS_X86_64_MANAGED_REGISTER_X86_64_H_

#include "constants_x86_64.h"
#include "utils/managed_register.h"

namespace art {
namespace x86_64 {

// Values for register pairs.
// The registers in kReservedCpuRegistersArray in x86.cc are not used in pairs.
// The table kRegisterPairs in x86.cc must be kept in sync with this enum.
enum RegisterPair {
  RAX_RDX = 0,
  RAX_RCX = 1,
  RAX_RBX = 2,
  RAX_RDI = 3,
  RDX_RCX = 4,
  RDX_RBX = 5,
  RDX_RDI = 6,
  RCX_RBX = 7,
  RCX_RDI = 8,
  RBX_RDI = 9,
  kNumberOfRegisterPairs = 10,
  kNoRegisterPair = -1,
};

std::ostream& operator<<(std::ostream& os, const RegisterPair& reg);

const int kNumberOfCpuRegIds = kNumberOfCpuRegisters;
const int kNumberOfCpuAllocIds = kNumberOfCpuRegisters;

const int kNumberOfXmmRegIds = kNumberOfFloatRegisters;
const int kNumberOfXmmAllocIds = kNumberOfFloatRegisters;

const int kNumberOfX87RegIds = kNumberOfX87Registers;
const int kNumberOfX87AllocIds = kNumberOfX87Registers;

const int kNumberOfPairRegIds = kNumberOfRegisterPairs;

const int kNumberOfRegIds = kNumberOfCpuRegIds + kNumberOfXmmRegIds +
    kNumberOfX87RegIds + kNumberOfPairRegIds;
const int kNumberOfAllocIds = kNumberOfCpuAllocIds + kNumberOfXmmAllocIds +
    kNumberOfX87RegIds;

// Register ids map:
//   [0..R[  cpu registers (enum Register)
//   [R..X[  xmm registers (enum XmmRegister)
//   [X..S[  x87 registers (enum X87Register)
//   [S..P[  register pairs (enum RegisterPair)
// where
//   R = kNumberOfCpuRegIds
//   X = R + kNumberOfXmmRegIds
//   S = X + kNumberOfX87RegIds
//   P = X + kNumberOfRegisterPairs

// Allocation ids map:
//   [0..R[  cpu registers (enum Register)
//   [R..X[  xmm registers (enum XmmRegister)
//   [X..S[  x87 registers (enum X87Register)
// where
//   R = kNumberOfCpuRegIds
//   X = R + kNumberOfXmmRegIds
//   S = X + kNumberOfX87RegIds


// An instance of class 'ManagedRegister' represents a single cpu register (enum
// Register), an xmm register (enum XmmRegister), or a pair of cpu registers
// (enum RegisterPair).
// 'ManagedRegister::NoRegister()' provides an invalid register.
// There is a one-to-one mapping between ManagedRegister and register id.
class X86_64ManagedRegister : public ManagedRegister {
 public:
  int DWARFRegId() const {
    CHECK(IsCpuRegister());
    switch (RegId()) {
      case RAX: return  0;
      case RDX: return  1;
      case RCX: return  2;
      case RBX: return  3;
      case RSI: return  4;
      case RDI: return  5;
      case RBP: return  6;
      case RSP: return  7;
      default: return static_cast<int>(RegId());  // R8 ~ R15
    }
  }

  CpuRegister AsCpuRegister() const {
    CHECK(IsCpuRegister());
    return CpuRegister(static_cast<Register>(RegId()));
  }

  XmmRegister AsXmmRegister() const {
    CHECK(IsXmmRegister());
    return XmmRegister(static_cast<FloatRegister>(RegId() - kNumberOfCpuRegIds));
  }

  X87Register AsX87Register() const {
    CHECK(IsX87Register());
    return static_cast<X87Register>(RegId() -
                                    (kNumberOfCpuRegIds + kNumberOfXmmRegIds));
  }

  CpuRegister AsRegisterPairLow() const {
    CHECK(IsRegisterPair());
    // Appropriate mapping of register ids allows to use AllocIdLow().
    return FromRegId(AllocIdLow()).AsCpuRegister();
  }

  CpuRegister AsRegisterPairHigh() const {
    CHECK(IsRegisterPair());
    // Appropriate mapping of register ids allows to use AllocIdHigh().
    return FromRegId(AllocIdHigh()).AsCpuRegister();
  }

  bool IsCpuRegister() const {
    CHECK(IsValidManagedRegister());
    return (0 <= RegId()) && (RegId() < kNumberOfCpuRegIds);
  }

  bool IsXmmRegister() const {
    CHECK(IsValidManagedRegister());
    const int test = RegId() - kNumberOfCpuRegIds;
    return (0 <= test) && (test < kNumberOfXmmRegIds);
  }

  bool IsX87Register() const {
    CHECK(IsValidManagedRegister());
    const int test = RegId() - (kNumberOfCpuRegIds + kNumberOfXmmRegIds);
    return (0 <= test) && (test < kNumberOfX87RegIds);
  }

  bool IsRegisterPair() const {
    CHECK(IsValidManagedRegister());
    const int test = RegId() -
        (kNumberOfCpuRegIds + kNumberOfXmmRegIds + kNumberOfX87RegIds);
    return (0 <= test) && (test < kNumberOfPairRegIds);
  }

  void Print(std::ostream& os) const;

  // Returns true if the two managed-registers ('this' and 'other') overlap.
  // Either managed-register may be the NoRegister. If both are the NoRegister
  // then false is returned.
  bool Overlaps(const X86_64ManagedRegister& other) const;

  static X86_64ManagedRegister FromCpuRegister(Register r) {
    CHECK_NE(r, kNoRegister);
    return FromRegId(r);
  }

  static X86_64ManagedRegister FromXmmRegister(FloatRegister r) {
    return FromRegId(r + kNumberOfCpuRegIds);
  }

  static X86_64ManagedRegister FromX87Register(X87Register r) {
    CHECK_NE(r, kNoX87Register);
    return FromRegId(r + kNumberOfCpuRegIds + kNumberOfXmmRegIds);
  }

  static X86_64ManagedRegister FromRegisterPair(RegisterPair r) {
    CHECK_NE(r, kNoRegisterPair);
    return FromRegId(r + (kNumberOfCpuRegIds + kNumberOfXmmRegIds +
                          kNumberOfX87RegIds));
  }

 private:
  bool IsValidManagedRegister() const {
    return (0 <= RegId()) && (RegId() < kNumberOfRegIds);
  }

  int RegId() const {
    CHECK(!IsNoRegister());
    return ManagedRegister::RegId();
  }

  int AllocId() const {
    CHECK(IsValidManagedRegister() && !IsRegisterPair());
    CHECK_LT(RegId(), kNumberOfAllocIds);
    return RegId();
  }

  int AllocIdLow() const;
  int AllocIdHigh() const;

  friend class ManagedRegister;

  explicit X86_64ManagedRegister(int reg_id) : ManagedRegister(reg_id) {}

  static X86_64ManagedRegister FromRegId(int reg_id) {
    X86_64ManagedRegister reg(reg_id);
    CHECK(reg.IsValidManagedRegister());
    return reg;
  }
};

std::ostream& operator<<(std::ostream& os, const X86_64ManagedRegister& reg);

}  // namespace x86_64

inline x86_64::X86_64ManagedRegister ManagedRegister::AsX86_64() const {
  x86_64::X86_64ManagedRegister reg(RegId());
  CHECK(reg.IsNoRegister() || reg.IsValidManagedRegister());
  return reg;
}

}  // namespace art

#endif  // ART_COMPILER_UTILS_X86_64_MANAGED_REGISTER_X86_64_H_
