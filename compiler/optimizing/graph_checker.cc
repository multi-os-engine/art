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

#include "graph_checker.h"

namespace art {

namespace {

  // Is `inst` a branch instruction?
  inline bool IsBranchInstruction(HInstruction& inst) {
    return
      inst.IsExit()
      || inst.IsGoto()
      || inst.IsIf()
      || inst.IsReturn()
      || inst.IsReturnVoid();
  }

  // Is `inst` an arihmetic operation?
  inline bool IsArithmeticOperation(HInstruction& inst) {
    return
      inst.IsCondition()
      || inst.IsCompare()
      || inst.IsAdd()
      || inst.IsSub();
  }

}  // namespace


void GraphChecker::VisitBasicBlock(HBasicBlock* block) {
  // Check consistency wrt predecessors of `block`.
  const GrowableArray<HBasicBlock*>& predecessors = block->GetPredecessors();
  for (size_t i = 0, e = predecessors.Size(); i < e; ++i) {
    const HBasicBlock* p = predecessors.Get(i);
    const GrowableArray<HBasicBlock*>& p_successors = p->GetSuccessors();
    bool block_found_in_p_successors = false;
    for (size_t j = 0, f = p_successors.Size(); j < f; ++j) {
      if (p_successors.Get(j) == block) {
        block_found_in_p_successors = true;
        break;
      }
    }
    if (!block_found_in_p_successors) {
      std::stringstream error;
      error << "Block " << block->GetBlockId() << " lists block "
            << p->GetBlockId() << " as predecessor, but block "
            << p->GetBlockId() << " does not list block "
            << block->GetBlockId() << " as successor.";
      errors_.Insert(error.str());
    }
  }

  // Check consistency wrt successors of `block`.
  const GrowableArray<HBasicBlock*>& successors = block->GetSuccessors();
  for (size_t i = 0, e = successors.Size(); i < e; ++i) {
    const HBasicBlock* s = successors.Get(i);
    const GrowableArray<HBasicBlock*>& s_predecessors = s->GetPredecessors();
    bool block_found_in_s_predecessors = false;
    for (size_t j = 0, f = s_predecessors.Size(); j < f; ++j) {
      if (s_predecessors.Get(j) == block) {
        block_found_in_s_predecessors = true;
        break;
      }
    }
    if (!block_found_in_s_predecessors) {
      std::stringstream error;
      error << "Block " << block->GetBlockId() << " lists block "
            << s->GetBlockId() << " as successor, but block "
            << s->GetBlockId() << " does not list block "
            << block->GetBlockId() << " as predecessor.";
      errors_.Insert(error.str());
    }
  }

  // Ensure `block` ends with a branch instruction.
  HInstruction* last_inst = block->GetLastInstruction();
  if (last_inst == nullptr || !IsBranchInstruction(*last_inst)) {
    std::stringstream error;
    error  << "Block " << block->GetBlockId()
           << " does not end with a branch instruction.";
    errors_.Insert(error.str());
  }

  // Ensure this block's list of phi functions contains only phi functions.
  for (HInstructionIterator it(block->GetPhis()); !it.Done(); it.Advance()) {
    HInstruction* phi = it.Current();
    if (!phi->IsPhi()) {
      std::stringstream error;
      error  << "Block " << block->GetBlockId()
             << " has a non-phi function in its phi list";
      errors_.Insert(error.str());
    }
  }
  // Ensure this block's list of instructions does not contains phi
  // functions.
  for (HInstructionIterator it(block->GetInstructions()); !it.Done();
       it.Advance()) {
    HInstruction* inst = it.Current();
    if (inst->IsPhi()) {
      std::stringstream error;
      error  << "Block " << block->GetBlockId()
             << " has a phi function in its non-phi list";
      errors_.Insert(error.str());
    }
  }

  // Ensure this block's instructions are associated with this very block.
  for (HInstructionIterator it(block->GetInstructions()); !it.Done();
       it.Advance()) {
    HInstruction* inst = it.Current();
    if (inst->GetBlock() != block) {
      std::stringstream error;
      error << "Instruction " << inst->GetId() << "in block "
            << block->GetBlockId();
      if (inst->GetBlock() != nullptr)
        error << " associated with block "
              << inst->GetBlock()->GetBlockId() << ".";
      else
        error << "not associated with any block.";
      errors_.Insert(error.str());
    }
  }
  // Ensure this block's phi functions are associated with this very block.
  for (HInstructionIterator it(block->GetPhis()); !it.Done(); it.Advance()) {
    HInstruction* phi = it.Current();
    if (phi->GetBlock() != block) {
      std::stringstream error;
      error << "Phi function " << phi->GetId() << "in block "
            << block->GetBlockId();
      if (phi->GetBlock() != nullptr)
        error << " associated with block "
              << phi->GetBlock()->GetBlockId() << ".";
      else
        error << "not associated with any block.";
      errors_.Insert(error.str());
    }
  }

  // Ensure constants used as inputs of an arithmetic operation appear
  // at the end only.
  for (HInstructionIterator it(block->GetInstructions()); !it.Done();
       it.Advance()) {
    HInstruction* inst = it.Current();
    if (inst != nullptr && IsArithmeticOperation(*inst)) {
      bool constant_seen = false;
      for (HInputIterator it(inst); !it.Done(); it.Advance()) {
        HInstruction* input = it.Current();
        if (input != nullptr && input->IsConstant())
          constant_seen = true;
        else
          if (constant_seen) {
            std::stringstream error;
            error << "Arithmetic operation " << inst->GetId()
                  << " in block " << block->GetBlockId()
                  << " uses a constant not located at the end of its inputs.";
            errors_.Insert(error.str());
          }
      }
    }
  }
}

bool GraphChecker::IsValid() const {
  return errors_.IsEmpty();
}

const GrowableArray<std::string>& GraphChecker::GetErrors() const {
  return errors_;
}


void SSAChecker::VisitBasicBlock(HBasicBlock* block) {
  super_type::VisitBasicBlock(block);

  // Ensure an instruction dominates all its uses (or in the present
  // case, that all uses of an instruction (used as input) are
  // dominated by its definition).
  for (HInstructionIterator inst_it(block->GetInstructions()); !inst_it.Done();
       inst_it.Advance()) {
    HInstruction* inst = inst_it.Current();
    for (HInputIterator input_it(inst); !input_it.Done(); input_it.Advance()) {
      HInstruction* input = input_it.Current();
      HBasicBlock* def_block = input->GetBlock();
      // Ensure `def_block` dominates `block`.
      HBasicBlock* b = block;
      while (b != nullptr && b != def_block)
        b = b->GetDominator();
      if (b != def_block) {
        std::stringstream error;
        error << "Instruction " << input->GetId()
              << " in block " << def_block->GetBlockId()
              << " does not dominate use " << inst->GetId()
              << " in block " << block->GetBlockId() << ".";
        errors_.Insert(error.str());
      }
    }
  }

  // Ensure there is no critical edge (i.e., an edge connecting a
  // block with multiple successors to a block with multiple
  // predecessors).
  if (block->GetSuccessors().Size() > 1) {
    for (size_t j = 0; j < block->GetSuccessors().Size(); ++j) {
      HBasicBlock* successor = block->GetSuccessors().Get(j);
      if (successor->GetPredecessors().Size() > 1) {
        std::stringstream error;
        error << "Critical edge between blocks " << block->GetBlockId()
              << " and "  << successor->GetBlockId() << ".";
        errors_.Insert(error.str());
      }
    }
  }
}

}  // namespace art
