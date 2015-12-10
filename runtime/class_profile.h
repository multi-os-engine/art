/*
 * Copyright (C) 2016 The Android Open Source Project
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

class DexCacheProfileData {
 public:
  DexCacheProfileData() = default;
  explicit DexCacheProfileData(const DexFile& dex_file);
  DexCacheProfileData(uint32_t checksum, size_t num_class_defs);

  DexCacheProfileData(DexCacheProfileData&&) = default;

  void Update(mirror::DexCache* dex_cache)
      SHARED_REQUIRES(Locks::mutator_lock_);

  size_t NumClassDefs() const {
    return num_class_defs_;
  }

  bool IsResolved(size_t class_def_index) const;

  size_t WriteToVector(std::vector<uint8_t>* out) const;

  static DexCacheProfileData* ReadFromMemory(const uint8_t** in,
                                             size_t* in_size,
                                             std::string* error_msg);

  std::unordered_set<std::string> GetClassDescriptors(const DexFile& dex_file) const;

  uint32_t DexFileChecksum() const {
    return checksum_;
  }

 private:
  const uint32_t checksum_ = 0;
  const uint32_t num_class_defs_ = 0;
  // The index is the class def index.
  // If a class is resolved, it is in the resolved bitmap.
  std::unique_ptr<uint8_t[]> resolved_bitmap_;
};

class ClassProfile {
 public:
  // Add all of the classes in the class linker, incremental.
  void Collect();

  // Return the descriptors for resolved classes in all of the class profiles. Only works for
  // dex files that are already open.
  std::unordered_set<std::string> GetClassDescriptors() const;

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

  // Serialize data.
  size_t Serialize(std::vector<uint8_t>* out);

  // Deserialize.
  bool Deserialize(const uint8_t* in, size_t in_size, std::string* error_msg);

  // Key is dex_file_name.
  std::unordered_map<std::string, std::unique_ptr<DexCacheProfileData>> dex_caches_;

  using DexFileMap = std::unordered_map<std::string, const DexFile*>;
  static DexFileMap GetDexFileMap();
};

}  // namespace art

#endif  // ART_RUNTIME_CLASS_PROFILE_H_
