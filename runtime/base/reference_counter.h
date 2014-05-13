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

#ifndef ART_RUNTIME_BASE_REFERENCE_COUNTER_H_
#define ART_RUNTIME_BASE_REFERENCE_COUNTER_H_

#include "logging.h"

namespace art {

class ReferenceCounter {
 public:
  ReferenceCounter() : counter_(0) {}

  void Reset() {
    counter_ = 0;
  }

  // Returns true if the counter was set to 0, false otherwise.
  bool Increment() {
    bool result = (counter_ == 0);
    ++counter_;
    return result;
  }

  // Returns true if the counter reaches 0, false otherwise.
  bool Decrement() {
    DCHECK_GT(counter_, 0U);
    --counter_;
    return (counter_ == 0);
  }

  size_t GetCounter() const {
    return counter_;
  }

 private:
  size_t counter_;

  DISALLOW_COPY_AND_ASSIGN(ReferenceCounter);
};
}  // namespace art

#endif  // ART_RUNTIME_BASE_REFERENCE_COUNTER_H_
