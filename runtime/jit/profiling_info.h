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

#ifndef ART_RUNTIME_JIT_PROFILING_INFO_H_
#define ART_RUNTIME_JIT_PROFILING_INFO_H_

#include "base/hash_map.h"

namespace art {

class ArtMethod;

namespace mirror {
  class Class;
}

class ProfilingInfo {
 public:
  static ProfilingInfo* Create(ArtMethod* method);
  size_t CacheSize() const {
    return dex_pc_to_cache_entry_.Size();
  }
  GcRoot<mirror::Class>& CacheAt(size_t i) {
    return cache_[i];
  }
  void AddInvokeInfo(Thread* self, uint32_t dex_pc, mirror::Class* cls);

  static constexpr uint16_t kIndividualCacheSize = 3;

 private:
  // EmptyFn implementation for art::HashMap
  struct EmptyFn {
    void MakeEmpty(std::pair<uint32_t, uint32_t>& item) const {
      item.first = -1;
    }
    bool IsEmpty(const std::pair<uint32_t, uint32_t>& item) const {
      return item.first == static_cast<uint32_t>(-1);
    }
  };

  using DexPcToCache = HashMap<uint32_t, uint32_t, EmptyFn>;

  ProfilingInfo(const DexPcToCache& temp_map, std::pair<uint32_t, uint32_t>* map_data)
      : dex_pc_to_cache_entry_(temp_map) {
    memset(&cache_, 0, temp_map.Size() * kIndividualCacheSize * sizeof(GcRoot<mirror::Class>));
    memcpy(map_data,
           temp_map.GetData(),
           sizeof(std::pair<uint32_t, uint32_t>) * temp_map.NumBuckets());
    dex_pc_to_cache_entry_.SetData(map_data, /* owns_data */ false);
  }

  DexPcToCache dex_pc_to_cache_entry_;
  GcRoot<mirror::Class> cache_[0];

  DISALLOW_COPY_AND_ASSIGN(ProfilingInfo);
};

}  // namespace art

#endif  // ART_RUNTIME_JIT_PROFILING_INFO_H_
