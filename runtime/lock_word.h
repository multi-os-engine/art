/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_RUNTIME_LOCK_WORD_H_
#define ART_RUNTIME_LOCK_WORD_H_

#include <iosfwd>
#include <stdint.h>

#include "base/logging.h"
#include "utils.h"

namespace art {
namespace mirror {
  class Object;
}  // namespace mirror

class Monitor;

/* The lock value itself as stored in mirror::Object::monitor_. The three most significant bits of
 * the state. The possible states are thin lock biasable/unlocked, biased lock, thin lock not biasable,
 * fat lock, and hash code.
 * When the lock word is in the "thin lock biasable" state and its bits are formatted as follows:
 *
 *  |333|222222222|1111|1111110000000000|
 *  |109|876543210|9876|5432109876543210|
 *  |000|   lc    | pc |thread id owner |
 *  where lc stands for lock count and pc stands for profiling count.
 *
 * When the lock word is in the "biased" state and its bits are formatted as follows:
 *
 *  |333|2222222221111|1111110000000000|
 *  |109|8765432109876|5432109876543210|
 *  |001| lock count  |thread id owner |
 *
 * When the lock word is in the "thin lock not biasable" state and its bits are formatted as follows:
 *
 *  |333|2222222221111|1111110000000000|
 *  |109|8765432109876|5432109876543210|
 *  |010| lock count  |thread id owner |
 *
 * When the lock word is in the "fat" state and its bits are formatted as follows:
 *
 *  |333|22222222211111111110000000000|
 *  |109|87654321098765432109876543210|
 *  |011| MonitorId                   |
 *
 * When the lock word is in hash state and its bits are formatted as follows:
 *
 *  |333|22222222211111111110000000000|
 *  |109|87654321098765432109876543210|
 *  |100| HashCode                    |
 */
class LockWord {
 public:
  enum SizeShiftsAndMasks {  // private marker to avoid generate-operator-out.py from processing.
    // Number of bits to encode the state, currently just fat or thin biasable/unlocked, thin
    // not biasable, biased or hash code.
    kStateSize = 3,

    // Number of bits to encode the bias lock owner.
    kBiasLockOwnerSize = 16,
    // Remaining bits are the recursive lock count.
    kBiasLockCountSize = 32 - kBiasLockOwnerSize - kStateSize,
    // Bias lock bits. Owner in lowest bits.
    kBiasLockOwnerShift = 0,
    kBiasLockOwnerMask = (1 << kBiasLockOwnerSize) - 1,
    kBiasLockMaxOwner = kBiasLockOwnerMask,
    // Count in higher bits.
    kBiasLockCountShift = kBiasLockOwnerSize + kBiasLockOwnerShift,
    kBiasLockCountMask = (1 << kBiasLockCountSize) - 1,
    kBiasLockMaxCount = kBiasLockCountMask,

    // Two kinds of thin lock, biasable and not biasable. Currently, the layout of not biasable
    // is the same to bias lock. Thin lock biasable has two counters, recursive lock counter and
    // profiling counter. Profiling counter is used to predict whether the object would be
    // locked by only one thread.
    kThinLockOwnerSize = 16,
    kThinLockBiasableProfSize = 4,
    kThinLockBiasableCountSize = 32 - kThinLockOwnerSize - kStateSize - kThinLockBiasableProfSize,
    kThinLockOwnerShift = 0,
    kThinLockOwnerMask = (1 << kThinLockOwnerSize) - 1,
    kThinLockMaxOwner = kThinLockOwnerMask,
    kThinLockBiasableProfShift = kThinLockOwnerSize + kThinLockOwnerShift,
    kThinLockBiasableProfMask = (1 << kThinLockBiasableProfSize) - 1,
    kThinLockBiasableMaxProfCount = kThinLockBiasableProfMask,
    kThinLockBiasableCountShift = kThinLockBiasableProfSize + kThinLockBiasableProfShift,
    kThinLockBiasableCountMask = (1 << kThinLockBiasableCountSize) - 1,
    kThinLockBiasableMaxCount = kThinLockBiasableCountMask,

    kThinLockNotBiasableCountSize = 32 - kThinLockOwnerSize - kStateSize,
    kThinLockNotBiasableCountShift = kThinLockOwnerSize + kThinLockOwnerShift,
    kThinLockNotBiasableCountMask = (1 << kThinLockNotBiasableCountSize) - 1,
    kThinLockNotBiasableMaxCount = kThinLockNotBiasableCountMask,

    // State in the highest bits.
    kStateShift = kBiasLockCountSize + kBiasLockCountShift,
    kStateMask = (1 << kStateSize) - 1,
    kStateThinBiasableOrUnlocked = 0,
    kStateBias = 1,
    kStateThinNotBiasable = 2,
    kStateFat = 3,
    kStateHash = 4,
    kStateForwardingAddress = 5,

    // When the state is kHashCode, the non-state bits hold the hashcode.
    kHashShift = 0,
    kHashSize = 32 - kStateSize,
    kHashMask = (1 << kHashSize) - 1,
    kMaxHash = kHashMask,
    kMaxMonitorId = kMaxHash
  };

