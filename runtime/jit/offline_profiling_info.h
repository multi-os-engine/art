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

#include "atomic.h"
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
  bool NeedsSaving(uint64_t last_update_time_ns) const;
  void SaveMethodsInfo(const std::string& filename,
                       uint64_t last_update_time_ns,
                       const std::set<ArtMethod*>& methods);

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

  // Map identifying the location of the profiled methods.
  // (dex_file_location, dex_location_checksum) -> [dex_method_index]+
  using MethodInfo = SafeMap<DexLocationKey, std::set<uint32_t>, DexLocationKeyComparator>;

  void AddMethodInfo(ArtMethod* method, MethodInfo& info)
      SHARED_REQUIRES(Locks::mutator_lock_);
  bool Serialize(const std::string& filename, const MethodInfo& info) const;

  // TODO(calin): Verify if Atomic is really needed (are we sure to be called from a
  // singe thread?)
  Atomic<uint64_t> last_update_time_ns_;
};

}  // namespace art

#endif  // ART_RUNTIME_JIT_OFFLINE_PROFILING_INFO_H_
