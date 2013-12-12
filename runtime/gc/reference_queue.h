/*
 * Copyright (C) 2013 The Android Open Source Project
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

#ifndef ART_RUNTIME_GC_REFERENCE_QUEUE_H_
#define ART_RUNTIME_GC_REFERENCE_QUEUE_H_

#include <iosfwd>
#include <string>
#include <vector>

#include "atomic_integer.h"
#include "base/timing_logger.h"
#include "globals.h"
#include "gtest/gtest.h"
#include "jni.h"
#include "locks.h"
#include "offsets.h"
#include "root_visitor.h"
#include "thread_pool.h"

namespace art {

class Arena;

namespace gc {

class Heap;

// Used to temporarily store java.lang.ref.Reference(s) during GC and prior to queueing on the
// appropriate java.lang.ref.ReferenceQueue. The linked list is maintained in the
// java.lang.ref.Reference objects.
class ReferenceQueue {
 public:
  explicit ReferenceQueue(Heap* heap);
  // Enqueue a reference if is not already enqueued. Thread safe to call from multiple threads
  // since it uses a lock to avoid a race between checking for the references presence and adding
  // it.
  void AtomicEnqueueIfNotEnqueued(Thread* self, mirror::Object* ref)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) LOCKS_EXCLUDED(lock_);
  // Enqueue a reference, unlike EnqueuePendingReference, enqueue reference checks that the
  // reference IsEnqueueable. Not thread safe, used when mutators are paused to minimize lock
  // overhead.
  void EnqueueReference(mirror::Object* ref) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void EnqueuePendingReference(mirror::Object* ref) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  mirror::Object* DequeuePendingReference() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  // Enqueues finalizer references with white referents.  White referents are blackened, moved to the
  // zombie field, and the referent field is cleared.
  void EnqueueFinalizerReferences(ReferenceQueue& cleared_references,
                                  RootVisitor is_marked_callback,
                                  RootVisitor mark_callback, void* arg)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  // Walks the reference list marking any references subject to the reference clearing policy.
  // References with a black referent are removed from the list.  References with white referents
  // biased toward saving are blackened and also removed from the list.
  void PreserveSomeSoftReferences(RootVisitor preserve_callback, void* arg)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  // Unlink the reference list clearing references objects with white referents.  Cleared references
  // registered to a reference queue are scheduled for appending by the heap worker thread.
  void ClearWhiteReferences(ReferenceQueue& cleared_references, RootVisitor visitor, void* arg)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void Dump(std::ostream& os) const
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  bool IsEmpty() const {
    return list_ == nullptr;
  }
  void Clear() {
    list_ = nullptr;
  }
  mirror::Object* GetList() {
    return list_;
  }
  size_t GetLength() const;

 private:
  // Lock, used for parallel GC reference enqueuing. It allows for multiple threads simultaneously
  // calling AtomicEnqueueIfNotEnqueued.
  Mutex lock_;
  // The heap contains the reference offsets.
  Heap* const heap_;
  // The actual reference list. Not a root since it will be nullptr when the GC is not running.
  mirror::Object* list_;
};

class ReferenceBlock {
 public:
  ALWAYS_INLINE bool PushBack(mirror::Object* ref) {
    // Integer overflow shouldn't be able to happen as this would require around max int number
    // of threads.
    size_t old_pos = static_cast<size_t>(pos_.FetchAndAdd(1));
    if (LIKELY(old_pos < capacity_)) {
      references_[old_pos] = ref;
      return true;
    }
    return false;
  }
  explicit ReferenceBlock(size_t capacity);
  static ReferenceBlock* Create(void* mem, size_t bytes);
  size_t Capacity() const {
    return capacity_;
  }
  mirror::Object*& operator[](size_t index) {
    DCHECK_LT(index, capacity_);
    return references_[index];
  }
  ReferenceBlock* GetNext() {
    return next_;
  }
  size_t GetSize() const {
    return pos_;
  }

 private:
  size_t capacity_;
  AtomicInteger pos_;  // TODO: Use atomic pointer instead?
  ReferenceBlock* next_;
  mirror::Object* references_[0];
  friend class ReferenceBlockList;
};

// TODO: Investigate using this to replace atomic stack?
class ReferenceBlockList {
 public:
  class Iterator {
   public:
    Iterator(ReferenceBlock* block, size_t pos = 0) : block_(block), pos_(pos) {
    }
    // Iterator currently doesn't support decrement.
    void operator++() {
      ++pos_;
      if (LIKELY(pos_ >= block_->Capacity())) {
        block_ = block_->GetNext();
        pos_ = 0;
      }
    }
    mirror::Object*& operator*() {
      DCHECK(block_ != nullptr);
      return (*block_)[pos_];
    }
    bool operator==(const Iterator& it) const {
      return Compare(it) == 0;
    }
    bool operator!=(const Iterator& it) const {
      return Compare(it) != 0;
    }

   private:
    int Compare(const Iterator& it) const {
      if (block_ < it.block_) {
        return -1;
      } else if (block_ > it.block_) {
        return 1;
      }
      if (pos_ < it.pos_) {
        return -1;
      } else if (pos_ > it.pos_) {
        return 1;
      }
      return 0;
    }

    ReferenceBlock* block_;
    size_t pos_;
  };

  // Lower case to allow ranged base loops.
  // TODO: Worth adding const variants?
  Iterator begin() {
    if (head_ == nullptr || head_->GetSize() == 0) {
      return end();
    } else {
      return Iterator(head_, 0);
    }
  }
  Iterator end() {
    if (cur_ == nullptr || cur_->GetSize() >= cur_->Capacity()) {
      return Iterator(nullptr, 0);
    } else {
      return Iterator(cur_, cur_->GetSize());
    }
  }
  bool IsEmpty() {
    return begin() == end();
  }

  void Init();
  explicit ReferenceBlockList(Heap* heap);
  virtual ~ReferenceBlockList();
  // Adds a reference to the reference block list. Thread safe.
  ALWAYS_INLINE void PushBack(Thread* self, mirror::Object* ref) {
    if (UNLIKELY(!cur_->PushBack(ref))) {
      // Block is full, need to go slow path and use a lock to prevent race conditions.
      PushBackSlowPath(self, ref);
    }
  }
  void PushBackSlowPath(Thread* self, mirror::Object* ref);
  ReferenceBlock* AllocateBlock(size_t bytes);
  // Process the finalizer references, defers enqueueing to EnqueueFinalizerReferences which can be
  // done with mutators unpaused.
  void ProcessFinalizerReferences(ReferenceQueue& cleared_references,
                                  RootVisitor is_marked_callback,
                                  RootVisitor mark_callback, void* arg)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void EnqueueFinalizerReferences(ReferenceQueue& cleared_reference)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  // Filtering the references nulls elements in the reference list which are marked (since these
  // are not interesting).
  void RemovedMarkedReferences(RootVisitor is_marked_callback, void* arg)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void Clear() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 private:
  static constexpr size_t kDefaultArenaSize = 512 * KB;

  Mutex lock_;
  Heap* const heap_;
  ReferenceBlock* head_;
  ReferenceBlock* cur_;
};

}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_REFERENCE_QUEUE_H_
