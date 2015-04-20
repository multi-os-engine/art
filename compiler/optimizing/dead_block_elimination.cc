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

#include "dead_block_elimination.h"

namespace art {

typedef std::pair<HBasicBlock*, HBasicBlock*> BasicBlockPair;

static void VisitAllLiveBlocks(HBasicBlock* block, ArenaBitVector* visited) {
  int block_id = block->GetBlockId();
  if (visited->IsBitSet(block_id)) {
    return;
  }
  visited->SetBit(block_id);

  HInstruction* last_instruction = block->GetLastInstruction();
  if (last_instruction->IsIf()) {
    HInstruction* condition = last_instruction->InputAt(0);
    if (!condition->IsIntConstant()) {
      VisitAllLiveBlocks(last_instruction->AsIf()->IfTrueSuccessor(), visited);
      VisitAllLiveBlocks(last_instruction->AsIf()->IfFalseSuccessor(), visited);
    } else if (condition->AsIntConstant()->IsOne()) {
      VisitAllLiveBlocks(last_instruction->AsIf()->IfTrueSuccessor(), visited);
    } else {
      DCHECK(condition->AsIntConstant()->IsZero());
      VisitAllLiveBlocks(last_instruction->AsIf()->IfFalseSuccessor(), visited);
    }
  } else {
    for (size_t i = 0, e = block->GetSuccessors().Size(); i < e; ++i) {
      VisitAllLiveBlocks(block->GetSuccessors().Get(i), visited);
    }
  }
}

void HDeadBlockElimination::Run() {
  // Classify blocks as live/dead.
  ArenaAllocator* allocator = graph_->GetArena();
  ArenaBitVector live_blocks(allocator, graph_->GetBlocks().Size(), false);
  VisitAllLiveBlocks(graph_->GetEntryBlock(), &live_blocks);

  // Remove all dead blocks. Iterate in post order to remove dominees before
  // their dominators.
  for (HPostOrderIterator it(*graph_); !it.Done(); it.Advance()) {
    HBasicBlock* block  = it.Current();
    if (live_blocks.IsBitSet(block->GetBlockId())) {
      continue;
    }
    block->DisconnectFromAll();
    graph_->DeleteDeadBlock(block);
  }

  // Merge blocks whenever possible.
  for (size_t i = 0, e = graph_->GetBlocks().Size(); i < e; ++i) {
    HBasicBlock* block  = graph_->GetBlocks().Get(i);
    if (block == nullptr
        || block->IsEntryBlock()
        || block->GetSuccessors().Size() != 1u) {
      continue;
    }

    HBasicBlock* successor = block->GetSuccessors().Get(0);
    if (successor->IsExitBlock()
        || successor->GetPredecessors().Size() != 1u) {
      continue;
    }

    block->RemoveInstruction(block->GetLastInstruction());
    block->MergeWith(successor);
    graph_->DeleteDeadBlock(successor);
    --i;
  }
}

}  // namespace art
