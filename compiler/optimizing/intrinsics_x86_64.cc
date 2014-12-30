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

#include "intrinsics_x86_64.h"

#include "code_generator_x86_64.h"
#include "driver/dex_compilation_unit.h"
#include "entrypoints/quick/quick_entrypoints.h"
#include "gc/accounting/card_table.h"
#include "intrinsics.h"
#include "mirror/array-inl.h"
#include "mirror/art_method.h"
#include "mirror/class.h"
#include "mirror/object_reference.h"
#include "thread.h"
#include "utils/arena_containers.h"
#include "utils/assembler.h"
#include "utils/stack_checks.h"
#include "utils/x86_64/assembler_x86_64.h"
#include "utils/x86_64/managed_register_x86_64.h"

namespace art {

namespace x86_64 {

X86_64Assembler* IntrinsicCodeGeneratorX86_64::GetAssembler() {
  return reinterpret_cast<X86_64Assembler*>(codegen_->GetAssembler());
}

ArenaAllocator* IntrinsicCodeGeneratorX86_64::GetArena() {
  return codegen_->GetGraph()->GetArena();
}


#define __ reinterpret_cast<X86_64Assembler*>(codegen->GetAssembler())->

static void GenerateStaticOrDirectCall(HInvokeStaticOrDirect* invoke,
                                       CpuRegister temp,
                                       CodeGeneratorX86_64* codegen) {
  // All registers are assumed to be correctly set up.

  // TODO: Implement all kinds of calls:
  // 1) boot -> boot
  // 2) app -> boot
  // 3) app -> app
  //
  // Currently we implement the app -> app logic, which looks up in the resolve cache.

  // temp = method;
  codegen->LoadCurrentMethod(temp);
  // temp = temp->dex_cache_resolved_methods_;
  __ movl(temp, Address(temp, mirror::ArtMethod::DexCacheResolvedMethodsOffset().SizeValue()));
  // temp = temp[index_in_cache]
  __ movl(temp, Address(temp, CodeGenerator::GetCacheOffset(invoke->GetIndexInDexCache())));
  // (temp + offset_of_quick_compiled_code)()
  __ call(Address(temp, mirror::ArtMethod::EntryPointFromQuickCompiledCodeOffset(
      kX86_64WordSize).SizeValue()));

  DCHECK(!codegen->IsLeafMethod());
  codegen->RecordPcInfo(invoke, invoke->GetDexPc());
}

static void MoveOut(Location trg, Primitive::Type type, CodeGeneratorX86_64* codegen) {
  if (trg.IsValid()) {
    switch (type) {
      case Primitive::kPrimBoolean:
      case Primitive::kPrimByte:
      case Primitive::kPrimChar:
      case Primitive::kPrimShort:
      case Primitive::kPrimInt:
      case Primitive::kPrimNot:
        {
          CpuRegister trg_reg = trg.AsRegister<CpuRegister>();
          if (trg_reg.AsRegister() != RAX) {
            __ movl(trg_reg, CpuRegister(RAX));
          }
        }
        break;
      case Primitive::kPrimLong:
        {
          CpuRegister trg_reg = trg.AsRegister<CpuRegister>();
          if (trg_reg.AsRegister() != RAX) {
            __ movq(trg_reg, CpuRegister(RAX));
          }
        }
        break;

      case Primitive::kPrimVoid:
        break;

      case Primitive::kPrimDouble:
        {
          XmmRegister trg_reg = trg.AsFpuRegister<XmmRegister>();
          if (trg_reg.AsFloatRegister() != XMM0) {
            __ movsd(trg_reg, XmmRegister(XMM0));
          }
        }
        break;
      case Primitive::kPrimFloat:
        {
          XmmRegister trg_reg = trg.AsFpuRegister<XmmRegister>();
          if (trg_reg.AsFloatRegister() != XMM0) {
            __ movss(trg_reg, XmmRegister(XMM0));
          }
        }
        break;
    }
  }
}

static void MoveIn(HInvoke* invoke, ArenaAllocator* arena, CodeGeneratorX86_64* codegen) {
  if (invoke->InputCount() > 0) {
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
}

// Slow-path for intrinsic non-intrinsified call. This assumes the LocationSummary is set up for
// a normal call, and the registers are used correspondingly. Then we don't need to setup inputs
// or copy the output.
class IntrinsicSlowPathX86_64 : public SlowPathCodeX86_64 {
 public:
  explicit IntrinsicSlowPathX86_64(HInvoke* invoke) : invoke_(invoke) { }

  virtual void EmitNativeCode(CodeGenerator* codegen_in) OVERRIDE {
    CodeGeneratorX86_64* codegen = down_cast<CodeGeneratorX86_64*>(codegen_in);
    __ Bind(GetEntryLabel());

    codegen->SaveLiveRegisters(invoke_->GetLocations());

    MoveIn(invoke_, codegen->GetGraph()->GetArena(), codegen);

    if (invoke_->IsInvokeStaticOrDirect()) {
      GenerateStaticOrDirectCall(invoke_->AsInvokeStaticOrDirect(), CpuRegister(RDI), codegen);
    } else {
      UNIMPLEMENTED(FATAL) << "Non-direct intrinsic slow-path not yet implemented";
      UNREACHABLE();
    }

    // Copy the result back to the expected output.
    MoveOut(invoke_->GetLocations()->Out(), invoke_->GetType(), codegen);

    codegen->RestoreLiveRegisters(invoke_->GetLocations());
    __ jmp(GetExitLabel());
  }

 private:
  // The instruction where this slow path is happening.
  HInvoke* const invoke_;

  DISALLOW_COPY_AND_ASSIGN(IntrinsicSlowPathX86_64);
};

#undef __
#define __ assembler->

static bool CreateFPToIntLocations(ArenaAllocator* arena, HInvokeStaticOrDirect* invoke) {
  LocationSummary* locations = new (arena) LocationSummary(invoke, LocationSummary::kNoCall);
  locations->SetInAt(0, Location::RequiresFpuRegister());
  locations->SetOut(Location::RequiresRegister());
  return true;
}

static bool CreateIntToFPLocations(ArenaAllocator* arena, HInvokeStaticOrDirect* invoke) {
  LocationSummary* locations = new (arena) LocationSummary(invoke, LocationSummary::kNoCall);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresFpuRegister());
  return true;
}

static bool MoveFPToInt(LocationSummary* locations, bool is64bit, X86_64Assembler* assembler) {
  Location input = locations->InAt(0);
  Location output = locations->Out();
  __ movd(output.AsRegister<CpuRegister>(), input.AsFpuRegister<XmmRegister>(), is64bit);
  return true;
}

static bool MoveIntToFP(LocationSummary* locations, bool is64bit, X86_64Assembler* assembler) {
  Location input = locations->InAt(0);
  Location output = locations->Out();
  __ movd(output.AsFpuRegister<XmmRegister>(), input.AsRegister<CpuRegister>(), is64bit);
  return true;
}

bool IntrinsicLocationsBuilderX86_64::VisitDoubleDoubleToRawLongBits(HInvokeStaticOrDirect* invoke) {
  return CreateFPToIntLocations(arena_, invoke);
}
bool IntrinsicLocationsBuilderX86_64::VisitDoubleLongBitsToDouble(HInvokeStaticOrDirect* invoke) {
  return CreateIntToFPLocations(arena_, invoke);
}

bool IntrinsicCodeGeneratorX86_64::VisitDoubleDoubleToRawLongBits(HInvokeStaticOrDirect* invoke) {
  return MoveFPToInt(invoke->GetLocations(), true, GetAssembler());
}
bool IntrinsicCodeGeneratorX86_64::VisitDoubleLongBitsToDouble(HInvokeStaticOrDirect* invoke) {
  return MoveIntToFP(invoke->GetLocations(), true, GetAssembler());
}

bool IntrinsicLocationsBuilderX86_64::VisitFloatFloatToRawIntBits(HInvokeStaticOrDirect* invoke) {
  return CreateFPToIntLocations(arena_, invoke);
}
bool IntrinsicLocationsBuilderX86_64::VisitFloatIntBitsToFloat(HInvokeStaticOrDirect* invoke) {
  return CreateIntToFPLocations(arena_, invoke);
}

bool IntrinsicCodeGeneratorX86_64::VisitFloatFloatToRawIntBits(HInvokeStaticOrDirect* invoke) {
  return MoveFPToInt(invoke->GetLocations(), false, GetAssembler());
}
bool IntrinsicCodeGeneratorX86_64::VisitFloatIntBitsToFloat(HInvokeStaticOrDirect* invoke) {
  return MoveIntToFP(invoke->GetLocations(), false, GetAssembler());
}

static bool LocIntToInt(ArenaAllocator* arena, HInvokeStaticOrDirect* invoke) {
  LocationSummary* locations = new (arena) LocationSummary(invoke, LocationSummary::kNoCall);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::SameAsFirstInput());
  return true;
}

static bool GenReverseBytes(LocationSummary* locations, Primitive::Type size,
                            X86_64Assembler* assembler) {
  CpuRegister out = locations->Out().AsRegister<CpuRegister>();

  switch (size) {
    case Primitive::Type::kPrimShort:
      // TODO: Can be done with an xchg of 8b registers. This is straight from Quick.
      __ bswapl(out);
      __ sarl(out, Immediate(16));
      break;
    case Primitive::Type::kPrimInt:
      __ bswapl(out);
      break;
    case Primitive::Type::kPrimLong:
      __ bswapq(out);
      break;
    default:
      LOG(FATAL) << "Unexpected size for reverse-bytes: " << size;
      UNREACHABLE();
  }

  return true;
}

bool IntrinsicLocationsBuilderX86_64::VisitIntegerReverseBytes(HInvokeStaticOrDirect* invoke) {
  return LocIntToInt(arena_, invoke);
}

bool IntrinsicCodeGeneratorX86_64::VisitIntegerReverseBytes(HInvokeStaticOrDirect* invoke) {
  return GenReverseBytes(invoke->GetLocations(), Primitive::Type::kPrimInt, GetAssembler());
}

bool IntrinsicLocationsBuilderX86_64::VisitLongReverseBytes(HInvokeStaticOrDirect* invoke) {
  return LocIntToInt(arena_, invoke);
}

bool IntrinsicCodeGeneratorX86_64::VisitLongReverseBytes(HInvokeStaticOrDirect* invoke) {
  return GenReverseBytes(invoke->GetLocations(), Primitive::Type::kPrimLong, GetAssembler());
}

bool IntrinsicLocationsBuilderX86_64::VisitShortReverseBytes(HInvokeStaticOrDirect* invoke) {
  return LocIntToInt(arena_, invoke);
}

bool IntrinsicCodeGeneratorX86_64::VisitShortReverseBytes(HInvokeStaticOrDirect* invoke) {
  return GenReverseBytes(invoke->GetLocations(), Primitive::Type::kPrimShort, GetAssembler());
}


// TODO: Consider Quick's way of doing Double abs through integer operations, as the immediate we
//       need is 64b.

static bool CreateFloatToFloatPlusTemps(ArenaAllocator* arena, HInvokeStaticOrDirect* invoke) {
  // TODO: Any way to accept both register and memory here?
  LocationSummary* locations = new (arena) LocationSummary(invoke, LocationSummary::kNoCall);
  locations->SetInAt(0, Location::RequiresFpuRegister());
  locations->SetOut(Location::SameAsFirstInput());
  locations->AddTemp(Location::RequiresRegister());     // Immediate constant.
  locations->AddTemp(Location::RequiresFpuRegister());  // FP version of above.
  return true;
}

static bool MathAbsFP(LocationSummary* locations, bool is64bit, X86_64Assembler* assembler) {
  Location output = locations->Out();
  CpuRegister cpu_temp = locations->GetTemp(0).AsRegister<CpuRegister>();
  XmmRegister xmm_temp = locations->GetTemp(1).AsFpuRegister<XmmRegister>();

  if (is64bit) {
    __ movq(cpu_temp, Immediate(INT64_C(0x7FFFFFFFFFFFFFFF)));
    __ movd(xmm_temp, cpu_temp);
    __ andpd(output.AsFpuRegister<XmmRegister>(), xmm_temp);
  } else {
    __ movl(cpu_temp, Immediate(INT64_C(0x7FFFFFFF)));
    __ movd(xmm_temp, cpu_temp);
    __ andps(output.AsFpuRegister<XmmRegister>(), xmm_temp);
  }

  return true;
}

bool IntrinsicLocationsBuilderX86_64::VisitMathAbsDouble(HInvokeStaticOrDirect* invoke) {
  return CreateFloatToFloatPlusTemps(arena_, invoke);
}

bool IntrinsicCodeGeneratorX86_64::VisitMathAbsDouble(HInvokeStaticOrDirect* invoke) {
  return MathAbsFP(invoke->GetLocations(), true, GetAssembler());
}

bool IntrinsicLocationsBuilderX86_64::VisitMathAbsFloat(HInvokeStaticOrDirect* invoke) {
  return CreateFloatToFloatPlusTemps(arena_, invoke);
}

bool IntrinsicCodeGeneratorX86_64::VisitMathAbsFloat(HInvokeStaticOrDirect* invoke) {
  return MathAbsFP(invoke->GetLocations(), false, GetAssembler());
}

static bool CreateIntToIntPlusTemp(ArenaAllocator* arena, HInvoke* invoke) {
  LocationSummary* locations = new (arena) LocationSummary(invoke, LocationSummary::kNoCall);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::SameAsFirstInput());
  locations->AddTemp(Location::RequiresRegister());
  return true;
}

