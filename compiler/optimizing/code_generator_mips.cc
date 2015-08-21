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

#include "code_generator_mips.h"

#include "entrypoints/quick/quick_entrypoints.h"
#include "entrypoints/quick/quick_entrypoints_enum.h"
#include "gc/accounting/card_table.h"
#include "intrinsics.h"
#include "art_method.h"
#include "mirror/array-inl.h"
#include "mirror/class-inl.h"
#include "offsets.h"
#include "thread.h"
#include "utils/mips/assembler_mips.h"
#include "utils/assembler.h"
#include "utils/stack_checks.h"

namespace art {
namespace mips {

static bool ExpectedPairLayout(Location location) {
  // We expected this for both core and FPU register pairs.
  return ((location.low() & 1) == 0) && (location.low() + 1 == location.high());
}

static constexpr DRegister FromLowSToD(FRegister reg) {
  return DCHECK_CONSTEXPR(reg % 2 == 0, , D0)
      static_cast<DRegister>(reg / 2);
}

static constexpr int kCurrentMethodStackOffset = 0;
static constexpr Register kMethodRegisterArgument = A0;

// We need extra temporary/scratch registers (in addition to AT) in some cases.
static constexpr Register TMP = T8;
static constexpr FRegister FTMP = F8;
static constexpr FRegister FTMP2 = F9;
static constexpr DRegister DTMP = FromLowSToD(FTMP);  // == (FTMP, FTMP2) == (F8, F9).

// ART Thread Register.
static constexpr Register TR = S1;

Location MipsReturnLocation(Primitive::Type return_type) {
  switch (return_type) {
    case Primitive::kPrimBoolean:
    case Primitive::kPrimByte:
    case Primitive::kPrimChar:
    case Primitive::kPrimShort:
    case Primitive::kPrimInt:
    case Primitive::kPrimNot:
      return Location::RegisterLocation(V0);

    case Primitive::kPrimLong:
      return Location::RegisterPairLocation(V0, V1);

    case Primitive::kPrimFloat:
      return Location::FpuRegisterLocation(F0);

    case Primitive::kPrimDouble:
      return Location::FpuRegisterPairLocation(F0, F1);

    case Primitive::kPrimVoid:
      return Location();
  }
  UNREACHABLE();
}

Location InvokeDexCallingConventionVisitorMIPS::GetReturnLocation(Primitive::Type type) const {
  return MipsReturnLocation(type);
}

Location InvokeDexCallingConventionVisitorMIPS::GetMethodLocation() const {
  return Location::RegisterLocation(kMethodRegisterArgument);
}

Location InvokeDexCallingConventionVisitorMIPS::GetNextLocation(Primitive::Type type) {
  switch (type) {
    case Primitive::kPrimBoolean:
    case Primitive::kPrimByte:
    case Primitive::kPrimChar:
    case Primitive::kPrimShort:
    case Primitive::kPrimInt:
    case Primitive::kPrimNot: {
      uint32_t index = gp_index_++;
      uint32_t stack_index = stack_index_++;
      if (index < calling_convention.GetNumberOfRegisters()) {
        return Location::RegisterLocation(calling_convention.GetRegisterAt(index));
      } else {
        return Location::StackSlot(calling_convention.GetStackOffsetOf(stack_index));
      }
    }

    case Primitive::kPrimLong: {
      uint32_t index = gp_index_;
      uint32_t stack_index = stack_index_;
      gp_index_ += 2;
      stack_index_ += 2;
      if (index + 1 < calling_convention.GetNumberOfRegisters()) {
        if (calling_convention.GetRegisterAt(index) == A1) {
          // Skip A1, and use A2_A3 instead.
          gp_index_++;
          index++;
        }
      }
      if (index + 1 < calling_convention.GetNumberOfRegisters()) {
        DCHECK_EQ(calling_convention.GetRegisterAt(index) + 1,
                  calling_convention.GetRegisterAt(index + 1));
        return Location::RegisterPairLocation(calling_convention.GetRegisterAt(index),
                                              calling_convention.GetRegisterAt(index + 1));
      } else {
        return Location::DoubleStackSlot(calling_convention.GetStackOffsetOf(stack_index));
      }
    }

    case Primitive::kPrimFloat: {
      uint32_t stack_index = stack_index_++;
      if (float_index_ % 2 == 0) {
        float_index_ = std::max(double_index_, float_index_);
      }
      if (float_index_ < calling_convention.GetNumberOfFpuRegisters()) {
        return Location::FpuRegisterLocation(calling_convention.GetFpuRegisterAt(float_index_++));
      } else {
        return Location::StackSlot(calling_convention.GetStackOffsetOf(stack_index));
      }
    }

    case Primitive::kPrimDouble: {
      double_index_ = std::max(double_index_, RoundUp(float_index_, 2));
      uint32_t stack_index = stack_index_;
      stack_index_ += 2;
      if (double_index_ + 1 < calling_convention.GetNumberOfFpuRegisters()) {
        uint32_t index = double_index_;
        double_index_ += 2;
        Location result = Location::FpuRegisterPairLocation(
          calling_convention.GetFpuRegisterAt(index),
          calling_convention.GetFpuRegisterAt(index + 1));
        DCHECK(ExpectedPairLayout(result));
        return result;
      } else {
        return Location::DoubleStackSlot(calling_convention.GetStackOffsetOf(stack_index));
      }
    }

    case Primitive::kPrimVoid:
      LOG(FATAL) << "Unexpected parameter type " << type;
      break;
  }
  return Location();
}

Location InvokeRuntimeCallingConvention::GetReturnLocation(Primitive::Type type) {
  return MipsReturnLocation(type);
}

#define __ down_cast<CodeGeneratorMIPS*>(codegen)->GetAssembler()->
#define QUICK_ENTRY_POINT(x) QUICK_ENTRYPOINT_OFFSET(kMipsWordSize, x).Int32Value()

class BoundsCheckSlowPathMIPS : public SlowPathCodeMIPS {
 public:
  BoundsCheckSlowPathMIPS(HBoundsCheck* instruction,
                          Location index_location,
                          Location length_location)
      : instruction_(instruction),
        index_location_(index_location),
        length_location_(length_location) {}

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    CodeGeneratorMIPS* mips_codegen = down_cast<CodeGeneratorMIPS*>(codegen);
    __ Bind(GetEntryLabel(), /* is_jump */ false);  // TODO: Check second arg.
    // We're moving two locations to locations that could overlap, so we need a parallel
    // move resolver.
    InvokeRuntimeCallingConvention calling_convention;
    codegen->EmitParallelMoves(index_location_,
                               Location::RegisterLocation(calling_convention.GetRegisterAt(0)),
                               Primitive::kPrimInt,
                               length_location_,
                               Location::RegisterLocation(calling_convention.GetRegisterAt(1)),
                               Primitive::kPrimInt);
    mips_codegen->InvokeRuntime(QUICK_ENTRY_POINT(pThrowArrayBounds),
                                instruction_,
                                instruction_->GetDexPc(),
                                this);
    CheckEntrypointTypes<kQuickThrowArrayBounds, void, int32_t, int32_t>();
  }

  bool IsFatal() const OVERRIDE { return true; }

  const char* GetDescription() const OVERRIDE { return "BoundsCheckSlowPathMIPS"; }

 private:
  HBoundsCheck* const instruction_;
  const Location index_location_;
  const Location length_location_;

  DISALLOW_COPY_AND_ASSIGN(BoundsCheckSlowPathMIPS);
};

class DivZeroCheckSlowPathMIPS : public SlowPathCodeMIPS {
 public:
  explicit DivZeroCheckSlowPathMIPS(HDivZeroCheck* instruction) : instruction_(instruction) {}

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    CodeGeneratorMIPS* mips_codegen = down_cast<CodeGeneratorMIPS*>(codegen);
    __ Bind(GetEntryLabel(), /* is_jump */ false);  // TODO: Check second arg.
    mips_codegen->InvokeRuntime(QUICK_ENTRY_POINT(pThrowDivZero),
                                instruction_,
                                instruction_->GetDexPc(),
                                this);
    CheckEntrypointTypes<kQuickThrowDivZero, void, void>();
  }

  bool IsFatal() const OVERRIDE { return true; }

  const char* GetDescription() const OVERRIDE { return "DivZeroCheckSlowPathMIPS"; }

 private:
  HDivZeroCheck* const instruction_;
  DISALLOW_COPY_AND_ASSIGN(DivZeroCheckSlowPathMIPS);
};

class LoadClassSlowPathMIPS : public SlowPathCodeMIPS {
 public:
  LoadClassSlowPathMIPS(HLoadClass* cls,
                        HInstruction* at,
                        uint32_t dex_pc,
                        bool do_clinit)
      : cls_(cls), at_(at), dex_pc_(dex_pc), do_clinit_(do_clinit) {
    DCHECK(at->IsLoadClass() || at->IsClinitCheck());
  }

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    LocationSummary* locations = at_->GetLocations();
    CodeGeneratorMIPS* mips_codegen = down_cast<CodeGeneratorMIPS*>(codegen);

    __ Bind(GetEntryLabel(), /* is_jump */ false);  // TODO: Check second arg.
    SaveLiveRegisters(codegen, locations);

    InvokeRuntimeCallingConvention calling_convention;
    __ LoadImmediate(calling_convention.GetRegisterAt(0), cls_->GetTypeIndex());
    int32_t entry_point_offset = do_clinit_ ? QUICK_ENTRY_POINT(pInitializeStaticStorage)
                                            : QUICK_ENTRY_POINT(pInitializeType);
    mips_codegen->InvokeRuntime(entry_point_offset, at_, dex_pc_, this);
    if (do_clinit_) {
      CheckEntrypointTypes<kQuickInitializeStaticStorage, void*, uint32_t>();
    } else {
      CheckEntrypointTypes<kQuickInitializeType, void*, uint32_t>();
    }

    // Move the class to the desired location.
    Location out = locations->Out();
    if (out.IsValid()) {
      DCHECK(out.IsRegister() && !locations->GetLiveRegisters()->ContainsCoreRegister(out.reg()));
      Primitive::Type type = at_->GetType();
      mips_codegen->Move32(out, calling_convention.GetReturnLocation(type));
    }

    RestoreLiveRegisters(codegen, locations);
    __ J(GetExitLabel());
  }

  const char* GetDescription() const OVERRIDE { return "LoadClassSlowPathMIPS"; }

 private:
  // The class this slow path will load.
  HLoadClass* const cls_;

  // The instruction where this slow path is happening.
  // (Might be the load class or an initialization check).
  HInstruction* const at_;

  // The dex PC of `at_`.
  const uint32_t dex_pc_;

  // Whether to initialize the class.
  const bool do_clinit_;

  DISALLOW_COPY_AND_ASSIGN(LoadClassSlowPathMIPS);
};

class LoadStringSlowPathMIPS : public SlowPathCodeMIPS {
 public:
  explicit LoadStringSlowPathMIPS(HLoadString* instruction) : instruction_(instruction) {}

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    LocationSummary* locations = instruction_->GetLocations();
    DCHECK(!locations->GetLiveRegisters()->ContainsCoreRegister(locations->Out().reg()));
    CodeGeneratorMIPS* mips_codegen = down_cast<CodeGeneratorMIPS*>(codegen);

    __ Bind(GetEntryLabel(), /* is_jump */ false);  // TODO: Check second arg.
    SaveLiveRegisters(codegen, locations);

    InvokeRuntimeCallingConvention calling_convention;
    __ LoadImmediate(calling_convention.GetRegisterAt(0), instruction_->GetStringIndex());
    mips_codegen->InvokeRuntime(QUICK_ENTRY_POINT(pResolveString),
                                instruction_,
                                instruction_->GetDexPc(),
                                this);
    CheckEntrypointTypes<kQuickResolveString, void*, uint32_t>();
    Primitive::Type type = instruction_->GetType();
    mips_codegen->Move32(locations->Out(), calling_convention.GetReturnLocation(type));

    RestoreLiveRegisters(codegen, locations);
    __ J(GetExitLabel());
  }

  const char* GetDescription() const OVERRIDE { return "LoadStringSlowPathMIPS"; }

 private:
  HLoadString* const instruction_;

  DISALLOW_COPY_AND_ASSIGN(LoadStringSlowPathMIPS);
};

class NullCheckSlowPathMIPS : public SlowPathCodeMIPS {
 public:
  explicit NullCheckSlowPathMIPS(HNullCheck* instr) : instruction_(instr) {}

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    CodeGeneratorMIPS* mips_codegen = down_cast<CodeGeneratorMIPS*>(codegen);
    __ Bind(GetEntryLabel(), /* is_jump */ false);  // TODO: Check second arg.
    mips_codegen->InvokeRuntime(QUICK_ENTRY_POINT(pThrowNullPointer),
                                instruction_,
                                instruction_->GetDexPc(),
                                this);
    CheckEntrypointTypes<kQuickThrowNullPointer, void, void>();
  }

  bool IsFatal() const OVERRIDE { return true; }

  const char* GetDescription() const OVERRIDE { return "NullCheckSlowPathMIPS"; }

 private:
  HNullCheck* const instruction_;

  DISALLOW_COPY_AND_ASSIGN(NullCheckSlowPathMIPS);
};

class SuspendCheckSlowPathMIPS : public SlowPathCodeMIPS {
 public:
  explicit SuspendCheckSlowPathMIPS(HSuspendCheck* instruction, HBasicBlock* successor)
      : instruction_(instruction), successor_(successor) {}

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    CodeGeneratorMIPS* mips_codegen = down_cast<CodeGeneratorMIPS*>(codegen);
    __ Bind(GetEntryLabel(), /* is_jump */ false);  // TODO: Check second arg.
    SaveLiveRegisters(codegen, instruction_->GetLocations());
    mips_codegen->InvokeRuntime(QUICK_ENTRY_POINT(pTestSuspend),
                                instruction_,
                                instruction_->GetDexPc(),
                                this);
    CheckEntrypointTypes<kQuickTestSuspend, void, void>();
    RestoreLiveRegisters(codegen, instruction_->GetLocations());
    if (successor_ == nullptr) {
      __ J(GetReturnLabel());
    } else {
      __ J(mips_codegen->GetLabelOf(successor_));
    }
  }

  Label* GetReturnLabel() {
    DCHECK(successor_ == nullptr);
    return &return_label_;
  }

  const char* GetDescription() const OVERRIDE { return "SuspendCheckSlowPathMIPS"; }

 private:
  HSuspendCheck* const instruction_;
  // If not null, the block to branch to after the suspend check.
  HBasicBlock* const successor_;

  // If `successor_` is null, the label to branch to after the suspend check.
  Label return_label_;

  DISALLOW_COPY_AND_ASSIGN(SuspendCheckSlowPathMIPS);
};

class TypeCheckSlowPathMIPS : public SlowPathCodeMIPS {
 public:
  TypeCheckSlowPathMIPS(HInstruction* instruction,
                        Location class_to_check,
                        Location object_class,
                        uint32_t dex_pc)
      : instruction_(instruction),
        class_to_check_(class_to_check),
        object_class_(object_class),
        dex_pc_(dex_pc) {}

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    LocationSummary* locations = instruction_->GetLocations();
    DCHECK(instruction_->IsCheckCast()
           || !locations->GetLiveRegisters()->ContainsCoreRegister(locations->Out().reg()));
    CodeGeneratorMIPS* mips_codegen = down_cast<CodeGeneratorMIPS*>(codegen);

    __ Bind(GetEntryLabel(), /* is_jump */ false);  // TODO: Check second arg.
    SaveLiveRegisters(codegen, locations);

    // We're moving two locations to locations that could overlap, so we need a parallel
    // move resolver.
    InvokeRuntimeCallingConvention calling_convention;
    codegen->EmitParallelMoves(class_to_check_,
                               Location::RegisterLocation(calling_convention.GetRegisterAt(0)),
                               Primitive::kPrimNot,
                               object_class_,
                               Location::RegisterLocation(calling_convention.GetRegisterAt(1)),
                               Primitive::kPrimNot);

    if (instruction_->IsInstanceOf()) {
      mips_codegen->InvokeRuntime(QUICK_ENTRY_POINT(pInstanceofNonTrivial),
                                  instruction_,
                                  dex_pc_,
                                  this);
      Primitive::Type ret_type = instruction_->GetType();
      Location ret_loc = calling_convention.GetReturnLocation(ret_type);
      mips_codegen->Move32(locations->Out(), ret_loc);
      CheckEntrypointTypes<kQuickInstanceofNonTrivial,
                           uint32_t,
                           const mirror::Class*,
                           const mirror::Class*>();
    } else {
      DCHECK(instruction_->IsCheckCast());
      mips_codegen->InvokeRuntime(QUICK_ENTRY_POINT(pCheckCast), instruction_, dex_pc_, this);
      CheckEntrypointTypes<kQuickCheckCast, void, const mirror::Class*, const mirror::Class*>();
    }

    RestoreLiveRegisters(codegen, locations);
    __ J(GetExitLabel());
  }

  const char* GetDescription() const OVERRIDE { return "TypeCheckSlowPathMIPS"; }

 private:
  HInstruction* const instruction_;
  const Location class_to_check_;
  const Location object_class_;
  uint32_t dex_pc_;

  DISALLOW_COPY_AND_ASSIGN(TypeCheckSlowPathMIPS);
};

class DeoptimizationSlowPathMIPS : public SlowPathCodeMIPS {
 public:
  explicit DeoptimizationSlowPathMIPS(HInstruction* instruction)
    : instruction_(instruction) {}

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    __ Bind(GetEntryLabel(), /* is_jump */ false);  // TODO: Check second arg.
    SaveLiveRegisters(codegen, instruction_->GetLocations());
    DCHECK(instruction_->IsDeoptimize());
    HDeoptimize* deoptimize = instruction_->AsDeoptimize();
    uint32_t dex_pc = deoptimize->GetDexPc();
    CodeGeneratorMIPS* mips_codegen = down_cast<CodeGeneratorMIPS*>(codegen);
    mips_codegen->InvokeRuntime(QUICK_ENTRY_POINT(pDeoptimize), instruction_, dex_pc, this);
  }

  const char* GetDescription() const OVERRIDE { return "DeoptimizationSlowPathMIPS"; }

 private:
  HInstruction* const instruction_;
  DISALLOW_COPY_AND_ASSIGN(DeoptimizationSlowPathMIPS);
};

CodeGeneratorMIPS::CodeGeneratorMIPS(HGraph* graph,
                                     const MipsInstructionSetFeatures& isa_features,
                                     const CompilerOptions& compiler_options)
    : CodeGenerator(graph,
                    kNumberOfCoreRegisters,
                    kNumberOfFRegisters,
                    0,  // kNumberOfRegisterPairs
                    ComputeRegisterMask(reinterpret_cast<const int*>(kCoreCalleeSaves),
                                        arraysize(kCoreCalleeSaves)),
                    ComputeRegisterMask(reinterpret_cast<const int*>(kFpuCalleeSaves),
                                        arraysize(kFpuCalleeSaves)),
                    compiler_options),
      block_labels_(graph->GetArena(), 0),
      location_builder_(graph, this),
      instruction_visitor_(graph, this),
      move_resolver_(graph->GetArena(), this),
      isa_features_(isa_features) {
  // Save RA (containing the return address) to mimic Quick.
  AddAllocatedRegister(Location::RegisterLocation(RA));
}

#undef __
#define __ down_cast<MipsAssembler*>(GetAssembler())->
#define QUICK_ENTRY_POINT(x) QUICK_ENTRYPOINT_OFFSET(kMipsWordSize, x).Int32Value()

void CodeGeneratorMIPS::Finalize(CodeAllocator* allocator) {
  CodeGenerator::Finalize(allocator);
}

MipsAssembler* ParallelMoveResolverMIPS::GetAssembler() const {
  return codegen_->GetAssembler();
}

