/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "arch/arm/instruction_set_features_arm.h"
#include "arch/arm64/instruction_set_features_arm64.h"
#include "arch/mips/instruction_set_features_mips.h"
#include "arch/mips64/instruction_set_features_mips64.h"
#include "arch/x86/instruction_set_features_x86.h"
#include "arch/x86_64/instruction_set_features_x86_64.h"
#include "base/arena_allocator.h"
#include "base/time_utils.h"
#include "builder.h"
#include "code_generator_arm.h"
#include "code_generator_arm64.h"
#include "code_generator_x86_64.h"
#include "code_generator_x86.h"
#include "common_compiler_test.h"
#include "nodes.h"
#include "optimizing_unit_test.h"
#include "pc_relative_fixups_x86.h"
#include "register_allocator.h"
#include "scheduler_arm64.h"

namespace art {

class SchedulerTest : public CommonCompilerTest {};

TEST_F(SchedulerTest, DependencyGraph) {
  ArenaPool pool;
  ArenaAllocator allocator(&pool);
  HGraph* graph = CreateGraph(&allocator);
  HBasicBlock* entry = new (&allocator) HBasicBlock(graph);
  HBasicBlock* block1 = new (&allocator) HBasicBlock(graph);
  graph->AddBlock(entry);
  graph->AddBlock(block1);
  graph->SetEntryBlock(entry);

  // entry:
  // array         ParameterValue
  // c1            IntConstant
  // c2            IntConstant
  // block1:
  // add1          Add [c1, c2]
  // add2          Add [add1, c2]
  // mul           Mul [add1, add2]
  // div_check     DivZeroCheck [add2] (env: add2, mul)
  // div           Div [add1, div_check]
  // array_get     ArrayGet [array, add1]
  // array_set     ArraySet [array, add1, add2]

  HInstruction* array = new (&allocator) HParameterValue(graph->GetDexFile(),
                                                         0,
                                                         0,
                                                         Primitive::kPrimNot);
  HInstruction* c1 = graph->GetIntConstant(1);
  HInstruction* c2 = graph->GetIntConstant(10);
  HInstruction* add1 = new (&allocator) HAdd(Primitive::kPrimInt, c1, c2);
  HInstruction* add2 = new (&allocator) HAdd(Primitive::kPrimInt, add1, c2);
  HInstruction* mul = new (&allocator) HMul(Primitive::kPrimInt, add1, add2);
  HInstruction* div_check = new (&allocator) HDivZeroCheck(add2, 0);
  HInstruction* div = new (&allocator) HDiv(Primitive::kPrimInt, add1, div_check, 0);
  HInstruction* array_get = new (&allocator) HArrayGet(array, add1, Primitive::kPrimInt, 0);
  HInstruction* array_set = new (&allocator) HArraySet(array, add1, add2, Primitive::kPrimInt, 0);

  DCHECK(div_check->CanThrow());

  entry->AddInstruction(array);

  std::vector<HInstruction*> block_instructions = {add1,
                                                   add2,
                                                   mul,
                                                   div_check,
                                                   div,
                                                   array_get,
                                                   array_set};
  for (auto instr : block_instructions) {
    block1->AddInstruction(instr);
  }

  HEnvironment* environment = new (&allocator) HEnvironment(&allocator,
                                                            2,
                                                            graph->GetDexFile(),
                                                            graph->GetMethodIdx(),
                                                            0,
                                                            kStatic,
                                                            div_check);
  div_check->SetRawEnvironment(environment);
  environment->SetRawEnvAt(0, add2);
  add2->AddEnvUseAt(div_check->GetEnvironment(), 0);
  environment->SetRawEnvAt(1, mul);
  mul->AddEnvUseAt(div_check->GetEnvironment(), 1);

  ArenaAllocator* arena = graph->GetArena();
  arm64::HArm64Scheduler scheduler(arena);
  SchedulingGraph scheduling_graph(&scheduler, arena);
  // Instructions must be inserted in reverse order into the graph.
  for (std::vector<HInstruction*>::reverse_iterator it(block_instructions.rbegin());
       it != block_instructions.rend();
       it++) {
    scheduling_graph.AddNode(*it);
  }

  ASSERT_FALSE(scheduling_graph.HasImmediateDataDependency(add1, c1));
  ASSERT_FALSE(scheduling_graph.HasImmediateDataDependency(add2, c2));

  // Define-use dependency.
  ASSERT_TRUE(scheduling_graph.HasImmediateDataDependency(add2, add1));
  ASSERT_FALSE(scheduling_graph.HasImmediateDataDependency(add1, add2));
  ASSERT_TRUE(scheduling_graph.HasImmediateDataDependency(div_check, add2));
  ASSERT_FALSE(scheduling_graph.HasImmediateDataDependency(div_check, add1));
  ASSERT_TRUE(scheduling_graph.HasImmediateDataDependency(div, div_check));
  ASSERT_TRUE(scheduling_graph.HasImmediateDataDependency(array_set, add1));
  ASSERT_TRUE(scheduling_graph.HasImmediateDataDependency(array_set, add2));

  // Read-write dependency.
  ASSERT_TRUE(scheduling_graph.HasImmediateOtherDependency(array_set, array_get));

  // Env dependency.
  ASSERT_TRUE(scheduling_graph.HasImmediateOtherDependency(div_check, mul));
  ASSERT_FALSE(scheduling_graph.HasImmediateOtherDependency(mul, div_check));

  // CanThrow.
  ASSERT_TRUE(scheduling_graph.HasImmediateOtherDependency(array_set, div_check));
}

class InternalCodeAllocator : public CodeAllocator {
 public:
  InternalCodeAllocator() : size_(0) {}

