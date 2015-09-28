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

bool PrimitiveTypePropagation::MergePhiInputTypes(HPhi* phi) {
  bool conflict = false;
  Primitive::Type new_type = phi->GetType();

  // for (HInputIterator it(phi); !it.Done(); it.Advance()) {
  //   HInstruction* input = it.Current();
  for (size_t i = 0, e = phi->InputCount(); i < e; ++i) {
    HInstruction* input = phi->InputAt(i);
    if (input->IsPhi() && input->AsPhi()->IsDead()) {
      // All phis with uses (or environmental uses for --debuggable) were made live
      // by SsaDeadPhiElimination. If an input is a dead phi, it must have been
      // made dead because of conflicting inputs. Mark this phi conflicting too.
      // LOG(INFO) << "Input " << i << " is a conflicting phi " << input->GetId();
      conflict = true;
      break;
    }
    Primitive::Type input_type = HPhi::ToPhiType(input->GetType());

    // LOG(INFO) << "Input " << i << " is " << input->DebugName() << input->GetId() << ": " << new_type << " <= " << input_type;

    if (new_type == Primitive::kPrimVoid) {
      // Setting type for the first time.
      new_type = input_type;
    } else if (new_type == input_type || input_type == Primitive::kPrimVoid) {
      // Don't do anything.
    } else if (Primitive::ComponentSize(new_type) != Primitive::ComponentSize(input_type)) {
      // Input type is of a different size. Must be a conflicting type.
      conflict = true;
      break;
    } else if (Primitive::IsIntegralType(new_type)) {
      DCHECK(Primitive::IsFloatingPointType(input_type) || input_type == Primitive::kPrimNot);
      new_type = input_type;
    } else if (Primitive::IsIntegralType(input_type)) {
      DCHECK(Primitive::IsFloatingPointType(new_type) || new_type == Primitive::kPrimNot);
      // Keep `new_type`.
    } else {
      DCHECK((new_type == Primitive::kPrimFloat && input_type == Primitive::kPrimNot) ||
             (new_type == Primitive::kPrimNot && input_type == Primitive::kPrimFloat));
      conflict = true;
      break;
    }
  }

  if (conflict) {
    phi->SetDead();
    // LOG(INFO) << "Type merge failed for " << phi->GetId();
    return false;
  } else {
    phi->SetType(new_type);
    return true;
  }
}

bool PrimitiveTypePropagation::ReplacePhiInputs(HPhi* phi) {
  Primitive::Type new_type = phi->GetType();
  if (!Primitive::IsFloatingPointType(new_type) && new_type != Primitive::kPrimNot) {
    // No need to replace the inputs. TODO
    return true;
  }

  for (size_t i = 0, e = phi->InputCount(); i < e; ++i) {
    HInstruction* input = phi->InputAt(i);
    // LOG(INFO) << "input" << i << " = " << input->DebugName() << input->GetId() << input->GetType();
    if (input->GetType() != new_type) {
      HInstruction* equivalent = (new_type == Primitive::kPrimNot)
          ? SsaBuilder::GetReferenceTypeEquivalent(input)
          : SsaBuilder::GetFloatOrDoubleEquivalent(phi, input, new_type);

      if (equivalent == nullptr || (equivalent->IsPhi() && equivalent->AsPhi()->IsDead())) {
        // LOG(INFO) << "Equivalent failed for " << phi->GetId() << " input " << i << (equivalent == nullptr ? " (null)" : " (dead phi)");
        return false;
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

  return true;
}

// Re-compute and update the type of the instruction. Returns
// whether or not the type was changed.
bool PrimitiveTypePropagation::UpdateType(HPhi* phi) {
  if (phi->IsDead()) {
    return false;
  }

  // LOG(INFO) << " ====== " << phi->GetId() << " ========== ";
  Primitive::Type original_type = phi->GetType();
  if (!MergePhiInputTypes(phi)) {
    phi->SetDead();
    return true;
  }
  if (!ReplacePhiInputs(phi)) {
    phi->SetDead();
    return true;
  }
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
