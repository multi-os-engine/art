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

#ifndef ART_RUNTIME_CLASS_PROFILE_H_
#define ART_RUNTIME_CLASS_PROFILE_H_

#include <string>
#include <utility>
#include <vector>

#include "base/allocator.h"
#include "base/hash_set.h"
#include "base/macros.h"
#include "base/mutex.h"
#include "dex_file.h"
#include "gc_root.h"
#include "object_callbacks.h"
#include "runtime.h"

namespace art {

class ClassProfile {
 public:
  // Add all of the classes in the class linker, incremental.
  void Collect();

  // Write all of the collected data to a file.
  void WriteToFile(const char* file_name);

  // read from a file.
  void ReadFromFile();

  // Dump the colected data to a stream.
  void Dump(std::ostream& os);

 private:
  struct Header {
    static const uint32_t kCurrentVersion = 0;
    uint32_t version = kCurrentVersion;
  };
  struct DexFileProfileData {
    DexFileProfileData() = default;
    DexFileProfileData(DexFileProfileData&&) = default;
    uint32_t num_class_defs = 0;
    // The index is the class def index.
    // If a class is resolved, it is in the resolved bitmap.
    std::unique_ptr<uint8_t[]> resolved_bitmap;
    // If a class is verified, it is marked in the verified bitmap.
    std::unique_ptr<uint8_t[]> verified_bitmap;
  };

  // Serialize data.
  size_t Serialize(std::vector<uint8_t>* out);

  // Deserialize.
  bool Deserialize(const uint8_t* in, size_t in_size, std::string* error_msg);

  // Key is dex_file_name.
  std::unordered_map<std::string, DexFileProfileData> profiles_;
};

}  // namespace art

#endif  // ART_RUNTIME_CLASS_PROFILE_H_
