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

#include "base/arena_allocator.h"
#include "bounds_check_elimination.h"
#include "builder.h"
#include "gvn.h"
#include "induction_var_analysis.h"
#include "instruction_simplifier.h"
#include "nodes.h"
#include "optimizing_unit_test.h"
#include "side_effects_analysis.h"

#include "gtest/gtest.h"

namespace art {

/**
 * Fixture class for the BoundsCheckElimination tests.
 */
class BoundsCheckEliminationTest : public testing::Test {
 public:
  BoundsCheckEliminationTest()  : pool_(), allocator_(&pool_) {
    graph_ = CreateGraph(&allocator_);
    graph_->SetHasBoundsChecks(true);
  }

  ~BoundsCheckEliminationTest() { }

  void RunBCE() {
    graph_->BuildDominatorTree();
    graph_->AnalyzeNaturalLoops();

    InstructionSimplifier(graph_).Run();

    SideEffectsAnalysis side_effects(graph_);
    side_effects.Run();

    GVNOptimization(graph_, side_effects).Run();

    HInductionVarAnalysis induction(graph_);
    induction.Run();

    BoundsCheckElimination(graph_, &side_effects, &induction).Run();
  }

  ArenaPool pool_;
  ArenaAllocator allocator_;
  HGraph* graph_;
};


// if (i < 0) { array[i] = 1; // Can't eliminate. }
// else if (i >= array.length) { array[i] = 1; // Can't eliminate. }
// else { array[i] = 1; // Can eliminate. }
TEST_F(BoundsCheckEliminationTest, NarrowingRangeArrayBoundsElimination) {
  HBasicBlock* entry = new (&allocator_) HBasicBlock(graph_);
  graph_->AddBlock(entry);
  graph_->SetEntryBlock(entry);
  HInstruction* parameter1 = new (&allocator_)
      HParameterValue(graph_->GetDexFile(), 0, 0, Primitive::kPrimNot);  // array
  HInstruction* parameter2 = new (&allocator_)
      HParameterValue(graph_->GetDexFile(), 0, 0, Primitive::kPrimInt);  // i
  entry->AddInstruction(parameter1);
  entry->AddInstruction(parameter2);

  HInstruction* constant_1 = graph_->GetIntConstant(1);
  HInstruction* constant_0 = graph_->GetIntConstant(0);

  HBasicBlock* block1 = new (&allocator_) HBasicBlock(graph_);
  graph_->AddBlock(block1);
  HInstruction* cmp = new (&allocator_) HGreaterThanOrEqual(parameter2, constant_0);
  HIf* if_inst = new (&allocator_) HIf(cmp);
  block1->AddInstruction(cmp);
  block1->AddInstruction(if_inst);
  entry->AddSuccessor(block1);

  HBasicBlock* block2 = new (&allocator_) HBasicBlock(graph_);
  graph_->AddBlock(block2);
  HNullCheck* null_check = new (&allocator_) HNullCheck(parameter1, 0);
  HArrayLength* array_length = new (&allocator_) HArrayLength(null_check, 0);
  HBoundsCheck* bounds_check2 = new (&allocator_)
      HBoundsCheck(parameter2, array_length, 0);
  HArraySet* array_set = new (&allocator_) HArraySet(
    null_check, bounds_check2, constant_1, Primitive::kPrimInt, 0);
  block2->AddInstruction(null_check);
  block2->AddInstruction(array_length);
  block2->AddInstruction(bounds_check2);
  block2->AddInstruction(array_set);

  HBasicBlock* block3 = new (&allocator_) HBasicBlock(graph_);
  graph_->AddBlock(block3);
  null_check = new (&allocator_) HNullCheck(parameter1, 0);
  array_length = new (&allocator_) HArrayLength(null_check, 0);
  cmp = new (&allocator_) HLessThan(parameter2, array_length);
  if_inst = new (&allocator_) HIf(cmp);
  block3->AddInstruction(null_check);
  block3->AddInstruction(array_length);
  block3->AddInstruction(cmp);
  block3->AddInstruction(if_inst);

  HBasicBlock* block4 = new (&allocator_) HBasicBlock(graph_);
  graph_->AddBlock(block4);
  null_check = new (&allocator_) HNullCheck(parameter1, 0);
  array_length = new (&allocator_) HArrayLength(null_check, 0);
  HBoundsCheck* bounds_check4 = new (&allocator_)
      HBoundsCheck(parameter2, array_length, 0);
  array_set = new (&allocator_) HArraySet(
    null_check, bounds_check4, constant_1, Primitive::kPrimInt, 0);
  block4->AddInstruction(null_check);
  block4->AddInstruction(array_length);
  block4->AddInstruction(bounds_check4);
  block4->AddInstruction(array_set);

  HBasicBlock* block5 = new (&allocator_) HBasicBlock(graph_);
  graph_->AddBlock(block5);
  null_check = new (&allocator_) HNullCheck(parameter1, 0);
  array_length = new (&allocator_) HArrayLength(null_check, 0);
  HBoundsCheck* bounds_check5 = new (&allocator_)
      HBoundsCheck(parameter2, array_length, 0);
  array_set = new (&allocator_) HArraySet(
    null_check, bounds_check5, constant_1, Primitive::kPrimInt, 0);
  block5->AddInstruction(null_check);
  block5->AddInstruction(array_length);
  block5->AddInstruction(bounds_check5);
  block5->AddInstruction(array_set);

  HBasicBlock* exit = new (&allocator_) HBasicBlock(graph_);
  graph_->AddBlock(exit);
  block2->AddSuccessor(exit);
  block4->AddSuccessor(exit);
  block5->AddSuccessor(exit);
  exit->AddInstruction(new (&allocator_) HExit());

  block1->AddSuccessor(block3);  // True successor
  block1->AddSuccessor(block2);  // False successor

  block3->AddSuccessor(block5);  // True successor
  block3->AddSuccessor(block4);  // False successor

  RunBCE();

  ASSERT_FALSE(IsRemoved(bounds_check2));
  ASSERT_FALSE(IsRemoved(bounds_check4));
  ASSERT_TRUE(IsRemoved(bounds_check5));
}

// if (i > 0) {
//   // Positive number plus MAX_INT will overflow and be negative.
//   int j = i + Integer.MAX_VALUE;
//   if (j < array.length) array[j] = 1;  // Can't eliminate.
// }
TEST_F(BoundsCheckEliminationTest, OverflowArrayBoundsElimination) {
  HBasicBlock* entry = new (&allocator_) HBasicBlock(graph_);
  graph_->AddBlock(entry);
  graph_->SetEntryBlock(entry);
  HInstruction* parameter1 = new (&allocator_)
      HParameterValue(graph_->GetDexFile(), 0, 0, Primitive::kPrimNot);  // array
  HInstruction* parameter2 = new (&allocator_)
      HParameterValue(graph_->GetDexFile(), 0, 0, Primitive::kPrimInt);  // i
  entry->AddInstruction(parameter1);
  entry->AddInstruction(parameter2);

  HInstruction* constant_1 = graph_->GetIntConstant(1);
  HInstruction* constant_0 = graph_->GetIntConstant(0);
  HInstruction* constant_max_int = graph_->GetIntConstant(INT_MAX);

  HBasicBlock* block1 = new (&allocator_) HBasicBlock(graph_);
  graph_->AddBlock(block1);
  HInstruction* cmp = new (&allocator_) HLessThanOrEqual(parameter2, constant_0);
  HIf* if_inst = new (&allocator_) HIf(cmp);
  block1->AddInstruction(cmp);
  block1->AddInstruction(if_inst);
  entry->AddSuccessor(block1);

  HBasicBlock* block2 = new (&allocator_) HBasicBlock(graph_);
  graph_->AddBlock(block2);
  HInstruction* add = new (&allocator_) HAdd(Primitive::kPrimInt, parameter2, constant_max_int);
  HNullCheck* null_check = new (&allocator_) HNullCheck(parameter1, 0);
  HArrayLength* array_length = new (&allocator_) HArrayLength(null_check, 0);
  HInstruction* cmp2 = new (&allocator_) HGreaterThanOrEqual(add, array_length);
  if_inst = new (&allocator_) HIf(cmp2);
  block2->AddInstruction(add);
  block2->AddInstruction(null_check);
  block2->AddInstruction(array_length);
  block2->AddInstruction(cmp2);
  block2->AddInstruction(if_inst);

  HBasicBlock* block3 = new (&allocator_) HBasicBlock(graph_);
  graph_->AddBlock(block3);
  HBoundsCheck* bounds_check = new (&allocator_)
      HBoundsCheck(add, array_length, 0);
  HArraySet* array_set = new (&allocator_) HArraySet(
    null_check, bounds_check, constant_1, Primitive::kPrimInt, 0);
  block3->AddInstruction(bounds_check);
  block3->AddInstruction(array_set);

  HBasicBlock* exit = new (&allocator_) HBasicBlock(graph_);
  graph_->AddBlock(exit);
  exit->AddInstruction(new (&allocator_) HExit());
  block1->AddSuccessor(exit);    // true successor
  block1->AddSuccessor(block2);  // false successor
  block2->AddSuccessor(exit);    // true successor
  block2->AddSuccessor(block3);  // false successor
  block3->AddSuccessor(exit);

  RunBCE();

  ASSERT_FALSE(IsRemoved(bounds_check));
}

// if (i < array.length) {
//   int j = i - Integer.MAX_VALUE;
//   j = j - Integer.MAX_VALUE;  // j is (i+2) after subtracting MAX_INT twice
//   if (j > 0) array[j] = 1;    // Can't eliminate.
// }
TEST_F(BoundsCheckEliminationTest, UnderflowArrayBoundsElimination) {
  HBasicBlock* entry = new (&allocator_) HBasicBlock(graph_);
  graph_->AddBlock(entry);
  graph_->SetEntryBlock(entry);
  HInstruction* parameter1 = new (&allocator_)
      HParameterValue(graph_->GetDexFile(), 0, 0, Primitive::kPrimNot);  // array
  HInstruction* parameter2 = new (&allocator_)
      HParameterValue(graph_->GetDexFile(), 0, 0, Primitive::kPrimInt);  // i
  entry->AddInstruction(parameter1);
  entry->AddInstruction(parameter2);

  HInstruction* constant_1 = graph_->GetIntConstant(1);
  HInstruction* constant_0 = graph_->GetIntConstant(0);
  HInstruction* constant_max_int = graph_->GetIntConstant(INT_MAX);

  HBasicBlock* block1 = new (&allocator_) HBasicBlock(graph_);
  graph_->AddBlock(block1);
  HNullCheck* null_check = new (&allocator_) HNullCheck(parameter1, 0);
  HArrayLength* array_length = new (&allocator_) HArrayLength(null_check, 0);
  HInstruction* cmp = new (&allocator_) HGreaterThanOrEqual(parameter2, array_length);
  HIf* if_inst = new (&allocator_) HIf(cmp);
  block1->AddInstruction(null_check);
  block1->AddInstruction(array_length);
  block1->AddInstruction(cmp);
  block1->AddInstruction(if_inst);
  entry->AddSuccessor(block1);

  HBasicBlock* block2 = new (&allocator_) HBasicBlock(graph_);
  graph_->AddBlock(block2);
  HInstruction* sub1 = new (&allocator_) HSub(Primitive::kPrimInt, parameter2, constant_max_int);
  HInstruction* sub2 = new (&allocator_) HSub(Primitive::kPrimInt, sub1, constant_max_int);
  HInstruction* cmp2 = new (&allocator_) HLessThanOrEqual(sub2, constant_0);
  if_inst = new (&allocator_) HIf(cmp2);
  block2->AddInstruction(sub1);
  block2->AddInstruction(sub2);
  block2->AddInstruction(cmp2);
  block2->AddInstruction(if_inst);

  HBasicBlock* block3 = new (&allocator_) HBasicBlock(graph_);
  graph_->AddBlock(block3);
  HBoundsCheck* bounds_check = new (&allocator_)
      HBoundsCheck(sub2, array_length, 0);
  HArraySet* array_set = new (&allocator_) HArraySet(
    null_check, bounds_check, constant_1, Primitive::kPrimInt, 0);
  block3->AddInstruction(bounds_check);
  block3->AddInstruction(array_set);

  HBasicBlock* exit = new (&allocator_) HBasicBlock(graph_);
  graph_->AddBlock(exit);
  exit->AddInstruction(new (&allocator_) HExit());
  block1->AddSuccessor(exit);    // true successor
  block1->AddSuccessor(block2);  // false successor
  block2->AddSuccessor(exit);    // true successor
  block2->AddSuccessor(block3);  // false successor
  block3->AddSuccessor(exit);

  RunBCE();

  ASSERT_FALSE(IsRemoved(bounds_check));
}

// array[6] = 1; // Can't eliminate.
// array[5] = 1; // Can eliminate.
// array[4] = 1; // Can eliminate.
TEST_F(BoundsCheckEliminationTest, ConstantArrayBoundsElimination) {
  HBasicBlock* entry = new (&allocator_) HBasicBlock(graph_);
  graph_->AddBlock(entry);
  graph_->SetEntryBlock(entry);
  HInstruction* parameter = new (&allocator_) HParameterValue(
      graph_->GetDexFile(), 0, 0, Primitive::kPrimNot);
  entry->AddInstruction(parameter);

  HInstruction* constant_5 = graph_->GetIntConstant(5);
  HInstruction* constant_4 = graph_->GetIntConstant(4);
  HInstruction* constant_6 = graph_->GetIntConstant(6);
  HInstruction* constant_1 = graph_->GetIntConstant(1);

  HBasicBlock* block = new (&allocator_) HBasicBlock(graph_);
  graph_->AddBlock(block);
  entry->AddSuccessor(block);

  HNullCheck* null_check = new (&allocator_) HNullCheck(parameter, 0);
  HArrayLength* array_length = new (&allocator_) HArrayLength(null_check, 0);
  HBoundsCheck* bounds_check6 = new (&allocator_)
      HBoundsCheck(constant_6, array_length, 0);
  HInstruction* array_set = new (&allocator_) HArraySet(
    null_check, bounds_check6, constant_1, Primitive::kPrimInt, 0);
  block->AddInstruction(null_check);
  block->AddInstruction(array_length);
  block->AddInstruction(bounds_check6);
  block->AddInstruction(array_set);

  null_check = new (&allocator_) HNullCheck(parameter, 0);
  array_length = new (&allocator_) HArrayLength(null_check, 0);
  HBoundsCheck* bounds_check5 = new (&allocator_)
      HBoundsCheck(constant_5, array_length, 0);
  array_set = new (&allocator_) HArraySet(
    null_check, bounds_check5, constant_1, Primitive::kPrimInt, 0);
  block->AddInstruction(null_check);
  block->AddInstruction(array_length);
  block->AddInstruction(bounds_check5);
  block->AddInstruction(array_set);

  null_check = new (&allocator_) HNullCheck(parameter, 0);
  array_length = new (&allocator_) HArrayLength(null_check, 0);
  HBoundsCheck* bounds_check4 = new (&allocator_)
      HBoundsCheck(constant_4, array_length, 0);
  array_set = new (&allocator_) HArraySet(
    null_check, bounds_check4, constant_1, Primitive::kPrimInt, 0);
  block->AddInstruction(null_check);
  block->AddInstruction(array_length);
  block->AddInstruction(bounds_check4);
  block->AddInstruction(array_set);

  block->AddInstruction(new (&allocator_) HGoto());

  HBasicBlock* exit = new (&allocator_) HBasicBlock(graph_);
  graph_->AddBlock(exit);
  block->AddSuccessor(exit);
  exit->AddInstruction(new (&allocator_) HExit());

  RunBCE();

  ASSERT_FALSE(IsRemoved(bounds_check6));
  ASSERT_TRUE(IsRemoved(bounds_check5));
  ASSERT_TRUE(IsRemoved(bounds_check4));
}

// for (int i=initial; i<array.length; i+=increment) { array[i] = 10; }
static HInstruction* BuildSSAGraph1(HGraph* graph,
                                    ArenaAllocator* allocator,
                                    int initial,
                                    int increment,
                                    IfCondition cond = kCondGE) {
  HBasicBlock* entry = new (allocator) HBasicBlock(graph);
  graph->AddBlock(entry);
  graph->SetEntryBlock(entry);
  HInstruction* parameter = new (allocator) HParameterValue(
      graph->GetDexFile(), 0, 0, Primitive::kPrimNot);
  entry->AddInstruction(parameter);

  HInstruction* constant_initial = graph->GetIntConstant(initial);
  HInstruction* constant_increment = graph->GetIntConstant(increment);
  HInstruction* constant_10 = graph->GetIntConstant(10);

  HBasicBlock* block = new (allocator) HBasicBlock(graph);
  graph->AddBlock(block);
  entry->AddSuccessor(block);
  block->AddInstruction(new (allocator) HGoto());

  HBasicBlock* loop_header = new (allocator) HBasicBlock(graph);
  HBasicBlock* loop_body = new (allocator) HBasicBlock(graph);
  HBasicBlock* exit = new (allocator) HBasicBlock(graph);

  graph->AddBlock(loop_header);
  graph->AddBlock(loop_body);
  graph->AddBlock(exit);
  block->AddSuccessor(loop_header);
  loop_header->AddSuccessor(exit);       // true successor
  loop_header->AddSuccessor(loop_body);  // false successor
  loop_body->AddSuccessor(loop_header);

  HPhi* phi = new (allocator) HPhi(allocator, 0, 0, Primitive::kPrimInt);
  HInstruction* null_check = new (allocator) HNullCheck(parameter, 0);
  HInstruction* array_length = new (allocator) HArrayLength(null_check, 0);
  HInstruction* cmp = nullptr;
  if (cond == kCondGE) {
    cmp = new (allocator) HGreaterThanOrEqual(phi, array_length);
  } else {
    DCHECK(cond == kCondGT);
    cmp = new (allocator) HGreaterThan(phi, array_length);
  }
  HInstruction* if_inst = new (allocator) HIf(cmp);
  loop_header->AddPhi(phi);
  loop_header->AddInstruction(null_check);
  loop_header->AddInstruction(array_length);
  loop_header->AddInstruction(cmp);
  loop_header->AddInstruction(if_inst);
  phi->AddInput(constant_initial);

  null_check = new (allocator) HNullCheck(parameter, 0);
  array_length = new (allocator) HArrayLength(null_check, 0);
  HInstruction* bounds_check = new (allocator) HBoundsCheck(phi, array_length, 0);
  HInstruction* array_set = new (allocator) HArraySet(
      null_check, bounds_check, constant_10, Primitive::kPrimInt, 0);

  HInstruction* add = new (allocator) HAdd(Primitive::kPrimInt, phi, constant_increment);
  loop_body->AddInstruction(null_check);
  loop_body->AddInstruction(array_length);
  loop_body->AddInstruction(bounds_check);
  loop_body->AddInstruction(array_set);
  loop_body->AddInstruction(add);
  loop_body->AddInstruction(new (allocator) HGoto());
  phi->AddInput(add);

  exit->AddInstruction(new (allocator) HExit());

  return bounds_check;
}

TEST_F(BoundsCheckEliminationTest, LoopArrayBoundsElimination1a) {
  // for (int i=0; i<array.length; i++) { array[i] = 10; // Can eliminate with gvn. }
  HInstruction* bounds_check = BuildSSAGraph1(graph_, &allocator_, 0, 1);
  RunBCE();
  ASSERT_TRUE(IsRemoved(bounds_check));
}

TEST_F(BoundsCheckEliminationTest, LoopArrayBoundsElimination1b) {
  // for (int i=1; i<array.length; i++) { array[i] = 10; // Can eliminate. }
  HInstruction* bounds_check = BuildSSAGraph1(graph_, &allocator_, 1, 1);
  RunBCE();
  ASSERT_TRUE(IsRemoved(bounds_check));
}

TEST_F(BoundsCheckEliminationTest, LoopArrayBoundsElimination1c) {
  // for (int i=-1; i<array.length; i++) { array[i] = 10; // Can't eliminate. }
  HInstruction* bounds_check = BuildSSAGraph1(graph_, &allocator_, -1, 1);
  RunBCE();
  ASSERT_FALSE(IsRemoved(bounds_check));
}

TEST_F(BoundsCheckEliminationTest, LoopArrayBoundsElimination1d) {
  // for (int i=0; i<=array.length; i++) { array[i] = 10; // Can't eliminate. }
  HInstruction* bounds_check = BuildSSAGraph1(graph_, &allocator_, 0, 1, kCondGT);
  RunBCE();
  ASSERT_FALSE(IsRemoved(bounds_check));
}

TEST_F(BoundsCheckEliminationTest, LoopArrayBoundsElimination1e) {
  // for (int i=0; i<array.length; i += 2) {
  //   array[i] = 10; // Can't eliminate due to overflow concern. }
  HInstruction* bounds_check = BuildSSAGraph1(graph_, &allocator_, 0, 2);
  RunBCE();
  ASSERT_FALSE(IsRemoved(bounds_check));
}

TEST_F(BoundsCheckEliminationTest, LoopArrayBoundsElimination1f) {
  // for (int i=1; i<array.length; i += 2) { array[i] = 10; // Can eliminate. }
  HInstruction* bounds_check = BuildSSAGraph1(graph_, &allocator_, 1, 2);
  RunBCE();
  ASSERT_TRUE(IsRemoved(bounds_check));
}

// for (int i=array.length; i>0; i+=increment) { array[i-1] = 10; }
static HInstruction* BuildSSAGraph2(HGraph *graph,
                                    ArenaAllocator* allocator,
                                    int initial,
                                    int increment = -1,
                                    IfCondition cond = kCondLE) {
  HBasicBlock* entry = new (allocator) HBasicBlock(graph);
  graph->AddBlock(entry);
  graph->SetEntryBlock(entry);
  HInstruction* parameter = new (allocator) HParameterValue(
      graph->GetDexFile(), 0, 0, Primitive::kPrimNot);
  entry->AddInstruction(parameter);

  HInstruction* constant_initial = graph->GetIntConstant(initial);
  HInstruction* constant_increment = graph->GetIntConstant(increment);
  HInstruction* constant_minus_1 = graph->GetIntConstant(-1);
  HInstruction* constant_10 = graph->GetIntConstant(10);

  HBasicBlock* block = new (allocator) HBasicBlock(graph);
  graph->AddBlock(block);
  entry->AddSuccessor(block);
  HInstruction* null_check = new (allocator) HNullCheck(parameter, 0);
  HInstruction* array_length = new (allocator) HArrayLength(null_check, 0);
  block->AddInstruction(null_check);
  block->AddInstruction(array_length);
  block->AddInstruction(new (allocator) HGoto());

  HBasicBlock* loop_header = new (allocator) HBasicBlock(graph);
  HBasicBlock* loop_body = new (allocator) HBasicBlock(graph);
  HBasicBlock* exit = new (allocator) HBasicBlock(graph);

  graph->AddBlock(loop_header);
  graph->AddBlock(loop_body);
  graph->AddBlock(exit);
  block->AddSuccessor(loop_header);
  loop_header->AddSuccessor(exit);       // true successor
  loop_header->AddSuccessor(loop_body);  // false successor
  loop_body->AddSuccessor(loop_header);

  HPhi* phi = new (allocator) HPhi(allocator, 0, 0, Primitive::kPrimInt);
  HInstruction* cmp = nullptr;
  if (cond == kCondLE) {
    cmp = new (allocator) HLessThanOrEqual(phi, constant_initial);
  } else {
    DCHECK(cond == kCondLT);
    cmp = new (allocator) HLessThan(phi, constant_initial);
  }
  HInstruction* if_inst = new (allocator) HIf(cmp);
  loop_header->AddPhi(phi);
  loop_header->AddInstruction(cmp);
  loop_header->AddInstruction(if_inst);
  phi->AddInput(array_length);

  HInstruction* add = new (allocator) HAdd(Primitive::kPrimInt, phi, constant_minus_1);
  null_check = new (allocator) HNullCheck(parameter, 0);
  array_length = new (allocator) HArrayLength(null_check, 0);
  HInstruction* bounds_check = new (allocator) HBoundsCheck(add, array_length, 0);
  HInstruction* array_set = new (allocator) HArraySet(
      null_check, bounds_check, constant_10, Primitive::kPrimInt, 0);
  HInstruction* add_phi = new (allocator) HAdd(Primitive::kPrimInt, phi, constant_increment);
  loop_body->AddInstruction(add);
  loop_body->AddInstruction(null_check);
  loop_body->AddInstruction(array_length);
  loop_body->AddInstruction(bounds_check);
  loop_body->AddInstruction(array_set);
  loop_body->AddInstruction(add_phi);
  loop_body->AddInstruction(new (allocator) HGoto());
  phi->AddInput(add);

  exit->AddInstruction(new (allocator) HExit());

  return bounds_check;
}

TEST_F(BoundsCheckEliminationTest, LoopArrayBoundsElimination2a) {
  // for (int i=array.length; i>0; i--) { array[i-1] = 10; // Can eliminate with gvn. }
  HInstruction* bounds_check = BuildSSAGraph2(graph_, &allocator_, 0);
  RunBCE();
  ASSERT_TRUE(IsRemoved(bounds_check));
}

TEST_F(BoundsCheckEliminationTest, LoopArrayBoundsElimination2b) {
  // for (int i=array.length; i>1; i--) { array[i-1] = 10; // Can eliminate. }
  HInstruction* bounds_check = BuildSSAGraph2(graph_, &allocator_, 1);
  RunBCE();
  ASSERT_TRUE(IsRemoved(bounds_check));
}

TEST_F(BoundsCheckEliminationTest, LoopArrayBoundsElimination2c) {
  // for (int i=array.length; i>-1; i--) { array[i-1] = 10; // Can't eliminate. }
  HInstruction* bounds_check = BuildSSAGraph2(graph_, &allocator_, -1);
  RunBCE();
  ASSERT_FALSE(IsRemoved(bounds_check));
}

TEST_F(BoundsCheckEliminationTest, LoopArrayBoundsElimination2d) {
  // for (int i=array.length; i>=0; i--) { array[i-1] = 10; // Can't eliminate. }
  HInstruction* bounds_check = BuildSSAGraph2(graph_, &allocator_, 0, -1, kCondLT);
  RunBCE();
  ASSERT_FALSE(IsRemoved(bounds_check));
}

TEST_F(BoundsCheckEliminationTest, LoopArrayBoundsElimination2e) {
  // for (int i=array.length; i>0; i-=2) { array[i-1] = 10; // Can eliminate. }
  HInstruction* bounds_check = BuildSSAGraph2(graph_, &allocator_, 0, -2);
  RunBCE();
  ASSERT_TRUE(IsRemoved(bounds_check));
}

// int[] array = new int[10];
// for (int i=0; i<10; i+=increment) { array[i] = 10; }
static HInstruction* BuildSSAGraph3(HGraph* graph,
                                    ArenaAllocator* allocator,
                                    int initial,
                                    int increment,
                                    IfCondition cond) {
  HBasicBlock* entry = new (allocator) HBasicBlock(graph);
  graph->AddBlock(entry);
  graph->SetEntryBlock(entry);

  HInstruction* constant_10 = graph->GetIntConstant(10);
  HInstruction* constant_initial = graph->GetIntConstant(initial);
  HInstruction* constant_increment = graph->GetIntConstant(increment);

  HBasicBlock* block = new (allocator) HBasicBlock(graph);
  graph->AddBlock(block);
  entry->AddSuccessor(block);
  HInstruction* new_array = new (allocator) HNewArray(
      constant_10,
      graph->GetCurrentMethod(),
      0,
      Primitive::kPrimInt,
      graph->GetDexFile(),
      kQuickAllocArray);
  block->AddInstruction(new_array);
  block->AddInstruction(new (allocator) HGoto());

  HBasicBlock* loop_header = new (allocator) HBasicBlock(graph);
  HBasicBlock* loop_body = new (allocator) HBasicBlock(graph);
  HBasicBlock* exit = new (allocator) HBasicBlock(graph);

  graph->AddBlock(loop_header);
  graph->AddBlock(loop_body);
  graph->AddBlock(exit);
  block->AddSuccessor(loop_header);
  loop_header->AddSuccessor(exit);       // true successor
  loop_header->AddSuccessor(loop_body);  // false successor
  loop_body->AddSuccessor(loop_header);

  HPhi* phi = new (allocator) HPhi(allocator, 0, 0, Primitive::kPrimInt);
  HInstruction* cmp = nullptr;
  if (cond == kCondGE) {
    cmp = new (allocator) HGreaterThanOrEqual(phi, constant_10);
  } else {
    DCHECK(cond == kCondGT);
    cmp = new (allocator) HGreaterThan(phi, constant_10);
  }
  HInstruction* if_inst = new (allocator) HIf(cmp);
  loop_header->AddPhi(phi);
  loop_header->AddInstruction(cmp);
  loop_header->AddInstruction(if_inst);
  phi->AddInput(constant_initial);

  HNullCheck* null_check = new (allocator) HNullCheck(new_array, 0);
  HArrayLength* array_length = new (allocator) HArrayLength(null_check, 0);
  HInstruction* bounds_check = new (allocator) HBoundsCheck(phi, array_length, 0);
  HInstruction* array_set = new (allocator) HArraySet(
      null_check, bounds_check, constant_10, Primitive::kPrimInt, 0);
  HInstruction* add = new (allocator) HAdd(Primitive::kPrimInt, phi, constant_increment);
  loop_body->AddInstruction(null_check);
  loop_body->AddInstruction(array_length);
  loop_body->AddInstruction(bounds_check);
  loop_body->AddInstruction(array_set);
  loop_body->AddInstruction(add);
  loop_body->AddInstruction(new (allocator) HGoto());
  phi->AddInput(add);

  exit->AddInstruction(new (allocator) HExit());

  return bounds_check;
}

TEST_F(BoundsCheckEliminationTest, LoopArrayBoundsElimination3a) {
  // int[] array = new int[10];
  // for (int i=0; i<10; i++) { array[i] = 10; // Can eliminate. }
  HInstruction* bounds_check = BuildSSAGraph3(graph_, &allocator_, 0, 1, kCondGE);
  RunBCE();
  ASSERT_TRUE(IsRemoved(bounds_check));
}

TEST_F(BoundsCheckEliminationTest, LoopArrayBoundsElimination3b) {
  // int[] array = new int[10];
  // for (int i=1; i<10; i++) { array[i] = 10; // Can eliminate. }
  HInstruction* bounds_check = BuildSSAGraph3(graph_, &allocator_, 1, 1, kCondGE);
  RunBCE();
  ASSERT_TRUE(IsRemoved(bounds_check));
}

TEST_F(BoundsCheckEliminationTest, LoopArrayBoundsElimination3c) {
  // int[] array = new int[10];
  // for (int i=0; i<=10; i++) { array[i] = 10; // Can't eliminate. }
  HInstruction* bounds_check = BuildSSAGraph3(graph_, &allocator_, 0, 1, kCondGT);
  RunBCE();
  ASSERT_FALSE(IsRemoved(bounds_check));
}

TEST_F(BoundsCheckEliminationTest, LoopArrayBoundsElimination3d) {
  // int[] array = new int[10];
  // for (int i=1; i<10; i+=8) { array[i] = 10; // Can eliminate. }
  HInstruction* bounds_check = BuildSSAGraph3(graph_, &allocator_, 1, 8, kCondGE);
  RunBCE();
  ASSERT_TRUE(IsRemoved(bounds_check));
}

// for (int i=initial; i<array.length; i++) { array[array.length-i-1] = 10; }
static HInstruction* BuildSSAGraph4(HGraph* graph,
                                    ArenaAllocator* allocator,
                                    int initial,
                                    IfCondition cond = kCondGE) {
  HBasicBlock* entry = new (allocator) HBasicBlock(graph);
  graph->AddBlock(entry);
  graph->SetEntryBlock(entry);
  HInstruction* parameter = new (allocator) HParameterValue(
      graph->GetDexFile(), 0, 0, Primitive::kPrimNot);
  entry->AddInstruction(parameter);

  HInstruction* constant_initial = graph->GetIntConstant(initial);
  HInstruction* constant_1 = graph->GetIntConstant(1);
  HInstruction* constant_10 = graph->GetIntConstant(10);
  HInstruction* constant_minus_1 = graph->GetIntConstant(-1);

  HBasicBlock* block = new (allocator) HBasicBlock(graph);
  graph->AddBlock(block);
  entry->AddSuccessor(block);
  block->AddInstruction(new (allocator) HGoto());

  HBasicBlock* loop_header = new (allocator) HBasicBlock(graph);
  HBasicBlock* loop_body = new (allocator) HBasicBlock(graph);
  HBasicBlock* exit = new (allocator) HBasicBlock(graph);

  graph->AddBlock(loop_header);
  graph->AddBlock(loop_body);
  graph->AddBlock(exit);
  block->AddSuccessor(loop_header);
  loop_header->AddSuccessor(exit);       // true successor
  loop_header->AddSuccessor(loop_body);  // false successor
  loop_body->AddSuccessor(loop_header);

  HPhi* phi = new (allocator) HPhi(allocator, 0, 0, Primitive::kPrimInt);
  HInstruction* null_check = new (allocator) HNullCheck(parameter, 0);
  HInstruction* array_length = new (allocator) HArrayLength(null_check, 0);
  HInstruction* cmp = nullptr;
  if (cond == kCondGE) {
    cmp = new (allocator) HGreaterThanOrEqual(phi, array_length);
  } else if (cond == kCondGT) {
    cmp = new (allocator) HGreaterThan(phi, array_length);
  }
  HInstruction* if_inst = new (allocator) HIf(cmp);
  loop_header->AddPhi(phi);
  loop_header->AddInstruction(null_check);
  loop_header->AddInstruction(array_length);
  loop_header->AddInstruction(cmp);
  loop_header->AddInstruction(if_inst);
  phi->AddInput(constant_initial);

  null_check = new (allocator) HNullCheck(parameter, 0);
  array_length = new (allocator) HArrayLength(null_check, 0);
  HInstruction* sub = new (allocator) HSub(Primitive::kPrimInt, array_length, phi);
  HInstruction* add_minus_1 = new (allocator)
      HAdd(Primitive::kPrimInt, sub, constant_minus_1);
  HInstruction* bounds_check = new (allocator) HBoundsCheck(add_minus_1, array_length, 0);
  HInstruction* array_set = new (allocator) HArraySet(
      null_check, bounds_check, constant_10, Primitive::kPrimInt, 0);
  HInstruction* add = new (allocator) HAdd(Primitive::kPrimInt, phi, constant_1);
  loop_body->AddInstruction(null_check);
  loop_body->AddInstruction(array_length);
  loop_body->AddInstruction(sub);
  loop_body->AddInstruction(add_minus_1);
  loop_body->AddInstruction(bounds_check);
  loop_body->AddInstruction(array_set);
  loop_body->AddInstruction(add);
  loop_body->AddInstruction(new (allocator) HGoto());
  phi->AddInput(add);

  exit->AddInstruction(new (allocator) HExit());

  return bounds_check;
}

TEST_F(BoundsCheckEliminationTest, LoopArrayBoundsElimination4a) {
  // for (int i=0; i<array.length; i++) { array[array.length-i-1] = 10; // Can eliminate with gvn. }
  HInstruction* bounds_check = BuildSSAGraph4(graph_, &allocator_, 0);
  RunBCE();
  ASSERT_TRUE(IsRemoved(bounds_check));
}

TEST_F(BoundsCheckEliminationTest, LoopArrayBoundsElimination4b) {
  // for (int i=1; i<array.length; i++) { array[array.length-i-1] = 10; // Can eliminate. }
  HInstruction* bounds_check = BuildSSAGraph4(graph_, &allocator_, 1);
  RunBCE();
  ASSERT_TRUE(IsRemoved(bounds_check));
}

TEST_F(BoundsCheckEliminationTest, LoopArrayBoundsElimination4c) {
  // for (int i=0; i<=array.length; i++) { array[array.length-i] = 10; // Can't eliminate. }
  HInstruction* bounds_check = BuildSSAGraph4(graph_, &allocator_, 0, kCondGT);
  RunBCE();
  ASSERT_FALSE(IsRemoved(bounds_check));
}

// Bubble sort:
// (Every array access bounds-check can be eliminated.)
// for (int i=0; i<array.length-1; i++) {
//  for (int j=0; j<array.length-i-1; j++) {
//     if (array[j] > array[j+1]) {
//       int temp = array[j+1];
//       array[j+1] = array[j];
//       array[j] = temp;
//     }
//  }
// }
TEST_F(BoundsCheckEliminationTest, BubbleSortArrayBoundsElimination) {
  HBasicBlock* entry = new (&allocator_) HBasicBlock(graph_);
  graph_->AddBlock(entry);
  graph_->SetEntryBlock(entry);
  HInstruction* parameter = new (&allocator_) HParameterValue(
      graph_->GetDexFile(), 0, 0, Primitive::kPrimNot);
  entry->AddInstruction(parameter);

  HInstruction* constant_0 = graph_->GetIntConstant(0);
  HInstruction* constant_minus_1 = graph_->GetIntConstant(-1);
  HInstruction* constant_1 = graph_->GetIntConstant(1);

  HBasicBlock* block = new (&allocator_) HBasicBlock(graph_);
  graph_->AddBlock(block);
  entry->AddSuccessor(block);
  block->AddInstruction(new (&allocator_) HGoto());

  HBasicBlock* exit = new (&allocator_) HBasicBlock(graph_);
  graph_->AddBlock(exit);
  exit->AddInstruction(new (&allocator_) HExit());

  HBasicBlock* outer_header = new (&allocator_) HBasicBlock(graph_);
  graph_->AddBlock(outer_header);
  HPhi* phi_i = new (&allocator_) HPhi(&allocator_, 0, 0, Primitive::kPrimInt);
  HNullCheck* null_check = new (&allocator_) HNullCheck(parameter, 0);
  HArrayLength* array_length = new (&allocator_) HArrayLength(null_check, 0);
  HAdd* add = new (&allocator_) HAdd(Primitive::kPrimInt, array_length, constant_minus_1);
  HInstruction* cmp = new (&allocator_) HGreaterThanOrEqual(phi_i, add);
  HIf* if_inst = new (&allocator_) HIf(cmp);
  outer_header->AddPhi(phi_i);
  outer_header->AddInstruction(null_check);
  outer_header->AddInstruction(array_length);
  outer_header->AddInstruction(add);
  outer_header->AddInstruction(cmp);
  outer_header->AddInstruction(if_inst);
  phi_i->AddInput(constant_0);

  HBasicBlock* inner_header = new (&allocator_) HBasicBlock(graph_);
  graph_->AddBlock(inner_header);
  HPhi* phi_j = new (&allocator_) HPhi(&allocator_, 0, 0, Primitive::kPrimInt);
  null_check = new (&allocator_) HNullCheck(parameter, 0);
  array_length = new (&allocator_) HArrayLength(null_check, 0);
  HSub* sub = new (&allocator_) HSub(Primitive::kPrimInt, array_length, phi_i);
  add = new (&allocator_) HAdd(Primitive::kPrimInt, sub, constant_minus_1);
  cmp = new (&allocator_) HGreaterThanOrEqual(phi_j, add);
  if_inst = new (&allocator_) HIf(cmp);
  inner_header->AddPhi(phi_j);
  inner_header->AddInstruction(null_check);
  inner_header->AddInstruction(array_length);
  inner_header->AddInstruction(sub);
  inner_header->AddInstruction(add);
  inner_header->AddInstruction(cmp);
  inner_header->AddInstruction(if_inst);
  phi_j->AddInput(constant_0);

  HBasicBlock* inner_body_compare = new (&allocator_) HBasicBlock(graph_);
  graph_->AddBlock(inner_body_compare);
  null_check = new (&allocator_) HNullCheck(parameter, 0);
  array_length = new (&allocator_) HArrayLength(null_check, 0);
  HBoundsCheck* bounds_check1 = new (&allocator_) HBoundsCheck(phi_j, array_length, 0);
  HArrayGet* array_get_j = new (&allocator_)
      HArrayGet(null_check, bounds_check1, Primitive::kPrimInt, 0);
  inner_body_compare->AddInstruction(null_check);
  inner_body_compare->AddInstruction(array_length);
  inner_body_compare->AddInstruction(bounds_check1);
  inner_body_compare->AddInstruction(array_get_j);
  HInstruction* j_plus_1 = new (&allocator_) HAdd(Primitive::kPrimInt, phi_j, constant_1);
  null_check = new (&allocator_) HNullCheck(parameter, 0);
  array_length = new (&allocator_) HArrayLength(null_check, 0);
  HBoundsCheck* bounds_check2 = new (&allocator_) HBoundsCheck(j_plus_1, array_length, 0);
  HArrayGet* array_get_j_plus_1 = new (&allocator_)
      HArrayGet(null_check, bounds_check2, Primitive::kPrimInt, 0);
  cmp = new (&allocator_) HGreaterThanOrEqual(array_get_j, array_get_j_plus_1);
  if_inst = new (&allocator_) HIf(cmp);
  inner_body_compare->AddInstruction(j_plus_1);
  inner_body_compare->AddInstruction(null_check);
  inner_body_compare->AddInstruction(array_length);
  inner_body_compare->AddInstruction(bounds_check2);
  inner_body_compare->AddInstruction(array_get_j_plus_1);
  inner_body_compare->AddInstruction(cmp);
  inner_body_compare->AddInstruction(if_inst);

  HBasicBlock* inner_body_swap = new (&allocator_) HBasicBlock(graph_);
  graph_->AddBlock(inner_body_swap);
  j_plus_1 = new (&allocator_) HAdd(Primitive::kPrimInt, phi_j, constant_1);
  // temp = array[j+1]
  null_check = new (&allocator_) HNullCheck(parameter, 0);
  array_length = new (&allocator_) HArrayLength(null_check, 0);
  HInstruction* bounds_check3 = new (&allocator_) HBoundsCheck(j_plus_1, array_length, 0);
  array_get_j_plus_1 = new (&allocator_)
      HArrayGet(null_check, bounds_check3, Primitive::kPrimInt, 0);
  inner_body_swap->AddInstruction(j_plus_1);
  inner_body_swap->AddInstruction(null_check);
  inner_body_swap->AddInstruction(array_length);
  inner_body_swap->AddInstruction(bounds_check3);
  inner_body_swap->AddInstruction(array_get_j_plus_1);
  // array[j+1] = array[j]
  null_check = new (&allocator_) HNullCheck(parameter, 0);
  array_length = new (&allocator_) HArrayLength(null_check, 0);
  HInstruction* bounds_check4 = new (&allocator_) HBoundsCheck(phi_j, array_length, 0);
  array_get_j = new (&allocator_)
      HArrayGet(null_check, bounds_check4, Primitive::kPrimInt, 0);
  inner_body_swap->AddInstruction(null_check);
  inner_body_swap->AddInstruction(array_length);
  inner_body_swap->AddInstruction(bounds_check4);
  inner_body_swap->AddInstruction(array_get_j);
  null_check = new (&allocator_) HNullCheck(parameter, 0);
  array_length = new (&allocator_) HArrayLength(null_check, 0);
  HInstruction* bounds_check5 = new (&allocator_) HBoundsCheck(j_plus_1, array_length, 0);
  HArraySet* array_set_j_plus_1 = new (&allocator_)
      HArraySet(null_check, bounds_check5, array_get_j, Primitive::kPrimInt, 0);
  inner_body_swap->AddInstruction(null_check);
  inner_body_swap->AddInstruction(array_length);
  inner_body_swap->AddInstruction(bounds_check5);
  inner_body_swap->AddInstruction(array_set_j_plus_1);
  // array[j] = temp
  null_check = new (&allocator_) HNullCheck(parameter, 0);
  array_length = new (&allocator_) HArrayLength(null_check, 0);
  HInstruction* bounds_check6 = new (&allocator_) HBoundsCheck(phi_j, array_length, 0);
  HArraySet* array_set_j = new (&allocator_)
      HArraySet(null_check, bounds_check6, array_get_j_plus_1, Primitive::kPrimInt, 0);
  inner_body_swap->AddInstruction(null_check);
  inner_body_swap->AddInstruction(array_length);
  inner_body_swap->AddInstruction(bounds_check6);
  inner_body_swap->AddInstruction(array_set_j);
  inner_body_swap->AddInstruction(new (&allocator_) HGoto());

  HBasicBlock* inner_body_add = new (&allocator_) HBasicBlock(graph_);
  graph_->AddBlock(inner_body_add);
  add = new (&allocator_) HAdd(Primitive::kPrimInt, phi_j, constant_1);
  inner_body_add->AddInstruction(add);
  inner_body_add->AddInstruction(new (&allocator_) HGoto());
  phi_j->AddInput(add);

  HBasicBlock* outer_body_add = new (&allocator_) HBasicBlock(graph_);
  graph_->AddBlock(outer_body_add);
  add = new (&allocator_) HAdd(Primitive::kPrimInt, phi_i, constant_1);
  outer_body_add->AddInstruction(add);
  outer_body_add->AddInstruction(new (&allocator_) HGoto());
  phi_i->AddInput(add);

  block->AddSuccessor(outer_header);
  outer_header->AddSuccessor(exit);
  outer_header->AddSuccessor(inner_header);
  inner_header->AddSuccessor(outer_body_add);
  inner_header->AddSuccessor(inner_body_compare);
  inner_body_compare->AddSuccessor(inner_body_add);
  inner_body_compare->AddSuccessor(inner_body_swap);
  inner_body_swap->AddSuccessor(inner_body_add);
  inner_body_add->AddSuccessor(inner_header);
  outer_body_add->AddSuccessor(outer_header);

  RunBCE();  // gvn removes same bounds check already

  ASSERT_TRUE(IsRemoved(bounds_check1));
  ASSERT_TRUE(IsRemoved(bounds_check2));
  ASSERT_TRUE(IsRemoved(bounds_check3));
  ASSERT_TRUE(IsRemoved(bounds_check4));
  ASSERT_TRUE(IsRemoved(bounds_check5));
  ASSERT_TRUE(IsRemoved(bounds_check6));
}

}  // namespace art
