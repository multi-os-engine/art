/*
 * Copyright (C) 2014 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.nullptr (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.nullptr.
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stack>

#include "compiler_internals.h"
#include "dataflow_iterator-inl.h"
#include "dataflow_iterator.h"
#include "induction_variable.h"
#include "loop_information.h"
#include "bit_vector_block_iterator.h"

namespace art {
LoopInformation* (*LoopInformation::allocate_loop_information_)(CompilationUnit*) = nullptr;

LoopInformation::LoopInformation(CompilationUnit* c_unit) {
  Init(c_unit);
}

LoopInformation::~LoopInformation() {
}

void LoopInformation::Init(CompilationUnit* c_unit) {
  this->c_unit_ = c_unit;
  parent_ = nullptr;
  sibling_next_ = nullptr;
  sibling_previous_ = nullptr;
  nested_ = nullptr;
  depth_ = 0;
  basic_blocks_ = nullptr;
  backward_ = nullptr;
  entry_ = nullptr;
  pre_header_ = nullptr;

  // Initialize the BitVectors:
  ArenaAllocator *arena = &(c_unit->arena);
  exit_loop_ = new (arena) ArenaBitVector(arena, 1, true);
  basic_blocks_ = new (arena) ArenaBitVector(arena, 1, true);
}

bool LoopInformation::Contains(const BasicBlock* bb) const {
  // If we don't have any basic blocks or if bb is nil, return false.
  if (basic_blocks_ == nullptr || bb == nullptr) {
    return false;
  }

  // Otherwise check the bit.
  return basic_blocks_->IsBitSet(bb->id);
}

// Update depth_ for loop and nested_ loops.
void LoopInformation::SetDepth(int depth) {
  LoopInformation *info = this;
  while (info != nullptr) {
    info->depth_ = depth;
    if (info->nested_ != nullptr) {
      info->nested_->SetDepth(depth + 1);
    }
    info = info->sibling_next_;
  }
}

/**
 * Add takes a new LoopInformation and determines if info is nested_ with this instance or not.
 * If it is nested_ in this instance, we fill our nested_ information with it.
 * Otherwise, we are nested_ in it and we request it to nest us.
 * The function returns the outer nested_ loop, it can nest any level of a nested_ loop.
 */
LoopInformation *LoopInformation::Add(LoopInformation *info) {
  if (info == this) {
    return this;
  }

  // Do we include the current loop?
  if (Contains(info->GetEntryBlock()) == true) {
    // We contain them, so they should not contain us.
    DCHECK(info->Contains(GetEntryBlock()) == false);

    // Search in the children if anybody includes them.
    if (nested_ == nullptr) {
      nested_ = info;
    } else {
      nested_ = nested_->Add(info);
    }
    nested_->parent_ = this;
    nested_->SetDepth(GetDepth() + 1);
    return this;
  } else {
    // Otherwise, info contains us.
    if (info->Contains(GetEntryBlock()) == true) {
      return info->Add(this);
    } else {
      // It is sibling.
      info->depth_ = GetDepth();
      info->parent_ = this->GetParent();
      info->sibling_next_ = this;
      this->sibling_previous_ = info;
      return info;
    }
  }
}

/**
 * @brief Find all tail blocks to specified basic block.
 * @param bb the basic block which is suggested to be a loop head.
 * @return tail blocks or nullptr if there is no them.
 */
