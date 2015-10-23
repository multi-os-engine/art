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

OfflineProfilingInfo::OfflineProfilingInfo(const std::list<ArtMethod*>& methods) {
  DCHECK(!methods.empty());
  for (auto method_it = methods.begin(); method_it != methods.end(); method_it++) {
    ArtMethod* method = *method_it;

    const std::string& dex_location = method->GetDexFile()->GetLocation();
    uint32_t dex_location_checksum = method->GetDexFile()->GetLocationChecksum();
    OfflineProfilingInfo::DexLocationKey location_key(dex_location_checksum, dex_location);

    auto info_it = info_.find(location_key);
    if (info_it == info_.end()) {
      info_it = info_.Put(location_key, std::list<uint32_t>());
    }
    DCHECK_EQ(dex_location_checksum, info_it->first.dex_location_checksum);
    info_it->second.push_back(method->GetDexMethodIndex());
  }
}

static int OpenFile(const std::string& filename) {
  // TODO(calin): The file should already be created with the appropriate permissions.
  // If not, use generous permissions to allow easier testing.
  int fd = open(filename.c_str(), O_CREAT | O_WRONLY | O_TRUNC,
      S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
  if (fd < 0) {
    PLOG(WARNING) << "Failed to open profile file " << filename;
    return -1;
  }

  // Lock the file for exclusive access but don't wait if we can't lock it.
  int err = flock(fd, LOCK_EX | LOCK_NB);
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

static constexpr char field_separator = ',';
static constexpr char line_separator = '\n';

/**
 * Serialization format:
 *    dex_location1,dex_location_checksum1,method_id11,method_id12...
 *    dex_location2,dex_location_checksum2,method_id21,method_id22...
 **/
bool OfflineProfilingInfo::Serialize(const std::string& filename) const {
  int fd = OpenFile(filename);
  if (fd == -1) {
    return false;
  }

  // TODO(calin): Merge with a previous existing profile.
  // TODO(calin): Profile this and see how much memory it takes. If too much,
  // write to file directly.
  std::ostringstream os;
  for (auto it = info_.begin(); it != info_.end(); it++) {
    const OfflineProfilingInfo::DexLocationKey& location_key = it->first;
    const std::list<uint32_t>& method_dex_ids = it->second;

    os << location_key.dex_location << field_separator << location_key.dex_location_checksum;

    for (auto method_it = method_dex_ids.begin(); method_it != method_dex_ids.end(); method_it++) {
      os << field_separator << *method_it;
    }
    os << line_separator;
  }

  WriteToFile(fd, os);

  return CloseDescriptorForFile(fd, filename);
}
}  // namespace art
