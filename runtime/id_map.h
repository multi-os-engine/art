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

#ifndef ART_RUNTIME_ID_MAP_H_
#define ART_RUNTIME_ID_MAP_H_

#include "base/hash_set.h"
#include "base/logging.h"

namespace art {

// Id map assigns ids based on keys, same keys will get the same ids. Not thread safe.
template <class Key,
          class Counter,
          class HashFn = std::hash<Key>,
          class Pred = std::equal_to<Key>>
class IdMap {
  class HashCompare {
   public:
    explicit HashCompare(const IdMap& map) : map_(map) {}

    size_t operator()(const Counter& item) const {
      return operator()(map_.GetKey(item));
    }

    size_t operator()(const Key& key) const {
      return hash_(key);
    }

    bool operator()(const Counter& lhs, const Counter& rhs) const {
      return operator()(lhs, map_.GetKey(rhs));
    }

    // For insertion we don't have an index to compare against. So we use this comparator.
    bool operator()(const Counter& lhs, const Key& rhs) const {
      return pred_(map_.GetKey(lhs), rhs);
    }

   private:
    const IdMap& map_;
    HashFn hash_;
    Pred pred_;
  };

  class EmptyFn {
    static constexpr Counter kEmptySlot = static_cast<Counter>(-1);
   public:
    void MakeEmpty(Counter& item) const {
      item = kEmptySlot;
    }

    bool IsEmpty(const Counter& item) const {
      return item == kEmptySlot;
    }
  };

  typedef HashSet<Counter, EmptyFn, HashCompare, HashCompare> MapType;

 public:
  explicit IdMap(Counter initial)
      : hash_compare_(*this),
        ids_(hash_compare_, hash_compare_, alloc_),
        initial_(initial),
        counter_(initial) {}

  // If the key is already added, return the corresponding id. Otherwise return a new id and add
  // the key.
  Counter Add(const Key& key) {
    auto it = ids_.Find(key);
    if (it != ids_.end()) {
      return *it;
    }
    DCHECK_EQ(counter_ - initial_, keys_.size());
    keys_.push_back(key);
    Counter old_counter = counter_++;
    ids_.Insert(old_counter);
    DCHECK_EQ(GetKey(old_counter), key);
    return old_counter;
  }

  // Find the counter associated with a key. Returns null if not found, otherwise returns the
  // address of the counter.
  const Counter* Find(const Key& key) const {
    auto it = ids_.Find(key);
    if (it == ids_.end()) {
      return nullptr;
    }
    return &*it;
  }

  // For use with for each loops.
  typename MapType::iterator begin() {
    return ids_.begin();
  }

  // For use with for each loops.
  typename MapType::iterator end() {
    return ids_.end();
  }

  // Return the key for a given id.
  const Key& GetKey(Counter id) const {
    DCHECK_LT(id, counter_);
    DCHECK_LT(id - initial_, keys_.size());
    return keys_[id - initial_];
  }

 private:
  std::allocator<Counter> alloc_;
  HashCompare hash_compare_;
  MapType ids_;
  const Counter initial_;
  Counter counter_;
  std::vector<Key> keys_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(IdMap);
};

}  // namespace art

#endif  // ART_RUNTIME_ID_MAP_H_
