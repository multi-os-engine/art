/*
 * Copyright (C) 2012 The Android Open Source Project
 * Copyright 2014-2016 Intel Corporation
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

#ifndef ART_RUNTIME_MACHO_FILE_H_
#define ART_RUNTIME_MACHO_FILE_H_

#include "base/unix_file/fd_file.h"
#include "mem_map.h"
#include "os.h"
#include "UniquePtr.h"

namespace art {

// Used by the compiler to create and modify oat files.
class MachOFile {
 public:
  static MachOFile* Open(File* file, bool writable, std::string* error_msg);
  static MachOFile* Open(File* file, int prot, int flags, std::string* error_msg);

  ~MachOFile();

  File& GetFile() const {
    return *file_;
  }

  uint8_t* Begin() {
    return map_->Begin();
  }

  uint8_t* End() {
    return map_->End();
  }

  size_t Size() const {
    return map_->Size();
  }

 private:
  MachOFile();

  bool Setup(File* file, int prot, int flags, std::string* error_msg);

  bool SetMap(MemMap* map);

  File* file_;

  // The mapped content for the file.
  UniquePtr<MemMap> map_;
};

}  // namespace art

#endif  // ART_RUNTIME_MACHO_FILE_H_
