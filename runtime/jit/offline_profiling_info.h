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

#ifndef ART_RUNTIME_JIT_OFFLINE_PROFILING_INFO_H_
#define ART_RUNTIME_JIT_OFFLINE_PROFILING_INFO_H_

#include <set>

#include "safe_map.h"

namespace art {

class ArtMethod;

/**
 * Profiling information in a format that can be serialized to disk.
 * It is a serialize-friendly format based on information collected
 * by the interpreter (ProfileInfo).
 * Currently it stores only the hot compiled methods.
 */
class OfflineProfilingInfo {
 public:
  void AddMethodInfo(ArtMethod* method) SHARED_REQUIRES(Locks::mutator_lock_);

  bool HasData() const { return !info_.empty(); }
  bool Serialize(const std::string& filename) const;

 private:
  struct DexLocationKey {
    DexLocationKey(uint32_t checksum, const std::string& location)
        : dex_location_checksum(checksum), dex_location(location) {}
    uint32_t dex_location_checksum;
    std::string dex_location;
  };

  struct DexLocationKeyComparator {
    bool operator()(DexLocationKey dlk1, DexLocationKey dlk2) const {
      return dlk1.dex_location < dlk2.dex_location;
    }
  };

  // Profiling information.
  // (dex_file_location, dex_location_checksum) -> [dex_method_index]+
  SafeMap<DexLocationKey, std::set<uint32_t>, DexLocationKeyComparator> info_;
};

}  // namespace art

#endif  // ART_RUNTIME_JIT_OFFLINE_PROFILING_INFO_H_
