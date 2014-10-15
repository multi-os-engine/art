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

#include "builder.h"
#include "gvn.h"
#include "nodes.h"
#include "optimizing_unit_test.h"
#include "ssa_phi_elimination.h"
#include "utils/arena_allocator.h"

#include "gtest/gtest.h"

namespace art {

TEST(GVNTest, LocalFieldElimination) {
  ArenaPool pool;
  ArenaAllocator allocator(&pool);

  HGraph* graph = new (&allocator) HGraph(&allocator);
  HBasicBlock* entry = new (&allocator) HBasicBlock(graph);
  graph->AddBlock(entry);
  graph->SetEntryBlock(entry);
  HInstruction* parameter = new (&allocator) HParameterValue(0, Primitive::kPrimNot);
  entry->AddInstruction(parameter);

  HBasicBlock* block = new (&allocator) HBasicBlock(graph);
  graph->AddBlock(block);
  entry->AddSuccessor(block);

  block->AddInstruction(
      new (&allocator) HInstanceFieldGet(parameter, Primitive::kPrimNot, MemberOffset(42)));
  block->AddInstruction(
      new (&allocator) HInstanceFieldGet(parameter, Primitive::kPrimNot, MemberOffset(42)));
  HInstruction* to_remove = block->GetLastInstruction();
  block->AddInstruction(
      new (&allocator) HInstanceFieldGet(parameter, Primitive::kPrimNot, MemberOffset(43)));
  HInstruction* different_offset = block->GetLastInstruction();
  // Kill the value.
  block->AddInstruction(new (&allocator) HInstanceFieldSet(
      &allocator, parameter, parameter, Primitive::kPrimNot, MemberOffset(42)));
  block->AddInstruction(
      new (&allocator) HInstanceFieldGet(parameter, Primitive::kPrimNot, MemberOffset(42)));
  HInstruction* use_after_kill = block->GetLastInstruction();
  block->AddInstruction(new (&allocator) HExit());

  ASSERT_EQ(to_remove->GetBlock(), block);
  ASSERT_EQ(different_offset->GetBlock(), block);
  ASSERT_EQ(use_after_kill->GetBlock(), block);

  graph->BuildDominatorTree();
  graph->TransformToSSA();
  GlobalValueNumberer(&allocator, graph).Run();

  ASSERT_TRUE(to_remove->GetBlock() == nullptr);
  ASSERT_EQ(different_offset->GetBlock(), block);
  ASSERT_EQ(use_after_kill->GetBlock(), block);
}

TEST(GVNTest, GlobalFieldElimination) {
  ArenaPool pool;
  ArenaAllocator allocator(&pool);

  HGraph* graph = new (&allocator) HGraph(&allocator);
  HBasicBlock* entry = new (&allocator) HBasicBlock(graph);
  graph->AddBlock(entry);
  graph->SetEntryBlock(entry);
  HInstruction* parameter = new (&allocator) HParameterValue(0, Primitive::kPrimNot);
  entry->AddInstruction(parameter);

  HBasicBlock* block = new (&allocator) HBasicBlock(graph);
  graph->AddBlock(block);
  entry->AddSuccessor(block);
  block->AddInstruction(
      new (&allocator) HInstanceFieldGet(parameter, Primitive::kPrimBoolean, MemberOffset(42)));

  block->AddInstruction(new (&allocator) HIf(block->GetLastInstruction()));
  HBasicBlock* then = new (&allocator) HBasicBlock(graph);
  HBasicBlock* else_ = new (&allocator) HBasicBlock(graph);
  HBasicBlock* join = new (&allocator) HBasicBlock(graph);
  graph->AddBlock(then);
  graph->AddBlock(else_);
  graph->AddBlock(join);

  block->AddSuccessor(then);
  block->AddSuccessor(else_);
  then->AddSuccessor(join);
  else_->AddSuccessor(join);

  then->AddInstruction(
      new (&allocator) HInstanceFieldGet(parameter, Primitive::kPrimBoolean, MemberOffset(42)));
  then->AddInstruction(new (&allocator) HGoto());
  else_->AddInstruction(
      new (&allocator) HInstanceFieldGet(parameter, Primitive::kPrimBoolean, MemberOffset(42)));
  else_->AddInstruction(new (&allocator) HGoto());
  join->AddInstruction(
      new (&allocator) HInstanceFieldGet(parameter, Primitive::kPrimBoolean, MemberOffset(42)));
  join->AddInstruction(new (&allocator) HExit());

  graph->BuildDominatorTree();
  graph->TransformToSSA();
  GlobalValueNumberer(&allocator, graph).Run();

  // Check that all field get instructions have been GVN'ed.
  ASSERT_TRUE(then->GetFirstInstruction()->IsGoto());
  ASSERT_TRUE(else_->GetFirstInstruction()->IsGoto());
  ASSERT_TRUE(join->GetFirstInstruction()->IsExit());
}

TEST(GVNTest, LoopFieldElimination) {
  ArenaPool pool;
  ArenaAllocator allocator(&pool);

  HGraph* graph = new (&allocator) HGraph(&allocator);
  HBasicBlock* entry = new (&allocator) HBasicBlock(graph);
  graph->AddBlock(entry);
  graph->SetEntryBlock(entry);

  HInstruction* parameter = new (&allocator) HParameterValue(0, Primitive::kPrimNot);
  HInstruction* parameter2 = new (&allocator) HParameterValue(1, Primitive::kPrimNot);
  entry->AddInstruction(parameter);
  entry->AddInstruction(parameter2);

  HBasicBlock* block = new (&allocator) HBasicBlock(graph);
  graph->AddBlock(block);
  entry->AddSuccessor(block);
  block->AddInstruction(
      new (&allocator) HInstanceFieldGet(parameter, Primitive::kPrimBoolean, MemberOffset(42)));
  block->AddInstruction(new (&allocator) HGoto());

  HBasicBlock* loop_header = new (&allocator) HBasicBlock(graph);
  HBasicBlock* loop_body = new (&allocator) HBasicBlock(graph);
  HBasicBlock* exit = new (&allocator) HBasicBlock(graph);

  graph->AddBlock(loop_header);
  graph->AddBlock(loop_body);
  graph->AddBlock(exit);
  block->AddSuccessor(loop_header);
  loop_header->AddSuccessor(loop_body);
  loop_header->AddSuccessor(exit);
  loop_body->AddSuccessor(loop_header);

  loop_header->AddInstruction(
      new (&allocator) HInstanceFieldGet(parameter, Primitive::kPrimBoolean, MemberOffset(42)));
  HInstruction* field_get_in_loop_header = loop_header->GetLastInstruction();
  loop_header->AddInstruction(new (&allocator) HIf(block->GetLastInstruction()));

  // Kill inside the loop body to prevent field gets inside the loop header
  // and the body to be GVN'ed.
  loop_body->AddInstruction(new (&allocator) HInstanceFieldSet(
      &allocator, parameter, parameter, Primitive::kPrimNot, MemberOffset(42)));
  HInstruction* field_set = loop_body->GetLastInstruction();

  // Add an HArraySet instruction to make sure it doesn't interfere
  // with instance field stores.
  HInstruction* int_value = new (&allocator) HIntConstant(0);
  loop_body->AddInstruction(int_value);
  loop_body->AddInstruction(new (&allocator) HArraySet(
      parameter2, int_value, int_value, Primitive::kPrimInt, 10));

  loop_body->AddInstruction(
      new (&allocator) HInstanceFieldGet(parameter, Primitive::kPrimBoolean, MemberOffset(42)));
  HInstruction* field_get_in_loop_body = loop_body->GetLastInstruction();
  loop_body->AddInstruction(new (&allocator) HGoto());

  exit->AddInstruction(
      new (&allocator) HInstanceFieldGet(parameter, Primitive::kPrimBoolean, MemberOffset(42)));
  HInstruction* field_get_in_exit = exit->GetLastInstruction();
  exit->AddInstruction(new (&allocator) HExit());

  ASSERT_EQ(field_get_in_loop_header->GetBlock(), loop_header);
  ASSERT_EQ(field_get_in_loop_body->GetBlock(), loop_body);
  ASSERT_EQ(field_get_in_exit->GetBlock(), exit);

  graph->BuildDominatorTree();
  graph->TransformToSSA();
  SsaRedundantPhiElimination(graph).Run();
  graph->FindNaturalLoops();
  GlobalValueNumberer(&allocator, graph).Run();

  // Check that all field get instructions are still there.
  ASSERT_EQ(field_get_in_loop_header->GetBlock(), loop_header);
  ASSERT_EQ(field_get_in_loop_body->GetBlock(), loop_body);
  // The exit block is dominated by the loop header, whose field get
  // does not get killed by the loop flags.
  ASSERT_TRUE(field_get_in_exit->GetBlock() == nullptr);

  // Now remove the field set, and check that all field get instructions have been GVN'ed.
  SsaDeadPhiElimination(graph).RemoveStorePhis();
  loop_body->RemoveInstruction(field_set);
  graph->TransformToSSA();
  SsaRedundantPhiElimination(graph).Run();
  graph->FindNaturalLoops();
  GlobalValueNumberer(&allocator, graph).Run();

  ASSERT_TRUE(field_get_in_loop_header->GetBlock() == nullptr);
  ASSERT_TRUE(field_get_in_loop_body->GetBlock() == nullptr);
  ASSERT_TRUE(field_get_in_exit->GetBlock() == nullptr);
}

}  // namespace art
