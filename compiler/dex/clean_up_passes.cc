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

#include "clean_up_passes.h"
#include "dataflow_iterator.h"
#include "dataflow_iterator-inl.h"
#include "pass_driver.h"
#include "ssa_walkdata.h"

namespace art {

/*
 * MethodUseCount pass implementation start.
 */
bool MethodUseCount::Gate(const PassDataHolder* data) const {
  DCHECK(data != nullptr);
  CompilationUnit* cUnit = down_cast<const PassMEDataHolder*>(data)->c_unit;
  DCHECK(cUnit != nullptr);
  // First initialize the data.
  cUnit->mir_graph->InitializeMethodUses();

  // Now check if the pass is to be ignored.
  bool res = ((cUnit->disable_opt & (1 << kPromoteRegs)) == 0);

  return res;
}

bool MethodUseCount::Worker(const PassDataHolder* data) const {
  DCHECK(data != nullptr);
  const PassMEDataHolder* pass_me_data_holder = down_cast<const PassMEDataHolder*>(data);
  CompilationUnit* cUnit = pass_me_data_holder->c_unit;
  DCHECK(cUnit != nullptr);
  BasicBlock* bb = pass_me_data_holder->bb;
  DCHECK(bb != nullptr);
  cUnit->mir_graph->CountUses(bb);
  // No need of repeating, so just return false.
  return false;
}


bool ClearPhiInstructions::Worker(const PassDataHolder* data) const {
  DCHECK(data != nullptr);
  const PassMEDataHolder* pass_me_data_holder = down_cast<const PassMEDataHolder*>(data);
  CompilationUnit* c_unit = pass_me_data_holder->c_unit;
  DCHECK(c_unit != nullptr);
  BasicBlock* bb = pass_me_data_holder->bb;
  DCHECK(bb != nullptr);
  for (MIR *mir = bb->first_mir_insn; mir != nullptr; mir = mir->next) {
    Instruction::Code opcode = mir->dalvikInsn.opcode;

    if (opcode == static_cast<Instruction::Code> (kMirOpPhi)) {
      // First thing is to detach it.
      MIR *prev = mir->prev;
      MIR *next = mir->next;

      // Attach previous to next if it exists.
      if (prev != nullptr) {
        prev->next = next;
      }

      // Same for next.
      if (next != nullptr) {
        next->prev = prev;
      }

      // Instruction is now removed but we must handle first and last for the basic block.
      if (mir == bb->first_mir_insn) {
        bb->first_mir_insn = next;
      }

      // Remove the instruction for last.
      if (mir == bb->last_mir_insn) {
        bb->last_mir_insn = prev;
      }
    }
  }

  // We do not care in reporting a change or not in the MIR
  return false;
}

void CalculatePredecessors::Start(const PassDataHolder* data) const {
  DCHECK(data != nullptr);
  CompilationUnit* c_unit = down_cast<const PassMEDataHolder*>(data)->c_unit;
  DCHECK(c_unit != nullptr);
  // First get the MIRGraph here to factorize a bit the code.
  MIRGraph *mir_graph = c_unit->mir_graph.get();

  // First clear all predecessors.
  AllNodesIterator first(mir_graph);
  for (BasicBlock *bb = first.Next(); bb != nullptr; bb = first.Next()) {
    bb->predecessors->Reset();
  }

  // Now calculate all predecessors.
  AllNodesIterator second(mir_graph);
  for (BasicBlock *bb = second.Next(); bb != nullptr; bb = second.Next()) {
    // We only care about non hidden blocks.
    if (bb->hidden == true) {
      continue;
    }

    // Create iterator for visiting children.
    ChildBlockIterator child_iter(bb, mir_graph);

    // Now iterate through the children to set the predecessor bits.
    for (BasicBlock* child = child_iter.Next(); child != nullptr; child = child_iter.Next()) {
      child->predecessors->Insert(bb->id);
    }
  }
}

void BuildDefUseChain::Start(const PassDataHolder* data) const {
  DCHECK(data != nullptr);
  CompilationUnit* c_unit = down_cast<const PassMEDataHolder*>(data)->c_unit;
  DCHECK(c_unit != nullptr);
  MIRGraph *mir_graph = c_unit->mir_graph.get();

  // First clear the visited.
  GetPassInstance<ClearVisitedFlag>()->Start(data);

  SSAWalkData ssa_walk_data(c_unit->mir_graph.get());

  // Build the use-def chains for the MIRs.
  TopologicalSortIterator iterator(c_unit->mir_graph.get());
  for (BasicBlock *bb = iterator.Next(); bb != nullptr; bb = iterator.Next()) {
    Build(bb, mir_graph, &ssa_walk_data);
  }

  // Finally handle the uses that do not have definitions.
  ssa_walk_data.HandleNoDefinitions();
}

bool BuildDefUseChain::Build(BasicBlock *bb, MIRGraph *mir_graph, SSAWalkData *walk_data) const {
  bool res = false;
  unsigned int current_order = 0;

  GrowableArray<BasicBlockId>::Iterator iterator(bb->predecessors);

  while (true) {
    BasicBlock* pred_bb = mir_graph->GetBasicBlock(iterator.Next());
    if (pred_bb == nullptr) {
      break;
    }

    // If the pred was not handled yet (backward?) then skip it.
    if (pred_bb->visited == false) {
      continue;
    }

    // Get order from the entrance of the BB.
    unsigned int order = pred_bb->topological_order;

    // Get the last instruction's order.
    MIR *last_insn = pred_bb->last_mir_insn;

    // If no instruction, just use the BB order, so skip this.
    if (last_insn != nullptr) {
      order = last_insn->topological_order;
    }

    // Compare to actual minimum.
    if (current_order < order) {
      current_order = order;
    }
  }

  if (bb->topological_order != current_order) {
    // Set the basic block's order now.
    bb->topological_order = current_order;
    // A change occured.
    res = true;
  }

  // We now have the minimum topological order: go through the instructions.
  for (MIR *insn = bb->first_mir_insn; insn != 0; insn = insn->next) {
    // Augment the current topological order and then set.
    current_order++;

    insn->topological_order = current_order;

    // Now handle use and def chains.
    SSARepresentation *ssa_rep = insn->ssa_rep;

    // If we don't have a ssa_rep, there is nothing we can do here.
    if (ssa_rep != nullptr) {
      // First add to the use chains.
      int nbr = ssa_rep->num_uses;
      for (int i = 0; i < nbr; i++) {
        // Get use value.
        int value = ssa_rep->uses[i];

        // Set def_where for this instruction.
        MIR *defined = walk_data->GetDefinition(value);

        // In case there is no define yet, let's remember it to handle it afterwards.
        if (defined == 0) {
          walk_data->AddNoDefine(insn, i);
        } else {
          walk_data->AddUseToDefChain(i, insn, defined);
        }
      }

      // Now handle defs.
      nbr = ssa_rep->num_defs;
      for (int i = 0; i < nbr; i++) {
        // Before anything clean up the usedNext.
        ssa_rep->used_next_[i] = nullptr;

        // Get def value.
        int value = ssa_rep->defs[i];

        // Register definition.
        walk_data->SetDefinition(insn, value);
      }
    }

    // Something changed if we got here: at least one instruction was touched.
    res = true;
  }

  return res;
}

}  // namespace art