ArenaBitVector* LoopInformation::GetLoopTailBlocks(CompilationUnit* c_unit, BasicBlock* bb) {
  ArenaBitVector* tailblocks = nullptr;

  // If no predecessor information, we are done.
  if (bb->predecessors == nullptr) {
    return nullptr;
  }

  // Iterate through the predecessors.
  GrowableArray<BasicBlockId>::Iterator iter(bb->predecessors);

  while (true) {
    BasicBlock* pred_bb = c_unit->mir_graph.get()->GetBasicBlock(iter.Next());

    if (pred_bb == nullptr) {
      break;
    }

    // If we have no dominator information, we can skip it.
    if (pred_bb->dominators == nullptr) {
      continue;
    }

    if (pred_bb->dominators->IsBitSet(bb->id) == true) {
      // If not yet allocated.
      if (tailblocks == nullptr) {
        ArenaAllocator *arena = &(c_unit->arena);
        tailblocks = new (arena) ArenaBitVector(arena, 1, true);
        tailblocks->ClearAllBits();
      }

      // Set bit.
      tailblocks->SetBit(pred_bb->id);
    }
  }
  return tailblocks;
}

/**
 * @brief Get the one and only backward branch of the loop.
 * @return the backward branch BasicBlock, or 0 if it is not exactly one.
 */
BasicBlock* LoopInformation::GetBackwardBranchBlock() const {
  // Check if we have a backward blocks bitvector (Paranoid).
  if (backward_ == nullptr) {
    return nullptr;
  }

  // Check if we have only one backward block.
  int num_blocks = backward_->NumSetBits();

  if (num_blocks != 1) {
    return nullptr;
  }

  // It is alone, so just take it.
  int idx = backward_->GetHighestBitSet();
  return c_unit_->mir_graph.get()->GetBasicBlock(idx);
}

/**
 * @brief Get the one and only exit block of the loop.
 * @return the exit basic block, or 0 if it is not exactly one.
 */
BasicBlock* LoopInformation::GetExitBlock() const {
  // Check if we have a exit loops bitvector (Paranoid).
  if (exit_loop_ == nullptr) {
    return nullptr;
  }

  // Check if we have only one loop exit.
  int num_blocks = exit_loop_->NumSetBits();

  if (num_blocks != 1) {
    return nullptr;
  }

  // It is alone, so just take it.
  int idx = exit_loop_->GetHighestBitSet();
  return c_unit_->mir_graph.get()->GetBasicBlock(idx);
}

/**
 * @brief Check whether BB is a helper BB for this loop.
 * @details helper BBs are pre-header, backward_ branch and exit of the loop
 * @return true if bb is a pre-header, backward_ branch or exit of the loop.
 */
bool LoopInformation::IsBasicBlockALoopHelper(const BasicBlock* bb) {
  if (bb == nullptr) {
    return false;
  }
  return pre_header_ == bb
    || (exit_loop_ != nullptr && exit_loop_->IsBitSet(bb->id) == true);
}

/**
 * @brief Get the one and only post exit block of the loop.
 * @return the post exit basic block, or 0 if it is not exactly one.
 */
BasicBlock* LoopInformation::GetPostExitBlock() const {
  // Check if we have a backward blocks bitvector (Paranoid).
  if (post_exit_blocks_ == nullptr) {
    return nullptr;
  }

  // Check if we have only one backward block.
  int num_blocks = post_exit_blocks_->NumSetBits();

  if (num_blocks != 1) {
    return nullptr;
  }

  // It is alone, so just take it.
  int idx = post_exit_blocks_->GetHighestBitSet();
  return c_unit_->mir_graph.get()->GetBasicBlock(idx);
}

/**
 * @brief Check if the entry block passed in data is the entry for this loop.
 * @param data Contains a void* casted pair of "entry" BasicBlock and return loopInfo.
 * @return true if this info has the BasicBlock as entry. Also sets data if so.
 */
static bool GetLoopInformationByEntryHelper(LoopInformation *info, void *data) {
  std::pair<BasicBlock*, LoopInformation*> *pair = static_cast<std::pair<BasicBlock*, LoopInformation*> *>(data);

  if (info->GetEntryBlock() == pair->first) {
    pair->second = info;
    return false;
  }
  return true;
}

