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

#include "scoped_arena_allocator.h"

#include "arena_allocator.h"
#include <memcheck/memcheck.h>

namespace art {

static constexpr size_t kValgrindRedZoneBytes = 8;

ArenaStack::ArenaStack(ArenaPool* arena_pool)
  : DebugStackRefCounter(),
    pool_(arena_pool),
    bottom_arena_(nullptr),
    top_arena_(nullptr),
    top_ptr_(nullptr),
    top_end_(nullptr),
    running_on_valgrind_(RUNNING_ON_VALGRIND) {
}

ArenaStack::~ArenaStack() {
  Arena* arena = bottom_arena_;
  while (arena != nullptr) {
    Arena* next = arena->next_;
    pool_->FreeArena(arena);
    arena = next;
  }
}

uint8_t* ArenaStack::AllocateFromNextArena(size_t rounded_bytes) {
  UpdateBytesAllocated();
  size_t allocation_size = std::max(Arena::kDefaultSize, rounded_bytes);
  if (UNLIKELY(top_arena_ == nullptr)) {
    top_arena_ = bottom_arena_ = pool_->AllocArena(allocation_size);
    top_arena_->next_ = nullptr;
  } else if (top_arena_->next_ != nullptr && top_arena_->next_->Size() >= allocation_size) {
    top_arena_ = top_arena_->next_;
  } else {
    Arena* tail = top_arena_->next_;
    top_arena_->next_ = pool_->AllocArena(allocation_size);
    top_arena_ = top_arena_->next_;
    top_arena_->next_ = tail;
  }
  top_end_ = top_arena_->End();
  // top_ptr_ shall be updated by ScopedArenaAllocator.
  return top_arena_->Begin();
}

void ArenaStack::UpdateBytesAllocated() {
  if (top_arena_ != nullptr) {
    // Update how many bytes we have allocated into the arena so that the arena pool knows how
    // much memory to zero out. Though ScopedArenaAllocator doesn't guarantee the memory is
    // zero-initialized, the Arena may be reused by ArenaAllocator which does guarantee this.
    size_t allocated = static_cast<size_t>(top_ptr_ - top_arena_->Begin());
    if (top_arena_->bytes_allocated_ < allocated) {
      top_arena_->bytes_allocated_ = allocated;
    }
  }
}

void* ArenaStack::AllocValgrind(size_t bytes) {
  size_t rounded_bytes = (bytes + kValgrindRedZoneBytes + 3) & ~3;
  uint8_t* ptr = top_ptr_;
  if (UNLIKELY(static_cast<size_t>(top_end_ - ptr) < rounded_bytes)) {
    ptr = AllocateFromNextArena(rounded_bytes);
  }
  top_ptr_ = ptr + rounded_bytes;
  VALGRIND_MAKE_MEM_NOACCESS(ptr + bytes, rounded_bytes - bytes);
  return ptr;
}

ScopedArenaAllocator::ScopedArenaAllocator(ArenaStack* arena_stack)
  : DebugStackReference(arena_stack),
    DebugStackRefCounter(),
    arena_stack_(arena_stack),
    mark_arena_(arena_stack->top_arena_),
    mark_ptr_(arena_stack->top_ptr_),
    mark_end_(arena_stack->top_end_) {
}

ScopedArenaAllocator::~ScopedArenaAllocator() {
  Reset();
}

void ScopedArenaAllocator::Reset() {
  DebugStackReference::CheckTop();
  DebugStackRefCounter::CheckNoRefs();
  arena_stack_->UpdateBytesAllocated();
  if (LIKELY(mark_arena_ != nullptr)) {
    arena_stack_->top_arena_ = mark_arena_;
    arena_stack_->top_ptr_ = mark_ptr_;
    arena_stack_->top_end_ = mark_end_;
  } else if (arena_stack_->bottom_arena_ != nullptr) {
    mark_arena_ = arena_stack_->top_arena_ = arena_stack_->bottom_arena_;
    mark_ptr_ = arena_stack_->top_ptr_ = mark_arena_->Begin();
    mark_end_ = arena_stack_->top_end_ = mark_arena_->End();
  }
}

}  // namespace art
