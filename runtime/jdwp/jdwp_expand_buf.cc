/*
 * Copyright (C) 2008 The Android Open Source Project
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
/*
 * Implementation of an expandable byte buffer.  Designed for serializing
 * primitive values, e.g. JDWP replies.
 */

#include "jdwp/jdwp_expand_buf.h"

#include <stdlib.h>
#include <string.h>

#include "base/logging.h"
#include "jdwp/jdwp.h"
#include "jdwp/jdwp_bits.h"

namespace art {

namespace JDWP {

static constexpr size_t kInitialStorage = 64;

ExpandBuf::ExpandBuf() : storage_(nullptr), current_length_(0), max_length_(kInitialStorage) {
  storage_ = reinterpret_cast<uint8_t*>(malloc(kInitialStorage));
}

ExpandBuf::~ExpandBuf() {
  free(storage_);
}


void ExpandBuf::EnsureSpace(size_t newCount) {
  if (current_length_ + newCount <= max_length_) {
    return;
  }

  while (current_length_ + newCount > max_length_) {
    max_length_ *= 2;
  }

  uint8_t* newPtr = reinterpret_cast<uint8_t*>(realloc(storage_, max_length_));
  CHECK(newPtr != nullptr) << "realloc(" << max_length_ << ") failed";
  storage_ = newPtr;
}

uint8_t* ExpandBuf::AddSpace(size_t gapSize) {
  EnsureSpace(gapSize);
  uint8_t* gapStart = storage_ + current_length_;
  /* do we want to garbage-fill the gap for debugging? */
  current_length_ += gapSize;
  return gapStart;
}

void ExpandBuf::Add1(uint8_t val) {
  EnsureSpace(sizeof(val));
  *(storage_ + current_length_) = val;
  current_length_++;
}

void ExpandBuf::Add2BE(uint16_t val) {
  EnsureSpace(sizeof(val));
  Set2BE(storage_ + current_length_, val);
  current_length_ += sizeof(val);
}

void ExpandBuf::Add4BE(uint32_t val) {
  EnsureSpace(sizeof(val));
  Set4BE(storage_ + current_length_, val);
  current_length_ += sizeof(val);
}

void ExpandBuf::Add8BE(uint64_t val) {
  EnsureSpace(sizeof(val));
  Set8BE(storage_ + current_length_, val);
  current_length_ += sizeof(val);
}

static void SetUtf8String(uint8_t* buf, const char* str, size_t strLen) {
  Set4BE(buf, strLen);
  memcpy(buf + sizeof(uint32_t), str, strLen);
}

void ExpandBuf::AddUtf8String(const char* s) {
  size_t strLen = strlen(s);
  EnsureSpace(sizeof(uint32_t) + strLen);
  SetUtf8String(storage_ + current_length_, s, strLen);
  current_length_ += sizeof(uint32_t) + strLen;
}

void ExpandBuf::AddUtf8String(const std::string& s) {
  EnsureSpace(sizeof(uint32_t) + s.size());
  SetUtf8String(storage_ + current_length_, s.data(), s.size());
  current_length_ += sizeof(uint32_t) + s.size();
}

// TODO: move utility functions from jdwp.h to here.
void ExpandBuf::AddLocation(const JdwpLocation& location) {
  Add1(location.type_tag);
  AddObjectId(location.class_id);
  AddMethodId(location.method_id);
  Add8BE(location.dex_pc);
}

}  // namespace JDWP

}  // namespace art
