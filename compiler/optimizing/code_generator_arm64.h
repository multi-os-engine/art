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

#ifndef ART_COMPILER_OPTIMIZING_CODE_GENERATOR_ARM64_H_
#define ART_COMPILER_OPTIMIZING_CODE_GENERATOR_ARM64_H_

#include "code_generator.h"
#include "nodes.h"
#include "parallel_move_resolver.h"
#include "utils/arm64/assembler_arm64.h"
#include "a64/disasm-a64.h"
#include "a64/macro-assembler-a64.h"

using namespace vixl;   // NOLINT(build/namespaces)

namespace art {
namespace arm64 {

class CodeGeneratorARM64;

static constexpr size_t kArm64WordSize = 8;
static const vixl::Register kParameterCoreRegisters[] = { x0, x1, x2, x3, x4, x5, x6, x7 };
static constexpr size_t kParameterCoreRegistersLength = arraysize(kParameterCoreRegisters);
static const vixl::FPRegister kParameterCoreFPRegisters[] = { d0, d1, d2, d3, d4, d5, d6, d7 };
static constexpr size_t kParameterCoreFPRegistersLength = arraysize(kParameterCoreFPRegisters);

const vixl::Register tr = x18;        // Thread Register
const vixl::Register wSuspend = w19;  // Suspend Register
const vixl::Register xSuspend = x19;

const CPURegList vixl_reserved_core_registers(ip0, ip1);
const CPURegList runtime_reserved_core_registers(tr, xSuspend, lr);

// TODO: expand this for FP regs
//
// CallingConventionARM64:
//  * Argument registers: [r0, r7]: 64bit args X[n], 32bit args W[n]
//  * Return register: r0: 64bit return X0, 32bit return W0
class CallingConventionARM64 : public CallingConvention<vixl::Register, vixl::FPRegister> {
 public:
  CallingConventionARM64() :
    CallingConvention(kParameterCoreRegisters, kParameterCoreRegistersLength,
                      kParameterCoreFPRegisters, kParameterCoreFPRegistersLength),
    gp_index_(0), stack_index_(0) {}

  Location GetNextLocation(Primitive::Type type);
  void SetReturnLocation(LocationSummary* locations, Primitive::Type ret_type);

  // Custom GetStackOffsetOf() that does not include ArtMethod*
  uint8_t GetStackOffsetOfARM64(size_t index) const {
    return index * kVRegSize;
  }

 private:
  uint32_t gp_index_;
  uint32_t stack_index_;

  DISALLOW_COPY_AND_ASSIGN(CallingConventionARM64);
};

class InstructionCodeGeneratorARM64 : public HGraphVisitor {
 public:
  InstructionCodeGeneratorARM64(HGraph* graph, CodeGeneratorARM64* codegen);

#define DECLARE_VISIT_INSTRUCTION(name, super) \
  virtual void Visit##name(H##name* instr);
  FOR_EACH_CONCRETE_INSTRUCTION(DECLARE_VISIT_INSTRUCTION)
#undef DECLARE_VISIT_INSTRUCTION

  void LoadCurrentMethod(XRegister reg);

  Arm64Assembler* GetAssembler() const { return assembler_; }

 private:
  void HandleAddSub(HBinaryOperation* instr);

  Arm64Assembler* const assembler_;
  CodeGeneratorARM64* const codegen_;

  DISALLOW_COPY_AND_ASSIGN(InstructionCodeGeneratorARM64);
};

class LocationsBuilderARM64 : public HGraphVisitor {
 public:
  explicit LocationsBuilderARM64(HGraph* graph, CodeGeneratorARM64* codegen)
      : HGraphVisitor(graph), codegen_(codegen) {}

#define DECLARE_VISIT_INSTRUCTION(name, super) \
  virtual void Visit##name(H##name* instr);
  FOR_EACH_CONCRETE_INSTRUCTION(DECLARE_VISIT_INSTRUCTION)
#undef DECLARE_VISIT_INSTRUCTION

