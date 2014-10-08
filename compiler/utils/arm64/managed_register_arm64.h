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

#ifndef ART_COMPILER_UTILS_ARM64_MANAGED_REGISTER_ARM64_H_
#define ART_COMPILER_UTILS_ARM64_MANAGED_REGISTER_ARM64_H_

#include "base/logging.h"
#include "constants_arm64.h"
#include "primitive.h"
#include "utils/managed_register.h"
#include "a64/macro-assembler-a64.h"

namespace art {
namespace arm64 {

// An instance of class 'ManagedRegister' represents a single Arm64
// register. A register can be one of the following:
//  * core register 64bit context (enum XRegister)
//  * core register 32bit context (enum WRegister)
//  * VFP double precision register (enum DRegister)
//  * VFP single precision register (enum SRegister)
//
// There is a one to one mapping between ManagedRegister and register id.

class Arm64ManagedRegister : public ManagedRegister {
 public:
  enum RegisterType {
    // The 'invalid' type should be zero to ensure that a ManagedRegister
    // improperly converted to an Arm64ManagedRegister shows this type and
    // hits the appropriate checks.
    kInvalidRegisterType = 0,
    kXRegisterType = 1 << 0,
    kWRegisterType = 1 << 1,
    kDRegisterType = 1 << 2,
    kSRegisterType = 1 << 3
  };

  explicit Arm64ManagedRegister(uword value) : ManagedRegister() {
    value_ = value;
    DCHECK(IsValidManagedRegister());
  }

  Arm64ManagedRegister(RegisterType type, int reg_id) : ManagedRegister(reg_id) {
    SetRegType(type);
    DCHECK(IsValidManagedRegister());
  }

  XRegister AsXRegister() const {
    CHECK(IsXRegister());
    return static_cast<XRegister>(RegId());
  }

  WRegister AsWRegister() const {
    CHECK(IsWRegister());
    return static_cast<WRegister>(RegId());
  }

  DRegister AsDRegister() const {
    CHECK(IsDRegister());
    return static_cast<DRegister>(RegId());
  }

  SRegister AsSRegister() const {
    CHECK(IsSRegister());
    return static_cast<SRegister>(RegId());
  }

  XRegister AsOverlappingXRegister() const {
    return static_cast<XRegister>(AsWRegister());
  }

  WRegister AsOverlappingWRegister() const {
    return static_cast<WRegister>(AsXRegister());
  }

  DRegister AsOverlappingDRegister() const {
    return static_cast<DRegister>(AsSRegister());
  }

  SRegister AsOverlappingSRegister() const {
    return static_cast<SRegister>(AsDRegister());
  }

  bool IsXRegister() const {
    CHECK(IsValidManagedRegister());
    return RegType() == kXRegisterType;
  }

  bool IsWRegister() const {
    CHECK(IsValidManagedRegister());
    return RegType() == kWRegisterType;
  }

  bool IsDRegister() const {
    CHECK(IsValidManagedRegister());
    return RegType() == kDRegisterType;
  }

  bool IsSRegister() const {
    CHECK(IsValidManagedRegister());
    return RegType() == kSRegisterType;
  }

  bool IsGPRegister() const {
    return IsXRegister() || IsWRegister();
  }

  bool IsFPRegister() const {
    return IsDRegister() || IsSRegister();
  }

  bool Is64Bits() const {
    return IsXRegister() || IsDRegister();
  }
  bool Is32Bits() const {
    return IsWRegister() || IsSRegister();
  }

  bool IsSameSizeAndType(Arm64ManagedRegister other) const {
    DCHECK(IsValidManagedRegister() && other.IsValidManagedRegister());
    return RegType() == other.RegType();
  }

  // Returns true if this managed-register overlaps the other managed-register.
  // If either or both are a 'no register', the function returns false.
  bool Overlaps(const Arm64ManagedRegister& other) const;

  void Print(std::ostream& os) const;

  static Arm64ManagedRegister FromXRegister(XRegister r) {
    return Arm64ManagedRegister(kXRegisterType, r);
  }

  static Arm64ManagedRegister FromWRegister(WRegister r) {
    return Arm64ManagedRegister(kWRegisterType, r);
  }

  static Arm64ManagedRegister FromDRegister(DRegister r) {
    return Arm64ManagedRegister(kDRegisterType, r);
  }

