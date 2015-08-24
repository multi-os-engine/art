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

#include "load_store_elimination.h"
#include "side_effects_analysis.h"

namespace art {

// A heap location is a reference-offset/index pair that a value can be loaded from
// or stored to.
class HeapLocation : public ArenaObject<kArenaAllocMisc> {
 public:
  static constexpr size_t kInvalidFieldOffset = -1;
  explicit HeapLocation(HInstruction* ref, size_t offset, HInstruction* index)
      : ref_(ref), offset_(offset), index_(index), may_become_unknown_(true) {
    DCHECK((offset == kInvalidFieldOffset && index != nullptr) ||
           (offset != kInvalidFieldOffset && index == nullptr));
  }

  HInstruction* GetReference() const { return ref_; }
  size_t GetOffset() const { return offset_; }
  HInstruction* GetIndex() const { return index_; }
  bool IsArrayElement() const {
    return index_ != nullptr;
  }
  bool MayBecomeUnknown() const {
    return may_become_unknown_;
  }
  void SetMayBecomeUnknown(bool val) {
    may_become_unknown_ = val;
  }

 private:
  HInstruction* const ref_;    // reference for instance/static field or array access.
  const size_t offset_;        // offset of static/instance field.
  HInstruction* const index_;  // index of an array element.
  bool may_become_unknown_;    // value may become kUnknownHeapValue.
};

class ReferenceInfo : public ArenaObject<kArenaAllocMisc> {
 public:
  explicit ReferenceInfo(HInstruction* ref, int16_t class_def_index)
      : ref_(ref), class_def_index_(class_def_index) {
    // Set is_singleton_ to true if:
    // The ref is a result of an allocation, and it's not:
    // 1) used as an input of a phi
    // 2) used as a parameter of a method call
    // 3) stored to heap
    // Set is_global_singleton_ to true if it's a singleton and isn't returned.
    is_global_singleton_ = true;
    is_singleton_ = true;
    if (!ref->IsNewInstance() && !ref->IsNewArray()) {
      is_singleton_ = false;
      is_global_singleton_ = false;
      return;
    }
    for (HUseIterator<HInstruction*> use_it(ref_->GetUses());
         !use_it.Done(); use_it.Advance()) {
      HInstruction* use = use_it.Current()->GetUser();
      if (use->IsPhi() || use->IsInvoke() ||
          (use->IsInstanceFieldSet() && (ref_ == use->InputAt(1))) ||
          (use->IsStaticFieldSet() && (ref_ == use->InputAt(1))) ||
          (use->IsArraySet() && (ref_ == use->InputAt(2)))) {
        is_singleton_ = false;
        is_global_singleton_ = false;
        return;
      }
      if (use->IsReturn()) {
        is_global_singleton_ = false;
      }
    }
  }

  HInstruction* GetReference() const {
    return ref_;
  }

  int16_t GetDeclaringClassDefIndex() const {
    return class_def_index_;
  }

  bool IsSingleton() const {
    if (!is_singleton_) {
      DCHECK_EQ(is_global_singleton_, false);
    }
    return is_singleton_;
  }

  bool IsGlobalSingleton() const {
    return is_global_singleton_;
  }

 private:
  HInstruction* const ref_;
  int16_t class_def_index_;  // Dex index of ref_'s class definition.
  bool is_singleton_;        // Can only be referred to by a single value in the method.
                             // Can't alias with any other value.
  bool is_global_singleton_; // Is a singleton and isn't returned.
};

// A HeapLocationCollector collects all relevant heap locations and keeps
// an aliasing matrix for all locations.
class HeapLocationCollector : public HGraphVisitor {
 public:
  static constexpr size_t kHeapLocationNotFound = -1;

  explicit HeapLocationCollector(HGraph* graph)
      : HGraphVisitor(graph), ref_info_array_(graph->GetArena(), 0),
        heap_locations_(graph->GetArena(), 0),
        aliasing_matrix_(graph->GetArena(), 9, true), has_heap_stores_(false),
        may_deoptimize_(false) {}

