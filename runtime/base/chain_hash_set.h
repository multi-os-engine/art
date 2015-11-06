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

#ifndef ART_RUNTIME_BASE_CHAIN_HASH_SET_H_
#define ART_RUNTIME_BASE_CHAIN_HASH_SET_H_

#include "bit_utils.h"
#include "hash_set.h"  // For the DefaultEmptyFn<T>.

#include <functional>
#include <memory>
#include <vector>

namespace art {

/**
 * ChainHashSet is a hash table using separate chains to resolve
 * collisions. It makes the table faster than open addressing
 * approach.
 * The table should be filled from existing data and shouldn't be
 * modified by inserting of new values after the filling.
 * The table uses an extra 32-bit field for each entry to store the
 * index of the next element in the collision chain and a part of the
 * hash value of the current element.
 * When data is being looked up the table compares hash parts first
 * to eliminate redundant string comparisons.
 */
template <class T, class EmptyFn = DefaultEmptyFn<T>, class HashFn = std::hash<T>,
    class Pred = std::equal_to<T>>
class ChainHashSet {
  // The iterator for the container.
  class BaseIterator {
   public:
    BaseIterator(const BaseIterator&) = default;
    BaseIterator(ChainHashSet* hash_set, size_t index)
        : index_(index),
          hash_set_(hash_set) {
    }
    BaseIterator& operator=(const BaseIterator&) = default;

    bool operator==(const BaseIterator& other) const {
      return hash_set_ == other.hash_set_ && index_ == other.index_;
    }

    bool operator!=(const BaseIterator& other) const {
      return !(*this == other);
    }

    BaseIterator operator++() {  // Value after modification.
      index_ = NextNonEmptySlot(index_, hash_set_);
      return *this;
    }

    T& operator*() const {
      return hash_set_->ElementForIndex(index_).data;
    }

    T* operator->() const {
      return &**this;
    }

   private:
    size_t index_;
    ChainHashSet* hash_set_;

    size_t NextNonEmptySlot(size_t index, const ChainHashSet* hash_set) const {
      const size_t num_buckets = hash_set->num_buckets_;
      do {
        ++index;
      } while (index < num_buckets && hash_set->IsFreeSlot(index));
      return index;
    }

    friend class ChainHashSet;
  };

 public:
  using iterator = BaseIterator;

  // Because of separate chains approach and saving of data hash the amount
  // of string comparisons is very few. We can set the load factor to 1
  // without speed regression.
  static constexpr double kDefaultLoadFactor = 1;
  static_assert(0.0 < kDefaultLoadFactor && kDefaultLoadFactor <= 1.0,
                "Default load factor must be 0 < kDefaultLoadFactor <= 1");

  explicit ChainHashSet(double load_factor = kDefaultLoadFactor)
      : num_elements_(0u),
        num_buckets_(0u),
        owns_data_(false),
        data_(nullptr),
        index_mask_(0),
        hash_mask_(~0),
        load_factor_(load_factor) {
    DCHECK_GT(load_factor_, 0.0);
    DCHECK_LE(load_factor_, 1.0);
  }

  // Fill the table with known data starting with elements
  // which don't have collisions.
  template <class K>
  void Fill(const K& data) {
    Clear();
    AllocateStorage(static_cast<size_t>(data.Size() / load_factor_) + 1);
    // Fill with no collision data first.
    std::vector<T> collision_data;
    for (auto it = data.begin(); it != data.end(); ++it) {
      if (!InsertToInitialPos(*it, hashfn_(*it))) {
        collision_data.push_back(*it);
      }
    }
    // Fill with collision data.
    for (auto it : collision_data) {
      size_t hash = hashfn_(it);
      size_t idx = IndexForHash(hash);
      size_t slot = FirstAvailableSlot(idx);
      Entry& entry = GetLastEntryInChain(idx);
      SetIndexPart(entry, slot);
      SetHashPart(data_[slot], hash);
      data_[slot].data = it;
      ++num_elements_;
    }
  }

  // Construct from existing data.
  // Read from a block of memory.
  ChainHashSet(const uint8_t* ptr, size_t* read_count) {
    uint32_t temp;
    size_t offset = 0;
    offset = ReadFromBytes(ptr, offset, &temp);
    num_elements_ = static_cast<size_t>(temp);
    offset = ReadFromBytes(ptr, offset, &temp);
    num_buckets_ = static_cast<size_t>(temp);
    offset = ReadFromBytes(ptr, offset, &load_factor_);
    owns_data_ = false;
    data_ = const_cast<Entry*>(reinterpret_cast<const Entry*>(ptr + offset));
    offset += sizeof(*data_) * num_buckets_;
    index_mask_ = RoundUpToPowerOfTwo(num_buckets_) - 1;
    hash_mask_ = ~index_mask_;
    // Caller responsible for aligning.
    *read_count = offset;
  }