void ParallelMoveResolverMIPS::EmitMove(size_t index) {
  MoveOperands* move = moves_.Get(index);
  Location source = move->GetSource();
  Location destination = move->GetDestination();

  if (source.IsRegister()) {
    if (destination.IsRegister()) {
      __ Move(destination.AsRegister<Register>(), source.AsRegister<Register>());
    } else {
      DCHECK(destination.IsStackSlot());
      __ StoreToOffset(kStoreWord, source.AsRegister<Register>(),
                       SP, destination.GetStackIndex());
    }
  } else if (source.IsStackSlot()) {
    if (destination.IsRegister()) {
      __ LoadFromOffset(kLoadWord, destination.AsRegister<Register>(),
                        SP, source.GetStackIndex());
    } else if (destination.IsFpuRegister()) {
      __ LoadSFromOffset(destination.AsFpuRegister<FRegister>(), SP, source.GetStackIndex());
    } else {
      DCHECK(destination.IsStackSlot());
      __ LoadFromOffset(kLoadWord, TMP, SP, source.GetStackIndex());
      __ StoreToOffset(kStoreWord, TMP, SP, destination.GetStackIndex());
    }
  } else if (source.IsFpuRegister()) {
    if (destination.IsFpuRegister()) {
      __ MovS(destination.AsFpuRegister<FRegister>(), source.AsFpuRegister<FRegister>());
    } else {
      DCHECK(destination.IsStackSlot());
      __ StoreSToOffset(source.AsFpuRegister<FRegister>(), SP, destination.GetStackIndex());
    }
  } else if (source.IsDoubleStackSlot()) {
    if (destination.IsDoubleStackSlot()) {
      __ LoadDFromOffset(DTMP, SP, source.GetStackIndex());
      __ StoreDToOffset(DTMP, SP, destination.GetStackIndex());
    } else if (destination.IsRegisterPair()) {
      DCHECK(ExpectedPairLayout(destination));
      __ LoadFromOffset(kLoadWord,
                        destination.AsRegisterPairLow<Register>(),
                        SP,
                        source.GetStackIndex());
      __ LoadFromOffset(kLoadWord,
                        destination.AsRegisterPairHigh<Register>(),
                        SP,
                        source.GetHighStackIndex(kMipsWordSize));
    } else {
      DCHECK(destination.IsFpuRegisterPair()) << destination;
      __ LoadDFromOffset(FromLowSToD(destination.AsFpuRegisterPairLow<FRegister>()),
                         SP,
                         source.GetStackIndex());
    }
  } else if (source.IsRegisterPair()) {
    if (destination.IsRegisterPair()) {
      __ Move(destination.AsRegisterPairLow<Register>(), source.AsRegisterPairLow<Register>());
      __ Move(destination.AsRegisterPairHigh<Register>(), source.AsRegisterPairHigh<Register>());
    } else {
      DCHECK(destination.IsDoubleStackSlot()) << destination;
      DCHECK(ExpectedPairLayout(source));
      __ StoreToOffset(kStoreWord,
                       source.AsRegisterPairLow<Register>(),
                       SP,
                       destination.GetStackIndex());
      __ StoreToOffset(kStoreWord,
                       source.AsRegisterPairHigh<Register>(),
                       SP,
                       destination.GetHighStackIndex(kMipsWordSize));
    }
  } else if (source.IsFpuRegisterPair()) {
    if (destination.IsFpuRegisterPair()) {
      __ MovD(FromLowSToD(destination.AsFpuRegisterPairLow<FRegister>()),
              FromLowSToD(source.AsFpuRegisterPairLow<FRegister>()));
    } else {
      DCHECK(destination.IsDoubleStackSlot()) << destination;
      __ StoreDToOffset(FromLowSToD(source.AsFpuRegisterPairLow<FRegister>()),
                        SP,
                        destination.GetStackIndex());
    }
  } else {
    DCHECK(source.IsConstant()) << source;
    HConstant* constant = source.GetConstant();
    if (constant->IsIntConstant() || constant->IsNullConstant()) {
      int32_t value = CodeGenerator::GetInt32ValueOf(constant);
      if (destination.IsRegister()) {
        __ LoadImmediate(destination.AsRegister<Register>(), value);
      } else {
        DCHECK(destination.IsStackSlot());
        __ LoadImmediate(TMP, value);
        __ StoreToOffset(kStoreWord, TMP, SP, destination.GetStackIndex());
      }
    } else if (constant->IsLongConstant()) {
      int64_t value = constant->AsLongConstant()->GetValue();
      if (destination.IsRegisterPair()) {
        __ LoadImmediate(destination.AsRegisterPairLow<Register>(), Low32Bits(value));
        __ LoadImmediate(destination.AsRegisterPairHigh<Register>(), High32Bits(value));
      } else {
        DCHECK(destination.IsDoubleStackSlot()) << destination;
        __ LoadImmediate(TMP, Low32Bits(value));
        __ StoreToOffset(kStoreWord, TMP, SP, destination.GetStackIndex());
        __ LoadImmediate(TMP, High32Bits(value));
        __ StoreToOffset(kStoreWord, TMP, SP, destination.GetHighStackIndex(kMipsWordSize));
      }
    } else if (constant->IsDoubleConstant()) {
      double value = constant->AsDoubleConstant()->GetValue();
      if (destination.IsFpuRegisterPair()) {
        __ LoadDImmediate(FromLowSToD(destination.AsFpuRegisterPairLow<FRegister>()), value);
      } else {
        DCHECK(destination.IsDoubleStackSlot()) << destination;
        uint64_t int_value = bit_cast<uint64_t, double>(value);
        __ LoadImmediate(TMP, Low32Bits(int_value));
        __ StoreToOffset(kStoreWord, TMP, SP, destination.GetStackIndex());
        __ LoadImmediate(TMP, High32Bits(int_value));
        __ StoreToOffset(kStoreWord, TMP, SP, destination.GetHighStackIndex(kMipsWordSize));
      }
    } else {
      DCHECK(constant->IsFloatConstant()) << constant->DebugName();
      float value = constant->AsFloatConstant()->GetValue();
      if (destination.IsFpuRegister()) {
        __ LoadSImmediate(destination.AsFpuRegister<FRegister>(), value);
      } else {
        DCHECK(destination.IsStackSlot());
        __ LoadImmediate(TMP, bit_cast<int32_t, float>(value));
        __ StoreToOffset(kStoreWord, TMP, SP, destination.GetStackIndex());
      }
    }
  }
}

void ParallelMoveResolverMIPS::EmitSwap(size_t index) {
  MoveOperands* move = moves_.Get(index);
  Location source = move->GetSource();
  Location destination = move->GetDestination();

  if (source.IsRegister() && destination.IsRegister()) {
    DCHECK_NE(source.AsRegister<Register>(), TMP);
    DCHECK_NE(destination.AsRegister<Register>(), TMP);
    __ Move(TMP, source.AsRegister<Register>());
    __ Move(source.AsRegister<Register>(), destination.AsRegister<Register>());
    __ Move(destination.AsRegister<Register>(), TMP);
  } else if (source.IsRegister() && destination.IsStackSlot()) {
    Exchange(source.AsRegister<Register>(), destination.GetStackIndex());
  } else if (source.IsStackSlot() && destination.IsRegister()) {
    Exchange(destination.AsRegister<Register>(), source.GetStackIndex());
  } else if (source.IsStackSlot() && destination.IsStackSlot()) {
    Exchange(source.GetStackIndex(), destination.GetStackIndex());
  } else if (source.IsFpuRegister() && destination.IsFpuRegister()) {
    __ Mfc1(TMP, source.AsFpuRegister<FRegister>());
    __ MovS(source.AsFpuRegister<FRegister>(), destination.AsFpuRegister<FRegister>());
    __ Mtc1(destination.AsFpuRegister<FRegister>(), TMP);
  } else if (source.IsRegisterPair() && destination.IsRegisterPair()) {
    // Swap low part.
    __ Mtc1(FTMP, source.AsRegisterPairLow<Register>());
    __ Mtc1(FTMP2, source.AsRegisterPairHigh<Register>());
    __ Move(source.AsRegisterPairLow<Register>(), destination.AsRegisterPairLow<Register>());
    __ Move(source.AsRegisterPairHigh<Register>(), destination.AsRegisterPairHigh<Register>());
    __ Mfc1(destination.AsRegisterPairLow<Register>(), FTMP);
    __ Mfc1(destination.AsRegisterPairHigh<Register>(), FTMP2);
  } else if (source.IsRegisterPair() || destination.IsRegisterPair()) {
    Register low_reg = source.IsRegisterPair()
        ? source.AsRegisterPairLow<Register>()
        : destination.AsRegisterPairLow<Register>();
    Register high_reg = source.IsRegisterPair()
        ? source.AsRegisterPairHigh<Register>()
        : destination.AsRegisterPairHigh<Register>();
    int low_stack_slot = source.IsRegisterPair()
        ? destination.GetStackIndex()
        : source.GetStackIndex();
    int high_stack_slot = source.IsRegisterPair()
        ? destination.GetHighStackIndex(kMipsWordSize)
        : source.GetHighStackIndex(kMipsWordSize);
    DCHECK(ExpectedPairLayout(source.IsRegisterPair() ? source : destination));
    __ Mtc1(FTMP, low_reg);
    __ Mtc1(FTMP2, high_reg);
    __ LoadFromOffset(kLoadWord, low_reg, SP, low_stack_slot);
    __ LoadFromOffset(kLoadWord, high_reg, SP, high_stack_slot);
    __ StoreDToOffset(DTMP, SP, low_stack_slot);
  } else if (source.IsFpuRegisterPair() && destination.IsFpuRegisterPair()) {
    DRegister first = FromLowSToD(source.AsFpuRegisterPairLow<FRegister>());
    DRegister second = FromLowSToD(destination.AsFpuRegisterPairLow<FRegister>());
    __ MovD(DTMP, first);
    __ MovD(first, second);
    __ MovD(second, DTMP);
  } else if (source.IsFpuRegisterPair() || destination.IsFpuRegisterPair()) {
    DRegister reg = source.IsFpuRegisterPair()
        ? FromLowSToD(source.AsFpuRegisterPairLow<FRegister>())
        : FromLowSToD(destination.AsFpuRegisterPairLow<FRegister>());
    int mem = source.IsFpuRegisterPair() ? destination.GetStackIndex() : source.GetStackIndex();
    __ MovD(DTMP, reg);
    __ LoadDFromOffset(reg, SP, mem);
    __ StoreDToOffset(DTMP, SP, mem);
  } else if (source.IsFpuRegister() || destination.IsFpuRegister()) {
    FRegister reg = source.IsFpuRegister()
        ? source.AsFpuRegister<FRegister>()
        : destination.AsFpuRegister<FRegister>();
    int mem = source.IsFpuRegister() ? destination.GetStackIndex() : source.GetStackIndex();
    __ MovS(FTMP, reg);
    __ LoadSFromOffset(reg, SP, mem);
    __ StoreSToOffset(FTMP, SP, mem);
  } else if (source.IsDoubleStackSlot() && destination.IsDoubleStackSlot()) {
    Exchange(source.GetStackIndex(), destination.GetStackIndex());
    Exchange(source.GetHighStackIndex(kMipsWordSize),
             destination.GetHighStackIndex(kMipsWordSize));
  } else {
    LOG(FATAL) << "Unimplemented" << source << " <-> " << destination;
  }
}

void ParallelMoveResolverMIPS::RestoreScratch(int reg) {
  __ Pop(Register(reg));
}

void ParallelMoveResolverMIPS::SpillScratch(int reg) {
  __ Push(Register(reg));
}

void ParallelMoveResolverMIPS::Exchange(int index1, int index2) {
  // Allocate a scratch register other than TMP, if available.
  // Else, spill V0 (arbitrary choice) and use it as a scratch register (it will be
  // automatically unspilled when the scratch scope object is destroyed).
  ScratchRegisterScope ensure_scratch(this, TMP, V0, codegen_->GetNumberOfCoreRegisters());
  // If V0 spills onto the stack, SP-relative offsets need to be adjusted.
  int stack_offset = ensure_scratch.IsSpilled() ? kMipsWordSize : 0;
  __ LoadFromOffset(kLoadWord,
                    Register(ensure_scratch.GetRegister()),
                    SP,
                    index1 + stack_offset);
  __ LoadFromOffset(kLoadWord,
                    TMP,
                    SP,
                    index2 + stack_offset);
  __ StoreToOffset(kStoreWord,
                   Register(ensure_scratch.GetRegister()),
                   SP,
                   index2 + stack_offset);
  __ StoreToOffset(kStoreWord, TMP, SP, index1 + stack_offset);
}

static dwarf::Reg DWARFReg(Register reg) {
  return dwarf::Reg::MipsCore(static_cast<int>(reg));
}

// TODO: mapping of floating-point registers to DWARF

void CodeGeneratorMIPS::GenerateFrameEntry() {
  __ Bind(&frame_entry_label_, /* is_jump */ true);  // TODO: Check second arg.

  bool do_overflow_check = FrameNeedsStackCheck(GetFrameSize(), kMips) || !IsLeafMethod();

  if (do_overflow_check) {
    __ LoadFromOffset(kLoadWord,
                      ZERO,
                      SP,
                      -static_cast<int32_t>(GetStackOverflowReservedBytes(kMips)));
    RecordPcInfo(nullptr, 0);
  }

  // TODO: anything related to T9/GP/GOT/PIC/.so's?

  if (HasEmptyFrame()) {
    return;
  }

  // Make sure the frame size isn't unreasonably large. Per the various APIs
  // it looks like it should always be less than 2GB in size, which allows
  // us using 32-bit signed offsets from the stack pointer.
  if (GetFrameSize() > 0x7FFFFFFF)
    LOG(FATAL) << "Stack frame larger than 2GB";

  // Spill callee-saved registers.
  // Note that their cumulative size is small and they can be indexed using
  // 16-bit offsets.

  // TODO: increment/decrement SP in one step instead of two or remove this comment.

  uint32_t ofs = FrameEntrySpillSize();
  __ IncreaseFrameSize(ofs);

  for (int i = arraysize(kCoreCalleeSaves) - 1; i >= 0; --i) {
    Register reg = kCoreCalleeSaves[i];
    if (allocated_registers_.ContainsCoreRegister(reg)) {
      ofs -= kMipsWordSize;
      __ Sw(reg, SP, ofs);
      __ cfi().RelOffset(DWARFReg(reg), ofs);
    }
  }

  for (int i = arraysize(kFpuCalleeSaves) - 1; i >= 0; --i) {
    FRegister reg = kFpuCalleeSaves[i];
    if (allocated_registers_.ContainsFloatingPointRegister(reg)) {
      ofs -= kMipsWordSize;
      __ Swc1(reg, SP, ofs);
      // TODO: __ cfi().RelOffset(DWARFReg(reg), ofs);
    }
  }

  // Allocate the rest of the frame and store the current method pointer
  // at its end.

  __ IncreaseFrameSize(GetFrameSize() - FrameEntrySpillSize());

  static_assert(IsInt<16>(kCurrentMethodStackOffset),
                "kCurrentMethodStackOffset must fit into int16_t");
  __ Sw(kMethodRegisterArgument, SP, kCurrentMethodStackOffset);
}

void CodeGeneratorMIPS::GenerateFrameExit() {
  __ cfi().RememberState();

  // TODO: anything related to T9/GP/GOT/PIC/.so's?

  if (!HasEmptyFrame()) {
    // Deallocate the rest of the frame.

    __ DecreaseFrameSize(GetFrameSize() - FrameEntrySpillSize());

    // Restore callee-saved registers.
    // Note that their cumulative size is small and they can be indexed using
    // 16-bit offsets.

    // TODO: increment/decrement SP in one step instead of two or remove this comment.

    uint32_t ofs = 0;

    for (size_t i = 0; i < arraysize(kFpuCalleeSaves); ++i) {
      FRegister reg = kFpuCalleeSaves[i];
      if (allocated_registers_.ContainsFloatingPointRegister(reg)) {
        __ Lwc1(reg, SP, ofs);
        ofs += kMipsWordSize;
        // TODO: __ cfi().Restore(DWARFReg(reg));
      }
    }

    for (size_t i = 0; i < arraysize(kCoreCalleeSaves); ++i) {
      Register reg = kCoreCalleeSaves[i];
      if (allocated_registers_.ContainsCoreRegister(reg)) {
        __ Lw(reg, SP, ofs);
        ofs += kMipsWordSize;
        __ cfi().Restore(DWARFReg(reg));
      }
    }

    DCHECK_EQ(ofs, FrameEntrySpillSize());
    __ DecreaseFrameSize(ofs);
  }

  __ Jr(RA);

  __ cfi().RestoreState();
  __ cfi().DefCFAOffset(GetFrameSize());
}

void CodeGeneratorMIPS::Bind(HBasicBlock* block) {
  __ Bind(GetLabelOf(block), /* is_jump */ false);  // TODO: Check second arg.
}

void CodeGeneratorMIPS::Move32(Location destination, Location source) {
  if (source.Equals(destination)) {
    return;
  }
  if (destination.IsRegister()) {
    if (source.IsRegister()) {
      // Move to FP register from FP register.
      __ Move(destination.AsRegister<Register>(), source.AsRegister<Register>());
    } else if (source.IsFpuRegister()) {
      __ Mfc1(destination.AsRegister<Register>(), source.AsFpuRegister<FRegister>());
    } else {
      __ LoadFromOffset(kLoadWord, destination.AsRegister<Register>(), SP, source.GetStackIndex());
    }
  } else if (destination.IsFpuRegister()) {
    if (source.IsRegister()) {
      __ Mtc1(destination.AsFpuRegister<FRegister>(), source.AsRegister<Register>());
    } else if (source.IsFpuRegister()) {
      __ MovS(destination.AsFpuRegister<FRegister>(), source.AsFpuRegister<FRegister>());
    } else {
      __ LoadSFromOffset(destination.AsFpuRegister<FRegister>(), SP, source.GetStackIndex());
    }
  } else {
    DCHECK(destination.IsStackSlot()) << destination;
    if (source.IsRegister()) {
      __ StoreToOffset(kStoreWord, source.AsRegister<Register>(), SP, destination.GetStackIndex());
    } else if (source.IsFpuRegister()) {
      __ StoreSToOffset(source.AsFpuRegister<FRegister>(), SP, destination.GetStackIndex());
    } else {
      DCHECK(source.IsStackSlot()) << source;
      __ LoadFromOffset(kLoadWord, TMP, SP, source.GetStackIndex());
      __ StoreToOffset(kStoreWord, TMP, SP, destination.GetStackIndex());
    }
  }
}

void CodeGeneratorMIPS::Move64(Location destination, Location source) {
  if (source.Equals(destination)) {
    return;
  }
  if (destination.IsRegisterPair()) {
    if (source.IsRegisterPair()) {
      EmitParallelMoves(
          Location::RegisterLocation(source.AsRegisterPairHigh<Register>()),
          Location::RegisterLocation(destination.AsRegisterPairHigh<Register>()),
          Primitive::kPrimInt,
          Location::RegisterLocation(source.AsRegisterPairLow<Register>()),
          Location::RegisterLocation(destination.AsRegisterPairLow<Register>()),
          Primitive::kPrimInt);
    } else if (source.IsFpuRegister()) {
      UNIMPLEMENTED(FATAL);
    } else {
      DCHECK(source.IsDoubleStackSlot());
      DCHECK(ExpectedPairLayout(destination));
      __ LoadFromOffset(kLoadWord,
                        destination.AsRegisterPairLow<Register>(),
                        SP,
                        source.GetStackIndex());
      __ LoadFromOffset(kLoadWord,
                        destination.AsRegisterPairHigh<Register>(),
                        SP,
                        source.GetHighStackIndex(kMipsWordSize));
    }
  } else if (destination.IsFpuRegisterPair()) {
    if (source.IsDoubleStackSlot()) {
      __ LoadDFromOffset(FromLowSToD(destination.AsFpuRegisterPairLow<FRegister>()),
                         SP,
                         source.GetStackIndex());
    } else {
      UNIMPLEMENTED(FATAL);
    }
  } else {
    DCHECK(destination.IsDoubleStackSlot());
    if (source.IsRegisterPair()) {
      // No conflict possible, so just do the moves.
      __ StoreToOffset(kStoreWord,
                       source.AsRegisterPairLow<Register>(),
                       SP,
                       destination.GetStackIndex());
      __ StoreToOffset(kStoreWord,
                       source.AsRegisterPairHigh<Register>(),
                       SP,
                       destination.GetHighStackIndex(kMipsWordSize));
    } else if (source.IsFpuRegisterPair()) {
      __ StoreDToOffset(FromLowSToD(source.AsFpuRegisterPairLow<FRegister>()),
                        SP,
                        destination.GetStackIndex());
    } else {
      DCHECK(source.IsDoubleStackSlot());
      EmitParallelMoves(
          Location::StackSlot(source.GetStackIndex()),
          Location::StackSlot(destination.GetStackIndex()),
          Primitive::kPrimInt,
          Location::StackSlot(source.GetHighStackIndex(kMipsWordSize)),
          Location::StackSlot(destination.GetHighStackIndex(kMipsWordSize)),
          Primitive::kPrimInt);
    }
  }
}