  size_t GetNumberOfHeapLocations() const {
    return heap_locations_.Size();
  }

  HeapLocation* GetHeapLocation(size_t id) const {
    return heap_locations_.Get(id);
  }

  bool IsSingleton(HInstruction* ref) const {
    ReferenceInfo* ref_info = GetReferenceInfo(ref);
    if (ref_info != nullptr) {
      return GetReferenceInfo(ref)->IsSingleton();
    }
    return false;
  }

  bool IsGlobalSingleton(HInstruction* ref) const {
    ReferenceInfo* ref_info = GetReferenceInfo(ref);
    if (ref_info != nullptr) {
      return GetReferenceInfo(ref)->IsGlobalSingleton();
    }
    return false;
  }

  bool HasHeapStores() const {
    return has_heap_stores_;
  }

  // Returns whether this method may be deoptimized.
  // Currently we don't have meta data support for deoptimizing
  // method that eliminates allocations/stores.
  bool MayDeoptimize() const {
    return may_deoptimize_;
  }

  size_t GetHeapLocationId(HInstruction* ref, size_t offset, HInstruction* index) const {
    for (size_t i = 0; i < heap_locations_.Size(); i++) {
      HeapLocation* loc = heap_locations_.Get(i);
      if (loc->GetReference() == ref &&
          loc->GetOffset() == offset &&
          loc->GetIndex() == index) {
        return i;
      }
    }
    return kHeapLocationNotFound;
  }

  // Returns true if heap_locations_[id1] and heap_locations_[id2] may alias.
  bool MayAlias(size_t id1, size_t id2) const {
    if (id1 < id2) {
      return aliasing_matrix_.IsBitSet(AliasingMatrixPos(id1, id2));
    } else if (id1 > id2) {
      return aliasing_matrix_.IsBitSet(AliasingMatrixPos(id2, id1));
    } else {
      UNREACHABLE();
    }
  }

  void BuildAliasingMatrix() {
    size_t pos = 0;
    size_t num_of_locations = heap_locations_.Size();
    if (num_of_locations == 0) {
      return;
    }
    for (size_t i = 0; i < num_of_locations - 1; i++) {
      for (size_t j = i + 1; j < num_of_locations; j++) {
        if (CalculateMayAlias(i, j)) {
          aliasing_matrix_.SetBit(AliasingMatrixPos(i, j, pos));
        }
        pos++;
      }
    }
  }

 private:
  ReferenceInfo* GetReferenceInfo(HInstruction* ref) const {
    for (size_t i = 0; i < ref_info_array_.Size(); i++) {
      ReferenceInfo* ref_info = ref_info_array_.Get(i);
      if (ref_info->GetReference() == ref) {
        return ref_info;
      }
    }
    return nullptr;
  }

  // An allocation can't alias with a value which already pre-exists, such
  // as a parameter or a load happening before the allocation.
  bool MayAliasWithPreexistenceChecking(HInstruction* ref1, HInstruction* ref2) const {
    if (ref1->IsNewInstance() || ref1->IsNewArray()) {
      if (ref2->StrictlyDominates(ref1)) {
        return false;
      }
    }
    return true;
  }

  bool MayAlias(HInstruction* ref1, HInstruction* ref2) const {
    if (ref1 == ref2) {
      return true;
    }
    ReferenceInfo* ref_info1 = GetReferenceInfo(ref1);
    if (ref_info1->IsSingleton()) {
      return false;
    }
    ReferenceInfo* ref_info2 = GetReferenceInfo(ref2);
    if (ref_info2->IsSingleton()) {
      return false;
    }
    if (ref_info1->GetDeclaringClassDefIndex() != ref_info2->GetDeclaringClassDefIndex()) {
      // Different types.
      return false;
    }
    if (!MayAliasWithPreexistenceChecking(ref1, ref2) ||
        !MayAliasWithPreexistenceChecking(ref2, ref1)) {
      return false;
    }
    return true;
  }

