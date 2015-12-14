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

#include "select_generator.h"

namespace art {

// Returns true if `block` ends with a Goto and contains at most one other
// movable instruction with no side-effects.
static bool IsSimpleBlock(HBasicBlock* block) {
  DCHECK(block->EndsWithControlFlowInstruction());
  HInstruction* first_insn = block->GetFirstInstruction();
  HInstruction* last_insn = block->GetLastInstruction();

  if (last_insn->IsGoto()) {
    if (first_insn == last_insn) {
      return true;
    } else if (first_insn->GetNext() == last_insn) {
      return first_insn->CanBeMoved() && !first_insn->GetSideEffects().HasSideEffects();
    }
  }

  return false;
}

// Returns true if 'block1' and 'block2' are empty, merge into the same single
// successor and the successor can only be reached from them.
static bool BlocksMergeTogether(HBasicBlock* block1, HBasicBlock* block2) {
  return block1->GetSingleSuccessor() == block2->GetSingleSuccessor();
}

void HSelectGenerator::Run() {
  // Iterate in post order in the unlikely case that removing one occurrence of
  // the selection pattern empties a branch block of another occurrence.
  // Otherwise the order does not matter.
  for (HPostOrderIterator it(*graph_); !it.Done(); it.Advance()) {
    HBasicBlock* block = it.Current();
    if (!block->EndsWithIf()) continue;

    // Find elements of the diamond pattern.
    HIf* if_instruction = block->GetLastInstruction()->AsIf();
    HBasicBlock* true_block = if_instruction->IfTrueSuccessor();
    HBasicBlock* false_block = if_instruction->IfFalseSuccessor();
    DCHECK_NE(true_block, false_block);
    if (!IsSimpleBlock(true_block) ||
        !IsSimpleBlock(false_block) ||
        !BlocksMergeTogether(true_block, false_block)) {
      continue;
    }

    HBasicBlock* merge_block = true_block->GetSingleSuccessor();
    if (!merge_block->HasSinglePhi() ||
        // TODO: The following constraint can be lifted.
        merge_block->GetPredecessors().size() != 2u) {
      continue;
    }

    size_t predecessor_index_true = merge_block->GetPredecessorIndexOf(true_block);
    size_t predecessor_index_false = 1 - predecessor_index_true;
    DCHECK_EQ(predecessor_index_false, merge_block->GetPredecessorIndexOf(false_block));

    HPhi* phi = merge_block->GetFirstPhi()->AsPhi();
    HInstruction* true_value = phi->InputAt(predecessor_index_true);
    HInstruction* false_value = phi->InputAt(predecessor_index_false);

    // Create the Select instruction and insert it after the Phi.
    HSelect* select = new (graph_->GetArena()) HSelect(if_instruction->InputAt(0),
                                                       true_value,
                                                       false_value,
                                                       if_instruction->GetDexPc());
    if (phi->GetType() == Primitive::kPrimNot) {
      select->SetReferenceTypeInfo(phi->GetReferenceTypeInfo());
    }
    merge_block->InsertInstructionBefore(select, merge_block->GetFirstInstruction());

    // Replace the selection outcome with the new instruction.
    phi->ReplaceWith(select);
    merge_block->RemovePhi(phi);

    // If `true_block` has an instruction we need to move out, do it now. We do not
    // need to do the same for `false_block` because it will get merged with `block`.
    if (!true_block->IsSingleGoto()) {
      true_block->MoveInstructionBefore(true_block->GetFirstInstruction(), if_instruction);
    }
    DCHECK(true_block->IsSingleGoto());

    // Delete the true branch and merge the resulting chain of blocks
    // 'block->false_block->merge_block' into one.
    true_block->DisconnectAndDelete();
    block->MergeWith(false_block);
    block->MergeWith(merge_block);

    // No need to update any dominance information, as we are simplifying
    // a simple diamond shape, where the join block is merged with the
    // entry block. Any following blocks would have had the join block
    // as a dominator, and `MergeWith` handles changing that to the
    // entry block.
  }
}

}  // namespace art
