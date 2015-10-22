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

#include <memory>
#include <vector>

#include "arch/instruction_set.h"
#include "cfi_test.h"
#include "gtest/gtest.h"
#include "optimizing/code_generator.h"
#include "optimizing/optimizing_unit_test.h"
#include "utils/assembler.h"
#include "utils/arm/assembler_thumb2.h"

#include "optimizing/optimizing_cfi_test_expected.inc"

namespace art {

// Run the tests only on host.
#ifndef __ANDROID__

class OptimizingCFITest : public CFITest {
 public:
  // Enable this flag to generate the expected outputs.
  static constexpr bool kGenerateExpected = false;

  OptimizingCFITest()
      : pool_(),
        allocator_(&pool_),
        opts_(),
        isa_features_(),
        graph_(nullptr),
        code_gen_(),
        blocks_(allocator_.Adapter()) {}

  void SetUpFrame(InstructionSet isa) {
    // Setup simple context.
    std::string error;
    isa_features_.reset(InstructionSetFeatures::FromVariant(isa, "default", &error));
    graph_ = CreateGraph(&allocator_);
    // Generate simple frame with some spills.
    code_gen_.reset(CodeGenerator::Create(graph_, isa, *isa_features_, opts_));
    code_gen_->GetAssembler()->cfi().SetEnabled(true);
    const int frame_size = 64;
    int core_reg = 0;
    int fp_reg = 0;
    for (int i = 0; i < 2; i++) {  // Two registers of each kind.
      for (; core_reg < 32; core_reg++) {
        if (code_gen_->IsCoreCalleeSaveRegister(core_reg)) {
          auto location = Location::RegisterLocation(core_reg);
          code_gen_->AddAllocatedRegister(location);
          core_reg++;
          break;
        }
      }
      for (; fp_reg < 32; fp_reg++) {
        if (code_gen_->IsFloatingPointCalleeSaveRegister(fp_reg)) {
          auto location = Location::FpuRegisterLocation(fp_reg);
          code_gen_->AddAllocatedRegister(location);
          fp_reg++;
          break;
        }
      }
    }
    code_gen_->block_order_ = &blocks_;
    code_gen_->ComputeSpillMask();
    code_gen_->SetFrameSize(frame_size);
    code_gen_->GenerateFrameEntry();
  }

  void Finish() {
    code_gen_->GenerateFrameExit();
    code_gen_->Finalize(&code_allocator_);
  }

  void Check(InstructionSet isa,
             const char* isa_str,
             const std::vector<uint8_t>& expected_asm,
             const std::vector<uint8_t>& expected_cfi) {
    // Get the outputs.
    const std::vector<uint8_t>& actual_asm = code_allocator_.GetMemory();
    Assembler* opt_asm = code_gen_->GetAssembler();
    const std::vector<uint8_t>& actual_cfi = *(opt_asm->cfi().data());

    if (kGenerateExpected) {
      GenerateExpected(stdout, isa, isa_str, actual_asm, actual_cfi);
    } else {
      EXPECT_EQ(expected_asm, actual_asm);
      EXPECT_EQ(expected_cfi, actual_cfi);
    }
  }

  void TestImpl(InstructionSet isa, const char*
                isa_str,
                const std::vector<uint8_t>& expected_asm,
                const std::vector<uint8_t>& expected_cfi) {
    SetUpFrame(isa);
    Finish();
    Check(isa, isa_str, expected_asm, expected_cfi);
  }

  CodeGenerator* GetCodeGenerator() {
    return code_gen_.get();
  }

 private:
  class InternalCodeAllocator : public CodeAllocator {
   public:
    InternalCodeAllocator() {}

    virtual uint8_t* Allocate(size_t size) {
      memory_.resize(size);
      return memory_.data();
    }

    const std::vector<uint8_t>& GetMemory() { return memory_; }

   private:
    std::vector<uint8_t> memory_;

    DISALLOW_COPY_AND_ASSIGN(InternalCodeAllocator);
  };

  ArenaPool pool_;
  ArenaAllocator allocator_;
  CompilerOptions opts_;
  std::unique_ptr<const InstructionSetFeatures> isa_features_;
  HGraph* graph_;
  std::unique_ptr<CodeGenerator> code_gen_;
  ArenaVector<HBasicBlock*> blocks_;
  InternalCodeAllocator code_allocator_;
};

#define TEST_ISA(isa)                                         \
  TEST_F(OptimizingCFITest, isa) {                            \
    std::vector<uint8_t> expected_asm(                        \
        expected_asm_##isa,                                   \
        expected_asm_##isa + arraysize(expected_asm_##isa));  \
    std::vector<uint8_t> expected_cfi(                        \
        expected_cfi_##isa,                                   \
        expected_cfi_##isa + arraysize(expected_cfi_##isa));  \
    TestImpl(isa, #isa, expected_asm, expected_cfi);          \
  }

TEST_ISA(kThumb2)
TEST_ISA(kArm64)
TEST_ISA(kX86)
TEST_ISA(kX86_64)

TEST_F(OptimizingCFITest, kThumb2Adjust) {
  std::vector<uint8_t> expected_asm(
      expected_asm_kThumb2_adjust,
      expected_asm_kThumb2_adjust + arraysize(expected_asm_kThumb2_adjust));
  std::vector<uint8_t> expected_cfi(
      expected_cfi_kThumb2_adjust,
      expected_cfi_kThumb2_adjust + arraysize(expected_cfi_kThumb2_adjust));
  SetUpFrame(kThumb2);
#define __ down_cast<arm::Thumb2Assembler*>(GetCodeGenerator()->GetAssembler())->
  Label target;
  __ CompareAndBranchIfZero(arm::R0, &target);
  // Push the target out of range of CBZ.
  for (size_t i = 0; i != 65; ++i) {
    __ ldr(arm::R0, arm::Address(arm::R0));
  }
  __ Bind(&target);
#undef __
  Finish();
  Check(kThumb2, "kThumb2_adjust", expected_asm, expected_cfi);
}

#endif  // __ANDROID__

}  // namespace art