  size_t AliasingMatrixPos(size_t id1, size_t id2) const {
    DCHECK(id2 > id1);
    size_t num_of_locations = heap_locations_.Size();
    size_t calculated_pos;
    // It's (num_of_locations - 1) + ... + (num_of_locations - id1) + (id2 - id1 - 1)
    if (id1 == 0) {
      calculated_pos = id2 - id1 - 1;
    } else {
      calculated_pos = num_of_locations * id1 - (1 + id1) * id1 / 2 + (id2 - id1 - 1);
    }
    return calculated_pos;
  }

  // An additional pos is passed in to make sure the calculated position is correct.
  size_t AliasingMatrixPos(size_t id1, size_t id2, size_t pos) {
    size_t calculated_pos = AliasingMatrixPos(id1, id2);
    DCHECK_EQ(calculated_pos, pos);
    return calculated_pos;
  }

  // Calculate if two locations may alias to each other. The result is saved
  // in a matrix represented as a BitVector.
  bool CalculateMayAlias(size_t id1, size_t id2) const {
    HeapLocation* loc1 = heap_locations_.Get(id1);
    HeapLocation* loc2 = heap_locations_.Get(id2);
    if (loc1->GetOffset() != loc2->GetOffset()) {
      // Either two different instance fields, or one is an instance
      // field and the other is an array element.
      return false;
    }
    if (!MayAlias(loc1->GetReference(), loc2->GetReference())) {
      return false;
    }
    if (loc1->IsArrayElement()) {
      HInstruction* index1 = loc1->GetIndex();
      DCHECK(index1 != nullptr);
      HInstruction* index2 = loc2->GetIndex();
      if (index2 == nullptr) {
        // Not array element.
        return false;
      }
      if (index1->IsIntConstant() && index2->IsIntConstant() &&
          (index1->AsIntConstant()->GetValue() != index2->AsIntConstant()->GetValue())) {
        // Constant indices must be the same to alias.
        return false;
      }
    }
    return true;
  }

  HeapLocation* UpdateCollectedHeapLocations(HInstruction* ref, int16_t class_def_index,
                                    size_t offset, HInstruction* index) {
    if (ref->IsNullCheck()) {
      ref = ref->InputAt(0);
    }
    ReferenceInfo* ref_info = GetReferenceInfo(ref);
    if (ref_info == nullptr) {
      ref_info = new (GetGraph()->GetArena()) ReferenceInfo(ref, class_def_index);
      ref_info_array_.Add(ref_info);
    }
    size_t heap_location_id = GetHeapLocationId(ref, offset, index);
    if (heap_location_id == kHeapLocationNotFound) {
      HeapLocation* heap_loc = new (GetGraph()->GetArena()) HeapLocation(ref, offset, index);
      if (IsGlobalSingleton(ref)) {
        // We try to track stores to global singletons to eliminate the stores since values
        // in singleton's fields can't be killed due to aliasing. Those values can still be
        // killed due to merging values since we don't build phi for merging heap values.
        // SetMayBecomeUnknown(true) may be called later once such merge becomes possible.
        heap_loc->SetMayBecomeUnknown(false);
      }
      heap_locations_.Add(heap_loc);
      return heap_loc;
    }
    return heap_locations_.Get(heap_location_id);
  }

  void VisitFieldAccess(HInstruction* field_access,
                        HInstruction* ref,
                        const FieldInfo& field_info,
                        bool is_store) {
    uint16_t class_def_index = field_info.GetDeclaringClassDefIndex();
    size_t offset = field_info.GetFieldOffset().SizeValue();
    HeapLocation* loc = UpdateCollectedHeapLocations(ref, class_def_index, offset, nullptr);
    if (is_store && IsGlobalSingleton(ref) &&
        (field_access->GetBlock() != ref->GetBlock())) {
      // Value may be set in a block that doesn't reverse dominate the definition.
      // The value may be killed due to merging later.
      // Before we have reverse dominating info, we check if the store is in the same block
      // as the definition just to be conservative.
      loc->SetMayBecomeUnknown(true);
    }
  }