static bool GenAbsInteger(LocationSummary* locations, bool is64bit, X86_64Assembler* assembler) {
  Location output = locations->Out();
  CpuRegister out = output.AsRegister<CpuRegister>();
  CpuRegister mask = locations->GetTemp(0).AsRegister<CpuRegister>();

  // Create mask.
  if (is64bit) {
    __ movq(mask, out);
    __ sarq(mask, Immediate(63));
  } else {
    __ movl(mask, out);
    __ sarl(mask, Immediate(31));
  }

  // Add mask.
  if (is64bit) {
    __ addq(out, mask);
    __ xorq(out, mask);
  } else {
    __ addl(out, mask);
    __ xorl(out, mask);
  }

  return true;
}

bool IntrinsicLocationsBuilderX86_64::VisitMathAbsInt(HInvokeStaticOrDirect* invoke) {
  return CreateIntToIntPlusTemp(arena_, invoke);
}

bool IntrinsicCodeGeneratorX86_64::VisitMathAbsInt(HInvokeStaticOrDirect* invoke) {
  return GenAbsInteger(invoke->GetLocations(), false, GetAssembler());
}

bool IntrinsicLocationsBuilderX86_64::VisitMathAbsLong(HInvokeStaticOrDirect* invoke) {
  return CreateIntToIntPlusTemp(arena_, invoke);
}

