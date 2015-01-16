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

#include "intrinsics_arm64.h"

#include "code_generator_arm64.h"
#include "entrypoints/quick/quick_entrypoints.h"
#include "intrinsics.h"
#include "mirror/array-inl.h"
#include "mirror/art_method.h"
#include "mirror/string.h"
#include "thread.h"
#include "utils/arm64/assembler_arm64.h"
#include "utils/arm64/constants_arm64.h"

#include "a64/disasm-a64.h"
#include "a64/macro-assembler-a64.h"

using namespace vixl;   // NOLINT(build/namespaces)

namespace art {

namespace arm64 {


// TODO: Refactor to share with code generator.
namespace {

/*bool IsFPType(Primitive::Type type) {
  return type == Primitive::kPrimFloat || type == Primitive::kPrimDouble;
}

bool IsIntegralType(Primitive::Type type) {
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

bool Is64BitType(Primitive::Type type) {
  return type == Primitive::kPrimLong || type == Primitive::kPrimDouble;
}*/

// Convenience helpers to ease conversion to and from VIXL operands.
static_assert((SP == 31) && (WSP == 31) && (XZR == 32) && (WZR == 32),
              "Unexpected values for register codes.");

int VIXLRegCodeFromART(int code) {
  if (code == SP) {
    return vixl::kSPRegInternalCode;
  }
  if (code == XZR) {
    return vixl::kZeroRegCode;
  }
  return code;
}

/*
int ARTRegCodeFromVIXL(int code) {
  if (code == vixl::kSPRegInternalCode) {
    return SP;
  }
  if (code == vixl::kZeroRegCode) {
    return XZR;
  }
  return code;
}
*/

Register XRegisterFrom(Location location) {
  DCHECK(location.IsRegister());
  return Register::XRegFromCode(VIXLRegCodeFromART(location.reg()));
}

Register WRegisterFrom(Location location) {
  DCHECK(location.IsRegister());
  return Register::WRegFromCode(VIXLRegCodeFromART(location.reg()));
}

/*Register RegisterFrom(Location location, Primitive::Type type) {
  DCHECK(type != Primitive::kPrimVoid && !IsFPType(type));
  return type == Primitive::kPrimLong ? XRegisterFrom(location) : WRegisterFrom(location);
}

Register OutputRegister(HInstruction* instr) {
  return RegisterFrom(instr->GetLocations()->Out(), instr->GetType());
}

Register InputRegisterAt(HInstruction* instr, int input_index) {
  return RegisterFrom(instr->GetLocations()->InAt(input_index),
                      instr->InputAt(input_index)->GetType());
}*/

FPRegister DRegisterFrom(Location location) {
  DCHECK(location.IsFpuRegister());
  return FPRegister::DRegFromCode(location.reg());
}

FPRegister SRegisterFrom(Location location) {
  DCHECK(location.IsFpuRegister());
  return FPRegister::SRegFromCode(location.reg());
}

/*FPRegister FPRegisterFrom(Location location, Primitive::Type type) {
  DCHECK(IsFPType(type));
  return type == Primitive::kPrimDouble ? DRegisterFrom(location) : SRegisterFrom(location);
}

FPRegister OutputFPRegister(HInstruction* instr) {
  return FPRegisterFrom(instr->GetLocations()->Out(), instr->GetType());
}

FPRegister InputFPRegisterAt(HInstruction* instr, int input_index) {
  return FPRegisterFrom(instr->GetLocations()->InAt(input_index),
                        instr->InputAt(input_index)->GetType());
}

CPURegister CPURegisterFrom(Location location, Primitive::Type type) {
  return IsFPType(type) ? CPURegister(FPRegisterFrom(location, type))
                        : CPURegister(RegisterFrom(location, type));
}

CPURegister OutputCPURegister(HInstruction* instr) {
  return IsFPType(instr->GetType()) ? static_cast<CPURegister>(OutputFPRegister(instr))
                                    : static_cast<CPURegister>(OutputRegister(instr));
}

CPURegister InputCPURegisterAt(HInstruction* instr, int index) {
  return IsFPType(instr->InputAt(index)->GetType())
      ? static_cast<CPURegister>(InputFPRegisterAt(instr, index))
      : static_cast<CPURegister>(InputRegisterAt(instr, index));
}

int64_t Int64ConstantFrom(Location location) {
  HConstant* instr = location.GetConstant();
  return instr->IsIntConstant() ? instr->AsIntConstant()->GetValue()
                                : instr->AsLongConstant()->GetValue();
}

Operand OperandFrom(Location location, Primitive::Type type) {
  if (location.IsRegister()) {
    return Operand(RegisterFrom(location, type));
  } else {
    return Operand(Int64ConstantFrom(location));
  }
}

Operand InputOperandAt(HInstruction* instr, int input_index) {
  return OperandFrom(instr->GetLocations()->InAt(input_index),
                     instr->InputAt(input_index)->GetType());
}

MemOperand StackOperandFrom(Location location) {
  return MemOperand(sp, location.GetStackIndex());
}

MemOperand HeapOperand(const Register& base, size_t offset = 0) {
  // A heap reference must be 32bit, so fit in a W register.
  DCHECK(base.IsW());
  return MemOperand(base.X(), offset);
}

MemOperand HeapOperand(const Register& base, Offset offset) {
  return HeapOperand(base, offset.SizeValue());
}

MemOperand HeapOperandFrom(Location location, Offset offset) {
  return HeapOperand(RegisterFrom(location, Primitive::kPrimNot), offset);
}

Location LocationFrom(const Register& reg) {
  return Location::RegisterLocation(ARTRegCodeFromVIXL(reg.code()));
}

Location LocationFrom(const FPRegister& fpreg) {
  return Location::FpuRegisterLocation(fpreg.code());
}*/

}  // namespace

static constexpr bool kIntrinsified = true;

vixl::MacroAssembler* IntrinsicCodeGeneratorARM64::GetAssembler() {
  return codegen_->GetAssembler()->vixl_masm_;
}

ArenaAllocator* IntrinsicCodeGeneratorARM64::GetArena() {
  return codegen_->GetGraph()->GetArena();
}

bool IntrinsicLocationsBuilderARM64::TryDispatch(HInvoke* invoke) {
  UNUSED(arena_);
  Dispatch(invoke);
  const LocationSummary* res = invoke->GetLocations();
  return res != nullptr && res->Intrinsified();
}

/*
#define __ reinterpret_cast<Arm64Assembler*>(codegen->GetAssembler())->

// TODO: trg as memory.
static void MoveFromReturnRegister(Location trg,
                                   Primitive::Type type,
                                   CodeGeneratorARM64* codegen) {
  if (!trg.IsValid()) {
    DCHECK(type == Primitive::kPrimVoid);
    return;
  }

  switch (type) {
    case Primitive::kPrimBoolean:
    case Primitive::kPrimByte:
    case Primitive::kPrimChar:
    case Primitive::kPrimShort:
    case Primitive::kPrimInt:
    case Primitive::kPrimNot: {
      CpuRegister trg_reg = trg.AsRegister<CpuRegister>();
      if (trg_reg.AsRegister() != RAX) {
        __ movl(trg_reg, CpuRegister(RAX));
      }
      break;
    }
    case Primitive::kPrimLong: {
      CpuRegister trg_reg = trg.AsRegister<CpuRegister>();
      if (trg_reg.AsRegister() != RAX) {
        __ movq(trg_reg, CpuRegister(RAX));
      }
      break;
    }

    case Primitive::kPrimVoid:
      LOG(FATAL) << "Unexpected void type for valid location " << trg;
      UNREACHABLE();

    case Primitive::kPrimDouble: {
      XmmRegister trg_reg = trg.AsFpuRegister<XmmRegister>();
      if (trg_reg.AsFloatRegister() != XMM0) {
        __ movsd(trg_reg, XmmRegister(XMM0));
      }
      break;
    }
    case Primitive::kPrimFloat: {
      XmmRegister trg_reg = trg.AsFpuRegister<XmmRegister>();
      if (trg_reg.AsFloatRegister() != XMM0) {
        __ movss(trg_reg, XmmRegister(XMM0));
      }
      break;
    }
  }
}

static void MoveArguments(HInvoke* invoke, ArenaAllocator* arena, CodeGeneratorARM64* codegen) {
  if (invoke->InputCount() == 0) {
    return;
  }

  LocationSummary* locations = invoke->GetLocations();
  InvokeDexCallingConventionVisitor calling_convention_visitor;

  // We're moving potentially two or more locations to locations that could overlap, so we need
  // a parallel move resolver.
  HParallelMove parallel_move(arena);

  for (size_t i = 0; i < invoke->InputCount(); i++) {
    HInstruction* input = invoke->InputAt(i);
    Location cc_loc = calling_convention_visitor.GetNextLocation(input->GetType());
    Location actual_loc = locations->InAt(i);

    parallel_move.AddMove(new (arena) MoveOperands(actual_loc, cc_loc, nullptr));
  }

  codegen->GetMoveResolver()->EmitNativeCode(&parallel_move);
}

// Slow-path for fallback (calling the managed code to handle the intrinsic) in an intrinsified
// call. This will copy the arguments into the positions for a regular call.
//
// Note: The actual parameters are required to be in the locations given by the invoke's location
//       summary. If an intrinsic modifies those locations before a slowpath call, they must be
//       restored!
class IntrinsicSlowPathARM64 : public SlowPathCodeARM64 {
 public:
  explicit IntrinsicSlowPathARM64(HInvoke* invoke) : invoke_(invoke) { }

  void EmitNativeCode(CodeGenerator* codegen_in) OVERRIDE {
    CodeGeneratorARM64* codegen = down_cast<CodeGeneratorARM64*>(codegen_in);
    __ Bind(GetEntryLabel());

    codegen->SaveLiveRegisters(invoke_->GetLocations());

    MoveArguments(invoke_, codegen->GetGraph()->GetArena(), codegen);

    if (invoke_->IsInvokeStaticOrDirect()) {
      codegen->GenerateStaticOrDirectCall(invoke_->AsInvokeStaticOrDirect(), CpuRegister(RDI));
    } else {
      UNIMPLEMENTED(FATAL) << "Non-direct intrinsic slow-path not yet implemented";
      UNREACHABLE();
    }

    // Copy the result back to the expected output.
    Location out = invoke_->GetLocations()->Out();
    if (out.IsValid()) {
      DCHECK(out.IsRegister());  // TODO: Replace this when we support output in memory.
      DCHECK(!invoke_->GetLocations()->GetLiveRegisters()->ContainsCoreRegister(out.reg()));
      MoveFromReturnRegister(out, invoke_->GetType(), codegen);
    }

    codegen->RestoreLiveRegisters(invoke_->GetLocations());
    __ jmp(GetExitLabel());
  }

 private:
  // The instruction where this slow path is happening.
  HInvoke* const invoke_;

  DISALLOW_COPY_AND_ASSIGN(IntrinsicSlowPathARM64);
};

#undef __
*/

#define __ assembler->

static void CreateFPToIntLocations(ArenaAllocator* arena, HInvoke* invoke) {
  LocationSummary* locations = new (arena) LocationSummary(invoke,
                                                           LocationSummary::kNoCall,
                                                           kIntrinsified);
  locations->SetInAt(0, Location::RequiresFpuRegister());
  locations->SetOut(Location::RequiresRegister());
}

static void CreateIntToFPLocations(ArenaAllocator* arena, HInvoke* invoke) {
  LocationSummary* locations = new (arena) LocationSummary(invoke,
                                                           LocationSummary::kNoCall,
                                                           kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresFpuRegister());
}

static void MoveFPToInt(LocationSummary* locations, bool is64bit, vixl::MacroAssembler* assembler) {
  Location input = locations->InAt(0);
  Location output = locations->Out();
  __ Fmov(is64bit ? XRegisterFrom(output) : WRegisterFrom(output),
          is64bit ? DRegisterFrom(input) : SRegisterFrom(input));
}

static void MoveIntToFP(LocationSummary* locations, bool is64bit, vixl::MacroAssembler* assembler) {
  Location input = locations->InAt(0);
  Location output = locations->Out();
  __ Fmov(is64bit ? DRegisterFrom(output) : SRegisterFrom(output),
          is64bit ? XRegisterFrom(input) : WRegisterFrom(input));
}

void IntrinsicLocationsBuilderARM64::VisitDoubleDoubleToRawLongBits(HInvoke* invoke) {
  CreateFPToIntLocations(arena_, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitDoubleLongBitsToDouble(HInvoke* invoke) {
  CreateIntToFPLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitDoubleDoubleToRawLongBits(HInvoke* invoke) {
  MoveFPToInt(invoke->GetLocations(), true, GetAssembler());
}
void IntrinsicCodeGeneratorARM64::VisitDoubleLongBitsToDouble(HInvoke* invoke) {
  MoveIntToFP(invoke->GetLocations(), true, GetAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitFloatFloatToRawIntBits(HInvoke* invoke) {
  CreateFPToIntLocations(arena_, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitFloatIntBitsToFloat(HInvoke* invoke) {
  CreateIntToFPLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitFloatFloatToRawIntBits(HInvoke* invoke) {
  MoveFPToInt(invoke->GetLocations(), false, GetAssembler());
}
void IntrinsicCodeGeneratorARM64::VisitFloatIntBitsToFloat(HInvoke* invoke) {
  MoveIntToFP(invoke->GetLocations(), false, GetAssembler());
}


static void CreateIntToIntLocations(ArenaAllocator* arena, HInvoke* invoke) {
  LocationSummary* locations = new (arena) LocationSummary(invoke,
                                                           LocationSummary::kNoCall,
                                                           kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister());
}

static void GenReverseBytes(LocationSummary* locations,
                            Primitive::Type size,
                            vixl::MacroAssembler* assembler) {
  Location in = locations->InAt(0);
  Location out = locations->Out();

  switch (size) {
    case Primitive::kPrimShort:
      // TODO: Can be done with an xchg of 8b registers. This is straight from Quick.
      __ Rev16(WRegisterFrom(out), WRegisterFrom(in));
      __ Sxth(WRegisterFrom(out), WRegisterFrom(out));
      break;
    case Primitive::kPrimInt:
      __ Rev(WRegisterFrom(out), WRegisterFrom(in));
      break;
    case Primitive::kPrimLong:
      __ Rev(XRegisterFrom(out), XRegisterFrom(in));
      break;
    default:
      LOG(FATAL) << "Unexpected size for reverse-bytes: " << size;
      UNREACHABLE();
  }
}

void IntrinsicLocationsBuilderARM64::VisitIntegerReverseBytes(HInvoke* invoke) {
  CreateIntToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitIntegerReverseBytes(HInvoke* invoke) {
  GenReverseBytes(invoke->GetLocations(), Primitive::kPrimInt, GetAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitLongReverseBytes(HInvoke* invoke) {
  CreateIntToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitLongReverseBytes(HInvoke* invoke) {
  GenReverseBytes(invoke->GetLocations(), Primitive::kPrimLong, GetAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitShortReverseBytes(HInvoke* invoke) {
  CreateIntToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitShortReverseBytes(HInvoke* invoke) {
  GenReverseBytes(invoke->GetLocations(), Primitive::kPrimShort, GetAssembler());
}

static void CreateFloatToFloat(ArenaAllocator* arena, HInvoke* invoke) {
  // We only support FP registers here.
  LocationSummary* locations = new (arena) LocationSummary(invoke,
                                                           LocationSummary::kNoCall,
                                                           kIntrinsified);
  locations->SetInAt(0, Location::RequiresFpuRegister());
  locations->SetOut(Location::RequiresFpuRegister());
}

static void MathAbsFP(LocationSummary* locations, bool is64bit, vixl::MacroAssembler* assembler) {
  Location in = locations->InAt(0);
  Location out = locations->Out();
  if (is64bit) {
    __ Fabs(DRegisterFrom(out), DRegisterFrom(in));
  } else {
    __ Fabs(SRegisterFrom(out), SRegisterFrom(in));
  }
}

void IntrinsicLocationsBuilderARM64::VisitMathAbsDouble(HInvoke* invoke) {
  CreateFloatToFloat(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathAbsDouble(HInvoke* invoke) {
  MathAbsFP(invoke->GetLocations(), true, GetAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitMathAbsFloat(HInvoke* invoke) {
  CreateFloatToFloat(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathAbsFloat(HInvoke* invoke) {
  MathAbsFP(invoke->GetLocations(), false, GetAssembler());
}

static void CreateIntToIntPlusTemp(ArenaAllocator* arena, HInvoke* invoke) {
  LocationSummary* locations = new (arena) LocationSummary(invoke,
                                                           LocationSummary::kNoCall,
                                                           kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::SameAsFirstInput());
  locations->AddTemp(Location::RequiresRegister());
}

static void GenAbsInteger(LocationSummary* locations, bool is64bit,
                          vixl::MacroAssembler* assembler) {
  Location in = locations->InAt(0);
  Location output = locations->Out();

  Register in_reg = is64bit ? XRegisterFrom(in) : WRegisterFrom(in);
  Register out_reg = is64bit ? XRegisterFrom(output) : WRegisterFrom(output);

  __ Cmp(in_reg, Operand(0));
  __ Csneg(out_reg, in_reg, in_reg, pl);
}

void IntrinsicLocationsBuilderARM64::VisitMathAbsInt(HInvoke* invoke) {
  CreateIntToIntPlusTemp(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathAbsInt(HInvoke* invoke) {
  GenAbsInteger(invoke->GetLocations(), false, GetAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitMathAbsLong(HInvoke* invoke) {
  CreateIntToIntPlusTemp(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathAbsLong(HInvoke* invoke) {
  GenAbsInteger(invoke->GetLocations(), true, GetAssembler());
}

static void GenMinMaxFP(LocationSummary* locations, bool is_min, bool is_double,
                        vixl::MacroAssembler* assembler) {
  Location op1 = locations->InAt(0);
  Location op2 = locations->InAt(1);
  Location out = locations->Out();

  FPRegister op1_reg = is_double ? DRegisterFrom(op1) : SRegisterFrom(op1);
  FPRegister op2_reg = is_double ? DRegisterFrom(op2) : SRegisterFrom(op2);
  FPRegister out_reg = is_double ? DRegisterFrom(out) : SRegisterFrom(out);
  if (is_min) {
    __ Fmin(out_reg, op1_reg, op2_reg);
  } else {
    __ Fmax(out_reg, op1_reg, op2_reg);
  }
}

static void CreateFPFPToFPPlusTempLocations(ArenaAllocator* arena, HInvoke* invoke) {
  LocationSummary* locations = new (arena) LocationSummary(invoke,
                                                           LocationSummary::kNoCall,
                                                           kIntrinsified);
  locations->SetInAt(0, Location::RequiresFpuRegister());
  locations->SetInAt(1, Location::RequiresFpuRegister());
  locations->SetOut(Location::RequiresFpuRegister());
}

void IntrinsicLocationsBuilderARM64::VisitMathMinDoubleDouble(HInvoke* invoke) {
  CreateFPFPToFPPlusTempLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathMinDoubleDouble(HInvoke* invoke) {
  GenMinMaxFP(invoke->GetLocations(), true, true, GetAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitMathMinFloatFloat(HInvoke* invoke) {
  CreateFPFPToFPPlusTempLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathMinFloatFloat(HInvoke* invoke) {
  GenMinMaxFP(invoke->GetLocations(), true, false, GetAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitMathMaxDoubleDouble(HInvoke* invoke) {
  CreateFPFPToFPPlusTempLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathMaxDoubleDouble(HInvoke* invoke) {
  GenMinMaxFP(invoke->GetLocations(), false, true, GetAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitMathMaxFloatFloat(HInvoke* invoke) {
  CreateFPFPToFPPlusTempLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathMaxFloatFloat(HInvoke* invoke) {
  GenMinMaxFP(invoke->GetLocations(), false, false, GetAssembler());
}

static void GenMinMax(LocationSummary* locations, bool is_min, bool is_long,
                      vixl::MacroAssembler* assembler) {
  Location op1 = locations->InAt(0);
  Location op2 = locations->InAt(1);
  Location out = locations->Out();

  Register op1_reg = is_long ? XRegisterFrom(op1) : WRegisterFrom(op1);
  Register op2_reg = is_long ? XRegisterFrom(op2) : WRegisterFrom(op2);
  Register out_reg = is_long ? XRegisterFrom(out) : WRegisterFrom(out);

  __ Cmp(op1_reg, op2_reg);
  __ Csel(out_reg, op1_reg, op2_reg, is_min ? lt : gt);
}

static void CreateIntIntToIntLocations(ArenaAllocator* arena, HInvoke* invoke) {
  LocationSummary* locations = new (arena) LocationSummary(invoke,
                                                           LocationSummary::kNoCall,
                                                           kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister());
}

void IntrinsicLocationsBuilderARM64::VisitMathMinIntInt(HInvoke* invoke) {
  CreateIntIntToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathMinIntInt(HInvoke* invoke) {
  GenMinMax(invoke->GetLocations(), true, false, GetAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitMathMinLongLong(HInvoke* invoke) {
  CreateIntIntToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathMinLongLong(HInvoke* invoke) {
  GenMinMax(invoke->GetLocations(), true, true, GetAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitMathMaxIntInt(HInvoke* invoke) {
  CreateIntIntToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathMaxIntInt(HInvoke* invoke) {
  GenMinMax(invoke->GetLocations(), false, false, GetAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitMathMaxLongLong(HInvoke* invoke) {
  CreateIntIntToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathMaxLongLong(HInvoke* invoke) {
  GenMinMax(invoke->GetLocations(), false, true, GetAssembler());
}

static void CreateFPToFPLocations(ArenaAllocator* arena, HInvoke* invoke) {
  LocationSummary* locations = new (arena) LocationSummary(invoke,
                                                           LocationSummary::kNoCall,
                                                           kIntrinsified);
  locations->SetInAt(0, Location::RequiresFpuRegister());
  locations->SetOut(Location::RequiresFpuRegister());
}

void IntrinsicLocationsBuilderARM64::VisitMathSqrt(HInvoke* invoke) {
  CreateFPToFPLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathSqrt(HInvoke* invoke) {
  LocationSummary* locations = invoke->GetLocations();
  GetAssembler()->Fsqrt(DRegisterFrom(locations->Out()), DRegisterFrom(locations->InAt(0)));
}

void IntrinsicLocationsBuilderARM64::VisitMathCeil(HInvoke* invoke) {
  CreateFPToFPLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathCeil(HInvoke* invoke) {
  LocationSummary* locations = invoke->GetLocations();
  GetAssembler()->Frintp(DRegisterFrom(locations->Out()), DRegisterFrom(locations->InAt(0)));
}

void IntrinsicLocationsBuilderARM64::VisitMathFloor(HInvoke* invoke) {
  CreateFPToFPLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathFloor(HInvoke* invoke) {
  LocationSummary* locations = invoke->GetLocations();
  GetAssembler()->Frintm(DRegisterFrom(locations->Out()), DRegisterFrom(locations->InAt(0)));
}

void IntrinsicLocationsBuilderARM64::VisitMathRint(HInvoke* invoke) {
  CreateFPToFPLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathRint(HInvoke* invoke) {
  LocationSummary* locations = invoke->GetLocations();
  GetAssembler()->Frintn(DRegisterFrom(locations->Out()), DRegisterFrom(locations->InAt(0)));
}

static void CreateFPToFPPlus2FPTempLocations(ArenaAllocator* arena, HInvoke* invoke) {
  LocationSummary* locations = new (arena) LocationSummary(invoke,
                                                           LocationSummary::kNoCall,
                                                           kIntrinsified);
  locations->SetInAt(0, Location::RequiresFpuRegister());
  locations->SetOut(Location::RequiresRegister());
  locations->AddTemp(Location::RequiresFpuRegister());
  locations->AddTemp(Location::RequiresFpuRegister());
}

static void GenMathRound(LocationSummary* locations, bool is_double,
                         vixl::MacroAssembler* assembler) {
  FPRegister in_reg = is_double ?
      DRegisterFrom(locations->InAt(0)) : SRegisterFrom(locations->InAt(0));
  Register out_reg = is_double ?
      XRegisterFrom(locations->Out()) : WRegisterFrom(locations->Out());
  FPRegister temp1_reg = is_double ?
      DRegisterFrom(locations->GetTemp(0)) : SRegisterFrom(locations->GetTemp(0));
  FPRegister temp2_reg = is_double ?
      DRegisterFrom(locations->GetTemp(1)) : SRegisterFrom(locations->GetTemp(1));

  if (is_double) {
    __ Fmov(temp1_reg, static_cast<double>(0.5));
  } else {
    __ Fmov(temp1_reg, static_cast<float>(0.5));
  }
  __ Fadd(temp2_reg, in_reg, temp1_reg);  // This is from Quick. Do we really need two temps?
  __ Fcvtms(out_reg, temp2_reg);
}

void IntrinsicLocationsBuilderARM64::VisitMathRoundDouble(HInvoke* invoke) {
  CreateFPToFPPlus2FPTempLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathRoundDouble(HInvoke* invoke) {
  GenMathRound(invoke->GetLocations(), true, GetAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitMathRoundFloat(HInvoke* invoke) {
  CreateFPToFPPlus2FPTempLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathRoundFloat(HInvoke* invoke) {
  GenMathRound(invoke->GetLocations(), false, GetAssembler());
}

/*
void IntrinsicLocationsBuilderARM64::VisitStringCharAt(HInvoke* invoke) {
  // The inputs plus one temp.
  LocationSummary* locations = new (arena_) LocationSummary(invoke,
                                                            LocationSummary::kCallOnSlowPath,
                                                            kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RequiresRegister());
  locations->SetOut(Location::SameAsFirstInput());
  locations->AddTemp(Location::RequiresRegister());
}

void IntrinsicCodeGeneratorARM64::VisitStringCharAt(HInvoke* invoke) {
  LocationSummary* locations = invoke->GetLocations();

  // Location of reference to data array
  const int32_t value_offset = mirror::String::ValueOffset().Int32Value();
  // Location of count
  const int32_t count_offset = mirror::String::CountOffset().Int32Value();
  // Starting offset within data array
  const int32_t offset_offset = mirror::String::OffsetOffset().Int32Value();
  // Start of char data with array_
  const int32_t data_offset = mirror::Array::DataOffset(sizeof(uint16_t)).Int32Value();

  CpuRegister obj = locations->InAt(0).AsRegister<CpuRegister>();
  CpuRegister idx = locations->InAt(1).AsRegister<CpuRegister>();
  CpuRegister out = locations->Out().AsRegister<CpuRegister>();
  Location temp_loc = locations->GetTemp(0);
  CpuRegister temp = temp_loc.AsRegister<CpuRegister>();

  // Note: Nullcheck has been done before in a HNullCheck before the HInvokeVirtual. If/when we
  //       move to (coalesced) implicit checks, we have to do a null check below.
  DCHECK(!kCoalescedImplicitNullCheck);

  // TODO: Maybe we can support range check elimination. Overall, though, I think it's not worth
  //       the cost.
  // TODO: For simplicity, the index parameter is requested in a register, so different from Quick
  //       we will not optimize the code for constants (which would save a register).

  SlowPathCodeARM64* slow_path = new (GetArena()) IntrinsicSlowPathARM64(invoke);
  codegen_->AddSlowPath(slow_path);

  vixl::MacroAssembler* assembler = GetAssembler();

  __ cmpl(idx, Address(obj, count_offset));
  __ j(kAboveEqual, slow_path->GetEntryLabel());

  // Get the actual element.
  __ movl(temp, idx);                          // temp := idx.
  __ addl(temp, Address(obj, offset_offset));  // temp := offset + idx.
  __ movl(out, Address(obj, value_offset));    // obj := obj.array.
  // out = out[2*temp].
  __ movzxw(out, Address(out, temp, ScaleFactor::TIMES_2, data_offset));

  __ Bind(slow_path->GetExitLabel());
}

*/

void IntrinsicLocationsBuilderARM64::VisitMemoryPeekByte(HInvoke* invoke) {
  CreateIntToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMemoryPeekByte(HInvoke* invoke) {
  codegen_->Load(Primitive::kPrimByte,
                 WRegisterFrom(invoke->GetLocations()->Out()),
                 MemOperand(XRegisterFrom(invoke->GetLocations()->InAt(0)), 0));
}

void IntrinsicLocationsBuilderARM64::VisitMemoryPeekIntNative(HInvoke* invoke) {
  CreateIntToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMemoryPeekIntNative(HInvoke* invoke) {
  codegen_->Load(Primitive::kPrimInt,
                 WRegisterFrom(invoke->GetLocations()->Out()),
                 MemOperand(XRegisterFrom(invoke->GetLocations()->InAt(0)), 0));
}

void IntrinsicLocationsBuilderARM64::VisitMemoryPeekLongNative(HInvoke* invoke) {
  CreateIntToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMemoryPeekLongNative(HInvoke* invoke) {
  codegen_->Load(Primitive::kPrimLong,
                 XRegisterFrom(invoke->GetLocations()->Out()),
                 MemOperand(XRegisterFrom(invoke->GetLocations()->InAt(0)), 0));
}

void IntrinsicLocationsBuilderARM64::VisitMemoryPeekShortNative(HInvoke* invoke) {
  CreateIntToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMemoryPeekShortNative(HInvoke* invoke) {
  codegen_->Load(Primitive::kPrimShort,
                 WRegisterFrom(invoke->GetLocations()->Out()),
                 MemOperand(XRegisterFrom(invoke->GetLocations()->InAt(0)), 0));
}

static void CreateIntIntToVoidLocations(ArenaAllocator* arena, HInvoke* invoke) {
  LocationSummary* locations = new (arena) LocationSummary(invoke,
                                                           LocationSummary::kNoCall,
                                                           kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RequiresRegister());
}

void IntrinsicLocationsBuilderARM64::VisitMemoryPokeByte(HInvoke* invoke) {
  CreateIntIntToVoidLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMemoryPokeByte(HInvoke* invoke) {
  codegen_->Store(Primitive::kPrimByte,
                  WRegisterFrom(invoke->GetLocations()->InAt(1)),
                  MemOperand(XRegisterFrom(invoke->GetLocations()->InAt(0)), 0));
}

void IntrinsicLocationsBuilderARM64::VisitMemoryPokeIntNative(HInvoke* invoke) {
  CreateIntIntToVoidLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMemoryPokeIntNative(HInvoke* invoke) {
  codegen_->Store(Primitive::kPrimInt,
                  WRegisterFrom(invoke->GetLocations()->InAt(1)),
                  MemOperand(XRegisterFrom(invoke->GetLocations()->InAt(0)), 0));
}

void IntrinsicLocationsBuilderARM64::VisitMemoryPokeLongNative(HInvoke* invoke) {
  CreateIntIntToVoidLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMemoryPokeLongNative(HInvoke* invoke) {
  codegen_->Store(Primitive::kPrimLong,
                  XRegisterFrom(invoke->GetLocations()->InAt(1)),
                  MemOperand(XRegisterFrom(invoke->GetLocations()->InAt(0)), 0));
}

void IntrinsicLocationsBuilderARM64::VisitMemoryPokeShortNative(HInvoke* invoke) {
  CreateIntIntToVoidLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMemoryPokeShortNative(HInvoke* invoke) {
  codegen_->Store(Primitive::kPrimShort,
                  WRegisterFrom(invoke->GetLocations()->InAt(1)),
                  MemOperand(XRegisterFrom(invoke->GetLocations()->InAt(0)), 0));
}

void IntrinsicLocationsBuilderARM64::VisitThreadCurrentThread(HInvoke* invoke) {
  LocationSummary* locations = new (arena_) LocationSummary(invoke,
                                                            LocationSummary::kNoCall,
                                                            kIntrinsified);
  locations->SetOut(Location::RequiresRegister());
}

void IntrinsicCodeGeneratorARM64::VisitThreadCurrentThread(HInvoke* invoke) {
  codegen_->Load(Primitive::kPrimNot, WRegisterFrom(invoke->GetLocations()->Out()),
                 MemOperand(tr, Thread::PeerOffset<8>().Int32Value()));
}

static void GenUnsafeGet(LocationSummary* locations, bool is_long, bool is_volatile,
                         CodeGeneratorARM64* codegen) {
  Register base = XRegisterFrom(locations->InAt(1));  // Object pointer.
  Register offset = XRegisterFrom(locations->InAt(2));  // Long offset.
  Register trg = is_long ? XRegisterFrom(locations->Out()) : WRegisterFrom(locations->Out());

  codegen->Load(is_long ? Primitive::kPrimLong : Primitive::kPrimInt,
                trg,
                MemOperand(base, offset, SXTX, 0));

  if (is_volatile) {
    codegen->GetAssembler()->vixl_masm_->Dmb(InnerShareable, BarrierReads);
  }
}

static void CreateIntIntIntToIntLocations(ArenaAllocator* arena, HInvoke* invoke) {
  LocationSummary* locations = new (arena) LocationSummary(invoke,
                                                           LocationSummary::kNoCall,
                                                           kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RequiresRegister());
  locations->SetInAt(2, Location::RequiresRegister());
  locations->SetOut(Location::SameAsFirstInput());
}

void IntrinsicLocationsBuilderARM64::VisitUnsafeGet(HInvoke* invoke) {
  CreateIntIntIntToIntLocations(arena_, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitUnsafeGetVolatile(HInvoke* invoke) {
  CreateIntIntIntToIntLocations(arena_, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitUnsafeGetLong(HInvoke* invoke) {
  CreateIntIntIntToIntLocations(arena_, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitUnsafeGetLongVolatile(HInvoke* invoke) {
  CreateIntIntIntToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitUnsafeGet(HInvoke* invoke) {
  GenUnsafeGet(invoke->GetLocations(), false, false, codegen_);
}
void IntrinsicCodeGeneratorARM64::VisitUnsafeGetVolatile(HInvoke* invoke) {
  GenUnsafeGet(invoke->GetLocations(), false, true, codegen_);
}
void IntrinsicCodeGeneratorARM64::VisitUnsafeGetLong(HInvoke* invoke) {
  GenUnsafeGet(invoke->GetLocations(), true, false, codegen_);
}
void IntrinsicCodeGeneratorARM64::VisitUnsafeGetLongVolatile(HInvoke* invoke) {
  GenUnsafeGet(invoke->GetLocations(), true, true, codegen_);
}

static void CreateIntIntIntIntToVoidPlusTempsLocations(ArenaAllocator* arena,
                                                       Primitive::Type type,
                                                       HInvoke* invoke) {
  LocationSummary* locations = new (arena) LocationSummary(invoke,
                                                           LocationSummary::kNoCall,
                                                           kIntrinsified);
  locations->SetInAt(0, Location::NoLocation());
  locations->SetInAt(1, Location::RequiresRegister());
  locations->SetInAt(2, Location::RequiresRegister());
  locations->SetInAt(3, Location::RequiresRegister());
  if (type == Primitive::kPrimNot) {
    // Need temp registers for card-marking.
    locations->AddTemp(Location::RequiresRegister());
    locations->AddTemp(Location::RequiresRegister());
  }
}

void IntrinsicLocationsBuilderARM64::VisitUnsafePut(HInvoke* invoke) {
  CreateIntIntIntIntToVoidPlusTempsLocations(arena_, Primitive::kPrimInt, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitUnsafePutOrdered(HInvoke* invoke) {
  CreateIntIntIntIntToVoidPlusTempsLocations(arena_, Primitive::kPrimInt, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitUnsafePutVolatile(HInvoke* invoke) {
  CreateIntIntIntIntToVoidPlusTempsLocations(arena_, Primitive::kPrimInt, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitUnsafePutObject(HInvoke* invoke) {
  CreateIntIntIntIntToVoidPlusTempsLocations(arena_, Primitive::kPrimNot, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitUnsafePutObjectOrdered(HInvoke* invoke) {
  CreateIntIntIntIntToVoidPlusTempsLocations(arena_, Primitive::kPrimNot, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitUnsafePutObjectVolatile(HInvoke* invoke) {
  CreateIntIntIntIntToVoidPlusTempsLocations(arena_, Primitive::kPrimNot, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitUnsafePutLong(HInvoke* invoke) {
  CreateIntIntIntIntToVoidPlusTempsLocations(arena_, Primitive::kPrimLong, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitUnsafePutLongOrdered(HInvoke* invoke) {
  CreateIntIntIntIntToVoidPlusTempsLocations(arena_, Primitive::kPrimLong, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitUnsafePutLongVolatile(HInvoke* invoke) {
  CreateIntIntIntIntToVoidPlusTempsLocations(arena_, Primitive::kPrimLong, invoke);
}

// We don't care for ordered: it requires an AnyStore barrier, which is already given by the x86
// memory model.
static void GenUnsafePut(LocationSummary* locations, Primitive::Type type, bool is_volatile,
                         bool is_ordered, CodeGeneratorARM64* codegen) {
  vixl::MacroAssembler* assembler = codegen->GetAssembler()->vixl_masm_;

  Register base = XRegisterFrom(locations->InAt(1));    // Object pointer.
  Register offset = XRegisterFrom(locations->InAt(2));  // Long offset.
  Register value = (type == Primitive::kPrimLong) ?
      XRegisterFrom(locations->InAt(3)) : WRegisterFrom(locations->InAt(3));

  if (is_volatile || is_ordered) {
    __ Dmb(InnerShareable, BarrierAll);
  }

  codegen->Store(type, value, MemOperand(base, offset, SXTX, 0));

  if (is_volatile) {
    __ Dmb(InnerShareable, BarrierAll);
  }

  if (type == Primitive::kPrimNot) {
    codegen->MarkGCCard(base.W(), value);
  }
}

void IntrinsicCodeGeneratorARM64::VisitUnsafePut(HInvoke* invoke) {
  GenUnsafePut(invoke->GetLocations(), Primitive::kPrimInt, false, false, codegen_);
}
void IntrinsicCodeGeneratorARM64::VisitUnsafePutOrdered(HInvoke* invoke) {
  GenUnsafePut(invoke->GetLocations(), Primitive::kPrimInt, false, true, codegen_);
}
void IntrinsicCodeGeneratorARM64::VisitUnsafePutVolatile(HInvoke* invoke) {
  GenUnsafePut(invoke->GetLocations(), Primitive::kPrimInt, true, false, codegen_);
}
void IntrinsicCodeGeneratorARM64::VisitUnsafePutObject(HInvoke* invoke) {
  GenUnsafePut(invoke->GetLocations(), Primitive::kPrimNot, false, false, codegen_);
}
void IntrinsicCodeGeneratorARM64::VisitUnsafePutObjectOrdered(HInvoke* invoke) {
  GenUnsafePut(invoke->GetLocations(), Primitive::kPrimNot, false, true, codegen_);
}
void IntrinsicCodeGeneratorARM64::VisitUnsafePutObjectVolatile(HInvoke* invoke) {
  GenUnsafePut(invoke->GetLocations(), Primitive::kPrimNot, true, false, codegen_);
}
void IntrinsicCodeGeneratorARM64::VisitUnsafePutLong(HInvoke* invoke) {
  GenUnsafePut(invoke->GetLocations(), Primitive::kPrimLong, false, false, codegen_);
}
void IntrinsicCodeGeneratorARM64::VisitUnsafePutLongOrdered(HInvoke* invoke) {
  GenUnsafePut(invoke->GetLocations(), Primitive::kPrimLong, false, true, codegen_);
}
void IntrinsicCodeGeneratorARM64::VisitUnsafePutLongVolatile(HInvoke* invoke) {
  GenUnsafePut(invoke->GetLocations(), Primitive::kPrimLong, true, false, codegen_);
}

// Unimplemented intrinsics.

#define UNIMPLEMENTED_INTRINSIC(Name)                                                  \
void IntrinsicLocationsBuilderARM64::Visit ## Name(HInvoke* invoke ATTRIBUTE_UNUSED) { \
}                                                                                      \
void IntrinsicCodeGeneratorARM64::Visit ## Name(HInvoke* invoke ATTRIBUTE_UNUSED) {    \
}

UNIMPLEMENTED_INTRINSIC(IntegerReverse)
UNIMPLEMENTED_INTRINSIC(LongReverse)
UNIMPLEMENTED_INTRINSIC(SystemArrayCopyChar)
UNIMPLEMENTED_INTRINSIC(StringCharAt)
UNIMPLEMENTED_INTRINSIC(StringCompareTo)
UNIMPLEMENTED_INTRINSIC(StringIsEmpty)
UNIMPLEMENTED_INTRINSIC(StringIndexOf)
UNIMPLEMENTED_INTRINSIC(StringIndexOfAfter)
UNIMPLEMENTED_INTRINSIC(StringLength)
UNIMPLEMENTED_INTRINSIC(UnsafeCASInt)
UNIMPLEMENTED_INTRINSIC(UnsafeCASLong)
UNIMPLEMENTED_INTRINSIC(UnsafeCASObject)
UNIMPLEMENTED_INTRINSIC(ReferenceGetReferent)

}  // namespace arm64
}  // namespace art