  void VisitArrayAccess(HInstruction* array, HInstruction* index) {
    UpdateCollectedHeapLocations(array, 0, HeapLocation::kInvalidFieldOffset, index);
  }

  void VisitInstanceFieldGet(HInstanceFieldGet* instruction) {
    VisitFieldAccess(instruction, instruction->InputAt(0), instruction->GetFieldInfo(), false);
  }

  void VisitInstanceFieldSet(HInstanceFieldSet* instruction) {
    VisitFieldAccess(instruction, instruction->InputAt(0), instruction->GetFieldInfo(), true);
    has_heap_stores_ = true;
  }

  void VisitStaticFieldGet(HStaticFieldGet* instruction) {
    VisitFieldAccess(instruction, instruction->InputAt(0), instruction->GetFieldInfo(), false);
  }

  void VisitStaticFieldSet(HStaticFieldSet* instruction) {
    VisitFieldAccess(instruction, instruction->InputAt(0), instruction->GetFieldInfo(), true);
    has_heap_stores_ = true;
  }

  void VisitArrayGet(HArrayGet* instruction) {
    VisitArrayAccess(instruction->InputAt(0), instruction->InputAt(1));
  }

  void VisitArraySet(HArraySet* instruction) {
    VisitArrayAccess(instruction->InputAt(0), instruction->InputAt(1));
    has_heap_stores_ = true;
  }

  void VisitDeoptimize(HDeoptimize* instruction) {
    UNUSED(instruction);
    may_deoptimize_ = true;
  }

  void VisitInvokeStaticOrDirect(HInvokeStaticOrDirect* invoke) {
    UNUSED(invoke);
    may_deoptimize_ = true;
  }

  void VisitInvokeVirtual(HInvokeVirtual* invoke) {
    UNUSED(invoke);
    may_deoptimize_ = true;
  }

  void VisitInterface(HInvokeInterface* invoke) {
    UNUSED(invoke);
    may_deoptimize_ = true;
  }

  GrowableArray<ReferenceInfo*> ref_info_array_;
  GrowableArray<HeapLocation*> heap_locations_;
  ArenaBitVector aliasing_matrix_;
  bool has_heap_stores_;    // If there is no heap stores, LSE won't be as effective.
  bool may_deoptimize_;

  DISALLOW_COPY_AND_ASSIGN(HeapLocationCollector);
};

// A killed heap value. Load is necessary.
static HInstruction* const kUnknownHeapValue =
    reinterpret_cast<HInstruction*>(static_cast<uintptr_t>(-1));
// Default heap value after an allocation.
static HInstruction* const kDefaultHeapValue =
    reinterpret_cast<HInstruction*>(static_cast<uintptr_t>(-2));

class LSEVisitor : public HGraphVisitor {
 public:
  static constexpr int kInvalidMonitorLevel = -1;

  LSEVisitor(HGraph* graph,
             const HeapLocationCollector& heap_loc_collector,
             const SideEffectsAnalysis& side_effects)
      : HGraphVisitor(graph),
        heap_location_collector_(heap_loc_collector),
        side_effects_(side_effects),
        heap_values_for_(graph->GetArena(), graph->GetBlocks().Size(), nullptr),
        removed_instructions_(graph->GetArena(), 4),
        substitute_instructions_(graph->GetArena(), 4),
        singleton_new_instances_(graph->GetArena(), 4),
        monitor_levels_at_entry_of_block_(
            graph->GetArena(), graph->GetBlocks().Size(), kInvalidMonitorLevel),
        current_monitor_level_(0) {
    // We only do LSE for non-synchronized method.
    monitor_levels_at_entry_of_block_.Put(graph->GetEntryBlock()->GetBlockId(), 0);
  }

