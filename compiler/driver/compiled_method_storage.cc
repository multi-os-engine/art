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

#include <ostream>

#include "compiled_method_storage.h"

#include "base/logging.h"
#include "compiled_method.h"
#include "thread-inl.h"
#include "utils.h"
#include "utils/swap_space.h"

namespace art {

template <typename T>
const LengthPrefixedArray<T>* CompiledMethodStorage::CopyArray(const ArrayRef<const T>& array) {
  DCHECK(!array.empty());
  SwapAllocator<uint8_t> allocator(GetSwapSpaceAllocator());
  void* storage = allocator.allocate(LengthPrefixedArray<T>::ComputeSize(array.size()));
  LengthPrefixedArray<T>* copy = new(storage) LengthPrefixedArray<T>(array.size());
  memcpy(&copy->At(0), array.data(), array.size() * sizeof(T));
  return copy;
}

// Explicitly instantiate for the types we need to avoid a definition in the header file.
template const LengthPrefixedArray<uint8_t>* CompiledMethodStorage::CopyArray(
    const ArrayRef<const uint8_t>& array);
template const LengthPrefixedArray<LinkerPatch>* CompiledMethodStorage::CopyArray(
    const ArrayRef<const LinkerPatch>& array);
template const LengthPrefixedArray<SrcMapElem>* CompiledMethodStorage::CopyArray(
    const ArrayRef<const SrcMapElem>& array);

template <typename T>
void CompiledMethodStorage::ReleaseArray(const LengthPrefixedArray<T>* array) {
  SwapAllocator<uint8_t> allocator(GetSwapSpaceAllocator());
  size_t size = LengthPrefixedArray<T>::ComputeSize(array->size());
  array->~LengthPrefixedArray<T>();
  allocator.deallocate(const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(array)), size);
}

// Explicitly instantiate for the types we need to avoid a definition in the header file.
template void CompiledMethodStorage::ReleaseArray(const LengthPrefixedArray<uint8_t>* array);
template void CompiledMethodStorage::ReleaseArray(const LengthPrefixedArray<LinkerPatch>* array);
template void CompiledMethodStorage::ReleaseArray(const LengthPrefixedArray<SrcMapElem>* array);

template <typename T, typename DedupeSetType>
inline const LengthPrefixedArray<T>* CompiledMethodStorage::AllocateOrDeduplicateArray(
    const ArrayRef<const T>& data,
    DedupeSetType* dedupe_set) {
  if (data.empty()) {
    return nullptr;
  } else if (!DedupeEnabled()) {
    return CopyArray(data);
  } else {
    return dedupe_set->Add(Thread::Current(), data);
  }
}

template <typename T>
inline void CompiledMethodStorage::ReleaseArrayIfNotDeduplicated(const LengthPrefixedArray<T>* array) {
  if (array != nullptr && !DedupeEnabled()) {
    ReleaseArray(array);
  }
}

CompiledMethodStorage::CompiledMethodStorage(int swap_fd)
    : swap_space_(swap_fd == -1 ? nullptr : new SwapSpace(swap_fd, 10 * MB)),
      swap_space_allocator_(new SwapAllocator<void>(swap_space_.get())),
      dedupe_enabled_(true),
      dedupe_code_("dedupe code", LengthPrefixedArrayAlloc<uint8_t>(this)),
      dedupe_src_mapping_table_("dedupe source mapping table",
                                LengthPrefixedArrayAlloc<SrcMapElem>(this)),
      dedupe_mapping_table_("dedupe mapping table", LengthPrefixedArrayAlloc<uint8_t>(this)),
      dedupe_vmap_table_("dedupe vmap table", LengthPrefixedArrayAlloc<uint8_t>(this)),
      dedupe_gc_map_("dedupe gc map", LengthPrefixedArrayAlloc<uint8_t>(this)),
      dedupe_cfi_info_("dedupe cfi info", LengthPrefixedArrayAlloc<uint8_t>(this)),
      dedupe_linker_patches_("dedupe cfi info", LengthPrefixedArrayAlloc<LinkerPatch>(this)) {
}