bool IntrinsicCodeGeneratorX86_64::VisitMathAbsLong(HInvokeStaticOrDirect* invoke) {
  return GenAbsInteger(invoke->GetLocations(), true, GetAssembler());
}

static bool GenMinMaxFP(LocationSummary* locations, bool is_min, bool is_double,
                        X86_64Assembler* assembler) {
  Location op1_loc = locations->InAt(0);
  Location op2_loc = locations->InAt(1);
  Location out_loc = locations->Out();
  XmmRegister out = out_loc.AsFpuRegister<XmmRegister>();

  // Shortcut for same input locations.
  if (op1_loc.Equals(op2_loc)) {
    if (!out_loc.Equals(op1_loc) && !out_loc.Equals(op2_loc)) {
      // Copy to out.
      if (is_double) {
        __ movsd(out, op1_loc.AsFpuRegister<XmmRegister>());
      } else {
        __ movss(out, op1_loc.AsFpuRegister<XmmRegister>());
      }
    }
    return true;
  }

  //  (out := op1)
  //  out <=? op2
  //  if Nan jmp Nan_label
  //  if out is min jmp done
  //  if op2 is min jmp op2_label
  //  handle -0/+0
  //  jmp done
  // Nan_label:
  //  out := NaN
  // op2_label:
  //  out := op2
  // done:
  //
  // This removes one jmp, but needs to copy one input (op1) to out.
  //
  // TODO: This is straight from Quick (except literal pool). Make NaN an out-of-line slowpath?

  XmmRegister op2 = op2_loc.AsFpuRegister<XmmRegister>();

  Label nan, done, op2_label;
  if (is_double) {
    __ ucomisd(out, op2);
  } else {
    __ ucomiss(out, op2);
  }

  __ j(Condition::kParityEven, &nan);

  __ j(is_min ? Condition::kAbove : Condition::kBelow, &op2_label);
  __ j(is_min ? Condition::kBelow : Condition::kAbove, &done);

  // Handle 0.0/-0.0.
  if (is_min) {
    if (is_double) {
      __ orpd(out, op2);
    } else {
      __ orps(out, op2);
    }
  } else {
    if (is_double) {
      __ andpd(out, op2);
    } else {
      __ andps(out, op2);
    }
  }
  __ jmp(&done);

  // NaN handling.
  __ Bind(&nan);
  CpuRegister cpu_temp = locations->GetTemp(0).AsRegister<CpuRegister>();
  // TODO: Literal pool. Trades 64b immediate in CPU reg for direct memory access.
  if (is_double) {
    __ movq(cpu_temp, Immediate(INT64_C(0x7FF8000000000000)));
  } else {
    __ movl(cpu_temp, Immediate(INT64_C(0x7FC00000)));
  }
  __ movd(out, cpu_temp, is_double);
  __ jmp(&done);

  // out := op2;
  __ Bind(&op2_label);
  if (is_double) {
    __ movsd(out, op2);
  } else {
    __ movss(out, op2);
  }

  // Done.
  __ Bind(&done);

  return true;
}

