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

#ifndef ART_RUNTIME_GC_ACCOUNTING_REMEMBERED_SET_H_
#define ART_RUNTIME_GC_ACCOUNTING_REMEMBERED_SET_H_

#include "bitmap.h"
#include "base/allocator.h"
#include "card_table.h"
#include "globals.h"
#include "object_callbacks.h"
#include "safe_map.h"

#include <set>
#include <vector>

namespace art {
namespace gc {

namespace collector {
  class MarkSweep;
}  // namespace collector
namespace space {
  class ContinuousSpace;
}  // namespace space

class Heap;

namespace accounting {

class Bitmap;
// The remembered set keeps track of cards that may contain references
// from the free list spaces to the bump pointer spaces.
class RememberedSet {
 public:
  typedef MemoryRangeBitmap<CardTable::kCardSize> CardBitmap;

  explicit RememberedSet(const std::string& name, Heap* heap, space::ContinuousSpace* space);

  // Clear dirty cards and add them to the dirty card set.
  void ClearCards();

  // Mark through all references to the target space.
  void UpdateAndMarkReferences(MarkHeapReferenceCallback* callback,
                               DelayReferenceReferentCallback* ref_callback,
                               space::ContinuousSpace* target_space, void* arg)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::heap_bitmap_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Returns true if a card is marked inside the remembered set. Used for testing. The address
  // doesn't need to be aligned.
  bool ContainsCardFor(uintptr_t addr);

  // Sets all the cards in the remembered set to be marked.
  void SetCards();

  void Dump(std::ostream& os);

  space::ContinuousSpace* GetSpace() {
    return space_;
  }
  Heap* GetHeap() const {
    return heap_;
  }
  const std::string& GetName() const {
    return name_;
  }

 private:
  const std::string name_;
  Heap* const heap_;
  space::ContinuousSpace* const space_;

 protected:
  // Cleared card bitmap, used to update the remembered set.
  std::unique_ptr<CardBitmap> card_bitmap_;
};

}  // namespace accounting
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_ACCOUNTING_REMEMBERED_SET_H_
