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

#ifndef ART_RUNTIME_MONITOR_POOL_H_
#define ART_RUNTIME_MONITOR_POOL_H_

#include "monitor.h"

#ifdef __LP64__
#include <stdint.h>
#include "runtime.h"
#else
#include "base/stl_util.h"     // STLDeleteElements
#endif

namespace art {

// Abstraction to keep monitors small enough to fit in a lock word (32bits). On 32bit systems the
// monitor id loses the alignment bits of the Monitor*.
class MonitorPool {
 public:
  static MonitorPool* Create() {
#ifndef __LP64__
    return nullptr;
#else
    return new MonitorPool();
#endif
  }

  static Monitor* CreateMonitor(Thread* self, Thread* owner, mirror::Object* obj, int32_t hash_code)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
#ifndef __LP64__
    return new Monitor(self, owner, obj, hash_code);
#else
    return GetMonitorPool()->CreateMonitorInPool(self, owner, obj, hash_code);
#endif
  }

  static void ReleaseMonitor(Thread* self, Monitor* monitor) {
#ifndef __LP64__
    delete monitor;
#else
    GetMonitorPool()->ReleaseMonitorToPool(self, monitor);
#endif
  }

  static void ReleaseMonitors(Thread* self, std::list<Monitor*>* monitors) {
#ifndef __LP64__
    STLDeleteElements(monitors);
#else
    GetMonitorPool()->ReleaseMonitorsToPool(self, monitors);
#endif
  }

  static Monitor* MonitorFromMonitorId(MonitorId mon_id) {
#ifndef __LP64__
    return reinterpret_cast<Monitor*>(mon_id << 3);
#else
    return GetMonitorPool()->LookupMonitor(mon_id);
#endif
  }

  static MonitorId MonitorIdFromMonitor(Monitor* mon) {
#ifndef __LP64__
    return reinterpret_cast<MonitorId>(mon) >> 3;
#else
    return mon->GetMonitorId();
#endif
  }

  static MonitorId ComputeMonitorId(Monitor* mon) {
#ifndef __LP64__
    return MonitorIdFromMonitor(mon);
#else
    return GetMonitorPool()->ComputeMonitorIdInPool(mon);
#endif
  }

  static MonitorPool* GetMonitorPool() {
#ifndef __LP64__
    return nullptr;
#else
    return Runtime::Current()->GetMonitorPool();
#endif
  }

 private:
#ifdef __LP64__
  MonitorPool();

  void AllocateChunk();

  Monitor* CreateMonitorInPool(Thread* self, Thread* owner, mirror::Object* obj, int32_t hash_code)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void Resize(Thread* self) SHARED_LOCKS_REQUIRED(allocated_ids_lock_, Locks::mutator_lock_);

  void ReleaseMonitorToPool(Thread* self, Monitor* monitor);
  void ReleaseMonitorsToPool(Thread* self, std::list<Monitor*>* monitors);

  // Note: This is safe as we do not ever move chunks.
  Monitor* LookupMonitor(MonitorId mon_id) {
    size_t offset = MonitorIdToOffset(mon_id);
    size_t index = offset / kChunkSize;
    DCHECK_LT(index, monitor_chunks_.size());
    size_t offset_in_chunk = offset % kChunkSize;
    return reinterpret_cast<Monitor*>(monitor_chunks_[index] + offset_in_chunk);
  }

  static bool IsInChunk(uintptr_t base_addr, Monitor* mon) {
    uintptr_t mon_ptr = reinterpret_cast<uintptr_t>(mon);
    return base_addr <= mon_ptr && (mon_ptr - base_addr < kChunkSize);
  }

  // Note: This is safe as we do not ever move chunks.
  MonitorId ComputeMonitorIdInPool(Monitor* mon) {
    for (size_t index = 0; index < monitor_chunks_.size(); ++index) {
      uintptr_t chunk_addr = monitor_chunks_[index];
      if (IsInChunk(chunk_addr, mon)) {
        return OffsetToMonitorId(reinterpret_cast<uintptr_t>(mon) - chunk_addr + index * kChunkSize);
      }
    }
    LOG(FATAL) << "Did not find chunk that contains monitor.";
    return 0;
  }

  static size_t MonitorIdToOffset(MonitorId id) {
    return id << 3;
  }

  static MonitorId OffsetToMonitorId(size_t offset) {
    return static_cast<MonitorId>(offset >> 3);
  }

  Mutex allocated_ids_lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;

  // TODO: There are assumptions in the code that monitor addresses are 8B aligned (>>3).
  static constexpr size_t kMonitorAlignment = 8;
  // Size of a monitor, rounded up to a multiple of alignment.
  static constexpr size_t kAlignedMonitorSize = ((sizeof(Monitor) + kMonitorAlignment - 1) /
      kMonitorAlignment) * kMonitorAlignment;
  // As close to a page as we can get seems a good start.
  static constexpr size_t kChunkCapacity = kPageSize / kAlignedMonitorSize;
  static constexpr size_t kChunkSize = kChunkCapacity * kAlignedMonitorSize;

  // List of memory chunks. Each chunk is kChunkSize.
  std::vector<uintptr_t> monitor_chunks_;

  // Live bitmap.
  // Note: Does not need to have the size of the memory chunk. Anything above the size is not alive.
  // Note: We use just one vector, but could keep track of whether something's free in a chunk or
  // not, so we could move past chunks quicker.
  std::vector<bool> live_monitors_;
#endif
};

}  // namespace art

#endif  // ART_RUNTIME_MONITOR_POOL_H_
