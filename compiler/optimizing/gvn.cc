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

#include "gvn.h"
#include "side_effects_analysis.h"

namespace art {

/**
 * A node in the collision list of a ValueSet. Encodes the instruction,
 * the hash code, and the next node in the collision list.
 */
class ValueSetNode : public ArenaObject<kArenaAllocMisc> {
 public:
  ValueSetNode(HInstruction* instruction, size_t hash_code, ValueSetNode* next)
      : instruction_(instruction), hash_code_(hash_code), next_(next) {}

  size_t GetHashCode() const { return hash_code_; }
  HInstruction* GetInstruction() const { return instruction_; }
  ValueSetNode* GetNext() const { return next_; }
  void SetNext(ValueSetNode* node) { next_ = node; }

 private:
  HInstruction* const instruction_;
  const size_t hash_code_;
  ValueSetNode* next_;

  DISALLOW_COPY_AND_ASSIGN(ValueSetNode);
};

/**
 * A ValueSet holds instructions that can replace other instructions. It is updated
 * through the `Add` method, and the `Kill` method. The `Kill` method removes
 * instructions that are affected by the given side effect.
 *
 * The `Lookup` method returns an equivalent instruction to the given instruction
 * if there is one in the set. In GVN, we would say those instructions have the
 * same "number".
 */
class ValueSet : public ArenaObject<kArenaAllocMisc> {
 public:
  explicit ValueSet(ArenaAllocator* allocator)
      : ValueSet(allocator, kDefaultNumberOfEntries) {}

  ValueSet(ArenaAllocator* allocator, size_t initial_size)
    : allocator_(allocator),
      number_of_entries_(0),
      collisions_(nullptr),
      table_(allocator, NearestPowerOf2(initial_size), nullptr) {
    hash_code_mask_ = table_.Size() - 1;
  }

  // Adds an instruction in the set.
  void Add(HInstruction* instruction) {
    DCHECK(Lookup(instruction) == nullptr);
    size_t hash_code = instruction->ComputeHashCode();
    size_t index = hash_code & hash_code_mask_;
    if (table_.Get(index) == nullptr) {
      table_.Put(index, instruction);
    } else {
      collisions_ = new (allocator_) ValueSetNode(instruction, hash_code, collisions_);
    }
    ++number_of_entries_;

    if (LoadTooHigh()) {
      GrowAndRehash();
      DCHECK(!LoadTooHigh());
    }
  }

  // If in the set, returns an equivalent instruction to the given instruction. Returns
  // null otherwise.
  HInstruction* Lookup(HInstruction* instruction) const {
    DCHECK(instruction != nullptr);

    size_t hash_code = instruction->ComputeHashCode();
    size_t index = hash_code & hash_code_mask_;
    HInstruction* existing = table_.Get(index);
    if (existing != nullptr && existing->Equals(instruction)) {
      return existing;
    }

    for (ValueSetNode* node = collisions_; node != nullptr; node = node->GetNext()) {
      if (node->GetHashCode() == hash_code) {
        existing = node->GetInstruction();
        if (existing->Equals(instruction)) {
          return existing;
        }
      }
    }
    return nullptr;
  }

  // Returns whether `instruction` is in the set.
  HInstruction* IdentityLookup(HInstruction* instruction) const {
    DCHECK(instruction != nullptr);

    size_t hash_code = instruction->ComputeHashCode();
    size_t index = hash_code & hash_code_mask_;
    HInstruction* existing = table_.Get(index);
    if (existing == instruction) {
      return existing;
    }

    for (ValueSetNode* node = collisions_; node != nullptr; node = node->GetNext()) {
      existing = node->GetInstruction();
      if (existing == instruction) {
        return existing;
      }
    }
    return nullptr;
  }

  // Removes all instructions in the set that are affected by the given side effects.
  void Kill(SideEffects side_effects) {
    for (size_t i = 0; i < table_.Size(); ++i) {
      HInstruction* instruction = table_.Get(i);
      if (instruction != nullptr && instruction->GetSideEffects().DependsOn(side_effects)) {
        table_.Put(i, nullptr);
        --number_of_entries_;
      }
    }

    for (ValueSetNode* current = collisions_, *previous = nullptr;
         current != nullptr;
         current = current->GetNext()) {
      HInstruction* instruction = current->GetInstruction();
      if (instruction->GetSideEffects().DependsOn(side_effects)) {
        if (previous == nullptr) {
          collisions_ = current->GetNext();
        } else {
          previous->SetNext(current->GetNext());
        }
        --number_of_entries_;
      } else {
        previous = current;
      }
    }
  }

