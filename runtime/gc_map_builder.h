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

#ifndef ART_RUNTIME_GC_MAP_BUILDER_H_
#define ART_RUNTIME_GC_MAP_BUILDER_H_

#include <vector>

#include "gc_map.h"
#include "utils.h"

namespace art {

class GcMapBuilder {
 public:
  explicit GcMapBuilder(std::vector<uint8_t>* out_data, size_t num_entries, size_t key_bits,
                        size_t bitmap_bits) : bit_count_(0), bit_buffer_(0),
                        num_entries_(num_entries), key_bits_(key_bits), bitmap_bits_(bitmap_bits) {
    out_data->resize(GcMap::ComputeSize(num_entries, key_bits, bitmap_bits));
    byte_ptr_ = &out_data->operator[](0);
    start_ptr_ = byte_ptr_;
    WriteHeader();
  }
  void WriteKey(size_t key) {
    WriteBits(key_bits_, key);
  }
  static size_t GetMaxBitsPerWrite() {
    // -1 since we have up to kBitsPerByte - 1 bits active in the buffer.
    return (sizeof(bit_buffer_) - 1) * kBitsPerByte;
  }
  void WriteBits(size_t count, size_t value) {
    DCHECK_LT(value, static_cast<size_t>(1U) << count);
    DCHECK_LE(count, GetMaxBitsPerWrite());
    // Flush the buffer as much as we can, one byte at a time so that we have at least
    // GetMaxBitsPerWrite() bits available.
    FlushBitBuffer();
    bit_buffer_ = (bit_buffer_ << count) | value;
    bit_count_ += count;
  }
  void WriteBitsFromMap(GcMap* source_map, size_t bit_index, size_t bit_count) {
    const size_t bitmap_offset_limit = bit_index + bit_count;
    while (bit_index < bitmap_offset_limit) {
      const size_t count = std::min(bitmap_offset_limit - bit_index, GetMaxBitsPerWrite());
      WriteBits(count, source_map->ReadBits(bit_index, count));
      bit_index += count;
    }
  }
  ~GcMapBuilder() {
    FlushRemainingBits();
    if (kIsDebugBuild) {
      GcMap map(start_ptr_);
      DCHECK_EQ(map.NumEntries(), NumEntries());
      DCHECK_EQ(map.KeyBits(), KeyBits());
      DCHECK_EQ(map.BitmapBits(), BitmapBits());
      map.VerifySorted();
    }
  }
  size_t NumEntries() const {
    return num_entries_;
  }
  size_t KeyBits() const {
    return key_bits_;
  }
  size_t BitmapBits() const {
    return bitmap_bits_;
  }

 private:
  void WriteHeader() {
    size_t bitmap_size_bytes = GcMap::ComputeBitmapSizeBytes(bitmap_bits_);
    *byte_ptr_ = key_bits_ | (bitmap_size_bytes << GcMap::kKeyBits);
    ++byte_ptr_;
    for (size_t i = 0; i < bitmap_size_bytes; ++i) {
      *byte_ptr_ = bitmap_bits_ >> (kBitsPerByte * i);
      ++byte_ptr_;
    }
    bit_buffer_ = num_entries_;
    bit_count_ = key_bits_;
  }
  ALWAYS_INLINE void FlushBitBuffer() {
    while (bit_count_ >= kBitsPerByte) {  // Write a whole bit at a time until we no longer can.
      bit_count_ -= kBitsPerByte;
      *byte_ptr_ = static_cast<uint8_t>(bit_buffer_ >> bit_count_);
      ++byte_ptr_;
    }
  }
  void FlushRemainingBits() {
    FlushBitBuffer();
    if (bit_count_) {
      *byte_ptr_ = static_cast<uint8_t>(bit_buffer_ << (kBitsPerByte - bit_count_));
      bit_count_ = 0;
    }
  }
  byte* start_ptr_;  // Used for verification.
  byte* byte_ptr_;
  size_t bit_count_;  // Number of bits in bit_buffer_.
  size_t bit_buffer_;  // Current bit buffer.
  const size_t num_entries_;
  const size_t key_bits_;
  const size_t bitmap_bits_;
};

}  // namespace art

#endif  // ART_RUNTIME_GC_MAP_BUILDER_H_
