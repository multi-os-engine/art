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

#include <list>

#include "safe_map.h"

namespace art {

class ArtMethod;

// Profiling information collected during a JIT in a format that can be serialized to disk.
class OfflineProfilingInfo {
 public:
  explicit OfflineProfilingInfo(const std::list<ArtMethod*>& methods);

  bool Serialize(const std::string& filename) const;
  bool IsEmpty() const { return info_.empty(); }

 private:
  struct MethodKey {
    MethodKey(uint32_t location_index, uint32_t method_index)
        : dex_location_index(location_index), dex_method_index(method_index) {}
    uint32_t dex_location_index;
    uint32_t dex_method_index;
  };

  struct ClassKey {
    ClassKey(uint32_t location_index, uint32_t method_index)
        : dex_location_index(location_index), dex_class_index(method_index) {}
    uint32_t dex_location_index;
    uint32_t dex_class_index;
  };

  typedef SafeMap<uint32_t, std::list<ClassKey>> MethodInfo;

  struct MethodKeyComparator {
    bool operator()(MethodKey mk1, MethodKey mk2) const {
      if (mk1.dex_location_index == mk2.dex_location_index) {
        return mk1.dex_method_index < mk2.dex_method_index;
      } else {
        return mk1.dex_location_index < mk2.dex_location_index;
      }
    }
  };

  std::vector<std::string> dex_locations_;
  SafeMap<MethodKey, MethodInfo, OfflineProfilingInfo::MethodKeyComparator> info_;
};

}  // namespace art

#endif  // ART_RUNTIME_JIT_OFFLINE_PROFILING_INFO_H_