void CodeGeneratorMIPS::Move(HInstruction* instruction, Location location, HInstruction* move_for) {
  LocationSummary* locations = instruction->GetLocations();
  Primitive::Type type = instruction->GetType();
  DCHECK_NE(type, Primitive::kPrimVoid);
  if (instruction->IsCurrentMethod()) {
    Move32(location, Location::StackSlot(kCurrentMethodStackOffset));
  } else if (locations != nullptr && locations->Out().Equals(location)) {
    return;
  } else if (locations != nullptr && locations->Out().IsConstant()) {
    HConstant* const_to_move = locations->Out().GetConstant();
    if (const_to_move->IsIntConstant() || const_to_move->IsNullConstant()) {
      int32_t value = GetInt32ValueOf(const_to_move);
      if (location.IsRegister()) {
        __ LoadImmediate(location.AsRegister<Register>(), value);
      } else {
        DCHECK(location.IsStackSlot());
        __ LoadImmediate(TMP, value);
        __ StoreToOffset(kStoreWord, TMP, SP, location.GetStackIndex());
      }
    } else {
      DCHECK(const_to_move->IsLongConstant()) << const_to_move->DebugName();
      int64_t value = const_to_move->AsLongConstant()->GetValue();
      if (location.IsRegisterPair()) {
        __ LoadImmediate(location.AsRegisterPairLow<Register>(), Low32Bits(value));
        __ LoadImmediate(location.AsRegisterPairHigh<Register>(), High32Bits(value));
      } else {
        DCHECK(location.IsDoubleStackSlot());
        __ LoadImmediate(TMP, Low32Bits(value));
        __ StoreToOffset(kStoreWord, TMP, SP, location.GetStackIndex());
        __ LoadImmediate(TMP, High32Bits(value));
        __ StoreToOffset(kStoreWord, TMP, SP, location.GetHighStackIndex(kMipsWordSize));
      }
    }
  } else if (instruction->IsLoadLocal()) {
    uint32_t stack_slot = GetStackSlot(instruction->AsLoadLocal()->GetLocal());
    switch (type) {
      case Primitive::kPrimBoolean:
      case Primitive::kPrimByte:
      case Primitive::kPrimChar:
      case Primitive::kPrimShort:
      case Primitive::kPrimInt:
      case Primitive::kPrimNot:
      case Primitive::kPrimFloat:
        Move32(location, Location::StackSlot(stack_slot));
        break;

      case Primitive::kPrimLong:
      case Primitive::kPrimDouble:
        Move64(location, Location::DoubleStackSlot(stack_slot));
        break;

      default:
        LOG(FATAL) << "Unexpected type " << type;
    }
  } else if (instruction->IsTemporary()) {
    Location temp_location = GetTemporaryLocation(instruction->AsTemporary());
    if (temp_location.IsStackSlot()) {
      Move32(location, temp_location);
    } else {
      DCHECK(temp_location.IsDoubleStackSlot());
      Move64(location, temp_location);
    }
  } else {
    DCHECK((instruction->GetNext() == move_for) || instruction->GetNext()->IsTemporary());
    switch (type) {
      case Primitive::kPrimBoolean:
      case Primitive::kPrimByte:
      case Primitive::kPrimChar:
      case Primitive::kPrimShort:
      case Primitive::kPrimNot:
      case Primitive::kPrimInt:
      case Primitive::kPrimFloat:
        Move32(location, locations->Out());
        break;

      case Primitive::kPrimLong:
      case Primitive::kPrimDouble:
        Move64(location, locations->Out());
        break;

      default:
        LOG(FATAL) << "Unexpected type " << type;
    }
  }
}

Location CodeGeneratorMIPS::GetStackLocation(HLoadLocal* load) const {
  Primitive::Type type = load->GetType();

  switch (type) {
    case Primitive::kPrimNot:
    case Primitive::kPrimInt:
    case Primitive::kPrimFloat:
      return Location::StackSlot(GetStackSlot(load->GetLocal()));

    case Primitive::kPrimLong:
    case Primitive::kPrimDouble:
      return Location::DoubleStackSlot(GetStackSlot(load->GetLocal()));

    case Primitive::kPrimBoolean:
    case Primitive::kPrimByte:
    case Primitive::kPrimChar:
    case Primitive::kPrimShort:
    case Primitive::kPrimVoid:
      LOG(FATAL) << "Unexpected type " << type;
  }

  LOG(FATAL) << "Unreachable";
  return Location::NoLocation();
}

void CodeGeneratorMIPS::MarkGCCard(Register object, Register value) {
  Label done;
  Register card = AT;
  Register temp = TMP;
  __ Beq(value, ZERO, &done);
  __ LoadFromOffset(kLoadWord, card, TR, Thread::CardTableOffset<kMipsWordSize>().Int32Value());
  __ Srl(temp, object, gc::accounting::CardTable::kCardShift);
  __ Addu(temp, card, temp);
  __ Sb(card, temp, 0);
  __ Bind(&done, /* is_jump */ false);  // TODO: Check second arg.
}

void CodeGeneratorMIPS::SetupBlockedRegisters(bool is_baseline ATTRIBUTE_UNUSED) const {
  // ZERO, K0, K1, GP, SP, RA are always reserved and can't be allocated.
  blocked_core_registers_[ZERO] = true;
  blocked_core_registers_[K0] = true;
  blocked_core_registers_[K1] = true;
  blocked_core_registers_[GP] = true;
  blocked_core_registers_[SP] = true;
  blocked_core_registers_[RA] = true;

  // AT and TMP (T8) are used as temporary/scratch registers
  // (similar to how AT is used by MIPS assemblers).
  blocked_core_registers_[AT] = true;
  blocked_core_registers_[TMP] = true;
  // FTMP (F8) and FTMP2 (F9) are also used as as temporary/scratch registers.
  blocked_fpu_registers_[FTMP] = true;
  blocked_fpu_registers_[FTMP2] = true;

  // TODO: Is that correct?
  // Block odd-numbered FP registers.
  blocked_fpu_registers_[F1] = true;
  blocked_fpu_registers_[F3] = true;
  blocked_fpu_registers_[F5] = true;
  blocked_fpu_registers_[F7] = true;
  // F9 (FTMP2) is already blocked above.
  blocked_fpu_registers_[F11] = true;
  blocked_fpu_registers_[F13] = true;
  blocked_fpu_registers_[F15] = true;
  blocked_fpu_registers_[F17] = true;
  blocked_fpu_registers_[F19] = true;
  blocked_fpu_registers_[F21] = true;
  blocked_fpu_registers_[F23] = true;
  blocked_fpu_registers_[F25] = true;
  blocked_fpu_registers_[F27] = true;
  blocked_fpu_registers_[F29] = true;
  blocked_fpu_registers_[F31] = true;

  // Reserve suspend and thread registers.
  blocked_core_registers_[S0] = true;
  blocked_core_registers_[TR] = true;

  // Reserve T9 for function calls
  blocked_core_registers_[T9] = true;

  // TODO: review; anything else?

  // TODO: make these two for's conditional on is_baseline once
  // all the issues with register saving/restoring are sorted out.
  for (size_t i = 0; i < arraysize(kCoreCalleeSaves); ++i) {
    blocked_core_registers_[kCoreCalleeSaves[i]] = true;
  }

  for (size_t i = 0; i < arraysize(kFpuCalleeSaves); ++i) {
    blocked_fpu_registers_[kFpuCalleeSaves[i]] = true;
  }
}

Location CodeGeneratorMIPS::AllocateFreeRegister(Primitive::Type type) const {
  if (type == Primitive::kPrimVoid) {
    LOG(FATAL) << "Unreachable type " << type;
  }

  if (Primitive::IsFloatingPointType(type)) {
    size_t reg = FindFreeEntry(blocked_fpu_registers_, kNumberOfFRegisters);
    return Location::FpuRegisterLocation(reg);
  } else {
    size_t reg = FindFreeEntry(blocked_core_registers_, kNumberOfCoreRegisters);
    return Location::RegisterLocation(reg);
  }
}

size_t CodeGeneratorMIPS::SaveCoreRegister(size_t stack_index, uint32_t reg_id) {
  __ StoreToOffset(kStoreWord, Register(reg_id), SP, stack_index);
  return kMipsWordSize;
}

size_t CodeGeneratorMIPS::RestoreCoreRegister(size_t stack_index, uint32_t reg_id) {
  __ LoadFromOffset(kLoadWord, Register(reg_id), SP, stack_index);
  return kMipsWordSize;
}

size_t CodeGeneratorMIPS::SaveFloatingPointRegister(size_t stack_index, uint32_t reg_id) {
  __ StoreSToOffset(FRegister(reg_id), SP, stack_index);
  return kMipsWordSize;
}

size_t CodeGeneratorMIPS::RestoreFloatingPointRegister(size_t stack_index, uint32_t reg_id) {
  __ LoadSFromOffset(FRegister(reg_id), SP, stack_index);
  return kMipsWordSize;
}

void CodeGeneratorMIPS::DumpCoreRegister(std::ostream& stream, int reg) const {
  stream << MipsManagedRegister::FromCoreRegister(Register(reg));
}

void CodeGeneratorMIPS::DumpFloatingPointRegister(std::ostream& stream, int reg) const {
  stream << MipsManagedRegister::FromFRegister(FRegister(reg));
}

void CodeGeneratorMIPS::InvokeRuntime(int32_t entry_point_offset,
                                      HInstruction* instruction,
                                      uint32_t dex_pc,
                                      SlowPathCode* slow_path) {
  ValidateInvokeRuntime(instruction, slow_path);
  // TODO: anything related to T9/GP/GOT/PIC/.so's?
  __ LoadFromOffset(kLoadWord, T9, TR, entry_point_offset);
  __ Jalr(T9);
  RecordPcInfo(instruction, dex_pc, slow_path);
}

void InstructionCodeGeneratorMIPS::GenerateClassInitializationCheck(SlowPathCodeMIPS* slow_path,
                                                                    Register class_reg) {
  __ LoadFromOffset(kLoadWord, TMP, class_reg, mirror::Class::StatusOffset().Int32Value());
  __ LoadImmediate(AT, mirror::Class::kStatusInitialized);
  __ BranchOnLowerThan(TMP, AT, slow_path->GetEntryLabel());
  // TODO: barrier needed?
  __ Bind(slow_path->GetExitLabel(), /* is_jump */ false);  // TODO: Check second arg.
}

void InstructionCodeGeneratorMIPS::GenerateMemoryBarrier(MemBarrierKind kind ATTRIBUTE_UNUSED) {
  __ Sync(0);  // only stype 0 is supported
}

void InstructionCodeGeneratorMIPS::GenerateSuspendCheck(HSuspendCheck* instruction,
                                                        HBasicBlock* successor) {
  SuspendCheckSlowPathMIPS* slow_path =
    new (GetGraph()->GetArena()) SuspendCheckSlowPathMIPS(instruction, successor);
  codegen_->AddSlowPath(slow_path);

  __ LoadFromOffset(kLoadUnsignedHalfword,
                    TMP,
                    TR,
                    Thread::ThreadFlagsOffset<kMipsWordSize>().Int32Value());
  if (successor == nullptr) {
    __ Bne(TMP, ZERO, slow_path->GetEntryLabel());
    __ Bind(slow_path->GetReturnLabel(), /* is_jump */ false);  // TODO: Check second arg.
  } else {
    __ Beq(TMP, ZERO, codegen_->GetLabelOf(successor));
    __ J(slow_path->GetEntryLabel());
    // slow_path will return to GetLabelOf(successor).
  }
}

InstructionCodeGeneratorMIPS::InstructionCodeGeneratorMIPS(HGraph* graph,
                                                           CodeGeneratorMIPS* codegen)
      : HGraphVisitor(graph),
        assembler_(codegen->GetAssembler()),
        codegen_(codegen) {}

void LocationsBuilderMIPS::HandleBitwiseOperation(HBinaryOperation* instruction) {
  DCHECK(instruction->IsAnd() || instruction->IsOr() || instruction->IsXor());
  LocationSummary* locations = new (GetGraph()->GetArena()) LocationSummary(instruction);
  locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
  Primitive::Type type = instruction->GetResultType();
  switch (type) {
    case Primitive::kPrimInt: {
      locations->SetInAt(0, Location::RequiresRegister());
      HInstruction* right = instruction->InputAt(1);
      if (right->IsIntConstant() && IsUint<16>(right->AsIntConstant()->GetValue())) {
        locations->SetInAt(1, Location::ConstantLocation(right->AsConstant()));
      } else {
        locations->SetInAt(1, Location::RequiresRegister());
      }
      break;
    }

    case Primitive::kPrimLong: {
      locations->SetInAt(0, Location::RequiresRegister());
      // TODO: Handle 64-bit constant RHS better.
      locations->SetInAt(1, Location::RequiresRegister());
      break;
    }

    default:
      LOG(FATAL) << "Unexpected " << instruction->DebugName() << " type " << type;
  }
}

void InstructionCodeGeneratorMIPS::HandleBitwiseOperation(HBinaryOperation* instruction) {
  DCHECK(instruction->IsAnd() || instruction->IsOr() || instruction->IsXor());
  Primitive::Type type = instruction->GetType();
  LocationSummary* locations = instruction->GetLocations();

  switch (type) {
    case Primitive::kPrimInt: {
      Register out = locations->Out().AsRegister<Register>();
      Register lhs = locations->InAt(0).AsRegister<Register>();
      Location rhs_location = locations->InAt(1);

      Register rhs_reg = ZERO;
      int64_t rhs_imm = 0;
      bool use_imm = rhs_location.IsConstant();
      if (use_imm) {
        rhs_imm = CodeGenerator::GetInt64ValueOf(rhs_location.GetConstant());
      } else {
        rhs_reg = rhs_location.AsRegister<Register>();
      }

      if (instruction->IsAnd()) {
        if (use_imm)
          __ Andi(out, lhs, rhs_imm);
        else
          __ And(out, lhs, rhs_reg);
      } else if (instruction->IsOr()) {
        if (use_imm)
          __ Ori(out, lhs, rhs_imm);
        else
          __ Or(out, lhs, rhs_reg);
      } else {
        if (use_imm)
          __ Xori(out, lhs, rhs_imm);
        else
          __ Xor(out, lhs, rhs_reg);
      }
     break;
    }

    case Primitive::kPrimLong: {
      Register out_low = locations->Out().AsRegisterPairLow<Register>();
      Register out_high = locations->Out().AsRegisterPairHigh<Register>();
      Register lhs_low = locations->InAt(0).AsRegisterPairLow<Register>();
      Register lhs_high = locations->InAt(0).AsRegisterPairHigh<Register>();
      Register rhs_low = locations->InAt(1).AsRegisterPairLow<Register>();
      Register rhs_high = locations->InAt(1).AsRegisterPairHigh<Register>();
      if (instruction->IsAnd()) {
        __ And(out_low, lhs_low, rhs_low);
        __ And(out_high, lhs_high, rhs_high);
      } else if (instruction->IsOr()) {
        __ Or(out_low, lhs_low, rhs_low);
        __ Or(out_high, lhs_high, rhs_high);
      } else {
        DCHECK(instruction->IsXor());
        __ Xor(out_low, lhs_low, rhs_low);
        __ Xor(out_high, lhs_high, rhs_high);
      }
      break;
    }

    default:
      LOG(FATAL) << "Unexpected " << instruction->DebugName() << " type " << type;
  }
}

void LocationsBuilderMIPS::HandleShift(HBinaryOperation* instr) {
  DCHECK(instr->IsShl() || instr->IsShr() || instr->IsUShr());

  Primitive::Type type = instr->GetResultType();
  LocationSummary::CallKind call_kind = (type == Primitive::kPrimLong)
      ? LocationSummary::kCall
      : LocationSummary::kNoCall;
  LocationSummary* locations = new (GetGraph()->GetArena()) LocationSummary(instr, call_kind);

  switch (type) {
    case Primitive::kPrimInt:
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetInAt(1, Location::RegisterOrConstant(instr->InputAt(1)));
      locations->SetOut(Location::RequiresRegister());
      break;

    case Primitive::kPrimLong: {
      InvokeRuntimeCallingConvention calling_convention;
      locations->SetInAt(0, Location::RegisterPairLocation(calling_convention.GetRegisterAt(0),
                                                           calling_convention.GetRegisterAt(1)));
      locations->SetInAt(1, Location::RegisterLocation(calling_convention.GetRegisterAt(2)));
      locations->SetOut(calling_convention.GetReturnLocation(type));
      break;
    }

    default:
      LOG(FATAL) << "Unexpected shift type " << type;
  }
}

void InstructionCodeGeneratorMIPS::HandleShift(HBinaryOperation* instr) {
  DCHECK(instr->IsShl() || instr->IsShr() || instr->IsUShr());
  LocationSummary* locations = instr->GetLocations();
  Primitive::Type type = instr->GetType();

  switch (type) {
    case Primitive::kPrimInt: {
      Register dst = locations->Out().AsRegister<Register>();
      Register lhs = locations->InAt(0).AsRegister<Register>();
      Location rhs_location = locations->InAt(1);

      Register rhs_reg = ZERO;
      int32_t rhs_imm = 0;
      bool use_imm = rhs_location.IsConstant();
      if (use_imm) {
        rhs_imm = rhs_location.GetConstant()->AsIntConstant()->GetValue();
      } else {
        rhs_reg = rhs_location.AsRegister<Register>();
      }

      if (use_imm) {
        uint32_t shift_value = static_cast<uint32_t>(rhs_imm & kMaxIntShiftValue);
          if (instr->IsShl()) {
            __ Sll(dst, lhs, shift_value);
          } else if (instr->IsShr()) {
            __ Sra(dst, lhs, shift_value);
          } else {
            __ Srl(dst, lhs, shift_value);
          }
      } else {
        if (instr->IsShl()) {
          __ Sllv(dst, lhs, rhs_reg);
        } else if (instr->IsShr()) {
          __ Srav(dst, lhs, rhs_reg);
        } else {
          __ Srlv(dst, lhs, rhs_reg);
        }
      }
      break;
    }

    case Primitive::kPrimLong: {
      DCHECK(locations->Out().IsRegisterPair());
      DCHECK(locations->InAt(0).IsRegisterPair());
      DCHECK(locations->InAt(1).IsRegister());
      // TODO: Implement shl-long, shr-long and ushr-long witout
      // resorting to the runtime.  (Do not forget to update
      // art::CodeGenerator::RecordPcInfo when it is done).
      int32_t entry_point_offset = instr->IsShl()
          ? QUICK_ENTRY_POINT(pShlLong)
          : (instr->IsShr()
             ? QUICK_ENTRY_POINT(pShrLong)
             : QUICK_ENTRY_POINT(pUshrLong));
      codegen_->InvokeRuntime(entry_point_offset, instr, instr->GetDexPc(), nullptr);
      if (instr->IsShl()) {
        CheckEntrypointTypes<kQuickShlLong, uint64_t, uint64_t, uint32_t>();
      } else if (instr->IsShr()) {
        CheckEntrypointTypes<kQuickShrLong, uint64_t, uint64_t, uint32_t>();
      } else {
        CheckEntrypointTypes<kQuickUshrLong, uint64_t, uint64_t, uint32_t>();
      }
      break;
    }

    default:
      LOG(FATAL) << "Unexpected shift operation type " << type;
  }
}