 private:
  void HandleAddSub(HBinaryOperation* instr);
  void HandleInvoke(HInvoke* instr);

  CodeGeneratorARM64* const codegen_;
  CallingConventionARM64 parameter_visitor_;

  DISALLOW_COPY_AND_ASSIGN(LocationsBuilderARM64);
};

class CodeGeneratorARM64 : public CodeGenerator {
 public:
  explicit CodeGeneratorARM64(HGraph* graph);
  virtual ~CodeGeneratorARM64() { }

  virtual void GenerateFrameEntry() OVERRIDE;
  virtual void GenerateFrameExit() OVERRIDE;

  static const vixl::CPURegList& GetFramePreservedRegisters() {
    using vixl::CPURegList;
    using vixl::CPURegister;
    using vixl::kXRegSize;
    using vixl::lr;
    static const CPURegList frame_preserved_regs =
        CPURegList(CPURegister::kRegister, kXRegSize, lr.Bit());
    return frame_preserved_regs;
  }
  static int GetFramePreservedRegistersSize() {
    return GetFramePreservedRegisters().TotalSizeInBytes();
  }

  virtual void Bind(Label* label) OVERRIDE;
  virtual void Move(HInstruction* instruction, Location location, HInstruction* move_for) OVERRIDE;

  virtual size_t GetWordSize() const OVERRIDE {
    return kArm64WordSize;
  }

  virtual size_t FrameEntrySpillSize() const OVERRIDE;

  virtual HGraphVisitor* GetLocationBuilder() OVERRIDE { return &location_builder_; }
  virtual HGraphVisitor* GetInstructionVisitor() OVERRIDE { return &instruction_visitor_; }
  virtual Arm64Assembler* GetAssembler() OVERRIDE { return &assembler_; }

  // Emit a write barrier.
  void MarkGCCard(vixl::Register object, vixl::Register value);

  // Register allocation.

  virtual void SetupBlockedRegisters() const OVERRIDE;
  // AllocateFreeRegister() is only used when allocating registers locally
  // during CompileBaseline().
  virtual Location AllocateFreeRegister(Primitive::Type type) const OVERRIDE;

  virtual Location GetStackLocation(HLoadLocal* load) const OVERRIDE;

  void SaveCoreRegister(Location stack_location, uint32_t reg_id) {
    UNIMPLEMENTED(INFO) << "TODO: SaveCoreRegister";
  }

  void RestoreCoreRegister(Location stack_location, uint32_t reg_id) {
    UNIMPLEMENTED(INFO) << "TODO: RestoreCoreRegister";
  }

  // The number of registers that can be allocated. The register allocator may
  // decide to reserve and not use a few of them.
  // We do not consider registers sp, xzr, wzr. They are either not allocatable
  // (xzr, wzr), or make for poor allocatable registers (sp alignment
  // requirements, etc.). This also facilitates our task as all other registers
  // can easily be mapped via to or from their type and index or code.
  static const int kNumberOfAllocatableCoreRegisters = vixl::kNumberOfRegisters - 1;
  static const int kNumberOfAllocatableFloatingPointRegisters = vixl::kNumberOfFPRegisters;
  static const int kNumberOfAllocatableRegisters =
      kNumberOfAllocatableCoreRegisters + kNumberOfAllocatableFloatingPointRegisters;

  virtual void DumpCoreRegister(std::ostream& stream, int reg) const OVERRIDE;
  virtual void DumpFloatingPointRegister(std::ostream& stream, int reg) const OVERRIDE;

  virtual InstructionSet GetInstructionSet() const OVERRIDE {
    return InstructionSet::kArm64;
  }

  void MoveHelper(Location destination, Location source, Primitive::Type type);

 private:
  LocationsBuilderARM64 location_builder_;
  InstructionCodeGeneratorARM64 instruction_visitor_;
  Arm64Assembler assembler_;

  DISALLOW_COPY_AND_ASSIGN(CodeGeneratorARM64);
};

}  // namespace arm64
}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_CODE_GENERATOR_ARM64_H_
