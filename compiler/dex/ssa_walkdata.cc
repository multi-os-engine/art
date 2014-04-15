/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0.
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "ssa_walkdata.h"
#include "compiler_internals.h"

namespace art {

SSAWalkData::SSAWalkData(MIRGraph* mir_graph)
    : mir_graph_(mir_graph) {
  mir_graph->ResetUseDefScopedArena();

  int num = mir_graph->GetNumSSARegs();

  definitions_.resize(num);
  last_chain_.resize(num);

  for (int i = 0; i < num; i++) {
    definitions_[i] = nullptr;
    last_chain_[i] = nullptr;
  }
}

UsedChain *SSAWalkData::GetUsedChain(void) {
  UsedChain* res = mir_graph_->GetUsedChain();

  // Reset it.
  res->next_use_ = nullptr;
  res->prev_use_ = nullptr;
  res->mir_ = nullptr;

  return res;
}

UsedChain *SSAWalkData::GetLastChain(int ssa_reg) {
  return last_chain_[ssa_reg];
}

void SSAWalkData::SetLastChain(UsedChain* chain, int ssa_reg) {
  last_chain_[ssa_reg] = chain;
}

void SSAWalkData::SetDefinition(MIR* insn, int ssa_reg) {
  definitions_[ssa_reg] = insn;
}

MIR *SSAWalkData::GetDefinition(int ssa_reg) const {
  return definitions_[ssa_reg];
}

void SSAWalkData::AddUseToDefChain(int use_idx, MIR* used, MIR* defined) {
  SSARepresentation* ssa_rep = used->ssa_rep;

  // Set def_where for this instruction.
  ssa_rep->def_where_[use_idx] = defined;

  if (defined == nullptr) {
    return;
  }

  // We have a need of a new chain element.
  UsedChain* elem = GetUsedChain();

  elem->mir_ = used;
  int ssa_reg = ssa_rep->uses[use_idx];

  // Get the last use for this element and set it.
  UsedChain* last = GetLastChain(ssa_reg);
  SetLastChain(elem, ssa_reg);

  // If we already have one, just chain.
  if (last != nullptr) {
    last->next_use_ = elem;
    elem->prev_use_ = last;
    return;
  }

  // It's the first, tell defined about it.
  SSARepresentation* def_ssa = defined->ssa_rep;

  // Paranoid.
  DCHECK(def_ssa != nullptr);

  // Go through the defines and find ssa_reg.
  int max = def_ssa->num_defs;
  for (int j = 0; j < max; j++) {
    if (def_ssa->defs[j] == ssa_reg) {
      def_ssa->used_next_[j] = elem;
      return;
    }
  }

  // We should have found it.
  DCHECK(false);
}

void SSAWalkData::HandleNoDefinitions(void) {
  // Go through each no definition.
  for (const WalkDataNoDefine& info: no_define_) {
    MIR* mir = info.mir_;
    int idx = info.index_;

    SSARepresentation* ssa_rep = mir->ssa_rep;

    // Paranoid.
    DCHECK(ssa_rep != nullptr);
    DCHECK(0 <= idx && idx < ssa_rep->num_uses);

    // Get use ssa_reg.
    int ssa_reg = ssa_rep->uses[idx];

    // Set def_where for this instruction.
    MIR* defined = GetDefinition(ssa_reg);

    AddUseToDefChain(idx, mir, defined);
  }
}

void SSAWalkData::AddNoDefine(MIR* mir, int idx) {
  // Create the data and Set it.
  WalkDataNoDefine info = {mir, idx};
  no_define_.push_back(info);
}

}  // namespace art
