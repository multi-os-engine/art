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

#include "monitor_pool.h"

#include "base/logging.h"
#include "base/mutex-inl.h"
#include "thread-inl.h"
#include "monitor.h"

namespace art {

namespace mirror {
  class Object;
}  // namespace mirror

MonitorPool::MonitorPool() : allocated_ids_lock_("allocated monitor ids lock",
                                                 LockLevel::kMonitorPoolLock) {
  AllocateChunk();  // Get our first chunk.
}

// Assumes locks are held appropriately when necessary.
// We do not need a lock in the constructor, but we need one when in CreateMonitorInPool.
void MonitorPool::AllocateChunk() {
  void* chunk = malloc(kChunkSize);
  // Check we allocated memory.
  CHECK_NE(reinterpret_cast<uintptr_t>(nullptr), reinterpret_cast<uintptr_t>(chunk));
  // Check it is aligned as we need it.
  CHECK_EQ(0U, reinterpret_cast<uintptr_t>(chunk) % kMonitorAlignment);
  monitor_chunks_.push_back(reinterpret_cast<uintptr_t>(chunk));
}

Monitor* MonitorPool::CreateMonitorInPool(Thread* self, Thread* owner, mirror::Object* obj,
                                          int32_t hash_code)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  // We are gonna allocate, so acquire the writer lock.
  MutexLock mu(self, allocated_ids_lock_);

  // Scan the bitvector for the first free address
  size_t index = 0;

  auto begin = live_monitors_.begin() + index;
  auto end = live_monitors_.end();
  auto pos = std::find(begin, end, false);

  index = pos - begin;

  // Enough space, or need to resize?
  if (index >= monitor_chunks_.size() * kChunkCapacity) {
    AllocateChunk();
  }

  // Mark the space used by the monitor
  if (index == live_monitors_.size()) {
    // Extend the bitvector
    live_monitors_.push_back(true);
  } else {
    live_monitors_[index] = true;
  }
  // Construct the object
  // TODO: Exception on monitor construction. Is it worth it to catch and release the bit?
  size_t chunk_nr = index / kChunkCapacity;
  size_t chunk_index = index % kChunkCapacity;

  void* ptr = reinterpret_cast<void*>(monitor_chunks_[chunk_nr] + chunk_index * kAlignedMonitorSize);
  Monitor* monitor = new(ptr) Monitor(self, owner, obj, hash_code);

  return monitor;
}

void MonitorPool::ReleaseMonitorToPool(Thread* self, Monitor* monitor) {
  // Might be racy with allocation, so acquire lock.
  MutexLock mu(self, allocated_ids_lock_);

  MonitorId id = monitor->GetMonitorId();

  // Call the destructor.
  // TODO: Exception safety?
  monitor->~Monitor();

  // Compute which index.
  size_t index = MonitorIdToOffset(id) / kAlignedMonitorSize;
  DCHECK_LT(index, live_monitors_.size());
  live_monitors_[index] = false;
}

void MonitorPool::ReleaseMonitorsToPool(Thread* self, std::list<Monitor*>* monitors) {
  for (Monitor* mon : *monitors) {
    ReleaseMonitorToPool(self, mon);
  }
}

}  // namespace art
