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

#ifndef ART_RUNTIME_LOOKUP_TABLES_H_
#define ART_RUNTIME_LOOKUP_TABLES_H_

#include "dex_file.h"
#include "leb128.h"
#include "utf.h"

namespace art {

/**
 * TypeLookupTable used to find class_def_idx by class descriptor quickly.
 * Implementation of TypeLookupTable is based on hash table.
 * This class instantiated at compile time by calling Create() method and written into OAT file.
 * At runtime raw data are readed from memory-mapped file by calling Open() method.
 */
class TypeLookupTable {
 public:
  ~TypeLookupTable();

  uint32_t Size() const {
    return size_;
  }

  // Method returns pointer to binary data of lookup table.
  const uint8_t* RawData() const {
    return reinterpret_cast<const uint8_t*>(entries_);
  }

  // Method returns length of binary data
  uint32_t RawDataLength() const;

  // Method search class_def_idx by class descriptor and it's hash.
  // If no data found then the method returns DexFile::kDexNoIndex
  inline uint32_t Lookup(const char* str, size_t hash) const ALWAYS_INLINE {
    uint32_t mask = size_ - 1;
    uint32_t pos = static_cast<uint32_t>(hash & mask);

    // Thanks to special insertion algorithm
    // element at position pos can be empty or start of bucket
    if (LIKELY(!entries_[pos].IsEmpty() && CmpHashBits(entries_[pos].data, hash) &&
        IsStringsEquals(str, entries_[pos].str_offset))) {
      return GetClassDefIdx(entries_[pos].data);
    }
    if (entries_[pos].IsEmpty() || entries_[pos].IsLast()) {
      return DexFile::kDexNoIndex;
    }
    pos = (pos + entries_[pos].next_pos_delta) & mask;
    const Entry* entry = entries_ + pos;
    while (!entry->IsEmpty()) {
      if (CmpHashBits(entry->data, hash) && IsStringsEquals(str, entry->str_offset)) {
        return GetClassDefIdx(entry->data);
      }
      if (entry->IsLast()) {
        return DexFile::kDexNoIndex;
      }
      pos = (pos + entry->next_pos_delta) & mask;
      entry = entries_ + pos;
    }
    return entry->IsEmpty() ? DexFile::kDexNoIndex : GetClassDefIdx(entry->data);
  }

  // Method creates lookup table for dex file
  static TypeLookupTable* Create(const DexFile& dex_file);

  // Method opens lookup table from binary data. Lookup table does not owns binary data.
  static TypeLookupTable* Open(const uint8_t* raw_data, const DexFile& dex_file);

 private:
   /**
    * To find element we should compare strings.
    * It is faster to compare first hashes and then strings itself.
    * But we have no full hash of element of table. But we can use 2 ideas.
    * 1. All minor bits of hash inside one bucket are equals.
    * 2. If dex file contains N classes and size of hash table is 2^n (where N <= 2^n)
    *    then 16-n bits are free. So we can encode part of element's hash into these bits.
    * So hash of element can be divided on three parts:
    * XXXX XXXX XXXX YYYY YZZZ ZZZZ ZZZZZ
    * Z - a part of hash encoded in bucket (these bits of has are same for all elements in bucket) - n bits
    * Y - a part of hash that we can write into free 16-n bits (because only n bits used to store class_def_idx)
    * X - a part of has that we can't save size increase
    * So data element of Entry used to store class_def_idx and part of hash of entry.
    */
  struct Entry {
    uint32_t str_offset;
    uint16_t data;
    uint16_t next_pos_delta;

    inline bool IsEmpty() const {
      return str_offset == 0;
    }

    inline bool IsLast() const {
      return next_pos_delta == 0;
    }
  };

 private:
  explicit TypeLookupTable(const DexFile& dex_file);

  inline bool IsStringsEquals(const char* str, uint32_t str_offset) const {
    const uint8_t* ptr = dex_file_.Begin() + str_offset;
    DecodeUnsignedLeb128(&ptr);
    return CompareModifiedUtf8ToModifiedUtf8AsUtf16CodePointValues(str, reinterpret_cast<const char*>(ptr)) == 0;
  }

  // Method extracts hash bits from element's data and compare them with
  // the corresponding bits of the specified hash
  inline bool CmpHashBits(uint32_t data, uint32_t hash) const {
    uint32_t mask = static_cast<uint16_t>(~(size_ - 1));
    return (hash & mask) == (data & mask);
  }

  inline uint32_t GetClassDefIdx(uint32_t data) const {
    return data & (size_ - 1);
  }

  static void Fill(const DexFile& dex_file, Entry* entries, uint32_t size);
  static bool SetOnInitialPos(const Entry& entry, uint32_t hash, Entry* entries, uint32_t size);
  static void Insert(const Entry& entry, uint32_t hash, Entry* entries, uint32_t size);
  static uint32_t FindLastEntryInBucket(uint32_t cur_pos, const Entry* entries, uint32_t size);

 private:
  const DexFile& dex_file_;
  uint32_t size_;
  const Entry* entries_;
  bool delete_;
};

}  // namespace art

#endif  // ART_RUNTIME_LOOKUP_TABLES_H_
