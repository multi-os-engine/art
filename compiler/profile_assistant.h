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

#ifndef ART_COMPILER_PROFILE_ASSISTANT_H_
#define ART_COMPILER_PROFILE_ASSISTANT_H_

#include <string>
#include <vector>

#include "jit/offline_profiling_info.cc"

namespace art {

class ProfileAssistant {
 public:
  // Process the profile information present in the given files returning a not
  // null object if there is a significant difference between the profile_files
  // and reference_profile_files.
  //   - the returned object will be the merge of the data from all
  //     profile_files and reference_profile_files.
  //   - if reference_profile_files is not empty it must be the same size as
  //     profile_files.
  //   - if the return is not null, the data from profile_files[i] is merged
  //     into reference_profile_files[i] and the corresponding backing file is
  //     updated.
  // If there's any error (e.g. file parsing, non existing files), or the
  // difference between files is insignificant, this function returns null.
  static ProfileCompilationInfo* ProcessProfiles(
      const std::vector<std::string>& profile_files,
      const std::vector<std::string>& reference_profile_files);
 private:
  ProfileAssistant() {}
};

}  // namespace art

#endif  // ART_COMPILER_PROFILE_ASSISTANT_H_
