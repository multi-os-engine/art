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

#include "profile_assistant.h"

namespace art {

static constexpr const uint32_t kMethodsThreshold = 10;


static void CloseAllFds(const std::vector<uint32_t>& fds, const char* description) {
  for (size_t i = 0; i < fds.size(); i++) {
    if (close(fds[i]) < 0) {
      PLOG(WARNING) << "Failed to close descriptor for " << description << " at index" << i;
    }
  }
}

// Lock the file for exclusive access but don't wait if we can't lock it.
static bool LockAllFds(const std::vector<uint32_t>& fds, const char* description) {
  for (size_t i = 0; i < fds.size(); i++) {
    int err = flock(fds[i], LOCK_EX | LOCK_NB);
    if (err < 0) {
      PLOG(WARNING) << "Failed to lock descriptor for " << description << " at index" << i;
      return false;
    }
  }
  return true;
}

static void UnlockAllFds(const std::vector<uint32_t>& fds, const char* description) {
  for (size_t i = 0; i < fds.size(); i++) {
    if (flock(fds[i], LOCK_UN) < 0) {
      PLOG(WARNING) << "Failed to unlock descriptor for " << description << " at index" << i;
    }
  }
}

bool ProfileAssistant::ProcessProfilesInternal(
        const std::vector<uint32_t>& profile_files_fd,
        const std::vector<uint32_t>& reference_profile_files_fd,
        /*out*/ ProfileCompilationInfo** profile_compilation_info) {
  std::vector<ProfileCompilationInfo> new_info(profile_files_fd.size());
  bool should_compile = false;
  // Read the main profile files.
  for (size_t i = 0; i < profile_files_fd.size(); i++) {
    if (!new_info[i].Load(profile_files_fd[i])) {
      LOG(WARNING) << "Could not load profile file at index " << i;
      return false;
    }
    // Do we have enough new profiled methods that will make the compilation worth while?
    should_compile |= (new_info[i].GetNumberOfMethods() > kMethodsThreshold);
  }
  if (!should_compile) {
    *profile_compilation_info = nullptr;
    return true;
  }

  std::unique_ptr<ProfileCompilationInfo> result(new ProfileCompilationInfo());
  for (size_t i = 0; i < new_info.size(); i++) {
    // Merge all data into a single object.
    result->Load(new_info[i]);
    // If we have any reference profile information merge their information with
    // the current profiles and save them back to disk.
    if (!reference_profile_files_fd.empty()) {
      if (!new_info[i].Load(reference_profile_files_fd[i])) {
        LOG(WARNING) << "Could not load reference profile file at index " << i;
        return false;
      }
      if (!new_info[i].Save(reference_profile_files_fd[i])) {
        LOG(WARNING) << "Could not save reference profile file at index " << i;
        return false;
      }
    }
  }
  *profile_compilation_info = result.release();
  return true;
}

bool ProfileAssistant::ProcessProfiles(
        const std::vector<uint32_t>& profile_files_fd,
        const std::vector<uint32_t>& reference_profile_files_fd,
        /*out*/ ProfileCompilationInfo** profile_compilation_info) {
  DCHECK(!profile_files_fd.empty());
  DCHECK(!reference_profile_files_fd.empty() ||
      (profile_files_fd.size() == reference_profile_files_fd.size()));

  // Try to lock the files for exclusive access.
  if (!LockAllFds(profile_files_fd, "profile_files_fd")) {
    LOG(WARNING) << "Couldn't lock profile files for exclusive access";
    return false;
  }
  if (!LockAllFds(reference_profile_files_fd, "reference_profile_files_fd")) {
    LOG(WARNING) << "Couldn't lock reference profile files for exclusive access";
    return false;
  }

  bool result = ProcessProfilesInternal(profile_files_fd,
                                        reference_profile_files_fd,
                                        profile_compilation_info);

  // Unlock the files.
  UnlockAllFds(profile_files_fd, "profile_files_fd");
  UnlockAllFds(reference_profile_files_fd, "reference_profile_files_fd");

  return result;
}

bool ProfileAssistant::ProcessProfiles(
        const std::vector<std::string>& profile_files,
        const std::vector<std::string>& reference_profile_files,
        /*out*/ ProfileCompilationInfo** profile_compilation_info) {
  std::vector<uint32_t> profile_files_fd;
  std::vector<uint32_t> reference_profile_files_fd;

  // Open profile_files.
  for (size_t i = 0; i < profile_files.size(); i++) {
    int fd = open(profile_files[i].c_str(), O_RDONLY);
    if (fd < 0) {
      PLOG(WARNING) << "Failed to open profile file " << profile_files[i];
      CloseAllFds(profile_files_fd, "profile_files");
      return false;
    }
    profile_files_fd.push_back(fd);
  }

  // Open reference_profile_files.
  for (size_t i = 0; i < reference_profile_files.size(); i++) {
    int fd = open(reference_profile_files[i].c_str(), O_RDWR);
    if (fd < 0) {
      PLOG(WARNING) << "Failed to open profile file " << reference_profile_files[i];
      CloseAllFds(profile_files_fd, "profile_files");
      CloseAllFds(reference_profile_files_fd, "reference_profile_files");
      return false;
    }
    reference_profile_files_fd.push_back(fd);
  }

  // Do the analysis.
  bool result = ProcessProfiles(profile_files_fd,
                                reference_profile_files_fd,
                                profile_compilation_info);

  // Close all descriptors.
  CloseAllFds(profile_files_fd, "profile_files");
  CloseAllFds(reference_profile_files_fd, "reference_profile_files");

  return result;
}

}  // namespace art