  void VisitBasicBlock(HBasicBlock* block) OVERRIDE {
    int block_id = block->GetBlockId();
    GrowableArray<HInstruction*>* heap_values = heap_values_for_.Get(block_id);
    if (heap_values == nullptr) {
      heap_values = new (GetGraph()->GetArena()) GrowableArray<HInstruction*>(
          GetGraph()->GetArena(), heap_location_collector_.GetNumberOfHeapLocations(),
          kUnknownHeapValue);
      heap_values_for_.Put(block_id, heap_values);
    }
    if (block->IsLoopHeader()) {
      // We do a single pass in reverse post order. For loops, use the side effects as a hint
      // to see if the heap values should be killed.
      if (side_effects_.GetLoopEffects(block).DoesAnyWrite()) {
        heap_values = heap_values_for_.Get(block_id);
        HBasicBlock* preheader = block->GetPredecessors().Get(0);
        // heap_values should be the one that's reused from the preheader.
        DCHECK_EQ(heap_values, heap_values_for_.Get(preheader->GetBlockId()));
        for (size_t i = 0; i < heap_values->Size(); i++) {
          // Don't kill a global singleton's value if the value can't become
          // kUnknownHeapValue due to merging.
          if (heap_location_collector_.GetHeapLocation(i)->MayBecomeUnknown()) {
            heap_values->Put(i, kUnknownHeapValue);
          }
        }
      }
    } else {
      MergePredecessorValues(block);
    }
    current_monitor_level_ = monitor_levels_at_entry_of_block_.Get(block_id);
    DCHECK(current_monitor_level_ != kInvalidMonitorLevel);
    HGraphVisitor::VisitBasicBlock(block);

    // Try to reuse heap_values for the successor.
    const GrowableArray<HBasicBlock*> successors = block->GetSuccessors();
    if (successors.Size() == 1) {
      if (heap_values_for_.Get(successors.Get(0)->GetBlockId()) == nullptr) {
        heap_values_for_.Put(successors.Get(0)->GetBlockId(), heap_values);
      }
    }

    for (size_t i = 0; i < successors.Size(); i++) {
      HBasicBlock* successor = successors.Get(i);
      int level = monitor_levels_at_entry_of_block_.Get(successor->GetBlockId());
      if (level != kInvalidMonitorLevel) {
        // Monitor levels from different paths should agree with each other.
        DCHECK_EQ(level, current_monitor_level_);
      } else {
        monitor_levels_at_entry_of_block_.Put(successor->GetBlockId(), current_monitor_level_);
      }
    }
  }

  // Remove recorded instructions that should be eliminated.
  void RemoveInstructions() {
    size_t size = removed_instructions_.Size();
    DCHECK_EQ(size, substitute_instructions_.Size());
    for (size_t i = 0; i < size; i++) {
      HInstruction* instruction = removed_instructions_.Get(i);
      DCHECK(instruction != nullptr);
      HInstruction* substitute = substitute_instructions_.Get(i);
      if (substitute != nullptr) {
        // Keep tracing substitute till one that's not removed.
        HInstruction* sub_sub = FindSubstitute(substitute);
        while (sub_sub != substitute) {
          substitute = sub_sub;
          sub_sub = FindSubstitute(substitute);
        }
        instruction->ReplaceWith(substitute);
      }
      instruction->GetBlock()->RemoveInstruction(instruction);
    }
    // Remove unnecessary allocations.
    for (size_t i = 0; i < singleton_new_instances_.Size(); i++) {
      HInstruction* new_instance = singleton_new_instances_.Get(i);
      if (!new_instance->HasUses()) {
        new_instance->GetBlock()->RemoveInstruction(new_instance);
      }
    }
  }

