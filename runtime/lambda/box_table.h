/*
 * Copyright (C) 2015 The Android Open Source Project
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
#ifndef ART_RUNTIME_LAMBDA_BOX_TABLE_H_
#define ART_RUNTIME_LAMBDA_BOX_TABLE_H_

#include "base/hash_map.h"
#include "base/macros.h"
#include "base/mutex.h"
#include "object_callbacks.h"

#include <stdint.h>

namespace art {

class ArtMethod;  // forward declaration

namespace mirror {
class Object;
}  // namespace mirror

namespace lambda {

/*
 * Store a table of boxed lambdas. This is required to maintain object referential equality
 * when a lambda is re-boxed.
 *
 * Conceptually, we store a mapping of Closures -> Weak Reference<Boxed Lambda Object>.
 * When too many objects get GCd, we shrink the underlying table to use less space.
 */
class BoxTable FINAL {
 public:
  using ClosureType = art::ArtMethod*;

  // Boxes a closure into an object. Returns null and throws an exception on failure.
  mirror::Object* BoxLambda(const ClosureType& closure) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  // Unboxes an object back into the lambda. Returns false and throws an exception on failure.
  bool UnboxLambda(mirror::Object* object, ClosureType* out_closure)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Sweep weak references to lambda boxes. Update the addresses if the objects have been
  // moved, and delete them from the table if the objects have been cleaned up.
  void SweepWeakBoxedLambdas(IsMarkedCallback* visitor, void* arg)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  BoxTable() = default;
  ~BoxTable() = default;

  // should this be just RegisterBoxedLambda instead?
 private:
  using ValueType = mirror::Object*;

  // Attempt to look up the lambda in the map, or return null if it's not there yet.
  ValueType FindBoxedLambda(const ClosureType& closure) const
      SHARED_LOCKS_REQUIRED(Locks::lambda_table_lock_);
  bool RegisterBoxedLambda(const ClosureType& closure);


  // EmptyFn implementation for art::HashMap
  struct EmptyFn {
    void MakeEmpty(std::pair<ClosureType, ValueType>& item) const {
      item.first = nullptr;
    }
    bool IsEmpty(const std::pair<ClosureType, ValueType>& item) const {
      return item.first == nullptr;
    }
  };

  // HashFn implementation for art::HashMap
  struct HashFn {
    size_t operator()(const ClosureType& key) const {
      return static_cast<size_t>(reinterpret_cast<uintptr_t>(key));
    }
  };

  // TODO(iam): Write a custom hash function when ClosureType is no longer an ArtMethod*
  art::HashMap<ClosureType, ValueType, EmptyFn, HashFn> map_;

  // Shrink the map when we get below this load factor.
  static constexpr double kMinimumLoadFactor = decltype(map_)::kDefaultMinLoadFactor / 2;

  DISALLOW_COPY_AND_ASSIGN(BoxTable);
};

}  // namespace lambda
}  // namespace art

#endif  // ART_RUNTIME_LAMBDA_BOX_TABLE_H_
