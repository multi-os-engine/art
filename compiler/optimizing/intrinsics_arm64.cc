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

Register XRegisterFrom(Location location) {
  DCHECK(location.IsRegister());
  return Register::XRegFromCode(VIXLRegCodeFromART(location.reg()));
}

Register WRegisterFrom(Location location) {
  DCHECK(location.IsRegister());
  return Register::WRegFromCode(VIXLRegCodeFromART(location.reg()));
}

FPRegister DRegisterFrom(Location location) {
  DCHECK(location.IsFpuRegister());
  return FPRegister::DRegFromCode(location.reg());
}

FPRegister SRegisterFrom(Location location) {
  DCHECK(location.IsFpuRegister());
  return FPRegister::SRegFromCode(location.reg());
}

bool IsFPType(Primitive::Type type) {
  return type == Primitive::kPrimFloat || type == Primitive::kPrimDouble;
}

Register RegisterFrom(Location location, Primitive::Type type) {
  DCHECK(type != Primitive::kPrimVoid && !IsFPType(type));
  return (type == Primitive::kPrimLong) ? XRegisterFrom(location) : WRegisterFrom(location);
}

FPRegister FPRegisterFrom(Location location, Primitive::Type type) {
  DCHECK(IsFPType(type));
  return type == Primitive::kPrimDouble ? DRegisterFrom(location) : SRegisterFrom(location);
}

MemOperand AbsoluteHeapOperandFrom(Location location, size_t offset = 0) {
  return MemOperand(XRegisterFrom(location), offset);
}

}  // namespace

static constexpr bool kIntrinsified = true;

vixl::MacroAssembler* IntrinsicCodeGeneratorARM64::GetVIXLAssembler() {
  return codegen_->GetAssembler()->vixl_masm_;
}

ArenaAllocator* IntrinsicCodeGeneratorARM64::GetArena() {
  return codegen_->GetGraph()->GetArena();
}

#define __ codegen->GetAssembler()->vixl_masm_->

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
    case Primitive::kPrimLong:
    case Primitive::kPrimNot: {
      Register trg_reg = RegisterFrom(trg, type);
      Register res_reg = RegisterFrom(ARM64ReturnLocation(type), type);
      if (trg_reg.code() != res_reg.code()) {
        __ Mov(trg_reg, res_reg);
      }
      break;
    }

    case Primitive::kPrimVoid:
      LOG(FATAL) << "Unexpected void type for valid location " << trg;
      UNREACHABLE();

    case Primitive::kPrimFloat:
    case Primitive::kPrimDouble: {
      FPRegister trg_reg = FPRegisterFrom(trg, type);
      FPRegister res_reg = FPRegisterFrom(ARM64ReturnLocation(type), type);
      if (trg_reg.code() != res_reg.code()) {
        __ Fmov(trg_reg, res_reg);
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
      codegen->GenerateStaticOrDirectCall(invoke_->AsInvokeStaticOrDirect(), w0);
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
    __ B(GetExitLabel());
  }

 private:
  // The instruction where this slow path is happening.
  HInvoke* const invoke_;

  DISALLOW_COPY_AND_ASSIGN(IntrinsicSlowPathARM64);
};

#undef __

