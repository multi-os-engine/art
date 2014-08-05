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

#include "../../runtime/leb128.h"
#include "../../runtime/arch/x86/registers_x86.h"
#include "../../runtime/arch/x86_64/registers_x86_64.h"
#include "dwarf_cfi.h"

namespace art {

bool ARTRegIDToDWARFRegID(bool is_x86_64, int art_reg_id, int* dwarf_reg_id) {
  if (is_x86_64) {
    switch (art_reg_id) {
    case x86_64::RBX: *dwarf_reg_id =  3; return true;
    case x86_64::RBP: *dwarf_reg_id =  6; return true;
    case x86_64::R12: *dwarf_reg_id = 12; return true;
    case x86_64::R13: *dwarf_reg_id = 13; return true;
    case x86_64::R14: *dwarf_reg_id = 14; return true;
    case x86_64::R15: *dwarf_reg_id = 15; return true;
    default: return false;  // Should not get here
    }
  } else {
    switch (art_reg_id) {
    case x86::EBP: *dwarf_reg_id = 5; return true;
    case x86::ESI: *dwarf_reg_id = 6; return true;
    case x86::EDI: *dwarf_reg_id = 7; return true;
    default: return false;  // Should not get here
    }
  }
}

void EncodeUnsignedLeb128(std::vector<uint8_t>& buf, uint32_t value) {
  uint8_t buffer[12];
  uint8_t *ptr = EncodeUnsignedLeb128(buffer, value);
  for (uint8_t *p = buffer; p < ptr; p++) {
    buf.push_back(*p);
  }
}

void EncodeSignedLeb128(std::vector<uint8_t>& buf, int32_t value) {
  uint8_t buffer[12];
  uint8_t *ptr = EncodeSignedLeb128(buffer, value);
  for (uint8_t *p = buffer; p < ptr; p++) {
    buf.push_back(*p);
  }
}

void PushWord(std::vector<uint8_t>&buf, int32_t data) {
  buf.push_back(data & 0xff);
  buf.push_back((data >> 8) & 0xff);
  buf.push_back((data >> 16) & 0xff);
  buf.push_back((data >> 24) & 0xff);
}

void DW_CFA_advance_loc(std::vector<uint8_t>& buf, uint32_t increment) {
  if (increment < 64) {
    // Encoding in opcode.
    buf.push_back(0x1 << 6 | increment);
  } else if (increment < 256) {
    // Single byte delta.
    buf.push_back(0x02);
    buf.push_back(increment);
  } else if (increment < 256 * 256) {
    // Two byte delta.
    buf.push_back(0x03);
    buf.push_back(increment & 0xff);
    buf.push_back((increment >> 8) & 0xff);
  } else {
    // Four byte delta.
    buf.push_back(0x04);
    PushWord(buf, increment);
  }
}

void DW_CFA_offset_extended_sf(std::vector<uint8_t>& buf, int reg, int32_t offset) {
  buf.push_back(0x11);
  EncodeUnsignedLeb128(buf, reg);
  EncodeSignedLeb128(buf, offset);
}

void DW_CFA_offset(std::vector<uint8_t>& buf, int reg, uint32_t offset) {
  buf.push_back((0x2 << 6) | reg);
  EncodeUnsignedLeb128(buf, offset);
}

void DW_CFA_def_cfa_offset(std::vector<uint8_t>& buf, int32_t offset) {
  buf.push_back(0x0e);
  EncodeUnsignedLeb128(buf, offset);
}

void DW_CFA_remember_state(std::vector<uint8_t>& buf) {
  buf.push_back(0x0a);
}

void DW_CFA_restore_state(std::vector<uint8_t>& buf) {
  buf.push_back(0x0b);
}

std::vector<uint8_t>* X86GetCIE(bool is_x86_64) {
  std::vector<uint8_t>* buf = new std::vector<uint8_t>;

  // Length (will be filled in later in this routine).
  PushWord(*buf, 0);

  // CIE id: always 0.
  PushWord(*buf, 0);

  // Version: always 1.
  buf->push_back(0x01);

  // Augmentation: 'zR\0'
  buf->push_back(0x7a);
  buf->push_back(0x52);
  buf->push_back(0x0);

  // Code alignment: 1.
  EncodeUnsignedLeb128(*buf, 1);

  // Data alignment.
  if (is_x86_64) {
    EncodeSignedLeb128(*buf, -8);
  } else {
    EncodeSignedLeb128(*buf, -4);
  }

  // Return address register.
  if (is_x86_64) {
    // R16(RIP)
    buf->push_back(0x10);
  } else {
    // R8(EIP)
    buf->push_back(0x08);
  }

  // Augmentation length: 1.
  buf->push_back(1);

  // Augmentation data: 0x03 ((DW_EH_PE_absptr << 4) | DW_EH_PE_udata4).
  buf->push_back(0x03);

  // Initial instructions.
  if (is_x86_64) {
    // DW_CFA_def_cfa R7(RSP) 8.
    buf->push_back(0x0c);
    buf->push_back(0x07);
    buf->push_back(0x08);

    // DW_CFA_offset R16(RIP) 1 (* -8).
    buf->push_back(0x90);
    buf->push_back(0x01);
  } else {
    // DW_CFA_def_cfa R4(ESP) 4.
    buf->push_back(0x0c);
    buf->push_back(0x04);
    buf->push_back(0x04);

    // DW_CFA_offset R8(EIP) 1 (* -4).
    buf->push_back(0x88);
    buf->push_back(0x01);
  }

  PadCFI(*buf);
  WriteCFILength(*buf);

  return buf;
}

void WriteFDEHeader(std::vector<uint8_t>& buf) {
  // 'length' (filled in by other functions).
  PushWord(buf, 0);

  // 'CIE_pointer' (filled in by linker).
  PushWord(buf, 0);

  // 'initial_location' (filled in by linker).
  PushWord(buf, 0);

  // 'address_range' (filled in by other functions).
  PushWord(buf, 0);

  // Augmentation length: 0
  buf.push_back(0);
}

void WriteFDEAddressRange(std::vector<uint8_t>& buf, uint32_t data) {
  const int kOffsetOfAddressRange = 12;
  DCHECK(buf.size() >= kOffsetOfAddressRange + sizeof(uint32_t));

  uint8_t *p = buf.data() + kOffsetOfAddressRange;
  p[0] = data;
  p[1] = data >> 8;
  p[2] = data >> 16;
  p[3] = data >> 24;
}

void WriteCFILength(std::vector<uint8_t>& buf) {
  uint32_t length = buf.size() - 4;
  DCHECK((length & 0x3) == 0);
  DCHECK(length > 4);

  uint8_t *p = buf.data();
  p[0] = length;
  p[1] = length >> 8;
  p[2] = length >> 16;
  p[3] = length >> 24;
}

void PadCFI(std::vector<uint8_t>& buf) {
  while (buf.size() & 0x3) {
    buf.push_back(0);
  }
}

}  // namespace art
