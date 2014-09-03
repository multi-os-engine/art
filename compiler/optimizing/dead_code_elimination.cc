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

#include "dead_code_elimination.h"

#include "base/bit_vector-inl.h"

namespace art {

void DeadCodeElimination::Run() {
  const GrowableArray<HBasicBlock*>& blocks = graph_->GetBlocks();

  // Identify leaf nodes in the dominator tree.  We actually register
  // non-leaf nodes, as this enables us to use
  // ArenaBitVector::ClearAllBits() to initialize the bit vector
  // (ArenaBitVector does not provide a SetAllBits() method for
  // implementation reasons).
  ArenaBitVector is_non_leaf(graph_->GetArena(), blocks.Size(), false);
  is_non_leaf.ClearAllBits();
  for (size_t i = 0 ; i < blocks.Size(); ++i) {
    // An (immediate) dominator is by definition a non-leaf node.
    HBasicBlock* dominator = blocks.Get(i)->GetDominator();
    if (dominator != nullptr
        && is_non_leaf.IsBitSet(dominator->GetBlockId())) {
      is_non_leaf.SetBit(dominator->GetBlockId());
    }
  }

  // Sort nodes in reverse level-order in the dominator tree (i.e.,
  // all leaves first, then the parents of these nodes, then their
  // parents, etc. up to the root node).
  GrowableArray<HBasicBlock*> dominator_reverse_level_order(graph_->GetArena(),
                                                            blocks.Size());
  // Auxiliary bit vector to keep track of nodes (blocks) inserted in
  // `dominator_reverse_level_order`.
  ArenaBitVector inserted(graph_->GetArena(), blocks.Size(), false);
  inserted.ClearAllBits();

  // Insert the leaves of the dominator tree.
  for (size_t i = 0 ; i < blocks.Size(); ++i) {
    if (!is_non_leaf.IsBitSet(i)) {
      dominator_reverse_level_order.Insert(blocks.Get(i));
      inserted.SetBit(i);
    }
  }

  // Browse, process and populate blocks registered in
  // `dominator_reverse_level_order`.
  for (size_t j = 0 ; j < dominator_reverse_level_order.Size(); ++j) {
    HBasicBlock* block = dominator_reverse_level_order.Get(j);
    // Traverse this block's instructions in reverse order and remove
    // the unused ones.
    for (HBackwardInstructionIterator it(block->GetInstructions());
         !it.Done(); it.Advance()) {
      HInstruction* inst = it.Current();
      if (!inst->HasSideEffects() && !inst->HasUses()) {
        block->RemoveInstruction(inst);
      }
    }
    // Add parent node (dominator block) to the list of blocks to be
    // processed, if it has not yet been queued.
    HBasicBlock* dominator = block->GetDominator();
    if (dominator != nullptr && !inserted.IsBitSet(dominator->GetBlockId())) {
      dominator_reverse_level_order.Insert(dominator);
      inserted.SetBit(dominator->GetBlockId());
    }
  }
}

}  // namespace art
