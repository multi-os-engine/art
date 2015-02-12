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

#include "reference_type_propagation.h"

#include "class_linker.h"
#include "mirror/class-inl.h"
#include "mirror/dex_cache.h"
#include "scoped_thread_state_change.h"

namespace art {

// TODO: Only do the analysis on reference types. We currently have to handle
// the `null` constant, that is represented as a `HIntConstant` and therefore
// has the Primitive::kPrimInt type.

// TODO: handle:
//  public Main ifNullTest(int count, Main a) {
//    Main m = new Main();
//    if (a == null) {
//      a = m;
//    }
//    return a.g();
//  }
//  public Main ifNotNullTest(Main a) {
//    if (a != null) {
//      return a.g();
//    }
//    return new Main();
//  }

void ReferenceTypePropagation::Run() {
  // Compute null status for instructions.

  // To properly propagate not-null info we need to visit in the dominator-based order.
  // Reverse post order guarantees a node's dominators are visited first.
  // We take advantage of this order in `VisitBasicBlock`.
  for (HReversePostOrderIterator it(*graph_); !it.Done(); it.Advance()) {
    VisitBasicBlock(it.Current());
  }
  ProcessWorklist();
}

// Re-computes and updates the nullability of the instruction. Returns whether or
// not the nullability was changed.
bool ReferenceTypePropagation::UpdateNullability(HPhi* phi) {
  bool existing_can_be_null = phi->CanBeNull();
  bool new_can_be_null = false;
  for (size_t i = 0; i < phi->InputCount(); i++) {
    new_can_be_null |= phi->InputAt(i)->CanBeNull();
  }
  phi->SetCanBeNull(new_can_be_null);

  return existing_can_be_null != new_can_be_null;
}

bool ReferenceTypePropagation::UpdateReferenceTypeInfo(HPhi* phi)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  ReferenceTypeInfo existing_rti = phi->GetReferenceTypeInfo();
  if (existing_rti.IsTop()) {
    return false;
  }

  ReferenceTypeInfo new_rti = existing_rti;

  for (size_t i = 1; i < phi->InputCount(); i++) {
    ReferenceTypeInfo input_rti = phi->InputAt(i)->GetReferenceTypeInfo();

    if (!new_rti.IsKnown()) {
      new_rti = input_rti;
      continue;
    }
    if (!input_rti.IsKnown()) {
      continue;  // Keep the existing type
    }

    if (input_rti.IsTop()) {
      new_rti.Top();
      break;
    }

    if (!input_rti.IsPrecise()) {
      new_rti.Imprecise();
    }

    mirror::Class* phi_class = new_rti.GetTypeHandle().Get();
    mirror::Class* input_class = input_rti.GetTypeHandle().Get();
    DCHECK(phi_class != nullptr);
    DCHECK(input_class != nullptr);
    if (phi_class == input_class) {
      // nothing todo for the class
    } else if (input_class->IsAssignableFrom(phi_class)) {
      new_rti.SetTypeHandle(input_rti.GetTypeHandle());
      new_rti.Imprecise();
    } else if (phi_class->IsAssignableFrom(input_class)) {
      new_rti.Imprecise();
    } else {
      new_rti.Top();
      break;
    }
  }
  phi->SetReferenceTypeInfo(new_rti);

  bool has_changed = new_rti.IsKnown() != existing_rti.IsKnown()
    || new_rti.IsTop() != existing_rti.IsTop()
    || new_rti.IsPrecise() != new_rti.IsPrecise()
    || (new_rti.IsKnown() && !new_rti.IsTop()
        && (new_rti.GetTypeHandle().Get() != existing_rti.GetTypeHandle().Get()));
  return has_changed;
}

void ReferenceTypePropagation::SetInstructionType(HInstruction* instr, uint16_t type_index) {
  ScopedObjectAccess soa(Thread::Current());
  mirror::DexCache* dex_cache = dex_compilation_unit_.GetClassLinker()->FindDexCache(dex_file_);
  // Get type from dex cache assuming it was populated by the verifier.
  mirror::Class* resolved_class = dex_cache->GetResolvedType(type_index);
  if (resolved_class != nullptr) {
    MutableHandle<mirror::Class> handle = handles_->NewHandle(resolved_class);
    instr->SetReferenceTypeInfo(ReferenceTypeInfo(handle));
  }
}
void ReferenceTypePropagation::VisitNewInstance(HNewInstance* instr) {
  SetInstructionType(instr, instr->GetTypeIndex());
}

void ReferenceTypePropagation::VisitLoadClass(HLoadClass* instr) {
  SetInstructionType(instr, instr->GetTypeIndex());
}

void ReferenceTypePropagation::VisitBasicBlock(HBasicBlock* block) {
  for (HInstructionIterator it(block->GetInstructions()); !it.Done(); it.Advance()) {
    if (it.Current()->IsNewInstance()) {
      VisitNewInstance(it.Current()->AsNewInstance());
    } else if (it.Current()->IsLoadClass()) {
      VisitLoadClass(it.Current()->AsLoadClass());
    }
  }
  if (block->IsLoopHeader()) {
    for (HInstructionIterator it(block->GetPhis()); !it.Done(); it.Advance()) {
      // Set the initial type for the phi. Use the non back edge input for reaching
      // a fixed point faster.
      HPhi* phi = it.Current()->AsPhi();
      AddToWorklist(phi);
      phi->SetCanBeNull(phi->InputAt(0)->CanBeNull());
      phi->SetReferenceTypeInfo(phi->InputAt(0)->GetReferenceTypeInfo());
    }
  } else {
    for (HInstructionIterator it(block->GetPhis()); !it.Done(); it.Advance()) {
      // Eagerly compute the type of the phi, for quicker convergence. Note
      // that we don't need to add users to the worklist because we are
      // doing a reverse post-order visit, therefore either the phi users are
      // non-loop phi and will be visited later in the visit, or are loop-phis,
      // and they are already in the work list.
      UpdateNullability(it.Current()->AsPhi());
      ScopedObjectAccess soa(Thread::Current());
      UpdateReferenceTypeInfo(it.Current()->AsPhi());
    }
  }
}

void ReferenceTypePropagation::ProcessWorklist() {
  ScopedObjectAccess soa(Thread::Current());
  while (!worklist_.IsEmpty()) {
    HPhi* instruction = worklist_.Pop();
    if (UpdateNullability(instruction) || UpdateReferenceTypeInfo(instruction)) {
      AddDependentInstructionsToWorklist(instruction);
    }
  }
}

void ReferenceTypePropagation::AddToWorklist(HPhi* instruction) {
  worklist_.Add(instruction);
}

void ReferenceTypePropagation::AddDependentInstructionsToWorklist(HPhi* instruction) {
  for (HUseIterator<HInstruction*> it(instruction->GetUses()); !it.Done(); it.Advance()) {
    HPhi* phi = it.Current()->GetUser()->AsPhi();
    if (phi != nullptr) {
      AddToWorklist(phi);
    }
  }
}

}  // namespace art
