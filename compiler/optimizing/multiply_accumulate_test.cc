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

#include "instruction_simplifier_arm.h"
#include "instruction_simplifier_arm64.h"

#include "optimizing_unit_test.h"

namespace art {

class MultiplyAccumulateTest: public testing::Test {
 public:
  MultiplyAccumulateTest(): pool_(), allocator_(&pool_) {
    graph_ = CreateGraph(&allocator_);
  }

  ~MultiplyAccumulateTest() {}

  HInstruction* CreateMulInBasicBlock(HBasicBlock* bb, HInstruction* param1,
                                      HInstruction* param2) {
    HMul* instr = new (&allocator_) HMul(param1->GetType(), param1, param2);
    bb->AddInstruction(instr);
    return instr;
  }

  HInstruction* CreateAddInBasicBlock(HBasicBlock* bb, HInstruction* param1,
                                      HInstruction* param2) {
    HAdd* instr = new (&allocator_) HAdd(param1->GetType(), param1, param2);
    bb->AddInstruction(instr);
    return instr;
  }

  HInstruction* CreateSubInBasicBlock(HBasicBlock* bb, HInstruction* param1,
                                      HInstruction* param2) {
    HSub* instr = new (&allocator_) HSub(param1->GetType(), param1, param2);
    bb->AddInstruction(instr);
    return instr;
  }

  HInstruction* CreateNegInBasicBlock(HBasicBlock* bb, HInstruction* param) {
    HNeg* instr = new (&allocator_) HNeg(param->GetType(), param);
    bb->AddInstruction(instr);
    return instr;
  }

  bool IsMadd(HInstruction* instr) {
    if (!instr->IsArm64MultiplyAccumulate()) {
      return false;
    }
    HArm64MultiplyAccumulate* mulacc = instr->AsArm64MultiplyAccumulate();
    return mulacc->GetOpKind() == HInstruction::kAdd;
  }

  bool IsMsub(HInstruction* instr) {
    if (!instr->IsArm64MultiplyAccumulate()) {
      return false;
    }
    HArm64MultiplyAccumulate* mulacc = instr->AsArm64MultiplyAccumulate();
    return mulacc->GetOpKind() == HInstruction::kSub;
  }

  bool IsMneg(HInstruction* instr) {
    if (!instr->IsArm64MultiplyAccumulate()) {
      return false;
    }
    HArm64MultiplyAccumulate* mulacc = instr->AsArm64MultiplyAccumulate();
    if (mulacc->GetOpKind() != HInstruction::kSub) {
      return false;
    }
    HInstruction* accumulator = mulacc->InputAt(HArm64MultiplyAccumulate::kInputAccumulatorIndex);
    return accumulator->IsConstant() && accumulator->AsConstant()->IsZero();
  }

  bool IsMla(HInstruction* instr) {
    if (!instr->IsArmMultiplyAccumulate()) {
      return false;
    }
    HArmMultiplyAccumulate* mulacc = instr->AsArmMultiplyAccumulate();
    return mulacc->GetOpKind() == HInstruction::kAdd;
  }

  bool IsMls(HInstruction* instr) {
    if (!instr->IsArmMultiplyAccumulate()) {
      return false;
    }
    HArmMultiplyAccumulate* mulacc = instr->AsArmMultiplyAccumulate();
    return mulacc->GetOpKind() == HInstruction::kSub;
  }