  // Returns a copy of this set.
  ValueSet* Copy() const {
    return new (allocator_) ValueSet(this);
  }

  void Clear() {
    number_of_entries_ = 0;
    collisions_ = nullptr;
    for (size_t i = 0; i < table_.Size(); ++i) {
      table_.Put(i, nullptr);
    }
  }

  // Update this `ValueSet` by intersecting with instructions in `other`.
  void IntersectionWith(const ValueSet* other) {
    if (IsEmpty()) {
      return;
    } else if (other->IsEmpty()) {
      Clear();
    } else {
      for (size_t i = 0; i < table_.Size(); ++i) {
        HInstruction* instruction = table_.Get(i);
        if (instruction != nullptr && other->IdentityLookup(instruction) == nullptr) {
          --number_of_entries_;
          table_.Put(i, nullptr);
        }
      }
      for (ValueSetNode* current = collisions_, *previous = nullptr;
           current != nullptr;
           current = current->GetNext()) {
        if (other->IdentityLookup(current->GetInstruction()) == nullptr) {
          if (previous == nullptr) {
            collisions_ = current->GetNext();
          } else {
            previous->SetNext(current->GetNext());
          }
          --number_of_entries_;
        } else {
          previous = current;
        }
      }
    }
  }

  bool IsEmpty() const { return number_of_entries_ == 0; }
  size_t GetNumberOfEntries() const { return number_of_entries_; }

 private:
  // Private copy constructor. It passes a pointer to avoid colliding with
  // the standard copy constructor deleted below.
  explicit ValueSet(const ValueSet* to_clone)
    : allocator_(to_clone->allocator_),
      number_of_entries_(to_clone->number_of_entries_),
      hash_code_mask_(to_clone->hash_code_mask_),
      collisions_(nullptr),
      table_(to_clone->table_) {
    // Note that the order will be inverted in the copy. This is fine, as the order is not
    // relevant for a ValueSet.
    for (ValueSetNode* node = to_clone->collisions_; node != nullptr; node = node->GetNext()) {
      collisions_ = new (allocator_) ValueSetNode(
          node->GetInstruction(), node->GetHashCode(), collisions_);
    }
  }

  bool LoadTooHigh() {
    // Load factor threshold (ratio of entries over buckets) is 0.75
    return number_of_entries_ * 4 > 3 * table_.Size();
  }

  size_t NearestPowerOf2(size_t x) {
    if ((x != 0) && !(x & (x - 1))) {
      // x is a power of two already
      return x;
    } else {
      size_t pow2 = 1;
      while (pow2 < x) {
        pow2 <<= 1;
      }
      return pow2;
    }
  }

  void GrowAndRehash() {
    size_t old_size = table_.Size();
    size_t new_size = old_size << 1;
    DCHECK_LT(old_size, new_size);

    table_.SetSize(new_size);
    hash_code_mask_ = new_size - 1;
    for (size_t i = old_size; i < new_size; ++i) {
      table_.Put(i, nullptr);
    }

    for (size_t old_index = 0; old_index < old_size; ++old_index) {
      HInstruction* instruction = table_.Get(old_index);
      if (instruction == nullptr) continue;
      size_t hash_code = instruction->ComputeHashCode();
      size_t new_index = hash_code & hash_code_mask_;
      if (old_index != new_index) {
        table_.Put(old_index, nullptr);
        table_.Put(new_index, instruction);
      }
    }

    ValueSetNode* next;
    ValueSetNode* node = collisions_;
    collisions_ = nullptr;
    while (node != nullptr) {
      next = node->GetNext();
      HInstruction* instruction = node->GetInstruction();
      size_t hash_code = node->GetHashCode();
      size_t new_index = hash_code & hash_code_mask_;
      if (table_.Get(new_index) == nullptr) {
        table_.Put(new_index, instruction);
      } else {
        node->SetNext(collisions_);
        collisions_ = node;
      }
      node = next;
    }
  }