static bool LocFPFPToFPPlusTemp(ArenaAllocator* arena, HInvokeStaticOrDirect* invoke) {
  LocationSummary* locations = new (arena) LocationSummary(invoke, LocationSummary::kNoCall);
  locations->SetInAt(0, Location::RequiresFpuRegister());
  locations->SetInAt(1, Location::RequiresFpuRegister());
  // The following is sub-optimal, but all we can do for now. It would be fine to also accept
  // the second input to be the output (we can simply swap inputs).
  locations->SetOut(Location::SameAsFirstInput());
  locations->AddTemp(Location::RequiresRegister());     // Immediate constant.
  return true;
}

bool IntrinsicLocationsBuilderX86_64::VisitMathMinDoubleDouble(HInvokeStaticOrDirect* invoke) {
  return LocFPFPToFPPlusTemp(arena_, invoke);
}

bool IntrinsicCodeGeneratorX86_64::VisitMathMinDoubleDouble(HInvokeStaticOrDirect* invoke) {
  return GenMinMaxFP(invoke->GetLocations(), true, true, GetAssembler());
}

bool IntrinsicLocationsBuilderX86_64::VisitMathMinFloatFloat(HInvokeStaticOrDirect* invoke) {
  return LocFPFPToFPPlusTemp(arena_, invoke);
}

