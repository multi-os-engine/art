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

#ifndef ART_COMPILER_OPTIMIZING_CODE_GENERATOR_MIPS_H_
#define ART_COMPILER_OPTIMIZING_CODE_GENERATOR_MIPS_H_

#include "code_generator.h"
#include "dex/compiler_enums.h"
#include "driver/compiler_options.h"
#include "nodes.h"
#include "parallel_move_resolver.h"
#include "utils/mips/assembler_mips.h"

namespace art {
namespace mips {

// Use a local definition to prevent copying mistakes.
static constexpr size_t kMipsWordSize = kMipsPointerSize;


// InvokeDexCallingConvention registers

static constexpr Register kParameterCoreRegisters[] = { A1, A2, A3 };
static constexpr size_t kParameterCoreRegistersLength = arraysize(kParameterCoreRegisters);

static constexpr FRegister kParameterFpuRegisters[] =
  { F12, F13, F14, F15 };  // TODO: review

static constexpr size_t kParameterFpuRegistersLength = arraysize(kParameterFpuRegisters);


// InvokeRuntimeCallingConvention registers

static constexpr Register kRuntimeParameterCoreRegisters[] = { A0, A1, A2, A3 };
static constexpr size_t kRuntimeParameterCoreRegistersLength =
    arraysize(kRuntimeParameterCoreRegisters);

static constexpr FRegister kRuntimeParameterFpuRegisters[] =
  { F12, F13, F14, F15 };  // TODO: review

static constexpr size_t kRuntimeParameterFpuRegistersLength =
    arraysize(kRuntimeParameterFpuRegisters);


static constexpr Register kCoreCalleeSaves[] =
    { S0, S1, S2, S3, S4, S5, S6, S7, GP, FP, RA };  // TODO: review
static constexpr FRegister kFpuCalleeSaves[] =
    { F20, F21, F22, F23, F24, F25, F26, F27, F28, F29, F30, F31 };


class CodeGeneratorMIPS;

class InvokeDexCallingConvention : public CallingConvention<Register, FRegister> {
 public:
  InvokeDexCallingConvention()
      : CallingConvention(kParameterCoreRegisters,
                          kParameterCoreRegistersLength,
                          kParameterFpuRegisters,
                          kParameterFpuRegistersLength,
                          kMipsPointerSize) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(InvokeDexCallingConvention);
};

class InvokeDexCallingConventionVisitorMIPS : public InvokeDexCallingConventionVisitor {
 public:
  InvokeDexCallingConventionVisitorMIPS() {}
  virtual ~InvokeDexCallingConventionVisitorMIPS() {}

  Location GetNextLocation(Primitive::Type type) OVERRIDE;
  Location GetReturnLocation(Primitive::Type type) const OVERRIDE;
  Location GetMethodLocation() const OVERRIDE;

 private:
  InvokeDexCallingConvention calling_convention;
  uint32_t double_index_ = 0;

  DISALLOW_COPY_AND_ASSIGN(InvokeDexCallingConventionVisitorMIPS);
};

class InvokeRuntimeCallingConvention : public CallingConvention<Register, FRegister> {
 public:
  InvokeRuntimeCallingConvention()
      : CallingConvention(kRuntimeParameterCoreRegisters,
                          kRuntimeParameterCoreRegistersLength,
                          kRuntimeParameterFpuRegisters,
                          kRuntimeParameterFpuRegistersLength,
                          kMipsPointerSize) {}

  Location GetReturnLocation(Primitive::Type return_type);

 private:
  DISALLOW_COPY_AND_ASSIGN(InvokeRuntimeCallingConvention);
};

class ParallelMoveResolverMIPS : public ParallelMoveResolverWithSwap {
 public:
  ParallelMoveResolverMIPS(ArenaAllocator* allocator, CodeGeneratorMIPS* codegen)
      : ParallelMoveResolverWithSwap(allocator), codegen_(codegen) {}

  void EmitMove(size_t index) OVERRIDE;
  void EmitSwap(size_t index) OVERRIDE;
  void SpillScratch(int reg) OVERRIDE;
  void RestoreScratch(int reg) OVERRIDE;

  // Exchange two (single) stack slots.
  void Exchange(int index1, int index2);