void LocationsBuilderMIPS::VisitAdd(HAdd* add) {
  LocationSummary* locations =
      new (GetGraph()->GetArena()) LocationSummary(add, LocationSummary::kNoCall);
  switch (add->GetResultType()) {
    case Primitive::kPrimInt: {
      locations->SetInAt(0, Location::RequiresRegister());
      HInstruction* rhs = add->InputAt(1);
      if (rhs->IsIntConstant() && IsInt<16>(rhs->AsIntConstant()->GetValue())) {
        locations->SetInAt(1, Location::ConstantLocation(rhs->AsConstant()));
      } else {
        locations->SetInAt(1, Location::RequiresRegister());
      }
      locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
      break;
    }

    case Primitive::kPrimLong: {
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetInAt(1, Location::RequiresRegister());
      locations->SetOut(Location::RequiresRegister(), Location::kOutputOverlap);
      break;
    }

    case Primitive::kPrimFloat:
    case Primitive::kPrimDouble: {
      locations->SetInAt(0, Location::RequiresFpuRegister());
      locations->SetInAt(1, Location::RequiresFpuRegister());
      locations->SetOut(Location::RequiresFpuRegister(), Location::kNoOutputOverlap);
      break;
    }

    default:
      LOG(FATAL) << "Unexpected add type " << add->GetResultType();
  }
}

void InstructionCodeGeneratorMIPS::VisitAdd(HAdd* add) {
  LocationSummary* locations = add->GetLocations();
  Location out = locations->Out();
  Location lhs = locations->InAt(0);
  Location rhs = locations->InAt(1);
  switch (add->GetResultType()) {
    case Primitive::kPrimInt:
      if (rhs.IsRegister()) {
        __ Addu(out.AsRegister<Register>(),
                lhs.AsRegister<Register>(),
                rhs.AsRegister<Register>());
      } else {
        __ Addiu(out.AsRegister<Register>(),
                 lhs.AsRegister<Register>(),
                 rhs.GetConstant()->AsIntConstant()->GetValue());
      }
      break;

    case Primitive::kPrimLong: {
      DCHECK(rhs.IsRegisterPair());
      Register out_low = out.AsRegisterPairLow<Register>();
      Register out_high = out.AsRegisterPairHigh<Register>();
      Register lhs_low = lhs.AsRegisterPairLow<Register>();
      Register lhs_high = lhs.AsRegisterPairHigh<Register>();
      Register rhs_low = rhs.AsRegisterPairLow<Register>();
      Register rhs_high = rhs.AsRegisterPairHigh<Register>();
      __ Addu(out_low, lhs_low, rhs_low);
      __ Addu(out_high, lhs_high, rhs_high);
      // Carry.
      __ Sltu(TMP, out_low, rhs_low);
      __ Addu(out_high, out_high, TMP);
      break;
    }

    case Primitive::kPrimFloat:
      __ AddS(out.AsFpuRegister<FRegister>(),
              lhs.AsFpuRegister<FRegister>(),
              rhs.AsFpuRegister<FRegister>());
      break;

    case Primitive::kPrimDouble:
      __ AddD(FromLowSToD(out.AsFpuRegisterPairLow<FRegister>()),
              FromLowSToD(lhs.AsFpuRegisterPairLow<FRegister>()),
              FromLowSToD(rhs.AsFpuRegisterPairLow<FRegister>()));
      break;

    default:
      LOG(FATAL) << "Unexpected add type " << add->GetResultType();
  }
}

void LocationsBuilderMIPS::VisitAnd(HAnd* instruction) {
  HandleBitwiseOperation(instruction);
}

void InstructionCodeGeneratorMIPS::VisitAnd(HAnd* instruction) {
  HandleBitwiseOperation(instruction);
}

void LocationsBuilderMIPS::VisitArrayGet(HArrayGet* instruction) {
  LocationSummary* locations =
      new (GetGraph()->GetArena()) LocationSummary(instruction, LocationSummary::kNoCall);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RegisterOrConstant(instruction->InputAt(1)));
  if (Primitive::IsFloatingPointType(instruction->GetType())) {
    locations->SetOut(Location::RequiresFpuRegister(), Location::kNoOutputOverlap);
  } else {
    locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
  }
}

void InstructionCodeGeneratorMIPS::VisitArrayGet(HArrayGet* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  Register obj = locations->InAt(0).AsRegister<Register>();
  Location index = locations->InAt(1);
  Primitive::Type type = instruction->GetType();

  switch (type) {
    case Primitive::kPrimBoolean: {
      uint32_t data_offset = mirror::Array::DataOffset(sizeof(uint8_t)).Uint32Value();
      Register out = locations->Out().AsRegister<Register>();
      if (index.IsConstant()) {
        size_t offset =
            (index.GetConstant()->AsIntConstant()->GetValue() << TIMES_1) + data_offset;
        __ LoadFromOffset(kLoadUnsignedByte, out, obj, offset);
      } else {
        __ Addu(TMP, obj, index.AsRegister<Register>());
        __ LoadFromOffset(kLoadUnsignedByte, out, TMP, data_offset);
      }
      break;
    }

    case Primitive::kPrimByte: {
      uint32_t data_offset = mirror::Array::DataOffset(sizeof(int8_t)).Uint32Value();
      Register out = locations->Out().AsRegister<Register>();
      if (index.IsConstant()) {
        size_t offset =
            (index.GetConstant()->AsIntConstant()->GetValue() << TIMES_1) + data_offset;
        __ LoadFromOffset(kLoadSignedByte, out, obj, offset);
      } else {
        __ Addu(TMP, obj, index.AsRegister<Register>());
        __ LoadFromOffset(kLoadSignedByte, out, TMP, data_offset);
      }
      break;
    }

    case Primitive::kPrimShort: {
      uint32_t data_offset = mirror::Array::DataOffset(sizeof(int16_t)).Uint32Value();
      Register out = locations->Out().AsRegister<Register>();
      if (index.IsConstant()) {
        size_t offset =
            (index.GetConstant()->AsIntConstant()->GetValue() << TIMES_2) + data_offset;
        __ LoadFromOffset(kLoadSignedHalfword, out, obj, offset);
      } else {
        __ Sll(TMP, index.AsRegister<Register>(), TIMES_2);
        __ Addu(TMP, obj, TMP);
        __ LoadFromOffset(kLoadSignedHalfword, out, TMP, data_offset);
      }
      break;
    }

    case Primitive::kPrimChar: {
      uint32_t data_offset = mirror::Array::DataOffset(sizeof(uint16_t)).Uint32Value();
      Register out = locations->Out().AsRegister<Register>();
      if (index.IsConstant()) {
        size_t offset =
            (index.GetConstant()->AsIntConstant()->GetValue() << TIMES_2) + data_offset;
        __ LoadFromOffset(kLoadUnsignedHalfword, out, obj, offset);
      } else {
        __ Sll(TMP, index.AsRegister<Register>(), TIMES_2);
        __ Addu(TMP, obj, TMP);
        __ LoadFromOffset(kLoadUnsignedHalfword, out, TMP, data_offset);
      }
      break;
    }

    case Primitive::kPrimInt:
    case Primitive::kPrimNot: {
      DCHECK_EQ(sizeof(mirror::HeapReference<mirror::Object>), sizeof(int32_t));
      uint32_t data_offset = mirror::Array::DataOffset(sizeof(int32_t)).Uint32Value();
      Register out = locations->Out().AsRegister<Register>();
      if (index.IsConstant()) {
        size_t offset =
            (index.GetConstant()->AsIntConstant()->GetValue() << TIMES_4) + data_offset;
        __ LoadFromOffset(kLoadWord, out, obj, offset);
      } else {
        __ Sll(TMP, index.AsRegister<Register>(), TIMES_4);
        __ Addu(TMP, obj, TMP);
        __ LoadFromOffset(kLoadWord, out, TMP, data_offset);
      }
      break;
    }

    case Primitive::kPrimLong: {
      uint32_t data_offset = mirror::Array::DataOffset(sizeof(int64_t)).Uint32Value();
      Register out_low = locations->Out().AsRegisterPairLow<Register>();
      Register out_high = locations->Out().AsRegisterPairHigh<Register>();
      if (index.IsConstant()) {
        size_t offset =
            (index.GetConstant()->AsIntConstant()->GetValue() << TIMES_8) + data_offset;
        __ LoadFromOffset(kLoadWord, out_low, obj, offset);
        __ LoadFromOffset(kLoadWord, out_high, obj, offset + kMipsWordSize);
      } else {
        __ Sll(TMP, index.AsRegister<Register>(), TIMES_8);
        __ Addu(TMP, obj, TMP);
        __ LoadFromOffset(kLoadWord, out_low, TMP, data_offset);
        __ LoadFromOffset(kLoadWord, out_high, TMP, data_offset + kMipsWordSize);
      }
      break;
    }

    case Primitive::kPrimFloat: {
      uint32_t data_offset = mirror::Array::DataOffset(sizeof(float)).Uint32Value();
      FRegister out = locations->Out().AsFpuRegister<FRegister>();
      if (index.IsConstant()) {
        size_t offset =
            (index.GetConstant()->AsIntConstant()->GetValue() << TIMES_4) + data_offset;
        __ LoadSFromOffset(out, obj, offset);
      } else {
        __ Sll(TMP, index.AsRegister<Register>(), TIMES_4);
        __ Addu(TMP, obj, TMP);
        __ LoadSFromOffset(out, TMP, data_offset);
      }
      break;
    }

    case Primitive::kPrimDouble: {
      uint32_t data_offset = mirror::Array::DataOffset(sizeof(double)).Uint32Value();
      DRegister out = FromLowSToD(locations->Out().AsFpuRegisterPairLow<FRegister>());
      if (index.IsConstant()) {
        size_t offset =
            (index.GetConstant()->AsIntConstant()->GetValue() << TIMES_8) + data_offset;
        __ LoadDFromOffset(out, obj, offset);
      } else {
        __ Sll(TMP, index.AsRegister<Register>(), TIMES_8);
        __ Addu(TMP, obj, TMP);
        __ LoadDFromOffset(out, TMP, data_offset);
      }
      break;
    }

    case Primitive::kPrimVoid:
      LOG(FATAL) << "Unreachable type " << instruction->GetType();
      UNREACHABLE();
  }
  codegen_->MaybeRecordImplicitNullCheck(instruction);
}

void LocationsBuilderMIPS::VisitArrayLength(HArrayLength* instruction) {
  LocationSummary* locations = new (GetGraph()->GetArena()) LocationSummary(instruction);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
}

void InstructionCodeGeneratorMIPS::VisitArrayLength(HArrayLength* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  uint32_t offset = mirror::Array::LengthOffset().Uint32Value();
  Register obj = locations->InAt(0).AsRegister<Register>();
  Register out = locations->Out().AsRegister<Register>();
  __ LoadFromOffset(kLoadWord, out, obj, offset);
  codegen_->MaybeRecordImplicitNullCheck(instruction);
}

void LocationsBuilderMIPS::VisitArraySet(HArraySet* instruction) {
  Primitive::Type value_type = instruction->GetComponentType();
  bool is_object = value_type == Primitive::kPrimNot;
  LocationSummary* locations = new (GetGraph()->GetArena()) LocationSummary(
      instruction,
      is_object ? LocationSummary::kCall : LocationSummary::kNoCall);
  if (is_object) {
    InvokeRuntimeCallingConvention calling_convention;
    locations->SetInAt(0, Location::RegisterLocation(calling_convention.GetRegisterAt(0)));
    locations->SetInAt(1, Location::RegisterLocation(calling_convention.GetRegisterAt(1)));
    locations->SetInAt(2, Location::RegisterLocation(calling_convention.GetRegisterAt(2)));
  } else {
    locations->SetInAt(0, Location::RequiresRegister());
    locations->SetInAt(1, Location::RegisterOrConstant(instruction->InputAt(1)));
    if (Primitive::IsFloatingPointType(instruction->InputAt(2)->GetType())) {
      locations->SetInAt(2, Location::RequiresFpuRegister());
    } else {
      locations->SetInAt(2, Location::RequiresRegister());
    }
  }
}

void InstructionCodeGeneratorMIPS::VisitArraySet(HArraySet* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  Register obj = locations->InAt(0).AsRegister<Register>();
  Location index = locations->InAt(1);
  Primitive::Type value_type = instruction->GetComponentType();
  bool needs_runtime_call = locations->WillCall();
  bool needs_write_barrier =
      CodeGenerator::StoreNeedsWriteBarrier(value_type, instruction->GetValue());

  switch (value_type) {
    case Primitive::kPrimBoolean:
    case Primitive::kPrimByte: {
      uint32_t data_offset = mirror::Array::DataOffset(sizeof(uint8_t)).Uint32Value();
      Register value = locations->InAt(2).AsRegister<Register>();
      if (index.IsConstant()) {
        size_t offset =
            (index.GetConstant()->AsIntConstant()->GetValue() << TIMES_1) + data_offset;
        __ StoreToOffset(kStoreByte, value, obj, offset);
      } else {
        __ Addu(TMP, obj, index.AsRegister<Register>());
        __ StoreToOffset(kStoreByte, value, TMP, data_offset);
      }
      break;
    }

    case Primitive::kPrimShort:
    case Primitive::kPrimChar: {
      uint32_t data_offset = mirror::Array::DataOffset(sizeof(uint16_t)).Uint32Value();
      Register value = locations->InAt(2).AsRegister<Register>();
      if (index.IsConstant()) {
        size_t offset =
            (index.GetConstant()->AsIntConstant()->GetValue() << TIMES_2) + data_offset;
        __ StoreToOffset(kStoreHalfword, value, obj, offset);
      } else {
        __ Sll(TMP, index.AsRegister<Register>(), TIMES_2);
        __ Addu(TMP, obj, TMP);
        __ StoreToOffset(kStoreHalfword, value, TMP, data_offset);
      }
      break;
    }

    case Primitive::kPrimInt:
    case Primitive::kPrimNot: {
      if (!needs_runtime_call) {
        uint32_t data_offset = mirror::Array::DataOffset(sizeof(int32_t)).Uint32Value();
        Register value = locations->InAt(2).AsRegister<Register>();
        if (index.IsConstant()) {
          size_t offset =
              (index.GetConstant()->AsIntConstant()->GetValue() << TIMES_4) + data_offset;
          __ StoreToOffset(kStoreWord, value, obj, offset);
        } else {
          DCHECK(index.IsRegister()) << index;
          __ Sll(TMP, index.AsRegister<Register>(), TIMES_4);
          __ Addu(TMP, obj, TMP);
          __ StoreToOffset(kStoreWord, value, TMP, data_offset);
        }
        codegen_->MaybeRecordImplicitNullCheck(instruction);
        if (needs_write_barrier) {
          DCHECK_EQ(value_type, Primitive::kPrimNot);
          codegen_->MarkGCCard(obj, value);
        }
      } else {
        DCHECK_EQ(value_type, Primitive::kPrimNot);
        codegen_->InvokeRuntime(QUICK_ENTRY_POINT(pAputObject),
                                instruction,
                                instruction->GetDexPc(),
                                nullptr);
        // TODO: Add CheckEntrypointTypes.
      }
      break;
    }

    case Primitive::kPrimLong: {
      uint32_t data_offset = mirror::Array::DataOffset(sizeof(int64_t)).Uint32Value();
      Register value_low = locations->InAt(2).AsRegisterPairLow<Register>();
      Register value_high = locations->InAt(2).AsRegisterPairHigh<Register>();
      if (index.IsConstant()) {
        size_t offset =
            (index.GetConstant()->AsIntConstant()->GetValue() << TIMES_8) + data_offset;
        __ StoreToOffset(kStoreWord, value_low, obj, offset);
        __ StoreToOffset(kStoreWord, value_high, obj, offset + kMipsWordSize);
      } else {
        __ Sll(TMP, index.AsRegister<Register>(), TIMES_8);
        __ Addu(TMP, obj, TMP);
        __ StoreToOffset(kStoreWord, value_low, TMP, data_offset);
        __ StoreToOffset(kStoreWord, value_high, TMP, data_offset + kMipsWordSize);
      }
      break;
    }

    case Primitive::kPrimFloat: {
      uint32_t data_offset = mirror::Array::DataOffset(sizeof(float)).Uint32Value();
      FRegister value = locations->InAt(2).AsFpuRegister<FRegister>();
      DCHECK(locations->InAt(2).IsFpuRegister());
      if (index.IsConstant()) {
        size_t offset =
            (index.GetConstant()->AsIntConstant()->GetValue() << TIMES_4) + data_offset;
        __ StoreSToOffset(value, obj, offset);
      } else {
        __ Sll(TMP, index.AsRegister<Register>(), TIMES_4);
        __ Addu(TMP, obj, TMP);
        __ StoreSToOffset(value, TMP, data_offset);
      }
      break;
    }

    case Primitive::kPrimDouble: {
      uint32_t data_offset = mirror::Array::DataOffset(sizeof(double)).Uint32Value();
      DRegister value = FromLowSToD(locations->InAt(2).AsFpuRegisterPairLow<FRegister>());
      DCHECK(locations->InAt(2).IsFpuRegisterPair());
      if (index.IsConstant()) {
        size_t offset =
            (index.GetConstant()->AsIntConstant()->GetValue() << TIMES_8) + data_offset;
        __ StoreDToOffset(value, obj, offset);
      } else {
        __ Sll(TMP, index.AsRegister<Register>(), TIMES_8);
        __ Addu(TMP, obj, TMP);
        __ StoreDToOffset(value, TMP, data_offset);
      }
      break;
    }

    case Primitive::kPrimVoid:
      LOG(FATAL) << "Unreachable type " << instruction->GetType();
      UNREACHABLE();
  }

  // Ints and objects are handled in the switch.
  if (value_type != Primitive::kPrimInt && value_type != Primitive::kPrimNot) {
    codegen_->MaybeRecordImplicitNullCheck(instruction);
  }
}

void LocationsBuilderMIPS::VisitBoundsCheck(HBoundsCheck* instruction) {
  LocationSummary* locations =
      new (GetGraph()->GetArena()) LocationSummary(instruction, LocationSummary::kNoCall);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RequiresRegister());
  if (instruction->HasUses()) {
    locations->SetOut(Location::SameAsFirstInput());
  }
}

void InstructionCodeGeneratorMIPS::VisitBoundsCheck(HBoundsCheck* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  BoundsCheckSlowPathMIPS* slow_path = new (GetGraph()->GetArena()) BoundsCheckSlowPathMIPS(
      instruction,
      locations->InAt(0),
      locations->InAt(1));
  codegen_->AddSlowPath(slow_path);

  Register index = locations->InAt(0).AsRegister<Register>();
  Register length = locations->InAt(1).AsRegister<Register>();

  // length is limited by the maximum positive signed 32-bit integer.
  // Unsigned comparison of length and index checks for index < 0
  // and for length <= index simultaneously.
  // Mips R6 requires lhs != rhs for compact branches.
  if (index == length) {
    __ J(slow_path->GetEntryLabel());
  } else {
    __ BranchOnGreaterThanOrEqualUnsigned(index, length, slow_path->GetEntryLabel());
  }
}

void LocationsBuilderMIPS::VisitCheckCast(HCheckCast* instruction) {
  LocationSummary* locations = new (GetGraph()->GetArena()) LocationSummary(
      instruction,
      LocationSummary::kCallOnSlowPath);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RequiresRegister());
  locations->AddTemp(Location::RequiresRegister());
}

