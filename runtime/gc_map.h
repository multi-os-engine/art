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
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/serializable.h"
#include "globals.h"
#include "safe_map.h"
#include "utils.h"

namespace art {

  enum EncodedGcMapFormat {
    kMapFormatNativePcToRef,
    kMapFormatDexToRef,
  };

// Lightweight wrapper for native PC offset to reference bit maps.
class NativePcOffsetToReferenceMap {
 public:
  explicit NativePcOffsetToReferenceMap(const uint8_t* data) : data_(data) {
    CHECK(data_ != NULL);
  }

  // The number of entries in the table.
  size_t NumEntries() const {
    return data_[2] | (data_[3] << 8);
  }

  // Return address of bitmap encoding what are live references.
  const uint8_t* GetBitMap(size_t index) const {
    size_t entry_offset = index * EntryWidth();
    return &Table()[entry_offset + NativeOffsetWidth()];
  }

  // Get the native PC encoded in the table at the given index.
  uintptr_t GetNativePcOffset(size_t index) const {
    size_t entry_offset = index * EntryWidth();
    uintptr_t result = 0;
    for (size_t i = 0; i < NativeOffsetWidth(); ++i) {
      result |= Table()[entry_offset + i] << (i * 8);
    }
    return result;
  }

  // Does the given offset have an entry?
  bool HasEntry(uintptr_t native_pc_offset) {
    for (size_t i = 0; i < NumEntries(); ++i) {
      if (GetNativePcOffset(i) == native_pc_offset) {
        return true;
      }
    }
    return false;
  }

  // Finds the bitmap associated with the native pc offset.
  const uint8_t* FindBitMap(uintptr_t native_pc_offset) {
    size_t num_entries = NumEntries();
    size_t index = Hash(native_pc_offset) % num_entries;
    size_t misses = 0;
    while (GetNativePcOffset(index) != native_pc_offset) {
      index = (index + 1) % num_entries;
      misses++;
      DCHECK_LT(misses, num_entries) << "Failed to find offset: " << native_pc_offset;
    }
    return GetBitMap(index);
  }

  static uint32_t Hash(uint32_t native_offset) {
    uint32_t hash = native_offset;
    hash ^= (hash >> 20) ^ (hash >> 12);
    hash ^= (hash >> 7) ^ (hash >> 4);
    return hash;
  }

  // The number of bytes used to encode registers.
  size_t RegWidth() const {
    return (static_cast<size_t>(data_[0]) | (static_cast<size_t>(data_[1]) << 8)) >> 3;
  }

 private:
  // Skip the size information at the beginning of data.
  const uint8_t* Table() const {
    return data_ + 4;
  }

  // Number of bytes used to encode a native offset.
  size_t NativeOffsetWidth() const {
    return data_[0] & 7;
  }

  // The width of an entry in the table.
  size_t EntryWidth() const {
    return NativeOffsetWidth() + RegWidth();
  }

  const uint8_t* const data_;  // The header and table data
};

class EncodedGcMap : public Serializable {
  public:
    virtual ~EncodedGcMap() {}
    virtual uint8_t GetFormat() { return format_; }

    class EncodedGcMapBitMapIterator {
      public:
        virtual ~EncodedGcMapBitMapIterator() {}
        virtual void Reset() {}
        virtual int32_t GetNextReferenceVR() { return kInvalidVirtualReg; }
        virtual bool IsValidVR (int32_t vr) { return vr != kInvalidVirtualReg; }

      protected:
        static constexpr int32_t kInvalidVirtualReg = 0xFFFF;
    };

    virtual EncodedGcMapBitMapIterator* GetBitMapIterByNative(uintptr_t native_pc_offset) = 0;
    virtual EncodedGcMapBitMapIterator* GetBitMapIterByDex(uint16_t dex_pc_offset) = 0;

  protected:
    EncodedGcMapFormat format_;
};

class NativePcOffsetToReferenceMap2 : public EncodedGcMap {
  public:
      NativePcOffsetToReferenceMap2()
          : entries_(0), references_width_(0), native_offset_width_(0), offset_to_references() {
        format_ = kMapFormatNativePcToRef;
    }