  virtual uint8_t* Allocate(size_t size) {
    size_ = size;
    memory_.reset(new uint8_t[size]);
    return memory_.get();
  }

  size_t GetSize() const { return size_; }
  uint8_t* GetMemory() const { return memory_.get(); }

 private:
  size_t size_;
  std::unique_ptr<uint8_t[]> memory_;

  DISALLOW_COPY_AND_ASSIGN(InternalCodeAllocator);
};

template <typename Expected>
static void RunCode(const InternalCodeAllocator& allocator,
                    const CodeGenerator* codegen,
                    bool has_result,
                    Expected expected) {
  typedef Expected (*fptr)();
  CommonCompilerTest::MakeExecutable(allocator.GetMemory(), allocator.GetSize());
  fptr f = reinterpret_cast<fptr>(allocator.GetMemory());
  if (codegen->GetInstructionSet() == kThumb2) {
    // For thumb we need the bottom bit set.
    f = reinterpret_cast<fptr>(reinterpret_cast<uintptr_t>(f) + 1);
  }
  Expected result = f();
  if (has_result) {
    ASSERT_EQ(expected, result);
  }
}

// Get default runtime ISA instruction features.
static std::unique_ptr<const InstructionSetFeatures> GetDefaultInstructionSetFeatures() {
  std::unique_ptr<const InstructionSetFeatures> instruction_set_features;
  // Currently only x86 and arm are supported.
  switch (kRuntimeISA) {
    case kArm64:
      instruction_set_features.reset(Arm64InstructionSetFeatures::FromCppDefines());
      break;
    case kArm:
    case kThumb2:
      instruction_set_features.reset(ArmInstructionSetFeatures::FromCppDefines());
      break;
    case kX86_64:
      instruction_set_features.reset(X86_64InstructionSetFeatures::FromCppDefines());
      break;
    case kX86:
      instruction_set_features.reset(X86InstructionSetFeatures::FromCppDefines());
      break;
    case kMips:
      instruction_set_features.reset(MipsInstructionSetFeatures::FromCppDefines());
      break;
    case kMips64:
      instruction_set_features.reset(Mips64InstructionSetFeatures::FromCppDefines());
      break;
    default:
      break;
  }
  return instruction_set_features;
}

// Create code generator based on given instruction set and instruction features.
static std::unique_ptr<CodeGenerator> CreateTestCodegen(
    HGraph* graph,
    CompilerOptions& compiler_options,
    InstructionSet instruction_set,
    const InstructionSetFeatures* isa_features,
    ArenaAllocator* arena) {
  // Currently only x86 and arm are supported.
  switch (instruction_set) {
    case kArm64:
    case kX86_64:
      return std::unique_ptr<CodeGenerator>(
          CodeGenerator::Create(graph,
                                kRuntimeISA,
                                *isa_features,
                                compiler_options));
    case kArm:
    case kThumb2:
      return std::unique_ptr<CodeGenerator>(
          new(arena) TestCodeGeneratorARM(graph,
                                          *isa_features->AsArmInstructionSetFeatures(),
                                          compiler_options));
    case kX86:
      return std::unique_ptr<CodeGenerator>(
          new(arena) TestCodeGeneratorX86(graph,
                                          *isa_features->AsX86InstructionSetFeatures(),
                                          compiler_options));
    default:
      return nullptr;
  }
}

template <typename Expected>
static void CompileWithRandomSchedulerAndRun(const uint16_t* data,
                                             bool has_result,
                                             Expected expected) {
  ArenaPool pool;
  ArenaAllocator arena(&pool);
  CompilerOptions compiler_options;
  HGraph* graph = CreateCFG(&arena, data);
  std::unique_ptr<const InstructionSetFeatures> isa_features = GetDefaultInstructionSetFeatures();
  std::unique_ptr<CodeGenerator> codegen = CreateTestCodegen(graph,
                                                             compiler_options,
                                                             kRuntimeISA,
                                                             isa_features.get(),
                                                             &arena);

  // Remove suspend checks, they cannot be executed in this context.
  RemoveSuspendChecks(graph);

  if (kRuntimeISA == kX86) {
    // Workaround to make x86 codegen happy.
    OptimizingCompilerStats stats;
    x86::PcRelativeFixups pc_relative_fixups(graph, codegen.get(), &stats);
    pc_relative_fixups.Run();
  }

  // Run random scheduler.
  arm64::HArm64Scheduler scheduler(graph->GetArena());
  scheduler.SetOptimizeLoopOnly(false);
  RandomSchedulingNodeSelector selector(static_cast<int>(NanoTime()));
  scheduler.SetSelector(&selector);
  scheduler.Schedule(graph);

  SsaLivenessAnalysis liveness(graph, codegen.get());
  liveness.Analyze();

  RegisterAllocator register_allocator(graph->GetArena(), codegen.get(), liveness);
  register_allocator.AllocateRegisters();

  InternalCodeAllocator allocator;
  codegen->Compile(&allocator);

  // Execute the generated code and check the return value.
  RunCode(allocator, codegen.get(), has_result, expected);
}

TEST_F(SchedulerTest, RandomScheduling) {
  //
  // Java source: crafted code to make sure (random) scheduling should get correct result.
  //
  //  int result = 0;
  //  float fr = 10.0f;
  //  for (int i = 1; i < 10; i++) {
  //    fr ++;
  //    int t1 = result >> i;
  //    int t2 = result * i;
  //    result = result + t1 - t2;
  //    fr = fr / i;
  //    result += (int)fr;
  //  }
  //  return result;
  //
  const uint16_t data[] = SIX_REGISTERS_CODE_ITEM(
    Instruction::CONST_4 | 0 << 12 | 2 << 8,          // const/4 v2, #int 0
    Instruction::CONST_HIGH16 | 0 << 8, 0x4120,       // const/high16 v0, #float 10.0 // #41200000
    Instruction::CONST_4 | 1 << 12 | 1 << 8,          // const/4 v1, #int 1
    Instruction::CONST_16 | 5 << 8, 0x000a,           // const/16 v5, #int 10
    Instruction::IF_GE | 5 << 12 | 1 << 8, 0x0014,    // if-ge v1, v5, 001a // +0014
    Instruction::CONST_HIGH16 | 5 << 8, 0x3f80,       // const/high16 v5, #float 1.0 // #3f800000
    Instruction::ADD_FLOAT_2ADDR | 5 << 12 | 0 << 8,  // add-float/2addr v0, v5
    Instruction::SHR_INT | 3 << 8, 1 << 8 | 2 ,       // shr-int v3, v2, v1
    Instruction::MUL_INT | 4 << 8, 1 << 8 | 2,        // mul-int v4, v2, v1
    Instruction::ADD_INT | 5 << 8, 3 << 8 | 2,        // add-int v5, v2, v3
    Instruction::SUB_INT | 2 << 8, 4 << 8 | 5,        // sub-int v2, v5, v4
    Instruction::INT_TO_FLOAT | 1 << 12 | 5 << 8,     // int-to-float v5, v1
    Instruction::DIV_FLOAT_2ADDR | 5 << 12 | 0 << 8,  // div-float/2addr v0, v5
    Instruction::FLOAT_TO_INT | 0 << 12 | 5 << 8,     // float-to-int v5, v0
    Instruction::ADD_INT_2ADDR | 5 << 12 | 2 << 8,    // add-int/2addr v2, v5
    Instruction::ADD_INT_LIT8 | 1 << 8, 1 << 8 | 1,   // add-int/lit8 v1, v1, #int 1 // #01
    Instruction::GOTO | 0xeb << 8,                    // goto 0004 // -0015
    Instruction::RETURN | 2 << 8);                    // return v2

  constexpr int NUM_OF_RUNS = 10;
  for (int i = 0; i < NUM_OF_RUNS; ++i) {
    CompileWithRandomSchedulerAndRun(data, true, 138774);
  }
}

}  // namespace art
