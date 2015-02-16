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

// TODO: handle: a !=/== null.

void ReferenceTypePropagation::Run() {
  // To properly propagate type info we need to visit in the dominator-based order.
  // Reverse post order guarantees a node's dominators are visited first.
  // We take advantage of this order in `VisitBasicBlock`.
  for (HReversePostOrderIterator it(*graph_); !it.Done(); it.Advance()) {
    VisitBasicBlock(it.Current());
  }
  ProcessWorklist();
}

void ReferenceTypePropagation::VisitBasicBlock(HBasicBlock* block) {
  InitializeExactTypes(block);
  HandlePhis(block);
  BoundTypeForIfInstanceOf(block);
}

void ReferenceTypePropagation::InitializeExactTypes(HBasicBlock* block) {
  // TODO: handle other instructions that give type info
  // (NewArray/Call/Field accesses/array accesses)
  for (HInstructionIterator it(block->GetInstructions()); !it.Done(); it.Advance()) {
    HInstruction* instr = it.Current();
    if (instr->IsNewInstance()) {
      VisitNewInstance(instr->AsNewInstance());
    } else if (instr->IsLoadClass()) {
      VisitLoadClass(instr->AsLoadClass());
    }
  }
}

void ReferenceTypePropagation::HandlePhis(HBasicBlock* block) {
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
      HPhi* phi = it.Current()->AsPhi();
      UpdateNullability(phi);
      UpdateReferenceTypeInfo(phi);
    }
  }
}

// Detects if `block` is the True block for the pattern
// `if (x instanceof ClassX) { }`
// If that's the case insert an HBoundType instruction to bound the type of `x`
// to `ClassX` in the scope of the dominated blocks.
void ReferenceTypePropagation::BoundTypeForIfInstanceOf(HBasicBlock* block) {
  HInstruction* lastInstruction = block->GetLastInstruction();
  if (!lastInstruction->IsIf()) {
    return;
  }
  HInstruction* ifInput = lastInstruction->InputAt(0);
  if (!ifInput->IsEqual()) {
    return;
  }
  HInstruction* instanceOf = ifInput->InputAt(0);
  HInstruction* zero = ifInput->InputAt(1);
  if (!instanceOf->IsInstanceOf() || !zero->IsConstant()
      || (zero->AsIntConstant()->GetValue() != 0)) {
    return;
  }

  HInstruction* obj = instanceOf->InputAt(0);
  HLoadClass* load_class = instanceOf->InputAt(1)->AsLoadClass();

  ReferenceTypeInfo obj_rti = obj->GetReferenceTypeInfo();
  ReferenceTypeInfo class_rti = load_class->GetLoadedClassRTI();
  HBoundType* bound_type = new (graph_->GetArena()) HBoundType(obj, class_rti);

  // Narrow the type as much as possible.
  {
    ScopedObjectAccess soa(Thread::Current());
    if (!load_class->IsResolved() || class_rti.IsSupertypeOf(obj_rti)) {
      bound_type->SetReferenceTypeInfo(obj_rti);
    } else {
      class_rti.SetInexact();
      bound_type->SetReferenceTypeInfo(class_rti);
    }
  }

  block->InsertInstructionBefore(bound_type, lastInstruction);
  // HInstanceOf returns 1 when True but HIf will compare with 0, so the branch
  // where HInstanceOf is True is actually the FalseSuccessor.
  HBasicBlock* instanceOfTrueBlock = lastInstruction->AsIf()->IfFalseSuccessor();

  for (HUseIterator<HInstruction*> it(obj->GetUses()); !it.Done(); it.Advance()) {
    HInstruction* user = it.Current()->GetUser();
    if (instanceOfTrueBlock->Dominates(user->GetBlock())) {
      user->ReplaceInput(bound_type, it.Current()->GetIndex());
    }
  }
}

void ReferenceTypePropagation::VisitNewInstance(HNewInstance* instr) {
  ScopedObjectAccess soa(Thread::Current());
  mirror::DexCache* dex_cache = dex_compilation_unit_.GetClassLinker()->FindDexCache(dex_file_);
  // Get type from dex cache assuming it was populated by the verifier.
  mirror::Class* resolved_class = dex_cache->GetResolvedType(instr->GetTypeIndex());
  if (resolved_class != nullptr) {
    MutableHandle<mirror::Class> handle = handles_->NewHandle(resolved_class);
    instr->SetReferenceTypeInfo(ReferenceTypeInfo(handle, true));
  }
}