 private:
  void MergePredecessorValues(HBasicBlock* block) {
    const GrowableArray<HBasicBlock*>& predecessors = block->GetPredecessors();
    if (predecessors.Size() == 0) {
      return;
    }
    GrowableArray<HInstruction*>* heap_values = heap_values_for_.Get(block->GetBlockId());
    for (size_t i = 0; i < heap_values->Size(); i++) {
      HInstruction* value = heap_values_for_.Get(predecessors.Get(0)->GetBlockId())->Get(i);
      if (value != kUnknownHeapValue) {
        for (size_t j = 1; j < predecessors.Size(); j++) {
          if (heap_values_for_.Get(predecessors.Get(j)->GetBlockId())->Get(i) != value) {
            value = kUnknownHeapValue;
            break;
          }
        }
      }
      heap_values->Put(i, value);
    }
  }

  // Only fields that aren't volatile or inside synchronized blocks can be removed.
  bool CanRemove(HInstruction* instruction) {
    bool is_volatile =
        (instruction->IsInstanceFieldGet() &&
         instruction->AsInstanceFieldGet()->GetFieldInfo().IsVolatile()) ||
        (instruction->IsInstanceFieldSet() &&
         instruction->AsInstanceFieldSet()->GetFieldInfo().IsVolatile()) ||
        (instruction->IsStaticFieldGet() &&
         instruction->AsStaticFieldGet()->GetFieldInfo().IsVolatile()) ||
        (instruction->IsStaticFieldSet() &&
         instruction->AsStaticFieldSet()->GetFieldInfo().IsVolatile());
    return !is_volatile && (current_monitor_level_ == 0);
  }

  // `instruction` is being removed. Try to see if the null check on it
  // can be removed.
  void TryRemovingNullCheck(HInstruction* instruction) {
    HInstruction* prev = instruction->GetPrevious();
    if ((prev != nullptr) && prev->IsNullCheck() && (prev == instruction->InputAt(0))) {
      // Previous instruction is a null check for this instruction. Remove the null check.
      prev->ReplaceWith(prev->InputAt(0));
      prev->GetBlock()->RemoveInstruction(prev);
    }
  }

  HInstruction* GetDefaultValue(Primitive::Type type) {
    switch (type) {
      case Primitive::kPrimNot:
        return GetGraph()->GetNullConstant();
      case Primitive::kPrimBoolean:
      case Primitive::kPrimByte:
      case Primitive::kPrimChar:
      case Primitive::kPrimShort:
      case Primitive::kPrimInt:
        return GetGraph()->GetIntConstant(0);
      case Primitive::kPrimLong:
        return GetGraph()->GetLongConstant(0);
      case Primitive::kPrimFloat:
        return GetGraph()->GetFloatConstant(0);
      case Primitive::kPrimDouble:
        return GetGraph()->GetDoubleConstant(0);
      default:
        UNREACHABLE();
    }
  }

  void VisitGetLocation(HInstruction* instruction, HInstruction* ref,
                        size_t offset, HInstruction* index) {
    if (ref->IsNullCheck()) {
      ref = ref->InputAt(0);
    }
    size_t id = heap_location_collector_.GetHeapLocationId(ref, offset, index);
    DCHECK_NE(id, HeapLocationCollector::kHeapLocationNotFound);
    GrowableArray<HInstruction*>* heap_values =
        heap_values_for_.Get(instruction->GetBlock()->GetBlockId());
    HInstruction* heap_value = heap_values->Get(id);
    if (heap_value == kDefaultHeapValue) {
        if (CanRemove(instruction)) {
          HInstruction* constant = GetDefaultValue(instruction->GetType());
          removed_instructions_.Add(instruction);
          substitute_instructions_.Add(constant);
          heap_values->Put(id, constant);
          return;
        } else {
          // Proceed as an unknown value to do the load.
          heap_value = kUnknownHeapValue;
        }
    }
    if ((heap_value != kUnknownHeapValue) &&
        // Keep the load if e.g. one is int, the other is float.
        (heap_value->GetType() == instruction->GetType()) &&
        CanRemove(instruction)) {
      removed_instructions_.Add(instruction);
      substitute_instructions_.Add(heap_value);
      TryRemovingNullCheck(instruction);
    }
    if (heap_value == kUnknownHeapValue) {
      // Put the load as the value into the HeapLocation.
      // This acts like GVN but with better aliasing analysis.
      heap_values->Put(id, instruction);
    }
  }

