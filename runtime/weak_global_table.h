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

#ifndef ART_RUNTIME_WEAK_GLOBAL_TABLE_H_
#define ART_RUNTIME_WEAK_GLOBAL_TABLE_H_

// TODO: Use unordered map when we use libcxx instead of STLport.

#include "handle_table.h"
#include "object_callbacks.h"
#include "safe_map.h"

namespace art {
namespace mirror {
class Object;
}  // namespace mirror

class WeakGlobalTable : public HandleTable {
 public:
  WeakGlobalTable(const char* name, size_t capacity, mirror::Object* clear_value);
  ~WeakGlobalTable() {
  }

  Reference SynchronizedAdd(Thread* self, mirror::Object* obj)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) LOCKS_EXCLUDED(lock_);
  Reference SynchronizedAddUnique(Thread* self, mirror::Object* obj)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) LOCKS_EXCLUDED(lock_);

  void EnableSlowPath(Thread* self)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::mutator_lock_);
  void SetSweepArgs(IsMarkedCallback* is_marked_callback, void* arg)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::mutator_lock_);
  void Sweep() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) LOCKS_EXCLUDED(lock_, sweeping_lock_);
  mirror::Object* Decode(Thread* self, Reference ref) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 private:
  // Slow path decode, acquires the lock.
  mirror::Object* DecodeSlowPath(Thread* self, Reference ref)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) LOCKS_EXCLUDED(sweeping_lock_);

  // Map for add unique.
  SafeMap<mirror::Object*, Reference> dedupe_map_ GUARDED_BY(lock_);
  // Sweeping lock.
  Mutex sweeping_lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  // The value that cleared globals are set to.
  mirror::Object* const cleared_weak_global_value_;
  // If the slow path is enabled it means we are about to sweep the weak globals, in this case we
  // must acquire a lock and check that the object is marked.
  volatile bool slow_path_enabled_;
  // If we are sweeping, we can still return objects as long as they are marked.
  IsMarkedCallback* is_marked_callback_;
  // Current arg associated with the callback.
  void* arg_;
};

}  // namespace art

#endif  // ART_RUNTIME_WEAK_GLOBAL_TABLE_H_
