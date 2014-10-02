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

#ifndef ART_RUNTIME_JIT_JIT_CODE_CACHE_H_
#define ART_RUNTIME_JIT_JIT_CODE_CACHE_H_

#include <unordered_map>

#include "instrumentation.h"

#include "atomic.h"
#include "base/macros.h"
#include "base/mutex.h"
#include "gc_root.h"
#include "jni.h"
#include "oat_file.h"
#include "object_callbacks.h"
#include "thread_pool.h"

namespace art {

class CompiledMethod;
class CompilerCallbacks;

namespace mirror {
class ArtMethod;
}  // namespcae mirror

namespace jit {

class JitInstrumentationCache;

class JitCodeCache {
 public:
  static JitCodeCache* Create(size_t capacity);

  uint8_t* CodeCachePtr() {
    return code_cache_ptr_;
  }
  size_t CodeCacheSize() const {
    return code_cache_ptr_ - code_cache_begin_;
  }
  size_t CodeCacheRemain() const {
    return code_cache_end_ - code_cache_ptr_;
  }
  size_t DataCacheSize() const {
    return data_cache_ptr_ - data_cache_begin_;
  }
  size_t DataCacheRemain() const {
    return data_cache_end_ - data_cache_ptr_;
  }
  size_t NumMethods() const {
    return num_methods_;
  }

  bool ContainsMethod(mirror::ArtMethod* method) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Whether or not we require suspending all threads before adding code to the code cache.
  bool ModifyingCodeRequiresSuspension() const {
    return fallback_mode_;
  }

  uint8_t* ReserveCode(Thread* self, size_t size) LOCKS_EXCLUDED(lock_);

  uint8_t* AddDataArray(Thread* self, const uint8_t* begin, const uint8_t* end)
      LOCKS_EXCLUDED(lock_);

  void EnableCodeWriting(Thread* self, uint8_t* begin, uint8_t* end)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::mutator_lock_);

  void EnableCodeRunning(Thread* self, uint8_t* begin, uint8_t* end)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::mutator_lock_);

 private:
  // Takes ownership of code_mem_map.
  JitCodeCache(MemMap* code_mem_map, bool fallback_mode);
  void FlushInstructionCache();

  Mutex lock_;
  // Mem map which holds code and data. We do this since we need to have 32 bit offsets from method
  // headers in code cache which point to things in the data cache. If the maps are more than 4GB
  // apart, having multiple maps wouldn't work.
  std::unique_ptr<MemMap> mem_map_;
  // Code cache section.
  uint8_t* code_cache_ptr_;
  const uint8_t* code_cache_begin_;
  const uint8_t* code_cache_end_;
  // Data cache section.
  uint8_t* data_cache_ptr_;
  const uint8_t* data_cache_begin_;
  const uint8_t* data_cache_end_;
  size_t num_methods_;
  // Fallback mode means that we can't have read write execute memory blobs.
  bool fallback_mode_;

  DISALLOW_COPY_AND_ASSIGN(JitCodeCache);
};


}  // namespace jit
}  // namespace art

#endif  // ART_RUNTIME_JIT_JIT_CODE_CACHE_H_
