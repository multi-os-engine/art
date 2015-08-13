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

#ifndef ART_RUNTIME_LENGTH_PREFIXED_ARRAY_H_
#define ART_RUNTIME_LENGTH_PREFIXED_ARRAY_H_

#include <stddef.h>  // for offsetof()

#include "linear_alloc.h"
#include "stride_iterator.h"
#include "base/iteration_range.h"

namespace art {

template<typename T>
class LengthPrefixedArray {
 public:
  explicit LengthPrefixedArray(uint32_t length, uint32_t element_size)
      : length_(length),
        element_size_(element_size) {}

  T& At(size_t index) {
    DCHECK_LT(index, length_);
    return *reinterpret_cast<T*>(&data_[0] + index * element_size_);
  }

  StrideIterator<T> Begin() {
    return StrideIterator<T>(reinterpret_cast<T*>(&data_[0]), element_size_);
  }

  StrideIterator<T> End() {
    return StrideIterator<T>(reinterpret_cast<T*>(&data_[0] + element_size_ * length_),
                             element_size_);
  }

  static size_t OffsetOfElement(size_t index, size_t element_size = sizeof(T)) {
    return offsetof(LengthPrefixedArray<T>, data_) + index * element_size;
  }

  // Alignment is the caller's responsibility.
  static size_t ComputeSize(size_t num_elements, size_t element_size = sizeof(T)) {
    return OffsetOfElement(num_elements, element_size);
  }

  uint32_t Length() const {
    return length_;
  }

  // Update the length but does not reallocate storage.
  void SetLength(uint32_t length) {
    length_ = length;
  }

  uint32_t ElementSize() const {
    return element_size_;
  }

  // Update the element size but does not reallocate storage.
  void SetElementSize(uint32_t element_size) {
    element_size_ = element_size;
  }

 private:
  uint32_t length_;  // 64 bits for 8 byte alignment of data_.
  uint32_t element_size_;
  uint8_t data_[0];
};

// Returns empty iteration range if the array is null.
template<typename T>
IterationRange<StrideIterator<T>> MakeIterationRangeFromLengthPrefixedArray(
    LengthPrefixedArray<T>* arr) {
  return arr != nullptr ?
      MakeIterationRange(arr->Begin(), arr->End()) :
      MakeEmptyIterationRange(StrideIterator<T>(nullptr, 0));
}

}  // namespace art

#endif  // ART_RUNTIME_LENGTH_PREFIXED_ARRAY_H_
