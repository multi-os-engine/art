/*
 * Copyright (C) 2014 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http:// www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "compiler_internals.h"
#include "dataflow_iterator-inl.h"
#include "loop_formation.h"
#include "loop_information.h"
#include "bit_vector_block_iterator.h"

namespace art {
  // The find loop implementation is a wrapper around the LoopInformation API.
  void FindLoops::Start(CompilationUnit *c_unit) const {
    // We want to build the loop information.
    c_unit->loop_information = LoopInformation::GetLoopInformation(c_unit);
  }

  void FormLoops::Start(CompilationUnit *c_unit) const {
    LoopInformation* loop_info = c_unit->loop_information;

    if (loop_info != nullptr) {
      loop_info->Iterate(Worker);
    }
  }

  bool FormLoops::Worker(LoopInformation* loop_info, void* data) {
    MIRGraph* mir_graph = loop_info->GetCompilationUnit()->mir_graph.get();

    // First we'd like to ensure that some of back branches links to out.
    // It might require loop transformation.
    BasicBlock* entry = HandleTopLoop(mir_graph, loop_info);

    // Update loop information with new entry.
    loop_info->SetEntryBlock(entry);

    // For each exit we should add an exit basic block.
    BitVectorBlockIterator exit_iter(loop_info->GetExitLoops(), mir_graph);
    for (BasicBlock* not_loop = exit_iter.Next(); not_loop != nullptr; not_loop = exit_iter.Next()) {
      // Now go through all the predecessors. We need to insert an exit BB solely if predecessor is in loop.
      GrowableArray<BasicBlockId>::Iterator predecessor_iter(not_loop->predecessors);

      for (BasicBlockId pred_id = predecessor_iter.Next(); pred_id != NullBasicBlockId; pred_id = predecessor_iter.Next()) {
        bool predecessor_is_in_loop = loop_info->GetBasicBlocks()->IsBitSet(pred_id);

        if (predecessor_is_in_loop) {
          BasicBlock* out = mir_graph->GetBasicBlock(pred_id);
          BasicBlock* loop_exit_block = mir_graph->CreateNewBB(kDalvikByteCode);

          loop_exit_block->start_offset = out->start_offset;
          mir_graph->InsertBasicBlockBetween(loop_exit_block->id, out->id, not_loop->id);
        }
      }
    }

    // Finally add a pre-loop header.
    InsertPreLoopHeader(mir_graph, loop_info, entry);

    return true;
  }

  void FormLoops::InsertPreLoopHeader(MIRGraph* mir_graph, LoopInformation* loop_info, BasicBlock* entry) {
    BasicBlock* preheader = mir_graph->CreateNewBB(kDalvikByteCode);
    preheader->start_offset = entry->start_offset;

    // Now go through all the predecessors.
    GrowableArray<BasicBlockId>::Iterator predecessor_iter(entry->predecessors);
    for (BasicBlockId pred_id = predecessor_iter.Next();
         pred_id != NullBasicBlockId;
         pred_id = predecessor_iter.Next()) {
      BasicBlock* predecessor = mir_graph->GetBasicBlock(pred_id);

      if (loop_info->Contains(predecessor) == false) {
        mir_graph->InsertBasicBlockBetween(preheader->id, predecessor->id, entry->id, false);
      }
    }

    // Now we finished setting linking all entry predecessors to loop preheader.
    // Thus, we finally just now make the pre-header the entry's predecessor.
    preheader->predecessors->Insert(entry->id);
  }

  BasicBlock* FormLoops::HandleTopLoop(MIRGraph* mir_graph, LoopInformation* loop_info) {
    BasicBlock* entry = loop_info->GetEntryBlock();
    ArenaBitVector* loop_blocks = loop_info->GetBasicBlocks();
    ArenaBitVector* tail_blocks = loop_info->GetBackwardBranches();
    ArenaBitVector* loop_exit_blocks = loop_info->GetExitLoops();

    // No outs => nothing to do.
    if (loop_exit_blocks->NumSetBits() == 0) {
      return entry;
    }

    // Entry is a tail block => we are not a top loop.
    if (tail_blocks->IsBitSet(entry->id) == true) {
      return entry;
    }

    // Entry does not link to out => we are not a top loop.
    if (IsTransformationRequired(loop_exit_blocks, entry) == false) {
      return entry;
    }

    // All BB points to out => no need to transform because it would be infinite loop.
    bool not_all_go = true;
    BitVectorBlockIterator loop_blocks_iter(loop_blocks, mir_graph);
    for (BasicBlock* in_loop = loop_blocks_iter.Next(); not_all_go == true && in_loop != nullptr; in_loop = loop_blocks_iter.Next()) {
      // Does this block go to an exit?
      ChildBlockIterator successor(in_loop, mir_graph);
      for (BasicBlock* child = successor.Next(); child != nullptr; child = successor.Next()) {
        if (loop_exit_blocks->IsBitSet(child->id) == false) {
          not_all_go = false;
          break;
        }
      }
    }

    // All BBs from the loop go to an exit.
    if (not_all_go == true) {
      return entry;
    }

    // Let's transform top loop.
    while (IsTransformationRequired(loop_exit_blocks, entry) == true) {
      BasicBlock* in_loop_bb = mir_graph->GetBasicBlock(entry->fall_through);
      BasicBlock* not_in_loop_bb = mir_graph->GetBasicBlock(entry->taken);

      if (loop_exit_blocks->IsBitSet(entry->taken) == false) {
        std::swap(in_loop_bb, not_in_loop_bb);
      }

      // If inLoop is an entry of other loop we do not want to make it an entry of our loop.
      // Instead of that we add empty basic block to be loop entry.
      if (loop_info->GetLoopInformationByEntry(in_loop_bb) != nullptr) {
        BasicBlock* empty = mir_graph->CreateNewBB(kDalvikByteCode);
        mir_graph->InsertBasicBlockBetween(empty->id, entry->id, in_loop_bb->id);
        loop_blocks->SetBit(empty->id);
        in_loop_bb = empty;
      }

      // Copy entry to make it a tail block.
      BasicBlock* new_bb = entry->Copy(mir_graph);

      // Update the predecessor information.
      in_loop_bb->predecessors->Insert(new_bb->id);
      not_in_loop_bb->predecessors->Insert(new_bb->id);

      // Now all tail blocks should be re-directed to new loop tail block (old loop entry).
      BitVectorBlockIterator tail_iter(tail_blocks, mir_graph);
      for (BasicBlock* tail_bb = tail_iter.Next(); tail_bb != nullptr; tail_bb = tail_iter.Next()) {
        // Attach backedge to newBB.
        if (tail_bb->taken == entry->id) {
          tail_bb->taken = new_bb->id;
        }

        if (tail_bb->fall_through == entry->id) {
          tail_bb->fall_through = new_bb->id;
        }

        // Update the predecessor information.
        new_bb->predecessors->Insert(tail_bb->id);
        entry->predecessors->Delete(tail_bb->id);
      }

      // Old entry is not in a loop now, while new one it is.
      loop_blocks->ClearBit(entry->id);
      loop_blocks->SetBit(new_bb->id);
      entry = in_loop_bb;

      // Now we have only one new block tail.
      tail_blocks->ClearAllBits();
      tail_blocks->SetBit(new_bb->id);
    }

    return entry;
  }

  bool FormLoops::IsTransformationRequired(const BitVector* not_loop, const BasicBlock* entry) {
    // We do not want to transform complex top loop now.
    // So we will work with top loop in this case.
    if (entry->successor_block_list_type != kNotUsed) {
      return false;
    }

    // Loop entry has a taken and it is not in our loop => we want to transform this top loop.
    if ((entry->taken != NullBasicBlockId) && (not_loop->IsBitSet(entry->taken) == true)) {
      return true;
    }

    // Loop entry has a fallThrough and it is not in our loop => we want to transform this top loop.
    if ((entry->fall_through != NullBasicBlockId) && (not_loop->IsBitSet(entry->fall_through) == true)) {
      return true;
    }

    // Loop entry does not lead to out of loop => so we consider this as bottom loop.
    // Note in the future it might be interesting to transform the following loop.
    //      BB1 (loop entry), BB2 (leads to out), BB3 (backward).
    // to bottom loop.
    //      BB1, BB2 (leads to out), BB3 (new loop entry), BB1_copy, BB2_copy (new backward to BB4).
    // But it is too complex for now
    return false;
  }

}  // namespace art
