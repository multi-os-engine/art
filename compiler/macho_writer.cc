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

#include "macho_writer.h"

#include "base/unix_file/fd_file.h"

namespace art {

MachOWriter::MachOWriter(const CompilerDriver& driver, File* macho_file)
  : compiler_driver_(&driver), macho_file_(macho_file) {}

MachOWriter::~MachOWriter() {}

void MachOWriter::GetOatMachOInformation(File* file,
                                     size_t* oat_loaded_size,
                                     size_t* oat_data_offset) {
  if(file != nullptr)
    *oat_loaded_size = (size_t)file->GetLength();
  else
    *oat_loaded_size = 0;
  *oat_data_offset = 0;
}

}  // namespace art
