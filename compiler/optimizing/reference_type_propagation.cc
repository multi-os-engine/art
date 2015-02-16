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

typedef ArenaSafeMap<int, ArenaSafeMap<int, ReferenceTypeInfo>*> TypeInfoMap;

ReferenceTypeInfo GetReferenceTypeInfoFromMap(const TypeInfoMap& type_info_map,
                                              HInstruction* instr,
                                              int block_id) {
  if (type_info_map.find(instr->GetId()) == type_info_map.end()) {
    return ReferenceTypeInfo();
  }

  ArenaSafeMap<int, ReferenceTypeInfo>* block_rti = type_info_map.Get(instr->GetId());
  if (block_rti->find(block_id) != block_rti->end()) {
    return block_rti->Get(block_id);
  }
  if (block_rti->find(instr->GetBlock()->GetBlockId()) != block_rti->end()) {
    return block_rti->Get(instr->GetBlock()->GetBlockId());
  }
  return ReferenceTypeInfo();
}

class CheckRemovalVisitor : public HGraphVisitor {
 public:
  explicit CheckRemovalVisitor(HGraph* graph,
                               const TypeInfoMap& type_info_map,
                               const ArenaSafeMap<int, ReferenceTypeInfo>& load_class_map,
                               OptimizingCompilerStats* stats)
      : HGraphVisitor(graph), type_info_map_(type_info_map),
        load_class_map_(load_class_map), stats_(stats) {}

 private:
  void VisitCheckCast(HCheckCast* instruction) OVERRIDE;

  const TypeInfoMap& type_info_map_;
  const ArenaSafeMap<int, ReferenceTypeInfo>& load_class_map_;
  OptimizingCompilerStats* stats_;
};

void CheckRemovalVisitor::VisitCheckCast(HCheckCast* check_cast) {
  int block_id = check_cast->GetBlock()->GetBlockId();
  ReferenceTypeInfo obj_rti = GetReferenceTypeInfoFromMap(
      type_info_map_, check_cast->InputAt(0), block_id);
  ReferenceTypeInfo class_rti = load_class_map_.Get(check_cast->InputAt(1)->GetId());
  // class_rti cannot be Top() as we never merge its type.
  DCHECK(!class_rti.IsTop());
  ScopedObjectAccess soa(Thread::Current());
  if (class_rti.IsSupertypeOf(obj_rti)) {
    check_cast->GetBlock()->RemoveInstruction(check_cast);
    if (stats_ != nullptr) {
      stats_->RecordStat(MethodCompilationStat::kRemovedCheckedCast);
    }
  }
}

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

ReferenceTypePropagation::~ReferenceTypePropagation() {
  for (auto it = type_info_map_.begin(); it != type_info_map_.end(); it++) {
    delete it->second;
  }
}

void ReferenceTypePropagation::Run() {
  // To properly propagate type info we need to visit in the dominator-based order.
  // Reverse post order guarantees a node's dominators are visited first.
  // We take advantage of this order in `VisitBasicBlock`.
  for (HReversePostOrderIterator it(*graph_); !it.Done(); it.Advance()) {
    VisitBasicBlock(it.Current());
  }
  ProcessWorklist();

  CheckRemovalVisitor visitor(graph_, type_info_map_, load_class_map_, stats_);
  visitor.VisitInsertionOrder();
}

void ReferenceTypePropagation::SetReferenceTypeInfo(HInstruction* instr,
                                                    ReferenceTypeInfo rti) {
  SetReferenceTypeInfo(instr, instr->GetBlock()->GetBlockId(), rti);
}

void ReferenceTypePropagation::SetReferenceTypeInfo(HInstruction* instr,
                                                    int block_id,
                                                    ReferenceTypeInfo rti) {
  int instr_id = instr->GetId();
  if (type_info_map_.find(instr_id) == type_info_map_.end()) {
    ArenaSafeMap<int, ReferenceTypeInfo>* block_map = new
        ArenaSafeMap<int, ReferenceTypeInfo>(std::less<int>(), graph_->GetArena()->Adapter());
    type_info_map_.Put(instr_id, block_map);
  }
  type_info_map_.Get(instr_id)->Overwrite(block_id, rti);
}