void InstructionCodeGeneratorMIPS::VisitCheckCast(HCheckCast* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  Register obj = locations->InAt(0).AsRegister<Register>();
  Register cls = locations->InAt(1).AsRegister<Register>();
  Register obj_cls = locations->GetTemp(0).AsRegister<Register>();

  SlowPathCodeMIPS* slow_path = new (GetGraph()->GetArena()) TypeCheckSlowPathMIPS(
      instruction,
      locations->InAt(1),
      Location::RegisterLocation(obj_cls),
      instruction->GetDexPc());
  codegen_->AddSlowPath(slow_path);

  // TODO: avoid this check if we know obj is not null.
  __ Beq(obj, ZERO, slow_path->GetExitLabel());
  // Compare the class of `obj` with `cls`.
  __ LoadFromOffset(kLoadWord, obj_cls, obj, mirror::Object::ClassOffset().Int32Value());
  __ Bne(obj_cls, cls, slow_path->GetEntryLabel());
  __ Bind(slow_path->GetExitLabel(), /* is_jump */ false);  // TODO: Check second arg.
}

void LocationsBuilderMIPS::VisitClinitCheck(HClinitCheck* check) {
  LocationSummary* locations =
      new (GetGraph()->GetArena()) LocationSummary(check, LocationSummary::kCallOnSlowPath);
  locations->SetInAt(0, Location::RequiresRegister());
  if (check->HasUses()) {
    locations->SetOut(Location::SameAsFirstInput());
  }
}

void InstructionCodeGeneratorMIPS::VisitClinitCheck(HClinitCheck* check) {
  // We assume the class is not null.
  SlowPathCodeMIPS* slow_path = new (GetGraph()->GetArena()) LoadClassSlowPathMIPS(
      check->GetLoadClass(),
      check,
      check->GetDexPc(),
      true);
  codegen_->AddSlowPath(slow_path);
  GenerateClassInitializationCheck(slow_path,
                                   check->GetLocations()->InAt(0).AsRegister<Register>());
}

void LocationsBuilderMIPS::VisitCompare(HCompare* compare) {
  Primitive::Type in_type = compare->InputAt(0)->GetType();

  LocationSummary::CallKind call_kind = Primitive::IsFloatingPointType(in_type)
      ? LocationSummary::kCall
      : LocationSummary::kNoCall;

  LocationSummary* locations = new (GetGraph()->GetArena()) LocationSummary(compare, call_kind);

  switch (in_type) {
    case Primitive::kPrimLong:
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetInAt(1, Location::RequiresRegister());
      locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
      break;

    case Primitive::kPrimFloat: {
      InvokeRuntimeCallingConvention calling_convention;
      locations->SetInAt(0, Location::FpuRegisterLocation(calling_convention.GetFpuRegisterAt(0)));
      locations->SetInAt(1, Location::FpuRegisterLocation(calling_convention.GetFpuRegisterAt(1)));
      locations->SetOut(calling_convention.GetReturnLocation(Primitive::kPrimInt));
      break;
    }

    case Primitive::kPrimDouble: {
      InvokeRuntimeCallingConvention calling_convention;
      locations->SetInAt(0, Location::FpuRegisterPairLocation(
          calling_convention.GetFpuRegisterAt(0),
          calling_convention.GetFpuRegisterAt(1)));
      locations->SetInAt(1, Location::FpuRegisterPairLocation(
          calling_convention.GetFpuRegisterAt(2),
          calling_convention.GetFpuRegisterAt(3)));
      locations->SetOut(calling_convention.GetReturnLocation(Primitive::kPrimInt));
      break;
    }

    default:
      LOG(FATAL) << "Unexpected type for compare operation " << in_type;
  }
}

void InstructionCodeGeneratorMIPS::VisitCompare(HCompare* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  Primitive::Type in_type = instruction->InputAt(0)->GetType();

  //  0 if: left == right
  //  1 if: left  > right
  // -1 if: left  < right
  switch (in_type) {
    case Primitive::kPrimLong: {
      Label done;
      Register out = locations->Out().AsRegister<Register>();
      Register lhs_low = locations->InAt(0).AsRegisterPairLow<Register>();
      Register lhs_high = locations->InAt(0).AsRegisterPairHigh<Register>();
      Register rhs_low = locations->InAt(1).AsRegisterPairLow<Register>();
      Register rhs_high = locations->InAt(1).AsRegisterPairHigh<Register>();
      Register tmp1 = TMP;
      Register tmp2 = AT;
      __ Slt(tmp1, lhs_high, rhs_high);
      __ Slt(tmp2, rhs_high, lhs_high);
      __ Subu(out, tmp1, tmp2);
      __ Bne(out, ZERO, &done);
      __ Sltu(tmp1, lhs_low, rhs_low);
      __ Sltu(tmp2, rhs_low, lhs_low);
      __ Subu(out, tmp1, tmp2);
      __ Bind(&done, /* is_jump */ false);  // TODO: Check second arg.
      break;
    }

    case Primitive::kPrimFloat:
    case Primitive::kPrimDouble: {
      int32_t entry_point_offset;
      if (in_type == Primitive::kPrimFloat) {
        entry_point_offset = instruction->IsGtBias() ? QUICK_ENTRY_POINT(pCmpgFloat)
                                                     : QUICK_ENTRY_POINT(pCmplFloat);
      } else {
        entry_point_offset = instruction->IsGtBias() ? QUICK_ENTRY_POINT(pCmpgDouble)
                                                     : QUICK_ENTRY_POINT(pCmplDouble);
      }
      codegen_->InvokeRuntime(entry_point_offset, instruction, instruction->GetDexPc(), nullptr);
      // TODO: Add CheckEntrypointTypes.
      break;
    }

    default:
      LOG(FATAL) << "Unimplemented compare type " << in_type;
  }
}

void LocationsBuilderMIPS::VisitCondition(HCondition* instruction) {
  LocationSummary* locations = new (GetGraph()->GetArena()) LocationSummary(instruction);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RegisterOrConstant(instruction->InputAt(1)));
  if (instruction->NeedsMaterialization()) {
    locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
  }
}

void InstructionCodeGeneratorMIPS::VisitCondition(HCondition* instruction) {
  if (!instruction->NeedsMaterialization()) {
    return;
  }

  LocationSummary* locations = instruction->GetLocations();

  Register dst = locations->Out().AsRegister<Register>();
  Register lhs = locations->InAt(0).AsRegister<Register>();
  Location rhs_location = locations->InAt(1);

  Register rhs_reg = ZERO;
  int64_t rhs_imm = 0;
  bool use_imm = rhs_location.IsConstant();
  if (use_imm) {
    rhs_imm = CodeGenerator::GetInt32ValueOf(rhs_location.GetConstant());
  } else {
    rhs_reg = rhs_location.AsRegister<Register>();
  }

  IfCondition if_cond = instruction->GetCondition();

  switch (if_cond) {
    case kCondEQ:
    case kCondNE:
      if (use_imm && IsUint<16>(rhs_imm)) {
        __ Xori(dst, lhs, rhs_imm);
      } else {
        if (use_imm) {
          rhs_reg = TMP;
          __ LoadImmediate(rhs_reg, rhs_imm);
        }
        __ Xor(dst, lhs, rhs_reg);
      }
      if (if_cond == kCondEQ) {
        __ Sltiu(dst, dst, 1);
      } else {
        __ Sltu(dst, ZERO, dst);
      }
      break;

    case kCondLT:
    case kCondGE:
      if (use_imm && IsInt<16>(rhs_imm)) {
        __ Slti(dst, lhs, rhs_imm);
      } else {
        if (use_imm) {
          rhs_reg = TMP;
          __ LoadImmediate(rhs_reg, rhs_imm);
        }
        __ Slt(dst, lhs, rhs_reg);
      }
      if (if_cond == kCondGE) {
        // Simulate lhs >= rhs via !(lhs < rhs) since there's
        // only the slt instruction but no sge.
        __ Xori(dst, dst, 1);
      }
      break;

    case kCondLE:
    case kCondGT:
      if (use_imm && IsInt<16>(rhs_imm + 1)) {
        // Simulate lhs <= rhs via lhs < rhs + 1.
        __ Slti(dst, lhs, rhs_imm + 1);
        if (if_cond == kCondGT) {
          // Simulate lhs > rhs via !(lhs <= rhs) since there's
          // only the slti instruction but no sgti.
          __ Xori(dst, dst, 1);
        }
      } else {
        if (use_imm) {
          rhs_reg = TMP;
          __ LoadImmediate(rhs_reg, rhs_imm);
        }
        __ Slt(dst, rhs_reg, lhs);
        if (if_cond == kCondLE) {
          // Simulate lhs <= rhs via !(rhs < lhs) since there's
          // only the slt instruction but no sle.
          __ Xori(dst, dst, 1);
        }
      }
      break;
  }
}

void LocationsBuilderMIPS::VisitDiv(HDiv* div) {
  Primitive::Type type = div->GetResultType();
  LocationSummary::CallKind call_kind = (type == Primitive::kPrimLong)
      ? LocationSummary::kCall
      : LocationSummary::kNoCall;
  LocationSummary* locations = new (GetGraph()->GetArena()) LocationSummary(div, call_kind);
  switch (type) {
    case Primitive::kPrimInt:
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetInAt(1, Location::RequiresRegister());
      locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
      break;

    case Primitive::kPrimLong: {
      InvokeRuntimeCallingConvention calling_convention;
      // TODO: Likewise, probably incorrect.
      locations->SetInAt(0, Location::RegisterPairLocation(calling_convention.GetRegisterAt(0),
                                                           calling_convention.GetRegisterAt(1)));
      locations->SetInAt(1, Location::RegisterPairLocation(calling_convention.GetRegisterAt(2),
                                                           calling_convention.GetRegisterAt(3)));
      locations->SetOut(calling_convention.GetReturnLocation(type));
      break;
    }

    case Primitive::kPrimFloat:
    case Primitive::kPrimDouble:
      locations->SetInAt(0, Location::RequiresFpuRegister());
      locations->SetInAt(1, Location::RequiresFpuRegister());
      locations->SetOut(Location::RequiresFpuRegister(), Location::kNoOutputOverlap);
      break;

    default:
      LOG(FATAL) << "Unexpected div type " << div->GetResultType();
  }
}

void InstructionCodeGeneratorMIPS::VisitDiv(HDiv* div) {
  Primitive::Type type = div->GetType();
  LocationSummary* locations = div->GetLocations();
  Location out = locations->Out();
  Location lhs = locations->InAt(0);
  Location rhs = locations->InAt(1);

  switch (type) {
    case Primitive::kPrimInt:
      __ Div(out.AsRegister<Register>(), lhs.AsRegister<Register>(), rhs.AsRegister<Register>());
      break;

    case Primitive::kPrimLong:
      // TODO: Implement div-long witout resorting to the runtime. (Do
      // not forget to update art::CodeGenerator::RecordPcInfo when it
      // is done).
      codegen_->InvokeRuntime(QUICK_ENTRY_POINT(pLdiv), div, div->GetDexPc(), nullptr);
      CheckEntrypointTypes<kQuickLdiv, int64_t, int64_t, int64_t>();
      break;

    case Primitive::kPrimFloat:
      __ DivS(out.AsFpuRegister<FRegister>(),
              lhs.AsFpuRegister<FRegister>(),
              rhs.AsFpuRegister<FRegister>());
      break;

    case Primitive::kPrimDouble:
      __ DivD(FromLowSToD(out.AsFpuRegisterPairLow<FRegister>()),
              FromLowSToD(lhs.AsFpuRegisterPairLow<FRegister>()),
              FromLowSToD(rhs.AsFpuRegisterPairLow<FRegister>()));
      break;

    default:
      LOG(FATAL) << "Unexpected div type " << type;
  }
}

void LocationsBuilderMIPS::VisitDivZeroCheck(HDivZeroCheck* instruction) {
  LocationSummary* locations =
      new (GetGraph()->GetArena()) LocationSummary(instruction, LocationSummary::kNoCall);
  locations->SetInAt(0, Location::RegisterOrConstant(instruction->InputAt(0)));
  if (instruction->HasUses()) {
    locations->SetOut(Location::SameAsFirstInput());
  }
}

void InstructionCodeGeneratorMIPS::VisitDivZeroCheck(HDivZeroCheck* instruction) {
  SlowPathCodeMIPS* slow_path =
    new (GetGraph()->GetArena()) DivZeroCheckSlowPathMIPS(instruction);
  codegen_->AddSlowPath(slow_path);
  Location value = instruction->GetLocations()->InAt(0);

  Primitive::Type type = instruction->GetType();

  if ((type == Primitive::kPrimBoolean) || !Primitive::IsIntegralType(type)) {
      LOG(FATAL) << "Unexpected type " << type << " for DivZeroCheck.";
    return;
  }

  if (value.IsConstant()) {
    int64_t divisor = codegen_->GetInt64ValueOf(value.GetConstant()->AsConstant());
    if (divisor == 0) {
      __ J(slow_path->GetEntryLabel());
    } else {
      // A division by a non-null constant is valid. We don't need to perform
      // any check, so simply fall through.
    }
  } else {
    if (value.IsRegister()) {
      __ Beq(value.AsRegister<Register>(), ZERO, slow_path->GetEntryLabel());
    } else {
      DCHECK(value.IsRegisterPair());
      __ Or(TMP, value.AsRegisterPairLow<Register>(), value.AsRegisterPairHigh<Register>());
      __ Beq(TMP, ZERO, slow_path->GetEntryLabel());
    }
  }
}

void LocationsBuilderMIPS::VisitDoubleConstant(HDoubleConstant* constant) {
  LocationSummary* locations =
      new (GetGraph()->GetArena()) LocationSummary(constant, LocationSummary::kNoCall);
  locations->SetOut(Location::ConstantLocation(constant));
}

void InstructionCodeGeneratorMIPS::VisitDoubleConstant(HDoubleConstant* cst ATTRIBUTE_UNUSED) {
  // Will be generated at use site.
}

void LocationsBuilderMIPS::VisitExit(HExit* exit) {
  exit->SetLocations(nullptr);
}

void InstructionCodeGeneratorMIPS::VisitExit(HExit* exit ATTRIBUTE_UNUSED) {
}

void LocationsBuilderMIPS::VisitFloatConstant(HFloatConstant* constant) {
  LocationSummary* locations =
      new (GetGraph()->GetArena()) LocationSummary(constant, LocationSummary::kNoCall);
  locations->SetOut(Location::ConstantLocation(constant));
}

void InstructionCodeGeneratorMIPS::VisitFloatConstant(HFloatConstant* constant ATTRIBUTE_UNUSED) {
  // Will be generated at use site.
}

void InstructionCodeGeneratorMIPS::HandleGoto(HInstruction* got, HBasicBlock* successor) {
  DCHECK(!successor->IsExitBlock());
  HBasicBlock* block = got->GetBlock();
  HInstruction* previous = got->GetPrevious();
  HLoopInformation* info = block->GetLoopInformation();

  if (info != nullptr && info->IsBackEdge(*block) && info->HasSuspendCheck()) {
    codegen_->ClearSpillSlotsFromLoopPhisInStackMap(info->GetSuspendCheck());
    GenerateSuspendCheck(info->GetSuspendCheck(), successor);
    return;
  }
  if (block->IsEntryBlock() && (previous != nullptr) && previous->IsSuspendCheck()) {
    GenerateSuspendCheck(previous->AsSuspendCheck(), nullptr);
  }
  if (!codegen_->GoesToNextBlock(block, successor)) {
    __ J(codegen_->GetLabelOf(successor));
  }
}

void LocationsBuilderMIPS::VisitGoto(HGoto* got) {
  got->SetLocations(nullptr);
}

void InstructionCodeGeneratorMIPS::VisitGoto(HGoto* got) {
  HandleGoto(got, got->GetSuccessor());
}

void LocationsBuilderMIPS::VisitTryBoundary(HTryBoundary* try_boundary) {
  try_boundary->SetLocations(nullptr);
}

void InstructionCodeGeneratorMIPS::VisitTryBoundary(HTryBoundary* try_boundary) {
  HBasicBlock* successor = try_boundary->GetNormalFlowSuccessor();
  if (!successor->IsExitBlock()) {
    HandleGoto(try_boundary, successor);
  }
}

void InstructionCodeGeneratorMIPS::GenerateTestAndBranch(HInstruction* instruction,
                                                         Label* true_target,
                                                         Label* false_target,
                                                         Label* always_true_target) {
  HInstruction* cond = instruction->InputAt(0);
  HCondition* condition = cond->AsCondition();

  if (cond->IsIntConstant()) {
    int32_t cond_value = cond->AsIntConstant()->GetValue();
    if (cond_value == 1) {
      if (always_true_target != nullptr) {
        __ J(always_true_target);
      }
      return;
    } else {
      DCHECK_EQ(cond_value, 0);
    }
  } else if (!cond->IsCondition() || condition->NeedsMaterialization()) {
    // The condition instruction has been materialized, compare the output to 0.
    Location cond_val = instruction->GetLocations()->InAt(0);
    DCHECK(cond_val.IsRegister());
    __ Bne(cond_val.AsRegister<Register>(), ZERO, true_target);
  } else {
    // The condition instruction has not been materialized, use its inputs as
    // the comparison and its condition as the branch condition.
    Register lhs = condition->GetLocations()->InAt(0).AsRegister<Register>();
    Location rhs_location = condition->GetLocations()->InAt(1);
    Register rhs_reg = ZERO;
    int32_t rhs_imm = 0;
    bool use_imm = rhs_location.IsConstant();
    if (use_imm) {
      rhs_imm = CodeGenerator::GetInt32ValueOf(rhs_location.GetConstant());
    } else {
      rhs_reg = rhs_location.AsRegister<Register>();
    }

    IfCondition if_cond = condition->GetCondition();
    if (use_imm && rhs_imm == 0) {
      switch (if_cond) {
        case kCondEQ:
          __ Beq(lhs, ZERO, true_target);
          break;
        case kCondNE:
          __ Bne(lhs, ZERO, true_target);
          break;
        case kCondLT:
          __ Bltz(lhs, true_target);
          break;
        case kCondGE:
          __ Bgez(lhs, true_target);
          break;
        case kCondLE:
          __ Blez(lhs, true_target);
          break;
        case kCondGT:
          __ Bgtz(lhs, true_target);
          break;
      }
    } else {
      if (use_imm) {
        rhs_reg = TMP;
        __ LoadImmediate(rhs_reg, rhs_imm);
      }
      // It looks like we can get here with lhs == rhs. Should that be possible at all?
      if (lhs == rhs_reg) {
        DCHECK(!use_imm);
        switch (if_cond) {
          case kCondEQ:
          case kCondGE:
          case kCondLE:
            // if lhs == rhs for a positive condition, then it is a branch
            __ J(true_target);
            break;
          case kCondNE:
          case kCondLT:
          case kCondGT:
            // if lhs == rhs for a negative condition, then it is a NOP
            break;
        }
      } else {
        switch (if_cond) {
          case kCondEQ:
            __ Beq(lhs, rhs_reg, true_target);
            break;
          case kCondNE:
            __ Bne(lhs, rhs_reg, true_target);
            break;
          case kCondLT:
            __ BranchOnLowerThan(lhs, rhs_reg, true_target);
            break;
          case kCondGE:
            __ BranchOnGreaterThanOrEqual(lhs, rhs_reg, true_target);
            break;
          case kCondLE:
            __ BranchOnLowerThanOrEqual(lhs, rhs_reg, true_target);
            break;
          case kCondGT:
            __ BranchOnGreaterThan(lhs, rhs_reg, true_target);
            break;
        }
      }
    }
  }
  if (false_target != nullptr) {
    __ J(false_target);
  }
}

void LocationsBuilderMIPS::VisitIf(HIf* if_instr) {
  LocationSummary* locations = new (GetGraph()->GetArena()) LocationSummary(if_instr);
  HInstruction* cond = if_instr->InputAt(0);
  if (!cond->IsCondition() || cond->AsCondition()->NeedsMaterialization()) {
    locations->SetInAt(0, Location::RequiresRegister());
  }
}

void InstructionCodeGeneratorMIPS::VisitIf(HIf* if_instr) {
  Label* true_target = codegen_->GetLabelOf(if_instr->IfTrueSuccessor());
  Label* false_target = codegen_->GetLabelOf(if_instr->IfFalseSuccessor());
  Label* always_true_target = true_target;
  if (codegen_->GoesToNextBlock(if_instr->GetBlock(),
                                if_instr->IfTrueSuccessor())) {
    always_true_target = nullptr;
  }
  if (codegen_->GoesToNextBlock(if_instr->GetBlock(),
                                if_instr->IfFalseSuccessor())) {
    false_target = nullptr;
  }
  GenerateTestAndBranch(if_instr, true_target, false_target, always_true_target);
}

