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

#include "bounds_check_elimination.h"
#include "builder.h"
#include "gvn.h"
#include "nodes.h"
#include "optimizing_unit_test.h"
#include "utils/arena_allocator.h"

#include "gtest/gtest.h"

namespace art {

// for (int i=initial; i<array.length; i++) { array[i] = 10; }
static HGraph* BuildSSAGraph1(ArenaAllocator* allocator,
                              HInstruction** bounds_check,
                              int initial,
                              IfCondition cond = kCondGE) {
  HGraph* graph = new (allocator) HGraph(allocator);

  HBasicBlock* entry = new (allocator) HBasicBlock(graph);
  graph->AddBlock(entry);
  graph->SetEntryBlock(entry);
  HInstruction* parameter = new (allocator) HParameterValue(0, Primitive::kPrimNot);
  HInstruction* constant_initial = new (allocator) HIntConstant(initial);
  HInstruction* constant_1 = new (allocator) HIntConstant(1);
  HInstruction* constant_10 = new (allocator) HIntConstant(10);
  entry->AddInstruction(parameter);
  entry->AddInstruction(constant_initial);
  entry->AddInstruction(constant_1);
  entry->AddInstruction(constant_10);

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
  phi->AddInput(constant_initial);
  HInstruction* null_check = new (allocator) HNullCheck(parameter, 0);
  HInstruction* array_length = new (allocator) HArrayLength(null_check);
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

  null_check = new (allocator) HNullCheck(parameter, 0);
  array_length = new (allocator) HArrayLength(null_check);
  *bounds_check = new (allocator) HBoundsCheck(phi, array_length, 0);
  HInstruction* array_set = new (allocator) HArraySet(
      null_check, *bounds_check, constant_10, Primitive::kPrimInt, 0);
  HInstruction* add = new (allocator) HAdd(Primitive::kPrimInt, phi, constant_1);
  loop_body->AddInstruction(null_check);
  loop_body->AddInstruction(array_length);
  loop_body->AddInstruction(*bounds_check);
  loop_body->AddInstruction(array_set);
  loop_body->AddInstruction(add);
  loop_body->AddInstruction(new (allocator) HGoto());
  phi->AddInput(add);

  exit->AddInstruction(new (allocator) HExit());

  return graph;
}

TEST(BoundsCheckEliminationTest, LoopArrayBoundsElimination1) {
  ArenaPool pool;
  ArenaAllocator allocator(&pool);

  // Try for (int i=0; i<array.length; i++) { array[i] = 10; }
  HInstruction* bounds_check = nullptr;
  HGraph* graph = BuildSSAGraph1(&allocator, &bounds_check, 0);
  graph->BuildDominatorTree();
  BoundsCheckElimination bounds_check_elimination(graph);
  bounds_check_elimination.Run();
  ASSERT_NE(bounds_check->GetBlock(), nullptr);

  // This time add gvn. Need gvn to eliminate the second
  // HArrayLength which uses the null check as its input.
  graph = BuildSSAGraph1(&allocator, &bounds_check, 0);
  graph->BuildDominatorTree();
  GlobalValueNumberer(&allocator, graph).Run();
  BoundsCheckElimination bounds_check_elimination_after_gvn(graph);
  bounds_check_elimination_after_gvn.Run();
  ASSERT_EQ(bounds_check->GetBlock(), nullptr);

  // Try for (int i=1; i<array.length; i++) { array[i] = 10; }
  graph = BuildSSAGraph1(&allocator, &bounds_check, 1);
  graph->BuildDominatorTree();
  GlobalValueNumberer(&allocator, graph).Run();
  BoundsCheckElimination bounds_check_elimination_with_initial_1(graph);
  bounds_check_elimination_with_initial_1.Run();
  ASSERT_EQ(bounds_check->GetBlock(), nullptr);

  // Try for (int i=-1; i<array.length; i++) { array[i] = 10; }
  graph = BuildSSAGraph1(&allocator, &bounds_check, -1);
  graph->BuildDominatorTree();
  GlobalValueNumberer(&allocator, graph).Run();
  BoundsCheckElimination bounds_check_elimination_with_initial_minus_1(graph);
  bounds_check_elimination_with_initial_minus_1.Run();
  ASSERT_NE(bounds_check->GetBlock(), nullptr);

  // Try for (int i=0; i<=array.length; i++) { array[i] = 10; }
  graph = BuildSSAGraph1(&allocator, &bounds_check, 0, kCondGT);
  graph->BuildDominatorTree();
  GlobalValueNumberer(&allocator, graph).Run();
  BoundsCheckElimination bounds_check_elimination_with_greater_than(graph);
  bounds_check_elimination_with_greater_than.Run();
  ASSERT_NE(bounds_check->GetBlock(), nullptr);
}

// for (int i=array.length; i>0; i--) { array[i-1] = 10; }
static HGraph* BuildSSAGraph2(ArenaAllocator* allocator,
                              HInstruction** bounds_check,
                              int initial,
                              IfCondition cond = kCondLE) {
  HGraph* graph = new (allocator) HGraph(allocator);

  HBasicBlock* entry = new (allocator) HBasicBlock(graph);
  graph->AddBlock(entry);
  graph->SetEntryBlock(entry);
  HInstruction* parameter = new (allocator) HParameterValue(0, Primitive::kPrimNot);
  HInstruction* constant_initial = new (allocator) HIntConstant(initial);
  HInstruction* constant_minus_1 = new (allocator) HIntConstant(-1);
  HInstruction* constant_10 = new (allocator) HIntConstant(10);
  entry->AddInstruction(parameter);
  entry->AddInstruction(constant_initial);
  entry->AddInstruction(constant_minus_1);
  entry->AddInstruction(constant_10);

  HBasicBlock* block = new (allocator) HBasicBlock(graph);
  graph->AddBlock(block);
  entry->AddSuccessor(block);
  HInstruction* null_check = new (allocator) HNullCheck(parameter, 0);
  HInstruction* array_length = new (allocator) HArrayLength(null_check);
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
  phi->AddInput(array_length);
  HInstruction* cmp = nullptr;
  if (cond == kCondLE) {
    cmp = new (allocator) HLessThanOrEqual(phi, constant_initial);
  } else if (cond == kCondLT) {
    cmp = new (allocator) HLessThan(phi, constant_initial);
  }
  HInstruction* if_inst = new (allocator) HIf(cmp);
  loop_header->AddPhi(phi);
  loop_header->AddInstruction(cmp);
  loop_header->AddInstruction(if_inst);

  HInstruction* add = new (allocator) HAdd(Primitive::kPrimInt, phi, constant_minus_1);
  null_check = new (allocator) HNullCheck(parameter, 0);
  array_length = new (allocator) HArrayLength(null_check);
  *bounds_check = new (allocator) HBoundsCheck(add, array_length, 0);
  HInstruction* array_set = new (allocator) HArraySet(
      null_check, *bounds_check, constant_10, Primitive::kPrimInt, 0);
  loop_body->AddInstruction(add);
  loop_body->AddInstruction(null_check);
  loop_body->AddInstruction(array_length);
  loop_body->AddInstruction(*bounds_check);
  loop_body->AddInstruction(array_set);
  loop_body->AddInstruction(new (allocator) HGoto());
  phi->AddInput(add);

  exit->AddInstruction(new (allocator) HExit());

  return graph;
}

TEST(BoundsCheckEliminationTest, LoopArrayBoundsElimination2) {
  ArenaPool pool;
  ArenaAllocator allocator(&pool);

  // Try for (int i=array.length; i>0; i--) { array[i-1] = 10; }
  HInstruction* bounds_check = nullptr;
  HGraph* graph = BuildSSAGraph2(&allocator, &bounds_check, 0);
  graph->BuildDominatorTree();
  BoundsCheckElimination bounds_check_elimination(graph);
  bounds_check_elimination.Run();
  ASSERT_NE(bounds_check->GetBlock(), nullptr);

  // This time add gvn. Need gvn to eliminate the second
  // HArrayLength which uses the null check as its input.
  graph = BuildSSAGraph2(&allocator, &bounds_check, 0);
  graph->BuildDominatorTree();
  GlobalValueNumberer(&allocator, graph).Run();
  BoundsCheckElimination bounds_check_elimination_after_gvn(graph);
  bounds_check_elimination_after_gvn.Run();
  ASSERT_EQ(bounds_check->GetBlock(), nullptr);

  // Try for (int i=array.length; i>1; i--) { array[i-1] = 10; }
  graph = BuildSSAGraph2(&allocator, &bounds_check, 1);
  graph->BuildDominatorTree();
  GlobalValueNumberer(&allocator, graph).Run();
  BoundsCheckElimination bounds_check_elimination_with_initial_1(graph);
  bounds_check_elimination_with_initial_1.Run();
  ASSERT_EQ(bounds_check->GetBlock(), nullptr);

  // Try for (int i=array.length; i>-1; i--) { array[i-1] = 10; }
  graph = BuildSSAGraph2(&allocator, &bounds_check, -1);
  graph->BuildDominatorTree();
  GlobalValueNumberer(&allocator, graph).Run();
  BoundsCheckElimination bounds_check_elimination_with_initial_minus_1(graph);
  bounds_check_elimination_with_initial_minus_1.Run();
  ASSERT_NE(bounds_check->GetBlock(), nullptr);

  // Try for (int i=array.length; i>=0; i--) { array[i-1] = 10; }
  graph = BuildSSAGraph2(&allocator, &bounds_check, 0, kCondLT);
  graph->BuildDominatorTree();
  GlobalValueNumberer(&allocator, graph).Run();
  BoundsCheckElimination bounds_check_elimination_with_less_than(graph);
  bounds_check_elimination_with_less_than.Run();
  ASSERT_NE(bounds_check->GetBlock(), nullptr);
}

// int[] array = new array[10];
// for (int i=0; i<10; i++) { array[i] = 10; }
static HGraph* BuildSSAGraph3(ArenaAllocator* allocator,
                              HInstruction** bounds_check,
                              int initial,
                              IfCondition cond) {
  HGraph* graph = new (allocator) HGraph(allocator);

  HBasicBlock* entry = new (allocator) HBasicBlock(graph);
  graph->AddBlock(entry);
  graph->SetEntryBlock(entry);
  HInstruction* constant_10 = new (allocator) HIntConstant(10);
  HInstruction* constant_initial = new (allocator) HIntConstant(initial);
  HInstruction* constant_1 = new (allocator) HIntConstant(1);
  entry->AddInstruction(constant_10);
  entry->AddInstruction(constant_initial);
  entry->AddInstruction(constant_1);

  HBasicBlock* block = new (allocator) HBasicBlock(graph);
  graph->AddBlock(block);
  entry->AddSuccessor(block);
  HInstruction* new_array = new (allocator) HNewArray(constant_10, 0, Primitive::kPrimInt);
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
  phi->AddInput(constant_initial);
  HInstruction* cmp = nullptr;
  if (cond == kCondGE) {
    cmp = new (allocator) HGreaterThanOrEqual(phi, constant_10);
  } else if (cond == kCondGT) {
    cmp = new (allocator) HGreaterThan(phi, constant_10);
  }
  HInstruction* if_inst = new (allocator) HIf(cmp);
  loop_header->AddPhi(phi);
  loop_header->AddInstruction(cmp);
  loop_header->AddInstruction(if_inst);

  HNullCheck* null_check = new (allocator) HNullCheck(new_array, 0);
  HArrayLength* array_length = new (allocator) HArrayLength(null_check);
  *bounds_check = new (allocator) HBoundsCheck(phi, array_length, 0);
  HInstruction* array_set = new (allocator) HArraySet(
      null_check, *bounds_check, constant_10, Primitive::kPrimInt, 0);
  HInstruction* add = new (allocator) HAdd(Primitive::kPrimInt, phi, constant_1);
  loop_body->AddInstruction(null_check);
  loop_body->AddInstruction(array_length);
  loop_body->AddInstruction(*bounds_check);
  loop_body->AddInstruction(array_set);
  loop_body->AddInstruction(add);
  loop_body->AddInstruction(new (allocator) HGoto());
  phi->AddInput(add);

  exit->AddInstruction(new (allocator) HExit());

  return graph;
}

TEST(BoundsCheckEliminationTest, LoopArrayBoundsElimination3) {
  ArenaPool pool;
  ArenaAllocator allocator(&pool);

  // int[] array = new array[10];
  // for (int i=0; i<10; i++) { array[i] = 10; }
  HInstruction* bounds_check = nullptr;
  HGraph* graph = BuildSSAGraph3(&allocator, &bounds_check, 0, kCondGE);
  graph->BuildDominatorTree();
  GlobalValueNumberer(&allocator, graph).Run();
  BoundsCheckElimination bounds_check_elimination_after_gvn(graph);
  bounds_check_elimination_after_gvn.Run();
  ASSERT_EQ(bounds_check->GetBlock(), nullptr);

  // int[] array = new array[10];
  // for (int i=1; i<10; i++) { array[i] = 10; }
  graph = BuildSSAGraph3(&allocator, &bounds_check, 1, kCondGE);
  graph->BuildDominatorTree();
  GlobalValueNumberer(&allocator, graph).Run();
  BoundsCheckElimination bounds_check_elimination_with_initial_1(graph);
  bounds_check_elimination_with_initial_1.Run();
  ASSERT_EQ(bounds_check->GetBlock(), nullptr);

  // int[] array = new array[10];
  // for (int i=0; i<=10; i++) { array[i] = 10; }
  graph = BuildSSAGraph3(&allocator, &bounds_check, 0, kCondGT);
  graph->BuildDominatorTree();
  GlobalValueNumberer(&allocator, graph).Run();
  BoundsCheckElimination bounds_check_elimination_with_greater_than(graph);
  bounds_check_elimination_with_greater_than.Run();
  ASSERT_NE(bounds_check->GetBlock(), nullptr);
}

// array[5] = 1; array[4] = 1;
TEST(BoundsCheckEliminationTest, ConstantArrayBoundsElimination) {
  ArenaPool pool;
  ArenaAllocator allocator(&pool);

  HGraph* graph = new (&allocator) HGraph(&allocator);

  HBasicBlock* entry = new (&allocator) HBasicBlock(graph);
  graph->AddBlock(entry);
  graph->SetEntryBlock(entry);
  HInstruction* parameter = new (&allocator) HParameterValue(0, Primitive::kPrimNot);
  HInstruction* constant_5 = new (&allocator) HIntConstant(5);
  HInstruction* constant_4 = new (&allocator) HIntConstant(4);
  HInstruction* constant_1 = new (&allocator) HIntConstant(1);
  entry->AddInstruction(parameter);
  entry->AddInstruction(constant_5);
  entry->AddInstruction(constant_4);
  entry->AddInstruction(constant_1);

  HBasicBlock* block = new (&allocator) HBasicBlock(graph);
  graph->AddBlock(block);
  entry->AddSuccessor(block);

  HNullCheck* null_check = new (&allocator) HNullCheck(parameter, 0);
  HArrayLength* array_length = new (&allocator) HArrayLength(null_check);
  HBoundsCheck* bounds_check1 = new (&allocator) HBoundsCheck(constant_5, array_length, 0);
  HInstruction* array_set = new (&allocator) HArraySet(
    null_check, bounds_check1, array_length, Primitive::kPrimInt, 0);
  block->AddInstruction(null_check);
  block->AddInstruction(array_length);
  block->AddInstruction(bounds_check1);
  block->AddInstruction(array_set);

  null_check = new (&allocator) HNullCheck(parameter, 0);
  array_length = new (&allocator) HArrayLength(null_check);
  HBoundsCheck* bounds_check2 = new (&allocator) HBoundsCheck(constant_4, array_length, 0);
  array_set = new (&allocator) HArraySet(
    null_check, bounds_check2, array_length, Primitive::kPrimInt, 0);
  block->AddInstruction(null_check);
  block->AddInstruction(array_length);
  block->AddInstruction(bounds_check2);
  block->AddInstruction(array_set);

  block->AddInstruction(new (&allocator) HGoto());

  HBasicBlock* exit = new (&allocator) HBasicBlock(graph);
  graph->AddBlock(exit);
  block->AddSuccessor(exit);
  exit->AddInstruction(new (&allocator) HExit());

  graph->BuildDominatorTree();
  GlobalValueNumberer(&allocator, graph).Run();
  BoundsCheckElimination bounds_check_elimination(graph);
  bounds_check_elimination.Run();
  ASSERT_NE(bounds_check1->GetBlock(), nullptr);
  ASSERT_EQ(bounds_check2->GetBlock(), nullptr);
}

// if (i < 0) { array[i] = 1; }
// else if (i >= array.length) { array[i] = 1; }
// else { array[i] = 1; }
TEST(BoundsCheckEliminationTest, NarrowingRangeArrayBoundsElimination) {
  ArenaPool pool;
  ArenaAllocator allocator(&pool);

  HGraph* graph = new (&allocator) HGraph(&allocator);

  HBasicBlock* entry = new (&allocator) HBasicBlock(graph);
  graph->AddBlock(entry);
  graph->SetEntryBlock(entry);
  HInstruction* parameter1 = new (&allocator) HParameterValue(0, Primitive::kPrimNot);  // array
  HInstruction* parameter2 = new (&allocator) HParameterValue(0, Primitive::kPrimInt);  // i
  HInstruction* constant_1 = new (&allocator) HIntConstant(1);
  HInstruction* constant_0 = new (&allocator) HIntConstant(0);
  entry->AddInstruction(parameter1);
  entry->AddInstruction(parameter2);
  entry->AddInstruction(constant_1);
  entry->AddInstruction(constant_0);

  HBasicBlock* block1 = new (&allocator) HBasicBlock(graph);
  graph->AddBlock(block1);
  HInstruction* cmp = new (&allocator) HGreaterThanOrEqual(parameter2, constant_0);
  HIf* if_inst = new (&allocator) HIf(cmp);
  block1->AddInstruction(cmp);
  block1->AddInstruction(if_inst);
  entry->AddSuccessor(block1);

  HBasicBlock* block2 = new (&allocator) HBasicBlock(graph);
  graph->AddBlock(block2);
  HNullCheck* null_check = new (&allocator) HNullCheck(parameter1, 0);
  HArrayLength* array_length = new (&allocator) HArrayLength(null_check);
  HBoundsCheck* bounds_check2 = new (&allocator) HBoundsCheck(parameter2, array_length, 0);
  HArraySet* array_set = new (&allocator) HArraySet(
    null_check, bounds_check2, array_length, Primitive::kPrimInt, 0);
  block2->AddInstruction(null_check);
  block2->AddInstruction(array_length);
  block2->AddInstruction(bounds_check2);
  block2->AddInstruction(array_set);

  HBasicBlock* block3 = new (&allocator) HBasicBlock(graph);
  graph->AddBlock(block3);
  null_check = new (&allocator) HNullCheck(parameter1, 0);
  array_length = new (&allocator) HArrayLength(null_check);
  cmp = new (&allocator) HLessThan(parameter2, array_length);
  if_inst = new (&allocator) HIf(cmp);
  block3->AddInstruction(null_check);
  block3->AddInstruction(array_length);
  block3->AddInstruction(cmp);
  block3->AddInstruction(if_inst);

  HBasicBlock* block4 = new (&allocator) HBasicBlock(graph);
  graph->AddBlock(block4);
  null_check = new (&allocator) HNullCheck(parameter1, 0);
  array_length = new (&allocator) HArrayLength(null_check);
  HBoundsCheck* bounds_check4 = new (&allocator) HBoundsCheck(parameter2, array_length, 0);
  array_set = new (&allocator) HArraySet(
    null_check, bounds_check4, array_length, Primitive::kPrimInt, 0);
  block4->AddInstruction(null_check);
  block4->AddInstruction(array_length);
  block4->AddInstruction(bounds_check4);
  block4->AddInstruction(array_set);

  HBasicBlock* block5 = new (&allocator) HBasicBlock(graph);
  graph->AddBlock(block5);
  null_check = new (&allocator) HNullCheck(parameter1, 0);
  array_length = new (&allocator) HArrayLength(null_check);
  HBoundsCheck* bounds_check5 = new (&allocator) HBoundsCheck(parameter2, array_length, 0);
  array_set = new (&allocator) HArraySet(
    null_check, bounds_check5, array_length, Primitive::kPrimInt, 0);
  block5->AddInstruction(null_check);
  block5->AddInstruction(array_length);
  block5->AddInstruction(bounds_check5);
  block5->AddInstruction(array_set);

  HBasicBlock* exit = new (&allocator) HBasicBlock(graph);
  graph->AddBlock(exit);
  block2->AddSuccessor(exit);
  block4->AddSuccessor(exit);
  block5->AddSuccessor(exit);
  exit->AddInstruction(new (&allocator) HExit());

  block1->AddSuccessor(block3);  // True successor
  block1->AddSuccessor(block2);  // False successor

  block3->AddSuccessor(block5);  // True successor
  block3->AddSuccessor(block4);  // False successor

  graph->BuildDominatorTree();
  GlobalValueNumberer(&allocator, graph).Run();
  BoundsCheckElimination bounds_check_elimination(graph);
  bounds_check_elimination.Run();
  ASSERT_NE(bounds_check2->GetBlock(), nullptr);
  ASSERT_NE(bounds_check4->GetBlock(), nullptr);
  ASSERT_EQ(bounds_check5->GetBlock(), nullptr);
}

}  // namespace art
