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

#include "mem_map.h"
#include "mirror/art_method-inl.h"
#include "oat_file-inl.h"

namespace art {
namespace jit {

JitCodeCache* JitCodeCache::Create(size_t capacity) {
  std::string error_msg;
  // Map name specific for android_os_Debug.cpp accounting.
  MemMap* map = MemMap::MapAnonymous("jit-code-cache", nullptr, capacity,
                                     PROT_READ | PROT_WRITE | PROT_EXEC, false, &error_msg);
  bool fallback_mode = false;
  if (map == nullptr) {
    LOG(WARNING) << "Failed to create read write exceute cache: " << error_msg << " size="
        << capacity;
    // If the OS doesn't let us do RWE then we go to the fallback mode that requires thread
    // suspension.
    fallback_mode = true;
    map = MemMap::MapAnonymous("jit-code-cache", nullptr, capacity, PROT_READ | PROT_EXEC, false,
                               &error_msg);
    if (map == nullptr) {
      LOG(FATAL) << "Failed to create read execute code cache: " << error_msg << " size="
          << capacity;
      return nullptr;
    }
  }
  return new JitCodeCache(map, fallback_mode);
}

JitCodeCache::JitCodeCache(MemMap* mem_map, bool fallback_mode)
    : lock_("Jit lock"),
      num_methods_(0),
      fallback_mode_(fallback_mode) {
  LOG(INFO) << "Created jit code cache size=" << PrettySize(mem_map->Size()) << " fallback="
      << fallback_mode;
  mem_map_.reset(mem_map);
  uint8_t* divider = mem_map->Begin() + RoundUp(mem_map->Size() / 4, kPageSize);
  // Data cache is 1 / 4 of the map. TODO: Make this variable?
  // Put data at the start.
  data_cache_ptr_ = mem_map->Begin();
  data_cache_end_ = divider;
  data_cache_begin_ = data_cache_ptr_;
  mprotect(data_cache_ptr_, data_cache_end_ - data_cache_begin_, PROT_READ | PROT_WRITE);
  // Code cache after.
  code_cache_begin_ = divider;
  code_cache_ptr_ = divider;
  code_cache_end_ = mem_map->End();
}

bool JitCodeCache::ContainsMethod(mirror::ArtMethod* method) const {
  const auto* const code_ptr = method->GetEntryPointFromQuickCompiledCode();
  return code_ptr >= code_cache_begin_ && code_ptr < code_cache_end_;
}

void JitCodeCache::FlushInstructionCache() {
  // TODO: Implement
  // __clear_cache(reinterpret_cast<char*>(code_mem_map_->Begin()), static_cast<int>(CodeCacheSize()));
}

uint8_t* JitCodeCache::ReserveCode(Thread* self, size_t size) {
  MutexLock mu(self, lock_);
  CHECK_LE(size, CodeCacheRemain()) << "Out of space in the code cache";
  code_cache_ptr_ += size;
  return code_cache_ptr_ - size;
}

void JitCodeCache::EnableCodeWriting(Thread* self, uint8_t* begin, uint8_t* end) {
  Locks::mutator_lock_->AssertExclusiveHeld(self);
  begin = AlignDown(begin, kPageSize);
  end = AlignUp(begin, kPageSize);
  mprotect(begin, end - begin, PROT_READ | PROT_WRITE);
}

void JitCodeCache::EnableCodeRunning(Thread* self, uint8_t* begin, uint8_t* end) {
  Locks::mutator_lock_->AssertExclusiveHeld(self);
  begin = AlignDown(begin, kPageSize);
  end = AlignUp(begin, kPageSize);
  mprotect(begin, end - begin, PROT_READ | PROT_EXEC);
}

uint8_t* JitCodeCache::AddDataArray(Thread* self, const uint8_t* begin, const uint8_t* end) {
  MutexLock mu(self, lock_);
  const size_t size = end - begin;
  if (size > DataCacheRemain()) {
    return nullptr;  // Out of space in the data cache.
  }
  std::copy(begin, end, data_cache_ptr_);
  data_cache_ptr_ += size;
  return data_cache_ptr_ - size;
}

}  // namespace jit
}  // namespace art
