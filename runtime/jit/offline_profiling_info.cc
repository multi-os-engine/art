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

#include "offline_profiling_info.h"

#include <fstream>
#include <list>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include "art_method-inl.h"
#include "base/mutex.h"
#include "jit/profiling_info.h"
#include "safe_map.h"
#include "utils.h"

namespace art {

// We don't need or use the class information for now.
static constexpr bool kSaveClassInfo = false;

static uint32_t FindIndexWithCache(const std::string& dex_location,
                                   SafeMap<std::string, uint32_t>& dex_locations_to_index,
                                   std::vector<std::string>& dex_locations) {
  uint32_t dex_location_index;
  auto it = dex_locations_to_index.find(dex_location);
  if (it == dex_locations_to_index.end()) {
    dex_location_index = dex_locations.size();
    dex_locations.push_back(dex_location);
    dex_locations_to_index.Put(dex_location, dex_location_index);
  } else {
    dex_location_index = it->second;
  }
  return dex_location_index;
}

OfflineProfilingInfo::OfflineProfilingInfo(const std::list<ArtMethod*>& methods) {
  SafeMap<std::string, uint32_t> dex_locations_to_index;

  for (auto method_it = methods.begin(); method_it != methods.end(); method_it++) {
    ArtMethod* method = *method_it;
    ProfilingInfo* info = method->GetProfilingInfo(sizeof(void*));
    if (info == nullptr) {
      continue;
    }
    uint32_t dex_location_index = FindIndexWithCache(
        method->GetDexFile()->GetLocation(), dex_locations_to_index, dex_locations_);

    OfflineProfilingInfo::MethodKey method_key(dex_location_index,
                                               method->GetDexMethodIndex());

    auto method_info_it = info_.Put(method_key, OfflineProfilingInfo::MethodInfo());
    OfflineProfilingInfo::MethodInfo& method_info = method_info_it->second;
    uint32_t number_of_inlined_caches = info->GetNumberOfInlinedCaches();
    const ProfilingInfo::InlineCache* inline_caches = info->GetInlineCache();

    if (kSaveClassInfo) {
      // TODO(calin): This is failing. Investigate why.
      ScopedObjectAccess soa(Thread::Current());  // For accessing the gc root.
      for (uint32_t cache_idx = 0; cache_idx < number_of_inlined_caches; cache_idx++) {
        const ProfilingInfo::InlineCache& inline_cache = inline_caches[cache_idx];
        uint16_t number_of_classes = inline_cache.GetNumberOfClasses();
        const GcRoot<mirror::Class>* classes = inline_cache.GetClasses();
        std::list<OfflineProfilingInfo::ClassKey> class_keys;

        for (uint16_t j = 0; j < number_of_classes; j++) {
          GcRoot<mirror::Class> clazz = classes[j];
          uint32_t class_dex_location_index = FindIndexWithCache(
              clazz.Read()->GetDexFile().GetLocation(),
              dex_locations_to_index,
              dex_locations_);
          class_keys.emplace_back(class_dex_location_index, clazz.Read()->GetDexTypeIndex());
        }
        method_info.Put(inline_cache.GetDexPc(), class_keys);
      }
    }
  }
}

static int OpenFile(const std::string& filename) {
  // TODO(calin): The file should already be created with the appropriate permissions.
  // If not, use generous permissions to allow easier testing.
  int fd = open(filename.c_str(), O_CREAT | O_WRONLY | O_TRUNC, S_IRWXO);
  if (fd < 0) {
    PLOG(WARNING) << "Failed to open profile file " << filename;
    return -1;
  }

  // Lock the file for exclusive access.
  // This will block if another process is using the file.
  int err = flock(fd, LOCK_EX);
  if (err < 0) {
    PLOG(WARNING) << "Failed to lock profile file " << filename;
    return -1;
  }

  return fd;
}

static bool CloseDescriptorForFile(int fd, const std::string& filename) {
  // Now unlock the file, allowing another process in.
  int err = flock(fd, LOCK_UN);
  if (err < 0) {
    PLOG(WARNING) << "Failed to unlock profile file " << filename;
    return false;
  }

  // Done, close the file.
  err = ::close(fd);
  if (err < 0) {
    PLOG(WARNING) << "Failed to close descriptor for profile file" << filename;
    return false;
  }

  return true;
}

static void WriteToFile(int fd, const std::ostringstream& os) {
  std::string data(os.str());
  const char *p = data.c_str();
  size_t length = data.length();
  do {
    int n = ::write(fd, p, length);
    p += n;
    length -= n;
  } while (length > 0);
}

/**
 * Serialization format:
 *
 * dex_file_count
 *   dex_location_1
 *   ...
 *   dex_location_n
 * method_info_count
 *   method_1_dex_index, method_1_dex_location_index, method_1_nr_of_dex_pcs
 *     dex_pc_1
 *       class_1_dex_index, class_1_dex_location_index
 *       ...
 *       class_n_dex_index, class_n_dex_location_index
 *     ...
 *     dex_pc_n
 *       ...
 *  method_n_dex_index, method_n_dex_location_index, method_n_nr_of_dex_pcs\
 *    ...
 **/
bool OfflineProfilingInfo::Serialize(const std::string& filename) const {
  int fd = OpenFile(filename);
  if (fd == -1) {
    return false;
  }

  // Format the profile output and write to the file.

  // TODO(calin): Should we bother merging with a previous existing profile?
  // TODO(calin): profile this and see how much memory it takes. If too much
  // write to file directly.
  std::ostringstream os;
  os << dex_locations_.size() << '\n';
  for (auto it = dex_locations_.begin(); it != dex_locations_.end(); it++) {
    os << *it << '\n';
  }
  os << info_.size() << '\n';
  for (auto it = info_.begin(); it != info_.end(); it++) {
    const OfflineProfilingInfo::MethodKey& method_key = it->first;
    const OfflineProfilingInfo::MethodInfo& method_info = it->second;

    os << method_key.dex_method_index << ','
      << method_key.dex_location_index << ','
      << method_info.size() << '\n';

    for (auto info_it = method_info.begin(); info_it != method_info.end(); info_it++) {
      const std::list<ClassKey>& classes = info_it->second;
      os << info_it->first << ','
        << classes.size() << '\n';
      for (auto class_it = classes.begin(); class_it != classes.end(); class_it++) {
        os << class_it->dex_class_index << ','
          << class_it->dex_location_index << '\n';
      }
    }
  }

  WriteToFile(fd, os);

  return CloseDescriptorForFile(fd, filename);
}
}  // namespace art
