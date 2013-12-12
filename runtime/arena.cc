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

#include "arena.h"

#include "thread-inl.h"
#include <memcheck/memcheck.h>

namespace art {

// Memmap is a bit slower than malloc according to my measurements.
static constexpr bool kUseMemMap = true;
static constexpr bool kUseMemSet = true && kUseMemMap;
constexpr size_t Arena::kDefaultSize;

Arena::Arena(size_t size)
    : bytes_allocated_(0),
      map_(nullptr),
      next_(nullptr) {
  if (kUseMemMap) {
    std::string error_msg;
    map_ = MemMap::MapAnonymous("arena", NULL, size, PROT_READ | PROT_WRITE, &error_msg);
    CHECK(map_ != nullptr) << error_msg;
    memory_ = map_->Begin();
    size_ = map_->Size();
  } else {
    memory_ = reinterpret_cast<uint8_t*>(calloc(1, size));
    size_ = size;
  }
  LOG(ERROR) << "Allocating new arena with size " << size;
}

Arena::~Arena() {
  if (kUseMemMap) {
    delete map_;
  } else {
    free(reinterpret_cast<void*>(memory_));
  }
}

void Arena::Reset() {
  if (bytes_allocated_) {
    if (kUseMemSet || !kUseMemMap) {
      memset(Begin(), 0, bytes_allocated_);
    } else {
      madvise(Begin(), bytes_allocated_, MADV_DONTNEED);
    }
    bytes_allocated_ = 0;
  }
}

ArenaPool::ArenaPool()
    : lock_("Arena pool lock", kAllocatorLock),
      free_arenas_(nullptr) {
}

ArenaPool::~ArenaPool() {
  while (free_arenas_ != nullptr) {
    auto* arena = free_arenas_;
    free_arenas_ = free_arenas_->next_;
    delete arena;
  }
}

Arena* ArenaPool::AllocArena(size_t size) {
  Thread* self = Thread::Current();
  Arena* ret = nullptr;
  {
    MutexLock lock(self, lock_);
    if (free_arenas_ != nullptr && LIKELY(free_arenas_->Size() >= size)) {
      ret = free_arenas_;
      free_arenas_ = free_arenas_->next_;
    }
    if (ret == nullptr) {
      ret = new Arena(size);
    }
    used_arenas_.insert(ret);
  }
  ret->Reset();
  return ret;
}

void ArenaPool::FreeArena(Arena* arena) {
  Thread* self = Thread::Current();
  MutexLock lock(self, lock_);
  AddFreeArena(arena);
  auto it = used_arenas_.find(arena);
  DCHECK(it != used_arenas_.end());
  used_arenas_.erase(it);
}

void ArenaPool::AddFreeArena(Arena* arena) {
  arena->next_ = free_arenas_;
  free_arenas_ = arena;
  if (UNLIKELY(RUNNING_ON_VALGRIND)) {
    VALGRIND_MAKE_MEM_UNDEFINED(arena->memory_, arena->bytes_allocated_);
  }
}

void ArenaPool::FreeAllArenas() {
  MutexLock lock(Thread::Current(), lock_);
  for (Arena* arena : used_arenas_) {
    AddFreeArena(arena);
  }
  used_arenas_.clear();
}

AtomicArenaAllocator::AtomicArenaAllocator()
    : lock_("Atomic arena allocator lock"),
      arena_pool_(nullptr),
      head_(nullptr),
      cur_(nullptr) {
}

void AtomicArenaAllocator::Init(ArenaPool* arena_pool) {
  arena_pool_ = arena_pool;
  head_ = cur_ = ArenaRegion::Create(arena_pool_->AllocArena(kDefaultArenaSize));
}

void AtomicArenaAllocator::Invalidate() {
  head_ = cur_ = nullptr;
}

AtomicArenaAllocator::ArenaRegion::ArenaRegion(Arena* arena, size_t capacity)
    : arena_(arena),
      capacity_(capacity),
      pos_(0),
      next_(nullptr) {
}

void* AtomicArenaAllocator::AllocSlowPath(size_t bytes) {
  MutexLock mu(Thread::Current(), lock_);
  for (;;) {
    void* ptr = cur_->Alloc(bytes);
    if (ptr != nullptr) {
      return ptr;
    }
    cur_->next_ = ArenaRegion::Create(arena_pool_->AllocArena(std::max(bytes, kDefaultArenaSize)));
    cur_ = cur_->next_;
  }
}

AtomicArenaAllocator::ArenaRegion* AtomicArenaAllocator::ArenaRegion::Create(Arena* arena) {
  return new(arena->Begin())ArenaRegion(arena, arena->Size() - sizeof(ArenaRegion));
}

}  // namespace art