  ArenaPool pool_;
  ArenaAllocator allocator_;
  HGraph* graph_ = CreateGraph(&allocator_);
};

TEST_F(MultiplyAccumulateTest, arm64) {
  HBasicBlock* bb = new (&allocator_) HBasicBlock(graph_);
  graph_->AddBlock(bb);
  graph_->SetEntryBlock(bb);
  graph_->SetExitBlock(bb);
  auto param = new (&allocator_) HParameterValue(graph_->GetDexFile(), 0, 0,
                                                 Primitive::kPrimInt);
  bb->AddInstruction(param);
  // Shall be combined.
  auto mul1 = CreateMulInBasicBlock(bb, param, param);
  CreateAddInBasicBlock(bb, mul1, param);
  auto mul2 = CreateMulInBasicBlock(bb, param, param);
  CreateAddInBasicBlock(bb, param, mul2);
  // Shall not be combined.
  auto mul3 = CreateMulInBasicBlock(bb, param, param);
  CreateAddInBasicBlock(bb, mul3, mul3);
  CreateMulInBasicBlock(bb, param, param);
  CreateAddInBasicBlock(bb, param, param);
  // Shall be combined.
  auto mul5 = CreateMulInBasicBlock(bb, param, param);
  CreateSubInBasicBlock(bb, param, mul5);
  // Shall not be combined.
  auto mul6 = CreateMulInBasicBlock(bb, param, param);
  CreateSubInBasicBlock(bb, mul6, param);
  auto mul7 = CreateMulInBasicBlock(bb, param, param);
  CreateSubInBasicBlock(bb, mul7, mul7);
  CreateMulInBasicBlock(bb, param, param);
  CreateSubInBasicBlock(bb, param, param);
  // Shall be combined.
  auto mul8 = CreateMulInBasicBlock(bb, param, param);
  CreateNegInBasicBlock(bb, mul8);
  // Shall not be combined.
  CreateMulInBasicBlock(bb, param, param);
  CreateNegInBasicBlock(bb, param);

  graph_->TryBuildingSsa();
  arm64::InstructionSimplifierArm64 simplifier(graph_);
  simplifier.Run();

  int i = 0;
  for (HInstructionIterator iter(bb->GetInstructions()); !iter.Done();
       iter.Advance(), i++) {
    auto instr = iter.Current();
    switch (i) {
      case 0: EXPECT_TRUE(instr->IsParameterValue()); break;
      case 1: EXPECT_TRUE(IsMadd(instr)); break;
      case 2: EXPECT_TRUE(IsMadd(instr)); break;
      case 3: EXPECT_TRUE(instr->IsMul()); break;
      case 4: EXPECT_TRUE(instr->IsAdd()); break;
      case 5: EXPECT_TRUE(instr->IsMul()); break;
      case 6: EXPECT_TRUE(instr->IsAdd()); break;
      case 7: EXPECT_TRUE(IsMsub(instr)); break;
      case 8: EXPECT_TRUE(instr->IsMul()); break;
      case 9: EXPECT_TRUE(instr->IsSub()); break;
      case 10: EXPECT_TRUE(instr->IsMul()); break;
      case 11: EXPECT_TRUE(instr->IsSub()); break;
      case 12: EXPECT_TRUE(instr->IsMul()); break;
      case 13: EXPECT_TRUE(instr->IsSub()); break;
      case 14: EXPECT_TRUE(IsMneg(instr)); break;
      case 15: EXPECT_TRUE(instr->IsMul()); break;
      case 16: EXPECT_TRUE(instr->IsNeg()); break;
      case 17: EXPECT_TRUE(instr->IsConstant()); break;  // Constant added for Mneg.
      default: EXPECT_TRUE(false); break;
    }
  }
  EXPECT_EQ(18, i);
}

TEST_F(MultiplyAccumulateTest, arm) {
  HBasicBlock* bb = new (&allocator_) HBasicBlock(graph_);
  graph_->AddBlock(bb);
  graph_->SetEntryBlock(bb);
  graph_->SetExitBlock(bb);
  auto param = new (&allocator_) HParameterValue(graph_->GetDexFile(), 0, 0,
                                                 Primitive::kPrimInt);
  bb->AddInstruction(param);
  // Shall be combined.
  auto mul1 = CreateMulInBasicBlock(bb, param, param);
  CreateAddInBasicBlock(bb, mul1, param);
  auto mul2 = CreateMulInBasicBlock(bb, param, param);
  CreateAddInBasicBlock(bb, param, mul2);
  // Shall not be combined.
  auto mul3 = CreateMulInBasicBlock(bb, param, param);
  CreateAddInBasicBlock(bb, mul3, mul3);
  CreateMulInBasicBlock(bb, param, param);
  CreateAddInBasicBlock(bb, param, param);
  // Shall be combined.
  auto mul5 = CreateMulInBasicBlock(bb, param, param);
  CreateSubInBasicBlock(bb, param, mul5);
  // Shall not be combined.
  auto mul6 = CreateMulInBasicBlock(bb, param, param);
  CreateSubInBasicBlock(bb, mul6, param);
  auto mul7 = CreateMulInBasicBlock(bb, param, param);
  CreateSubInBasicBlock(bb, mul7, mul7);
  CreateMulInBasicBlock(bb, param, param);
  CreateSubInBasicBlock(bb, param, param);

  graph_->TryBuildingSsa();
  arm::InstructionSimplifierArm simplifier(graph_);
  simplifier.Run();

  int i = 0;
  for (HInstructionIterator iter(bb->GetInstructions()); !iter.Done();
       iter.Advance(), i++) {
    auto instr = iter.Current();
    switch (i) {
      case 0: EXPECT_TRUE(instr->IsParameterValue()); break;
      case 1: EXPECT_TRUE(IsMla(instr)); break;
      case 2: EXPECT_TRUE(IsMla(instr)); break;
      case 3: EXPECT_TRUE(instr->IsMul()); break;
      case 4: EXPECT_TRUE(instr->IsAdd()); break;
      case 5: EXPECT_TRUE(instr->IsMul()); break;
      case 6: EXPECT_TRUE(instr->IsAdd()); break;
      case 7: EXPECT_TRUE(IsMls(instr)); break;
      case 8: EXPECT_TRUE(instr->IsMul()); break;
      case 9: EXPECT_TRUE(instr->IsSub()); break;
      case 10: EXPECT_TRUE(instr->IsMul()); break;
      case 11: EXPECT_TRUE(instr->IsSub()); break;
      case 12: EXPECT_TRUE(instr->IsMul()); break;
      case 13: EXPECT_TRUE(instr->IsSub()); break;
      default: EXPECT_TRUE(false); break;
    }
  }
  EXPECT_EQ(14, i);
}

}  // namespace art
