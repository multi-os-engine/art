/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include "disassembler.h"

#include <iostream>

#include "disassembler_arm.h"
#include "disassembler_arm64.h"
#include "disassembler_mips.h"
#include "disassembler_x86.h"

namespace art {

Disassembler* Disassembler::Create(InstructionSet instruction_set, DisassemblerOptions* options,
    DisassemblerAnnotator* ann) {
  if (instruction_set == kArm || instruction_set == kThumb2) {
    return new arm::DisassemblerArm(options, ann);
  } else if (instruction_set == kArm64) {
    return new arm64::DisassemblerArm64(options, ann);
  } else if (instruction_set == kMips) {
    return new mips::DisassemblerMips(options, ann);
  } else if (instruction_set == kX86) {
    return new x86::DisassemblerX86(options, false, ann);
  } else if (instruction_set == kX86_64) {
    return new x86::DisassemblerX86(options, true, ann);
  } else {
    // UNIMPLEMENTED(FATAL) << "no disassembler for " << instruction_set;
  }
  return NULL;
}

std::string Disassembler::FormatInstructionPointer(const uint8_t* begin) {
  if (disassembler_options_->absolute_addresses_) {
    return StringPrintf("%p", begin);
  } else {
    size_t offset = begin - disassembler_options_->base_address_;
    return StringPrintf("0x%08zx", offset);
  }
}

void Disassembler::Annotate(std::ostringstream* str, ...) {
  if (annotator_ != nullptr) {
    va_list ap;
    va_start(ap, str);
    annotator_->Annotate(*str, ap);
    va_end(ap);
  }
}

void StringAppendV(std::string* dst, const char* format, va_list ap) {
  // First try with a small fixed size buffer
  char space[1024];

  // It's possible for methods that use a va_list to invalidate
  // the data in it upon use.  The fix is to make a copy
  // of the structure before using it and use that copy instead.
  va_list backup_ap;
  va_copy(backup_ap, ap);
  int result = vsnprintf(space, sizeof(space), format, backup_ap);
  va_end(backup_ap);

  if (result < static_cast<int>(sizeof(space))) {
    if (result >= 0) {
      // Normal case -- everything fit.
      dst->append(space, result);
      return;
    }

    if (result < 0) {
      // Just an error.
      return;
    }
  }

  // Increase the buffer size to the size requested by vsnprintf,
  // plus one for the closing \0.
  int length = result+1;
  char* buf = new char[length];

  // Restore the va_list before we use it again
  va_copy(backup_ap, ap);
  result = vsnprintf(buf, length, format, backup_ap);
  va_end(backup_ap);

  if (result >= 0 && result < length) {
    // It fit
    dst->append(buf, result);
  }
  delete[] buf;
}

std::string StringPrintf(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  std::string result;
  StringAppendV(&result, fmt, ap);
  va_end(ap);
  return result;
}

void StringAppendF(std::string* dst, const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  StringAppendV(dst, format, ap);
  va_end(ap);
}


}  // namespace art