bool IntrinsicLocationsBuilderARM64::TryDispatch(HInvoke* invoke) {
  UNUSED(arena_);
  Dispatch(invoke);
  const LocationSummary* res = invoke->GetLocations();
  return res != nullptr && res->Intrinsified();
}

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
  MoveFPToInt(invoke->GetLocations(), true, GetVIXLAssembler());
}
void IntrinsicCodeGeneratorARM64::VisitDoubleLongBitsToDouble(HInvoke* invoke) {
  MoveIntToFP(invoke->GetLocations(), true, GetVIXLAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitFloatFloatToRawIntBits(HInvoke* invoke) {
  CreateFPToIntLocations(arena_, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitFloatIntBitsToFloat(HInvoke* invoke) {
  CreateIntToFPLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitFloatFloatToRawIntBits(HInvoke* invoke) {
  MoveFPToInt(invoke->GetLocations(), false, GetVIXLAssembler());
}
void IntrinsicCodeGeneratorARM64::VisitFloatIntBitsToFloat(HInvoke* invoke) {
  MoveIntToFP(invoke->GetLocations(), false, GetVIXLAssembler());
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
    case Primitive::kPrimLong:
      __ Rev(RegisterFrom(out, size), RegisterFrom(in, size));
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
  GenReverseBytes(invoke->GetLocations(), Primitive::kPrimInt, GetVIXLAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitLongReverseBytes(HInvoke* invoke) {
  CreateIntToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitLongReverseBytes(HInvoke* invoke) {
  GenReverseBytes(invoke->GetLocations(), Primitive::kPrimLong, GetVIXLAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitShortReverseBytes(HInvoke* invoke) {
  CreateIntToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitShortReverseBytes(HInvoke* invoke) {
  GenReverseBytes(invoke->GetLocations(), Primitive::kPrimShort, GetVIXLAssembler());
}

static void GenReverse(LocationSummary* locations,
                       Primitive::Type size,
                       vixl::MacroAssembler* assembler) {
  DCHECK(size == Primitive::kPrimInt || size == Primitive::kPrimLong);

  Location in = locations->InAt(0);
  Location out = locations->Out();

  __ Rbit(RegisterFrom(out, size), RegisterFrom(in, size));
}

void IntrinsicLocationsBuilderARM64::VisitIntegerReverse(HInvoke* invoke) {
  CreateIntToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitIntegerReverse(HInvoke* invoke) {
  GenReverse(invoke->GetLocations(), Primitive::kPrimInt, GetVIXLAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitLongReverse(HInvoke* invoke) {
  CreateIntToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitLongReverse(HInvoke* invoke) {
  GenReverse(invoke->GetLocations(), Primitive::kPrimLong, GetVIXLAssembler());
}

static void CreateFPToFPLocations(ArenaAllocator* arena, HInvoke* invoke) {
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

  FPRegister in_reg = is64bit ? DRegisterFrom(in) : SRegisterFrom(in);
  FPRegister out_reg = is64bit ? DRegisterFrom(out) : SRegisterFrom(out);

  __ Fabs(out_reg, in_reg);
}

void IntrinsicLocationsBuilderARM64::VisitMathAbsDouble(HInvoke* invoke) {
  CreateFPToFPLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathAbsDouble(HInvoke* invoke) {
  MathAbsFP(invoke->GetLocations(), true, GetVIXLAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitMathAbsFloat(HInvoke* invoke) {
  CreateFPToFPLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathAbsFloat(HInvoke* invoke) {
  MathAbsFP(invoke->GetLocations(), false, GetVIXLAssembler());
}

static void CreateIntToInt(ArenaAllocator* arena, HInvoke* invoke) {
  LocationSummary* locations = new (arena) LocationSummary(invoke,
                                                           LocationSummary::kNoCall,
                                                           kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::SameAsFirstInput());
}

static void GenAbsInteger(LocationSummary* locations, bool is64bit,
                          vixl::MacroAssembler* assembler) {
  Location in = locations->InAt(0);
  Location output = locations->Out();

  Register in_reg = is64bit ? XRegisterFrom(in) : WRegisterFrom(in);
  Register out_reg = is64bit ? XRegisterFrom(output) : WRegisterFrom(output);

  __ Cmp(in_reg, Operand(0));
  __ Cneg(out_reg, in_reg, lt);
}

void IntrinsicLocationsBuilderARM64::VisitMathAbsInt(HInvoke* invoke) {
  CreateIntToInt(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathAbsInt(HInvoke* invoke) {
  GenAbsInteger(invoke->GetLocations(), false, GetVIXLAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitMathAbsLong(HInvoke* invoke) {
  CreateIntToInt(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathAbsLong(HInvoke* invoke) {
  GenAbsInteger(invoke->GetLocations(), true, GetVIXLAssembler());
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

static void CreateFPFPToFPLocations(ArenaAllocator* arena, HInvoke* invoke) {
  LocationSummary* locations = new (arena) LocationSummary(invoke,
                                                           LocationSummary::kNoCall,
                                                           kIntrinsified);
  locations->SetInAt(0, Location::RequiresFpuRegister());
  locations->SetInAt(1, Location::RequiresFpuRegister());
  locations->SetOut(Location::RequiresFpuRegister());
}

void IntrinsicLocationsBuilderARM64::VisitMathMinDoubleDouble(HInvoke* invoke) {
  CreateFPFPToFPLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathMinDoubleDouble(HInvoke* invoke) {
  GenMinMaxFP(invoke->GetLocations(), true, true, GetVIXLAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitMathMinFloatFloat(HInvoke* invoke) {
  CreateFPFPToFPLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathMinFloatFloat(HInvoke* invoke) {
  GenMinMaxFP(invoke->GetLocations(), true, false, GetVIXLAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitMathMaxDoubleDouble(HInvoke* invoke) {
  CreateFPFPToFPLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathMaxDoubleDouble(HInvoke* invoke) {
  GenMinMaxFP(invoke->GetLocations(), false, true, GetVIXLAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitMathMaxFloatFloat(HInvoke* invoke) {
  CreateFPFPToFPLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathMaxFloatFloat(HInvoke* invoke) {
  GenMinMaxFP(invoke->GetLocations(), false, false, GetVIXLAssembler());
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
  GenMinMax(invoke->GetLocations(), true, false, GetVIXLAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitMathMinLongLong(HInvoke* invoke) {
  CreateIntIntToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathMinLongLong(HInvoke* invoke) {
  GenMinMax(invoke->GetLocations(), true, true, GetVIXLAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitMathMaxIntInt(HInvoke* invoke) {
  CreateIntIntToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathMaxIntInt(HInvoke* invoke) {
  GenMinMax(invoke->GetLocations(), false, false, GetVIXLAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitMathMaxLongLong(HInvoke* invoke) {
  CreateIntIntToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathMaxLongLong(HInvoke* invoke) {
  GenMinMax(invoke->GetLocations(), false, true, GetVIXLAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitMathSqrt(HInvoke* invoke) {
  CreateFPToFPLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathSqrt(HInvoke* invoke) {
  LocationSummary* locations = invoke->GetLocations();
  vixl::MacroAssembler* assembler = GetVIXLAssembler();
  __ Fsqrt(DRegisterFrom(locations->Out()), DRegisterFrom(locations->InAt(0)));
}

void IntrinsicLocationsBuilderARM64::VisitMathCeil(HInvoke* invoke) {
  CreateFPToFPLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathCeil(HInvoke* invoke) {
  LocationSummary* locations = invoke->GetLocations();
  vixl::MacroAssembler* assembler = GetVIXLAssembler();
  __ Frintp(DRegisterFrom(locations->Out()), DRegisterFrom(locations->InAt(0)));
}

void IntrinsicLocationsBuilderARM64::VisitMathFloor(HInvoke* invoke) {
  CreateFPToFPLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathFloor(HInvoke* invoke) {
  LocationSummary* locations = invoke->GetLocations();
  vixl::MacroAssembler* assembler = GetVIXLAssembler();
  __ Frintm(DRegisterFrom(locations->Out()), DRegisterFrom(locations->InAt(0)));
}

void IntrinsicLocationsBuilderARM64::VisitMathRint(HInvoke* invoke) {
  CreateFPToFPLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathRint(HInvoke* invoke) {
  LocationSummary* locations = invoke->GetLocations();
  vixl::MacroAssembler* assembler = GetVIXLAssembler();
  __ Frintn(DRegisterFrom(locations->Out()), DRegisterFrom(locations->InAt(0)));
}

static void CreateFPToIntPlusTempLocations(ArenaAllocator* arena, HInvoke* invoke) {
  LocationSummary* locations = new (arena) LocationSummary(invoke,
                                                           LocationSummary::kNoCall,
                                                           kIntrinsified);
  locations->SetInAt(0, Location::RequiresFpuRegister());
  locations->SetOut(Location::RequiresRegister());
}

static void GenMathRound(LocationSummary* locations, bool is_double,
                         vixl::MacroAssembler* assembler) {
  FPRegister in_reg = is_double ?
      DRegisterFrom(locations->InAt(0)) : SRegisterFrom(locations->InAt(0));
  Register out_reg = is_double ?
      XRegisterFrom(locations->Out()) : WRegisterFrom(locations->Out());
  UseScratchRegisterScope temps(assembler);
  FPRegister temp1_reg = temps.AcquireSameSizeAs(in_reg);

  // 0.5 can be encoded as an immediate, so use fmov.
  if (is_double) {
    __ Fmov(temp1_reg, static_cast<double>(0.5));
  } else {
    __ Fmov(temp1_reg, static_cast<float>(0.5));
  }
  __ Fadd(temp1_reg, in_reg, temp1_reg);
  __ Fcvtms(out_reg, temp1_reg);
}

void IntrinsicLocationsBuilderARM64::VisitMathRoundDouble(HInvoke* invoke) {
  CreateFPToIntPlusTempLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathRoundDouble(HInvoke* invoke) {
  GenMathRound(invoke->GetLocations(), true, GetVIXLAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitMathRoundFloat(HInvoke* invoke) {
  CreateFPToIntPlusTempLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathRoundFloat(HInvoke* invoke) {
  GenMathRound(invoke->GetLocations(), false, GetVIXLAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitMemoryPeekByte(HInvoke* invoke) {
  CreateIntToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMemoryPeekByte(HInvoke* invoke) {
  vixl::MacroAssembler* assembler = GetVIXLAssembler();
  __ Ldrsb(WRegisterFrom(invoke->GetLocations()->Out()),
          AbsoluteHeapOperandFrom(invoke->GetLocations()->InAt(0), 0));
}

void IntrinsicLocationsBuilderARM64::VisitMemoryPeekIntNative(HInvoke* invoke) {
  CreateIntToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMemoryPeekIntNative(HInvoke* invoke) {
  vixl::MacroAssembler* assembler = GetVIXLAssembler();
  __ Ldr(WRegisterFrom(invoke->GetLocations()->Out()),
         AbsoluteHeapOperandFrom(invoke->GetLocations()->InAt(0), 0));
}

void IntrinsicLocationsBuilderARM64::VisitMemoryPeekLongNative(HInvoke* invoke) {
  CreateIntToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMemoryPeekLongNative(HInvoke* invoke) {
  vixl::MacroAssembler* assembler = GetVIXLAssembler();
  __ Ldr(XRegisterFrom(invoke->GetLocations()->Out()),
         AbsoluteHeapOperandFrom(invoke->GetLocations()->InAt(0), 0));
}

void IntrinsicLocationsBuilderARM64::VisitMemoryPeekShortNative(HInvoke* invoke) {
  CreateIntToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMemoryPeekShortNative(HInvoke* invoke) {
  vixl::MacroAssembler* assembler = GetVIXLAssembler();
  __ Ldrsh(WRegisterFrom(invoke->GetLocations()->Out()),
           AbsoluteHeapOperandFrom(invoke->GetLocations()->InAt(0), 0));
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
  vixl::MacroAssembler* assembler = GetVIXLAssembler();
  __ Strb(WRegisterFrom(invoke->GetLocations()->InAt(1)),
          AbsoluteHeapOperandFrom(invoke->GetLocations()->InAt(0), 0));
}

void IntrinsicLocationsBuilderARM64::VisitMemoryPokeIntNative(HInvoke* invoke) {
  CreateIntIntToVoidLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMemoryPokeIntNative(HInvoke* invoke) {
  vixl::MacroAssembler* assembler = GetVIXLAssembler();
  __ Str(WRegisterFrom(invoke->GetLocations()->InAt(1)),
         AbsoluteHeapOperandFrom(invoke->GetLocations()->InAt(0), 0));
}

void IntrinsicLocationsBuilderARM64::VisitMemoryPokeLongNative(HInvoke* invoke) {
  CreateIntIntToVoidLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMemoryPokeLongNative(HInvoke* invoke) {
  vixl::MacroAssembler* assembler = GetVIXLAssembler();
  __ Str(XRegisterFrom(invoke->GetLocations()->InAt(1)),
         AbsoluteHeapOperandFrom(invoke->GetLocations()->InAt(0), 0));
}

void IntrinsicLocationsBuilderARM64::VisitMemoryPokeShortNative(HInvoke* invoke) {
  CreateIntIntToVoidLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMemoryPokeShortNative(HInvoke* invoke) {
  vixl::MacroAssembler* assembler = GetVIXLAssembler();
  __ Strh(WRegisterFrom(invoke->GetLocations()->InAt(1)),
          AbsoluteHeapOperandFrom(invoke->GetLocations()->InAt(0), 0));
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

static void GenUnsafeGet(LocationSummary* locations, Primitive::Type type, bool is_volatile,
                         CodeGeneratorARM64* codegen) {
  DCHECK((type == Primitive::kPrimInt) ||
         (type == Primitive::kPrimLong) ||
         (type == Primitive::kPrimNot));
  vixl::MacroAssembler* assembler = codegen->GetAssembler()->vixl_masm_;
  Register base = WRegisterFrom(locations->InAt(1));    // Object pointer.
  Register offset = XRegisterFrom(locations->InAt(2));  // Long offset.
  Register trg = RegisterFrom(locations->Out(), type);

  MemOperand mem_op(base.X(), offset);
  if (is_volatile) {
    if (kUseAcquireRelease) {
      codegen->LoadAcquire(type, trg, mem_op);
    } else {
      codegen->Load(type, trg, mem_op);
      __ Dmb(InnerShareable, BarrierReads);
    }
  } else {
    codegen->Load(type, trg, mem_op);
  }
}

static void CreateIntIntIntToIntLocations(ArenaAllocator* arena, HInvoke* invoke) {
  LocationSummary* locations = new (arena) LocationSummary(invoke,
                                                           LocationSummary::kNoCall,
                                                           kIntrinsified);
  locations->SetInAt(0, Location::NoLocation());        // Unused receiver.
  locations->SetInAt(1, Location::RequiresRegister());
  locations->SetInAt(2, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister());
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
void IntrinsicLocationsBuilderARM64::VisitUnsafeGetObject(HInvoke* invoke) {
  CreateIntIntIntToIntLocations(arena_, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitUnsafeGetObjectVolatile(HInvoke* invoke) {
  CreateIntIntIntToIntLocations(arena_, invoke);
}


void IntrinsicCodeGeneratorARM64::VisitUnsafeGet(HInvoke* invoke) {
  GenUnsafeGet(invoke->GetLocations(), Primitive::kPrimInt, false, codegen_);
}
void IntrinsicCodeGeneratorARM64::VisitUnsafeGetVolatile(HInvoke* invoke) {
  GenUnsafeGet(invoke->GetLocations(), Primitive::kPrimInt, true, codegen_);
}
void IntrinsicCodeGeneratorARM64::VisitUnsafeGetLong(HInvoke* invoke) {
  GenUnsafeGet(invoke->GetLocations(), Primitive::kPrimLong, false, codegen_);
}
void IntrinsicCodeGeneratorARM64::VisitUnsafeGetLongVolatile(HInvoke* invoke) {
  GenUnsafeGet(invoke->GetLocations(), Primitive::kPrimLong, true, codegen_);
}
void IntrinsicCodeGeneratorARM64::VisitUnsafeGetObject(HInvoke* invoke) {
  GenUnsafeGet(invoke->GetLocations(), Primitive::kPrimNot, false, codegen_);
}
void IntrinsicCodeGeneratorARM64::VisitUnsafeGetObjectVolatile(HInvoke* invoke) {
  GenUnsafeGet(invoke->GetLocations(), Primitive::kPrimNot, true, codegen_);
}

static void CreateIntIntIntIntToVoid(ArenaAllocator* arena, HInvoke* invoke) {
  LocationSummary* locations = new (arena) LocationSummary(invoke,
                                                           LocationSummary::kNoCall,
                                                           kIntrinsified);
  locations->SetInAt(0, Location::NoLocation());        // Unused receiver.
  locations->SetInAt(1, Location::RequiresRegister());
  locations->SetInAt(2, Location::RequiresRegister());
  locations->SetInAt(3, Location::RequiresRegister());
}

void IntrinsicLocationsBuilderARM64::VisitUnsafePut(HInvoke* invoke) {
  CreateIntIntIntIntToVoid(arena_, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitUnsafePutOrdered(HInvoke* invoke) {
  CreateIntIntIntIntToVoid(arena_, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitUnsafePutVolatile(HInvoke* invoke) {
  CreateIntIntIntIntToVoid(arena_, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitUnsafePutObject(HInvoke* invoke) {
  CreateIntIntIntIntToVoid(arena_, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitUnsafePutObjectOrdered(HInvoke* invoke) {
  CreateIntIntIntIntToVoid(arena_, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitUnsafePutObjectVolatile(HInvoke* invoke) {
  CreateIntIntIntIntToVoid(arena_, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitUnsafePutLong(HInvoke* invoke) {
  CreateIntIntIntIntToVoid(arena_, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitUnsafePutLongOrdered(HInvoke* invoke) {
  CreateIntIntIntIntToVoid(arena_, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitUnsafePutLongVolatile(HInvoke* invoke) {
  CreateIntIntIntIntToVoid(arena_, invoke);
}

static void GenUnsafePut(LocationSummary* locations, Primitive::Type type, bool is_volatile,
                         bool is_ordered, CodeGeneratorARM64* codegen) {
  vixl::MacroAssembler* assembler = codegen->GetAssembler()->vixl_masm_;

  Register base = WRegisterFrom(locations->InAt(1));    // Object pointer.
  Register offset = XRegisterFrom(locations->InAt(2));  // Long offset.
  Register value = RegisterFrom(locations->InAt(3), type);

  MemOperand mem_op(base.X(), offset);

  if (is_volatile || is_ordered) {
    if (kUseAcquireRelease) {
      codegen->StoreRelease(type, value, mem_op);
    } else {
      __ Dmb(InnerShareable, BarrierAll);
      codegen->Store(type, value, mem_op);
      if (is_volatile) {
        __ Dmb(InnerShareable, BarrierReads);
      }
    }
  } else {
    codegen->Store(type, value, mem_op);
  }

  if (type == Primitive::kPrimNot) {
    codegen->MarkGCCard(base, value);
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

static void CreateIntIntIntIntIntToIntPlusTempsLocations(ArenaAllocator* arena, HInvoke* invoke) {
  LocationSummary* locations = new (arena) LocationSummary(invoke,
                                                           LocationSummary::kNoCall,
                                                           kIntrinsified);
  locations->SetInAt(0, Location::NoLocation());        // Unused receiver.
  locations->SetInAt(1, Location::RequiresRegister());
  locations->SetInAt(2, Location::RequiresRegister());
  locations->SetInAt(3, Location::RequiresRegister());
  locations->SetInAt(4, Location::RequiresRegister());

  locations->SetOut(Location::RequiresRegister());

  locations->AddTemp(Location::RequiresRegister());
  locations->AddTemp(Location::RequiresRegister());
}

static void GenCas(LocationSummary* locations, Primitive::Type type, CodeGeneratorARM64* codegen) {
  vixl::MacroAssembler* assembler = codegen->GetAssembler()->vixl_masm_;

  Register out = WRegisterFrom(locations->Out());                  // Boolean result.

  Register base = WRegisterFrom(locations->InAt(1));               // Object pointer.
  Register offset = XRegisterFrom(locations->InAt(2));             // Long offset.
  Register expected = RegisterFrom(locations->InAt(3), type);      // Expected.
  Register value = RegisterFrom(locations->InAt(4), type);         // Value.

  // This needs to be before the temp registers, as MarkGCCard also uses VIXL temps.
  if (type == Primitive::kPrimNot) {
    // Mark card for object assuming new value is stored.
    codegen->MarkGCCard(base.W(), value);
  }

  UseScratchRegisterScope temps(assembler);
  Register tmp_ptr = temps.AcquireX();                             // Pointer to actual memory.
  Register tmp_value = temps.AcquireSameSizeAs(value);             // Value in memory.

  Register tmp_32 = tmp_value.W();

  __ Add(tmp_ptr, base.X(), Operand(offset));

  // do {
  //   tmp_value = [tmp_ptr] - expected;
  // } while (tmp_value == 0 && failure([tmp_ptr] <- r_new_value));
  // result = tmp_value != 0;

  vixl::Label loop_head, exit_loop;
  __ Bind(&loop_head);

  __ Ldaxr(tmp_value, MemOperand(tmp_ptr));
  __ Cmp(tmp_value, expected);
  __ B(&exit_loop, ne);

  __ Stlxr(tmp_32, value, MemOperand(tmp_ptr));
  __ Cbnz(tmp_32, &loop_head);

  __ Bind(&exit_loop);
  __ Cset(out, eq);
}

void IntrinsicLocationsBuilderARM64::VisitUnsafeCASInt(HInvoke* invoke) {
  CreateIntIntIntIntIntToIntPlusTempsLocations(arena_, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitUnsafeCASLong(HInvoke* invoke) {
  CreateIntIntIntIntIntToIntPlusTempsLocations(arena_, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitUnsafeCASObject(HInvoke* invoke) {
  CreateIntIntIntIntIntToIntPlusTempsLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitUnsafeCASInt(HInvoke* invoke) {
  GenCas(invoke->GetLocations(), Primitive::kPrimInt, codegen_);
}
void IntrinsicCodeGeneratorARM64::VisitUnsafeCASLong(HInvoke* invoke) {
  GenCas(invoke->GetLocations(), Primitive::kPrimLong, codegen_);
}
void IntrinsicCodeGeneratorARM64::VisitUnsafeCASObject(HInvoke* invoke) {
  GenCas(invoke->GetLocations(), Primitive::kPrimNot, codegen_);
}

void IntrinsicLocationsBuilderARM64::VisitStringCharAt(HInvoke* invoke) {
  // The inputs plus one temp.
  LocationSummary* locations = new (arena_) LocationSummary(invoke,
                                                            LocationSummary::kCallOnSlowPath,
                                                            kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister());
}

void IntrinsicCodeGeneratorARM64::VisitStringCharAt(HInvoke* invoke) {
  vixl::MacroAssembler* assembler = GetVIXLAssembler();
  LocationSummary* locations = invoke->GetLocations();

  // Location of reference to data array
  const int32_t value_offset = mirror::String::ValueOffset().Int32Value();
  // Location of count
  const int32_t count_offset = mirror::String::CountOffset().Int32Value();
  // Starting offset within data array
  const int32_t offset_offset = mirror::String::OffsetOffset().Int32Value();
  // Start of char data with array_
  const int32_t data_offset = mirror::Array::DataOffset(sizeof(uint16_t)).Int32Value();

  Register obj = WRegisterFrom(locations->InAt(0));  // String object pointer.
  Register idx = WRegisterFrom(locations->InAt(1));  // Index of character.
  Register out = WRegisterFrom(locations->Out());    // Result character.

  UseScratchRegisterScope temps(assembler);
  Register temp = temps.AcquireW();
  Register array_temp = temps.AcquireW();            // We can trade this for worse worse
                                                     // scheduling.

  // Note: Nullcheck has been done before in a HNullCheck before the HInvokeVirtual. If/when we
  //       move to (coalesced) implicit checks, we have to do a null check below.
  DCHECK(!kCoalescedImplicitNullCheck);

  // TODO: Maybe we can support range check elimination. Overall, though, I think it's not worth
  //       the cost.
  // TODO: For simplicity, the index parameter is requested in a register, so different from Quick
  //       we will not optimize the code for constants (which would save a register).

  SlowPathCodeARM64* slow_path = new (GetArena()) IntrinsicSlowPathARM64(invoke);
  codegen_->AddSlowPath(slow_path);

  __ Ldr(temp, MemOperand(obj.X(), count_offset));          // temp = str.length.
  __ Cmp(idx, temp);
  __ B(hi, slow_path->GetEntryLabel());

  // Index computation.
  __ Ldr(temp, MemOperand(obj.X(), offset_offset));         // temp := str.offset.
  __ Ldr(array_temp, MemOperand(obj.X(), value_offset));    // array_temp := str.offset.
  __ Add(temp, temp, idx);
  DCHECK_EQ(data_offset % 2, 0);                            // We'll compensate by shifting.
  __ Add(temp, temp, Operand(data_offset / 2));

  // Load the value.
  __ Ldrh(out, MemOperand(array_temp.X(), temp, UXTW, 1));  // out := array_temp[temp].

  __ Bind(slow_path->GetExitLabel());
}

//  // Location of reference to data array
//  int value_offset = mirror::String::ValueOffset().Int32Value();
//  // Location of count
//  int count_offset = mirror::String::CountOffset().Int32Value();
//  // Starting offset within data array
//  int offset_offset = mirror::String::OffsetOffset().Int32Value();
//  // Start of char data with array_
//  int data_offset = mirror::Array::DataOffset(sizeof(uint16_t)).Int32Value();
//
//  RegLocation rl_obj = info->args[0];
//  RegLocation rl_idx = info->args[1];
//  rl_obj = LoadValue(rl_obj, kRefReg);
//  rl_idx = LoadValue(rl_idx, kCoreReg);
//  RegStorage reg_max;
//  GenNullCheck(rl_obj.reg, info->opt_flags);
//  bool range_check = (!(info->opt_flags & MIR_IGNORE_RANGE_CHECK));
//  LIR* range_check_branch = nullptr;
//  RegStorage reg_off;
//  RegStorage reg_ptr;
//  reg_off = AllocTemp();
//  reg_ptr = AllocTempRef();
//  if (range_check) {
//    reg_max = AllocTemp();
//    Load32Disp(rl_obj.reg, count_offset, reg_max);
//    MarkPossibleNullPointerException(info->opt_flags);
//  }
//  Load32Disp(rl_obj.reg, offset_offset, reg_off);
//  MarkPossibleNullPointerException(info->opt_flags);
//  LoadRefDisp(rl_obj.reg, value_offset, reg_ptr, kNotVolatile);
//  if (range_check) {
//    // Set up a slow path to allow retry in case of bounds violation */
//    OpRegReg(kOpCmp, rl_idx.reg, reg_max);
//    FreeTemp(reg_max);
//    range_check_branch = OpCondBranch(kCondUge, nullptr);
//  }
//  OpRegImm(kOpAdd, reg_ptr, data_offset);
//  if (rl_idx.is_const) {
//    OpRegImm(kOpAdd, reg_off, mir_graph_->ConstantValue(rl_idx.orig_sreg));
//  } else {
//    OpRegReg(kOpAdd, reg_off, rl_idx.reg);
//  }
//  FreeTemp(rl_obj.reg);
//  if (rl_idx.location == kLocPhysReg) {
//    FreeTemp(rl_idx.reg);
//  }
//  RegLocation rl_dest = InlineTarget(info);
//  RegLocation rl_result = EvalLoc(rl_dest, kCoreReg, true);
//  LoadBaseIndexed(reg_ptr, reg_off, rl_result.reg, 1, kUnsignedHalf);
//  FreeTemp(reg_off);
//  FreeTemp(reg_ptr);
//  StoreValue(rl_dest, rl_result);
//  if (range_check) {
//    DCHECK(range_check_branch != nullptr);
//    info->opt_flags |= MIR_IGNORE_NULL_CHECK;  // Record that we've already null checked.
//    AddIntrinsicSlowPath(info, range_check_branch);
//  }
//  return true;

// Unimplemented intrinsics.

#define UNIMPLEMENTED_INTRINSIC(Name)                                                  \
void IntrinsicLocationsBuilderARM64::Visit ## Name(HInvoke* invoke ATTRIBUTE_UNUSED) { \
}                                                                                      \
void IntrinsicCodeGeneratorARM64::Visit ## Name(HInvoke* invoke ATTRIBUTE_UNUSED) {    \
}

UNIMPLEMENTED_INTRINSIC(SystemArrayCopyChar)
UNIMPLEMENTED_INTRINSIC(StringCompareTo)
UNIMPLEMENTED_INTRINSIC(StringIsEmpty)  // Might not want to do these two anyways, inlining should
UNIMPLEMENTED_INTRINSIC(StringLength)   // be good enough here.
UNIMPLEMENTED_INTRINSIC(StringIndexOf)
UNIMPLEMENTED_INTRINSIC(StringIndexOfAfter)
UNIMPLEMENTED_INTRINSIC(ReferenceGetReferent)

}  // namespace arm64
}  // namespace art