bool IntrinsicCodeGeneratorX86_64::VisitMathMinFloatFloat(HInvokeStaticOrDirect* invoke) {
  return GenMinMaxFP(invoke->GetLocations(), true, false, GetAssembler());
}

bool IntrinsicLocationsBuilderX86_64::VisitMathMaxDoubleDouble(HInvokeStaticOrDirect* invoke) {
  return LocFPFPToFPPlusTemp(arena_, invoke);
}

bool IntrinsicCodeGeneratorX86_64::VisitMathMaxDoubleDouble(HInvokeStaticOrDirect* invoke) {
  return GenMinMaxFP(invoke->GetLocations(), false, true, GetAssembler());
}

bool IntrinsicLocationsBuilderX86_64::VisitMathMaxFloatFloat(HInvokeStaticOrDirect* invoke) {
  return LocFPFPToFPPlusTemp(arena_, invoke);
}

bool IntrinsicCodeGeneratorX86_64::VisitMathMaxFloatFloat(HInvokeStaticOrDirect* invoke) {
  return GenMinMaxFP(invoke->GetLocations(), false, false, GetAssembler());
}


static bool GenMinMax(LocationSummary* locations, bool is_min, bool is_long,
                      X86_64Assembler* assembler) {
  Location op1_loc = locations->InAt(0);
  Location op2_loc = locations->InAt(1);

  // Shortcut for same input locations.
  if (op1_loc.Equals(op2_loc)) {
    // Can return immediately, as op1_loc == out_loc.
    return true;
  }

  CpuRegister out = locations->Out().AsRegister<CpuRegister>();
  CpuRegister op2 = op2_loc.AsRegister<CpuRegister>();

  //  (out := op1)
  //  out <=? op2
  //  if out is min jmp done
  //  out := op2
  // done:

  if (is_long) {
    __ cmpq(out, op2);
  } else {
    __ cmpl(out, op2);
  }

  __ cmov(is_min ? Condition::kGreater : Condition::kLess, out, op2, is_long);
  return true;
}

