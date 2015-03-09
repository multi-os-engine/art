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

#ifndef ART_COMPILER_DWARF_DEBUG_FRAME_WRITER_H_
#define ART_COMPILER_DWARF_DEBUG_FRAME_WRITER_H_

#include "enums.h"
#include "writer.h"
#include "debug_frame_opcode_writer.h"

namespace art {
namespace dwarf {

// Writer for the .eh_frame section (which based on the .debug_frame specification)
class DebugFrameWriter : protected Writer {
 public:
  void WriteCIE(int return_address_register,
                const uint8_t* initial_opcodes,
                int initial_opcodes_size) {
    DCHECK(cie_header_start_ == ~0u);
    cie_header_start_ = data_->size();
    PushUint32(0);  // length placeholder
    PushUint32(0);  // CIE id
    PushUint8(1);   // version
    PushString("zR");
    PushUleb128(DebugFrameOpCodeWriter::kCodeAlignmentFactor);
    PushSleb128(DebugFrameOpCodeWriter::kDataAlignmentFactor);
    PushUleb128(return_address_register);  // ubyte in DWARF2
    PushUleb128(1);  // z: Augmentation data size
    if (use_64bit_address_) {
      PushUint8(0x04);  // R: ((DW_EH_PE_absptr << 4) | DW_EH_PE_udata8).
    } else {
      PushUint8(0x03);  // R: ((DW_EH_PE_absptr << 4) | DW_EH_PE_udata4).
    }
    PushData(initial_opcodes, initial_opcodes_size);
    Pad(use_64bit_address_ ? 8 : 4);
    UpdateUint32(cie_header_start_, data_->size() - cie_header_start_ - 4);
  }

  void WriteCIE(int return_address_register,
                DebugFrameOpCodeWriter& opcodes) {
    WriteCIE(return_address_register, opcodes.data()->data(), opcodes.data()->size());
  }

  void WriteFDE(uint64_t initial_address,
                uint64_t address_range,
                const uint8_t* unwind_opcodes,
                int unwind_opcodes_size) {
    DCHECK(cie_header_start_ != ~0u);
    size_t fde_header_start = data_->size();
    PushUint32(0);  // length placeholder
    PushUint32(data_->size() - cie_header_start_);  // 'CIE_pointer'
    if (use_64bit_address_) {
      PushUint64(initial_address);
      PushUint64(address_range);
    } else {
      PushUint32(initial_address);
      PushUint32(address_range);
    }
    PushUleb128(0);  // Augmentation data size
    PushData(unwind_opcodes, unwind_opcodes_size);
    Pad(use_64bit_address_ ? 8 : 4);
    UpdateUint32(fde_header_start, data_->size() - fde_header_start - 4);
  }

  DebugFrameWriter(std::vector<uint8_t>* buffer, bool use_64bit_address)
      : Writer(buffer),
        use_64bit_address_(use_64bit_address),
        cie_header_start_(~0u) {
  }

 protected:
  bool use_64bit_address_;
  size_t cie_header_start_;
};

}  // namespace dwarf
}  // namespace art

#endif  // ART_COMPILER_DWARF_DEBUG_FRAME_WRITER_H_