  static constexpr size_t kDefaultNumberOfEntries = 8;

  ArenaAllocator* const allocator_;

  // The number of entries in the set.
  size_t number_of_entries_;

  // Bitmask for converting a hash code into a table index.
  size_t hash_code_mask_;

  // The internal implementation of the set. It uses a combination of a hash code based
  // growable list, and a linked list to handle hash code collisions.
  // TODO: Tune the initial size.
  ValueSetNode* collisions_;
  GrowableArray<HInstruction*> table_;

  DISALLOW_COPY_AND_ASSIGN(ValueSet);
};

/**
 * Optimization phase that removes redundant instruction.
 */
class GlobalValueNumberer : public ValueObject {
 public:
  GlobalValueNumberer(ArenaAllocator* allocator,
                      HGraph* graph,
                      const SideEffectsAnalysis& side_effects)
      : graph_(graph),
        allocator_(allocator),
        side_effects_(side_effects),
        sets_(allocator, graph->GetBlocks().Size(), nullptr) {}

  void Run();

 private:
  // Per-block GVN. Will also update the ValueSet of the dominated and
  // successor blocks.
  void VisitBasicBlock(HBasicBlock* block);

  HGraph* graph_;
  ArenaAllocator* const allocator_;
  const SideEffectsAnalysis& side_effects_;

  // ValueSet for blocks. Initially null, but for an individual block they
  // are allocated and populated by the dominator, and updated by all blocks
  // in the path from the dominator to the block.
  GrowableArray<ValueSet*> sets_;

  DISALLOW_COPY_AND_ASSIGN(GlobalValueNumberer);
};

void GlobalValueNumberer::Run() {
  DCHECK(side_effects_.HasRun());
  sets_.Put(graph_->GetEntryBlock()->GetBlockId(), new (allocator_) ValueSet(allocator_));

  // Use the reverse post order to ensure the non back-edge predecessors of a block are
  // visited before the block itself.
  for (HReversePostOrderIterator it(*graph_); !it.Done(); it.Advance()) {
    VisitBasicBlock(it.Current());
  }
}

void GlobalValueNumberer::VisitBasicBlock(HBasicBlock* block) {
  ValueSet* set = nullptr;
  const GrowableArray<HBasicBlock*>& predecessors = block->GetPredecessors();
  if (predecessors.Size() == 0 || predecessors.Get(0)->IsEntryBlock()) {
    // The entry block should only accumulate constant instructions, and
    // the builder puts constants only in the entry block.
    // Therefore, there is no need to propagate the value set to the next block.
    set = new (allocator_) ValueSet(allocator_);
  } else {
    HBasicBlock* dominator = block->GetDominator();
    set = sets_.Get(dominator->GetBlockId());
    if (dominator->GetSuccessors().Size() != 1 || dominator->GetSuccessors().Get(0) != block) {
      // We have to copy if the dominator has other successors, or `block` is not a successor
      // of the dominator.
      set = set->Copy();
    }
    if (!set->IsEmpty()) {
      if (block->IsLoopHeader()) {
        DCHECK_EQ(block->GetDominator(), block->GetLoopInformation()->GetPreHeader());
        set->Kill(side_effects_.GetLoopEffects(block));
      } else if (predecessors.Size() > 1) {
        for (size_t i = 0, e = predecessors.Size(); i < e; ++i) {
          set->IntersectionWith(sets_.Get(predecessors.Get(i)->GetBlockId()));
          if (set->IsEmpty()) {
            break;
          }
        }
      }
    }
  }

  sets_.Put(block->GetBlockId(), set);

  HInstruction* current = block->GetFirstInstruction();
  while (current != nullptr) {
    set->Kill(current->GetSideEffects());
    // Save the next instruction in case `current` is removed from the graph.
    HInstruction* next = current->GetNext();
    if (current->CanBeMoved()) {
      HInstruction* existing = set->Lookup(current);
      if (existing != nullptr) {
        current->ReplaceWith(existing);
        current->GetBlock()->RemoveInstruction(current);
      } else {
        set->Add(current);
      }
    }
    current = next;
  }
}

void GVNOptimization::Run() {
  GlobalValueNumberer gvn(graph_->GetArena(), graph_, side_effects_);
  gvn.Run();
}

}  // namespace art
