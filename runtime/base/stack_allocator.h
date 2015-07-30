/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ART_RUNTIME_BASE_STACK_ALLOCATOR_H_
#define ART_RUNTIME_BASE_STACK_ALLOCATOR_H_

#include "base/macros.h"

#include <alloca.h>
#include <utility>

// Truly always inline. Even if we are debugging.
#define ALWAYS_INLINE_YES_REALLY __attribute__ ((always_inline))  // NOLINT [whitespace/parens] [4]

namespace art {

// Dynamically allocate C++ objects on the caller's stack frame and keep track of them.
// The StackAllocator instance itself should only ever be a local variable in a single function.
//
// Either storing this into a field or passing down or returning to other functions will
// result in incorrect behavior. In particular, since the underlying allocas only get
// inlined into the callee frame, attempting to invoke ::Allocate or ::MakeInstance when
// the StackAllocator instance isn't itself live on the same stack frame will cause the
// memory to be deallocated sooner than the StackAllocator itself.
class StackAllocator : ValueObject {
 public:
  // Allocate enough memory to store an object of the specified size.
  inline void* Allocate(size_t size) ALWAYS_INLINE_YES_REALLY {
    // Note: the inline is here for correctness, the alloca goes into the caller's frame.
    return alloca(size);
  }

  // Construct a new instance of T, forwarding the arguments to constructor. Allocated on stack.
  template <typename T, typename ... Args>
  inline T* MakeInstance(Args&& ... args) ALWAYS_INLINE_YES_REALLY {
    return new (Allocate(sizeof(T))) T{std::forward<Args>(args)...};  // NOLINT [readability/braces] [4]
  }
};

}  // namespace art
#endif  // ART_RUNTIME_BASE_STACK_ALLOCATOR_H_
