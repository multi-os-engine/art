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
#include "utils.h"

namespace art {

/*
 * GcMap maps from an index to a inline bitmap.
 * Format:
 * 1 byte: key_bits_[5 bits], line_size_bits[3 bits]: line size's size in bytes.
 * 0-7 bytes: Bitmap Bits
 * num_entries_ : [Key bits]
 * [Line bits]* = [Key bits][Bitmap bits] (tightly packed)
 * 0 to 7 bits of padding at the end of the array.
 */
class GcMap {
 public:
  // Returns header size in bits.
  static size_t ComputeHeaderBits(size_t num_entries, size_t key_bits, size_t bitmap_bits) {
    DCHECK_LT(num_entries, 1U << key_bits);
    DCHECK_LE(key_bits, kKeyMask);
    size_t bits = kBitsPerByte;
    // How many bytes are used to write the line bitmap size.
    bits += ComputeBitmapSizeBytes(bitmap_bits) * kBitsPerByte;
    // Finally the NumEntries() bits.
    bits += key_bits;
    return bits;
  }
  static size_t ComputeBitmapSizeBytes(size_t bitmap_bits) {
    if (!bitmap_bits) {
      return 0;
    }
    size_t bits = sizeof(bitmap_bits) * kBitsPerByte - CLZ(bitmap_bits);
    return RoundUp(bits, kBitsPerByte) / kBitsPerByte;
  }
  static size_t ComputeSize(size_t num_entries, size_t key_bits, size_t bitmap_bits) {
    size_t total_bits = ComputeHeaderBits(num_entries, key_bits, bitmap_bits);
    total_bits += (key_bits + bitmap_bits) * num_entries;
    return RoundUp(total_bits, kBitsPerByte) / kBitsPerByte;
  }
  GcMap() : num_entries_(0), key_bits_(0), bitmap_bits_(0), data_begin_(0), data_(0) {
  }
  // Constructor for reading a GC map.
  explicit GcMap(const uint8_t* data) : data_(const_cast<uint8_t*>(data)) {
    ReadHeader();
  }
  // Constructor for writing a new GcMap.
  GcMap(uint8_t* data, size_t num_entries, size_t key_bits, size_t bitmap_bits)
      : num_entries_(num_entries), key_bits_(key_bits), bitmap_bits_(bitmap_bits), data_(data) {
    WriteHeader();
  }
  void VerifyHeader() {
    CHECK_EQ(DataBegin(), ComputeHeaderBits(NumEntries(), KeyBits(), BitmapBits())) <<
        NumEntries() << " " << KeyBits() << " " << BitmapBits();
  }
  void ReadHeader() {
    size_t pos = 0;
    key_bits_ = data_[pos] & kKeyMask;
    size_t line_size_bits = data_[pos] >> kKeyBits;
    ++pos;
    bitmap_bits_ = 0;
    for (size_t i = 0; i < line_size_bits; ++i) {
      bitmap_bits_ |= data_[pos++] << (kBitsPerByte * i);
    }
    data_begin_ = pos * kBitsPerByte + KeyBits();
    num_entries_ = 0;
    num_entries_ = ReadBits(data_begin_ - KeyBits(), KeyBits());
    VerifyHeader();
  }
  void WriteHeader() {
    size_t line_size_bits = ComputeBitmapSizeBytes(bitmap_bits_);
    size_t pos = 0;
    data_[pos++] = key_bits_ | (line_size_bits << kKeyBits);
    for (size_t i = 0; i < line_size_bits; ++i) {
      data_[pos++] = bitmap_bits_ >> (kBitsPerByte * i);
    }
    data_begin_ = pos * kBitsPerByte + KeyBits();
    WriteBits(data_begin_ - KeyBits(), KeyBits(), NumEntries());
    VerifyHeader();
  }
  // Returns the bit position for a key index.
  size_t KeyBits() const {
    return key_bits_;
  }
  size_t BitmapBits() const {
    return bitmap_bits_;
  }
  size_t TotalBitsPerLine() const {
    return KeyBits() + BitmapBits();
  }
  size_t DataBegin() const {
    return data_begin_;
  }
  size_t KeyPosForIndex(size_t index) const {
    return DataBegin() + index * TotalBitsPerLine();
  }
  size_t BitmapPosForIndex(size_t index) const {
    return KeyPosForIndex(index) + KeyBits();
  }
  size_t TotalSizeInBits() const {
    return DataBegin() + TotalBitsPerLine() * NumEntries();
  }
  size_t GetBit(size_t bit_index) const {
    DCHECK_LT(bit_index, TotalSizeInBits());
    return (data_[bit_index / kBitsPerByte] >> (bit_index % kBitsPerByte)) & 1;
  }
  void SetBit(size_t bit_index) {
    DCHECK_LT(bit_index, TotalSizeInBits());
    data_[bit_index / kBitsPerByte] |= (1 << (bit_index % kBitsPerByte));
  }
  void ClearBit(size_t bit_index) {
    DCHECK_LT(bit_index, TotalSizeInBits());
    data_[bit_index / kBitsPerByte] &= ~(1 << (bit_index % kBitsPerByte));
  }
  // TODO: Optimize.
  size_t ReadBits(size_t index, size_t count) const {
    size_t bits = 0;
    while (count) {
      size_t bit = GetBit(index);
      bits = (bits << 1) | bit;
      --count;
      ++index;
    }
    return bits;
  }
  // TODO: Optimize.
  void WriteBits(size_t index, size_t count, size_t value) {
    for (size_t i = count; i != 0; --i) {
      if ((value & (1 << (i - 1))) != 0) {
        SetBit(index);
      } else {
        ClearBit(index);
      }
      index++;
    }
  }
  size_t GetKey(size_t key_index) const {
    return ReadBits(KeyPosForIndex(key_index), KeyBits());
  }
  size_t NumEntries() const {
    return num_entries_;
  }
  // Returns the bit location for a value associated with a key.
  // Returns 0 if not found.
  size_t Find(size_t key) {
    const size_t num_entries = NumEntries();
    // Low since reading a key is expensive.
    static constexpr size_t kLinearSearchThreshold = 3;
    size_t lo = 0;
    size_t hi = num_entries;
    while (true) {
      if (hi - lo <= kLinearSearchThreshold) {
        for (size_t i = lo; i < hi; i++)  {
          if (GetKey(i) == key) {
            return BitmapPosForIndex(i);
          }
        }
        return 0;
      }
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
  static constexpr size_t kKeyBits = 5;
  static constexpr size_t kKeyMask = (1U << kKeyBits) - 1;
  static constexpr size_t kLineSizeBits = kBitsPerByte - kKeyBits;
  // Stats read from the backing data.
  size_t num_entries_;  // Number of entries per bitmap.
  size_t key_bits_;  //  Keys bits per entry.
  size_t bitmap_bits_;  // Bitmap bits per entry.
  size_t data_begin_;  // Which bit offset the key value pairs start.
  // Backing data, not owned by the gc map.
  uint8_t* data_;
};

class GcMapBuilder {
 public:
  explicit GcMapBuilder(std::vector<uint8_t>* out_data, size_t num_entries, size_t key_bits,
                        size_t bitmap_bits) {
    out_data->resize(GcMap::ComputeSize(num_entries, key_bits, bitmap_bits));
    gc_map_ = GcMap(&out_data->operator[](0), num_entries, key_bits, bitmap_bits);  // TODO: Clean?
    bit_pos_ = gc_map_.DataBegin();
  }
  void WriteKey(size_t key) {;
    gc_map_.WriteBits(bit_pos_, gc_map_.KeyBits(), key);
    bit_pos_ += gc_map_.KeyBits();
  }
  void WriteBit(size_t bit) {
    if (bit) {
      gc_map_.SetBit(bit_pos_);
    } else {
      gc_map_.ClearBit(bit_pos_);
    }
    ++bit_pos_;
  }
  size_t GetBitPos() const {
    return bit_pos_;
  }
  GcMap* GetGcMap() {
    return &gc_map_;
  }

 private:
  GcMap gc_map_;
  size_t bit_pos_;
};

}  // namespace art

#endif  // ART_RUNTIME_GC_MAP_H_