static bool LocIntIntToInt(ArenaAllocator* arena, HInvokeStaticOrDirect* invoke) {
  LocationSummary* locations = new (arena) LocationSummary(invoke, LocationSummary::kNoCall);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RequiresRegister());
  locations->SetOut(Location::SameAsFirstInput());
  return true;
}

bool IntrinsicLocationsBuilderX86_64::VisitMathMinIntInt(HInvokeStaticOrDirect* invoke) {
  return LocIntIntToInt(arena_, invoke);
}

bool IntrinsicCodeGeneratorX86_64::VisitMathMinIntInt(HInvokeStaticOrDirect* invoke) {
  return GenMinMax(invoke->GetLocations(), true, false, GetAssembler());
}

bool IntrinsicLocationsBuilderX86_64::VisitMathMinLongLong(HInvokeStaticOrDirect* invoke) {
  return LocIntIntToInt(arena_, invoke);
}

bool IntrinsicCodeGeneratorX86_64::VisitMathMinLongLong(HInvokeStaticOrDirect* invoke) {
  return GenMinMax(invoke->GetLocations(), true, true, GetAssembler());
}

bool IntrinsicLocationsBuilderX86_64::VisitMathMaxIntInt(HInvokeStaticOrDirect* invoke) {
  return LocIntIntToInt(arena_, invoke);
}

bool IntrinsicCodeGeneratorX86_64::VisitMathMaxIntInt(HInvokeStaticOrDirect* invoke) {
  return GenMinMax(invoke->GetLocations(), false, false, GetAssembler());
}

bool IntrinsicLocationsBuilderX86_64::VisitMathMaxLongLong(HInvokeStaticOrDirect* invoke) {
  return LocIntIntToInt(arena_, invoke);
}

bool IntrinsicCodeGeneratorX86_64::VisitMathMaxLongLong(HInvokeStaticOrDirect* invoke) {
  return GenMinMax(invoke->GetLocations(), false, true, GetAssembler());
}

static bool LocFPToFP(ArenaAllocator* arena, HInvokeStaticOrDirect* invoke) {
  LocationSummary* locations = new (arena) LocationSummary(invoke, LocationSummary::kNoCall);
  locations->SetInAt(0, Location::RequiresFpuRegister());
  locations->SetOut(Location::SameAsFirstInput());
  return true;
}

bool IntrinsicLocationsBuilderX86_64::VisitMathSqrt(HInvokeStaticOrDirect* invoke) {
  return LocFPToFP(arena_, invoke);
}

bool IntrinsicCodeGeneratorX86_64::VisitMathSqrt(HInvokeStaticOrDirect* invoke) {
  LocationSummary* locations = invoke->GetLocations();
  // in should be equal to out, but we do not really care here.
  XmmRegister in = locations->InAt(0).AsFpuRegister<XmmRegister>();
  XmmRegister out = locations->Out().AsFpuRegister<XmmRegister>();

  GetAssembler()->sqrtsd(out, in);

  return true;
}