LoopInformation* LoopInformation::GetLoopInformationByEntry(const BasicBlock* entry) {
  // Fast check.
  if (entry_ == entry) {
    return this;
  }

  // Iterate over all loops.
  std::pair<const BasicBlock*, LoopInformation*> pair(entry, nullptr);

  if (Iterate(GetLoopInformationByEntryHelper, static_cast<void *>(&pair)) == false) {
    return pair.second;
  }

  return nullptr;
}


/**
 * @brief Check if the block passed in data is contained in this loop but in none of the others.
 * @param data Contains a void* casted pair of a block and its associated loop information.
 * @return true if the iteration should continue, false otherwise.
 */
static bool GetLoopInformationByBasicBlockHelper(LoopInformation *info, void *data) {
  std::pair<BasicBlock*, LoopInformation*> *pair = static_cast<std::pair<BasicBlock*, LoopInformation*> *>(data);

  // If we have no pair information, no need to continue.
  if (pair == nullptr) {
    return true;
  }

  BasicBlock* block = pair->first;
  if (info->Contains(block) == true) {
    // Ok, but that is not enough, do none of our nested contain it?
    LoopInformation* nested = info->GetNested();

    while (nested != nullptr) {
      if (nested->Contains(block) == true) {
        // Then continue the iteration because someone else contains it.
        return true;
      }
      nested = nested->GetNextSibling();
    }

    // If we got to this point, we are done, we found the inner loop containing the block.
    pair->second = info;
    return false;
  }

  return true;
}

LoopInformation* LoopInformation::GetLoopInformationByBasicBlock(const BasicBlock* block) {
  // Iterate over all loops.
  std::pair<const BasicBlock*, LoopInformation*> pair(block , nullptr);

  if (Iterate(GetLoopInformationByBasicBlockHelper, static_cast<void *>(&pair)) == false) {
    return pair.second;
  }

  return nullptr;
}

/**
 * @brief Get the Phi node defining a given virtual register
 * @param vr the virtual register we want the phi node from
 * @return nullptr if not found, the MIR otherwise.
 */
MIR* LoopInformation::GetPhiInstruction(int reg) const {
  // Get the BasicBlock vector for this loop.
  ArenaBitVector *blocks = GetBasicBlocks();

  // Iterate through them.
  ArenaBitVector::Iterator iterator(blocks);

  while (true) {
    int idx = iterator.Next();

    if (idx == -1) {
      break;
    }

    BasicBlock *bb = c_unit_->mir_graph.get()->GetBasicBlock(idx);

    DCHECK(bb != nullptr);

    for (MIR *mir = bb->first_mir_insn; mir != nullptr; mir = mir->next) {
      DecodedInstruction &insn = mir->dalvikInsn;

      // Is it a phi node?
      if (insn.opcode == static_cast<Instruction::Code>(kMirOpPhi)) {
        SSARepresentation *ssa = mir->ssa_rep;

        DCHECK(ssa != nullptr && ssa->num_defs == 1);

        // Does it define our vr?
        int ssa_reg = ssa->defs[0];

        // Is it a match?
        if (c_unit_->mir_graph.get()->SRegToVReg(ssa_reg) == reg) {
          /* In a complex CFG we can have several Phi nodes for the same VR.
           * We'd like to find the first one, namely Phi node where the one of
           * uses comes from outside of the loop.
           */
          if (ssa->def_where != nullptr) {
            for (int i = 0; i < ssa->num_uses; i++) {
              MIR *def_mir = ssa->def_where[i];

              // If defMir is nullptr then it is come from outside of trace.
              if (def_mir == nullptr || Contains(def_mir->bb) == false) {
                return mir;
              }
            }
          }
        }
      }
    }
  }

  // Did not find it.
  return nullptr;
}

/**
 * @brief Does the loop have an invoke in it?
 * @return whether the loop has an invoke bytecode.
 */