void LocationsBuilderMIPS::VisitDeoptimize(HDeoptimize* deoptimize) {
  LocationSummary* locations = new (GetGraph()->GetArena())
      LocationSummary(deoptimize, LocationSummary::kCallOnSlowPath);
  HInstruction* cond = deoptimize->InputAt(0);
  DCHECK(cond->IsCondition());
  if (cond->AsCondition()->NeedsMaterialization()) {
    locations->SetInAt(0, Location::RequiresRegister());
  }
}

void InstructionCodeGeneratorMIPS::VisitDeoptimize(HDeoptimize* deoptimize) {
  SlowPathCodeMIPS* slow_path = new (GetGraph()->GetArena()) DeoptimizationSlowPathMIPS(deoptimize);
  codegen_->AddSlowPath(slow_path);
  Label* slow_path_entry = slow_path->GetEntryLabel();
  GenerateTestAndBranch(deoptimize, slow_path_entry, nullptr, slow_path_entry);
}

void LocationsBuilderMIPS::HandleFieldGet(HInstruction* instruction,
                                          const FieldInfo& field_info ATTRIBUTE_UNUSED) {
  LocationSummary* locations =
      new (GetGraph()->GetArena()) LocationSummary(instruction, LocationSummary::kNoCall);
  locations->SetInAt(0, Location::RequiresRegister());
  if (Primitive::IsFloatingPointType(instruction->GetType())) {
    locations->SetOut(Location::RequiresFpuRegister());
  } else {
    // The output overlaps in case of long: we don't want the low move to overwrite
    // the object's location.
    locations->SetOut(Location::RequiresRegister(),
        (instruction->GetType() == Primitive::kPrimLong) ? Location::kOutputOverlap
                                                         : Location::kNoOutputOverlap);
  }
}

void InstructionCodeGeneratorMIPS::HandleFieldGet(HInstruction* instruction,
                                                  const FieldInfo& field_info) {
  Primitive::Type type = field_info.GetFieldType();
  LocationSummary* locations = instruction->GetLocations();
  Register obj = locations->InAt(0).AsRegister<Register>();
  Location out = locations->Out();
  uint32_t offset = field_info.GetFieldOffset().Uint32Value();

  switch (type) {
    case Primitive::kPrimBoolean:
      __ LoadFromOffset(kLoadUnsignedByte, out.AsRegister<Register>(), obj, offset);
      break;

    case Primitive::kPrimByte:
      __ LoadFromOffset(kLoadSignedByte, out.AsRegister<Register>(), obj, offset);
      break;

    case Primitive::kPrimShort:
      __ LoadFromOffset(kLoadSignedHalfword, out.AsRegister<Register>(), obj, offset);
      break;

    case Primitive::kPrimChar:
      __ LoadFromOffset(kLoadUnsignedHalfword, out.AsRegister<Register>(), obj, offset);
      break;

    case Primitive::kPrimInt:
    case Primitive::kPrimNot:
      __ LoadFromOffset(kLoadWord, out.AsRegister<Register>(), obj, offset);
      break;

    case Primitive::kPrimLong:
      // TODO: Handle volatile fields.
      __ LoadFromOffset(kLoadWord, out.AsRegisterPairLow<Register>(), obj, offset);
      __ LoadFromOffset(
          kLoadWord, out.AsRegisterPairHigh<Register>(), obj, offset + kMipsWordSize);
      break;

    case Primitive::kPrimFloat:
      __ LoadSFromOffset(out.AsFpuRegister<FRegister>(), obj, offset);
      break;

    case Primitive::kPrimDouble:
      // TODO: Do volatile fields need special care here as well?
      __ LoadDFromOffset(FromLowSToD(out.AsFpuRegisterPairLow<FRegister>()), obj, offset);
      break;

    case Primitive::kPrimVoid:
      LOG(FATAL) << "Unreachable type " << type;
      UNREACHABLE();
  }

  codegen_->MaybeRecordImplicitNullCheck(instruction);

  // TODO: memory barrier?
}

void LocationsBuilderMIPS::HandleFieldSet(HInstruction* instruction,
                                          const FieldInfo& field_info ATTRIBUTE_UNUSED) {
  LocationSummary* locations =
      new (GetGraph()->GetArena()) LocationSummary(instruction, LocationSummary::kNoCall);
  locations->SetInAt(0, Location::RequiresRegister());
  if (Primitive::IsFloatingPointType(instruction->InputAt(1)->GetType())) {
    locations->SetInAt(1, Location::RequiresFpuRegister());
  } else {
    locations->SetInAt(1, Location::RequiresRegister());
  }
}

void InstructionCodeGeneratorMIPS::HandleFieldSet(HInstruction* instruction,
                                                  const FieldInfo& field_info) {
  Primitive::Type type = field_info.GetFieldType();
  LocationSummary* locations = instruction->GetLocations();
  Register obj = locations->InAt(0).AsRegister<Register>();
  Location value = locations->InAt(1);
  uint32_t offset = field_info.GetFieldOffset().Uint32Value();

  switch (type) {
    case Primitive::kPrimBoolean:
    case Primitive::kPrimByte:
      __ StoreToOffset(kStoreByte, value.AsRegister<Register>(), obj, offset);
      break;

    case Primitive::kPrimShort:
    case Primitive::kPrimChar:
      __ StoreToOffset(kStoreHalfword, value.AsRegister<Register>(), obj, offset);
      break;

    case Primitive::kPrimInt:
    case Primitive::kPrimNot:
      __ StoreToOffset(kStoreWord, value.AsRegister<Register>(), obj, offset);
      break;

    case Primitive::kPrimLong:
      __ StoreToOffset(kStoreWord, value.AsRegisterPairLow<Register>(), obj, offset);
      __ StoreToOffset(
          kStoreWord, value.AsRegisterPairHigh<Register>(), obj, offset + kMipsWordSize);
      break;

    case Primitive::kPrimFloat:
      // TODO: Handle volatile fields.
      __ StoreSToOffset(value.AsFpuRegister<FRegister>(), obj, offset);
      break;

    case Primitive::kPrimDouble:
      // TODO: Do volatile fields need special care here as well?
      __ StoreDToOffset(FromLowSToD(value.AsFpuRegisterPairLow<FRegister>()), obj, offset);
      break;

    case Primitive::kPrimVoid:
      LOG(FATAL) << "Unreachable type " << type;
      UNREACHABLE();
  }

  codegen_->MaybeRecordImplicitNullCheck(instruction);

  // TODO: memory barriers?

  if (CodeGenerator::StoreNeedsWriteBarrier(type, instruction->InputAt(1))) {
    DCHECK(value.IsRegister());
    Register src = value.AsRegister<Register>();
    codegen_->MarkGCCard(obj, src);
  }
}

void LocationsBuilderMIPS::VisitInstanceFieldGet(HInstanceFieldGet* instruction) {
  HandleFieldGet(instruction, instruction->GetFieldInfo());
}

void InstructionCodeGeneratorMIPS::VisitInstanceFieldGet(HInstanceFieldGet* instruction) {
  HandleFieldGet(instruction, instruction->GetFieldInfo());
}

void LocationsBuilderMIPS::VisitInstanceFieldSet(HInstanceFieldSet* instruction) {
  HandleFieldSet(instruction, instruction->GetFieldInfo());
}

void InstructionCodeGeneratorMIPS::VisitInstanceFieldSet(HInstanceFieldSet* instruction) {
  HandleFieldSet(instruction, instruction->GetFieldInfo());
}

void LocationsBuilderMIPS::VisitInstanceOf(HInstanceOf* instruction) {
  LocationSummary::CallKind call_kind =
      instruction->IsClassFinal() ? LocationSummary::kNoCall : LocationSummary::kCallOnSlowPath;
  LocationSummary* locations = new (GetGraph()->GetArena()) LocationSummary(instruction, call_kind);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RequiresRegister());
  // The output does overlap inputs.
  locations->SetOut(Location::RequiresRegister(), Location::kOutputOverlap);
}

void InstructionCodeGeneratorMIPS::VisitInstanceOf(HInstanceOf* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  Register obj = locations->InAt(0).AsRegister<Register>();
  Register cls = locations->InAt(1).AsRegister<Register>();
  Register out = locations->Out().AsRegister<Register>();

  Label done;

  // Return 0 if `obj` is null.
  // TODO: Avoid this check if we know `obj` is not null.
  __ Move(out, ZERO);
  __ Beq(obj, ZERO, &done);

  // Compare the class of `obj` with `cls`.
  __ LoadFromOffset(kLoadWord, out, obj, mirror::Object::ClassOffset().Int32Value());
  if (instruction->IsClassFinal()) {
    // Classes must be equal for the instanceof to succeed.
    __ Xor(out, out, cls);
    __ Sltiu(out, out, 1);
  } else {
    // If the classes are not equal, we go into a slow path.
    DCHECK(locations->OnlyCallsOnSlowPath());
    SlowPathCodeMIPS* slow_path =
        new (GetGraph()->GetArena()) TypeCheckSlowPathMIPS(instruction,
                                                           locations->InAt(1),
                                                           locations->Out(),
                                                           instruction->GetDexPc());
    codegen_->AddSlowPath(slow_path);
    __ Bne(out, cls, slow_path->GetEntryLabel());
    __ LoadImmediate(out, 1);
    __ Bind(slow_path->GetExitLabel(), /* is_jump */ false);  // TODO: Check second arg.
  }

  __ Bind(&done, /* is_jump */ false);  // TODO: Check second arg.
}

void LocationsBuilderMIPS::VisitIntConstant(HIntConstant* constant) {
  LocationSummary* locations = new (GetGraph()->GetArena()) LocationSummary(constant);
  locations->SetOut(Location::ConstantLocation(constant));
}

void InstructionCodeGeneratorMIPS::VisitIntConstant(HIntConstant* constant ATTRIBUTE_UNUSED) {
  // Will be generated at use site.
}

void LocationsBuilderMIPS::VisitNullConstant(HNullConstant* constant) {
  LocationSummary* locations = new (GetGraph()->GetArena()) LocationSummary(constant);
  locations->SetOut(Location::ConstantLocation(constant));
}

void InstructionCodeGeneratorMIPS::VisitNullConstant(HNullConstant* constant ATTRIBUTE_UNUSED) {
  // Will be generated at use site.
}

void LocationsBuilderMIPS::HandleInvoke(HInvoke* invoke) {
  InvokeDexCallingConventionVisitorMIPS calling_convention_visitor;
  CodeGenerator::CreateCommonInvokeLocationSummary(invoke, &calling_convention_visitor);
}

void LocationsBuilderMIPS::VisitInvokeInterface(HInvokeInterface* invoke) {
  HandleInvoke(invoke);
  // The register T0 is required to be used for the hidden argument in
  // art_quick_imt_conflict_trampoline, so add the hidden argument.
  invoke->GetLocations()->AddTemp(Location::RegisterLocation(T0));
}

void InstructionCodeGeneratorMIPS::VisitInvokeInterface(HInvokeInterface* invoke) {
  // TODO: b/18116999, our IMTs can miss an IncompatibleClassChangeError.
  Register temp = invoke->GetLocations()->GetTemp(0).AsRegister<Register>();
  uint32_t method_offset = mirror::Class::EmbeddedImTableEntryOffset(
      invoke->GetImtIndex() % mirror::Class::kImtSize, kMipsPointerSize).Uint32Value();
  Location receiver = invoke->GetLocations()->InAt(0);
  uint32_t class_offset = mirror::Object::ClassOffset().Int32Value();
  Offset entry_point = ArtMethod::EntryPointFromQuickCompiledCodeOffset(kMipsWordSize);

  // Set the hidden argument.
  __ LoadImmediate(invoke->GetLocations()->GetTemp(1).AsRegister<Register>(),
                   invoke->GetDexMethodIndex());

  // temp = object->GetClass();
  if (receiver.IsStackSlot()) {
    __ LoadFromOffset(kLoadWord, temp, SP, receiver.GetStackIndex());
    __ LoadFromOffset(kLoadWord, temp, temp, class_offset);
  } else {
    __ LoadFromOffset(kLoadWord, temp, receiver.AsRegister<Register>(), class_offset);
  }
  codegen_->MaybeRecordImplicitNullCheck(invoke);
  // temp = temp->GetImtEntryAt(method_offset);
  __ LoadFromOffset(kLoadWord, temp, temp, method_offset);
  // T9 = temp->GetEntryPoint();
  __ LoadFromOffset(kLoadWord, T9, temp, entry_point.Int32Value());
  // T9();
  __ Jalr(T9);
  DCHECK(!codegen_->IsLeafMethod());
  codegen_->RecordPcInfo(invoke, invoke->GetDexPc());
}

void LocationsBuilderMIPS::VisitInvokeVirtual(HInvokeVirtual* invoke) {
  // TODO intrinsic function
  HandleInvoke(invoke);
}

void LocationsBuilderMIPS::VisitInvokeStaticOrDirect(HInvokeStaticOrDirect* invoke) {
  // When we do not run baseline, explicit clinit checks triggered by static
  // invokes must have been pruned by art::PrepareForRegisterAllocation.
  DCHECK(codegen_->IsBaseline() || !invoke->IsStaticWithExplicitClinitCheck());

  // TODO - intrinsic function
  HandleInvoke(invoke);

  // While SetupBlockedRegisters() blocks registers S2-S8 due to their
  // clobbering somewhere else, reduce further register pressure by avoiding
  // allocation of a register for the current method pointer like on x86 baseline.
  // TODO: remove this once all the issues with register saving/restoring are
  // sorted out.
  LocationSummary* locations = invoke->GetLocations();
  Location location = locations->InAt(invoke->GetCurrentMethodInputIndex());
  if (location.IsUnallocated() && location.GetPolicy() == Location::kRequiresRegister) {
    locations->SetInAt(invoke->GetCurrentMethodInputIndex(), Location::NoLocation());
  }
}

static bool TryGenerateIntrinsicCode(HInvoke* invoke, CodeGeneratorMIPS* codegen ATTRIBUTE_UNUSED) {
  if (invoke->GetLocations()->Intrinsified()) {
    // TODO - intrinsic function
    return true;
  }
  return false;
}

void CodeGeneratorMIPS::GenerateStaticOrDirectCall(HInvokeStaticOrDirect* invoke, Location temp) {
  // All registers are assumed to be correctly set up per the calling convention.

  // TODO: Implement all kinds of calls:
  // 1) boot -> boot
  // 2) app -> boot
  // 3) app -> app
  //
  // Currently we implement the app -> app logic, which looks up in the resolve cache.

  if (invoke->IsStringInit()) {
    Register reg = temp.AsRegister<Register>();
    // temp = thread->string_init_entrypoint
    __ LoadFromOffset(kLoadWord, reg, TR, invoke->GetStringInitOffset());
    // T9 = temp->entry_point_from_quick_compiled_code_;
    __ LoadFromOffset(kLoadWord,
                      T9,
                      reg,
                      ArtMethod::EntryPointFromQuickCompiledCodeOffset(kMipsWordSize).Int32Value());
    // T9()
    __ Jalr(T9);
  } else if (invoke->IsRecursive()) {
    __ Jal(&frame_entry_label_);
  } else {
    Location current_method = invoke->GetLocations()->InAt(invoke->GetCurrentMethodInputIndex());
    Register reg = temp.AsRegister<Register>();
    Register method_reg;
    if (current_method.IsRegister()) {
      method_reg = current_method.AsRegister<Register>();
    } else {
      // TODO: use the appropriate DCHECK() here if possible.
      // DCHECK(invoke->GetLocations()->Intrinsified());
      DCHECK(!current_method.IsValid());
      method_reg = reg;
      __ Lw(reg, SP, kCurrentMethodStackOffset);
    }

    // temp = temp->dex_cache_resolved_methods_;
    __ LoadFromOffset(kLoadWord,
                      reg,
                      method_reg,
                      ArtMethod::DexCacheResolvedMethodsOffset().Int32Value());
    // temp = temp[index_in_cache]
    __ LoadFromOffset(kLoadWord,
                      reg,
                      reg,
                      CodeGenerator::GetCachePointerOffset(invoke->GetDexMethodIndex()));
    // T9 = temp[offset_of_quick_compiled_code]
    __ LoadFromOffset(kLoadWord,
                      T9,
                      reg,
                      ArtMethod::EntryPointFromQuickCompiledCodeOffset(kMipsWordSize).Int32Value());
    // T9()
    __ Jalr(T9);
  }

  DCHECK(!IsLeafMethod());
}

void InstructionCodeGeneratorMIPS::VisitInvokeStaticOrDirect(HInvokeStaticOrDirect* invoke) {
  // When we do not run baseline, explicit clinit checks triggered by static
  // invokes must have been pruned by art::PrepareForRegisterAllocation.
  DCHECK(codegen_->IsBaseline() || !invoke->IsStaticWithExplicitClinitCheck());

  if (TryGenerateIntrinsicCode(invoke, codegen_)) {
    return;
  }

  LocationSummary* locations = invoke->GetLocations();
  codegen_->GenerateStaticOrDirectCall(invoke,
                                       locations->HasTemps()
                                           ? locations->GetTemp(0)
                                           : Location::NoLocation());
  codegen_->RecordPcInfo(invoke, invoke->GetDexPc());
}

void InstructionCodeGeneratorMIPS::VisitInvokeVirtual(HInvokeVirtual* invoke) {
  // TODO: Try to generate intrinsics code.
  LocationSummary* locations = invoke->GetLocations();
  Location receiver = locations->InAt(0);
  Register temp = invoke->GetLocations()->GetTemp(0).AsRegister<Register>();
  size_t method_offset = mirror::Class::EmbeddedVTableEntryOffset(
      invoke->GetVTableIndex(), kMipsPointerSize).SizeValue();
  uint32_t class_offset = mirror::Object::ClassOffset().Int32Value();
  Offset entry_point = ArtMethod::EntryPointFromQuickCompiledCodeOffset(kMipsWordSize);

  // temp = object->GetClass();
  DCHECK(receiver.IsRegister());
  __ LoadFromOffset(kLoadWord, temp, receiver.AsRegister<Register>(), class_offset);
  codegen_->MaybeRecordImplicitNullCheck(invoke);
  // temp = temp->GetMethodAt(method_offset);
  __ LoadFromOffset(kLoadWord, temp, temp, method_offset);
  // T9 = temp->GetEntryPoint();
  __ LoadFromOffset(kLoadWord, T9, temp, entry_point.Int32Value());
  // T9();
  __ Jalr(T9);
  DCHECK(!codegen_->IsLeafMethod());
  codegen_->RecordPcInfo(invoke, invoke->GetDexPc());
}

void LocationsBuilderMIPS::VisitLoadClass(HLoadClass* cls) {
  LocationSummary::CallKind call_kind = cls->CanCallRuntime() ? LocationSummary::kCallOnSlowPath
                                                              : LocationSummary::kNoCall;
  LocationSummary* locations = new (GetGraph()->GetArena()) LocationSummary(cls, call_kind);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister());
}

void InstructionCodeGeneratorMIPS::VisitLoadClass(HLoadClass* cls) {
  LocationSummary* locations = cls->GetLocations();
  Register out = locations->Out().AsRegister<Register>();
  Register current_method = locations->InAt(0).AsRegister<Register>();
  if (cls->IsReferrersClass()) {
    DCHECK(!cls->CanCallRuntime());
    DCHECK(!cls->MustGenerateClinitCheck());
    __ LoadFromOffset(kLoadWord, out, current_method,
                      ArtMethod::DeclaringClassOffset().Int32Value());
  } else {
    DCHECK(cls->CanCallRuntime());
    __ LoadFromOffset(kLoadWord, out, current_method,
                      ArtMethod::DexCacheResolvedTypesOffset().Int32Value());
    __ LoadFromOffset(kLoadWord, out, out, CodeGenerator::GetCacheOffset(cls->GetTypeIndex()));
    SlowPathCodeMIPS* slow_path = new (GetGraph()->GetArena()) LoadClassSlowPathMIPS(
        cls,
        cls,
        cls->GetDexPc(),
        cls->MustGenerateClinitCheck());
    codegen_->AddSlowPath(slow_path);
    __ Beq(out, ZERO, slow_path->GetEntryLabel());
    if (cls->MustGenerateClinitCheck()) {
      GenerateClassInitializationCheck(slow_path, out);
    } else {
      __ Bind(slow_path->GetExitLabel(), /* is_jump */ false);  // TODO: Check second arg.
    }
  }
}