bool IntrinsicLocationsBuilderX86_64::VisitStringCharAt(HInvokeStaticOrDirect* invoke) {
  // The inputs plus one temp.
  LocationSummary* locations = new (arena_) LocationSummary(invoke,
                                                            LocationSummary::kCallOnSlowPath);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RequiresRegister());
  locations->SetOut(Location::SameAsFirstInput());
  locations->AddTemp(Location::RequiresRegister());
  return true;
}

bool IntrinsicCodeGeneratorX86_64::VisitStringCharAt(HInvokeStaticOrDirect* invoke) {
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
  //       move to implicit checks, we have to do a null check below.

  // TODO: Maybe we can support range check elimination. Overall, though, I think it's not worth
  //       the cost.
  // TODO: On x86, we can compare to memory directly, but the BoundsCheckSlowPath can't work with
  //       that, yet.
  // Note: The index parameter is already in a register, different from Quick we won't optimize
  //       constants.

  SlowPathCodeX86_64* slow_path = new (GetArena()) IntrinsicSlowPathX86_64(invoke);
  codegen_->AddSlowPath(slow_path);

  // TODO: This is what we'd like, instead of the temp:
  // GetAssembler()->cmpl(idx, Address(obj, count_offset));
  X86_64Assembler* assembler = GetAssembler();

  __ movl(temp, Address(obj, count_offset));
  __ cmpl(idx, temp);
  __ j(kAboveEqual, slow_path->GetEntryLabel());

  // Get the actual element.
  __ movl(temp, idx);                          // temp := idx.
  __ addl(temp, Address(obj, offset_offset));  // temp := offset + idx.
  __ movl(out, Address(obj, value_offset));    // obj := obj.array.
  // out = out[2*temp].
  __ movzxw(out, Address(out, temp, ScaleFactor::TIMES_2, data_offset));

  __ Bind(slow_path->GetExitLabel());

  return true;
}

static bool GenPeek(LocationSummary* locations, Primitive::Type size, X86_64Assembler* assembler) {
  CpuRegister address = locations->InAt(0).AsRegister<CpuRegister>();
  CpuRegister out = locations->Out().AsRegister<CpuRegister>();  // == address, here for clarity.
  // x86 allows unaligned access, so no worries there.
  switch (size) {
    case Primitive::Type::kPrimByte:
      __ movsxb(out, Address(address, 0));
      break;
    case Primitive::Type::kPrimShort:
      __ movsxw(out, Address(address, 0));
      break;
    case Primitive::Type::kPrimInt:
      __ movl(out, Address(address, 0));
      break;
    case Primitive::Type::kPrimLong:
      __ movq(out, Address(address, 0));
      break;
    default:
      LOG(FATAL) << "Type not recognized for peek: " << size;
      UNREACHABLE();
  }
  return true;
}

bool IntrinsicLocationsBuilderX86_64::VisitMemoryPeekByte(HInvokeStaticOrDirect* invoke) {
  return LocIntToInt(arena_, invoke);
}

bool IntrinsicCodeGeneratorX86_64::VisitMemoryPeekByte(HInvokeStaticOrDirect* invoke) {
  return GenPeek(invoke->GetLocations(), Primitive::Type::kPrimByte, GetAssembler());
}

bool IntrinsicLocationsBuilderX86_64::VisitMemoryPeekIntNative(HInvokeStaticOrDirect* invoke) {
  return LocIntToInt(arena_, invoke);
}

bool IntrinsicCodeGeneratorX86_64::VisitMemoryPeekIntNative(HInvokeStaticOrDirect* invoke) {
  return GenPeek(invoke->GetLocations(), Primitive::Type::kPrimInt, GetAssembler());
}

bool IntrinsicLocationsBuilderX86_64::VisitMemoryPeekLongNative(HInvokeStaticOrDirect* invoke) {
  return LocIntToInt(arena_, invoke);
}

bool IntrinsicCodeGeneratorX86_64::VisitMemoryPeekLongNative(HInvokeStaticOrDirect* invoke) {
  return GenPeek(invoke->GetLocations(), Primitive::Type::kPrimLong, GetAssembler());
}

