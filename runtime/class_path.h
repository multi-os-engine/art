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

#ifndef ART_RUNTIME_CLASS_PATH_H_
#define ART_RUNTIME_CLASS_PATH_H_

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/mutex.h"
#include "dex_file.h"
#include "utf.h"

namespace art {

class DexFile;

class ClassPath {
 public:
  typedef std::pair<const DexFile*, const DexFile::ClassDef*> Entry;

  ClassPath();
  virtual ~ClassPath();
  void AddDexFile(const DexFile* dex_file);
  const std::vector<const DexFile*>* GetDexFiles() const {
    return &dex_files_;
  }
  ClassPath::Entry Find(const char* descriptor) const;

 private:
  struct UTF16HashCmp {
    // Hash function.
    size_t operator()(const char* key) const {
      size_t hash = 0;
      while (*key != '\0') {
        hash = hash * 31 + GetUtf16FromUtf8(&key);
      }
      return hash;
    }
    // std::equal function.
    bool operator()(const char* a, const char* b) const {
      return CompareModifiedUtf8ToModifiedUtf8AsUtf16CodePointValues(a, b) == 0;
    }
  };
  std::unordered_map<const char*, ClassPath::Entry, UTF16HashCmp, UTF16HashCmp> class_path_map_;
  std::vector<const DexFile*> dex_files_;
};

}  // namespace art
#endif  // ART_RUNTIME_CLASS_PATH_H_