bool LoopInformation::HasInvoke() const {
  // Get the BasicBlock vector for this loop.
  ArenaBitVector *blocks = GetBasicBlocks();

  // Iterate through them.
  ArenaBitVector::Iterator iterator(blocks);

  while (true) {
    int idx = iterator.Next();

    if (idx == -1) {
      break;
    }

    BasicBlock *bb = c_unit_->mir_graph.get()->GetBasicBlock(idx);

    // Paranoid.
    DCHECK(bb != nullptr);

    // Go through its instructions.
    for (MIR *mir = bb->first_mir_insn; mir != 0; mir = mir->next) {
      // Get instruction.
      DecodedInstruction &insn = mir->dalvikInsn;

      int flags = Instruction::FlagsOf(insn.opcode);

      // Check whether it is an invoke.
      if ((flags & Instruction::kInvoke) != 0) {
        return true;
      }
    }
  }

  // It is fine, no invoke instructions seen.
  return false;
}

bool LoopInformation::ExecutedPerIteration(const BasicBlock *bb) const {
  if (bb == nullptr) {
    return false;
  }

  // Go through the backward blocks.
  ArenaBitVector::Iterator iterator(backward_);

  while (true) {
    int idx = iterator.Next();

    if (idx == -1) {
      break;
    }

    BasicBlock *bwblock = c_unit_->mir_graph.get()->GetBasicBlock(idx);

    DCHECK(bwblock != nullptr);

    // To prove that the mir is executed per iteration, it's block must dominate each backward block.
    if (bwblock->dominators->IsBitSet(bb->id) == false) {
      return false;
    }
  }

  // The BasicBlock is always executed.
  return true;
}

bool LoopInformation::ExecutedPerIteration(MIR* mir) const {
  DCHECK(mir != nullptr);

  // Get the mir's BasicBlock.
  BasicBlock *current = mir->bb;

  // Call the function using the BasicBlock.
  return ExecutedPerIteration(current);
}

/**
 * @brief Find All BB in a loop.
 * @param entry_ loop entry_.
 * @param tailblocks tail blocks to loop entry_.
 * @param basic_blocks_ bit vector to fill with blocks in a loop.
 * @return false if it is not a loop, namely there is a BB which entry_ does not dominate.
 */
bool LoopInformation::GetAllBBInLoop(CompilationUnit* c_unit, BasicBlock* entry_, ArenaBitVector* tailblocks, ArenaBitVector* basic_blocks_) {
  DCHECK(tailblocks != nullptr);
  DCHECK(entry_ != nullptr);
  DCHECK(basic_blocks_ != nullptr);

  // Clear all basicblock bits.
  basic_blocks_->ClearAllBits();

  // Loop entry_ is in a loop.
  basic_blocks_->SetBit(entry_->id);

  // Have a work stack for blocks we want to look at.
  std::stack<BasicBlock*> workStack;

  // Start from tail blocks except entry_ loop if it is a tail block at the same time.
  BitVectorBlockIterator it(tailblocks, c_unit);
  for (BasicBlock* bb = it.Next(); bb != nullptr; bb = it.Next()) {
    if (bb != entry_) {
      workStack.push(bb);
    }
  }

  // Loop entry_ dominates us, so we are safe walking by predecessors stopping by loop entry_.
  while (workStack.empty() != true) {
    BasicBlock* cur = workStack.top();
    workStack.pop();

    // Check if we have domination information, we might not because domination is only created if the block is reachable.
    if (cur->dominators == nullptr) {
      continue;
    }

    if (cur->dominators->IsBitSet(entry_->id) == false) {
      // It is not a normal loop.
      return false;
    }

    // Set bit now.
    basic_blocks_->SetBit(cur->id);

    // Iterate through the predecessors.
    GrowableArray<BasicBlockId>::Iterator iter(cur->predecessors);

    while (true) {
      BasicBlock* pred_bb = c_unit->mir_graph.get()->GetBasicBlock(iter.Next());

      // Are we done?
      if (pred_bb == nullptr) {
        break;
      }

      // If not in the basic block bit vector.
      if (basic_blocks_->IsBitSet(pred_bb->id) == false) {
        workStack.push(pred_bb);
      }
    }
  }

  return true;
}

