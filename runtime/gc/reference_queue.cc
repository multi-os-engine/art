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

#include "reference_queue.h"

#include "arena.h"
#include "accounting/card_table-inl.h"
#include "heap.h"
#include "mirror/class-inl.h"
#include "mirror/object-inl.h"

namespace art {
namespace gc {

ReferenceQueue::ReferenceQueue(Heap* heap)
    : lock_("reference queue lock"),
      heap_(heap),
      list_(nullptr) {
}

void ReferenceQueue::AtomicEnqueueIfNotEnqueued(Thread* self, mirror::Object* ref) {
  DCHECK(ref != NULL);
  MutexLock mu(self, lock_);
  if (!heap_->IsEnqueued(ref)) {
    EnqueuePendingReference(ref);
  }
}

void ReferenceQueue::EnqueueReference(mirror::Object* ref) {
  CHECK(heap_->IsEnqueuable(ref));
  EnqueuePendingReference(ref);
}

void ReferenceQueue::EnqueuePendingReference(mirror::Object* ref) {
  DCHECK(ref != NULL);
  MemberOffset pending_next_offset = heap_->GetReferencePendingNextOffset();
  DCHECK_NE(pending_next_offset.Uint32Value(), 0U);
  if (IsEmpty()) {
    // 1 element cyclic queue, ie: Reference ref = ..; ref.pendingNext = ref;
    ref->SetFieldObject(pending_next_offset, ref, false);
    list_ = ref;
  } else {
    mirror::Object* head =
        list_->GetFieldObject<mirror::Object*>(pending_next_offset, false);
    ref->SetFieldObject(pending_next_offset, head, false);
    list_->SetFieldObject(pending_next_offset, ref, false);
  }
}

mirror::Object* ReferenceQueue::DequeuePendingReference() {
  DCHECK(!IsEmpty());
  MemberOffset pending_next_offset = heap_->GetReferencePendingNextOffset();
  mirror::Object* head = list_->GetFieldObject<mirror::Object*>(pending_next_offset, false);
  DCHECK(head != nullptr);
  mirror::Object* ref;
  // Note: the following code is thread-safe because it is only called from ProcessReferences which
  // is single threaded.
  if (list_ == head) {
    ref = list_;
    list_ = nullptr;
  } else {
    mirror::Object* next = head->GetFieldObject<mirror::Object*>(pending_next_offset, false);
    list_->SetFieldObject(pending_next_offset, next, false);
    ref = head;
  }
  ref->SetFieldObject(pending_next_offset, nullptr, false);
  return ref;
}

void ReferenceQueue::Dump(std::ostream& os) const {
  mirror::Object* cur = list_;
  os << "Reference starting at list_=" << list_ << "\n";
  while (cur != nullptr) {
    mirror::Object* pending_next =
        cur->GetFieldObject<mirror::Object*>(heap_->GetReferencePendingNextOffset(), false);
    os << "PendingNext=" << pending_next;
    if (cur->GetClass()->IsFinalizerReferenceClass()) {
      os << " Zombie=" <<
          cur->GetFieldObject<mirror::Object*>(heap_->GetFinalizerReferenceZombieOffset(), false);
    }
    os << "\n";
    cur = pending_next;
  }
}

void ReferenceQueue::ClearWhiteReferences(ReferenceQueue& cleared_references, RootVisitor visitor,
                                          void* arg) {
  while (!IsEmpty()) {
    mirror::Object* ref = DequeuePendingReference();
    mirror::Object* referent = heap_->GetReferenceReferent(ref);
    if (referent != nullptr) {
      mirror::Object* forward_address = visitor(referent, arg);
      if (forward_address == nullptr) {
        // Referent is white, clear it.
        heap_->ClearReferenceReferent(ref);
        if (heap_->IsEnqueuable(ref)) {
          cleared_references.EnqueuePendingReference(ref);
        }
      } else if (referent != forward_address) {
        // Object moved, need to updated the referrent.
        heap_->SetReferenceReferent(ref, forward_address);
      }
    }
  }
}

void ReferenceQueue::EnqueueFinalizerReferences(ReferenceQueue& cleared_references,
                                                RootVisitor is_marked_callback,
                                                RootVisitor mark_callback, void* arg) {
  size_t count = 0, marked_count = 0;
  MemberOffset zombie_offset = heap_->GetFinalizerReferenceZombieOffset();
  while (!IsEmpty()) {
    mirror::Object* ref = DequeuePendingReference();
    mirror::Object* referent = heap_->GetReferenceReferent(ref);
    ++count;
    if (referent != nullptr) {
      mirror::Object* forward_address = is_marked_callback(referent, arg);
      // If the referent isn't marked, mark it and update the zombie field.
      if (forward_address == nullptr) {
        ++marked_count;
        forward_address = mark_callback(referent, arg);
        DCHECK(forward_address != nullptr);
        // Move the updated referent to the zombie field.
        ref->SetFieldObject(zombie_offset, forward_address, false);
        heap_->ClearReferenceReferent(ref);
        cleared_references.EnqueueReference(ref);
      } else if (referent != forward_address) {
        heap_->SetReferenceReferent(ref, forward_address);
      }
    }
  }
  LOG(INFO) << "Finalizer scan " << count << " cleared " << marked_count;
}

void ReferenceQueue::PreserveSomeSoftReferences(RootVisitor preserve_callback, void* arg) {
  ReferenceQueue cleared(heap_);
  while (!IsEmpty()) {
    mirror::Object* ref = DequeuePendingReference();
    mirror::Object* referent = heap_->GetReferenceReferent(ref);
    if (referent != nullptr) {
      mirror::Object* forward_address = preserve_callback(referent, arg);
      if (forward_address == nullptr) {
        // Either the reference isn't marked or we don't wish to preserve it.
        cleared.EnqueuePendingReference(ref);
      } else {
        heap_->SetReferenceReferent(ref, forward_address);
      }
    }
  }
  list_ = cleared.GetList();
}

size_t ReferenceQueue::GetLength() const {
  MemberOffset pending_next_offset = heap_->GetReferencePendingNextOffset();
  if (IsEmpty()) {
    return 0;
  }
  mirror::Object* head = list_->GetFieldObject<mirror::Object*>(pending_next_offset, false);
  mirror::Object* cur = head;
  size_t length = 0;
  while (true) {
    ++length;
    mirror::Object* next = cur->GetFieldObject<mirror::Object*>(pending_next_offset, false);
    if (next == head) {
      break;
    }
    cur = next;
  }
  return length;
}

void ReferenceBlockList::Init() {
  // TODO: This is gross.
  head_ = cur_ = AllocateBlock(kDefaultArenaSize);
}

void ReferenceBlockList::PushBackSlowPath(Thread* self, mirror::Object* ref) {
  MutexLock mu(self, lock_);
  // Keep updating the cur_ until we insert into it.
  while (!cur_->PushBack(ref)) {
    if (cur_->next_ == nullptr) {
      cur_->next_ = AllocateBlock(kDefaultArenaSize);
    }
    cur_ = cur_->next_;
  }
}

ReferenceBlock::ReferenceBlock(size_t capacity)
    : capacity_(capacity),
      pos_(0),
      next_(nullptr) {
}

ReferenceBlock* ReferenceBlock::Create(void* mem, size_t bytes) {
  CHECK_GT(bytes, sizeof(ReferenceBlock));
  bytes -= sizeof(ReferenceBlock);
  const size_t capacity = bytes / sizeof(mirror::Object*);
  return new(mem)ReferenceBlock(capacity);
}

ReferenceBlockList::ReferenceBlockList(Heap* heap)
    : lock_("reference block list lock"),
      heap_(heap) {
}

ReferenceBlockList::~ReferenceBlockList() {
}

ReferenceBlock* ReferenceBlockList::AllocateBlock(size_t bytes) {;
  void* storage = heap_->ArenaAllocate(bytes);
  if (UNLIKELY(storage == nullptr)) {
    return nullptr;
  }
  return ReferenceBlock::Create(storage, bytes);
}

void ReferenceBlockList::RemovedMarkedReferences(RootVisitor is_marked_callback, void* arg) {
  size_t remove_count = 0;
  for (mirror::Object*& ref : *this) {
    mirror::Object* referent = heap_->GetReferenceReferent(ref);
    if (LIKELY(referent != nullptr)) {
      // Referent is already marked, don't need to process this again later.
      mirror::Object* forward_address = is_marked_callback(referent, arg);
      if (forward_address != nullptr) {
        if (referent != forward_address) {
          // The referent moved, need to update it.
          heap_->UpdateReferenceReferent(ref, forward_address);
        }
        ref = nullptr;
        ++remove_count;
      }
    }
  }
  LOG(INFO) << "REMOVED " << remove_count;
}

void ReferenceBlockList::ProcessFinalizerReferences(ReferenceQueue& cleared_references,
                                                    RootVisitor is_marked_callback,
                                                    RootVisitor mark_callback,
                                                    void* arg) {
  MemberOffset zombie_offset = heap_->GetFinalizerReferenceZombieOffset();
  size_t null_count = 0;
  size_t forward_count = 0;
  size_t other_count = 0;
  for (mirror::Object*& ref : *this) {
    if (ref == nullptr) {
      continue;
    }
    mirror::Object* referent = heap_->GetReferenceReferent(ref);
    if (LIKELY(referent != nullptr)) {
      mirror::Object* forward_address = is_marked_callback(referent, arg);
      // If the referent isn't marked, mark it and update the zombie field.
      if (LIKELY(forward_address == nullptr)) {
        forward_address = mark_callback(referent, arg);
        DCHECK(forward_address != nullptr);
        ++forward_count;
        // Move the updated referent to the zombie field, you can use field ptr since the object
        // still points to the same references, just one of them is in a different field.
        ref->SetFieldPtr(zombie_offset, forward_address, false);
      } else {
        if (referent != forward_address) {
          // The referent moved, need to update it.
          heap_->UpdateReferenceReferent(ref, forward_address);
        }
        // Clear the element in the reference block list so EnqueueFinalizerReferences knows to
        // not enqueue it.
        ref = nullptr;
        ++other_count;
      }
    } else {
      ++null_count;
    }
  }
  LOG(INFO) << "null " << null_count << " forward " << forward_count << " other " << other_count << " = " << null_count + forward_count + other_count;
}

void ReferenceBlockList::EnqueueFinalizerReferences(ReferenceQueue& cleared_references) {
  for (mirror::Object*& ref : *this) {
    if (ref != nullptr) {
      heap_->ClearReferenceReferent(ref);
      cleared_references.EnqueueReference(ref);
    }
  }
  Clear();
}

void ReferenceBlockList::Clear() {
  head_ = cur_ = nullptr;
}

}  // namespace gc
}  // namespace art