    ~NativePcOffsetToReferenceMap2() {
      offset_to_references.clear();
    }

    size_t NumEntries() const { return entries_; }

    size_t NativeOffsetWidth() const { return native_offset_width_; }

    size_t RegWidth() const { return references_width_; }

    size_t EntryWidth() const {
      return NativeOffsetWidth() + RegWidth();
    }

    void AddEntry(uint32_t native_offset, const uint8_t* references, size_t references_width) {
      std::vector<uint8_t> references_copy;
      for (size_t i = 0; i < references_width; i++) {
        references_copy.push_back(references[i]);
      }

      offset_to_references.Put(native_offset, references_copy);
      entries_ = offset_to_references.size();

      if (references_width > references_width_) {
        references_width_ = references_width;
      }

      size_t native_offset_width = sizeof(native_offset) - CLZ(native_offset) / 8u;
      if (native_offset_width > native_offset_width_) {
        native_offset_width_ = native_offset_width;
      }
    }

    virtual size_t GetBytesNeededToSerialize() const {
      return (EntryWidth() * entries_) + GetBytesNeededForHeader();
    }

    virtual void Serialize(uint8_t* output, size_t buf_size) const {
      CHECK_GE(buf_size, GetBytesNeededToSerialize());
      SerializeHeader(output);

      std::vector<bool> in_use(entries_, false);
      for (auto it = offset_to_references.begin(); it != offset_to_references.end(); it++) {
        uint32_t native_offset = it->first;
        const std::vector<uint8_t>& references = it->second;

        size_t table_index = TableIndex(native_offset);
        while (in_use[table_index]) {
          table_index = (table_index + 1) % entries_;
        }
        in_use[table_index] = true;
        WriteCodeOffset(output, table_index, native_offset);
        DCHECK_EQ(native_offset, ReadNativePcOffset(output, table_index));
        WriteReferences(output, table_index, references);
      }
    }

    virtual bool Deserialize(const uint8_t* input) {
      if (DeserializeHeader(input) == false) {
        return false;
      }

      for (size_t entry = 0; entry < NumEntries(); entry++) {
        uint32_t native_offset = ReadNativePcOffset(input, entry);
        std::vector<uint8_t> ref_holder;

        const uint8_t* references = GetBitMapPtr(input, entry);
        for (size_t ref_entry = 0; ref_entry < RegWidth(); ref_entry++) {
          ref_holder.push_back(references[ref_entry]);
        }

        offset_to_references.Put(native_offset, ref_holder);
      }

      // Since there is no versioning system, assume we successfully decoded input.
      return true;
    }

    virtual void Dump(::std::ostream& os) const {
      // TODO Implement the dumper to read the map
    }

    class BitMapIterator : public EncodedGcMap::EncodedGcMapBitMapIterator {
      public:
        BitMapIterator(NativePcOffsetToReferenceMap2 *map, uint32_t native_pc): map_(map), native_pc_(native_pc), cur_ref_(0) {}

        void Reset() {
          cur_ref_ = 0;
        }

        int32_t GetNextReferenceVR() {
          size_t out_of_bounds = map_->RegWidth() * kBitsPerByte;

          auto it = map_->offset_to_references.find(native_pc_);
          std::vector<uint8_t>& ref_vec = it->second;

          bool found_ref = false;
          while (found_ref == false || cur_ref_ >= out_of_bounds) {
            if (((ref_vec[cur_ref_ / kBitsPerByte] >> (cur_ref_ % kBitsPerByte)) & 0x01) != 0) {
              found_ref = true;
              break;
            }
            cur_ref_++;
          }

          if (found_ref == false || cur_ref_ >= out_of_bounds) {
            return kInvalidVirtualReg;
          } else {
            int32_t ref_vr = cur_ref_;
            cur_ref_++;
            return ref_vr;
          }
        }

      private:
        NativePcOffsetToReferenceMap2 *map_;
        uint32_t native_pc_;
        size_t cur_ref_;
    };

