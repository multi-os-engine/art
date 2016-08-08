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

#include "macho_file.h"

namespace art {

MachOFile::MachOFile()
  : file_(NULL) {}

MachOFile* MachOFile::Open(File* file, bool writable, std::string* error_msg) {
  UniquePtr<MachOFile> macho_file(new MachOFile());
  if (!macho_file->Setup(file, writable ? PROT_WRITE | PROT_READ : PROT_READ, writable ? MAP_SHARED : MAP_PRIVATE, error_msg)) {
    return NULL;
  }
  return macho_file.release();
}

MachOFile* MachOFile::Open(File* file, int prot, int flags, std::string* error_msg) {
  std::unique_ptr<MachOFile> macho_file(new MachOFile());
  if (!macho_file->Setup(file, prot, flags, error_msg)) {
    return nullptr;
  }
  return macho_file.release();
}

bool MachOFile::Setup(File* file, int prot, int flags, std::string* error_msg) {
  CHECK(file != NULL);
  file_ = file;
  int64_t file_length = file_->GetLength();
  if (file_length < 0) {
    errno = -file_length;
    PLOG(WARNING) << "Failed to get length of file: " << file_->GetPath() << " fd=" << file_->Fd();
    return false;
  }
  return SetMap(MemMap::MapFile(file_->GetLength(), prot, flags, file_->Fd(), 0, "__oatdata", error_msg));
}

MachOFile::~MachOFile() {}

bool MachOFile::SetMap(MemMap* map) {
  if (map == NULL) {
    // MemMap::Open should have already logged
    return false;
  }
  map_.reset(map);
  return true;
}

}  // namespace art
