/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef ART_RUNTIME_GC_MAP_H_
#define ART_RUNTIME_GC_MAP_H_

#include <stdint.h>

#include "base/logging.h"
#include "base/macros.h"
#include "globals.h"

namespace art {

/*
 * GcMap maps from an key to a inline bitmap. Each key bitmap pair is referred to as an entry.
 * An entry has a key which is key_bits_ in size as well as a bitmap which is bitmap_bits_ in size.
 * Bitmap_bits_ can be rather large so we use byte sto encode it. These bytes are the
 * bitmap_size_bytes_. The nunber of bitmap_size_bytes_ is equal to the bitmap_size_bytes_ 3 bits in
 * the first byte.
 * Format:
 * 1 byte: key_bits_[5 bits], bitmap_size_bytes[3 bits]
 * bitmap_bits_ : [bitmap_size_bytes * kBitsPerByte]
 * num_entries_ : [key_bits_]
 * entries[] = [key_bits_][bitmap_bits_] (tightly packed)
 * 0 to 7 bits of padding at the end of the array.
 */
class GcMap {
 public:
  static constexpr size_t kKeyBits = 5;  // Number of bits to store the key in the header.

  // Returns header size in bits.
  static size_t ComputeHeaderBits(size_t num_entries, size_t key_bits, size_t bitmap_bits);
  static size_t ComputeBitmapSizeBytes(size_t bitmap_bits);
  static size_t ComputeSize(size_t num_entries, size_t key_bits, size_t bitmap_bits);
  static size_t MaxKeyBits() {
    // Actually limited to 32 but the GC map builder has a 24 bit limit on 32 bit systems.
    return 24;
  }

  // Constructor for reading a GC map.
  explicit GcMap(const uint8_t* data) : data_(data) {
    ReadHeader();
  }
  void VerifyHeader() {
    CHECK_EQ(DataBegin(), ComputeHeaderBits(NumEntries(), KeyBits(), BitmapBits())) <<
        NumEntries() << " " << KeyBits() << " " << BitmapBits();
  }
  void ReadHeader() {
    size_t pos = 0;
    key_bits_ = data_[pos] & kKeyMask;
    size_t bitmap_size_bytes = data_[pos] >> kKeyBits;
    ++pos;
    bitmap_bits_ = 0;
    for (size_t i = 0; i < bitmap_size_bytes; ++i) {
      bitmap_bits_ |= data_[pos++] << (kBitsPerByte * i);
    }
    data_begin_ = pos * kBitsPerByte + KeyBits();
    num_entries_ = 0;
    num_entries_ = ReadBits(data_begin_ - KeyBits(), KeyBits());
    VerifyHeader();
  }
  size_t KeyBits() const {
    return key_bits_;
  }
  // Returns number of bits in a bitmap, there is one bitmap per key.
  size_t BitmapBits() const {
    return bitmap_bits_;
  }
  // Returns number of bits between keys.
  size_t TotalBitsPerLine() const {
    return KeyBits() + BitmapBits();
  }
  size_t DataBegin() const {  // Offset of the first key.
    return data_begin_;
  }
  size_t NumEntries() const {
    return num_entries_;
  }
  // Returns the bit position of a key for entry index where index < NumEntries().
  size_t KeyPosForIndex(size_t index) const {
    DCHECK_LT(index, NumEntries());
    return DataBegin() + index * TotalBitsPerLine();
  }
  // Returns the bit position of a bitmap for entry index where index < NumEntries().
  size_t BitmapPosForIndex(size_t index) const {
    DCHECK_LT(index, NumEntries());
    return KeyPosForIndex(index) + KeyBits();
  }
  size_t TotalSizeInBits() const {
    return DataBegin() + TotalBitsPerLine() * NumEntries();
  }
  size_t GetBit(size_t bit_index) const {
    DCHECK_LT(bit_index, TotalSizeInBits());
    return (data_[bit_index / kBitsPerByte] >> (kBitsPerByte - 1 - bit_index % kBitsPerByte)) & 1;
  }
  ALWAYS_INLINE size_t ReadBits(size_t index, size_t count) const {
    size_t bits = 0;
    DCHECK_LE(count, kBitsPerByte * sizeof(bits));
    size_t byte_index = index / kBitsPerByte;
    size_t bit_index = kBitsPerByte - index % kBitsPerByte;
    while (true) {
      const size_t cur_bits = data_[byte_index] & ((1 << bit_index) - 1);
      if (count <= bit_index) {
        return (bits << count) | (cur_bits >> (bit_index - count));
      }
      bits = (bits << bit_index) | cur_bits;
      count -= bit_index;
      bit_index = kBitsPerByte;
      ++byte_index;
    }
  }
  size_t GetKey(size_t key_index) const {
    return ReadBits(KeyPosForIndex(key_index), KeyBits());
  }
  void VerifySorted() const {
    for (size_t i = 1; i < NumEntries(); ++i) {
      CHECK_LT(GetKey(i - 1), GetKey(i));
    }
  }
  // Binary search to find a key, requires that the GC map is sorted.
  // No linear search since ReadBits is expensive.
  size_t Find(size_t key) const {
    size_t lo = 0;
    size_t hi = NumEntries();
    while (lo < hi) {
      const size_t mid = (hi + lo) / 2;
      const size_t mid_key = GetKey(mid);
      if (key > mid_key) {
        lo = mid + 1;
      } else if (key < mid_key) {
        hi = mid;
      } else {
        return BitmapPosForIndex(mid);
      }
    }
    return 0;
  }

 private:
  static constexpr size_t kKeyMask = (1U << kKeyBits) - 1;
  // Number for storing the number of bitmap bits.
  static constexpr size_t kLineSizeBits = kBitsPerByte - kKeyBits;
  size_t num_entries_;  // Number of entries per bitmap.
  size_t key_bits_;  //  Keys bits per entry.
  size_t bitmap_bits_;  // Bitmap bits per entry.
  size_t data_begin_;  // Which bit offset the key value pairs start.
  const uint8_t* data_;  // Backing data, not owned by the gc map.
};

}  // namespace art

#endif  // ART_RUNTIME_GC_MAP_H_
