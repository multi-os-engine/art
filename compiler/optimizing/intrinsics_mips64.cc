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

#include "arch/mips64/instruction_set_features_mips64.h"
#include "art_method.h"
#include "code_generator_mips64.h"
#include "entrypoints/quick/quick_entrypoints.h"
#include "intrinsics.h"
#include "mirror/array-inl.h"
#include "mirror/string.h"
#include "thread.h"
#include "utils/mips64/assembler_mips64.h"
#include "utils/mips64/constants_mips64.h"

#include "intrinsics_mips64.h"

namespace art {

namespace mips64 {

// We need extra temporary/scratch registers (in addition to AT) in some cases.
static constexpr GpuRegister TMP = T8;

// ART Thread Register.
static constexpr GpuRegister TR = S1;

IntrinsicLocationsBuilderMIPS64::IntrinsicLocationsBuilderMIPS64(CodeGeneratorMIPS64* codegen)
  : arena_(codegen->GetGraph()->GetArena()) {
}

Mips64Assembler* IntrinsicCodeGeneratorMIPS64::GetAssembler() {
  return reinterpret_cast<Mips64Assembler*>(codegen_->GetAssembler());
}

ArenaAllocator* IntrinsicCodeGeneratorMIPS64::GetAllocator() {
  return codegen_->GetGraph()->GetArena();
}

bool IntrinsicLocationsBuilderMIPS64::TryDispatch(HInvoke* invoke) {
  Dispatch(invoke);
  LocationSummary* res = invoke->GetLocations();
  return res != nullptr && res->Intrinsified();
}

#define __ masm->

static void CreateFPToIntLocations(ArenaAllocator* arena, HInvoke* invoke) {
  LocationSummary* locations = new (arena) LocationSummary(invoke,
                                                           LocationSummary::kNoCall,
                                                           kIntrinsified);
  locations->SetInAt(0, Location::RequiresFpuRegister());
  locations->SetOut(Location::RequiresRegister());
}

static void MoveFPToInt(LocationSummary* locations, bool is64bit, Mips64Assembler* masm) {
  Location input = locations->InAt(0);
  Location output = locations->Out();

  if (is64bit) {
    __ Dmfc1(output.AsRegister<GpuRegister>(), input.AsFpuRegister<FpuRegister>());
  } else {
    __ Mfc1(output.AsRegister<GpuRegister>(), input.AsFpuRegister<FpuRegister>());
  }
}

// long java.lang.Double.doubleToRawLongBits(double)
void IntrinsicLocationsBuilderMIPS64::VisitDoubleDoubleToRawLongBits(HInvoke* invoke) {
  CreateFPToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorMIPS64::VisitDoubleDoubleToRawLongBits(HInvoke* invoke) {
  MoveFPToInt(invoke->GetLocations(), true, GetAssembler());
}

// int java.lang.Float.floatToRawIntBits(float)
void IntrinsicLocationsBuilderMIPS64::VisitFloatFloatToRawIntBits(HInvoke* invoke) {
  CreateFPToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorMIPS64::VisitFloatFloatToRawIntBits(HInvoke* invoke) {
  MoveFPToInt(invoke->GetLocations(), false, GetAssembler());
}

static void CreateIntToFPLocations(ArenaAllocator* arena, HInvoke* invoke) {
  LocationSummary* locations = new (arena) LocationSummary(invoke,
                                                           LocationSummary::kNoCall,
                                                           kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresFpuRegister());
}

static void MoveIntToFP(LocationSummary* locations, bool is64bit, Mips64Assembler* masm) {
  Location input = locations->InAt(0);
  Location output = locations->Out();

  if (is64bit) {
    __ Dmtc1(input.AsRegister<GpuRegister>(), output.AsFpuRegister<FpuRegister>());
  } else {
    __ Mtc1(input.AsRegister<GpuRegister>(), output.AsFpuRegister<FpuRegister>());
  }
}

// double java.lang.Double.longBitsToDouble(long)
void IntrinsicLocationsBuilderMIPS64::VisitDoubleLongBitsToDouble(HInvoke* invoke) {
  CreateIntToFPLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorMIPS64::VisitDoubleLongBitsToDouble(HInvoke* invoke) {
  MoveIntToFP(invoke->GetLocations(), true, GetAssembler());
}

// float java.lang.Float.intBitsToFloat(int)
void IntrinsicLocationsBuilderMIPS64::VisitFloatIntBitsToFloat(HInvoke* invoke) {
  CreateIntToFPLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorMIPS64::VisitFloatIntBitsToFloat(HInvoke* invoke) {
  MoveIntToFP(invoke->GetLocations(), false, GetAssembler());
}

static void CreateIntToIntLocations(ArenaAllocator* arena, HInvoke* invoke) {
  LocationSummary* locations = new (arena) LocationSummary(invoke,
                                                           LocationSummary::kNoCall,
                                                           kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
}

static void GenReverseBytes(LocationSummary* locations,
                            Primitive::Type type,
                            Mips64Assembler* masm) {
  Location in = locations->InAt(0);
  Location out = locations->Out();

  switch (type) {
    case Primitive::kPrimShort:
      __ Dsbh(out.AsRegister<GpuRegister>(), in.AsRegister<GpuRegister>());
      __ Seh(out.AsRegister<GpuRegister>(), out.AsRegister<GpuRegister>());
      break;
    case Primitive::kPrimInt:
      __ Rotr(out.AsRegister<GpuRegister>(), in.AsRegister<GpuRegister>(), 16u);
      __ Wsbh(out.AsRegister<GpuRegister>(), out.AsRegister<GpuRegister>());
      break;
    case Primitive::kPrimLong:
      __ Dsbh(out.AsRegister<GpuRegister>(), in.AsRegister<GpuRegister>());
      __ Dshd(out.AsRegister<GpuRegister>(), out.AsRegister<GpuRegister>());
      break;
    default:
      LOG(FATAL) << "Unexpected size for reverse-bytes: " << type;
      UNREACHABLE();
  }
}

// int java.lang.Integer.reverseBytes(int)
void IntrinsicLocationsBuilderMIPS64::VisitIntegerReverseBytes(HInvoke* invoke) {
  CreateIntToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorMIPS64::VisitIntegerReverseBytes(HInvoke* invoke) {
  GenReverseBytes(invoke->GetLocations(), Primitive::kPrimInt, GetAssembler());
}

// long java.lang.Long.reverseBytes(long)
void IntrinsicLocationsBuilderMIPS64::VisitLongReverseBytes(HInvoke* invoke) {
  CreateIntToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorMIPS64::VisitLongReverseBytes(HInvoke* invoke) {
  GenReverseBytes(invoke->GetLocations(), Primitive::kPrimLong, GetAssembler());
}

// short java.lang.Short.reverseBytes(short)
void IntrinsicLocationsBuilderMIPS64::VisitShortReverseBytes(HInvoke* invoke) {
  CreateIntToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorMIPS64::VisitShortReverseBytes(HInvoke* invoke) {
  GenReverseBytes(invoke->GetLocations(), Primitive::kPrimShort, GetAssembler());
}

static void GenCountZeroes(LocationSummary* locations, bool is64bit, Mips64Assembler* masm) {
  Location input = locations->InAt(0);
  Location output = locations->Out();

  if (is64bit) {
    __ Dclz(output.AsRegister<GpuRegister>(), input.AsRegister<GpuRegister>());
  } else {
    __ Clz(output.AsRegister<GpuRegister>(), input.AsRegister<GpuRegister>());
  }
}

// int java.lang.Integer.numberOfLeadingZeros(int i)
void IntrinsicLocationsBuilderMIPS64::VisitIntegerNumberOfLeadingZeros(HInvoke* invoke) {
  CreateIntToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorMIPS64::VisitIntegerNumberOfLeadingZeros(HInvoke* invoke) {
  GenCountZeroes(invoke->GetLocations(), false, GetAssembler());
}

// int java.lang.Long.numberOfLeadingZeros(long i)
void IntrinsicLocationsBuilderMIPS64::VisitLongNumberOfLeadingZeros(HInvoke* invoke) {
  CreateIntToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorMIPS64::VisitLongNumberOfLeadingZeros(HInvoke* invoke) {
  GenCountZeroes(invoke->GetLocations(), true, GetAssembler());
}

static void GenReverse(LocationSummary* locations,
                       Primitive::Type type,
                       Mips64Assembler* masm) {
  DCHECK(type == Primitive::kPrimInt || type == Primitive::kPrimLong);

  Location in = locations->InAt(0);
  Location out = locations->Out();

  if (type == Primitive::kPrimInt) {
    __ Rotr(out.AsRegister<GpuRegister>(), in.AsRegister<GpuRegister>(), 16u);
    __ Wsbh(out.AsRegister<GpuRegister>(), out.AsRegister<GpuRegister>());
    __ Bitswap(out.AsRegister<GpuRegister>(), out.AsRegister<GpuRegister>());
  } else {
    __ Dsbh(out.AsRegister<GpuRegister>(), in.AsRegister<GpuRegister>());
    __ Dshd(out.AsRegister<GpuRegister>(), out.AsRegister<GpuRegister>());
    __ Dbitswap(out.AsRegister<GpuRegister>(), out.AsRegister<GpuRegister>());
  }
}

// int java.lang.Integer.reverse(int)
void IntrinsicLocationsBuilderMIPS64::VisitIntegerReverse(HInvoke* invoke) {
  CreateIntToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorMIPS64::VisitIntegerReverse(HInvoke* invoke) {
  GenReverse(invoke->GetLocations(), Primitive::kPrimInt, GetAssembler());
}

// long java.lang.Long.reverse(long)
void IntrinsicLocationsBuilderMIPS64::VisitLongReverse(HInvoke* invoke) {
  CreateIntToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorMIPS64::VisitLongReverse(HInvoke* invoke) {
  GenReverse(invoke->GetLocations(), Primitive::kPrimLong, GetAssembler());
}

static void CreateFPToFPLocations(ArenaAllocator* arena, HInvoke* invoke) {
  LocationSummary* locations = new (arena) LocationSummary(invoke,
                                                           LocationSummary::kNoCall,
                                                           kIntrinsified);
  locations->SetInAt(0, Location::RequiresFpuRegister());
  locations->SetOut(Location::RequiresFpuRegister(), Location::kNoOutputOverlap);
}

static void MathAbsFP(LocationSummary* locations, bool is64bit, Mips64Assembler* masm) {
  Location in = locations->InAt(0);
  Location out = locations->Out();

  if (is64bit) {
    __ AbsD(out.AsFpuRegister<FpuRegister>(), in.AsFpuRegister<FpuRegister>());
  } else {
    __ AbsS(out.AsFpuRegister<FpuRegister>(), in.AsFpuRegister<FpuRegister>());
  }
}

// double java.lang.Object.Math.abs(double)
void IntrinsicLocationsBuilderMIPS64::VisitMathAbsDouble(HInvoke* invoke) {
  CreateFPToFPLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorMIPS64::VisitMathAbsDouble(HInvoke* invoke) {
  MathAbsFP(invoke->GetLocations(), true, GetAssembler());
}

// float java.lang.Object.Math.abs(float)
void IntrinsicLocationsBuilderMIPS64::VisitMathAbsFloat(HInvoke* invoke) {
  CreateFPToFPLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorMIPS64::VisitMathAbsFloat(HInvoke* invoke) {
  MathAbsFP(invoke->GetLocations(), false, GetAssembler());
}

static void CreateIntToInt(ArenaAllocator* arena, HInvoke* invoke) {
  LocationSummary* locations = new (arena) LocationSummary(invoke,
                                                           LocationSummary::kNoCall,
                                                           kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
}

static void GenAbsInteger(LocationSummary* locations,
                          bool is64bit,
                          Mips64Assembler* masm) {
  Location in = locations->InAt(0);
  Location out = locations->Out();

  if (is64bit) {
    __ Dsra32(AT, in.AsRegister<GpuRegister>(), 31);
    __ Xor(out.AsRegister<GpuRegister>(), in.AsRegister<GpuRegister>(), AT);
    __ Dsubu(out.AsRegister<GpuRegister>(), out.AsRegister<GpuRegister>(), AT);
  } else {
    __ Sra(AT, in.AsRegister<GpuRegister>(), 31);
    __ Xor(out.AsRegister<GpuRegister>(), in.AsRegister<GpuRegister>(), AT);
    __ Subu(out.AsRegister<GpuRegister>(), out.AsRegister<GpuRegister>(), AT);
  }
}

// int java.lang.Object.Math.abs(int)
void IntrinsicLocationsBuilderMIPS64::VisitMathAbsInt(HInvoke* invoke) {
  CreateIntToInt(arena_, invoke);
}

void IntrinsicCodeGeneratorMIPS64::VisitMathAbsInt(HInvoke* invoke) {
  GenAbsInteger(invoke->GetLocations(), false, GetAssembler());
}

// long java.lang.Object.Math.abs(long)
void IntrinsicLocationsBuilderMIPS64::VisitMathAbsLong(HInvoke* invoke) {
  CreateIntToInt(arena_, invoke);
}

void IntrinsicCodeGeneratorMIPS64::VisitMathAbsLong(HInvoke* invoke) {
  GenAbsInteger(invoke->GetLocations(), true, GetAssembler());
}

static void GenMinMaxFP(LocationSummary* locations,
                        bool is_min,
                        bool is_double,
                        Mips64Assembler* masm) {
  Location op1 = locations->InAt(0);
  Location op2 = locations->InAt(1);
  Location out = locations->Out();

  if (is_double) {
    if (is_min) {
      __ MinD(out.AsFpuRegister<FpuRegister>(),
              op1.AsFpuRegister<FpuRegister>(),
              op2.AsFpuRegister<FpuRegister>());
    } else {
      __ MaxD(out.AsFpuRegister<FpuRegister>(),
              op1.AsFpuRegister<FpuRegister>(),
              op2.AsFpuRegister<FpuRegister>());
    }
  } else {
    if (is_min) {
      __ MinS(out.AsFpuRegister<FpuRegister>(),
              op1.AsFpuRegister<FpuRegister>(),
              op2.AsFpuRegister<FpuRegister>());
    } else {
      __ MaxS(out.AsFpuRegister<FpuRegister>(),
              op1.AsFpuRegister<FpuRegister>(),
              op2.AsFpuRegister<FpuRegister>());
    }
  }
}

static void CreateFPFPToFPLocations(ArenaAllocator* arena, HInvoke* invoke) {
  LocationSummary* locations = new (arena) LocationSummary(invoke,
                                                           LocationSummary::kNoCall,
                                                           kIntrinsified);
  locations->SetInAt(0, Location::RequiresFpuRegister());
  locations->SetInAt(1, Location::RequiresFpuRegister());
  locations->SetOut(Location::RequiresFpuRegister(), Location::kNoOutputOverlap);
}

// double java.lang.Object.Math.min(double, double)
void IntrinsicLocationsBuilderMIPS64::VisitMathMinDoubleDouble(HInvoke* invoke) {
  CreateFPFPToFPLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorMIPS64::VisitMathMinDoubleDouble(HInvoke* invoke) {
  GenMinMaxFP(invoke->GetLocations(), true, true, GetAssembler());
}

// float java.lang.Object.Math.min(float, float)
void IntrinsicLocationsBuilderMIPS64::VisitMathMinFloatFloat(HInvoke* invoke) {
  CreateFPFPToFPLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorMIPS64::VisitMathMinFloatFloat(HInvoke* invoke) {
  GenMinMaxFP(invoke->GetLocations(), true, false, GetAssembler());
}

// double java.lang.Object.Math.max(double, double)
void IntrinsicLocationsBuilderMIPS64::VisitMathMaxDoubleDouble(HInvoke* invoke) {
  CreateFPFPToFPLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorMIPS64::VisitMathMaxDoubleDouble(HInvoke* invoke) {
  GenMinMaxFP(invoke->GetLocations(), false, true, GetAssembler());
}

// float java.lang.Object.Math.max(float, float)
void IntrinsicLocationsBuilderMIPS64::VisitMathMaxFloatFloat(HInvoke* invoke) {
  CreateFPFPToFPLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorMIPS64::VisitMathMaxFloatFloat(HInvoke* invoke) {
  GenMinMaxFP(invoke->GetLocations(), false, false, GetAssembler());
}

static void GenMinMax(LocationSummary* locations,
                      bool is_min,
                      bool is_long ATTRIBUTE_UNUSED,
                      Mips64Assembler* masm) {
  Location op1 = locations->InAt(0);
  Location op2 = locations->InAt(1);
  Location out = locations->Out();

  if (out.AsRegister<GpuRegister>() == op1.AsRegister<GpuRegister>()) {
    __ Slt(AT, op2.AsRegister<GpuRegister>(), op1.AsRegister<GpuRegister>());
    if (is_min) {
      __ Seleqz(out.AsRegister<GpuRegister>(), op1.AsRegister<GpuRegister>(), AT);
      __ Selnez(AT, op2.AsRegister<GpuRegister>(), AT);
    } else {
      __ Selnez(out.AsRegister<GpuRegister>(), op1.AsRegister<GpuRegister>(), AT);
      __ Seleqz(AT, op2.AsRegister<GpuRegister>(), AT);
    }
  } else {
    __ Slt(AT, op1.AsRegister<GpuRegister>(), op2.AsRegister<GpuRegister>());
    if (is_min) {
      __ Seleqz(out.AsRegister<GpuRegister>(), op2.AsRegister<GpuRegister>(), AT);
      __ Selnez(AT, op1.AsRegister<GpuRegister>(), AT);
    } else {
      __ Selnez(out.AsRegister<GpuRegister>(), op2.AsRegister<GpuRegister>(), AT);
      __ Seleqz(AT, op1.AsRegister<GpuRegister>(), AT);
    }
  }
  __ Or(out.AsRegister<GpuRegister>(), out.AsRegister<GpuRegister>(), AT);
}

static void CreateIntIntToIntLocations(ArenaAllocator* arena, HInvoke* invoke) {
  LocationSummary* locations = new (arena) LocationSummary(invoke,
                                                           LocationSummary::kNoCall,
                                                           kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
}

// int java.lang.Object.Math.min(int, int)
void IntrinsicLocationsBuilderMIPS64::VisitMathMinIntInt(HInvoke* invoke) {
  CreateIntIntToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorMIPS64::VisitMathMinIntInt(HInvoke* invoke) {
  GenMinMax(invoke->GetLocations(), true, false, GetAssembler());
}

// long java.lang.Object.Math.min(long, long)
void IntrinsicLocationsBuilderMIPS64::VisitMathMinLongLong(HInvoke* invoke) {
  CreateIntIntToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorMIPS64::VisitMathMinLongLong(HInvoke* invoke) {
  GenMinMax(invoke->GetLocations(), true, true, GetAssembler());
}

// int java.lang.Object.Math.max(int, int)
void IntrinsicLocationsBuilderMIPS64::VisitMathMaxIntInt(HInvoke* invoke) {
  CreateIntIntToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorMIPS64::VisitMathMaxIntInt(HInvoke* invoke) {
  GenMinMax(invoke->GetLocations(), false, false, GetAssembler());
}

// long java.lang.Object.Math.max(long, long)
void IntrinsicLocationsBuilderMIPS64::VisitMathMaxLongLong(HInvoke* invoke) {
  CreateIntIntToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorMIPS64::VisitMathMaxLongLong(HInvoke* invoke) {
  GenMinMax(invoke->GetLocations(), false, true, GetAssembler());
}

// double java.lang.Object.Math.sqrt(double)
void IntrinsicLocationsBuilderMIPS64::VisitMathSqrt(HInvoke* invoke) {
  CreateFPToFPLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorMIPS64::VisitMathSqrt(HInvoke* invoke) {
  LocationSummary* locations = invoke->GetLocations();
  Mips64Assembler* masm = GetAssembler();
  __ SqrtD(locations->Out().AsFpuRegister<FpuRegister>(),
           locations->InAt(0).AsFpuRegister<FpuRegister>());
}

static void CreateFPToFP(ArenaAllocator* arena, HInvoke* invoke) {
  LocationSummary* locations = new (arena) LocationSummary(invoke,
                                                           LocationSummary::kNoCall,
                                                           kIntrinsified);
  locations->SetInAt(0, Location::RequiresFpuRegister());
  locations->SetOut(Location::RequiresFpuRegister(), Location::kNoOutputOverlap);
}

// double java.lang.Object.Math.rint(double)
void IntrinsicLocationsBuilderMIPS64::VisitMathRint(HInvoke* invoke) {
  CreateFPToFP(arena_, invoke);
}

void IntrinsicCodeGeneratorMIPS64::VisitMathRint(HInvoke* invoke) {
  LocationSummary* locations = invoke->GetLocations();
  Mips64Assembler* masm = GetAssembler();
  FpuRegister in = locations->InAt(0).AsFpuRegister<FpuRegister>();
  FpuRegister out = locations->Out().AsFpuRegister<FpuRegister>();

  __ RintD(out, in);
}

// double java.lang.Object.Math.floor(double)
void IntrinsicLocationsBuilderMIPS64::VisitMathFloor(HInvoke* invoke) {
  CreateFPToFP(arena_, invoke);
}

void IntrinsicCodeGeneratorMIPS64::VisitMathFloor(HInvoke* invoke) {
  LocationSummary* locations = invoke->GetLocations();
  Mips64Assembler* masm = GetAssembler();
  FpuRegister in = locations->InAt(0).AsFpuRegister<FpuRegister>();
  FpuRegister out = locations->Out().AsFpuRegister<FpuRegister>();

  Label done;

  // double floor(double in) {
  //     if in.isNaN || in.isInfinite || in.isZero {
  //         return in;
  //     }
  __ ClassD(out, in);
  __ Dmfc1(AT, out);
  __ Andi(AT, AT, 0x267);       // +0.0 | +Inf | -0.0 | -Inf | qNaN | sNaN
  __ MovD(out, in);
  __ Bnezc(AT, &done);

  //     Long outLong = floor(in);
  //     if outLong == Long.MAXINT {
  //         // floor() has almost certainly returned a value which
  //         // can't be successfully represented as a signed 64-bit
  //         // number.  Java expects that the input value will be
  //         // returned in these cases.
  //         // There is also a small probability that floor(in)
  //         // correctly truncates the input value to Long.MAXINT.  In
  //         // that case, this exception handling code still does the
  //         // correct thing.
  //         return in;
  //     }
  __ FloorLD(out, in);
  __ Dmfc1(AT, out);
  __ MovD(out, in);
  __ LoadConst64(TMP, kPrimLongMax);
  __ Beqc(AT, TMP, &done);

  //     double out = outLong;
  //     return out;
  __ Dmtc1(AT, out);
  __ Cvtdl(out, out);
  __ Bind(&done);
  // }
}

// double java.lang.Object.Math.ceil(double)
void IntrinsicLocationsBuilderMIPS64::VisitMathCeil(HInvoke* invoke) {
  CreateFPToFP(arena_, invoke);
}

void IntrinsicCodeGeneratorMIPS64::VisitMathCeil(HInvoke* invoke) {
  LocationSummary* locations = invoke->GetLocations();
  Mips64Assembler* masm = GetAssembler();
  FpuRegister in = locations->InAt(0).AsFpuRegister<FpuRegister>();
  FpuRegister out = locations->Out().AsFpuRegister<FpuRegister>();

  Label done;

  // double ceil(double in) {
  //     if in.isNaN || in.isInfinite || in.isZero {
  //         return in;
  //     }
  __ ClassD(out, in);
  __ Dmfc1(AT, out);
  __ Andi(AT, AT, 0x267);       // +0.0 | +Inf | -0.0 | -Inf | qNaN | sNaN
  __ MovD(out, in);
  __ Bnezc(AT, &done);

  //     Long outLong = ceil(in);
  //     if outLong == Long.MAXINT {
  //         // ceil() has almost certainly returned a value which
  //         // can't be successfully represented as a signed 64-bit
  //         // number.  Java expects that the input value will be
  //         // returned in these cases.
  //         // There is also a small probability that ceil(in)
  //         // correctly rounds up the input value to Long.MAXINT.  In
  //         // that case, this exception handling code still does the
  //         // correct thing.
  //         return in;
  //     }
  __ CeilLD(out, in);
  __ Dmfc1(AT, out);
  __ MovD(out, in);
  __ LoadConst64(TMP, kPrimLongMax);
  __ Beqc(AT, TMP, &done);

  //     double out = outLong;
  //     return out;
  __ Dmtc1(AT, out);
  __ Cvtdl(out, out);
  __ Bind(&done);
  // }
}

// byte libcore.io.Memory.peekByte(long address)
void IntrinsicLocationsBuilderMIPS64::VisitMemoryPeekByte(HInvoke* invoke) {
  CreateIntToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorMIPS64::VisitMemoryPeekByte(HInvoke* invoke) {
  Mips64Assembler* masm = GetAssembler();
  __ Lb(invoke->GetLocations()->Out().AsRegister<GpuRegister>(),
        invoke->GetLocations()->InAt(0).AsRegister<GpuRegister>(),
        0);
}

// short libcore.io.Memory.peekShort(long address)
void IntrinsicLocationsBuilderMIPS64::VisitMemoryPeekShortNative(HInvoke* invoke) {
  CreateIntToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorMIPS64::VisitMemoryPeekShortNative(HInvoke* invoke) {
  Mips64Assembler* masm = GetAssembler();
  __ Lh(invoke->GetLocations()->Out().AsRegister<GpuRegister>(),
        invoke->GetLocations()->InAt(0).AsRegister<GpuRegister>(),
        0);
}

// int libcore.io.Memory.peekInt(long address)
void IntrinsicLocationsBuilderMIPS64::VisitMemoryPeekIntNative(HInvoke* invoke) {
  CreateIntToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorMIPS64::VisitMemoryPeekIntNative(HInvoke* invoke) {
  Mips64Assembler* masm = GetAssembler();
  __ Lw(invoke->GetLocations()->Out().AsRegister<GpuRegister>(),
        invoke->GetLocations()->InAt(0).AsRegister<GpuRegister>(),
        0);
}

// long libcore.io.Memory.peekLong(long address)
void IntrinsicLocationsBuilderMIPS64::VisitMemoryPeekLongNative(HInvoke* invoke) {
  CreateIntToIntLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorMIPS64::VisitMemoryPeekLongNative(HInvoke* invoke) {
  Mips64Assembler* masm = GetAssembler();
  __ Ld(invoke->GetLocations()->Out().AsRegister<GpuRegister>(),
        invoke->GetLocations()->InAt(0).AsRegister<GpuRegister>(),
        0);
}

static void CreateIntIntToVoidLocations(ArenaAllocator* arena, HInvoke* invoke) {
  LocationSummary* locations = new (arena) LocationSummary(invoke,
                                                           LocationSummary::kNoCall,
                                                           kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RequiresRegister());
}

// void libcore.io.Memory.pokeByte(long address, byte value)
void IntrinsicLocationsBuilderMIPS64::VisitMemoryPokeByte(HInvoke* invoke) {
  CreateIntIntToVoidLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorMIPS64::VisitMemoryPokeByte(HInvoke* invoke) {
  Mips64Assembler* masm = GetAssembler();
  __ Sb(invoke->GetLocations()->InAt(1).AsRegister<GpuRegister>(),
        invoke->GetLocations()->InAt(0).AsRegister<GpuRegister>(),
        0);
}

// void libcore.io.Memory.pokeShort(long address, short value)
void IntrinsicLocationsBuilderMIPS64::VisitMemoryPokeShortNative(HInvoke* invoke) {
  CreateIntIntToVoidLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorMIPS64::VisitMemoryPokeShortNative(HInvoke* invoke) {
  Mips64Assembler* masm = GetAssembler();
  __ Sh(invoke->GetLocations()->InAt(1).AsRegister<GpuRegister>(),
        invoke->GetLocations()->InAt(0).AsRegister<GpuRegister>(),
        0);
}

// void libcore.io.Memory.pokeInt(long address, int value)
void IntrinsicLocationsBuilderMIPS64::VisitMemoryPokeIntNative(HInvoke* invoke) {
  CreateIntIntToVoidLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorMIPS64::VisitMemoryPokeIntNative(HInvoke* invoke) {
  Mips64Assembler* masm = GetAssembler();
  __ Sw(invoke->GetLocations()->InAt(1).AsRegister<GpuRegister>(),
        invoke->GetLocations()->InAt(0).AsRegister<GpuRegister>(),
        0);
}

// void libcore.io.Memory.pokeLong(long address, long value)
void IntrinsicLocationsBuilderMIPS64::VisitMemoryPokeLongNative(HInvoke* invoke) {
  CreateIntIntToVoidLocations(arena_, invoke);
}

void IntrinsicCodeGeneratorMIPS64::VisitMemoryPokeLongNative(HInvoke* invoke) {
  Mips64Assembler* masm = GetAssembler();
  __ Sd(invoke->GetLocations()->InAt(1).AsRegister<GpuRegister>(),
        invoke->GetLocations()->InAt(0).AsRegister<GpuRegister>(),
        0);
}

// Thread java.lang.Thread.currentThread()
void IntrinsicLocationsBuilderMIPS64::VisitThreadCurrentThread(HInvoke* invoke) {
  LocationSummary* locations = new (arena_) LocationSummary(invoke,
                                                            LocationSummary::kNoCall,
                                                            kIntrinsified);
  locations->SetOut(Location::RequiresRegister());
}

void IntrinsicCodeGeneratorMIPS64::VisitThreadCurrentThread(HInvoke* invoke) {
  Mips64Assembler* masm = GetAssembler();
  __ LoadFromOffset(kLoadUnsignedWord,
                    invoke->GetLocations()->Out().AsRegister<GpuRegister>(),
                    TR,
                    Thread::PeerOffset<8>().Int32Value());
}

// Unimplemented intrinsics.

#define UNIMPLEMENTED_INTRINSIC(Name)                                                  \
void IntrinsicLocationsBuilderMIPS64::Visit ## Name(HInvoke* invoke ATTRIBUTE_UNUSED) { \
}                                                                                      \
void IntrinsicCodeGeneratorMIPS64::Visit ## Name(HInvoke* invoke ATTRIBUTE_UNUSED) {    \
}

UNIMPLEMENTED_INTRINSIC(MathRoundDouble)
UNIMPLEMENTED_INTRINSIC(MathRoundFloat)

UNIMPLEMENTED_INTRINSIC(UnsafeGet)
UNIMPLEMENTED_INTRINSIC(UnsafeGetVolatile)
UNIMPLEMENTED_INTRINSIC(UnsafeGetLong)
UNIMPLEMENTED_INTRINSIC(UnsafeGetLongVolatile)
UNIMPLEMENTED_INTRINSIC(UnsafeGetObject)
UNIMPLEMENTED_INTRINSIC(UnsafeGetObjectVolatile)
UNIMPLEMENTED_INTRINSIC(UnsafePut)
UNIMPLEMENTED_INTRINSIC(UnsafePutOrdered)
UNIMPLEMENTED_INTRINSIC(UnsafePutVolatile)
UNIMPLEMENTED_INTRINSIC(UnsafePutObject)
UNIMPLEMENTED_INTRINSIC(UnsafePutObjectOrdered)
UNIMPLEMENTED_INTRINSIC(UnsafePutObjectVolatile)
UNIMPLEMENTED_INTRINSIC(UnsafePutLong)
UNIMPLEMENTED_INTRINSIC(UnsafePutLongOrdered)
UNIMPLEMENTED_INTRINSIC(UnsafePutLongVolatile)
UNIMPLEMENTED_INTRINSIC(UnsafeCASInt)
UNIMPLEMENTED_INTRINSIC(UnsafeCASLong)
UNIMPLEMENTED_INTRINSIC(UnsafeCASObject)
UNIMPLEMENTED_INTRINSIC(StringCharAt)
UNIMPLEMENTED_INTRINSIC(StringCompareTo)
UNIMPLEMENTED_INTRINSIC(StringEquals)
UNIMPLEMENTED_INTRINSIC(StringIndexOf)
UNIMPLEMENTED_INTRINSIC(StringIndexOfAfter)
UNIMPLEMENTED_INTRINSIC(StringNewStringFromBytes)
UNIMPLEMENTED_INTRINSIC(StringNewStringFromChars)
UNIMPLEMENTED_INTRINSIC(StringNewStringFromString)
UNIMPLEMENTED_INTRINSIC(LongRotateLeft)
UNIMPLEMENTED_INTRINSIC(LongRotateRight)
UNIMPLEMENTED_INTRINSIC(LongNumberOfTrailingZeros)
UNIMPLEMENTED_INTRINSIC(IntegerRotateLeft)
UNIMPLEMENTED_INTRINSIC(IntegerRotateRight)
UNIMPLEMENTED_INTRINSIC(IntegerNumberOfTrailingZeros)

UNIMPLEMENTED_INTRINSIC(ReferenceGetReferent)
UNIMPLEMENTED_INTRINSIC(StringGetCharsNoCheck)
UNIMPLEMENTED_INTRINSIC(SystemArrayCopyChar)

#undef UNIMPLEMENTED_INTRINSIC

#undef __

}  // namespace mips64
}  // namespace art