  void VisitSetLocation(HInstruction* instruction, HInstruction* ref,
                        size_t offset, HInstruction* index, HInstruction* value) {
    if (ref->IsNullCheck()) {
      ref = ref->InputAt(0);
    }
    size_t id = heap_location_collector_.GetHeapLocationId(ref, offset, index);
    DCHECK_NE(id, HeapLocationCollector::kHeapLocationNotFound);
    GrowableArray<HInstruction*>* heap_values =
        heap_values_for_.Get(instruction->GetBlock()->GetBlockId());
    HInstruction* heap_value = heap_values->Get(id);
    bool redundant_store = false;
    if (heap_value == value) {
      // Store into the heap location with the same value.
      redundant_store = true;
    } else {
      if (!heap_location_collector_.MayDeoptimize() &&
          heap_location_collector_.IsGlobalSingleton(ref) &&
          (index == nullptr) &&
          !heap_location_collector_.GetHeapLocation(id)->MayBecomeUnknown()) {
        // Store into a global singleton's field. And that value can't be killed due
        // to merge. It's redundant since future loads will get `value`.
        redundant_store = true;
      }
    }
    if (redundant_store && CanRemove(instruction)) {
      removed_instructions_.Add(instruction);
      substitute_instructions_.Add(nullptr);
      TryRemovingNullCheck(instruction);
    }
    heap_values->Put(id, value);
    for (size_t i = 0; i < heap_values->Size(); i++) {
      if (i != id) {
        // Kill heap locations that may alias.
        // Skip if the location has the same value or if the value already killed.
        if ((heap_values->Get(i) != value) &&
            (heap_values->Get(i) != kUnknownHeapValue) &&
            heap_location_collector_.MayAlias(i, id)) {
          heap_values->Put(i, kUnknownHeapValue);
        }
      }
    }
  }

  void VisitInstanceFieldGet(HInstanceFieldGet* instruction) {
    HInstruction* obj = instruction->InputAt(0);
    size_t offset = instruction->GetFieldInfo().GetFieldOffset().SizeValue();
    VisitGetLocation(instruction, obj, offset, nullptr);
  }

  void VisitInstanceFieldSet(HInstanceFieldSet* instruction) {
    HInstruction* obj = instruction->InputAt(0);
    size_t offset = instruction->GetFieldInfo().GetFieldOffset().SizeValue();
    HInstruction* value = instruction->InputAt(1);
    VisitSetLocation(instruction, obj, offset, nullptr, value);
  }

  void VisitStaticFieldGet(HStaticFieldGet* instruction) {
    HInstruction* cls = instruction->InputAt(0);
    size_t offset = instruction->GetFieldInfo().GetFieldOffset().SizeValue();
    VisitGetLocation(instruction, cls, offset, nullptr);
  }

  void VisitStaticFieldSet(HStaticFieldSet* instruction) {
    HInstruction* cls = instruction->InputAt(0);
    size_t offset = instruction->GetFieldInfo().GetFieldOffset().SizeValue();
    HInstruction* value = instruction->InputAt(1);
    VisitSetLocation(instruction, cls, offset, nullptr, value);
  }

  void VisitArrayGet(HArrayGet* instruction) {
    HInstruction* array = instruction->InputAt(0);
    HInstruction* index = instruction->InputAt(1);
    VisitGetLocation(instruction, array, HeapLocation::kInvalidFieldOffset, index);
  }

  void VisitArraySet(HArraySet* instruction) {
    HInstruction* array = instruction->InputAt(0);
    HInstruction* index = instruction->InputAt(1);
    HInstruction* value = instruction->InputAt(2);
    VisitSetLocation(instruction, array, HeapLocation::kInvalidFieldOffset, index, value);
  }

