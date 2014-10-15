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

#include "gvn.h"

namespace art {

void GlobalValueNumberer::Run() {
  sets_.Put(graph_->GetEntryBlock()->GetBlockId(), new (allocator_) ValueSet(allocator_));

  // Do reverse post order to ensure the non back-edge predecessors of a block are
  // visited before the block itself.
  for (HReversePostOrderIterator it(*graph_); !it.Done(); it.Advance()) {
    VisitBasicBlock(it.Current());
  }
}

void GlobalValueNumberer::VisitBasicBlock(HBasicBlock* block) {
  if (kIsDebugBuild) {
    // Check that all non back-edge processors have been visited.
    for (size_t i = 0, e = block->GetPredecessors().Size(); i < e; ++i) {
      HBasicBlock* predecessor = block->GetPredecessors().Get(i);
      DCHECK(visited_.Get(predecessor->GetBlockId())
             || (block->GetLoopInformation() != nullptr
                 && (block->GetLoopInformation()->GetBackEdges().Get(0) == predecessor)));
    }
    visited_.Put(block->GetBlockId(), true);
  }

  ValueSet* set = sets_.Get(block->GetBlockId());

  HInstruction* current = block->GetFirstInstruction();
  while (current != nullptr) {
    // Save the next instruction in case `current` is removed from the graph.
    HInstruction* next = current->GetNext();
    if (current->CanBeMoved()) {
      HInstruction* existing = set->Lookup(current);
      if (existing != nullptr) {
        current->ReplaceWith(existing);
        current->GetBlock()->RemoveInstruction(current);
      } else {
        set->Add(current);
      }
    }
    current = next;
  }

  if (block == graph_->GetEntryBlock()) {
    // The entry block should only accumulate constant instructions, and
    // the builder puts constants only in the entry block.
    // Therefore, there is no need to propagate the value set to the next block.
    DCHECK_EQ(block->GetDominatedBlocks().Size(), 1u);
    HBasicBlock* dominated = block->GetDominatedBlocks().Get(0);
    sets_.Put(dominated->GetBlockId(), new (allocator_) ValueSet(allocator_));
    return;
  }

  // Copy the value set to dominated blocks. We can re-use
  // the current set for the last dominated block because we are done visiting
  // this block.
  for (size_t i = 0, e = block->GetDominatedBlocks().Size(); i < e; ++i) {
    HBasicBlock* dominated = block->GetDominatedBlocks().Get(i);
    sets_.Put(dominated->GetBlockId(), i == e - 1 ? set : set->Copy());
  }
}
}  // namespace art
