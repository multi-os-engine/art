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

#include "generate_selects.h"

namespace art {

static bool IsBlockTooComplexForSelect(HBasicBlock* block) {
  HInstruction *instr = block->GetFirstInstruction();
  if (!block->GetPhis().IsEmpty()) {
    return true;
  }

  if (instr->IsGoto()) {
    // Empty block.
    return false;
  }

  HInstruction* next_instr = instr->GetNext();
  HInstruction* last_instr = block->GetLastInstruction();
  if (next_instr != last_instr || !last_instr->IsGoto()) {
    // More than one instruction in the block.
    return true;
  }

  // Don't generate a select for a dangerous instruction, such as an invoke, volatile,
  // write, etc.
  return (!instr->CanBeMoved()) || instr->HasSideEffects();
}

static bool ShouldCreateSelect(HBasicBlock* true_block,
                               HBasicBlock* false_block) {
  // Allow up to 1 'Movable' instruction in each block.
  if (IsBlockTooComplexForSelect(true_block) || IsBlockTooComplexForSelect(false_block)) {
    return false;
  }

  // Simple enough to merge.
  return true;
}

static bool NotFromSameIf(HBasicBlock* pred1, HBasicBlock* pred2) {
  // They have to come from the same block.
  if (pred1 != pred2) {
    return true;
  }

  // The block has to end with an HIf.
  return !pred1->GetLastInstruction()->IsIf();
}

void HGenerateSelects::TryGeneratingSelects(HBasicBlock* block)
     SHARED_REQUIRES(Locks::mutator_lock_) {
  // See if this came from an HIf pattern.
  const ArenaVector<HBasicBlock*>& predecessors = block->GetPredecessors();
  if (predecessors.size() != 2u) {
    return;
  }
  HBasicBlock* first_pred = block->GetPredecessors().at(0);
  HBasicBlock* second_pred = block->GetPredecessors().at(1);

  // Is this a simple diamond from an HIf?
  if (first_pred->GetPredecessors().size() != 1u ||
      second_pred->GetPredecessors().size() != 1u ||
      NotFromSameIf(first_pred->GetPredecessors().at(0),
                    second_pred->GetPredecessors().at(0))) {
    return;
  }

  // Find elements of the diamond pattern.  This represents an if/else or an if statement.
  HBasicBlock* if_block = first_pred->GetPredecessors().at(0);
  HIf* if_instruction = if_block->GetLastInstruction()->AsIf();
  DCHECK(if_instruction != nullptr);
  HBasicBlock* true_block = if_instruction->IfTrueSuccessor();
  HBasicBlock* false_block = if_instruction->IfFalseSuccessor();

  DCHECK(block->HasSinglePhi());
  HPhi* phi = block->GetFirstPhi()->AsPhi();
  HInstruction* true_value = phi->InputAt(block->GetPredecessorIndexOf(true_block));
  HInstruction* false_value = phi->InputAt(block->GetPredecessorIndexOf(false_block));

  HInstruction* if_condition = if_instruction->InputAt(0);
  if (!if_condition->IsCondition()) {
    return;
  }

  // Can the backend handle these types?
  Primitive::Type cond_type = if_condition->InputAt(0)->GetType();
  Primitive::Type value_type = phi->GetType();
  if (!SupportsSelect(cond_type, value_type)) {
    return;
  }

  // Only create Selects for small blocks.
  if (!ShouldCreateSelect(true_block, false_block)) {
    return;
  }

  HConditionalSelect* replacement =
      new (graph_->GetArena()) HConditionalSelect(if_condition->AsCondition(), true_value, false_value);
  block->InsertInstructionBefore(replacement, block->GetFirstInstruction());

  // Replace the phi with the new Select instruction.
  phi->ReplaceWith(replacement);
  block->RemovePhi(phi);

  // Merge 'if_block->true_block->false_block->block' into one. This ensures that any
  // code in the blocks are merged.
  // Move any non-goto instruction from true_block and delete it. This is because
  // MergeWith can't handle the true block due to the number of successors.
  HInstruction* true_instruction = true_block->GetFirstInstruction();
  if (!true_instruction->IsGoto()) {
    if_block->MoveInstructionBefore(true_instruction, if_instruction);
  }
  true_block->DisconnectAndDelete();
  if_block->MergeWith(false_block);
  if_block->MergeWith(block);

  // No need to update any dominance information, as we are simplifying
  // a simple diamond shape, where the join block is merged with the
  // entry block. Any following blocks would have had the join block
  // as a dominator, and `MergeWith` handles changing that to the
  // entry block.

  // Remove the original condition if it is now unused.
  if (!if_condition->HasUses()) {
    if_condition->GetBlock()->RemoveInstructionOrPhi(if_condition);
  }
  MaybeRecordStat(MethodCompilationStat::kGeneratedSelects, 1);
}

void HGenerateSelects::Run() {
  // We may merge blocks, so iterators are dangerous.
  // Walk the blocks in order, handling merged blocks.
  for (size_t i = 0, e = graph_->GetBlocks().size(); i < e; i++) {
    HBasicBlock* block = graph_->GetBlock(i);
    if (block != nullptr &&  block->HasSinglePhi()) {
      TryGeneratingSelects(block);
    }
  }
}

}  // namespace art
