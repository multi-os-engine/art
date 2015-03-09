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

#ifndef ART_COMPILER_DWARF_WRITER_H_
#define ART_COMPILER_DWARF_WRITER_H_

#include <vector>
#include "leb128.h"
#include "base/logging.h"

namespace art {
namespace dwarf {

// The base class for all DWARF writers
class Writer {
 public:
  void PushUint8(int value) {
    DCHECK_GE(value, 0);
    DCHECK_LE(value, UINT8_MAX);
    data_->push_back(value & 0xff);
  }

  void PushUint16(int value) {
    DCHECK_GE(value, 0);
    DCHECK_LE(value, UINT16_MAX);
    data_->push_back((value >> 0) & 0xff);
    data_->push_back((value >> 8) & 0xff);
  }

  void PushUint32(uint32_t value) {
    data_->push_back((value >> 0) & 0xff);
    data_->push_back((value >> 8) & 0xff);
    data_->push_back((value >> 16) & 0xff);
    data_->push_back((value >> 24) & 0xff);
  }

  void PushUint32(int value) {
    DCHECK_GE(value, 0);
    PushUint32(static_cast<uint32_t>(value));
  }

  void PushUint32(uint64_t value) {
    DCHECK_LE(value, UINT32_MAX);
    PushUint32(static_cast<uint32_t>(value));
  }

  void PushUint64(uint64_t value) {
    data_->push_back((value >> 0) & 0xff);
    data_->push_back((value >> 8) & 0xff);
    data_->push_back((value >> 16) & 0xff);
    data_->push_back((value >> 24) & 0xff);
    data_->push_back((value >> 32) & 0xff);
    data_->push_back((value >> 40) & 0xff);
    data_->push_back((value >> 48) & 0xff);
    data_->push_back((value >> 56) & 0xff);
  }

  void PushInt8(int value) {
    DCHECK_GE(value, INT8_MIN);
    DCHECK_LE(value, INT8_MAX);
    PushUint8(static_cast<uint8_t>(value));
  }

  void PushInt16(int value) {
    DCHECK_GE(value, INT16_MIN);
    DCHECK_LE(value, INT16_MAX);
    PushUint16(static_cast<uint16_t>(value));
  }

  void PushInt32(int value) {
    PushUint32(static_cast<uint32_t>(value));
  }

  void PushInt64(int64_t value) {
    PushUint64(static_cast<uint64_t>(value));
  }

  // Variable-length encoders

  void PushUleb128(uint32_t value) {
    EncodeUnsignedLeb128(data_, value);
  }

  void PushUleb128(int value) {
    DCHECK_GE(value, 0);
    EncodeUnsignedLeb128(data_, value);
  }

  void PushSleb128(int value) {
    EncodeSignedLeb128(data_, value);
  }

  // Miscellaneous functions

  void PushString(const char* value) {
    data_->insert(data_->end(), value, value + strlen(value) + 1);
  }

  void PushData(const void* ptr, size_t size) {
    const char* p = reinterpret_cast<const char*>(ptr);
    data_->insert(data_->end(), p, p + size);
  }

  void UpdateUint32(size_t offset, uint32_t value) {
    DCHECK_LT(offset + 3, data_->size());
    (*data_)[offset + 0] = (value >> 0) & 0xFF;
    (*data_)[offset + 1] = (value >> 8) & 0xFF;
    (*data_)[offset + 2] = (value >> 16) & 0xFF;
    (*data_)[offset + 3] = (value >> 24) & 0xFF;
  }

  void UpdateUint64(size_t offset, uint64_t value) {
    DCHECK_LT(offset + 7, data_->size());
    (*data_)[offset + 0] = (value >> 0) & 0xFF;
    (*data_)[offset + 1] = (value >> 8) & 0xFF;
    (*data_)[offset + 2] = (value >> 16) & 0xFF;
    (*data_)[offset + 3] = (value >> 24) & 0xFF;
    (*data_)[offset + 4] = (value >> 32) & 0xFF;
    (*data_)[offset + 5] = (value >> 40) & 0xFF;
    (*data_)[offset + 6] = (value >> 48) & 0xFF;
    (*data_)[offset + 7] = (value >> 56) & 0xFF;
  }

  void Pad(int alignment) {
    DCHECK_NE(alignment, 0);
    DCHECK_EQ((alignment & (alignment - 1)), 0);  // is power of 2?
    while ((data_->size() & (alignment - 1)) != 0) {
      data_->push_back(0);
    }
  }

  const std::vector<uint8_t>* data() const {
    return data_;
  }

  explicit Writer(std::vector<uint8_t>* buffer) : data_(buffer) { }

 protected:
  std::vector<uint8_t>* data_;
};

}  // namespace dwarf
}  // namespace art

#endif  // ART_COMPILER_DWARF_WRITER_H_
