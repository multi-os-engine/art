/*
 * Copyright (C) 2014 The Android Open Source Project
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

#include "base/stl_util.h"
#include "class_path.h"
#include "dex_file-inl.h"
#include "utf-inl.h"
#include "utils.h"

namespace art {

ClassPath::ClassPath() {
}

ClassPath::~ClassPath() {
  // STLDeleteElements(&dex_files_);
}

ClassPath::Entry ClassPath::Find(const char* descriptor) const {
  if (true) {
    auto it = class_path_map_.find(descriptor);
    if (LIKELY(it != class_path_map_.end())) {
      return it->second;
    }
  } else {
    for (const DexFile* dex_file : dex_files_) {
      const DexFile::ClassDef* dex_class_def = dex_file->FindClassDef(descriptor);
      if (dex_class_def != nullptr) {
        return Entry(dex_file, dex_class_def);
      }
    }
  }
  return Entry(nullptr, nullptr);
}

void ClassPath::AddDexFile(const DexFile* dex_file) {
  const uint64_t start = NanoTime();
  // Number of type ids (upper bound on number of class def).
  const size_t num_type_ids = dex_file->NumTypeIds();
  // Maximum number of class indexes is 64k.
  const size_t num_class_defs = dex_file->NumClassDefs();
  CHECK_LE(num_class_defs, num_type_ids);
  for (size_t i = 0; i < num_class_defs; ++i) {
    const DexFile::ClassDef* class_def = &dex_file->GetClassDef(i);
    const size_t idx = class_def->class_idx_;
    const DexFile::TypeId* type_id = &dex_file->GetTypeId(idx);
    const uint32_t descriptor_idx = type_id->descriptor_idx_;
    const DexFile::StringId* string_id = &dex_file->GetStringId(descriptor_idx);
    const char* data = dex_file->GetStringData(*string_id);
    auto it = class_path_map_.find(data);
    if (LIKELY(it == class_path_map_.end())) {
      class_path_map_.insert(std::make_pair(data, ClassPath::Entry(dex_file, class_def)));
    }
  }
  VLOG(verifier) << "Adding dex file " << dex_file << " with " << num_class_defs << " class defs "
                 << " took " << PrettyDuration(NanoTime() - start);
  dex_files_.push_back(dex_file);
}

}  // namespace art
