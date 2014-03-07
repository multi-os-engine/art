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

#include "remembered_set.h"

#include "base/stl_util.h"
#include "card_table-inl.h"
#include "heap_bitmap.h"
#include "gc/collector/mark_sweep.h"
#include "gc/collector/mark_sweep-inl.h"
#include "gc/collector/semi_space.h"
#include "gc/heap.h"
#include "gc/space/space.h"
#include "mirror/art_field-inl.h"
#include "mirror/object-inl.h"
#include "mirror/class-inl.h"
#include "mirror/object_array-inl.h"
#include "space_bitmap-inl.h"
#include "thread.h"
#include "UniquePtr.h"

namespace art {
namespace gc {
namespace accounting {

class RememberedSetCardVisitor {
 public:
  explicit RememberedSetCardVisitor(RememberedSet::CardSet* const dirty_cards)
      : dirty_cards_(dirty_cards) {}

  void operator()(byte* card, byte expected_value, byte new_value) const {
    if (expected_value == CardTable::kCardDirty) {
      dirty_cards_->insert(card);
    }
  }

 private:
  RememberedSet::CardSet* const dirty_cards_;
};

void RememberedSet::ClearCards() {
  CardTable* card_table = GetHeap()->GetCardTable();
  RememberedSetCardVisitor card_visitor(&dirty_cards_);
  // Clear dirty cards in the space and insert them into the dirty card set.
  card_table->ModifyCardsAtomic(space_->Begin(), space_->End(), AgeCardVisitor(), card_visitor);
}

class RememberedSetReferenceVisitor {
 public:
  RememberedSetReferenceVisitor(MarkObjectCallback* callback, space::ContinuousSpace* target_space,
                                void* arg)
      : callback_(callback), target_space_(target_space), arg_(arg),
        contains_reference_to_target_space_(false) {}

  void operator()(mirror::Object* obj, mirror::Object* ref,
                  const MemberOffset& offset, bool /* is_static */) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    if (ref != nullptr) {
      if (target_space_->HasAddress(ref)) {
        SetContainsReferenceToTargetSpace();
        mirror::Object* new_ref = callback_(ref, arg_);
        DCHECK(!target_space_->HasAddress(new_ref));
        if (new_ref != ref) {
          if (Runtime::Current()->IsActiveTransaction()) {
            obj->SetFieldObjectWithoutWriteBarrier<true>(offset, new_ref, false);
          } else {
            obj->SetFieldObjectWithoutWriteBarrier<false>(offset, new_ref, false);
          }
        }
      }
    }
  }

  bool ContainsReferenceToTargetSpace() const {
    return contains_reference_to_target_space_;
  }

  void SetContainsReferenceToTargetSpace() const {
    // Need a const_cast here as SpaceBitmap::VisitMarkedRange() and
    // MarkSweep::VisitObjectReferences() require a const visitor
    // (like a lambda).
    RememberedSetReferenceVisitor* non_const_this =
        const_cast<RememberedSetReferenceVisitor*>(this);
    non_const_this->contains_reference_to_target_space_ = true;
  }

 private:
  MarkObjectCallback* callback_;
  space::ContinuousSpace* target_space_;
  void* arg_;
  bool contains_reference_to_target_space_;
};

class RememberedSetObjectVisitor {
 public:
  RememberedSetObjectVisitor(MarkObjectCallback* callback, space::ContinuousSpace* target_space,
                             void* arg)
      : callback_(callback), target_space_(target_space), arg_(arg),
        contains_reference_to_target_space_(false) {}

  void operator()(mirror::Object* obj) const EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    DCHECK(obj != NULL);
    RememberedSetReferenceVisitor ref_visitor(callback_, target_space_, arg_);
    collector::MarkSweep::VisitObjectReferences(obj, ref_visitor, true);
    if (ref_visitor.ContainsReferenceToTargetSpace()) {
      SetContainsReferenceToTargetSpace();
    }
  }

  void ResetContainsReferenceToTargetSpace() {
    contains_reference_to_target_space_ = false;
  }

  bool ContainsReferenceToTargetSpace() {
    return contains_reference_to_target_space_;
  }

  void SetContainsReferenceToTargetSpace() const {
    // Need a const_cast here as SpaceBitmap::VisitMarkedRange() and
    // MarkSweep::VisitObjectReferences() require a const visitor
    // (like a lambda).
    RememberedSetObjectVisitor* non_const_this = const_cast<RememberedSetObjectVisitor*>(this);
    non_const_this->contains_reference_to_target_space_ = true;
  }

 private:
  MarkObjectCallback* callback_;
  space::ContinuousSpace* target_space_;
  void* arg_;
  bool contains_reference_to_target_space_;
};

void RememberedSet::UpdateAndMarkReferences(MarkObjectCallback* callback, space::ContinuousSpace* target_space,
                                            void* arg) {
  CardTable* card_table = heap_->GetCardTable();
  RememberedSetObjectVisitor obj_visitor(callback, target_space, arg);
  SpaceBitmap* bitmap = space_->GetLiveBitmap();
  CardSet remove_card_set;
  for (byte* const card_addr : dirty_cards_) {
    obj_visitor.ResetContainsReferenceToTargetSpace();
    uintptr_t start = reinterpret_cast<uintptr_t>(card_table->AddrFromCard(card_addr));
    DCHECK(space_->HasAddress(reinterpret_cast<mirror::Object*>(start)));
    bitmap->VisitMarkedRange(start, start + CardTable::kCardSize, obj_visitor);
    if (!obj_visitor.ContainsReferenceToTargetSpace()) {
      // It was in the dirty card set, but it didn't actually contain
      // a reference to the target space. So, remove it from the dirty
      // card set so we won't have to scan it again (unless it gets
      // dirty again.)
      remove_card_set.insert(card_addr);
    }
  }

  // Remove the cards that didn't contain a reference to the target
  // space from the dirty card set.
  for (byte* const card_addr : remove_card_set) {
    DCHECK(dirty_cards_.find(card_addr) != dirty_cards_.end());
    dirty_cards_.erase(card_addr);
  }
}

void RememberedSet::Dump(std::ostream& os) {
  CardTable* card_table = heap_->GetCardTable();
  os << "RememberedSet dirty cards: [";
  for (const byte* card_addr : dirty_cards_) {
    auto start = reinterpret_cast<uintptr_t>(card_table->AddrFromCard(card_addr));
    auto end = start + CardTable::kCardSize;
    os << reinterpret_cast<void*>(start) << "-" << reinterpret_cast<void*>(end) << "\n";
  }
  os << "]";
}

void RememberedSet::AssertAllDirtyCardsAreWithinSpace() const {
  CardTable* card_table = heap_->GetCardTable();
  for (const byte* card_addr : dirty_cards_) {
    auto start = reinterpret_cast<byte*>(card_table->AddrFromCard(card_addr));
    auto end = start + CardTable::kCardSize;
    DCHECK(space_->Begin() <= start && end <= space_->End());
  }
}

}  // namespace accounting
}  // namespace gc
}  // namespace art
