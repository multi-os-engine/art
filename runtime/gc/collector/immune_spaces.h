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

#ifndef ART_RUNTIME_GC_COLLECTOR_IMMUNE_SPACES_H_
#define ART_RUNTIME_GC_COLLECTOR_IMMUNE_SPACES_H_

#include "base/macros.h"
#include "base/mutex.h"
#include "immune_region.h"

#include <set>

namespace art {
namespace gc {
namespace space {
class ContinuousSpace;
}  // namespace space

namespace collector {

// ImmuneSpaces is a set of spaces which are not going to have any objects become marked during the
// GC.
class ImmuneSpaces {
  class CompareByBegin {
   public:
    bool operator()(space::ContinuousSpace* a, space::ContinuousSpace* b) const;
  };

 public:
  ImmuneSpaces() {}
  void Reset();

  // Setup the immune region to the largest continuous set of spaces.
  void CreateLargestImmuneRegion();

  // Add a continuous space to the immune spaces set.
  void AddSpace(space::ContinuousSpace* space) REQUIRES(Locks::heap_bitmap_lock_);

  // Return the immune region.
  ALWAYS_INLINE const ImmuneRegion& GetImmuneRegion() const {
    return immune_region_;
  }

  // Return true if the spaces is contained.
  bool ContainsSpace(space::ContinuousSpace* space) const;

  // Return the set of spaces in the immune region.
  const std::set<space::ContinuousSpace*, CompareByBegin>& GetSpaces() {
    return spaces_;
  }

 private:
  std::set<space::ContinuousSpace*, CompareByBegin> spaces_;
  ImmuneRegion immune_region_;
};

}  // namespace collector
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_COLLECTOR_IMMUNE_SPACES_H_
