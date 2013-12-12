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

#ifndef ART_RUNTIME_ARENA_H_
#define ART_RUNTIME_ARENA_H_

#include <set>
#include <stdint.h>
#include <stddef.h>

#include "atomic_integer.h"
#include "base/mutex.h"
#include "mem_map.h"
#include "utils.h"

namespace art {

class Arena;
class ArenaPool;

class Arena {
 public:
  static constexpr size_t kDefaultSize = 128 * KB;
  explicit Arena(size_t size = kDefaultSize);
  ~Arena();
  void Reset();
  uint8_t* Begin() {
    return memory_;
  }
  uint8_t* End() {
    return memory_ + size_;
  }
  size_t Size() const {
    return size_;
  }
  size_t RemainingSpace() const {
    return Size() - bytes_allocated_;
  }
  void SetBytesAllocated(size_t bytes) {
    DCHECK_LE(bytes, size_);
    bytes_allocated_ = bytes;
  }
  Arena* GetNext() {
    return next_;
  }
  void SetNext(Arena* next) {
    next_ = next;
  }

 private:
  size_t bytes_allocated_;
  uint8_t* memory_;
  size_t size_;
  MemMap* map_;
  Arena* next_;
  friend class ArenaPool;
  DISALLOW_COPY_AND_ASSIGN(Arena);
};

class ArenaPool {
 public:
  ArenaPool();
  ~ArenaPool();
  Arena* AllocArena(size_t size);
  void FreeArena(Arena* arena) LOCKS_EXCLUDED(lock_);
  void FreeAllArenas() LOCKS_EXCLUDED(lock_);

 private:
  void AddFreeArena(Arena* arena) EXCLUSIVE_LOCKS_REQUIRED(lock_);

  Mutex lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  std::set<Arena*> used_arenas_ GUARDED_BY(lock_);
  Arena* free_arenas_ GUARDED_BY(lock_);
  DISALLOW_COPY_AND_ASSIGN(ArenaPool);
};

class AtomicArenaAllocator {
 public:
  static const size_t kDefaultArenaSize = 4 * MB;

  explicit AtomicArenaAllocator();
  void Init(ArenaPool* pool);
  void Invalidate();
  template <const size_t kAlignment = 8>
  ALWAYS_INLINE void* Alloc(size_t bytes) {
    bytes = RoundUp(bytes, kAlignment);
    void* ptr = cur_->Alloc(bytes);
    if (LIKELY(ptr != nullptr)) {
      return ptr;
    }
    return AllocSlowPath(bytes);
  }

 private:
  class ArenaRegion {
   public:
    ArenaRegion(Arena* arena, size_t capacity);
    static ArenaRegion* Create(Arena* arena);

    ALWAYS_INLINE void* Alloc(size_t bytes) {
      // This if statement is unnecessary but helps to prevent integer overflows.
      if (UNLIKELY(bytes >= capacity_)) {
        return nullptr;
      }
      // TODO: Investigate the chance of integer overflow?
      size_t old_pos = static_cast<size_t>(pos_.FetchAndAdd(bytes));
      if (UNLIKELY(old_pos + bytes >= capacity_)) {
        return nullptr;
      }
      return reinterpret_cast<void*>(&storage_[old_pos]);
    }

   private:
    // Which arena we are exclusively allocating into.
    Arena* arena_;
    // Capacity of the region.
    size_t capacity_;
    // The position inside the block where we are.
    AtomicInteger pos_;
    // Next arena in the list.
    ArenaRegion* next_;
    // Storage is where the
    byte storage_[0];

    friend class AtomicArenaAllocator;
  };
  // Takes in an aligned size.
  void* AllocSlowPath(size_t bytes);

  // The head is where the linked list starts.
  Mutex lock_;
  ArenaPool* arena_pool_;
  ArenaRegion* head_;
  ArenaRegion* cur_;
};

}  // namespace art

#endif  // ART_RUNTIME_ARENA_H_
