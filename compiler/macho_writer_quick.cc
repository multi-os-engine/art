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

#include "macho_writer_quick.h"

#include "base/unix_file/fd_file.h"
#include "buffered_output_stream.h"
#include "file_output_stream.h"
#include "oat_writer.h"

namespace art {

MachOWriterQuick::MachOWriterQuick(const CompilerDriver& driver, File* macho_file)
  : MachOWriter(driver, macho_file) {}

MachOWriterQuick::~MachOWriterQuick() {}

bool MachOWriterQuick::Create(File* macho_file,
                            OatWriter* oat_writer,
                            const std::vector<const DexFile*>& dex_files,
                            const std::string& android_root,
                            bool is_host,
                            const CompilerDriver& driver) {
  MachOWriterQuick macho_writer(driver, macho_file);
  return macho_writer.Write(oat_writer, dex_files, android_root, is_host);
}
    
bool MachOWriterQuick::Write(OatWriter* oat_writer,
                           const std::vector<const DexFile*>& dex_files_unused,
                           const std::string& android_root_unused,
                           bool is_host_unused) {
  BufferedOutputStream output_stream(new FileOutputStream(macho_file_));

  if ((oat_writer != nullptr) && !oat_writer->WriteRodata(&output_stream)) {
    PLOG(ERROR) << "Failed to write Rodata for " << macho_file_->GetPath();
    return false;
  }
  if ((oat_writer != nullptr) && !oat_writer->WriteCode(&output_stream)) {
    PLOG(ERROR) << "Failed to write code for " << macho_file_->GetPath();
    return false;
  }
  macho_file_->Flush();

  return true;
}

}  // namespace art
