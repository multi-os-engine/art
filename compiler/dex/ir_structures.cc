/*
 * Copyright (C) 2013 The Android Open Source Project
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

#include "ir_structures.h"
#include "mir_graph.h"

namespace art {

bool BasicBlock::RemoveMIR(MIR* mir) {
  if (mir == 0) {
    return false;
  }

  // Find the MIR, and the one before it if they exist
  MIR* current = 0;
  MIR* prev = 0;

  for (current = first_mir_insn; current != 0; prev = current, current = current->next) {
    // If found: break
    if (current == mir) {
      break;
    }
  }

  // Did we find it?
  if (current != 0) {
    MIR* next = current->next;

    // Just update the links of prev and next and current is almost gone
    if (prev != 0) {
      prev->next = next;
    }

    // Exceptions are if first or last mirs are invoke
    if (first_mir_insn == current) {
      first_mir_insn = next;
    }

    if (last_mir_insn == current) {
      last_mir_insn = prev;
    }

    // Found it and removed it
    return true;
  }

  // We did not find it
  return false;
}

bool MIR::RemoveFromBasicBlock() {
  // Punt to the BasicBlock handler
  if (bb != nullptr) {
    return bb->RemoveMIR(this);
  }
  return false;
}

void BasicBlock::ResetOptimizationFlags(uint16_t reset_flags) {
  // Reset flags for all MIRs in bb
  for (MIR *mir = first_mir_insn; mir != NULL; mir = mir->next) {
    mir->optimization_flags &= (~reset_flags);
  }
}

ChildBlockIterator::ChildBlockIterator(BasicBlock *bb, MIRGraph *mir_graph) {
    basic_block_ = bb;
    mir_graph_ = mir_graph;

    // We have not yet visited any of the children.
    visited_fallthrough_ = false;
    visited_taken_ = false;

    // Check if we have successors.
    if (basic_block_ != 0 && basic_block_->successor_block_list_type != kNotUsed) {
        have_successors_ = true;

        successor_iter_ = new (mir_graph->GetArena()) GrowableArray<SuccessorBlockInfo*>::Iterator(bb->successor_blocks);
    } else {
        // We have no successors if the block list is unused.
        have_successors_ = false;
    }
}

BasicBlock* ChildBlockIterator::GetNextChildPtr(void) {
    // We check if we have a basic block. If we don't we cannot get next child.
    if (basic_block_ == 0) {
        return nullptr;
    }

    // If we haven't visited fallthrough, return that.
    if (visited_fallthrough_ == false) {
        visited_fallthrough_ = true;

        BasicBlock* result = mir_graph_->GetBasicBlock(basic_block_->fall_through);
        if (result != nullptr) {
            return result;
        }
    }

    // If we haven't visited taken, return that.
    if (visited_taken_ == false) {
        visited_taken_ = true;

        BasicBlock* result = mir_graph_->GetBasicBlock(basic_block_->taken);
        if (result != nullptr) {
            return result;
        }
    }

    // We visited both taken and fallthrough. Now check if we have successors we need to visit.
    if (have_successors_ == true) {
        // Get information about next successor block.
        SuccessorBlockInfo *successor_block_info = successor_iter_->Next();

        // If we don't have anymore successors, return 0.
        if (successor_block_info != nullptr) {
          return mir_graph_->GetBasicBlock(successor_block_info->block);
        }
    }

    // We do not have anything.
    return nullptr;
}

}  // namespace art
