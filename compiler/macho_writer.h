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

#ifndef ART_COMPILER_MACHO_WRITER_H_
#define ART_COMPILER_MACHO_WRITER_H_

#include <vector>

#include "base/macros.h"
#include "os.h"

namespace art {

class CompilerDriver;
class DexFile;
class MachOFile;
class OatWriter;

class MachOWriter {
 public:
  // Returns the loadable size and the data offset of the oat.
  // At this current state the oat file is a plain binary blob
  // without any kind of format, so the first one will be the
  // same as the file size and the second one will always be 0.
  static void GetOatMachOInformation(File* file,
                                   size_t* oat_loaded_size,
                                   size_t* oat_data_offset);

 protected:
  MachOWriter(const CompilerDriver& driver, File* elf_file);
  virtual ~MachOWriter();

  virtual bool Write(OatWriter* oat_writer,
                     const std::vector<const DexFile*>& dex_files,
                     const std::string& android_root,
                     bool is_host) = 0;

  // Setup by constructor
  const CompilerDriver* compiler_driver_;
  File* macho_file_;
};

}  // namespace art

#endif  // ART_COMPILER_MACHO_WRITER_H_