bool IntrinsicLocationsBuilderX86_64::VisitMemoryPeekShortNative(HInvokeStaticOrDirect* invoke) {
  return LocIntToInt(arena_, invoke);
}

bool IntrinsicCodeGeneratorX86_64::VisitMemoryPeekShortNative(HInvokeStaticOrDirect* invoke) {
  return GenPeek(invoke->GetLocations(), Primitive::Type::kPrimShort, GetAssembler());
}

static bool LocIntIntToVoid(ArenaAllocator* arena, HInvokeStaticOrDirect* invoke) {
  LocationSummary* locations = new (arena) LocationSummary(invoke, LocationSummary::kNoCall);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RequiresRegister());
  return true;
}

static bool GenPoke(LocationSummary* locations, Primitive::Type size, X86_64Assembler* assembler) {
  CpuRegister address = locations->InAt(0).AsRegister<CpuRegister>();
  CpuRegister value = locations->InAt(1).AsRegister<CpuRegister>();
  // x86 allows unaligned access, so no worries there.
  switch (size) {
    case Primitive::Type::kPrimByte:
      __ movb(Address(address, 0), value);
      break;
    case Primitive::Type::kPrimShort:
      __ movw(Address(address, 0), value);
      break;
    case Primitive::Type::kPrimInt:
      __ movl(Address(address, 0), value);
      break;
    case Primitive::Type::kPrimLong:
      __ movq(Address(address, 0), value);
      break;
    default:
      LOG(FATAL) << "Type not recognized for poke: " << size;
      UNREACHABLE();
  }
  return true;
}

bool IntrinsicLocationsBuilderX86_64::VisitMemoryPokeByte(HInvokeStaticOrDirect* invoke) {
  return LocIntIntToVoid(arena_, invoke);
}

bool IntrinsicCodeGeneratorX86_64::VisitMemoryPokeByte(HInvokeStaticOrDirect* invoke) {
  return GenPoke(invoke->GetLocations(), Primitive::Type::kPrimByte, GetAssembler());
}

bool IntrinsicLocationsBuilderX86_64::VisitMemoryPokeIntNative(HInvokeStaticOrDirect* invoke) {
  return LocIntIntToVoid(arena_, invoke);
}

bool IntrinsicCodeGeneratorX86_64::VisitMemoryPokeIntNative(HInvokeStaticOrDirect* invoke) {
  return GenPoke(invoke->GetLocations(), Primitive::Type::kPrimInt, GetAssembler());
}

bool IntrinsicLocationsBuilderX86_64::VisitMemoryPokeLongNative(HInvokeStaticOrDirect* invoke) {
  return LocIntIntToVoid(arena_, invoke);
}

bool IntrinsicCodeGeneratorX86_64::VisitMemoryPokeLongNative(HInvokeStaticOrDirect* invoke) {
  return GenPoke(invoke->GetLocations(), Primitive::Type::kPrimLong, GetAssembler());
}

bool IntrinsicLocationsBuilderX86_64::VisitMemoryPokeShortNative(HInvokeStaticOrDirect* invoke) {
  return LocIntIntToVoid(arena_, invoke);
}

bool IntrinsicCodeGeneratorX86_64::VisitMemoryPokeShortNative(HInvokeStaticOrDirect* invoke) {
  return GenPoke(invoke->GetLocations(), Primitive::Type::kPrimShort, GetAssembler());
}

bool IntrinsicLocationsBuilderX86_64::VisitThreadCurrentThread(HInvokeStaticOrDirect* invoke) {
  LocationSummary* locations = new (arena_) LocationSummary(invoke, LocationSummary::kNoCall);
  locations->SetOut(Location::RequiresRegister());
  return true;
}

bool IntrinsicCodeGeneratorX86_64::VisitThreadCurrentThread(HInvokeStaticOrDirect* invoke) {
  CpuRegister out = invoke->GetLocations()->Out().AsRegister<CpuRegister>();
  GetAssembler()->gs()->movl(out, Address::Absolute(Thread::PeerOffset<8>(), true));
  return true;
}

}  // namespace x86_64
}  // namespace art
