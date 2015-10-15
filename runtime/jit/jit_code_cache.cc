/*
 * Copyright 2014 The Android Open Source Project
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

#include "jit_code_cache.h"

#include <sstream>

#include "art_method-inl.h"
#include "mem_map.h"
#include "oat_file-inl.h"

namespace art {
namespace jit {

JitCodeCache* JitCodeCache::Create(size_t capacity, std::string* error_msg) {
  CHECK_GT(capacity, 0U);
  CHECK_LT(capacity, kMaxCapacity);
  std::string error_str;
  // Map name specific for android_os_Debug.cpp accounting.
  MemMap* map = MemMap::MapAnonymous(
      "jit-code-cache", nullptr, capacity, PROT_READ | PROT_WRITE | PROT_EXEC, false, false, &error_str);
  if (map == nullptr) {
    std::ostringstream oss;
    oss << "Failed to create read write execute cache: " << error_str << " size=" << capacity;
    *error_msg = oss.str();
    return nullptr;
  }
  return new JitCodeCache(map);
}

JitCodeCache::JitCodeCache(MemMap* mem_map)
    : lock_("Jit code cache", kJitCodeCacheLock), num_methods_(0) {
  VLOG(jit) << "Created jit code cache size=" << PrettySize(mem_map->Size());
  // Data cache is 1 / 4 of the map. TODO: Make this variable?
  size_t data_size = RoundUp(mem_map->Size() / 4, kPageSize);
  size_t code_size = mem_map->Size() - data_size;
  uint8_t* divider = mem_map->Begin() + data_size;

  MemMap* dummy = MemMap::MapDummy("jit-code-cache", divider, code_size);
  code_space_.reset(gc::space::DlMallocSpace::CreateFromMemMap(
      dummy, "jit-code-cache", code_size, code_size, code_size, code_size, true, false));

  mprotect(dummy->Begin(), code_size, PROT_READ | PROT_EXEC);

  dummy =  MemMap::MapDummy("jit-data-cache", mem_map->Begin(), mem_map->Size() - code_size);
  // No need for bitmaps for the data cache, the compiled code knows what to free.
  data_space_.reset(gc::space::DlMallocSpace::CreateFromMemMap(
      dummy, "jit-data-cache", data_size, data_size, data_size, data_size, false, false));

  mprotect(dummy->Begin(), data_size, PROT_READ | PROT_WRITE);
}

bool JitCodeCache::ContainsMethod(ArtMethod* method) const {
  return ContainsCodePtr(method->GetEntryPointFromQuickCompiledCode());
}

bool JitCodeCache::ContainsCodePtr(const void* ptr) const {
  return code_space_->Contains(reinterpret_cast<mirror::Object*>(const_cast<void*>(ptr)));
}

uint8_t* JitCodeCache::CommitCode(Thread* self,
                                  const uint8_t* mapping_table,
                                  const uint8_t* vmap_table,
                                  const uint8_t* gc_map,
                                  size_t frame_size_in_bytes,
                                  size_t core_spill_mask,
                                  size_t fp_spill_mask,
                                  const uint8_t* code,
                                  size_t code_size) {
  size_t bytes_allocated, usable_size, bytes_tl_bulk_allocated;
  size_t total_size = RoundUp(sizeof(OatQuickMethodHeader) + code_size + 32, sizeof(void*));

  MutexLock mu(self, lock_);
  // TODO(ngeoffray): Only set the write bit to the memory allocated.
  mprotect(code_space_->Begin(), code_space_->Capacity(), PROT_READ | PROT_WRITE | PROT_EXEC);
  uint8_t* result = reinterpret_cast<uint8_t*>(code_space_->AllocNonvirtual(
      self, total_size, &bytes_allocated, &usable_size, &bytes_tl_bulk_allocated));

  if (result == nullptr) {
    return nullptr;
  }

  uint8_t* code_ptr = reinterpret_cast<uint8_t*>(
      RoundUp(reinterpret_cast<size_t>(result + sizeof(OatQuickMethodHeader)),
              GetInstructionSetAlignment(kRuntimeISA)));

  std::copy(code, code + code_size, code_ptr);
  OatQuickMethodHeader* method_header = reinterpret_cast<OatQuickMethodHeader*>(code_ptr) - 1;
  new (method_header) OatQuickMethodHeader(
      (mapping_table == nullptr) ? 0 : code_ptr - mapping_table,
      (vmap_table == nullptr) ? 0 : code_ptr - vmap_table,
      (gc_map == nullptr) ? 0 : code_ptr - gc_map,
      frame_size_in_bytes,
      core_spill_mask,
      fp_spill_mask,
      code_size);
  mprotect(code_space_->Begin(), code_space_->Capacity(), PROT_READ | PROT_EXEC);

  __builtin___clear_cache(reinterpret_cast<char*>(code_ptr),
                          reinterpret_cast<char*>(code_ptr + code_size));

  ++num_methods_;  // TODO: This is hacky but works since each method has exactly one code region.
  return reinterpret_cast<uint8_t*>(method_header);
}

uint8_t* JitCodeCache::ReserveData(Thread* self, size_t size) {
  size = RoundUp(size, sizeof(void*));
  size_t bytes_allocated, usable_size, bytes_tl_bulk_allocated;
  uint8_t* result = reinterpret_cast<uint8_t*>(data_space_->AllocNonvirtual(
      self, size, &bytes_allocated, &usable_size, &bytes_tl_bulk_allocated));
  return result;
}

uint8_t* JitCodeCache::AddDataArray(Thread* self, const uint8_t* begin, const uint8_t* end) {
  uint8_t* result = ReserveData(self, end - begin);
  if (result == nullptr) {
    return nullptr;  // Out of space in the data cache.
  }
  std::copy(begin, end, result);
  return result;
}

const void* JitCodeCache::GetCodeFor(ArtMethod* method) {
  const void* code = method->GetEntryPointFromQuickCompiledCode();
  if (ContainsCodePtr(code)) {
    return code;
  }
  MutexLock mu(Thread::Current(), lock_);
  auto it = method_code_map_.find(method);
  if (it != method_code_map_.end()) {
    return it->second;
  }
  return nullptr;
}

void JitCodeCache::SaveCompiledCode(ArtMethod* method, const void* old_code_ptr) {
  DCHECK_EQ(method->GetEntryPointFromQuickCompiledCode(), old_code_ptr);
  DCHECK(ContainsCodePtr(old_code_ptr)) << PrettyMethod(method) << " old_code_ptr="
      << old_code_ptr;
  MutexLock mu(Thread::Current(), lock_);
  auto it = method_code_map_.find(method);
  if (it != method_code_map_.end()) {
    return;
  }
  method_code_map_.Put(method, old_code_ptr);
}

}  // namespace jit
}  // namespace art
