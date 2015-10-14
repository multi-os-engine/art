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

#include <memory>
#include <cstring>

#include "dex_file-inl.h"
#include "lookup_table.h"
#include "utils.h"

namespace art {

namespace {

uint16_t MakeData(uint16_t class_def_idx, uint32_t hash, uint32_t size) {
  uint16_t hash_mask = static_cast<uint16_t>(~(size - 1));
  return (static_cast<uint16_t>(hash) & hash_mask) | class_def_idx;
}

}

TypeLookupTable::~TypeLookupTable() {
  if (delete_) {
    delete[] entries_;
  }
}

uint32_t TypeLookupTable::RawDataLength() const {
  return size_ * sizeof(Entry);
}

TypeLookupTable* TypeLookupTable::Create(const DexFile& dex_file) {
  if (dex_file.NumClassDefs() > std::numeric_limits<uint16_t>::max()) {
    return nullptr;
  }
  std::unique_ptr<TypeLookupTable> table(new TypeLookupTable(dex_file));
  size_t capacity = RoundUpToPowerOfTwo(dex_file.NumClassDefs());
  std::unique_ptr<Entry> entries(new Entry[capacity]);
  memset(entries.get(), 0, capacity * sizeof(Entry));
  Fill(dex_file, entries.get(), capacity);
  table->size_ = capacity;
  table->entries_ = entries.release();
  table->delete_ = true;
  return table.release();
}

TypeLookupTable* TypeLookupTable::Open(const uint8_t* raw_data, const DexFile& dex_file) {
  std::unique_ptr<TypeLookupTable> table(new TypeLookupTable(dex_file));
  table->size_ = RoundUpToPowerOfTwo(dex_file.NumClassDefs());
  table->entries_ = reinterpret_cast<const Entry*>(raw_data);
  table->delete_ = false;
  return table.release();
}

TypeLookupTable::TypeLookupTable(const DexFile& dex_file): dex_file_(dex_file) {
}

void TypeLookupTable::Fill(const DexFile& dex_file, Entry* entries, uint32_t size) {
  std::vector<uint16_t> class_def_ids;
  // The first stage. Put elements on their initial positions.
  // If an initial position is already occupied then
  // delay the insertion of the element on the second stage
  for (size_t i = 0; i < dex_file.NumClassDefs(); ++i) {
    const DexFile::ClassDef& class_def = dex_file.GetClassDef(i);
    const DexFile::TypeId& type_id = dex_file.GetTypeId(class_def.class_idx_);
    const DexFile::StringId& str_id = dex_file.GetStringId(type_id.descriptor_idx_);
    uint32_t hash = ComputeModifiedUtf8Hash(dex_file.GetStringData(str_id));
    Entry entry;
    entry.str_offset = str_id.string_data_off_;
    entry.data = MakeData(i, hash, size);
    if (!SetOnInitialPos(entry, hash, entries, size)) {
        class_def_ids.push_back(i);
    }
  }
  // The second stage. The initial position of these elements are occupied.
  // So we have a collision. Put these elements into the nearest free cells and link their.
  for (uint16_t class_def_idx : class_def_ids) {
    const DexFile::ClassDef& class_def = dex_file.GetClassDef(class_def_idx);
    const DexFile::TypeId& type_id = dex_file.GetTypeId(class_def.class_idx_);
    const DexFile::StringId& str_id = dex_file.GetStringId(type_id.descriptor_idx_);
    uint32_t hash = ComputeModifiedUtf8Hash(dex_file.GetStringData(str_id));
    Entry entry;
    entry.str_offset = str_id.string_data_off_;
    entry.data = MakeData(class_def_idx, hash, size);
    Insert(entry, hash, entries, size);
  }
}

bool TypeLookupTable::SetOnInitialPos(const Entry& entry, uint32_t hash, Entry* entries, uint32_t size) {
  uint32_t mask = size - 1;
  uint32_t pos = hash & mask;
  if (entries[pos].str_offset != 0) {
    return false;
  }
  entries[pos] = entry;
  entries[pos].next_pos_delta = 0;
  return true;
}

void TypeLookupTable::Insert(const Entry& entry, uint32_t hash, Entry* entries, uint32_t size) {
  uint32_t mask = size - 1;
  uint32_t pos = hash & mask;
  pos = FindLastEntryInBucket(pos, entries, size);
  uint32_t next_pos = (pos + 1) & mask;
  while (entries[next_pos].str_offset != 0) {
    next_pos = (next_pos + 1) & mask;
  }
  uint32_t delta;
  if (next_pos >= pos) {
    delta = next_pos - pos;
  } else {
    delta = next_pos + size - pos;
  }
  entries[pos].next_pos_delta = delta;
  entries[next_pos] = entry;
  entries[next_pos].next_pos_delta = 0;
}

uint32_t TypeLookupTable::FindLastEntryInBucket(uint32_t pos, const Entry* entries, uint32_t size) {
  if (entries[pos].next_pos_delta == 0) {
    return pos;
  }

  uint32_t mask = size - 1;
  pos = (pos + entries[pos].next_pos_delta) & mask;
  const Entry* entry = entries + pos;
  while (entry->next_pos_delta != 0) {
    pos = (pos + entry->next_pos_delta) & mask;
    entry = entries + pos;
  }
  return pos;
}

}  // namespace art
