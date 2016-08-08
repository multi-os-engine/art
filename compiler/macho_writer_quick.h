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

#ifndef ART_COMPILER_MACHO_WRITER_QUICK_H_
#define ART_COMPILER_MACHO_WRITER_QUICK_H_

#include "macho_writer.h"

namespace art {

class MachOWriterQuick : public MachOWriter {
 public:
  // Writes an oat file.
  // At this current state the oat file is a plain binary blob file.
  static bool Create(File* file,
                     OatWriter* oat_writer,
                     const std::vector<const DexFile*>& dex_files,
                     const std::string& android_root,
                     bool is_host,
                     const CompilerDriver& driver);

 protected:
  virtual bool Write(OatWriter* oat_writer,
                     const std::vector<const DexFile*>& dex_files,
                     const std::string& android_root,
                     bool is_host);

 private:
  MachOWriterQuick(const CompilerDriver& driver, File* macho_file);
  ~MachOWriterQuick();

  DISALLOW_IMPLICIT_CONSTRUCTORS(MachOWriterQuick);
};

#ifdef MOE
typedef art::MachOWriterQuick ElfWriterQuick32;
typedef art::MachOWriterQuick ElfWriterQuick64;
#endif
    
}  // namespace art

#endif  // ART_COMPILER_MACHO_WRITER_QUICK_H_