CompiledMethodStorage::~CompiledMethodStorage() {
  // All done by member constructors.
}

void CompiledMethodStorage::DumpMemoryUsage(std::ostream& os, bool extended) const {
  if (swap_space_.get() != nullptr) {
    os << " swap=" << PrettySize(swap_space_->GetSize());
  }
  if (extended) {
    os << "\nCode dedupe: " << dedupe_code_.DumpStats();
    os << "\nMapping table dedupe: " << dedupe_mapping_table_.DumpStats();
    os << "\nVmap table dedupe: " << dedupe_vmap_table_.DumpStats();
    os << "\nGC map dedupe: " << dedupe_gc_map_.DumpStats();
    os << "\nCFI info dedupe: " << dedupe_cfi_info_.DumpStats();
  }
}

const LengthPrefixedArray<uint8_t>* CompiledMethodStorage::DeduplicateCode(
    const ArrayRef<const uint8_t>& code) {
  return AllocateOrDeduplicateArray(code, &dedupe_code_);
}

void CompiledMethodStorage::ReleaseCode(const LengthPrefixedArray<uint8_t>* code) {
  ReleaseArrayIfNotDeduplicated(code);
}

const LengthPrefixedArray<SrcMapElem>* CompiledMethodStorage::DeduplicateSrcMappingTable(
    const ArrayRef<const SrcMapElem>& src_map) {
  return AllocateOrDeduplicateArray(src_map, &dedupe_src_mapping_table_);
}

void CompiledMethodStorage::ReleaseSrcMappingTable(const LengthPrefixedArray<SrcMapElem>* src_map) {
  ReleaseArrayIfNotDeduplicated(src_map);
}

const LengthPrefixedArray<uint8_t>* CompiledMethodStorage::DeduplicateMappingTable(
    const ArrayRef<const uint8_t>& table) {
  return AllocateOrDeduplicateArray(table, &dedupe_mapping_table_);
}

void CompiledMethodStorage::ReleaseMappingTable(const LengthPrefixedArray<uint8_t>* table) {
  ReleaseArrayIfNotDeduplicated(table);
}

const LengthPrefixedArray<uint8_t>* CompiledMethodStorage::DeduplicateVMapTable(
    const ArrayRef<const uint8_t>& table) {
  return AllocateOrDeduplicateArray(table, &dedupe_vmap_table_);
}

void CompiledMethodStorage::ReleaseVMapTable(const LengthPrefixedArray<uint8_t>* table) {
  ReleaseArrayIfNotDeduplicated(table);
}

const LengthPrefixedArray<uint8_t>* CompiledMethodStorage::DeduplicateGCMap(
    const ArrayRef<const uint8_t>& gc_map) {
  return AllocateOrDeduplicateArray(gc_map, &dedupe_gc_map_);
}

void CompiledMethodStorage::ReleaseGCMap(const LengthPrefixedArray<uint8_t>* gc_map) {
  ReleaseArrayIfNotDeduplicated(gc_map);
}

const LengthPrefixedArray<uint8_t>* CompiledMethodStorage::DeduplicateCFIInfo(
    const ArrayRef<const uint8_t>& cfi_info) {
  return AllocateOrDeduplicateArray(cfi_info, &dedupe_cfi_info_);
}

void CompiledMethodStorage::ReleaseCFIInfo(const LengthPrefixedArray<uint8_t>* cfi_info) {
  ReleaseArrayIfNotDeduplicated(cfi_info);
}

const LengthPrefixedArray<LinkerPatch>* CompiledMethodStorage::DeduplicateLinkerPatches(
    const ArrayRef<const LinkerPatch>& linker_patches) {
  return AllocateOrDeduplicateArray(linker_patches, &dedupe_linker_patches_);
}

void CompiledMethodStorage::ReleaseLinkerPatches(
    const LengthPrefixedArray<LinkerPatch>* linker_patches) {
  ReleaseArrayIfNotDeduplicated(linker_patches);
}

}  // namespace art
