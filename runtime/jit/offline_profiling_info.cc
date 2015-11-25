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
#include <set>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include "art_method-inl.h"
#include "base/mutex.h"
#include "jit/profiling_info.h"
#include "safe_map.h"
#include "utils.h"

namespace art {

// An arbitrary value to throttle save requests. Set to 500ms for now.
static constexpr const uint64_t kMilisecondsToNano = 1000000;
static constexpr const uint64_t kMinimumTimeBetweenSavesNs = 500 * kMilisecondsToNano;

bool OfflineProfilingInfo::NeedsSaving(uint64_t last_update_time_ns) const {
  return last_update_time_ns - last_update_time_ns_.LoadRelaxed() > kMinimumTimeBetweenSavesNs;
}

void OfflineProfilingInfo::SaveProfilingInfo(const std::string& filename,
                                             uint64_t last_update_time_ns,
                                             const std::set<ArtMethod*>& methods) {
  if (!NeedsSaving(last_update_time_ns)) {
    VLOG(profiler) << "No need to saved profile info to " << filename;
    return;
  }

  if (methods.empty()) {
    VLOG(profiler) << "No info to save to " << filename;
    return;
  }

  DexFileToMethodsMap info;
  {
    ScopedObjectAccess soa(Thread::Current());
    for (auto it = methods.begin(); it != methods.end(); it++) {
      AddMethodInfo(*it, &info);
    }
  }

  // This doesn't need locking because we are trying to lock the file for exclusive
  // access and fail immediately if we can't.
  if (Serialize(filename, info)) {
    last_update_time_ns_.StoreRelaxed(last_update_time_ns);
    VLOG(profiler) << "Successfully saved profile info to "
                   << filename << " with time stamp: " << last_update_time_ns;
  }
}


void OfflineProfilingInfo::AddMethodInfo(ArtMethod* method, DexFileToMethodsMap* info) {
  DCHECK(method != nullptr);
  const DexFile* dex_file = method->GetDexFile();

  auto info_it = info->find(dex_file);
  if (info_it == info->end()) {
    info_it = info->Put(dex_file, std::set<uint32_t>());
  }
  info_it->second.insert(method->GetDexMethodIndex());
}


enum OpenMode {
  READ,
  READ_WRITE
};

static int OpenFile(const std::string& filename, OpenMode open_mode) {
  int fd = -1;
  switch (open_mode) {
    case READ:
      fd = open(filename.c_str(), O_RDONLY);
      break;
    case READ_WRITE:
      // TODO(calin) allow the shared uid of the app to access the file.
      fd = open(filename.c_str(),
                    O_CREAT | O_WRONLY | O_TRUNC | O_NOFOLLOW | O_CLOEXEC,
                    S_IRUSR | S_IWUSR);
      break;
  }

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

static constexpr const char kFieldSeparator = ',';
static constexpr const char kLineSeparator = '\n';
static const std::string kFirstDexFileSyntheticKey = ":classes.dex";
/**
 * Serialization format:
 *    multidex_suffix1,dex_location_checksum1,method_id11,method_id12...
 *    multidex_suffix2,dex_location_checksum2,method_id21,method_id22...
 * e.g.
 *    :classes.dex,131232145,11,23,454,54   -> this is the first dex file, it has no multidex suffix
 *                                             but we add a made-up value for easier parsing.
 *    :classes5.dex,218490184,39,13,49,1    -> this is the fifth dex file.
 **/
bool OfflineProfilingInfo::Serialize(const std::string& filename,
                                     const DexFileToMethodsMap& info) const {
  int fd = OpenFile(filename, READ_WRITE);
  if (fd == -1) {
    return false;
  }

  // TODO(calin): Merge with a previous existing profile.
  // TODO(calin): Profile this and see how much memory it takes. If too much,
  // write to file directly.
  std::ostringstream os;
  for (auto it : info) {
    const DexFile* dex_file = it.first;
    const std::set<uint32_t>& method_dex_ids = it.second;

    std::string multidex_suffix = DexFile::GetMultiDexSuffix(dex_file->GetLocation());
    os << (multidex_suffix.empty() ? kFirstDexFileSyntheticKey : multidex_suffix)
        << kFieldSeparator
        << dex_file->GetLocationChecksum();
    for (auto method_it : method_dex_ids) {
      os << kFieldSeparator << method_it;
    }
    os << kLineSeparator;
  }

  WriteToFile(fd, os);

  return CloseDescriptorForFile(fd, filename);
}

bool ProfileCompilationInfo::ProcessLine(const std::string& line,
                                         const std::vector<const DexFile*>& dex_files) {
  std::vector<std::string> parts;
  Split(line, ',', &parts);
  if (parts.size() < 3) {
    return false;
  }

  // If we detect the synthetic key, reset the multidex_suffix to the empty string.
  const std::string& multidex_suffix = (parts[0] == kFirstDexFileSyntheticKey) ? "" : parts[0];
  uint32_t checksum;
  if (!ParseInt(parts[1].c_str(), &checksum)) {
    return false;
  }
  const DexFile* current_dex_file = nullptr;
  for (auto dex_file : dex_files) {
    if (DexFile::GetMultiDexSuffix(dex_file->GetLocation()) == multidex_suffix) {
      if (checksum != dex_file->GetLocationChecksum()) {
        LOG(WARNING) << "Checksum mismatch for "
            << dex_file->GetLocation() << " when parsing " << filename_;
        return false;
      }
      current_dex_file = dex_file;
      break;
    }
  }
  if (current_dex_file == nullptr) {
    return true;
  }

  bool success = true;
  for (size_t i = 2; i < parts.size(); i++) {
    uint32_t method_idx;
    if (!ParseInt(parts[i].c_str(), &method_idx)) {
      LOG(WARNING) << "Cannot parse method_idx " << parts[i];
      success = false;
      break;
    }
    uint16_t class_idx = current_dex_file->GetMethodId(method_idx).class_idx_;
    auto info_it = info_.find(current_dex_file);
    if (info_it == info_.end()) {
      info_it = info_.Put(current_dex_file, ClassIdxToMethodsIdxMap());
    }
    ClassIdxToMethodsIdxMap& class_map = info_it->second;
    auto class_it = class_map.find(class_idx);
    if (class_it == class_map.end()) {
      class_it = class_map.Put(class_idx, std::set<uint32_t>());
    }
    class_it->second.insert(method_idx);
  }
  return success;
}

bool ProfileCompilationInfo::Load(const std::vector<const DexFile*>& dex_files) {
  if (kIsDebugBuild) {
    // In debug builds verify that the multidex suffixes are unique.
    std::set<std::string> suffixes;
    for (auto dex_file : dex_files) {
      std::string multidex_suffix = DexFile::GetMultiDexSuffix(dex_file->GetLocation());
      DCHECK(suffixes.find(multidex_suffix) == suffixes.end())
          << "DexFiles appears to belong to different apks."
          << " There are multiple dex files wit the same multidex suffix: `"
          << multidex_suffix << "'";
    }
  }
  info_.clear();

  int fd = OpenFile(filename_, READ);
  if (fd == -1) {
    return false;
  }

  std::string line;
  char buffer[1024];
  while (true) {
    int n = read(fd, buffer, 1024);
    if (n < 0) {
      PLOG(WARNING) << "Error when reading profile file " << filename_;
      CloseDescriptorForFile(fd, filename_);
      return false;
    } else if (n == 0) {
      break;
    }
    int new_line_detect_start = 0;
    while (new_line_detect_start < n) {
      int new_line_pos = -1;
      for (int i = new_line_detect_start; i < n; i++) {
        if (buffer[i] == '\n') {
          new_line_pos = i;
          break;
        }
      }
      if (new_line_pos != -1) {
        line.append(buffer, new_line_pos);
        if (!ProcessLine(line, dex_files)) {
          CloseDescriptorForFile(fd, filename_);
          return false;
        }
        line.clear();
        if (new_line_pos != n - 1) {
          line.append(buffer + new_line_pos + 1, n - new_line_pos - 1);
        }
        new_line_detect_start = new_line_pos + 1;
      } else {
        line.append(buffer, n);
        break;
      }
    }
  }
  return CloseDescriptorForFile(fd, filename_);
}

bool ProfileCompilationInfo::ContainsMethod(const MethodReference& method_ref) const {
  auto info_it = info_.find(method_ref.dex_file);
  if (info_it != info_.end()) {
    uint16_t class_idx = method_ref.dex_file->GetMethodId(method_ref.dex_method_index).class_idx_;
    const ClassIdxToMethodsIdxMap& class_map = info_it->second;
    auto class_it = class_map.find(class_idx);
    if (class_it != class_map.end()) {
      const std::set<uint32_t>& methods = class_it->second;
      return methods.find(method_ref.dex_method_index) != methods.end();
    }
    return false;
  }
  return false;
}

void ProfileCompilationInfo::DumpInfo() const {
  LOG(INFO) << "[ProfileGuidedCompilation] ProfileInfo: " << (info_.empty() ? "empty" : "");
  for (auto info_it : info_) {
    LOG(INFO) << '\n' << info_it.first->GetLocation() << '\n';
    for (auto class_it : info_it.second) {
      for (auto method_it : class_it.second) {
        LOG(INFO) << '\t' << PrettyMethod(method_it, *(info_it.first), true) << '\n';
      }
    }
  }
}

}  // namespace art