/**
 * @brief Determine not in a loop's BB with link from loop body.
 * @param basic_blocks_ the basic block forming a loop.
 * @param exitBlocks not in a loop's BBs with link from loop body (to fill).
 */
void LoopInformation::GetOutsFromLoop(CompilationUnit* c_unit, ArenaBitVector* basic_blocks, ArenaBitVector* exit_blocks) {
  DCHECK(basic_blocks != nullptr);
  DCHECK(exit_blocks != nullptr);

  exit_blocks->ClearAllBits();

  // Get MIRGraph.
  MIRGraph *mir_graph = c_unit->mir_graph.get();

  // Iterate over BB in a loop and if its edge comes out of a loop => add it to bit vector.
  BitVectorBlockIterator it(basic_blocks, mir_graph);
  for (BasicBlock* cur = it.Next(); cur != nullptr; cur = it.Next()) {
    // Go through the children and add the child to exit_blocks if not in the loop.
    ChildBlockIterator succ_iter(cur, mir_graph);

    for (BasicBlock* successor = succ_iter.Next(); successor != 0; successor = succ_iter.Next()) {
      if (basic_blocks->IsBitSet(successor->id) == false) {
        exit_blocks->SetBit(successor->id);
      }
    }
  }
}

LoopInformation * LoopInformation::GetLoopInformation(CompilationUnit* c_unit, LoopInformation *current) {
  LoopInformation *result = nullptr;

  AllNodesIterator iterator(c_unit->mir_graph.get());

  // Iterate over all BB.
  for (BasicBlock* bb = iterator.Next(); bb != nullptr; bb = iterator.Next()) {
    // Skip it if it's hidden.
    if (bb->hidden == true) {
      continue;
    }

    // First find all tail blocks.
    ArenaBitVector* tailblocks = GetLoopTailBlocks(c_unit, bb);

    if (tailblocks == nullptr) {
      // bb is not a loop entry_.
      continue;
    }

    // Do we have an old loop information for bb loop entry_?
    ArenaAllocator *arena = &(c_unit->arena);

    // We must create a new LoopInformation, do we have a hook defined for that?
    LoopInformation* info = nullptr;
    if (allocate_loop_information_ != nullptr) {
      info = allocate_loop_information_(c_unit);
    } else {
      info = new (arena) LoopInformation(c_unit);
    }
    info->SetEntryBlock(bb);

    // Set backward_s.
    info->backward_ = tailblocks;

    // Now, Find all BB in a loop.
    if (GetAllBBInLoop(c_unit, bb, tailblocks, info->basic_blocks_) == false) {
      // It is not a normal loop.
      continue;
    }

    // Now, Find out from a loop.
    GetOutsFromLoop(c_unit, info->basic_blocks_, info->exit_loop_);

    // Now, check for pre-header
    // We need to find a predecessor dominating us
    // In correctly formed loop it will be alone
    GrowableArray<BasicBlockId>::Iterator iter(bb->predecessors);
    while (true) {
      BasicBlock* pred_bb = c_unit->mir_graph.get()->GetBasicBlock(iter.Next());
      // Are we done?
      if (pred_bb == nullptr) {
        break;
      }
      // Skip ourselves.
      if (pred_bb == bb) {
        continue;
      }
      if (bb->dominators->IsBitSet(pred_bb->id)) {
        info->pre_header_ = pred_bb;
      }
    }

    // Nest loop information.
    result = (result == nullptr) ? info : result->Add(info);
  }

  return result;
}