    virtual BitMapIterator* GetBitMapIterByNative(uintptr_t native_pc_offset) {
      auto it = offset_to_iterators.find(native_pc_offset);

      if (it == offset_to_iterators.end()) {
        BitMapIterator iter(this, native_pc_offset);
        offset_to_iterators.Put(native_pc_offset, iter);
        auto it = offset_to_iterators.find(native_pc_offset);
        return &(it->second);
      } else {
        it->second.Reset();
        return &(it->second);
      }
    }

    virtual BitMapIterator* GetBitMapIterByDex(uint16_t dex_pc_offset) {
      // The native PC map does not have information about dex PCs.
      return nullptr;
    }

    virtual size_t GetBitMapSizeInBytes() {
      return RegWidth();
    }

  private:
    size_t GetBytesNeededForHeader() const {
      return sizeof(uint32_t);
    }

    void SerializeHeader(uint8_t* output) const {
      // TODO Add the format to the header
      CHECK_LT(native_offset_width_, 1U << 3);
      output[0] = native_offset_width_ & 7;
      CHECK_LT(references_width_, 1U << 13);
      output[0] |= (references_width_ << 3) & 0xFF;
      output[1] = (references_width_ >> 5) & 0xFF;
      CHECK_LT(entries_, 1U << 16);
      output[2] = entries_ & 0xFF;
      output[3] = (entries_ >> 8) & 0xFF;
    }

    bool DeserializeHeader(const uint8_t* input) {
      // TODO Check the format in the header to make sure it can be deserialized.
      references_width_ = (static_cast<size_t>(input[0]) | (static_cast<size_t>(input[1]) << 8)) >> 3;
      native_offset_width_ = input[0] & 7;
      entries_ = input[2] | (input[3] << 8);
      return true;
    }

    static uint32_t Hash(uint32_t native_offset) {
      uint32_t hash = native_offset;
      hash ^= (hash >> 20) ^ (hash >> 12);
      hash ^= (hash >> 7) ^ (hash >> 4);
      return hash;
    }

    size_t TableIndex(uint32_t native_offset) const {
      return Hash(native_offset) % entries_;
    }

    uint32_t ReadNativePcOffset(const uint8_t* table, size_t table_index) const {
      uint32_t native_offset = 0;
      size_t table_offset = (table_index * EntryWidth()) + GetBytesNeededForHeader();
      for (size_t i = 0; i < NativeOffsetWidth(); i++) {
        native_offset |= table[table_offset + i] << (i * 8);
      }
      return native_offset;
    }

    // Return address of bitmap encoding what are live references.
    const uint8_t* GetBitMapPtr(const uint8_t* table, size_t table_index) const {
      size_t table_offset = (table_index * EntryWidth()) + GetBytesNeededForHeader();
      return &(table[table_offset + NativeOffsetWidth()]);
    }

    void WriteCodeOffset(uint8_t* table, size_t table_index, uint32_t native_offset) const {
      size_t table_offset = (table_index * EntryWidth()) + GetBytesNeededForHeader();
      for (size_t i = 0; i < NativeOffsetWidth(); i++) {
        table[table_offset + i] = (native_offset >> (i * 8)) & 0xFF;
      }
    }

    void WriteReferences(uint8_t* table, size_t table_index, const std::vector<uint8_t> &references) const {
      size_t table_offset = (table_index * EntryWidth()) + GetBytesNeededForHeader();
      size_t ref_index = 0;
      for (; ref_index < references.size(); ref_index++) {
        table[table_offset + NativeOffsetWidth() + ref_index] = references[ref_index];
      }
      for (; ref_index < references_width_; ref_index++) {
        table[table_offset + NativeOffsetWidth() + ref_index] = 0;
      }
    }

    /**
     * @brief Number of entries in the table.
     */
    size_t entries_;

    /**
     * @brief Number of bytes used to encode the reference bitmap.
     */
    size_t references_width_;

    /**
     * @brief Number of bytes used to encode a native offset.
     */
    size_t native_offset_width_;

    /**
     * @brief Map of native offset to references.
     */
    SafeMap<uint32_t, std::vector<uint8_t> > offset_to_references;

    /**
     * @brief Map of native offset to reference iterator.
     */
    SafeMap<uint32_t, BitMapIterator> offset_to_iterators;
};

}  // namespace art

#endif  // ART_RUNTIME_GC_MAP_H_
