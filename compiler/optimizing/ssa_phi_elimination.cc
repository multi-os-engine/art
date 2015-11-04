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

#include "ssa_phi_elimination.h"

#include "base/arena_containers.h"

namespace art {

void SsaRedundantPhiElimination::Run() {
  // Add all phis in the worklist. Order does not matter for correctness, and
  // neither will necessarily converge faster.
  for (HReversePostOrderIterator it(*graph_); !it.Done(); it.Advance()) {
    HBasicBlock* block = it.Current();
    for (HInstructionIterator inst_it(block->GetPhis()); !inst_it.Done(); inst_it.Advance()) {
      worklist_.push_back(inst_it.Current()->AsPhi());
    }
  }

  while (!worklist_.empty()) {
    HPhi* phi = worklist_.back();
    worklist_.pop_back();

    // If the phi has already been processed, continue.
    if (!phi->IsInBlock()) {
      continue;
    }

    if (phi->InputCount() == 0) {
      DCHECK(phi->IsDead());
      continue;
    }

    // Find if the inputs of the phi are the same instruction.
    HInstruction* candidate = phi->InputAt(0);
    // A loop phi cannot have itself as the first phi. Note that this
    // check relies on our simplification pass ensuring the pre-header
    // block is first in the list of predecessors of the loop header.
    DCHECK(!phi->IsLoopHeaderPhi() || phi->GetBlock()->IsLoopPreHeaderFirstPredecessor());
    DCHECK_NE(phi, candidate);

    for (size_t i = 1; i < phi->InputCount(); ++i) {
      HInstruction* input = phi->InputAt(i);
      // For a loop phi, if the input is the phi, the phi is still candidate for
      // elimination.
      if (input != candidate && input != phi) {
        candidate = nullptr;
        break;
      }
    }

    // If the inputs are not the same, continue.
    if (candidate == nullptr) {
      continue;
    }

    // The candidate may not dominate a phi in a catch block.
    if (phi->IsCatchPhi() && !candidate->StrictlyDominates(phi)) {
      continue;
    }

    // Because we're updating the users of this phi, we may have new candidates
    // for elimination. Add phis that use this phi to the worklist.
    for (HUseIterator<HInstruction*> it(phi->GetUses()); !it.Done(); it.Advance()) {
      HUseListNode<HInstruction*>* current = it.Current();
      HInstruction* user = current->GetUser();
      if (user->IsPhi()) {
        worklist_.push_back(user->AsPhi());
      }
    }

    phi->ReplaceWith(candidate);
    phi->GetBlock()->RemovePhi(phi);
  }
}

}  // namespace art
