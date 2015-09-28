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

#include "primitive_type_propagation.h"

#include "nodes.h"
#include "ssa_builder.h"

namespace art {

// Sets the type of the phi based on its inputs. Returns true if the types
// conflict and false otherwise.
static bool MergeInputTypes(HPhi* phi) {
  Primitive::Type new_type = phi->GetType();

  for (size_t i = 0, e = phi->InputCount(); i < e; ++i) {
    HInstruction* input = phi->InputAt(i);
    if (input->IsPhi() && input->AsPhi()->IsDead()) {
      // All phis with uses (or environmental uses for --debuggable) were made live
      // by SsaDeadPhiElimination. If an input is a dead phi, it must have been
      // made dead because of conflicting inputs. Mark this phi conflicting too.
      return true;
    }
    Primitive::Type input_type = HPhi::ToPhiType(input->GetType());

    if (new_type == Primitive::kPrimVoid) {
      // Setting type for the first time.
      new_type = input_type;
    } else if (new_type == input_type || input_type == Primitive::kPrimVoid) {
      // Don't do anything.
    } else if (Primitive::ComponentSize(new_type) != Primitive::ComponentSize(input_type)) {
      // Input type is of a different size. Must be a conflicting type.
      return true;
    } else if (Primitive::IsIntegralType(new_type)) {
      DCHECK(Primitive::IsFloatingPointType(input_type) || input_type == Primitive::kPrimNot);
      new_type = input_type;
    } else if (Primitive::IsIntegralType(input_type)) {
      DCHECK(Primitive::IsFloatingPointType(new_type) || new_type == Primitive::kPrimNot);
      // Keep `new_type`.
    } else {
      DCHECK((new_type == Primitive::kPrimFloat && input_type == Primitive::kPrimNot) ||
             (new_type == Primitive::kPrimNot && input_type == Primitive::kPrimFloat));
      return true;
    }
  }

  phi->SetType(new_type);
  return false;
}

// Re-compute and update the type of the instruction. Returns
// whether or not the type was changed.
bool PrimitiveTypePropagation::UpdateType(HPhi* phi) {
  if (phi->IsDead()) {
    return false;
  }

  // LOG(INFO) << " ====== " << phi->GetId() << " ========== ";
  Primitive::Type old_type = phi->GetType();
  if (MergeInputTypes(phi)) {
    phi->SetDead();
    // LOG(INFO) << "Type merge failed for " << phi->GetId();
    return true;
  }
  Primitive::Type new_type = phi->GetType();

  // LOG(INFO) << "Setting type of " << phi->GetId() << " from " << old_type << " to " << new_type;

  // Must iterate over inputs even if new_type == old_type because phis may
  // need input replacement.
  if (Primitive::IsFloatingPointType(new_type) || new_type == Primitive::kPrimNot) {
    for (size_t i = 0, e = phi->InputCount(); i < e; ++i) {
      HInstruction* input = phi->InputAt(i);
      // LOG(INFO) << "input" << i << " = " << input->DebugName() << input->GetId() << input->GetType();
      if (input->GetType() != new_type) {
        HInstruction* equivalent = (new_type == Primitive::kPrimNot)
            ? SsaBuilder::GetReferenceTypeEquivalent(input)
            : SsaBuilder::GetFloatOrDoubleEquivalent(phi, input, new_type);

        if (equivalent == nullptr || (equivalent->IsPhi() && equivalent->AsPhi()->IsDead())) {
          phi->SetDead();  // Conflict.
          // LOG(INFO) << "Equivalent failed for " << phi->GetId() << " input " << i;
          return true;
        }

        // LOG(INFO) << "Requesting equivalent for " << input->GetId() << " got " << equivalent->DebugName() << equivalent->GetId() << equivalent->GetType();

        phi->ReplaceInput(equivalent, i);
        if (equivalent->IsPhi()) {
          AddToWorklist(equivalent->AsPhi());
        } else if (equivalent == input) {
          AddDependentInstructionsToWorklist(input);
        }
      }
    }
  }

  phi->SetType(new_type);
  return old_type != new_type;
}

void PrimitiveTypePropagation::Run() {
  for (HReversePostOrderIterator it(*graph_); !it.Done(); it.Advance()) {
    VisitBasicBlock(it.Current());
  }
  ProcessWorklist();
}

void PrimitiveTypePropagation::VisitBasicBlock(HBasicBlock* block) {
  if (block->IsLoopHeader()) {
    for (HInstructionIterator it(block->GetPhis()); !it.Done(); it.Advance()) {
      HPhi* phi = it.Current()->AsPhi();
      if (phi->IsLive()) {
        AddToWorklist(phi);
      }
    }
  } else {
    for (HInstructionIterator it(block->GetPhis()); !it.Done(); it.Advance()) {
      // Eagerly compute the type of the phi, for quicker convergence. Note
      // that we don't need to add users to the worklist because we are
      // doing a reverse post-order visit, therefore either the phi users are
      // non-loop phi and will be visited later in the visit, or are loop-phis,
      // and they are already in the work list.
      HPhi* phi = it.Current()->AsPhi();
      if (phi->IsLive()) {
        UpdateType(phi);
      }
    }
  }
}

void PrimitiveTypePropagation::ProcessWorklist() {
  while (!worklist_.empty()) {
    HPhi* instruction = worklist_.back();
    worklist_.pop_back();
    if (UpdateType(instruction)) {
      AddDependentInstructionsToWorklist(instruction);
    }
  }
}

void PrimitiveTypePropagation::AddToWorklist(HPhi* instruction) {
  DCHECK(instruction->IsLive());
  worklist_.push_back(instruction);
}

void PrimitiveTypePropagation::AddDependentInstructionsToWorklist(HInstruction* instruction) {
  for (HUseIterator<HInstruction*> it(instruction->GetUses()); !it.Done(); it.Advance()) {
    HPhi* phi = it.Current()->GetUser()->AsPhi();
    if (phi != nullptr && phi->IsLive() &&
        (phi->GetType() != instruction->GetType() ||
         (instruction->IsPhi() && instruction->AsPhi()->IsDead()))) {
      AddToWorklist(phi);
    }
  }
}

}  // namespace art