  // Returns how large the table is after being written. If target is null,
  // then no writing happens but the size is still returned. Target must be
  // 8 byte aligned.
  size_t WriteToMemory(uint8_t* ptr) const {
    size_t offset = 0;
    offset = WriteToBytes(ptr, offset, static_cast<uint32_t>(num_elements_));
    offset = WriteToBytes(ptr, offset, static_cast<uint32_t>(num_buckets_));
    offset = WriteToBytes(ptr, offset, load_factor_);
    // Write elements, not that this may not be safe for cross compilation if
    // the elements are pointer sized.
    for (size_t i = 0; i < num_buckets_; ++i) {
      offset = WriteToBytes(ptr, offset, data_[i]);
    }
    // Caller responsible for aligning.
    return offset;
  }

  ~ChainHashSet() {
    DeallocateStorage();
  }

  // Move all table data form the other table one into the
  // current one.
  ChainHashSet& operator=(ChainHashSet&& other) {
    std::swap(data_, other.data_);
    std::swap(num_buckets_, other.num_buckets_);
    std::swap(num_elements_, other.num_elements_);
    std::swap(load_factor_, other.load_factor_);
    std::swap(owns_data_, other.owns_data_);
    std::swap(index_mask_, other.index_mask_);
    std::swap(hash_mask_, other.hash_mask_);
    return *this;
  }

  // Lower case for c++11 for each.
  iterator begin() {
    iterator ret(this, 0);
    if (num_buckets_ != 0 && IsFreeSlot(ret.index_)) {
      ++ret;  // Skip all the empty slots.
    }
    return ret;
  }

  // Lower case for c++11 for each.
  iterator end() {
    return iterator(this, num_buckets_);
  }

  bool Empty() const {
    return Size() == 0;
  }

  // Find an element, returns end() if not found.
  ALWAYS_INLINE iterator Find(const T& element) {
    if (emptyfn_.IsEmpty(element)) {
      return end();
    }
    return iterator(this, FindIndex(element, hashfn_(element)));
  }

  // Erase algorithm:
  // Make an empty slot where the iterator is pointing.
  // Shift the whole chain to fill the empty index. As the result
  // the last entry in the chain will be empty.
  iterator Erase(iterator it) {
    DCHECK(it != end());
    DCHECK(!IsFreeSlot(it.index_));
    size_t slot_id = it.index_;
    Entry* slot = &ElementForIndex(slot_id);
    --num_elements_;
    if (!IsLastElementInChain(*slot, slot_id)) {
      size_t next_slot_id = GetIndexPart(*slot);
      Entry* next_slot = &ElementForIndex(next_slot_id);
      while (!IsLastElementInChain(*next_slot, next_slot_id)) {
        slot->data = next_slot->data;
        SetHashPart(*slot, GetHashPart(*next_slot));
        slot = next_slot;
        slot_id = next_slot_id;
        next_slot_id = GetIndexPart(*slot);
        next_slot = &ElementForIndex(next_slot_id);
      }
      slot->data = next_slot->data;
      SetHashPart(*slot, GetHashPart(*next_slot));
      SetIndexPart(*slot, slot_id);
      emptyfn_.MakeEmpty(next_slot->data);
      return it;
    } else {
      emptyfn_.MakeEmpty(slot->data);
      return ++it;
    }
  }

  // Return the number of elements in the table.
  inline size_t Size() const {
    return num_elements_;
  }

 private:
  // The structure to wrap data and index of the next entry in a collision
  // chain.
  struct Entry {
    T data;
    // The field info consists of two parts: the hash value of the data (most
    // significant part) and the index of the next element in a collision
    // chain (less significant part) according with the mask specified.
    uint32_t info;
  };

  // The allocator for new Entries.
  using Alloc_ = std::allocator<Entry>;

  void SetHashPart(Entry& slot, size_t hash) const {
    slot.info = (slot.info & index_mask_) |
                             (static_cast<uint32_t>(hash) & hash_mask_);
  }

  void SetIndexPart(Entry& slot, size_t next_idx) const {
    slot.info = (slot.info & hash_mask_) |
                             (static_cast<uint32_t>(next_idx) & index_mask_);
  }

  size_t GetHashPart(const Entry& slot) const {
    return slot.info & hash_mask_;
  }

  size_t GetIndexPart(const Entry& slot) const {
    return slot.info & index_mask_;
  }

  bool IsLastElementInChain(const Entry& slot, size_t slot_id) const {
    return GetIndexPart(slot) == slot_id;
  }

  // Create a new array for the future data with given size.
  void Clear() {
    DeallocateStorage();
  }

