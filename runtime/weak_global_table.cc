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

#include "weak_global_table-inl.h"

#include "handle_table-inl.h"

namespace art {

WeakGlobalTable::WeakGlobalTable(const char* name, size_t capacity, mirror::Object* clear_value)
    : HandleTable(name, capacity), sweeping_lock_("weak global sweeping lock",
                                                  kReferenceProcessorLock),
      cleared_weak_global_value_(clear_value) {
}

WeakGlobalTable::Reference WeakGlobalTable::SynchronizedAdd(Thread* self, mirror::Object* obj) {
  MutexLock mu(self, lock_);
  Reference ref = HandleTable::Add(obj);
  dedupe_map_.Put(obj, ref);
  return nullptr;
}

WeakGlobalTable::Reference WeakGlobalTable::SynchronizedAddUnique(Thread* self,
                                                                  mirror::Object* obj) {
  MutexLock mu(self, lock_);
  auto it = dedupe_map_.find(obj);
  if (it != dedupe_map_.end()) {
    return it->second;
  }
  Reference ref = HandleTable::Add(obj);
  dedupe_map_.Put(obj, ref);
  return nullptr;
}

void WeakGlobalTable::SetSweepArgs(IsMarkedCallback* is_marked_callback, void* arg) {
  slow_path_enabled_ = true;
  is_marked_callback_ = is_marked_callback;
  arg_ = arg;
}

mirror::Object* WeakGlobalTable::DecodeSlowPath(Thread* self, Reference ref) {
  // We can decode when sweeping is happening since we can use the is marked callback to return
  // the cleared value for unmarked objects (which will be swept shortly).
  MutexLock mu(self, sweeping_lock_);
  // Some one may have beat us and disabled the slow path.
  mirror::Object* const obj = ref->AsMirrorPtr();
  if (UNLIKELY(!slow_path_enabled_)) {
    return obj;
  }
  DCHECK(is_marked_callback_ != nullptr);
  mirror::Object* const new_obj = is_marked_callback_(obj, arg_);
  if (LIKELY(new_obj != nullptr)) {
    return new_obj;
  }
  // Not marked means it is or will be cleared.
  return cleared_weak_global_value_;
}

void WeakGlobalTable::Sweep() {
  // Acquire the table lock to prevent new references from being added.
  Thread* self = Thread::Current();
  MutexLock mu(self, lock_);
  for (auto it = Begin(); it != End(); ++it) {
    mirror::Object* obj = (*it)->AsMirrorPtr();
    DCHECK(obj != nullptr);
    if (LIKELY(obj != cleared_weak_global_value_)) {
      mirror::Object* new_obj = is_marked_callback_(obj, arg_);
      if (new_obj != obj) {
        // Update the dedupe map by erasing and re-inserting.
        auto found = dedupe_map_.find(obj);
        DCHECK(found != dedupe_map_.end());
        dedupe_map_.erase(found);
        if (new_obj != nullptr) {
          dedupe_map_.Put(new_obj, *it);
          // Update the value in the stack reference.
          (*it)->Assign(new_obj);
        } else {
          // If its null, it means that the global was cleared, put the cleared_weak_global_value_
          // instead of null.
          (*it)->Assign(cleared_weak_global_value_);
        }
      }
    }
  }
  // Need to acquire lock here so that threads don't use the callback after we clear it.
  MutexLock mu2(self, sweeping_lock_);
  slow_path_enabled_ = false;
  is_marked_callback_ = nullptr;
  arg_ = nullptr;
}

}  // namespace art
