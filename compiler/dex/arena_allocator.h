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

#ifndef ART_COMPILER_DEX_ARENA_ALLOCATOR_H_
#define ART_COMPILER_DEX_ARENA_ALLOCATOR_H_

#include <stdint.h>
#include <stddef.h>

#include "arena.h"
#include "compiler_enums.h"

namespace art {

class Arena;
class ArenaPool;

class ArenaAllocator {
 public:
  // Type of allocation for memory tuning.
  enum ArenaAllocKind {
    kAllocMisc,
    kAllocBB,
    kAllocLIR,
    kAllocMIR,
    kAllocDFInfo,
    kAllocGrowableArray,
    kAllocGrowableBitMap,
    kAllocDalvikToSSAMap,
    kAllocDebugInfo,
    kAllocSuccessor,
    kAllocRegAlloc,
    kAllocData,
    kAllocPredecessors,
    kNumAllocKinds
  };

  static constexpr bool kCountAllocations = false;

  explicit ArenaAllocator(ArenaPool* pool);
  ~ArenaAllocator();

  // Returns zeroed memory.
  void* Alloc(size_t bytes, ArenaAllocKind kind) ALWAYS_INLINE {
    if (UNLIKELY(running_on_valgrind_)) {
      return AllocValgrind(bytes, kind);
    }
    bytes = (bytes + 3) & ~3;
    if (UNLIKELY(ptr_ + bytes > end_)) {
      // Obtain a new block.
      ObtainNewArenaForAllocation(bytes);
      if (UNLIKELY(ptr_ == nullptr)) {
        return nullptr;
      }
    }
    if (kCountAllocations) {
      alloc_stats_[kind] += bytes;
      ++num_allocations_;
    }
    uint8_t* ret = ptr_;
    ptr_ += bytes;
    return ret;
  }

  void* AllocValgrind(size_t bytes, ArenaAllocKind kind);
  void ObtainNewArenaForAllocation(size_t allocation_size);
  size_t BytesAllocated() const;
  void DumpMemStats(std::ostream& os) const;

 private:
  void UpdateBytesAllocated();

  ArenaPool* pool_;
  uint8_t* begin_;
  uint8_t* end_;
  uint8_t* ptr_;
  Arena* arena_head_;
  size_t num_allocations_;
  size_t alloc_stats_[kNumAllocKinds];  // Bytes used by various allocation kinds.
  bool running_on_valgrind_;

  DISALLOW_COPY_AND_ASSIGN(ArenaAllocator);
};  // ArenaAllocator

struct MemStats {
   public:
     void Dump(std::ostream& os) const {
       arena_.DumpMemStats(os);
     }
     explicit MemStats(const ArenaAllocator &arena) : arena_(arena) {}
  private:
    const ArenaAllocator &arena_;
};  // MemStats

}  // namespace art

#endif  // ART_COMPILER_DEX_ARENA_ALLOCATOR_H_
