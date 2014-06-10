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

#ifndef ART_RUNTIME_GC_COLLECTOR_MARK_COMPACT_INL_H_
#define ART_RUNTIME_GC_COLLECTOR_MARK_COMPACT_INL_H_

#include "mark_compact.h"

#include "gc/accounting/heap_bitmap.h"
#include "mirror/object-inl.h"

namespace art {
namespace gc {
namespace collector {

class BitmapSetSlowPathVisitor {
 public:
  void operator()(const mirror::Object* obj) const {
    // Marking a large object, make sure its aligned as a sanity check.
    CHECK(IsAligned<kPageSize>(obj));
  }
};

}  // namespace collector
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_COLLECTOR_MARK_COMPACT_INL_H_