bool LoopInformation::DumpInformationHelper(LoopInformation *info, void *data) {
  unsigned int tab = *(static_cast<int*>(data)) + info->GetDepth();

  const unsigned int max_tabs = 10;
  const char* tabs = "          ";

  // Set up tab array.
  tab = (tab > max_tabs) ? max_tabs : tab;

  // Update pointer to tabs.
  tabs += sizeof(tabs) - tab;

  // Print out base information.
  std::ostringstream buffer;

  buffer << tabs << "This: " << info << "\n";

  buffer << tabs << "Depth: " << info->GetDepth() << "\n";

  buffer << tabs << "Entry: " << (info->GetEntryBlock() != nullptr ? info->GetEntryBlock()->id : -1) << "\n";

  buffer << tabs << "PreHeader: " << (info->GetPreHeader() != nullptr ? info->GetPreHeader()->id : -1) << "\n";

  // Print the backward_ chaining blocks.
  buffer << tabs << "Post Loop: ";
  info->GetExitLoops()->Dump(buffer, "");

  // Print the backward_ chaining blocks.
  buffer << tabs << "Backward: ";
  info->GetBackwardBranches()->Dump(buffer, "");

  // Print the BitVector.
  buffer << tabs << "BasicBlocks: ";
  info->GetBasicBlocks()->Dump(buffer, "");

  LOG(INFO) << buffer.str();

  return true;
}

void LoopInformation::DumpInformation(unsigned int tab) {
  void *data = static_cast<void *>(&tab);
  Iterate(DumpInformationHelper, data);
}

bool LoopInformation::Iterate(bool (*func)(LoopInformation *, void *), void *data) {
  LoopInformation *item = this;
  while (item != nullptr) {
    if (func(item, data) == false) {
      return false;
    }
    if (item->nested_ != nullptr) {
      if (item->nested_->Iterate(func, data) == false) {
        return false;
      }
    }
    item = item->sibling_next_;
  }
  return true;
}

bool LoopInformation::CanThrow() const {
  // Iterate through them.
  BitVectorBlockIterator it(GetBasicBlocks(), c_unit_);
  for (BasicBlock* bb = it.Next(); bb != nullptr; bb = it.Next()) {
    // Go through its instructions.
    for (MIR *mir = bb->first_mir_insn; mir != nullptr; mir = mir->next) {
      // Get dalvik instruction.
      DecodedInstruction &insn = mir->dalvikInsn;

      int flags = Instruction::FlagsOf(insn.opcode);

      if ((flags & Instruction::kThrow) != 0) {
        return true;
      }
    }
  }
  return false;
}

void LoopInformation::AddInstructionsToExits(const std::vector<MIR*> &insns) {
  ArenaBitVector* bv = GetExitLoops();
  c_unit_->mir_graph.get()->PrependInstructionsToBasicBlocks(bv, insns);
  bv = GetBackwardBranches();
  c_unit_->mir_graph.get()->PrependInstructionsToBasicBlocks(bv, insns);
}

void LoopInformation::AddInstructionToExits(MIR *mir) {
  std::vector<MIR *> insns;
  insns.push_back(mir);
  AddInstructionsToExits(insns);
}

MIR *LoopInformation::GetPhiInstruction(const CompilationUnit* c_unit, int vr) const {
  MIRGraph *mir_graph = c_unit->mir_graph.get();

  ArenaBitVector* blocks = GetBasicBlocks();

  BitVectorBlockIterator it(blocks, mir_graph);
  for (BasicBlock* bb = it.Next(); bb != nullptr; bb = it.Next()) {
    for (MIR *mir = bb->first_mir_insn; mir != nullptr; mir = mir->next) {
      DecodedInstruction &insn = mir->dalvikInsn;

      // Is it a phi node?
      if (insn.opcode == static_cast<Instruction::Code>(kMirOpPhi)) {
        // Get ssa representation.
        SSARepresentation *ssa = mir->ssa_rep;

        // Paranoid.
        DCHECK(ssa != 0 && ssa->num_defs == 1);

        // Does it define our vr?
        int ssa_reg = ssa->defs[0];

        // Is it a match?
        if (mir_graph->SRegToVReg(ssa_reg) == vr) {
          // In a complex CFG we can have several Phi nodes for the same VR.
          // We'd like to find the first one, namely Phi node where the one of.
          // uses comes from outside of the loop.
          if (ssa->def_where != 0) {
            for (int i = 0; i < ssa->num_uses; i++) {
              MIR *def_mir = ssa->def_where[i];
              // If def_mir is 0 then it is come from outside of trace.
              if (def_mir == 0 || Contains(def_mir->bb) == false) {
                return mir;
              }
            }
          }
        }
      }
    }
  }
  return nullptr;
}

