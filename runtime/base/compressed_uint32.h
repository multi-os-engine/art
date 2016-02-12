/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ART_RUNTIME_BASE_COMPRESSED_UINT32_H_
#define ART_RUNTIME_BASE_COMPRESSED_UINT32_H_

#include <stdint.h>

#include "base/logging.h"

namespace art {

// Simple variable-length encoding scheme.
// Integers between 0 and 127 take one byte and the value is stored as-is in the byte.
// Other integers take five bytes and the main byte encodes offset to the actual value,
// which is stored in nearby scratch space (within the following 128 bytes).
class CompressedUint32 {
 public:
  typedef __attribute__((__aligned__(1))) uint32_t unaligned_uint32_t;

  CompressedUint32() : value_(0) { }

  uint32_t Get() const {
    if (LIKELY(value_ >= 0)) {
      // Positive values encode the value as-is.
      return value_;
    } else {
      // Negative values encode offset to nearby scratch space.
      const ptrdiff_t offset = -value_;
      return *reinterpret_cast<const unaligned_uint32_t*>(&value_ + offset);
    }
  }

  // The location of the scratch space is up to the user, however it must be
  // near (within 128 bytes) and it must not move relative to this structure.
  void Set(uint32_t new_value, unaligned_uint32_t** scratch_space) {
    if (LIKELY(value_ >= 0)) {
      if (new_value < 0x80) {
        // Overwrite the single byte value.
        value_ = new_value;
      } else {
        DCHECK(scratch_space != nullptr);
        // Point the main byte to the scratch space.
        ptrdiff_t offset = reinterpret_cast<int8_t*>(*scratch_space) - &value_;
        DCHECK_GT(offset, 0);
        value_ = -offset;
        DCHECK_EQ(value_, -offset) << "The offset does not fit int8_t";
        // Allocate scratch space and store the value there.
        *(*scratch_space)++ = new_value;
      }
    } else {
      // Overwrite the value in scratch space.
      const ptrdiff_t offset = -value_;
      *reinterpret_cast<unaligned_uint32_t*>(&value_ + offset) = new_value;
    }
    DCHECK_EQ(Get(), new_value);
  }

 private:
  int8_t value_;
};

}  // namespace art

#endif  // ART_RUNTIME_BASE_COMPRESSED_UINT32_H_