  static LockWord FromThinLockBiasableId(uint32_t thread_id, uint32_t prof_count, uint32_t count) {
    CHECK_LE(thread_id, static_cast<uint32_t>(kThinLockMaxOwner));
    CHECK_LE(count, static_cast<uint32_t>(kThinLockBiasableMaxCount));
    return LockWord((thread_id << kThinLockOwnerShift) | (prof_count << kThinLockBiasableProfShift) |
                    (count << kThinLockBiasableCountShift) | (kStateThinBiasableOrUnlocked << kStateShift));
  }

  static LockWord FromThinLockNotBiasableId(uint32_t thread_id, uint32_t count) {
    CHECK_LE(thread_id, static_cast<uint32_t>(kThinLockOwnerMask));
    return LockWord((thread_id << kThinLockOwnerShift) | (count << kThinLockNotBiasableCountShift) |
                    (kStateThinNotBiasable << kStateShift));
  }

  static LockWord FromBiasLockId(uint32_t thread_id, uint32_t count) {
    CHECK_LE(thread_id, static_cast<uint32_t>(kBiasLockOwnerMask));
    return LockWord((thread_id << kBiasLockOwnerShift) | (count << kBiasLockCountShift) |
                    (kStateBias << kStateShift));
  }

  static LockWord FromForwardingAddress(size_t target) {
    DCHECK(IsAligned < 1 << kStateSize>(target));
    return LockWord((target >> kStateSize) | (kStateForwardingAddress << kStateShift));
  }

  static LockWord FromHashCode(uint32_t hash_code) {
    CHECK_LE(hash_code, static_cast<uint32_t>(kMaxHash));
    return LockWord((hash_code << kHashShift) | (kStateHash << kStateShift));
  }

  enum LockState {
    kUnlocked,    // No lock owners. Would change into kThinLockBiasable state for a locking
                  // request by a thread.
    kThinLockBiasable,  // Thin lock with a profiling counter. Could change into a bias lock
                  // when the counter reaches a threadhold (only locked by one thread before
                  // the counter is full).
    kBiasLocked,  // Lock biased to a particular thread. The lock state will change from
                  // kThinLockBiasable to kBiasLocked when the profiling counter is full.
    kThinLockNotBiasable,  // Thin lock that cannot change into bias lock. When the lock was
                  // requested by the second thread (but no long time contension) in
                  // kThinLockBiasable state, the lock will change into kThinLockNotBiasable
                  // state.
    kFatLocked,   // See associated monitor.
    kHashCode,    // Lock word contains an identity hash.
    kForwardingAddress,  // Lock word contains the forwarding address of an object.
  };

  LockState GetState() const {
    if (UNLIKELY(value_ == 0)) {
      return kUnlocked;
    } else {
      uint32_t internal_state = (value_ >> kStateShift) & kStateMask;
      switch (internal_state) {
        case kStateThinBiasableOrUnlocked:
          return kThinLockBiasable;  // The thin lock biasable state use the same state
                                     // encoding (000) with the unlocked state. These
                                     // two states are distinguished by whether there
                                     // was a lock owner in the lock word.
        case kStateBias:
          return kBiasLocked;
        case kStateThinNotBiasable:
          return kThinLockNotBiasable;
        case kStateHash:
          return kHashCode;
        case kStateForwardingAddress:
          return kForwardingAddress;
        default:
          DCHECK_EQ(internal_state, static_cast<uint32_t>(kStateFat));
          return kFatLocked;
      }
    }
  }

  // Return the owner thin lock thread id.
  uint32_t ThinLockOwner() const;

  // Return the owner biased lock thread id.
  uint32_t BiasLockOwner() const;

  // Return the number of times a lock value has been locked in thin lock biasable state.
  uint32_t ThinLockBiasableCount() const;

  // Return the value of the profiling counter in thin lock biasable state.
  uint32_t ThinLockBiasableProfCount() const;

  // Return the number of times a lock value has been locked in thin lock not biasable state.
  uint32_t ThinLockNotBiasableCount() const;

  // Return the number of times a lock value has been locked in bias lock state.
  uint32_t BiasLockCount() const;

  // Return the Monitor encoded in a fat lock.
  Monitor* FatLockMonitor() const;

  // Return the forwarding address stored in the monitor.
  size_t ForwardingAddress() const;

  // Default constructor with no lock ownership.
  LockWord();

  // Constructor a lock word for inflation to use a Monitor.
  explicit LockWord(Monitor* mon);

  bool operator==(const LockWord& rhs) const {
    return GetValue() == rhs.GetValue();
  }

  // Whether the lock word was not locked in thin lock biasable or not biasable.
  bool IsThinLockUnlocked() const;

  // Whether the lock word was not locked in bias lock state.
  bool IsBiasLockUnlocked() const;

  // Return the hash code stored in the lock word, must be kHashCode state.
  int32_t GetHashCode() const;

  uint32_t GetValue() const {
    return value_;
  }

 private:
  explicit LockWord(uint32_t val) : value_(val) {}

  // Only Object should be converting LockWords to/from uints.
  friend class mirror::Object;

  // The encoded value holding all the state.
  uint32_t value_;
};
std::ostream& operator<<(std::ostream& os, const LockWord::LockState& code);

}  // namespace art


#endif  // ART_RUNTIME_LOCK_WORD_H_