static int32_t GetExceptionTlsOffset() {
  return Thread::ExceptionOffset<kMipsWordSize>().Int32Value();
}

void LocationsBuilderMIPS::VisitLoadException(HLoadException* load) {
  LocationSummary* locations =
      new (GetGraph()->GetArena()) LocationSummary(load, LocationSummary::kNoCall);
  locations->SetOut(Location::RequiresRegister());
}

void InstructionCodeGeneratorMIPS::VisitLoadException(HLoadException* load) {
  Register out = load->GetLocations()->Out().AsRegister<Register>();
  __ LoadFromOffset(kLoadWord, out, TR, GetExceptionTlsOffset());
}

void LocationsBuilderMIPS::VisitClearException(HClearException* clear) {
  new (GetGraph()->GetArena()) LocationSummary(clear, LocationSummary::kNoCall);
}

void InstructionCodeGeneratorMIPS::VisitClearException(HClearException* clear ATTRIBUTE_UNUSED) {
  __ StoreToOffset(kStoreWord, ZERO, TR, GetExceptionTlsOffset());
}

void LocationsBuilderMIPS::VisitLoadLocal(HLoadLocal* load) {
  load->SetLocations(nullptr);
}

void InstructionCodeGeneratorMIPS::VisitLoadLocal(HLoadLocal* load ATTRIBUTE_UNUSED) {
  // Nothing to do, this is driven by the code generator.
}

void LocationsBuilderMIPS::VisitLoadString(HLoadString* load) {
  LocationSummary* locations =
      new (GetGraph()->GetArena()) LocationSummary(load, LocationSummary::kCallOnSlowPath);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister());
}

void InstructionCodeGeneratorMIPS::VisitLoadString(HLoadString* load) {
  SlowPathCodeMIPS* slow_path = new (GetGraph()->GetArena()) LoadStringSlowPathMIPS(load);
  codegen_->AddSlowPath(slow_path);

  LocationSummary* locations = load->GetLocations();
  Register out = locations->Out().AsRegister<Register>();
  Register current_method = locations->InAt(0).AsRegister<Register>();
  __ LoadFromOffset(kLoadWord, out, current_method,
                    ArtMethod::DeclaringClassOffset().Int32Value());
  __ LoadFromOffset(kLoadWord, out, out, mirror::Class::DexCacheStringsOffset().Int32Value());
  __ LoadFromOffset(kLoadWord, out, out, CodeGenerator::GetCacheOffset(load->GetStringIndex()));
  __ Beq(out, ZERO, slow_path->GetEntryLabel());
  __ Bind(slow_path->GetExitLabel(), /* is_jump */ false);  // TODO: Check second arg.
}

void LocationsBuilderMIPS::VisitLocal(HLocal* local) {
  local->SetLocations(nullptr);
}

void InstructionCodeGeneratorMIPS::VisitLocal(HLocal* local) {
  DCHECK_EQ(local->GetBlock(), GetGraph()->GetEntryBlock());
}

void LocationsBuilderMIPS::VisitLongConstant(HLongConstant* constant) {
  LocationSummary* locations = new (GetGraph()->GetArena()) LocationSummary(constant);
  locations->SetOut(Location::ConstantLocation(constant));
}

void InstructionCodeGeneratorMIPS::VisitLongConstant(HLongConstant* constant ATTRIBUTE_UNUSED) {
  // Will be generated at use site.
}

void LocationsBuilderMIPS::VisitMonitorOperation(HMonitorOperation* instruction) {
  LocationSummary* locations =
      new (GetGraph()->GetArena()) LocationSummary(instruction, LocationSummary::kCall);
  InvokeRuntimeCallingConvention calling_convention;
  locations->SetInAt(0, Location::RegisterLocation(calling_convention.GetRegisterAt(0)));
}

void InstructionCodeGeneratorMIPS::VisitMonitorOperation(HMonitorOperation* instruction) {
  codegen_->InvokeRuntime(instruction->IsEnter()
                              ? QUICK_ENTRY_POINT(pLockObject)
                              : QUICK_ENTRY_POINT(pUnlockObject),
                          instruction,
                          instruction->GetDexPc(),
                          nullptr);
  CheckEntrypointTypes<kQuickLockObject, void, mirror::Object*>();
}

void LocationsBuilderMIPS::VisitMul(HMul* mul) {
  Primitive::Type type = mul->GetResultType();
  LocationSummary::CallKind call_kind = (type == Primitive::kPrimLong)
      ? LocationSummary::kCall
      : LocationSummary::kNoCall;
  LocationSummary* locations = new (GetGraph()->GetArena()) LocationSummary(mul, call_kind);
  switch (type) {
    case Primitive::kPrimInt:
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetInAt(1, Location::RequiresRegister());
      locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
      break;

    case Primitive::kPrimLong: {
      InvokeRuntimeCallingConvention calling_convention;
      locations->SetInAt(0, Location::RegisterPairLocation(calling_convention.GetRegisterAt(0),
                                                           calling_convention.GetRegisterAt(1)));
      locations->SetInAt(1, Location::RegisterPairLocation(calling_convention.GetRegisterAt(2),
                                                           calling_convention.GetRegisterAt(3)));
      locations->SetOut(calling_convention.GetReturnLocation(type));
      break;
    }

    case Primitive::kPrimFloat:
    case Primitive::kPrimDouble:
      locations->SetInAt(0, Location::RequiresFpuRegister());
      locations->SetInAt(1, Location::RequiresFpuRegister());
      locations->SetOut(Location::RequiresFpuRegister(), Location::kNoOutputOverlap);
      break;

    default:
      LOG(FATAL) << "Unexpected mul type " << mul->GetResultType();
  }
}

void InstructionCodeGeneratorMIPS::VisitMul(HMul* mul) {
  Primitive::Type type = mul->GetType();
  LocationSummary* locations = mul->GetLocations();
  Location out = locations->Out();
  Location lhs = locations->InAt(0);
  Location rhs = locations->InAt(1);

  switch (type) {
    case Primitive::kPrimInt:
      __ Mul(out.AsRegister<Register>(), lhs.AsRegister<Register>(), rhs.AsRegister<Register>());
      break;

    case Primitive::kPrimLong:
      // TODO: Implement mul-long witout resorting to the runtime. (Do
      // not forget to update art::CodeGenerator::RecordPcInfo when it
      // is done).
      codegen_->InvokeRuntime(QUICK_ENTRY_POINT(pLmul), mul, mul->GetDexPc(), nullptr);
      CheckEntrypointTypes<kQuickLmul, int64_t, int64_t, int64_t>();
      break;

    case Primitive::kPrimFloat:
      __ MulS(out.AsFpuRegister<FRegister>(),
              lhs.AsFpuRegister<FRegister>(),
              rhs.AsFpuRegister<FRegister>());
      break;

    case Primitive::kPrimDouble:
      __ MulD(FromLowSToD(out.AsFpuRegisterPairLow<FRegister>()),
              FromLowSToD(lhs.AsFpuRegisterPairLow<FRegister>()),
              FromLowSToD(rhs.AsFpuRegisterPairLow<FRegister>()));
      break;

    default:
      LOG(FATAL) << "Unexpected mul type " << type;
  }
}

void LocationsBuilderMIPS::VisitNeg(HNeg* neg) {
  LocationSummary* locations =
      new (GetGraph()->GetArena()) LocationSummary(neg, LocationSummary::kNoCall);
  switch (neg->GetResultType()) {
    case Primitive::kPrimInt:
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
      break;

    case Primitive::kPrimLong:
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetOut(Location::RequiresRegister(), Location::kOutputOverlap);
      break;

    case Primitive::kPrimFloat:
    case Primitive::kPrimDouble:
      locations->SetInAt(0, Location::RequiresFpuRegister());
      locations->SetOut(Location::RequiresFpuRegister(), Location::kNoOutputOverlap);
      break;

    default:
      LOG(FATAL) << "Unexpected neg type " << neg->GetResultType();
  }
}

void InstructionCodeGeneratorMIPS::VisitNeg(HNeg* instruction) {
  Primitive::Type type = instruction->GetType();
  LocationSummary* locations = instruction->GetLocations();
  Location out = locations->Out();
  Location in = locations->InAt(0);

  switch (type) {
    case Primitive::kPrimInt:
      __ Subu(out.AsRegister<Register>(), ZERO, in.AsRegister<Register>());
      break;

    case Primitive::kPrimLong: {
      Register out_low = out.AsRegisterPairLow<Register>();
      Register out_high = out.AsRegisterPairHigh<Register>();
      Register in_low = in.AsRegisterPairLow<Register>();
      Register in_high = in.AsRegisterPairHigh<Register>();
      __ Subu(out_low, ZERO, in_low);
      __ Subu(out_high, ZERO, in_high);
      __ Sltu(TMP, ZERO, in_low);
      __ Subu(out_high, out_high, TMP);
      break;
    }

    case Primitive::kPrimFloat:
      __ NegS(out.AsFpuRegister<FRegister>(), in.AsFpuRegister<FRegister>());
      break;

    case Primitive::kPrimDouble:
      __ NegD(FromLowSToD(out.AsFpuRegisterPairLow<FRegister>()),
              FromLowSToD(in.AsFpuRegisterPairLow<FRegister>()));
      break;

    default:
      LOG(FATAL) << "Unexpected neg type " << type;
  }
}

void LocationsBuilderMIPS::VisitNewArray(HNewArray* instruction) {
  LocationSummary* locations =
      new (GetGraph()->GetArena()) LocationSummary(instruction, LocationSummary::kCall);
  InvokeRuntimeCallingConvention calling_convention;
  locations->AddTemp(Location::RegisterLocation(calling_convention.GetRegisterAt(0)));
  locations->SetOut(calling_convention.GetReturnLocation(Primitive::kPrimNot));
  locations->SetInAt(0, Location::RegisterLocation(calling_convention.GetRegisterAt(1)));
  locations->SetInAt(1, Location::RegisterLocation(calling_convention.GetRegisterAt(2)));
}

void InstructionCodeGeneratorMIPS::VisitNewArray(HNewArray* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  // Move an uint16_t value to a register.
  __ LoadImmediate(locations->GetTemp(0).AsRegister<Register>(), instruction->GetTypeIndex());
  codegen_->InvokeRuntime(
      GetThreadOffset<kMipsWordSize>(instruction->GetEntrypoint()).Int32Value(),
      instruction,
      instruction->GetDexPc(),
      nullptr);
  CheckEntrypointTypes<kQuickAllocArrayWithAccessCheck, void*, uint32_t, int32_t, ArtMethod*>();
}

void LocationsBuilderMIPS::VisitNewInstance(HNewInstance* instruction) {
  LocationSummary* locations =
      new (GetGraph()->GetArena()) LocationSummary(instruction, LocationSummary::kCall);
  InvokeRuntimeCallingConvention calling_convention;
  locations->AddTemp(Location::RegisterLocation(calling_convention.GetRegisterAt(0)));
  locations->SetInAt(0, Location::RegisterLocation(calling_convention.GetRegisterAt(1)));
  locations->SetOut(calling_convention.GetReturnLocation(Primitive::kPrimNot));
}

void InstructionCodeGeneratorMIPS::VisitNewInstance(HNewInstance* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  // Move an uint16_t value to a register.
  __ LoadImmediate(locations->GetTemp(0).AsRegister<Register>(), instruction->GetTypeIndex());
  codegen_->InvokeRuntime(
      GetThreadOffset<kMipsWordSize>(instruction->GetEntrypoint()).Int32Value(),
      instruction,
      instruction->GetDexPc(),
      nullptr);
  CheckEntrypointTypes<kQuickAllocObjectWithAccessCheck, void*, uint32_t, ArtMethod*>();
}

void LocationsBuilderMIPS::VisitNot(HNot* instruction) {
  LocationSummary* locations = new (GetGraph()->GetArena()) LocationSummary(instruction);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
}

void InstructionCodeGeneratorMIPS::VisitNot(HNot* instruction) {
  Primitive::Type type = instruction->GetType();
  LocationSummary* locations = instruction->GetLocations();
  Location out = locations->Out();
  Location in = locations->InAt(0);

  switch (type) {
    case Primitive::kPrimInt: {
      __ Nor(out.AsRegister<Register>(), in.AsRegister<Register>(), ZERO);
      break;
    }

    case Primitive::kPrimLong:
      __ Nor(out.AsRegisterPairLow<Register>(), in.AsRegisterPairLow<Register>(), ZERO);
      __ Nor(out.AsRegisterPairHigh<Register>(), in.AsRegisterPairHigh<Register>(), ZERO);
      break;

    default:
      LOG(FATAL) << "Unexpected type for not operation " << instruction->GetResultType();
  }
}

void LocationsBuilderMIPS::VisitBooleanNot(HBooleanNot* instruction) {
  LocationSummary* locations = new (GetGraph()->GetArena()) LocationSummary(instruction);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
}

void InstructionCodeGeneratorMIPS::VisitBooleanNot(HBooleanNot* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  __ Xori(locations->Out().AsRegister<Register>(),
          locations->InAt(0).AsRegister<Register>(),
          1);
}

void LocationsBuilderMIPS::VisitNullCheck(HNullCheck* instruction) {
  LocationSummary* locations =
      new (GetGraph()->GetArena()) LocationSummary(instruction, LocationSummary::kNoCall);
  locations->SetInAt(0, Location::RequiresRegister());
  if (instruction->HasUses()) {
    locations->SetOut(Location::SameAsFirstInput());
  }
}

void InstructionCodeGeneratorMIPS::GenerateImplicitNullCheck(HNullCheck* instruction) {
  if (codegen_->CanMoveNullCheckToUser(instruction)) {
    return;
  }
  Location obj = instruction->GetLocations()->InAt(0);

  __ Lw(ZERO, obj.AsRegister<Register>(), 0);
  codegen_->RecordPcInfo(instruction, instruction->GetDexPc());
}

void InstructionCodeGeneratorMIPS::GenerateExplicitNullCheck(HNullCheck* instruction) {
  SlowPathCodeMIPS* slow_path = new (GetGraph()->GetArena()) NullCheckSlowPathMIPS(instruction);
  codegen_->AddSlowPath(slow_path);

  Location obj = instruction->GetLocations()->InAt(0);

  __ Beq(obj.AsRegister<Register>(), ZERO, slow_path->GetEntryLabel());
}

void InstructionCodeGeneratorMIPS::VisitNullCheck(HNullCheck* instruction) {
  if (codegen_->GetCompilerOptions().GetImplicitNullChecks()) {
    GenerateImplicitNullCheck(instruction);
  } else {
    GenerateExplicitNullCheck(instruction);
  }
}

void LocationsBuilderMIPS::VisitOr(HOr* instruction) {
  HandleBitwiseOperation(instruction);
}

void InstructionCodeGeneratorMIPS::VisitOr(HOr* instruction) {
  HandleBitwiseOperation(instruction);
}

void LocationsBuilderMIPS::VisitParallelMove(HParallelMove* instruction ATTRIBUTE_UNUSED) {
  LOG(FATAL) << "Unreachable";
}

void InstructionCodeGeneratorMIPS::VisitParallelMove(HParallelMove* instruction) {
  codegen_->GetMoveResolver()->EmitNativeCode(instruction);
}

void LocationsBuilderMIPS::VisitParameterValue(HParameterValue* instruction) {
  LocationSummary* locations =
      new (GetGraph()->GetArena()) LocationSummary(instruction, LocationSummary::kNoCall);
  Location location = parameter_visitor_.GetNextLocation(instruction->GetType());
  if (location.IsStackSlot()) {
    location = Location::StackSlot(location.GetStackIndex() + codegen_->GetFrameSize());
  } else if (location.IsDoubleStackSlot()) {
    location = Location::DoubleStackSlot(location.GetStackIndex() + codegen_->GetFrameSize());
  }
  locations->SetOut(location);
}

void InstructionCodeGeneratorMIPS::VisitParameterValue(HParameterValue* instruction
                                                       ATTRIBUTE_UNUSED) {
  // Nothing to do, the parameter is already at its location.
}

void LocationsBuilderMIPS::VisitCurrentMethod(HCurrentMethod* instruction) {
  LocationSummary* locations =
      new (GetGraph()->GetArena()) LocationSummary(instruction, LocationSummary::kNoCall);
  locations->SetOut(Location::RegisterLocation(kMethodRegisterArgument));
}

void InstructionCodeGeneratorMIPS::VisitCurrentMethod(HCurrentMethod* instruction
                                                      ATTRIBUTE_UNUSED) {
  // Nothing to do, the method is already at its location.
}

void LocationsBuilderMIPS::VisitPhi(HPhi* instruction) {
  LocationSummary* locations = new (GetGraph()->GetArena()) LocationSummary(instruction);
  for (size_t i = 0, e = instruction->InputCount(); i < e; ++i) {
    locations->SetInAt(i, Location::Any());
  }
  locations->SetOut(Location::Any());
}

void InstructionCodeGeneratorMIPS::VisitPhi(HPhi* instruction ATTRIBUTE_UNUSED) {
  LOG(FATAL) << "Unreachable";
}

void LocationsBuilderMIPS::VisitRem(HRem* rem) {
  Primitive::Type type = rem->GetResultType();
  LocationSummary::CallKind call_kind =
      ((type == Primitive::kPrimLong) || Primitive::IsFloatingPointType(type))
      ? LocationSummary::kCall
      : LocationSummary::kNoCall;
  LocationSummary* locations = new (GetGraph()->GetArena()) LocationSummary(rem, call_kind);

  switch (type) {
    case Primitive::kPrimInt:
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetInAt(1, Location::RequiresRegister());
      locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
      break;

    case Primitive::kPrimLong: {
      InvokeRuntimeCallingConvention calling_convention;
      locations->SetInAt(0, Location::RegisterPairLocation(calling_convention.GetRegisterAt(0),
                                                           calling_convention.GetRegisterAt(1)));
      locations->SetInAt(1, Location::RegisterPairLocation(calling_convention.GetRegisterAt(2),
                                                           calling_convention.GetRegisterAt(3)));
      locations->SetOut(calling_convention.GetReturnLocation(type));
      break;
    }

    case Primitive::kPrimFloat: {
      InvokeRuntimeCallingConvention calling_convention;
      locations->SetInAt(0, Location::FpuRegisterLocation(calling_convention.GetFpuRegisterAt(0)));
      locations->SetInAt(1, Location::FpuRegisterLocation(calling_convention.GetFpuRegisterAt(1)));
      locations->SetOut(calling_convention.GetReturnLocation(type));
      break;
    }

    case Primitive::kPrimDouble: {
      InvokeRuntimeCallingConvention calling_convention;
      locations->SetInAt(0, Location::FpuRegisterPairLocation(
          calling_convention.GetFpuRegisterAt(0),
          calling_convention.GetFpuRegisterAt(1)));
      locations->SetInAt(1, Location::FpuRegisterPairLocation(
          calling_convention.GetFpuRegisterAt(2),
          calling_convention.GetFpuRegisterAt(3)));
      locations->SetOut(calling_convention.GetReturnLocation(type));
      break;
    }

    default:
      LOG(FATAL) << "Unexpected rem type " << type;
  }
}

