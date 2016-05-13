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

#include <stdint.h>
#include <stdio.h>
#include <string>
#include <memory>

//
// Hidden implementation helper class. Ordinarily this would be
// overkill, but it turns out that the protobuf-related header files
// do not mix well with art runtime headers (for example, art defines
// a macro MutexLock that clashes with a similarly named protobuf
// class).
//
class OatMapBuilderImpl;

class OatMapBuilder {
 public:
  // Create builder for an OAT file with specicified checksum
  OatMapBuilder(uint32_t adler32_checksum);
  ~OatMapBuilder();

  // Register dex file with specified signature
  void AddDexFile(const std::string &sha1sig);

  // Register class within current dex file
  void AddClass(uint32_t class_def_index);

  // Register method within current class
  void AddMethod(uint32_t method_index,
                 uint64_t code_start_offset,
                 uint32_t code_size);

  // Emit to file. Returns TRUE if write succeeded, FALSE if error.
  bool EmitToFile(int file_descriptor);

 private:
  std::unique_ptr<OatMapBuilderImpl> impl;
};
