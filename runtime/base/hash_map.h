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

#ifndef ART_RUNTIME_BASE_HASH_MAP_H_
#define ART_RUNTIME_BASE_HASH_MAP_H_

#include <utility>

#include "hash_set.h"

namespace art {

template <typename Fn>
class HashMapWrapper {
 public:
  // Hash function.
  template <class Key, class Value>
  size_t operator()(const std::pair<Key, Value>& pair) const {
    return fn_(pair.first);
  }
  template <class Key>
  size_t operator()(const Key& key) const {
    return fn_(key);
  }
  template <class Key, class Value>
  bool operator()(const std::pair<Key, Value>& a, const std::pair<Key, Value>& b) const {
    return fn_(a.first, b.first);
  }
  template <class Key, class Value, class Element>
  bool operator()(const std::pair<Key, Value>& a, const Element& element) const {
    return fn_(a.first, element);
  }

 private:
  Fn fn_;
};

template <class Key, class Value, class EmptyFn,
    class HashFn = std::hash<Key>, class Pred = std::equal_to<Key>,
    class Alloc = std::allocator<std::pair<Key, Value>>>
class HashMap : public HashSet<std::pair<Key, Value>, EmptyFn, HashMapWrapper<HashFn>,
                               HashMapWrapper<Pred>, Alloc> {
 public:
  using KeyType = Key;
  using ValueType = std::pair<Key, Value>;  // note: intentionally shadow HashSet::ValueType

  // Insert the key/value pair, allows duplicates. Copies the key and the value.
  void Insert(const Key& key, const Value& value) {
    Insert(std::make_pair(key, value));
  }

  // Insert the key/value pair, allows duplicates. Moves-from the key, copies the value.
  void Insert(Key&& key, const Value& value) {
    Insert(std::make_pair(std::move(key), value));
  }

  // Insert the key/value pair, allows duplicates. Copies the key, moves-from the value.
  void Insert(const Key& key, Value&& value) {
    Insert(std::make_pair(key, std::move(value)));
  }

  // Insert the key/value pair, allows duplicates. Moves-from the key and the value.
  void Insert(Key&& key, Value&& value) {
    Insert(std::make_pair(std::move(key), std::move(value)));
  }

  using HashSet<
      std::pair<Key, Value>, EmptyFn, HashMapWrapper<HashFn>, HashMapWrapper<Pred>, Alloc>::Insert;
};

}  // namespace art

#endif  // ART_RUNTIME_BASE_HASH_MAP_H_
