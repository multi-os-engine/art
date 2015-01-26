/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include "type_analysis.h"

namespace art {

// TODO: follow dominators to see if a value has already been NullChecked
//       and thus guaranteed not to be null.
// TODO: investigate if it is worth keeping track of field-set/field-get patterns.
// TODO: consider have an abstraction for the worklist algorithm.

void TypeAnalysis::Run() {
  // To properly propagate not-null info we need to visit in the dominator-based order.
  // Reverse post order guarantees a node's dominators are visited first.
  for (HReversePostOrderIterator it(*graph_); !it.Done(); it.Advance()) {
    VisitBasicBlock(it.Current());
  }
  ProcessWorklist();
}

// Re-compute and update the nullability of the instruction. Returns whether or
// not the nullability was changed.
bool TypeAnalysis::UpdateNullability(HPhi* phi) {
  bool existing_can_be_null = phi->CanBeNull();
  bool new_can_be_null = false;
  for (size_t i = 0; i < phi->InputCount(); i++) {
    new_can_be_null |= phi->InputAt(i)->CanBeNull();
  }
  phi->SetCanBeNull(new_can_be_null);

  return existing_can_be_null != new_can_be_null;
}


void TypeAnalysis::VisitBasicBlock(HBasicBlock* block) {
  if (block->IsLoopHeader()) {
    for (HInstructionIterator it(block->GetPhis()); !it.Done(); it.Advance()) {
      // Set the initial type for the phi. Use the non back edge input for reaching
      // a fixed point faster.
      HPhi* phi = it.Current()->AsPhi();
      AddToWorklist(phi);
      phi->SetCanBeNull(phi->InputAt(0)->CanBeNull());
    }
  } else {
    for (HInstructionIterator it(block->GetPhis()); !it.Done(); it.Advance()) {
      // Eagerly compute the type of the phi, for quicker convergence. Note
      // that we don't need to add users to the worklist because we are
      // doing a reverse post-order visit, therefore either the phi users are
      // non-loop phi and will be visited later in the visit, or are loop-phis,
      // and they are already in the work list.
      UpdateNullability(it.Current()->AsPhi());
    }
  }
}

void TypeAnalysis::ProcessWorklist() {
  while (!worklist_.IsEmpty()) {
    HPhi* instruction = worklist_.Pop();
    if (UpdateNullability(instruction)) {
      AddDependentInstructionsToWorklist(instruction);
    }
  }
}

void TypeAnalysis::AddToWorklist(HPhi* instruction) {
  worklist_.Add(instruction);
}

void TypeAnalysis::AddDependentInstructionsToWorklist(HPhi* instruction) {
  for (HUseIterator<HInstruction> it(instruction->GetUses()); !it.Done(); it.Advance()) {
    HPhi* phi = it.Current()->GetUser()->AsPhi();
    if (phi != nullptr) {
      AddToWorklist(phi);
    }
  }
}

}  // namespace art