ReferenceTypeInfo ReferenceTypePropagation::GetReferenceTypeInfo(HInstruction* instr) {
  return GetReferenceTypeInfoFromMap(type_info_map_, instr, instr->GetBlock()->GetBlockId());
}

ReferenceTypeInfo ReferenceTypePropagation::GetReferenceTypeInfo(HInstruction* instr,
                                                                 int block_id) {
  return GetReferenceTypeInfoFromMap(type_info_map_, instr, block_id);
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

void ReferenceTypePropagation::MergeTypes(ReferenceTypeInfo& new_rti,
                                          const ReferenceTypeInfo& input_rti) {
  if (!input_rti.IsKnown()) {
    return;  // Keep the existing type
  }
  if (!new_rti.IsKnown()) {
    new_rti = input_rti;
    return;
  }
  if (input_rti.IsTop()) {
    new_rti.SetTop();
    return;
  }

  if (!input_rti.IsExact()) {
    new_rti.SetInexact();
  }

  Handle<mirror::Class> phi_type = new_rti.GetTypeHandle();
  Handle<mirror::Class> input_type = input_rti.GetTypeHandle();
  DCHECK(phi_type.Get() != nullptr);
  DCHECK(input_type.Get() != nullptr);
  if (phi_type.Get() == input_type.Get()) {
    // nothing todo for the class
  } else if (input_type->IsAssignableFrom(phi_type.Get())) {
    new_rti.SetTypeHandle(input_rti.GetTypeHandle());
    new_rti.SetInexact();
  } else if (phi_type->IsAssignableFrom(input_type.Get())) {
    new_rti.SetInexact();
  } else {
    new_rti.SetTop();
  }
}

bool ReferenceTypePropagation::UpdateReferenceTypeInfo(HPhi* phi, int block_id) {
  ReferenceTypeInfo existing_rti = GetReferenceTypeInfo(phi, block_id);
  ReferenceTypeInfo new_rti = GetReferenceTypeInfo(phi->InputAt(0), block_id);

  for (size_t i = 1; i < phi->InputCount(); i++) {
    ReferenceTypeInfo input_rti = GetReferenceTypeInfo(phi->InputAt(i), block_id);
    MergeTypes(new_rti, input_rti);
    if (new_rti.IsTop()) {
      break;
    }
  }

  SetReferenceTypeInfo(phi, block_id, new_rti);
  return !new_rti.IsEquivalent(existing_rti);
}

bool ReferenceTypePropagation::UpdateReferenceTypeInfo(HPhi* phi) {
  int phi_block_id = phi->GetBlock()->GetBlockId();
  bool has_changed = UpdateReferenceTypeInfo(phi, phi_block_id);
  std::set<int> processed_blocks;
  processed_blocks.insert(phi_block_id);
  for (HUseIterator<HInstruction*> it(phi->GetUses()); !it.Done(); it.Advance()) {
    int use_block_id = it.Current()->GetUser()->GetBlock()->GetBlockId();
    if (processed_blocks.find(use_block_id) == processed_blocks.end()) {
      has_changed |= UpdateReferenceTypeInfo(phi, use_block_id);
      processed_blocks.insert(use_block_id);
    }
  }

  return has_changed;
}

void ReferenceTypePropagation::VisitNewInstance(HNewInstance* instr) {
  ScopedObjectAccess soa(Thread::Current());
  mirror::DexCache* dex_cache = dex_compilation_unit_.GetClassLinker()->FindDexCache(dex_file_);
  // Get type from dex cache assuming it was populated by the verifier.
  mirror::Class* resolved_class = dex_cache->GetResolvedType(instr->GetTypeIndex());
  if (resolved_class != nullptr) {
    MutableHandle<mirror::Class> handle = handles_->NewHandle(resolved_class);
    SetReferenceTypeInfo(instr, ReferenceTypeInfo(handle));
  }
}

void ReferenceTypePropagation::VisitLoadClass(HLoadClass* instr) {
  ScopedObjectAccess soa(Thread::Current());
  mirror::DexCache* dex_cache = dex_compilation_unit_.GetClassLinker()->FindDexCache(dex_file_);
  // Get type from dex cache assuming it was populated by the verifier.
  mirror::Class* resolved_class = dex_cache->GetResolvedType(instr->GetTypeIndex());
  if (resolved_class != nullptr) {
    Handle<mirror::Class> handle = handles_->NewHandle(resolved_class);
    load_class_map_.Put(instr->GetId(), ReferenceTypeInfo(handle));
  }
  Handle<mirror::Class> class_handle = handles_->NewHandle(mirror::Class::GetJavaLangClass());
  SetReferenceTypeInfo(instr, ReferenceTypeInfo(class_handle));
}

void ReferenceTypePropagation::VisitBasicBlock(HBasicBlock* block) {
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
  if (block->IsLoopHeader()) {
    for (HInstructionIterator it(block->GetPhis()); !it.Done(); it.Advance()) {
      // Set the initial type for the phi. Use the non back edge input for reaching
      // a fixed point faster.
      HPhi* phi = it.Current()->AsPhi();
      int block_id = block->GetBlockId();
      AddToWorklist(phi);
      phi->SetCanBeNull(phi->InputAt(0)->CanBeNull());
      SetReferenceTypeInfo(phi, block_id, GetReferenceTypeInfo(phi->InputAt(0), block_id));
    }
  } else {
    ScopedObjectAccess soa(Thread::Current());
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

  TestForAndProcessInstanceOfSuccesor(block);
}

void ReferenceTypePropagation::TestForAndProcessInstanceOfSuccesor(HBasicBlock* block) {
  GrowableArray<HBasicBlock*> predecessors = block->GetPredecessors();
  if (predecessors.IsEmpty() || predecessors.Get(0)->IsEntryBlock()) {
    return;
  }
  HBasicBlock* previousBlock = predecessors.Get(0);
  HInstruction* previousIf = previousBlock->GetLastInstruction();
  if (!previousIf->IsIf()
      || (previousIf->AsIf()->IfFalseSuccessor() != block)) {  // InstanceOf returns 0 when True.
    return;
  }
  HInstruction* ifInput = previousIf->InputAt(0);
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
  ReferenceTypeInfo class_rti = load_class_map_.Get(load_class->GetId());
  ReferenceTypeInfo obj_rti = GetReferenceTypeInfo(obj);
  DCHECK(!class_rti.IsTop());

  ScopedObjectAccess soa(Thread::Current());
  if (!obj_rti.IsKnown()
      || obj_rti.IsTop()
      || !class_rti.GetTypeHandle()->IsAssignableFrom(obj_rti.GetTypeHandle().Get())) {
    int block_id = block->GetBlockId();
    obj_rti.SetTypeHandle(class_rti.GetTypeHandle());
    SetReferenceTypeInfo(obj, block_id, obj_rti);

    std::set<int> processed_blocks;
    processed_blocks.insert(block_id);
    for (HUseIterator<HInstruction*> it(obj->GetUses()); !it.Done(); it.Advance()) {
      HBasicBlock* use_block = it.Current()->GetUser()->GetBlock();
      int use_block_id = use_block->GetBlockId();
      if (processed_blocks.find(use_block_id) != processed_blocks.end()) {
        continue;
      }
      processed_blocks.insert(use_block_id);
      if (block->Dominates(use_block)) {
        HPhi* phi = it.Current()->GetUser()->AsPhi();
        SetReferenceTypeInfo(obj, use_block_id, obj_rti);
        if (phi != nullptr) {
          AddToWorklist(phi);
        }
      }
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
