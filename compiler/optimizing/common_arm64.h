/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ART_COMPILER_OPTIMIZING_COMMON_ARM64_H_
#define ART_COMPILER_OPTIMIZING_COMMON_ARM64_H_

#include "locations.h"
#include "nodes.h"
#include "utils/arm64/assembler_arm64.h"
#include "a64/disasm-a64.h"
#include "a64/macro-assembler-a64.h"

namespace art {
namespace arm64 {
namespace helpers {

constexpr bool IsFPType(Primitive::Type type) {
  return type == Primitive::kPrimFloat || type == Primitive::kPrimDouble;
}

ALWAYS_INLINE inline bool IsIntegralType(Primitive::Type type) {
  switch (type) {
    case Primitive::kPrimByte:
    case Primitive::kPrimChar:
    case Primitive::kPrimShort:
    case Primitive::kPrimInt:
    case Primitive::kPrimLong:
      return true;
    default:
      return false;
  }
}

constexpr bool Is64BitType(Primitive::Type type) {
  return type == Primitive::kPrimLong || type == Primitive::kPrimDouble;
}

// Convenience helpers to ease conversion to and from VIXL operands.
static_assert((SP == 31) && (WSP == 31) && (XZR == 32) && (WZR == 32),
              "Unexpected values for register codes.");

ALWAYS_INLINE inline int VIXLRegCodeFromART(int code) {
  if (code == SP) {
    return vixl::kSPRegInternalCode;
  }
  if (code == XZR) {
    return vixl::kZeroRegCode;
  }
  return code;
}

ALWAYS_INLINE inline int ARTRegCodeFromVIXL(int code) {
  if (code == vixl::kSPRegInternalCode) {
    return SP;
  }
  if (code == vixl::kZeroRegCode) {
    return XZR;
  }
  return code;
}

ALWAYS_INLINE inline vixl::Register XRegisterFrom(Location location) {
  DCHECK(location.IsRegister());
  return vixl::Register::XRegFromCode(VIXLRegCodeFromART(location.reg()));
}

ALWAYS_INLINE inline vixl::Register WRegisterFrom(Location location) {
  DCHECK(location.IsRegister());
  return vixl::Register::WRegFromCode(VIXLRegCodeFromART(location.reg()));
}

ALWAYS_INLINE inline vixl::Register RegisterFrom(Location location, Primitive::Type type) {
  DCHECK(type != Primitive::kPrimVoid && !IsFPType(type));
  return type == Primitive::kPrimLong ? XRegisterFrom(location) : WRegisterFrom(location);
}

ALWAYS_INLINE inline vixl::Register OutputRegister(HInstruction* instr) {
  return RegisterFrom(instr->GetLocations()->Out(), instr->GetType());
}

ALWAYS_INLINE inline vixl::Register InputRegisterAt(HInstruction* instr, int input_index) {
  return RegisterFrom(instr->GetLocations()->InAt(input_index),
                      instr->InputAt(input_index)->GetType());
}

ALWAYS_INLINE inline vixl::FPRegister DRegisterFrom(Location location) {
  DCHECK(location.IsFpuRegister());
  return vixl::FPRegister::DRegFromCode(location.reg());
}

ALWAYS_INLINE inline vixl::FPRegister SRegisterFrom(Location location) {
  DCHECK(location.IsFpuRegister());
  return vixl::FPRegister::SRegFromCode(location.reg());
}

ALWAYS_INLINE inline vixl::FPRegister FPRegisterFrom(Location location, Primitive::Type type) {
  DCHECK(IsFPType(type));
  return type == Primitive::kPrimDouble ? DRegisterFrom(location) : SRegisterFrom(location);
}

ALWAYS_INLINE inline vixl::FPRegister OutputFPRegister(HInstruction* instr) {
  return FPRegisterFrom(instr->GetLocations()->Out(), instr->GetType());
}

ALWAYS_INLINE inline vixl::FPRegister InputFPRegisterAt(HInstruction* instr, int input_index) {
  return FPRegisterFrom(instr->GetLocations()->InAt(input_index),
                        instr->InputAt(input_index)->GetType());
}

ALWAYS_INLINE inline vixl::CPURegister CPURegisterFrom(Location location, Primitive::Type type) {
  return IsFPType(type) ? vixl::CPURegister(FPRegisterFrom(location, type))
                        : vixl::CPURegister(RegisterFrom(location, type));
}

ALWAYS_INLINE inline vixl::CPURegister OutputCPURegister(HInstruction* instr) {
  return IsFPType(instr->GetType()) ? static_cast<vixl::CPURegister>(OutputFPRegister(instr))
                                    : static_cast<vixl::CPURegister>(OutputRegister(instr));
}

ALWAYS_INLINE inline vixl::CPURegister InputCPURegisterAt(HInstruction* instr, int index) {
  return IsFPType(instr->InputAt(index)->GetType())
      ? static_cast<vixl::CPURegister>(InputFPRegisterAt(instr, index))
      : static_cast<vixl::CPURegister>(InputRegisterAt(instr, index));
}

ALWAYS_INLINE inline int64_t Int64ConstantFrom(Location location) {
  HConstant* instr = location.GetConstant();
  return instr->IsIntConstant() ? instr->AsIntConstant()->GetValue()
                                : instr->AsLongConstant()->GetValue();
}

ALWAYS_INLINE inline vixl::Operand OperandFrom(Location location, Primitive::Type type) {
  if (location.IsRegister()) {
    return vixl::Operand(RegisterFrom(location, type));
  } else {
    return vixl::Operand(Int64ConstantFrom(location));
  }
}

ALWAYS_INLINE inline vixl::Operand InputOperandAt(HInstruction* instr, int input_index) {
  return OperandFrom(instr->GetLocations()->InAt(input_index),
                     instr->InputAt(input_index)->GetType());
}

ALWAYS_INLINE inline vixl::MemOperand StackOperandFrom(Location location) {
  return vixl::MemOperand(vixl::sp, location.GetStackIndex());
}

ALWAYS_INLINE inline vixl::MemOperand HeapOperand(const vixl::Register& base, size_t offset = 0) {
  // A heap reference must be 32bit, so fit in a W register.
  DCHECK(base.IsW());
  return vixl::MemOperand(base.X(), offset);
}

ALWAYS_INLINE inline vixl::MemOperand HeapOperand(const vixl::Register& base, Offset offset) {
  return HeapOperand(base, offset.SizeValue());
}

ALWAYS_INLINE inline vixl::MemOperand HeapOperandFrom(Location location, Offset offset) {
  return HeapOperand(RegisterFrom(location, Primitive::kPrimNot), offset);
}

ALWAYS_INLINE inline Location LocationFrom(const vixl::Register& reg) {
  return Location::RegisterLocation(ARTRegCodeFromVIXL(reg.code()));
}

ALWAYS_INLINE inline Location LocationFrom(const vixl::FPRegister& fpreg) {
  return Location::FpuRegisterLocation(fpreg.code());
}

ALWAYS_INLINE inline vixl::Operand OperandFromMemOperand(const vixl::MemOperand& mem_op) {
  if (mem_op.IsImmediateOffset()) {
    return vixl::Operand(mem_op.offset());
  } else {
    DCHECK(mem_op.IsRegisterOffset());
    if (mem_op.extend() != vixl::NO_EXTEND) {
      return vixl::Operand(mem_op.regoffset(), mem_op.extend(), mem_op.shift_amount());
    } else if (mem_op.shift() != vixl::NO_SHIFT) {
      return vixl::Operand(mem_op.regoffset(), mem_op.shift(), mem_op.shift_amount());
    } else {
      LOG(FATAL) << "Should not reach here";
      UNREACHABLE();
    }
  }
}

}  // namespace helpers
}  // namespace arm64
}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_COMMON_ARM64_H_
