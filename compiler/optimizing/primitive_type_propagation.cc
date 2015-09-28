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

bool PrimitiveTypePropagation::TypePhiFromInputs(HPhi* phi) {
  Primitive::Type common_type = phi->GetType();

  for (HInputIterator it(phi); !it.Done(); it.Advance()) {
    HInstruction* input = it.Current();
    if (input->IsPhi() && input->AsPhi()->IsDead()) {
      // Phis are constructed live so if an input is a dead phi, it must have
      // been made dead due to type conflict. Mark this phi conflicting too.
      return false;
    }

    Primitive::Type input_type = HPhi::ToPhiType(input->GetType());
    if (common_type == Primitive::kPrimVoid) {
      // Setting type for the first time.
      common_type = input_type;
    } else if (common_type == input_type) {
      // No change in type.
    } else if (input_type == Primitive::kPrimVoid) {
      // Input is a phi which has not been typed yet. Do nothing.
      DCHECK(input->IsPhi());
    } else if (Primitive::ComponentSize(common_type) != Primitive::ComponentSize(input_type)) {
      // Types are of different sizes. Must be a conflict.
      return false;
    } else if (Primitive::IsIntegralType(common_type)) {
      // Previous inputs were integral, this one is not but is of the same size.
      // This does not imply conflict since some bytecode instruction types are
      // ambiguous. ReplacePhiInputs will either type them or detect a conflict.
      DCHECK(Primitive::IsFloatingPointType(input_type) || input_type == Primitive::kPrimNot);
      common_type = input_type;
    } else if (Primitive::IsIntegralType(input_type)) {
      // Input is integral, common type is not. Same as in the previous case, if
      // there is a conflict, it will be detected during ReplacePhiInputs.
      DCHECK(Primitive::IsFloatingPointType(common_type) || common_type == Primitive::kPrimNot);
    } else {
      // Combining float and reference types. Clearly a conflict.
      DCHECK((common_type == Primitive::kPrimFloat && input_type == Primitive::kPrimNot) ||
             (common_type == Primitive::kPrimNot && input_type == Primitive::kPrimFloat));
      return false;
    }
  }

  phi->SetType(common_type);
  return true;
}

bool PrimitiveTypePropagation::ReplacePhiInputs(HPhi* phi) {
  Primitive::Type common_type = phi->GetType();
  if (Primitive::IsFloatingPointType(common_type) || common_type == Primitive::kPrimNot) {
    for (size_t i = 0, e = phi->InputCount(); i < e; ++i) {
      HInstruction* input = phi->InputAt(i);
      if (input->GetType() != common_type) {
        HInstruction* equivalent = (common_type == Primitive::kPrimNot)
            ? SsaBuilder::GetReferenceTypeEquivalent(input)
            : SsaBuilder::GetFloatOrDoubleEquivalent(phi, input, common_type);
        if (equivalent == nullptr) {
          // Input could not be typed. Report conflict.
          return false;
        }

        phi->ReplaceInput(equivalent, i);
        if (equivalent->IsPhi()) {
          AddToWorklist(equivalent->AsPhi());
        } else if (equivalent == input) {
          // The input has changed its type. It can be an input of other phis,
          // so we need to put phi users in the work list.
          AddDependentInstructionsToWorklist(input);
        }
      }
    }
  }

  return true;
}

bool PrimitiveTypePropagation::UpdateType(HPhi* phi) {
  DCHECK(phi->IsLive());
  Primitive::Type original_type = phi->GetType();

  if (!TypePhiFromInputs(phi) || !ReplacePhiInputs(phi)) {
    // Phi could not be typed due to conflicting inputs. Mark it dead.
    phi->SetDead();
    return true;
  }

  // Return true if the type of the phi has changed.
  return phi->GetType() != original_type;
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
    HPhi* phi = worklist_.back();
    worklist_.pop_back();
    // The phi could have been made dead as a result of conflicts while in the
    // worklist. If it is now dead, there is no point in updating its type.
    if (phi->IsLive() && UpdateType(phi)) {
      AddDependentInstructionsToWorklist(phi);
    }
  }
}

void PrimitiveTypePropagation::AddToWorklist(HPhi* instruction) {
  DCHECK(instruction->IsLive());
  worklist_.push_back(instruction);
}

void PrimitiveTypePropagation::AddDependentInstructionsToWorklist(HInstruction* instruction) {
  // If `instruction` is a dead phi, type conflict was just identified. All its
  // live phi users therefore need to be marked dead/conflicting too and we add
  // them to the worklist. Otherwise we add users whose type does not match and
  // needs to be updated.
  bool add_all_live_phis = instruction->IsPhi() && instruction->AsPhi()->IsDead();
  for (HUseIterator<HInstruction*> it(instruction->GetUses()); !it.Done(); it.Advance()) {
    HInstruction* user = it.Current()->GetUser();
    if (user->IsPhi() && user->AsPhi()->IsLive()) {
      if (add_all_live_phis || user->GetType() != instruction->GetType()) {
        AddToWorklist(user->AsPhi());
      }
    }
  }
}

}  // namespace art
