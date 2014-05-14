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

#ifndef ART_RUNTIME_HANDLE_TABLE_INL_H_
#define ART_RUNTIME_HANDLE_TABLE_INL_H_

#include "handle_table.h"

namespace art {
  HandleTable::Iterator::Iterator(Slot* slot, Slot* limit) : slot_(slot), limit_(limit) {
    SkipNulls();
  }

  HandleTable::Iterator& HandleTable::Iterator::operator++() {
    ++slot_;
    SkipNulls();
    return *this;
  }

  HandleTable::Reference HandleTable::Iterator::operator*() {
    return slot_->GetTopReference();
  }

  intptr_t HandleTable::Iterator::Compare(const Iterator& rhs) const {
    return reinterpret_cast<uintptr_t>(slot_) - reinterpret_cast<uintptr_t>(rhs.slot_);
  }

  void HandleTable::Iterator::SkipNulls() {
    while (slot_ != limit_ && slot_->GetTopReference()->AsMirrorPtr() == nullptr) {
      ++slot_;
    }
  }

  HandleTable::Iterator HandleTable::Begin() {
    return HandleTable::Iterator(slots_, slots_ + top_index_);
  }

  HandleTable::Iterator HandleTable::End() {
    return HandleTable::Iterator(slots_ + top_index_, slots_ + top_index_);
  }

  bool inline operator==(const HandleTable::Iterator& lhs, const HandleTable::Iterator& rhs) {
    return lhs.Compare(rhs) == 0;
  }

  bool inline operator!=(const HandleTable::Iterator& lhs, const HandleTable::Iterator& rhs) {
    return lhs.Compare(rhs) != 0;
  }
}  // namespace art

#endif  // ART_RUNTIME_HANDLE_TABLE_INL_H_