bool LoopInformation::CalculateBasicBlockInformation() {
  c_unit_->mir_graph->CalculateBasicBlockInformation();
  return false;
}

bool LoopInformation::IterateThroughBlocks(bbIteratorFctPtr fct, ArenaBitVector* bv, void* data) {
  // If bitvector is null, then we are done.
  if (bv == nullptr) {
    return true;
  }

  // Iterate through them.
  BitVectorBlockIterator iterator(bv, c_unit_->mir_graph.get());
  for (BasicBlock* bb = iterator.Next(); bb != nullptr; bb = iterator.Next()) {
    // Now call the worker function.
    if (fct(c_unit_, bb, data) == false) {
      return false;
    }
  }

  return true;
}

bool LoopInformation::IterateThroughLoopExitBlocks(bbIteratorFctPtr fct, void* data) {
  return IterateThroughBlocks(fct, GetExitLoops(), data);
}

bool LoopInformation::IterateThroughLoopBasicBlocks(bbIteratorFctPtr fct, void* data) {
  return IterateThroughBlocks(fct, GetBasicBlocks(), data);
}

bool LoopInformation::DumpDotHelper(LoopInformation *info, void *data) {
  FILE *file = static_cast<FILE*>(data);

  char buffer[256];

  // Create a node.
  uint64_t uid = (uint64_t) (info);

  fprintf(file, "%llu [shape=record, label =\"{ \\\n", uid);

  // Print out base information.
  snprintf(buffer, sizeof(buffer), "{Loop:} | \\\n");
  fprintf(file, "%s", buffer);
  snprintf(buffer, sizeof(buffer), "{Depth: %d} | \\\n", info->GetDepth());
  fprintf(file, "%s", buffer);
  snprintf(buffer, sizeof(buffer), "{Entry: %d} | \\\n", info->GetEntryBlock() != 0 ? info->GetEntryBlock()->id : -1);
  fprintf(file, "%s", buffer);
  snprintf(buffer, sizeof(buffer), "{PreHeader: %d} | \\\n", info->GetPreHeader() ? info->GetPreHeader()->id : -1);
  fprintf(file, "%s", buffer);

  ArenaBitVector* tmp = info->GetPostExitLoops();
  if (tmp != nullptr) {
    tmp->DumpDot(file, "Post Exit: ");
  }

  // Print the backward chaining blocks.
  tmp = info->GetExitLoops();
  if (tmp != nullptr) {
    tmp->DumpDot(file, "Exit Blocks: ");
  }

  // Print the backward chaining blocks.
  tmp = info->GetBackwardBranches();
  if (tmp != nullptr) {
    tmp->DumpDot(file, "Backward Blocks: ");
  }

  // Print the BasicBlocks BitVector.
  tmp = info->GetBasicBlocks();
  if (tmp != nullptr) {
    tmp->DumpDot(file, "Basic Blocks: ");
  }

  // End the block.
  fprintf(file, "}\"];\n\n");

  // Now make the link.
  uint64_t childUID = (uint64_t) (info->GetNested());
  if (childUID != 0) {
    fprintf(file, "%llu:s -> %llu:n\n", uid, childUID);
  }

  return true;
}

void LoopInformation::DumpDot(FILE* file) {
    Iterate(DumpDotHelper, file);
}

}  // namespace art.
