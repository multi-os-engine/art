/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_RUNTIME_MIRROR_STRING_INL_H_
#define ART_RUNTIME_MIRROR_STRING_INL_H_

#include "string.h"

#include "class.h"
#include "gc/heap-inl.h"

namespace art {
namespace mirror {

// Used for setting string count in the allocation code path to ensure it is guarded by a CAS.
class SetStringCountVisitor {
 public:
  explicit SetStringCountVisitor(int32_t count) : count_(count) {
  }

  void operator()(Object* obj) const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    // Avoid AsString as object is not yet in live bitmap or allocation stack.
    String* string = down_cast<String*>(obj);
    string->SetCount(count_);
  }

 private:
  const int32_t count_;
};

template <bool kIsInstrumented>
inline String* String::Alloc(Thread* self, int32_t utf16_length) {
  size_t header_size = sizeof(String);
  size_t data_size = sizeof(uint16_t) * utf16_length;
  size_t size = header_size + data_size;
  Class* string_class = GetJavaLangString();

  // Check for overflow and throw OutOfMemoryError if this was an unreasonable request.
  if (UNLIKELY(size < data_size)) {
    self->ThrowOutOfMemoryError(StringPrintf("%s of length %d would overflow",
                                             PrettyDescriptor(string_class).c_str(),
                                             utf16_length).c_str());
    return nullptr;
  }
  gc::Heap* heap = Runtime::Current()->GetHeap();
  gc::AllocatorType allocator_type = heap->GetCurrentAllocator();
  SetStringCountVisitor visitor(utf16_length);
  return down_cast<String*>(
      heap->AllocObjectWithAllocator<kIsInstrumented, true>(self, string_class, size,
                                                            allocator_type, visitor));
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_STRING_INL_H_