  static Arm64ManagedRegister FromSRegister(SRegister r) {
    return Arm64ManagedRegister(kSRegisterType, r);
  }

  static int NumberOfRegisters(RegisterType type) {
    switch (type) {
      case kXRegisterType: return kNumberOfXRegisters;
      case kWRegisterType: return kNumberOfWRegisters;
      case kDRegisterType: return kNumberOfDRegisters;
      case kSRegisterType: return kNumberOfSRegisters;
      default:
      LOG(FATAL) << "Unreachable";
      return -1;
    }
  }

  void SetRegType(RegisterType type) { value_ = RegTypeField::Update(type, value_); }
  RegisterType RegType() const { return RegTypeField::Decode(value_); }

  vixl::CPURegister AsVixlCPURegister() const {
    DCHECK(IsValidManagedRegister());
    // The stack pointer is encoded differently in VIXL and ART.
    if (RegId() == SP) {
      return Is64Bits() ? vixl::sp : vixl::wsp;
    } else if (RegId() == XZR) {
      return Is64Bits() ? vixl::xzr : vixl::wzr;
    } else {
      return vixl::CPURegister(
          RegId(),
          Is32Bits() ? vixl::kWRegSize : vixl::kXRegSize,
          IsFPRegister() ? vixl::CPURegister::kFPRegister : vixl::CPURegister::kRegister);
    }
  }

  vixl::Register AsVixlRegister() const { return vixl::Register(AsVixlCPURegister()); }
  vixl::FPRegister AsVixlFPRegister() const { return vixl::FPRegister(AsVixlCPURegister()); }

  static Arm64ManagedRegister FromVixlReg(vixl::CPURegister reg) {
    DCHECK(reg.IsValid());
    // SP and zero registers are encoded differently in VIXL and ART.
    if (reg.Is(vixl::sp)) {
      return FromXRegister(SP);
    } else if (reg.Is(vixl::wsp)) {
      return FromWRegister(WSP);
    }
    if (reg.Is(vixl::xzr)) {
      return FromXRegister(XZR);
    } else if (reg.Is(vixl::wzr)) {
      return FromWRegister(WZR);
    }

    RegisterType type = kInvalidRegisterType;
    switch (reg.type()) {
      case vixl::CPURegister::kRegister:
        type = reg.Is64Bits() ? kXRegisterType : kWRegisterType;
        break;
      case vixl::CPURegister::kFPRegister:
        type = reg.Is64Bits() ? kDRegisterType : kSRegisterType;
      default:
        LOG(FATAL) << "Unreachable";
    }
    return Arm64ManagedRegister(type, reg.code());
  }

  static RegisterType RegTypeFor(Primitive::Type type) {
    switch (type) {
      case Primitive::kPrimNot:
      case Primitive::kPrimBoolean:
      case Primitive::kPrimByte:
      case Primitive::kPrimChar:
      case Primitive::kPrimShort:
      case Primitive::kPrimInt:
        return kWRegisterType;
      case Primitive::kPrimLong:
        return kXRegisterType;
      case Primitive::kPrimFloat:
        return kSRegisterType;
      case Primitive::kPrimDouble:
        return kDRegisterType;
      case Primitive::kPrimVoid:
      default:
        LOG(FATAL) << "Unreachable";
        return kInvalidRegisterType;
    }
  }

 private:
  static constexpr uint32_t kBitsForRegType = 4;
  typedef BitField<RegisterType, kArchIndependentNBitsUsed, kBitsForRegType> RegTypeField;


  bool IsValidManagedRegister() const {
    RegisterType type = RegType();
    int id = ManagedRegister::RegId();
    return (type != kInvalidRegisterType && id <= NumberOfRegisters(type)) ||
        (id == kNoRegister);
  }

  int RegId() const {
    DCHECK(IsValidManagedRegister());
    return ManagedRegister::RegId();
  }

  friend class ManagedRegister;
};

std::ostream& operator<<(std::ostream& os, const Arm64ManagedRegister& reg);

}  // namespace arm64

inline arm64::Arm64ManagedRegister ManagedRegister::AsArm64() const {
  return arm64::Arm64ManagedRegister(value_);
}

}  // namespace art

#endif  // ART_COMPILER_UTILS_ARM64_MANAGED_REGISTER_ARM64_H_
