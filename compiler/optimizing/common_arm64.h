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

#include "code_generator.h"
#include "locations.h"
#include "nodes.h"
#include "utils/arm64/assembler_arm64.h"

#include "a64/disasm-a64.h"
#include "a64/macro-assembler-a64.h"

namespace art {
namespace arm64 {
namespace helpers {

// Convenience helpers to ease conversion to and from VIXL operands.
static_assert((SP == 31) && (WSP == 31) && (XZR == 32) && (WZR == 32),
              "Unexpected values for register codes.");

static inline int VIXLRegCodeFromART(int code) {
  if (code == SP) {
    return kSPRegInternalCode;
  }
  if (code == XZR) {
    return kZeroRegCode;
  }
  return code;
}

static inline int ARTRegCodeFromVIXL(int code) {
  if (code == kSPRegInternalCode) {
    return SP;
  }
  if (code == kZeroRegCode) {
    return XZR;
  }
  return code;
}

static inline Register XRegisterFrom(Location location) {
  DCHECK(location.IsRegister()) << location;
  return Register::GetXRegFromCode(VIXLRegCodeFromART(location.reg()));
}

static inline Register WRegisterFrom(Location location) {
  DCHECK(location.IsRegister()) << location;
  return Register::GetWRegFromCode(VIXLRegCodeFromART(location.reg()));
}

static inline Register RegisterFrom(Location location, Primitive::Type type) {
  DCHECK(type != Primitive::kPrimVoid && !Primitive::IsFloatingPointType(type)) << type;
  return type == Primitive::kPrimLong ? XRegisterFrom(location) : WRegisterFrom(location);
}

static inline Register OutputRegister(HInstruction* instr) {
  return RegisterFrom(instr->GetLocations()->Out(), instr->GetType());
}

static inline Register InputRegisterAt(HInstruction* instr, int input_index) {
  return RegisterFrom(instr->GetLocations()->InAt(input_index),
                      instr->InputAt(input_index)->GetType());
}

static inline FPRegister DRegisterFrom(Location location) {
  DCHECK(location.IsFpuRegister()) << location;
  return FPRegister::GetDRegFromCode(location.reg());
}

static inline FPRegister SRegisterFrom(Location location) {
  DCHECK(location.IsFpuRegister()) << location;
  return FPRegister::GetSRegFromCode(location.reg());
}

static inline FPRegister FPRegisterFrom(Location location, Primitive::Type type) {
  DCHECK(Primitive::IsFloatingPointType(type)) << type;
  return type == Primitive::kPrimDouble ? DRegisterFrom(location) : SRegisterFrom(location);
}

static inline FPRegister OutputFPRegister(HInstruction* instr) {
  return FPRegisterFrom(instr->GetLocations()->Out(), instr->GetType());
}

static inline FPRegister InputFPRegisterAt(HInstruction* instr, int input_index) {
  return FPRegisterFrom(instr->GetLocations()->InAt(input_index),
                        instr->InputAt(input_index)->GetType());
}

static inline CPURegister CPURegisterFrom(Location location, Primitive::Type type) {
  return Primitive::IsFloatingPointType(type) ? CPURegister(FPRegisterFrom(location, type))
                                              : CPURegister(RegisterFrom(location, type));
}

static inline CPURegister OutputCPURegister(HInstruction* instr) {
  return Primitive::IsFloatingPointType(instr->GetType())
      ? static_cast<CPURegister>(OutputFPRegister(instr))
      : static_cast<CPURegister>(OutputRegister(instr));
}

static inline CPURegister InputCPURegisterAt(HInstruction* instr, int index) {
  return Primitive::IsFloatingPointType(instr->InputAt(index)->GetType())
      ? static_cast<CPURegister>(InputFPRegisterAt(instr, index))
      : static_cast<CPURegister>(InputRegisterAt(instr, index));
}

static inline int64_t Int64ConstantFrom(Location location) {
  HConstant* instr = location.GetConstant();
  if (instr->IsIntConstant()) {
    return instr->AsIntConstant()->GetValue();
  } else if (instr->IsNullConstant()) {
    return 0;
  } else {
    DCHECK(instr->IsLongConstant()) << instr->DebugName();
    return instr->AsLongConstant()->GetValue();
  }
}

static inline vixl::aarch64::Operand OperandFrom(Location location, Primitive::Type type) {
  if (location.IsRegister()) {
    return vixl::aarch64::Operand(RegisterFrom(location, type));
  } else {
    return vixl::aarch64::Operand(Int64ConstantFrom(location));
  }
}

static inline vixl::aarch64::Operand InputOperandAt(HInstruction* instr, int input_index) {
  return OperandFrom(instr->GetLocations()->InAt(input_index),
                     instr->InputAt(input_index)->GetType());
}

static inline MemOperand StackOperandFrom(Location location) {
  return MemOperand(sp, location.GetStackIndex());
}

static inline MemOperand HeapOperand(const Register& base, size_t offset = 0) {
  // A heap reference must be 32bit, so fit in a W register.
  DCHECK(base.IsW());
  return MemOperand(base.X(), offset);
}

static inline MemOperand HeapOperand(const Register& base,
                                     const Register& regoffset,
                                     Shift shift = LSL,
                                     unsigned shift_amount = 0) {
  // A heap reference must be 32bit, so fit in a W register.
  DCHECK(base.IsW());
  return MemOperand(base.X(), regoffset, shift, shift_amount);
}

static inline MemOperand HeapOperand(const Register& base, Offset offset) {
  return HeapOperand(base, offset.SizeValue());
}

static inline MemOperand HeapOperandFrom(Location location, Offset offset) {
  return HeapOperand(RegisterFrom(location, Primitive::kPrimNot), offset);
}

static inline Location LocationFrom(const Register& reg) {
  return Location::RegisterLocation(ARTRegCodeFromVIXL(reg.GetCode()));
}

static inline Location LocationFrom(const FPRegister& fpreg) {
  return Location::FpuRegisterLocation(fpreg.GetCode());
}

static inline vixl::aarch64::Operand OperandFromMemOperand(const MemOperand& mem_op) {
  if (mem_op.IsImmediateOffset()) {
    return vixl::aarch64::Operand(mem_op.GetOffset());
  } else {
    DCHECK(mem_op.IsRegisterOffset());
    if (mem_op.GetExtend() != NO_EXTEND) {
      return vixl::aarch64::Operand(mem_op.GetRegisterOffset(),
                                    mem_op.GetExtend(),
                                    mem_op.GetShiftAmount());
    } else if (mem_op.GetShift() != NO_SHIFT) {
      return vixl::aarch64::Operand(mem_op.GetRegisterOffset(),
                                    mem_op.GetShift(),
                                    mem_op.GetShiftAmount());
    } else {
      LOG(FATAL) << "Should not reach here";
      UNREACHABLE();
    }
  }
}

static bool CanEncodeConstantAsImmediate(HConstant* constant, HInstruction* instr) {
  DCHECK(constant->IsIntConstant() || constant->IsLongConstant() || constant->IsNullConstant())
      << constant->DebugName();

  // For single uses we let VIXL handle the constant generation since it will
  // use registers that are not managed by the register allocator (wip0, wip1).
  if (constant->GetUses().HasExactlyOneElement()) {
    return true;
  }

  // Our code generator ensures shift distances are within an encodable range.
  if (instr->IsRor()) {
    return true;
  }

  int64_t value = CodeGenerator::GetInt64ValueOf(constant);

  if (instr->IsAnd() || instr->IsOr() || instr->IsXor()) {
    // Uses logical operations.
    return vixl::aarch64::Assembler::IsImmLogical(value, kXRegSize);
  } else if (instr->IsNeg()) {
    // Uses mov -immediate.
    return vixl::aarch64::Assembler::IsImmMovn(value, kXRegSize);
  } else {
    DCHECK(instr->IsAdd() ||
           instr->IsArm64IntermediateAddress() ||
           instr->IsBoundsCheck() ||
           instr->IsCompare() ||
           instr->IsCondition() ||
           instr->IsSub())
        << instr->DebugName();
    // Uses aliases of ADD/SUB instructions.
    // If `value` does not fit but `-value` does, VIXL will automatically use
    // the 'opposite' instruction.
    return vixl::aarch64::Assembler::IsImmAddSub(value)
        || vixl::aarch64::Assembler::IsImmAddSub(-value);
  }
}

static inline Location ARM64EncodableConstantOrRegister(HInstruction* constant,
                                                        HInstruction* instr) {
  if (constant->IsConstant()
      && CanEncodeConstantAsImmediate(constant->AsConstant(), instr)) {
    return Location::ConstantLocation(constant->AsConstant());
  }

  return Location::RequiresRegister();
}

// Check if registers in art register set have the same register code in vixl. If the register
// codes are same, we can initialize vixl register list simply by the register masks. Currently,
// only SP/WSP and ZXR/WZR codes are different between art and vixl.
// Note: This function is only used for debug checks.
static inline bool ArtVixlRegCodeCoherentForRegSet(uint32_t art_core_registers,
                                                   size_t num_core,
                                                   uint32_t art_fpu_registers,
                                                   size_t num_fpu) {
  // The register masks won't work if the number of register is larger than 32.
  DCHECK_GE(sizeof(art_core_registers) * 8, num_core);
  DCHECK_GE(sizeof(art_fpu_registers) * 8, num_fpu);
  for (size_t art_reg_code = 0;  art_reg_code < num_core; ++art_reg_code) {
    if (RegisterSet::Contains(art_core_registers, art_reg_code)) {
      if (art_reg_code != static_cast<size_t>(VIXLRegCodeFromART(art_reg_code))) {
        return false;
      }
    }
  }
  // There is no register code translation for float registers.
  return true;
}

static inline Shift ShiftFromOpKind(HArm64DataProcWithShifterOp::OpKind op_kind) {
  switch (op_kind) {
    case HArm64DataProcWithShifterOp::kASR: return ASR;
    case HArm64DataProcWithShifterOp::kLSL: return LSL;
    case HArm64DataProcWithShifterOp::kLSR: return LSR;
    default:
      LOG(FATAL) << "Unexpected op kind " << op_kind;
      UNREACHABLE();
      return NO_SHIFT;
  }
}

static inline Extend ExtendFromOpKind(HArm64DataProcWithShifterOp::OpKind op_kind) {
  switch (op_kind) {
    case HArm64DataProcWithShifterOp::kUXTB: return UXTB;
    case HArm64DataProcWithShifterOp::kUXTH: return UXTH;
    case HArm64DataProcWithShifterOp::kUXTW: return UXTW;
    case HArm64DataProcWithShifterOp::kSXTB: return SXTB;
    case HArm64DataProcWithShifterOp::kSXTH: return SXTH;
    case HArm64DataProcWithShifterOp::kSXTW: return SXTW;
    default:
      LOG(FATAL) << "Unexpected op kind " << op_kind;
      UNREACHABLE();
      return NO_EXTEND;
  }
}

static inline bool CanFitInShifterOperand(HInstruction* instruction) {
  if (instruction->IsTypeConversion()) {
    HTypeConversion* conversion = instruction->AsTypeConversion();
    Primitive::Type result_type = conversion->GetResultType();
    Primitive::Type input_type = conversion->GetInputType();
    // We don't expect to see the same type as input and result.
    return Primitive::IsIntegralType(result_type) && Primitive::IsIntegralType(input_type) &&
        (result_type != input_type);
  } else {
    return (instruction->IsShl() && instruction->AsShl()->InputAt(1)->IsIntConstant()) ||
        (instruction->IsShr() && instruction->AsShr()->InputAt(1)->IsIntConstant()) ||
        (instruction->IsUShr() && instruction->AsUShr()->InputAt(1)->IsIntConstant());
  }
}

static inline bool HasShifterOperand(HInstruction* instr) {
  // `neg` instructions are an alias of `sub` using the zero register as the
  // first register input.
  bool res = instr->IsAdd() || instr->IsAnd() || instr->IsNeg() ||
      instr->IsOr() || instr->IsSub() || instr->IsXor();
  return res;
}

static inline bool ShifterOperandSupportsExtension(HInstruction* instruction) {
  DCHECK(HasShifterOperand(instruction));
  // Although the `neg` instruction is an alias of the `sub` instruction, `HNeg`
  // does *not* support extension. This is because the `extended register` form
  // of the `sub` instruction interprets the left register with code 31 as the
  // stack pointer and not the zero register. (So does the `immediate` form.) In
  // the other form `shifted register, the register with code 31 is interpreted
  // as the zero register.
  return instruction->IsAdd() || instruction->IsSub();
}

}  // namespace helpers
}  // namespace arm64
}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_COMMON_ARM64_H_