  MipsAssembler* GetAssembler() const;

 private:
  CodeGeneratorMIPS* const codegen_;

  DISALLOW_COPY_AND_ASSIGN(ParallelMoveResolverMIPS);
};

class SlowPathCodeMIPS : public SlowPathCode {
 public:
  SlowPathCodeMIPS() : entry_label_(), exit_label_() {}

  Label* GetEntryLabel() { return &entry_label_; }
  Label* GetExitLabel() { return &exit_label_; }

 private:
  Label entry_label_;
  Label exit_label_;

  DISALLOW_COPY_AND_ASSIGN(SlowPathCodeMIPS);
};

class LocationsBuilderMIPS : public HGraphVisitor {
 public:
  LocationsBuilderMIPS(HGraph* graph, CodeGeneratorMIPS* codegen)
      : HGraphVisitor(graph), codegen_(codegen) {}

#define DECLARE_VISIT_INSTRUCTION(name, super)     \
  void Visit##name(H##name* instr) OVERRIDE;

  FOR_EACH_CONCRETE_INSTRUCTION_COMMON(DECLARE_VISIT_INSTRUCTION)
  FOR_EACH_CONCRETE_INSTRUCTION_MIPS(DECLARE_VISIT_INSTRUCTION)

#undef DECLARE_VISIT_INSTRUCTION

  void VisitInstruction(HInstruction* instruction) OVERRIDE {
    LOG(FATAL) << "Unreachable instruction " << instruction->DebugName()
               << " (id " << instruction->GetId() << ")";
  }

 private:
  void HandleInvoke(HInvoke* invoke);
  void HandleBitwiseOperation(HBinaryOperation* operation);
  void HandleShift(HBinaryOperation* operation);
  void HandleFieldSet(HInstruction* instruction, const FieldInfo& field_info);
  void HandleFieldGet(HInstruction* instruction, const FieldInfo& field_info);

  InvokeDexCallingConventionVisitorMIPS parameter_visitor_;

  CodeGeneratorMIPS* const codegen_;

  DISALLOW_COPY_AND_ASSIGN(LocationsBuilderMIPS);
};

class InstructionCodeGeneratorMIPS : public HGraphVisitor {
 public:
  InstructionCodeGeneratorMIPS(HGraph* graph, CodeGeneratorMIPS* codegen);

#define DECLARE_VISIT_INSTRUCTION(name, super)     \
  void Visit##name(H##name* instr) OVERRIDE;

  FOR_EACH_CONCRETE_INSTRUCTION_COMMON(DECLARE_VISIT_INSTRUCTION)
  FOR_EACH_CONCRETE_INSTRUCTION_MIPS(DECLARE_VISIT_INSTRUCTION)

#undef DECLARE_VISIT_INSTRUCTION

  void VisitInstruction(HInstruction* instruction) OVERRIDE {
    LOG(FATAL) << "Unreachable instruction " << instruction->DebugName()
               << " (id " << instruction->GetId() << ")";
  }

  MipsAssembler* GetAssembler() const { return assembler_; }

 private:
  // Generate code for the given suspend check. If not null, `successor`
  // is the block to branch to if the suspend check is not needed, and after
  // the suspend call.
  void GenerateClassInitializationCheck(SlowPathCodeMIPS* slow_path, Register class_reg);
  void GenerateMemoryBarrier(MemBarrierKind kind);
  void GenerateSuspendCheck(HSuspendCheck* check, HBasicBlock* successor);
  void HandleBitwiseOperation(HBinaryOperation* operation);
  void HandleShift(HBinaryOperation* operation);
  void HandleFieldSet(HInstruction* instruction, const FieldInfo& field_info);
  void HandleFieldGet(HInstruction* instruction, const FieldInfo& field_info);
  void GenerateImplicitNullCheck(HNullCheck* instruction);
  void GenerateExplicitNullCheck(HNullCheck* instruction);
  void GenerateTestAndBranch(HInstruction* instruction,
                             Label* true_target,
                             Label* false_target,
                             Label* always_true_target);
  void HandleGoto(HInstruction* got, HBasicBlock* successor);

