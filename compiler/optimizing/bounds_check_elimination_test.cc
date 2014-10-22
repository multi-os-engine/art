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
                              int initial = 0,
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
  HGraph* graph = BuildSSAGraph1(&allocator, &bounds_check);
  graph->BuildDominatorTree();
  BoundsCheckElimination bounds_check_elimination(&allocator, graph);
  bounds_check_elimination.Run();
  ASSERT_NE(bounds_check->GetBlock(), nullptr);

  // Need gvn to eliminate the second HArrayLength which uses the null check
  // as its input.
  graph = BuildSSAGraph1(&allocator, &bounds_check);
  graph->BuildDominatorTree();
  GlobalValueNumberer(&allocator, graph).Run();
  BoundsCheckElimination bounds_check_elimination_after_gvn(&allocator, graph);
  bounds_check_elimination_after_gvn.Run();
  ASSERT_EQ(bounds_check->GetBlock(), nullptr);

  // Try for (int i=1; i<array.length; i++) { array[i] = 10; }
  graph = BuildSSAGraph1(&allocator, &bounds_check, 1);
  graph->BuildDominatorTree();
  GlobalValueNumberer(&allocator, graph).Run();
  BoundsCheckElimination bounds_check_elimination_with_initial_1(&allocator, graph);
  bounds_check_elimination_with_initial_1.Run();
  ASSERT_EQ(bounds_check->GetBlock(), nullptr);

  // Try for (int i=-1; i<array.length; i++) { array[i] = 10; }
  graph = BuildSSAGraph1(&allocator, &bounds_check, -1);
  graph->BuildDominatorTree();
  GlobalValueNumberer(&allocator, graph).Run();
  BoundsCheckElimination bounds_check_elimination_with_initial_minus_1(&allocator, graph);
  bounds_check_elimination_with_initial_minus_1.Run();
  ASSERT_NE(bounds_check->GetBlock(), nullptr);

  // Try for (int i=0; i<=array.length; i++) { array[i] = 10; }
  graph = BuildSSAGraph1(&allocator, &bounds_check, 0, kCondGT);
  graph->BuildDominatorTree();
  GlobalValueNumberer(&allocator, graph).Run();
  BoundsCheckElimination bounds_check_elimination_with_greater_than(&allocator, graph);
  bounds_check_elimination_with_greater_than.Run();
  ASSERT_NE(bounds_check->GetBlock(), nullptr);
}

// for (int i=array.length; i>0; i--) { array[i-1] = 10; }
static HGraph* BuildSSAGraph2(ArenaAllocator* allocator,
                              HInstruction** bounds_check,
                              int initial = 0,
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
  HGraph* graph = BuildSSAGraph2(&allocator, &bounds_check);
  graph->BuildDominatorTree();
  BoundsCheckElimination bounds_check_elimination(&allocator, graph);
  bounds_check_elimination.Run();
  ASSERT_NE(bounds_check->GetBlock(), nullptr);

  // Need gvn to eliminate the second HArrayLength which uses the null check
  // as its input.
  graph = BuildSSAGraph2(&allocator, &bounds_check);
  graph->BuildDominatorTree();
  GlobalValueNumberer(&allocator, graph).Run();
  BoundsCheckElimination bounds_check_elimination_after_gvn(&allocator, graph);
  bounds_check_elimination_after_gvn.Run();
  ASSERT_EQ(bounds_check->GetBlock(), nullptr);

  // Try for (int i=array.length; i>1; i--) { array[i-1] = 10; }
  graph = BuildSSAGraph2(&allocator, &bounds_check, 1);
  graph->BuildDominatorTree();
  GlobalValueNumberer(&allocator, graph).Run();
  BoundsCheckElimination bounds_check_elimination_with_initial_1(&allocator, graph);
  bounds_check_elimination_with_initial_1.Run();
  ASSERT_EQ(bounds_check->GetBlock(), nullptr);

  // Try for (int i=array.length; i>-1; i--) { array[i-1] = 10; }
  graph = BuildSSAGraph2(&allocator, &bounds_check, -1);
  graph->BuildDominatorTree();
  GlobalValueNumberer(&allocator, graph).Run();
  BoundsCheckElimination bounds_check_elimination_with_initial_minus_1(&allocator, graph);
  bounds_check_elimination_with_initial_minus_1.Run();
  ASSERT_NE(bounds_check->GetBlock(), nullptr);

  // Try for (int i=array.length; i>=0; i--) { array[i-1] = 10; }
  graph = BuildSSAGraph2(&allocator, &bounds_check, 0, kCondLT);
  graph->BuildDominatorTree();
  GlobalValueNumberer(&allocator, graph).Run();
  BoundsCheckElimination bounds_check_elimination_with_less_than(&allocator, graph);
  bounds_check_elimination_with_less_than.Run();
  ASSERT_NE(bounds_check->GetBlock(), nullptr);
}

}  // namespace art
