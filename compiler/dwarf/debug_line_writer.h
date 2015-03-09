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

#ifndef ART_COMPILER_DWARF_DEBUG_LINE_WRITER_H_
#define ART_COMPILER_DWARF_DEBUG_LINE_WRITER_H_

#include "enums.h"
#include "writer.h"
#include "debug_line_opcode_writer.h"

namespace art {
namespace dwarf {

// Writer for the .debug_line section (DWARF-3)
class DebugLineWriter : protected Writer {
 public:
  typedef struct {
    const char* file_name;
    int directory_index;
    int modification_time;
    int file_size;
  } FileEntry;

  void WriteTable(std::vector<const char*>& include_directories,
                  std::vector<FileEntry>& files,
                  DebugLineOpCodeWriter& opcodes) {
    size_t header_start = data_->size();
    PushUint32(0);  // section-length placeholder
    // Claim DWARF-2 version even though we use some DWARF-3 features.
    // DWARF-2 consumers will ignore the unkown opcodes.
    // This is what clang currently does.
    PushUint16(2);  // debug_line version
    size_t header_length_pos = data_->size();
    PushUint32(0);  // header-length placeholder
    PushUint8(1 << opcodes.code_factor_bits());
    PushUint8(DebugLineOpCodeWriter::kDefaultIsStmt ? 1 : 0);
    PushInt8(DebugLineOpCodeWriter::kLineBase);
    PushUint8(DebugLineOpCodeWriter::kLineRange);
    PushUint8(DebugLineOpCodeWriter::kOpcodeBase);
    static const int opcode_lengths[DebugLineOpCodeWriter::kOpcodeBase] = { 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1 };
    for (int i = 1; i < DebugLineOpCodeWriter::kOpcodeBase; i++) {
      PushUint8(opcode_lengths[i]);
    }
    for (auto include_directory : include_directories) {
      PushString(include_directory);
    }
    PushUint8(0);  // terminate include_directories list
    for (auto file : files) {
      PushString(file.file_name);
      PushUleb128(file.directory_index);
      PushUleb128(file.modification_time);
      PushUleb128(file.file_size);
    }
    PushUint8(0);  // terminate file list
    UpdateUint32(header_length_pos, data_->size() - header_length_pos - 4);
    PushData(opcodes.data()->data(), opcodes.data()->size());
    UpdateUint32(header_start, data_->size() - header_start - 4);
  }

  explicit DebugLineWriter(std::vector<uint8_t>* buffer) : Writer(buffer) { }
};

}  // namespace dwarf
}  // namespace art

#endif  // ART_COMPILER_DWARF_DEBUG_LINE_WRITER_H_
