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
 * Expanding byte buffer, with primitives for appending basic data types.
 */
#ifndef ART_RUNTIME_JDWP_JDWP_EXPAND_BUF_H_
#define ART_RUNTIME_JDWP_JDWP_EXPAND_BUF_H_

#include <string>

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "jdwp/jdwp_constants.h"
#include "jdwp/jdwp_types.h"

namespace art {

namespace JDWP {

struct JdwpLocation;

/*
 * Data structure used to track buffer use.
 */
class ExpandBuf {
 public:
  ExpandBuf();

  ~ExpandBuf();

  /*
   * Accessors.  The buffer pointer and length will only be valid until more
   * data is added.
   */

  // Get a pointer to the start of the buffer.
  uint8_t* GetBuffer() {
    return storage_;
  }

  // Get the amount of data currently in the buffer.
  size_t GetLength() const {
    return current_length_;
  }

  // Allocates some space in the buffer and returns a pointer to the *start*
  // of the region added.
  uint8_t* AddSpace(size_t gapSize);

  /*
   * The "add" operations allocate additional storage and append the data.
   *
   * There are no "get" operations included with this "class", other than
   * GetBuffer().  If you want to get or set data from a position other
   * than the end, get a pointer to the buffer and use the inline functions
   * defined elsewhere.
   *
   */

  // Adds a byte.
  void Add1(uint8_t val);

  // Adds two big-endian bytes.
  void Add2BE(uint16_t val);

  // Adds four big-endian bytes.
  void Add4BE(uint32_t val);

  // Adds eight big-endian bytes.
  void Add8BE(uint64_t val);

  /*
   * Adds a UTF8 string as a 4-byte length followed by a non-nullptr-terminated
   * string.
   *
   * Because these strings are coming out of the VM, it's safe to assume that
   * they can be null-terminated (either they don't have null bytes or they
   * have stored null bytes in a multi-byte encoding).
   */
  void AddUtf8String(const char* s);
  void AddUtf8String(const std::string& s);
  void AddLocation(const JdwpLocation& location);

  void AddFieldId(FieldId id) {
    Add8BE(id);
  }

  void AddMethodId(MethodId id) {
    Add8BE(id);
  }

  void AddObjectId(ObjectId id) {
    Add8BE(id);
  }

  void AddRefTypeId(RefTypeId id) {
    Add8BE(id);
  }

  void AddFrameId(FrameId id) {
    Add8BE(id);
  }

  size_t CompleteReply(uint32_t request_id, JdwpError error);
  void CompleteEvent(uint32_t request_id);

 private:
  // Ensures that the buffer has enough space to hold incoming data. If it
  // doesn't, resizes the buffer.
  void EnsureSpace(size_t newCount);

  // The internal storage buffer. We use a malloc/realloc/free to manage memory.
  // TODO: we could use std::vector<uint8_t> but we'd lose the realloc benefit.
  uint8_t* storage_;
  size_t current_length_;
  size_t max_length_;

  DISALLOW_COPY_AND_ASSIGN(ExpandBuf);
};

}  // namespace JDWP

}  // namespace art

#endif  // ART_RUNTIME_JDWP_JDWP_EXPAND_BUF_H_
