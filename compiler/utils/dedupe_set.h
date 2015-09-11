/*
 * Copyright (C) 2013 The Android Open Source Project
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

#ifndef ART_COMPILER_UTILS_DEDUPE_SET_H_
#define ART_COMPILER_UTILS_DEDUPE_SET_H_

#include <algorithm>
#include <inttypes.h>
#include <memory>
#include <set>
#include <string>

#include "base/mutex.h"
#include "base/hash_set.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "base/time_utils.h"
#include "utils/swap_space.h"

namespace art {

// A set of Keys that support a HashFunc returning HashType. Used to find duplicates of Key in the
// Add method. The data-structure is thread-safe through the use of internal locks, it also
// supports the lock being sharded.
template <typename InKey, typename StoreKey, typename Alloc, typename HashType, typename HashFunc,
          HashType kShard = 1>
class DedupeSet {
 public:
  const StoreKey* Add(Thread* self, const InKey& key) {
    uint64_t hash_start;
    if (kIsDebugBuild) {
      hash_start = NanoTime();
    }
    HashType raw_hash = HashFunc()(key);
    if (kIsDebugBuild) {
      uint64_t hash_end = NanoTime();
      hash_time_ += hash_end - hash_start;
    }
    HashType shard_hash = raw_hash / kShard;
    HashType shard_bin = raw_hash % kShard;
    return shards_[shard_bin]->Add(self, shard_hash, key);
  }

  DedupeSet(const char* set_name, const Alloc& alloc)
      : allocator_(alloc), hash_time_(0) {
    for (HashType i = 0; i < kShard; ++i) {
      std::ostringstream oss;
      oss << set_name << " lock " << i;
      shards_[i].reset(new Shard(&allocator_, oss.str()));
    }
  }

  ~DedupeSet() {
    // Everything done by member destructors.
  }

  std::string DumpStats() const {
    size_t collision_sum = 0;
    size_t collision_max = 0;
    for (HashType shard = 0; shard < kShard; ++shard) {
#if 0
      HashType last_hash = 0;
      size_t collision_cur_max = 0;
      for (const HashedKey& key : keys_[shard]) {
        DCHECK(key.store_ptr != nullptr);
        if (key.store_hash == last_hash) {
          collision_cur_max++;
          if (collision_cur_max > 1) {
            collision_sum++;
            if (collision_cur_max > collision_max) {
              collision_max = collision_cur_max;
            }
          }
        } else {
          collision_cur_max = 1;
          last_hash = key.store_hash;
        }
      }
#endif
    }
    return StringPrintf("%zu collisions, %zu max bucket size, %" PRIu64 " ns hash time",
                        collision_sum, collision_max, hash_time_);
  }

 private:
  template <typename T>
  class HashedKey {
   public:
    HashedKey() : hash_(0u), key_(nullptr) { }
    HashedKey(size_t hash, const T* key) : hash_(hash), key_(key) { }

    size_t Hash() const {
      return hash_;
    }

    const T* Key() const {
      return key_;
    }

    bool IsEmpty() const {
      return Key() == nullptr;
    }

    void MakeEmpty() {
      key_ = nullptr;
    }

   private:
    size_t hash_;
    const T* key_;
  };

  class ShardEmptyFn {
   public:
    bool IsEmpty(const HashedKey<StoreKey>& key) const {
      return key.IsEmpty();
    }

    void MakeEmpty(HashedKey<StoreKey>& key) {
      key.MakeEmpty();
    }
  };

  struct ShardHashFn {
    template <typename T>
    size_t operator()(const HashedKey<T>& key) const {
      return key.Hash();
    }
  };

  struct ShardPred {
    template <typename LeftT, typename RightT>
    bool operator()(const HashedKey<LeftT>& lhs, const HashedKey<RightT>& rhs) const {
      DCHECK(lhs.Key() != nullptr);
      DCHECK(rhs.Key() != nullptr);
      return lhs.Hash() == rhs.Hash() &&
          lhs.Key()->size() == rhs.Key()->size() &&
          std::equal(lhs.Key()->begin(), lhs.Key()->end(), rhs.Key()->begin());
    }
  };

  class Shard {
   public:
    Shard(Alloc* alloc, const std::string& lock_name)
        : alloc_(alloc),
          lock_name_(lock_name),
          lock_(lock_name_.c_str()),
          keys_() {
    }

    ~Shard() {
      for (const HashedKey<StoreKey>& key : keys_) {
        DCHECK(key.Key() != nullptr);
        alloc_->Destroy(key.Key());
      }
    }

    const StoreKey* Add(Thread* self, size_t hash, const InKey& in_key) REQUIRES(!lock_) {
      MutexLock lock(self, lock_);
      HashedKey<InKey> hashed_in_key(hash, &in_key);
      auto it = keys_.Find(hashed_in_key);
      if (it != keys_.end()) {
        DCHECK(it->Key() != nullptr);
        return it->Key();
      }
      const StoreKey* store_key = alloc_->Copy(in_key);
      keys_.Insert(HashedKey<StoreKey> { hash, store_key });
      return store_key;
    }

   private:
    Alloc* const alloc_;
    const std::string lock_name_;
    Mutex lock_;
    HashSet<HashedKey<StoreKey>, ShardEmptyFn, ShardHashFn, ShardPred> keys_;
  };

  Alloc allocator_;
  std::unique_ptr<Shard> shards_[kShard];
  uint64_t hash_time_;

  DISALLOW_COPY_AND_ASSIGN(DedupeSet);
};

}  // namespace art

#endif  // ART_COMPILER_UTILS_DEDUPE_SET_H_
