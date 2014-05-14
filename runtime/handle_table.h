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

#ifndef ART_RUNTIME_HANDLE_TABLE_H_
#define ART_RUNTIME_HANDLE_TABLE_H_

#include "base/mutex.h"
#include "mem_map.h"
#include "stack.h"

namespace art {
namespace mirror {
class Object;
}  // namespace mirror

class HandleTable {
 public:
  typedef StackReference<mirror::Object>* Reference;
  typedef const StackReference<mirror::Object>* ConstReference;

  // We want cleared/deleted references to stay invalid for a bit after they are cleared before
  // being reused for a new object. TODO: Make this private when Iterator dependency is figured out.
  class Slot {
   public:
    Slot() : free_index_(0) {
    }
    bool Contains(Reference ref) const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      return ref >= &references_[0] && ref < &references_[kNumIndex];
    }
    bool IsFree(const mirror::Object* free_value) const
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
    Reference Add(mirror::Object* obj) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
    void Delete(Reference ref, mirror::Object* free_value)
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
    // Returns the top (active) reference, this is the one that is cleared when you call delete.
    ConstReference GetTopReference() const {
      return &references_[free_index_ % kNumIndex];
    }
    Reference GetTopReference() {
      return &references_[free_index_ % kNumIndex];
    }

   private:
    static constexpr size_t kNumIndex = 4;
    // Cyclic index which loops from 0 to kNumIndex - 1.
    size_t free_index_;
    StackReference<mirror::Object> references_[kNumIndex];
  };

  class Iterator {
   public:
    Iterator(Slot* slot, Slot* limit);
    Iterator& operator++() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
    Reference operator*() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
    intptr_t Compare(const Iterator& rhs) const;

   private:
    void SkipNulls() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
    Slot* slot_;
    Slot* const limit_;
  };

  HandleTable(const char* name, size_t capacity);
  ~HandleTable();
  // Thread safe add / remove, uses a lock.
  Reference SynchronizedAdd(Thread* self, mirror::Object* obj)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) LOCKS_EXCLUDED(lock_);
  void SynchronizedRemove(Thread* self, Reference ref)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) LOCKS_EXCLUDED(lock_);
  // Thread unsafe add / remove.
  Reference Add(mirror::Object* obj) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void Remove(Reference ref) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  bool Contains(ConstReference ref) const {
    return ref >= reinterpret_cast<ConstReference>(slots_) &&
        ref < reinterpret_cast<ConstReference>(slots_ + top_index_);
  }
  Iterator Begin() SHARED_LOCKS_REQUIRED(lock_);
  Iterator End() SHARED_LOCKS_REQUIRED(lock_);

 protected:
  std::string name_;
  // Lock that guards SynchronizedAdd / SynchronizedRemove
  Mutex lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  const size_t capacity_;  // Maximum number of handles which may be contained by the table.
  size_t top_index_;  // Highest slot index which may contain a valid handle.
  UniquePtr<MemMap> mem_map_;
  size_t num_slots_;  // Current size of the table, increases as the table grows.
  Slot* slots_;
  // Free indices into the slot table,
  std::vector<size_t> free_stack_;
};

bool operator==(const HandleTable::Iterator& lhs, const HandleTable::Iterator& rhs);
bool operator!=(const HandleTable::Iterator& lhs, const HandleTable::Iterator& rhs);

}  // namespace art

#endif  // ART_RUNTIME_HANDLE_TABLE_H_