void ReferenceTypePropagation::VisitLoadClass(HLoadClass* instr) {
  ScopedObjectAccess soa(Thread::Current());
  mirror::DexCache* dex_cache = dex_compilation_unit_.GetClassLinker()->FindDexCache(dex_file_);
  // Get type from dex cache assuming it was populated by the verifier.
  mirror::Class* resolved_class = dex_cache->GetResolvedType(instr->GetTypeIndex());
  if (resolved_class != nullptr) {
    Handle<mirror::Class> handle = handles_->NewHandle(resolved_class);
    instr->SetLoadedClassRTI(ReferenceTypeInfo(handle, true));
  }
  Handle<mirror::Class> class_handle = handles_->NewHandle(mirror::Class::GetJavaLangClass());
  instr->SetReferenceTypeInfo(ReferenceTypeInfo(class_handle, true));
}

// Merges new_rti with input_rti.
void ReferenceTypePropagation::MergeTypes(ReferenceTypeInfo* new_rti,
                                          const ReferenceTypeInfo& input_rti) {
  if (!input_rti.IsExact()) {
    new_rti->SetInexact();
  }

  if (new_rti->IsTop()) {
    // nothing todo, we are already at top.
  } else if (input_rti.IsTop()) {
    new_rti->SetTop();
  } else if (new_rti->GetTypeHandle().Get() == input_rti.GetTypeHandle().Get()) {
    // nothing to do if we have the same type
  } else if (input_rti.IsSupertypeOf(*new_rti)) {
    new_rti->SetTypeHandle(input_rti.GetTypeHandle(), false);
  } else if (new_rti->IsSupertypeOf(input_rti)) {
    new_rti->SetInexact();
  } else {
    // TODO: Find common parent.
    new_rti->SetTop();
    new_rti->SetInexact();
  }
}

bool ReferenceTypePropagation::UpdateReferenceTypeInfo(HInstruction* instr) {
  ScopedObjectAccess soa(Thread::Current());

  ReferenceTypeInfo previous_rti = instr->GetReferenceTypeInfo();
  ReferenceTypeInfo new_rti;
  if (instr->IsBoundType()) {
    VisitBoundType(instr->AsBoundType());
  } else if (instr->IsPhi()) {
    VisitPhi(instr->AsPhi());
  } else {
    LOG(FATAL) << "Invalid instruction (should not get here)";
  }

  return !previous_rti.IsEqual(instr->GetReferenceTypeInfo());
}

void ReferenceTypePropagation::VisitBoundType(HBoundType* instr) {
  ReferenceTypeInfo new_rti = instr->InputAt(0)->GetReferenceTypeInfo();
  // Be sure that we don't go over the bounded type.
  ReferenceTypeInfo top_rti = instr->GetMaxType();
  if (!top_rti.IsSupertypeOf(new_rti)) {
    new_rti = top_rti;
  }
  instr->SetReferenceTypeInfo(new_rti);
}

void ReferenceTypePropagation::VisitPhi(HPhi* instr) {
  ReferenceTypeInfo new_rti = instr->InputAt(0)->GetReferenceTypeInfo();
  if (new_rti.IsTop() && !new_rti.IsExact()) {
    // Early return if we are Top and inexact.
    instr->SetReferenceTypeInfo(new_rti);
    return;
  }
  for (size_t i = 1; i < instr->InputCount(); i++) {
    MergeTypes(&new_rti, instr->InputAt(i)->GetReferenceTypeInfo());
    if (new_rti.IsTop()) {
      if (!new_rti.IsExact()) {
        break;
      } else {
        continue;
      }
    }
  }
  instr->SetReferenceTypeInfo(new_rti);
}

// Re-computes and updates the nullability of the instruction. Returns whether or
// not the nullability was changed.
bool ReferenceTypePropagation::UpdateNullability(HInstruction* instr) {
  DCHECK(instr->IsPhi() || instr->IsBoundType());

  if (!instr->IsPhi()) {
    return false;
  }

  HPhi* phi = instr->AsPhi();
  bool existing_can_be_null = phi->CanBeNull();
  bool new_can_be_null = false;
  for (size_t i = 0; i < phi->InputCount(); i++) {
    new_can_be_null |= phi->InputAt(i)->CanBeNull();
  }
  phi->SetCanBeNull(new_can_be_null);

  return existing_can_be_null != new_can_be_null;
}

void ReferenceTypePropagation::ProcessWorklist() {
  while (!worklist_.IsEmpty()) {
    HInstruction* instruction = worklist_.Pop();
    if (UpdateNullability(instruction) || UpdateReferenceTypeInfo(instruction)) {
      AddDependentInstructionsToWorklist(instruction);
    }
  }
}

void ReferenceTypePropagation::AddToWorklist(HInstruction* instruction) {
  worklist_.Add(instruction);
}

void ReferenceTypePropagation::AddDependentInstructionsToWorklist(HInstruction* instruction) {
  for (HUseIterator<HInstruction*> it(instruction->GetUses()); !it.Done(); it.Advance()) {
    HInstruction* user = it.Current()->GetUser();
    if (user->IsPhi() || user->IsBoundType()) {
      AddToWorklist(user);
    }
  }
}
}  // namespace art
