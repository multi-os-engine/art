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

#ifndef ART_RUNTIME_GC_MAP_INL_H_
#define ART_RUNTIME_GC_MAP_INL_H_

#include "gc_map.h"

#include "utils.h"

namespace art {

// Returns header size in bits.
inline size_t GcMap::ComputeHeaderBits(size_t num_entries, size_t key_bits, size_t bitmap_bits) {
  DCHECK_LT(num_entries, 1U << key_bits);
  DCHECK_LE(key_bits, kKeyMask);
  size_t bits = kBitsPerByte;
  // How many bytes are used to write the line bitmap size.
  bits += ComputeBitmapSizeBytes(bitmap_bits) * kBitsPerByte;
  // Finally the NumEntries() bits which is equal to key bits since it doesn't make sense to have
  // more entries than the maximum key since we never have duplicate keys.
  bits += key_bits;
  return bits;
}
inline size_t GcMap::ComputeBitmapSizeBytes(size_t bitmap_bits) {
  if (!bitmap_bits) {
    return 0;
  }
  size_t bits = sizeof(bitmap_bits) * kBitsPerByte - CLZ(bitmap_bits);
  return RoundUp(bits, kBitsPerByte) / kBitsPerByte;
}
inline size_t GcMap::ComputeSize(size_t num_entries, size_t key_bits, size_t bitmap_bits) {
  size_t total_bits = ComputeHeaderBits(num_entries, key_bits, bitmap_bits);
  total_bits += (key_bits + bitmap_bits) * num_entries;
  return RoundUp(total_bits, kBitsPerByte) / kBitsPerByte;
}

}  // namespace art

#endif  // ART_RUNTIME_GC_MAP_INL_H_
