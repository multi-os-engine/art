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

#ifndef ART_COMPILER_DEX_SCOPED_ARENA_ALLOCATOR_H_
#define ART_COMPILER_DEX_SCOPED_ARENA_ALLOCATOR_H_

#include "base/logging.h"
#include "base/macros.h"
#include "debug_lifo.h"
#include "globals.h"

namespace art {

class Arena;
class ArenaPool;
class ArenaStack;
class ScopedArenaAllocator;

template <typename T>
class ScopedArenaAllocatorAdapter;

// Holds a list of Arenas for use by ScopedArenaAllocator stack.
class ArenaStack : private DebugLifoRefCounter {
 public:
  explicit ArenaStack(ArenaPool* arena_pool);
  ~ArenaStack();

 private:
  // Private - access via ScopedArenaAllocator or ScopedArenaAllocatorAdapter.
  void* Alloc(size_t bytes) ALWAYS_INLINE {
    if (UNLIKELY(running_on_valgrind_)) {
      return AllocValgrind(bytes);
    }
    size_t rounded_bytes = (bytes + 3) & ~3;
    uint8_t* ptr = top_ptr_;
    if (UNLIKELY(static_cast<size_t>(top_end_ - ptr) < rounded_bytes)) {
      ptr = AllocateFromNextArena(rounded_bytes);
    }
    top_ptr_ = ptr + rounded_bytes;
    return ptr;
  }

  uint8_t* AllocateFromNextArena(size_t rounded_bytes);
  void UpdateBytesAllocated();
  void* AllocValgrind(size_t bytes);

  ArenaPool* const pool_;
  Arena* bottom_arena_;
  Arena* top_arena_;
  uint8_t* top_ptr_;
  uint8_t* top_end_;

  bool running_on_valgrind_;

  friend class ScopedArenaAllocator;
  template <typename T>
  friend class ScopedArenaAllocatorAdapter;
};

class ScopedArenaAllocator : private DebugLifoReference, private DebugLifoRefCounter {
 public:
  explicit ScopedArenaAllocator(ArenaStack* arena_stack);
  ~ScopedArenaAllocator();

  void Reset();

  void* Alloc(size_t bytes) ALWAYS_INLINE {
    DebugLifoReference::CheckTop();
    return arena_stack_->Alloc(bytes);
  }

  // ScopedArenaAllocatorAdapter is incomplete here, we need to define this later.
  ScopedArenaAllocatorAdapter<void> Adapter();

 private:
  ArenaStack* arena_stack_;
  Arena* mark_arena_;
  uint8_t* mark_ptr_;
  uint8_t* mark_end_;

  template <typename T>
  friend class ScopedArenaAllocatorAdapter;
};

template <>
class ScopedArenaAllocatorAdapter<void> : DebugLifoReference, DebugLifoIndirectTopRef {
 public:
  typedef void value_type;
  typedef void* pointer;
  typedef const void* const_pointer;

  template <typename U>
  struct rebind {
    typedef ScopedArenaAllocatorAdapter<U> other;
  };

  explicit ScopedArenaAllocatorAdapter(ScopedArenaAllocator* arena_allocator)
      : DebugLifoReference(arena_allocator),
        DebugLifoIndirectTopRef(arena_allocator),
        arena_stack_(arena_allocator->arena_stack_) {
  }
  template <typename U>
  ScopedArenaAllocatorAdapter(const ScopedArenaAllocatorAdapter<U>& other)
      : DebugLifoReference(other),
        DebugLifoIndirectTopRef(other),
        arena_stack_(other.arena_stack_) {
  }
  ScopedArenaAllocatorAdapter(const ScopedArenaAllocatorAdapter& other) = default;
  ScopedArenaAllocatorAdapter& operator=(const ScopedArenaAllocatorAdapter& other) = default;
  ~ScopedArenaAllocatorAdapter() = default;

 private:
  ArenaStack* arena_stack_;

  template <typename U>
  friend class ScopedArenaAllocatorAdapter;
};

// Adapter for use of ScopedArenaAllocator in STL containers.
template <typename T>
class ScopedArenaAllocatorAdapter : DebugLifoReference, DebugLifoIndirectTopRef {
 public:
  typedef T value_type;
  typedef T* pointer;
  typedef T& reference;
  typedef const T* const_pointer;
  typedef const T& const_reference;
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;

  template <typename U>
  struct rebind {
    typedef ScopedArenaAllocatorAdapter<U> other;
  };

  explicit ScopedArenaAllocatorAdapter(ScopedArenaAllocator* arena_allocator)
      : DebugLifoReference(arena_allocator),
        DebugLifoIndirectTopRef(arena_allocator),
        arena_stack_(arena_allocator->arena_stack_) {
  }
  template <typename U>
  ScopedArenaAllocatorAdapter(const ScopedArenaAllocatorAdapter<U>& other)
      : DebugLifoReference(other),
        DebugLifoIndirectTopRef(other),
        arena_stack_(other.arena_stack_) {
  }
  ScopedArenaAllocatorAdapter(const ScopedArenaAllocatorAdapter& other) = default;
  ScopedArenaAllocatorAdapter& operator=(const ScopedArenaAllocatorAdapter& other) = default;
  ~ScopedArenaAllocatorAdapter() = default;

  size_type max_size() const {
    return static_cast<size_type>(-1) / sizeof(T);
  }

  pointer address(reference x) const { return &x; }
  const_pointer address(const_reference x) const { return &x; }

  pointer allocate(size_type n, ScopedArenaAllocatorAdapter<void>::pointer hint = nullptr) {
    DCHECK_LE(n, max_size());
    DebugLifoIndirectTopRef::CheckTop();
    return reinterpret_cast<T*>(arena_stack_->Alloc(n * sizeof(T)));
  }
  void deallocate(pointer p, size_type n) {
    DebugLifoIndirectTopRef::CheckTop();
  }

  void construct(pointer p, const_reference val) {
    DebugLifoIndirectTopRef::CheckTop();
    new (static_cast<void*>(p)) value_type(val);
  }
  void destroy(pointer p) {
    DebugLifoIndirectTopRef::CheckTop();
    p->~value_type();
  }

 private:
  ArenaStack* arena_stack_;

  template <typename U>
  friend class ScopedArenaAllocatorAdapter;
};

inline ScopedArenaAllocatorAdapter<void> ScopedArenaAllocator::Adapter() {
  return ScopedArenaAllocatorAdapter<void>(this);
}

}  // namespace art

#endif  // ART_COMPILER_DEX_SCOPED_ARENA_ALLOCATOR_H_