  // Insert an element with the hash.
  bool InsertToInitialPos(const T& element, size_t hash) {
    size_t idx = IndexForHash(hash);
    if (!emptyfn_.IsEmpty(data_[idx].data)) {
      return false;
    }
    data_[idx].data = element;
    SetHashPart(data_[idx], hash);
    ++num_elements_;
    return true;
  }

  // Get the element by its index.
  inline Entry& ElementForIndex(size_t index) const {
    return data_[index];
  }

  // Calculate the index by a hash value.
  inline size_t IndexForHash(size_t hash) const {
    return hash % num_buckets_;
  }

  // Return the next index in the table.
  inline size_t NextIndex(size_t index) const {
    return ++index % num_buckets_;
  }

  // Find the hash table slot for an element, or return num_buckets_ if not
  // found. This value for not found is important so that
  // iterator(this, FindIndex(...)) == end().
  ALWAYS_INLINE size_t FindIndex(const T& element, size_t hash) const {
    if (UNLIKELY(num_buckets_ == 0)) {
      return num_buckets_;
    }
    size_t index = IndexForHash(hash);
    if (UNLIKELY(emptyfn_.IsEmpty(ElementForIndex(index).data))) {
      return num_buckets_;
    }
    const uint32_t masked_hash = hash & hash_mask_;
    while (true) {
      const Entry& slot = ElementForIndex(index);
      // Check if we've found the element.
      if (masked_hash == GetHashPart(slot) && pred_(slot.data, element)) {
        return index;
      }
      // Check if we've achieved the last element in the chain.
      if (IsLastElementInChain(slot, index)) {
        return num_buckets_;
      }
      index = GetIndexPart(slot);
    }
  }

  // Check if the slot specified by an index contains no data.
  inline bool IsFreeSlot(size_t index) const {
    return emptyfn_.IsEmpty(ElementForIndex(index).data);
  }

  // Get the last entry in the collision chain started with a given index.
  Entry& GetLastEntryInChain(size_t index) const {
    while (!IsLastElementInChain(data_[index], index)) {
      index = GetIndexPart(data_[index]);
    }
    return ElementForIndex(index);
  }

  // Allocate a number of buckets.
  void AllocateStorage(size_t num_buckets) {
    num_buckets_ = num_buckets;
    index_mask_ = RoundUpToPowerOfTwo(num_buckets_) - 1;
    hash_mask_ = ~index_mask_;
    data_ = allocfn_.allocate(num_buckets_);
    owns_data_ = true;
    for (size_t i = 0; i < num_buckets_; ++i) {
      allocfn_.construct(allocfn_.address(data_[i]));
      emptyfn_.MakeEmpty(data_[i].data);
      SetIndexPart(data_[i], i);
      SetHashPart(data_[i], 0);
    }
    num_elements_ = 0;
  }

  // Remove all data if we own it and release the data pointer.
  // This method zeros out the num_buckets_ variable.
  void DeallocateStorage() {
    if (num_buckets_ != 0) {
      if (owns_data_) {
        for (size_t i = 0; i < num_buckets_; ++i) {
          allocfn_.destroy(allocfn_.address(data_[i]));
        }
        allocfn_.deallocate(data_, num_buckets_);
        owns_data_ = false;
      }
      data_ = nullptr;
      num_buckets_ = 0;
    }
  }

  // Get the first avaliable (empty) slot.
  size_t FirstAvailableSlot(size_t index) const {
    while (!emptyfn_.IsEmpty(data_[index].data)) {
      index = NextIndex(index);
    }
    return index;
  }

  // Return new offset.
  template <typename Elem>
  static size_t WriteToBytes(uint8_t* ptr, size_t offset, Elem n) {
    if (ptr != nullptr) {
      *reinterpret_cast<Elem*>(ptr + offset) = n;
    }
    return offset + sizeof(n);
  }

  template <typename Elem>
  static size_t ReadFromBytes(const uint8_t* ptr, size_t offset, Elem* out) {
    *out = *reinterpret_cast<const Elem*>(ptr + offset);
    return offset + sizeof(*out);
  }

  Alloc_ allocfn_;  // Allocator function.
  HashFn hashfn_;  // Hashing function.
  EmptyFn emptyfn_;  // IsEmpty/SetEmpty function.
  Pred pred_;  // Equals function.
  size_t num_elements_;  // Number of inserted elements.
  size_t num_buckets_;  // Number of hash table buckets.
  bool owns_data_;  // If we own data_ and are responsible for freeing it.
  Entry* data_;  // Backing storage.
  // The mask for the next_index part of the info field of Entry objects.
  uint32_t index_mask_;
  // The mask for the hash part of the info field of Entry objects.
  uint32_t hash_mask_;
  double load_factor_;
};

}  // namespace art

#endif  // ART_RUNTIME_BASE_CHAIN_HASH_SET_H_