  void HandleInvoke(HInstruction* invoke) {
    GrowableArray<HInstruction*>* heap_values =
        heap_values_for_.Get(invoke->GetBlock()->GetBlockId());
    for (size_t i = 0; i < heap_values->Size(); i++) {
      HInstruction* ref = heap_location_collector_.GetHeapLocation(i)->GetReference();
      // Method invocation invalidates all heap locations except those that have
      // singleton references.
      if (!heap_location_collector_.IsSingleton(ref)) {
        heap_values->Put(i, kUnknownHeapValue);
      }
    }
  }

  void VisitInvokeStaticOrDirect(HInvokeStaticOrDirect* invoke) {
    HandleInvoke(invoke);
  }

  void VisitInvokeVirtual(HInvokeVirtual* invoke) {
    HandleInvoke(invoke);
  }

  void VisitInterface(HInvokeInterface* invoke) {
    HandleInvoke(invoke);
  }

  void VisitClinitCheck(HClinitCheck* clinit) {
    HandleInvoke(clinit);
  }

  void VisitNewInstance(HNewInstance* new_instance) OVERRIDE {
    if (!heap_location_collector_.MayDeoptimize() &&
        heap_location_collector_.IsGlobalSingleton(new_instance)) {
      // Global singleton's allocation may be eliminated.
      singleton_new_instances_.Add(new_instance);
    }
    GrowableArray<HInstruction*>* heap_values =
        heap_values_for_.Get(new_instance->GetBlock()->GetBlockId());
    for (size_t i = 0; i < heap_values->Size(); i++) {
      HInstruction* ref = heap_location_collector_.GetHeapLocation(i)->GetReference();
      size_t offset = heap_location_collector_.GetHeapLocation(i)->GetOffset();
      if (ref == new_instance && offset >= mirror::kObjectHeaderSize) {
        // Instance fields except the header fields are set to default heap values.
        heap_values->Put(i, kDefaultHeapValue);
      }
    }
  }

  void VisitMonitorOperation(HMonitorOperation* monitor) {
    if (monitor->IsEnter()) {
      current_monitor_level_++;
    } else {
      current_monitor_level_--;
    }
  }

  // Find an instruction's substitute if it should be removed.
  // Return the same instruction if it should not be removed.
  HInstruction* FindSubstitute(HInstruction* instruction) {
    size_t size = removed_instructions_.Size();
    for (size_t i = 0; i < size; i++) {
      if (removed_instructions_.Get(i) == instruction) {
        return substitute_instructions_.Get(i);
      }
    }
    return instruction;
  }

  const HeapLocationCollector& heap_location_collector_;
  const SideEffectsAnalysis& side_effects_;

  // One array of heap values for each block.
  GrowableArray<GrowableArray<HInstruction*>*> heap_values_for_;

  // We record the instructions that should be eliminated but may be
  // used by heap locations. They'll be removed in the end.
  GrowableArray<HInstruction*> removed_instructions_;
  GrowableArray<HInstruction*> substitute_instructions_;
  GrowableArray<HInstruction*> singleton_new_instances_;

  GrowableArray<int> monitor_levels_at_entry_of_block_;
  int current_monitor_level_;

  DISALLOW_COPY_AND_ASSIGN(LSEVisitor);
};

void LoadStoreElimination::Run() {
  if (is_synchronized_) {
    return;
  }
  HeapLocationCollector heap_location_collector(graph_);
  for (HReversePostOrderIterator it(*graph_); !it.Done(); it.Advance()) {
    heap_location_collector.VisitBasicBlock(it.Current());
  }
  if (!heap_location_collector.HasHeapStores()) {
    // Without heap stores, this pass acts mostly as GVN on heap accesses.
    return;
  }
  heap_location_collector.BuildAliasingMatrix();
  LSEVisitor lse_visitor(graph_, heap_location_collector, side_effects_);
  for (HReversePostOrderIterator it(*graph_); !it.Done(); it.Advance()) {
    lse_visitor.VisitBasicBlock(it.Current());
  }
  lse_visitor.RemoveInstructions();
}

}  // namespace art