void InstructionCodeGeneratorMIPS::VisitRem(HRem* instruction) {
  Primitive::Type type = instruction->GetType();
  switch (type) {
    case Primitive::kPrimInt: {
      LocationSummary* locations = instruction->GetLocations();
        __ Rem(locations->Out().AsRegister<Register>(),
               locations->InAt(0).AsRegister<Register>(),
               locations->InAt(1).AsRegister<Register>());
      break;
    }

    case Primitive::kPrimLong:
      // TODO: Implement rem-long witout resorting to the runtime.
      // (Do not forget to update art::CodeGenerator::RecordPcInfo
      // when it is done).
      codegen_->InvokeRuntime(
          QUICK_ENTRY_POINT(pLmod), instruction, instruction->GetDexPc(), nullptr);
      CheckEntrypointTypes<kQuickLmod, int64_t, int64_t, int64_t>();
      break;

    case Primitive::kPrimFloat:
    case Primitive::kPrimDouble: {
      // TODO: Implement rem-float and rem-double witout resorting to
      // the runtime? (Do not forget to update
      // art::CodeGenerator::RecordPcInfo if it is eventually done).
      int32_t entry_offset = (type == Primitive::kPrimFloat)
          ? QUICK_ENTRY_POINT(pFmodf)
          : QUICK_ENTRY_POINT(pFmod);
      codegen_->InvokeRuntime(entry_offset, instruction, instruction->GetDexPc(), nullptr);
      if (type == Primitive::kPrimFloat) {
        CheckEntrypointTypes<kQuickFmodf, float, float, float>();
      } else {
        CheckEntrypointTypes<kQuickFmod, double, double, double>();
      }
      break;
    }
    default:
      LOG(FATAL) << "Unexpected rem type " << type;
  }
}

void LocationsBuilderMIPS::VisitMemoryBarrier(HMemoryBarrier* memory_barrier) {
  memory_barrier->SetLocations(nullptr);
}

void InstructionCodeGeneratorMIPS::VisitMemoryBarrier(HMemoryBarrier* memory_barrier) {
  GenerateMemoryBarrier(memory_barrier->GetBarrierKind());
}

void LocationsBuilderMIPS::VisitReturn(HReturn* ret) {
  LocationSummary* locations = new (GetGraph()->GetArena()) LocationSummary(ret);
  Primitive::Type return_type = ret->InputAt(0)->GetType();
  locations->SetInAt(0, MipsReturnLocation(return_type));
}

void InstructionCodeGeneratorMIPS::VisitReturn(HReturn* ret ATTRIBUTE_UNUSED) {
  codegen_->GenerateFrameExit();
}

void LocationsBuilderMIPS::VisitReturnVoid(HReturnVoid* ret) {
  ret->SetLocations(nullptr);
}

void InstructionCodeGeneratorMIPS::VisitReturnVoid(HReturnVoid* ret ATTRIBUTE_UNUSED) {
  codegen_->GenerateFrameExit();
}

void LocationsBuilderMIPS::VisitShl(HShl* shl) {
  HandleShift(shl);
}

void InstructionCodeGeneratorMIPS::VisitShl(HShl* shl) {
  HandleShift(shl);
}

void LocationsBuilderMIPS::VisitShr(HShr* shr) {
  HandleShift(shr);
}

void InstructionCodeGeneratorMIPS::VisitShr(HShr* shr) {
  HandleShift(shr);
}

void LocationsBuilderMIPS::VisitStoreLocal(HStoreLocal* store) {
  LocationSummary* locations = new (GetGraph()->GetArena()) LocationSummary(store);
  Primitive::Type field_type = store->InputAt(1)->GetType();
  switch (field_type) {
    case Primitive::kPrimNot:
    case Primitive::kPrimBoolean:
    case Primitive::kPrimByte:
    case Primitive::kPrimChar:
    case Primitive::kPrimShort:
    case Primitive::kPrimInt:
    case Primitive::kPrimFloat:
      locations->SetInAt(1, Location::StackSlot(codegen_->GetStackSlot(store->GetLocal())));
      break;

    case Primitive::kPrimLong:
    case Primitive::kPrimDouble:
      locations->SetInAt(1, Location::DoubleStackSlot(codegen_->GetStackSlot(store->GetLocal())));
      break;

    default:
      LOG(FATAL) << "Unimplemented local type " << field_type;
  }
}

void InstructionCodeGeneratorMIPS::VisitStoreLocal(HStoreLocal* store ATTRIBUTE_UNUSED) {
}

void LocationsBuilderMIPS::VisitSub(HSub* sub) {
  LocationSummary* locations =
      new (GetGraph()->GetArena()) LocationSummary(sub, LocationSummary::kNoCall);
  switch (sub->GetResultType()) {
    case Primitive::kPrimInt: {
      locations->SetInAt(0, Location::RequiresRegister());
      HInstruction* rhs = sub->InputAt(1);
      if (rhs->IsIntConstant() && IsInt<16>(-rhs->AsIntConstant()->GetValue())) {
        locations->SetInAt(1, Location::ConstantLocation(rhs->AsConstant()));
      } else {
        locations->SetInAt(1, Location::RequiresRegister());
      }
      locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
      break;
    }

    case Primitive::kPrimLong: {
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetInAt(1, Location::RequiresRegister());
      locations->SetOut(Location::RequiresRegister(), Location::kOutputOverlap);
      break;
    }

    case Primitive::kPrimFloat:
    case Primitive::kPrimDouble: {
      locations->SetInAt(0, Location::RequiresFpuRegister());
      locations->SetInAt(1, Location::RequiresFpuRegister());
      locations->SetOut(Location::RequiresFpuRegister(), Location::kNoOutputOverlap);
      break;
    }

    default:
      LOG(FATAL) << "Unexpected sub type " << sub->GetResultType();
  }
}

void InstructionCodeGeneratorMIPS::VisitSub(HSub* sub) {
  LocationSummary* locations = sub->GetLocations();
  Location out = locations->Out();
  Location lhs = locations->InAt(0);
  Location rhs = locations->InAt(1);
  switch (sub->GetResultType()) {
    case Primitive::kPrimInt:
      if (rhs.IsRegister()) {
        __ Subu(out.AsRegister<Register>(),
                lhs.AsRegister<Register>(),
                rhs.AsRegister<Register>());
      } else {
        __ Addiu(out.AsRegister<Register>(),
                 lhs.AsRegister<Register>(),
                 -rhs.GetConstant()->AsIntConstant()->GetValue());
      }
      break;

    case Primitive::kPrimLong: {
      DCHECK(rhs.IsRegisterPair());
      Register out_low = out.AsRegisterPairLow<Register>();
      Register out_high = out.AsRegisterPairHigh<Register>();
      Register lhs_low = lhs.AsRegisterPairLow<Register>();
      Register lhs_high = lhs.AsRegisterPairHigh<Register>();
      Register rhs_low = rhs.AsRegisterPairLow<Register>();
      Register rhs_high = rhs.AsRegisterPairHigh<Register>();
      __ Subu(out_low, lhs_low, rhs_low);
      __ Subu(out_high, lhs_high, rhs_high);
      // Borrow.
      __ Sltu(TMP, lhs_low, rhs_low);
      __ Subu(out_high, out_high, TMP);
      break;
    }

    case Primitive::kPrimFloat:
      __ SubS(out.AsFpuRegister<FRegister>(),
              lhs.AsFpuRegister<FRegister>(),
              rhs.AsFpuRegister<FRegister>());
      break;

    case Primitive::kPrimDouble:
      __ SubD(FromLowSToD(out.AsFpuRegisterPairLow<FRegister>()),
              FromLowSToD(lhs.AsFpuRegisterPairLow<FRegister>()),
              FromLowSToD(rhs.AsFpuRegisterPairLow<FRegister>()));
      break;

    default:
      LOG(FATAL) << "Unexpected sub type " << sub->GetResultType();
  }
}

void LocationsBuilderMIPS::VisitStaticFieldGet(HStaticFieldGet* instruction) {
  HandleFieldGet(instruction, instruction->GetFieldInfo());
}

void InstructionCodeGeneratorMIPS::VisitStaticFieldGet(HStaticFieldGet* instruction) {
  HandleFieldGet(instruction, instruction->GetFieldInfo());
}

void LocationsBuilderMIPS::VisitStaticFieldSet(HStaticFieldSet* instruction) {
  HandleFieldSet(instruction, instruction->GetFieldInfo());
}

void InstructionCodeGeneratorMIPS::VisitStaticFieldSet(HStaticFieldSet* instruction) {
  HandleFieldSet(instruction, instruction->GetFieldInfo());
}

void LocationsBuilderMIPS::VisitSuspendCheck(HSuspendCheck* instruction) {
  new (GetGraph()->GetArena()) LocationSummary(instruction, LocationSummary::kCallOnSlowPath);
}

void InstructionCodeGeneratorMIPS::VisitSuspendCheck(HSuspendCheck* instruction) {
  HBasicBlock* block = instruction->GetBlock();
  if (block->GetLoopInformation() != nullptr) {
    DCHECK(block->GetLoopInformation()->GetSuspendCheck() == instruction);
    // The back edge will generate the suspend check.
    return;
  }
  if (block->IsEntryBlock() && instruction->GetNext()->IsGoto()) {
    // The goto will generate the suspend check.
    return;
  }
  GenerateSuspendCheck(instruction, nullptr);
}

void LocationsBuilderMIPS::VisitTemporary(HTemporary* temp) {
  temp->SetLocations(nullptr);
}

void InstructionCodeGeneratorMIPS::VisitTemporary(HTemporary* temp ATTRIBUTE_UNUSED) {
  // Nothing to do, this is driven by the code generator.
}

void LocationsBuilderMIPS::VisitThrow(HThrow* instruction) {
  LocationSummary* locations =
      new (GetGraph()->GetArena()) LocationSummary(instruction, LocationSummary::kCall);
  InvokeRuntimeCallingConvention calling_convention;
  locations->SetInAt(0, Location::RegisterLocation(calling_convention.GetRegisterAt(0)));
}

void InstructionCodeGeneratorMIPS::VisitThrow(HThrow* instruction) {
  codegen_->InvokeRuntime(QUICK_ENTRY_POINT(pDeliverException),
                          instruction,
                          instruction->GetDexPc(),
                          nullptr);
  CheckEntrypointTypes<kQuickDeliverException, void, mirror::Object*>();
}

void LocationsBuilderMIPS::VisitTypeConversion(HTypeConversion* conversion) {
  Primitive::Type input_type = conversion->GetInputType();
  Primitive::Type result_type = conversion->GetResultType();
  DCHECK_NE(input_type, result_type);

  if ((input_type == Primitive::kPrimNot) || (input_type == Primitive::kPrimVoid) ||
      (result_type == Primitive::kPrimNot) || (result_type == Primitive::kPrimVoid)) {
    LOG(FATAL) << "Unexpected type conversion from " << input_type << " to " << result_type;
  }

  LocationSummary::CallKind call_kind = LocationSummary::kNoCall;
  if ((Primitive::IsFloatingPointType(result_type) && input_type == Primitive::kPrimLong) ||
      (Primitive::IsIntegralType(result_type) && Primitive::IsFloatingPointType(input_type))) {
    call_kind = LocationSummary::kCall;
  }

  LocationSummary* locations = new (GetGraph()->GetArena()) LocationSummary(conversion, call_kind);

  if (call_kind == LocationSummary::kNoCall) {
    if (Primitive::IsFloatingPointType(input_type)) {
      locations->SetInAt(0, Location::RequiresFpuRegister());
    } else {
      locations->SetInAt(0, Location::RequiresRegister());
    }

    if (Primitive::IsFloatingPointType(result_type)) {
      locations->SetOut(Location::RequiresFpuRegister(), Location::kNoOutputOverlap);
    } else {
      locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
    }
  } else {
    DCHECK(call_kind == LocationSummary::kCall);
    InvokeRuntimeCallingConvention calling_convention;

    if (input_type == Primitive::kPrimLong) {
      locations->SetInAt(0, Location::RegisterPairLocation(calling_convention.GetRegisterAt(0),
                                                           calling_convention.GetRegisterAt(1)));
    } else if (input_type == Primitive::kPrimFloat) {
      locations->SetInAt(0, Location::FpuRegisterLocation(calling_convention.GetFpuRegisterAt(0)));
    } else {
      DCHECK_EQ(input_type, Primitive::kPrimDouble);
      locations->SetInAt(0, Location::FpuRegisterPairLocation(
          calling_convention.GetFpuRegisterAt(0),
          calling_convention.GetFpuRegisterAt(1)));
    }

    locations->SetOut(calling_convention.GetReturnLocation(result_type));
  }
}

void InstructionCodeGeneratorMIPS::VisitTypeConversion(HTypeConversion* conversion) {
  LocationSummary* locations = conversion->GetLocations();
  Location out = locations->Out();
  Location in = locations->InAt(0);
  Primitive::Type result_type = conversion->GetResultType();
  Primitive::Type input_type = conversion->GetInputType();

  DCHECK_NE(input_type, result_type);

  if (Primitive::IsIntegralType(result_type) && Primitive::IsIntegralType(input_type)) {
    switch (result_type) {
      case Primitive::kPrimChar:
        DCHECK_NE(input_type, Primitive::kPrimLong);
        __ Andi(out.AsRegister<Register>(), in.AsRegister<Register>(), 0xFFFF);
        break;
      case Primitive::kPrimByte:
        DCHECK_NE(input_type, Primitive::kPrimLong);
        __ Seb(out.AsRegister<Register>(), in.AsRegister<Register>());
        break;
      case Primitive::kPrimShort:
        DCHECK_NE(input_type, Primitive::kPrimLong);
        __ Seh(out.AsRegister<Register>(), in.AsRegister<Register>());
        break;
      case Primitive::kPrimInt:
        DCHECK_EQ(input_type, Primitive::kPrimLong);
        __ Move(out.AsRegister<Register>(), in.AsRegisterPairLow<Register>());
        break;
      case Primitive::kPrimLong:
        DCHECK_NE(input_type, Primitive::kPrimLong);
          __ Move(out.AsRegisterPairLow<Register>(), in.AsRegister<Register>());
          // Sign extension.
          __ Sra(out.AsRegisterPairHigh<Register>(), out.AsRegisterPairLow<Register>(), 31);
        break;

      default:
        LOG(FATAL) << "Unexpected type conversion from " << input_type
                   << " to " << result_type;
    }
  } else if (Primitive::IsFloatingPointType(result_type)
             && Primitive::IsIntegralType(input_type)) {
    if (input_type != Primitive::kPrimLong) {
      __ Mtc1(FTMP, in.AsRegister<Register>());
      if (result_type == Primitive::kPrimFloat) {
        __ Cvtsw(out.AsFpuRegister<FRegister>(), FTMP);
      } else {
        __ Cvtdw(FromLowSToD(out.AsFpuRegisterPairLow<FRegister>()), FTMP);
      }
    } else {
      int32_t entry_offset = (result_type == Primitive::kPrimFloat) ? QUICK_ENTRY_POINT(pL2f)
                                                                    : QUICK_ENTRY_POINT(pL2d);
      codegen_->InvokeRuntime(entry_offset,
                              conversion,
                              conversion->GetDexPc(),
                              nullptr);
      if (result_type == Primitive::kPrimFloat) {
        CheckEntrypointTypes<kQuickL2f, float, int64_t>();
      } else {
        CheckEntrypointTypes<kQuickL2d, double, int64_t>();
      }
    }
  } else if (Primitive::IsIntegralType(result_type) && Primitive::IsFloatingPointType(input_type)) {
    CHECK(result_type == Primitive::kPrimInt || result_type == Primitive::kPrimLong);
    int32_t entry_offset;
    if (result_type != Primitive::kPrimLong) {
      entry_offset = (input_type == Primitive::kPrimFloat) ? QUICK_ENTRY_POINT(pF2iz)
                                                           : QUICK_ENTRY_POINT(pD2iz);
    } else {
      entry_offset = (input_type == Primitive::kPrimFloat) ? QUICK_ENTRY_POINT(pF2l)
                                                           : QUICK_ENTRY_POINT(pD2l);
    }
    codegen_->InvokeRuntime(entry_offset,
                            conversion,
                            conversion->GetDexPc(),
                            nullptr);
    if (result_type != Primitive::kPrimLong) {
      if (input_type == Primitive::kPrimFloat) {
        CheckEntrypointTypes<kQuickF2iz, int32_t, float>();
      } else {
        CheckEntrypointTypes<kQuickD2iz, int32_t, double>();
      }
    } else {
      if (input_type == Primitive::kPrimFloat) {
        CheckEntrypointTypes<kQuickF2l, int64_t, float>();
      } else {
        CheckEntrypointTypes<kQuickD2l, int64_t, double>();
      }
    }
  } else if (Primitive::IsFloatingPointType(result_type) &&
             Primitive::IsFloatingPointType(input_type)) {
    if (result_type == Primitive::kPrimFloat) {
      __ Cvtsd(out.AsFpuRegister<FRegister>(),
               FromLowSToD(in.AsFpuRegisterPairLow<FRegister>()));
    } else {
      __ Cvtds(FromLowSToD(out.AsFpuRegisterPairLow<FRegister>()),
               in.AsFpuRegister<FRegister>());
    }
  } else {
    LOG(FATAL) << "Unexpected or unimplemented type conversion from " << input_type
                << " to " << result_type;
  }
}

void LocationsBuilderMIPS::VisitUShr(HUShr* ushr) {
  HandleShift(ushr);
}

void InstructionCodeGeneratorMIPS::VisitUShr(HUShr* ushr) {
  HandleShift(ushr);
}

void LocationsBuilderMIPS::VisitXor(HXor* instruction) {
  HandleBitwiseOperation(instruction);
}

void InstructionCodeGeneratorMIPS::VisitXor(HXor* instruction) {
  HandleBitwiseOperation(instruction);
}

void LocationsBuilderMIPS::VisitBoundType(HBoundType* instruction ATTRIBUTE_UNUSED) {
  // Nothing to do, this should be removed during prepare for register allocator.
  LOG(FATAL) << "Unreachable";
}

void InstructionCodeGeneratorMIPS::VisitBoundType(HBoundType* instruction ATTRIBUTE_UNUSED) {
  // Nothing to do, this should be removed during prepare for register allocator.
  LOG(FATAL) << "Unreachable";
}

void LocationsBuilderMIPS::VisitEqual(HEqual* comp) {
  VisitCondition(comp);
}

void InstructionCodeGeneratorMIPS::VisitEqual(HEqual* comp) {
  VisitCondition(comp);
}

void LocationsBuilderMIPS::VisitNotEqual(HNotEqual* comp) {
  VisitCondition(comp);
}

void InstructionCodeGeneratorMIPS::VisitNotEqual(HNotEqual* comp) {
  VisitCondition(comp);
}

void LocationsBuilderMIPS::VisitLessThan(HLessThan* comp) {
  VisitCondition(comp);
}

void InstructionCodeGeneratorMIPS::VisitLessThan(HLessThan* comp) {
  VisitCondition(comp);
}

void LocationsBuilderMIPS::VisitLessThanOrEqual(HLessThanOrEqual* comp) {
  VisitCondition(comp);
}

void InstructionCodeGeneratorMIPS::VisitLessThanOrEqual(HLessThanOrEqual* comp) {
  VisitCondition(comp);
}

void LocationsBuilderMIPS::VisitGreaterThan(HGreaterThan* comp) {
  VisitCondition(comp);
}

void InstructionCodeGeneratorMIPS::VisitGreaterThan(HGreaterThan* comp) {
  VisitCondition(comp);
}

void LocationsBuilderMIPS::VisitGreaterThanOrEqual(HGreaterThanOrEqual* comp) {
  VisitCondition(comp);
}

void InstructionCodeGeneratorMIPS::VisitGreaterThanOrEqual(HGreaterThanOrEqual* comp) {
  VisitCondition(comp);
}

void LocationsBuilderMIPS::VisitFakeString(HFakeString* instruction) {
  DCHECK(codegen_->IsBaseline());
  LocationSummary* locations =
      new (GetGraph()->GetArena()) LocationSummary(instruction, LocationSummary::kNoCall);
  locations->SetOut(Location::ConstantLocation(GetGraph()->GetNullConstant()));
}

void InstructionCodeGeneratorMIPS::VisitFakeString(HFakeString* instruction ATTRIBUTE_UNUSED) {
  DCHECK(codegen_->IsBaseline());
  // Will be generated at use site.
}

}  // namespace mips
}  // namespace art
