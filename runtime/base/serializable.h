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

#ifndef ART_RUNTIME_BASE_SERIALIZABLE_H_
#define ART_RUNTIME_BASE_SERIALIZABLE_H_

#include <iostream>
#include <stdint.h>

namespace art {

class Serializable {
 public:
  virtual ~Serializable() {}
  virtual void Serialize(uint8_t* output, size_t buf_size) const = 0;
  virtual bool Deserialize(const uint8_t* input) = 0;
  virtual void Dump(::std::ostream& os) const = 0;
  virtual size_t GetBytesNeededToSerialize() const = 0;
};

}  // namespace art

#endif  // ART_RUNTIME_BASE_SERIALIZABLE_H_