  MipsAssembler* const assembler_;
  CodeGeneratorMIPS* const codegen_;

  DISALLOW_COPY_AND_ASSIGN(InstructionCodeGeneratorMIPS);
};

class CodeGeneratorMIPS : public CodeGenerator {
 public:
  CodeGeneratorMIPS(HGraph* graph,
                      const MipsInstructionSetFeatures& isa_features,
                      const CompilerOptions& compiler_options);
  virtual ~CodeGeneratorMIPS() {}

  void GenerateFrameEntry() OVERRIDE;
  void GenerateFrameExit() OVERRIDE;

  void Bind(HBasicBlock* block) OVERRIDE;

  void Move(HInstruction* instruction, Location location, HInstruction* move_for) OVERRIDE;

  size_t GetWordSize() const OVERRIDE { return kMipsWordSize; }

  size_t GetFloatingPointSpillSlotSize() const OVERRIDE { return kMipsWordSize; }

  uintptr_t GetAddressOf(HBasicBlock* block) const OVERRIDE {
    return GetLabelOf(block)->Position();
  }

  HGraphVisitor* GetLocationBuilder() OVERRIDE { return &location_builder_; }
  HGraphVisitor* GetInstructionVisitor() OVERRIDE { return &instruction_visitor_; }
  MipsAssembler* GetAssembler() OVERRIDE { return &assembler_; }
  const MipsAssembler& GetAssembler() const OVERRIDE { return assembler_; }

  void MarkGCCard(Register object, Register value);

  // Register allocation.

  void SetupBlockedRegisters(bool is_baseline) const OVERRIDE;
  // AllocateFreeRegister() is only used when allocating registers locally
  // during CompileBaseline().
  Location AllocateFreeRegister(Primitive::Type type) const OVERRIDE;

  Location GetStackLocation(HLoadLocal* load) const OVERRIDE;

  size_t SaveCoreRegister(size_t stack_index, uint32_t reg_id);
  size_t RestoreCoreRegister(size_t stack_index, uint32_t reg_id);
  size_t SaveFloatingPointRegister(size_t stack_index, uint32_t reg_id);
  size_t RestoreFloatingPointRegister(size_t stack_index, uint32_t reg_id);

  void DumpCoreRegister(std::ostream& stream, int reg) const OVERRIDE;
  void DumpFloatingPointRegister(std::ostream& stream, int reg) const OVERRIDE;

  InstructionSet GetInstructionSet() const OVERRIDE { return InstructionSet::kMips; }

  const MipsInstructionSetFeatures& GetInstructionSetFeatures() const {
    return isa_features_;
  }

  Label* GetLabelOf(HBasicBlock* block) const {
    return CommonGetLabelOf<Label>(block_labels_.GetRawStorage(), block);
  }

  void Initialize() OVERRIDE {
    block_labels_.SetSize(GetGraph()->GetBlocks().Size());
  }

  void Finalize(CodeAllocator* allocator) OVERRIDE;

  // Code generation helpers.
  void Move32(Location destination, Location source);
  void Move64(Location destination, Location source);

  // Generate code to invoke a runtime entry point.
  void InvokeRuntime(int32_t offset,
                     HInstruction* instruction,
                     uint32_t dex_pc,
                     SlowPathCode* slow_path);

  ParallelMoveResolver* GetMoveResolver() OVERRIDE { return &move_resolver_; }

  bool NeedsTwoRegisters(Primitive::Type type) const OVERRIDE {
    return (type == Primitive::kPrimLong) || (type == Primitive::kPrimDouble);
  }

  void GenerateStaticOrDirectCall(HInvokeStaticOrDirect* invoke, Location temp);

 private:
  // Labels for each block that will be compiled.
  GrowableArray<Label> block_labels_;
  Label frame_entry_label_;
  LocationsBuilderMIPS location_builder_;
  InstructionCodeGeneratorMIPS instruction_visitor_;
  ParallelMoveResolverMIPS move_resolver_;
  MipsAssembler assembler_;
  const MipsInstructionSetFeatures& isa_features_;

  DISALLOW_COPY_AND_ASSIGN(CodeGeneratorMIPS);
};

}  // namespace mips
}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_CODE_GENERATOR_MIPS_H_
