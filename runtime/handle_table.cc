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

#include "handle_table.h"

namespace art {

HandleTable::HandleTable(const char* name, size_t capacity)
    : name_(name), lock_(name), capacity_(capacity), top_index_(0) {
  std::string error_msg;
  mem_map_.reset(MemMap::MapAnonymous(name, nullptr, capacity * sizeof(Slot),
                                      PROT_READ | PROT_WRITE, false, &error_msg));
  CHECK(mem_map_.get() != nullptr) << error_msg;
  slots_ = reinterpret_cast<Slot*>(mem_map_->Begin());
  num_slots_ = mem_map_->Size() / sizeof(Slot);
}

HandleTable::Reference HandleTable::SynchronizedAdd(Thread* self, mirror::Object* obj) {
  MutexLock mu(self, lock_);
  return Add(obj);
}

void HandleTable::SynchronizedRemove(Thread* self, Reference ref) {
  MutexLock mu(self, lock_);
  Remove(ref);
}

HandleTable::Reference HandleTable::Add(mirror::Object* obj) {
  DCHECK(obj != nullptr);
  size_t idx;
  // Find an available slot, recycle if we have something in the free stack.
  if (!free_stack_.empty()) {
    idx = free_stack_.back();
    free_stack_.pop_back();
  } else {
    if (UNLIKELY(top_index_ >= capacity_)) {
      // Table is full, can't increase the top index.
      return nullptr;
    }
    idx = top_index_++;
  }
  CHECK(slots_[idx].IsFree(nullptr));
  return slots_[idx].Add(obj);
}

void HandleTable::Remove(Reference ref) {
  // Find the slot index.
  CHECK(Contains(ref));
  const size_t slot_idx =
      (reinterpret_cast<uintptr_t>(ref) - reinterpret_cast<uintptr_t>(slots_)) / sizeof(Slot);
  slots_[slot_idx].Delete(ref, nullptr);
  free_stack_.push_back(slot_idx);
  // We don't decrease the top_index since this is hard due to having a free stack.
}

HandleTable::~HandleTable() {
}

bool HandleTable::Slot::IsFree(const mirror::Object* free_value) const {
  // Check that the top reference is the free value.
  return GetTopReference()->AsMirrorPtr() == free_value;
}

HandleTable::Reference HandleTable::Slot::Add(mirror::Object* ref) {
  // Caller should check that that the slot is free by calling IsFree.
  ++free_index_;
  Reference top_reference(GetTopReference());
  top_reference->Assign(ref);
  return top_reference;
}

void HandleTable::Slot::Delete(Reference ref, mirror::Object* free_value) {
  DCHECK(Contains(ref));
  Reference top_reference(GetTopReference());
  CHECK_EQ(ref, top_reference);  // Make sure that we aren't deleting an already stale reference.
  top_reference->Assign(free_value);
}

}  // namespace art
