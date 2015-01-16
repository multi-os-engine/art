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

#ifndef ART_RUNTIME_LOCK_WORD_INL_H_
#define ART_RUNTIME_LOCK_WORD_INL_H_

#include "lock_word.h"
#include "monitor_pool.h"
#include "thread_list.h"

namespace art {

inline uint32_t LockWord::ThinLockOwner() const {
  DCHECK(GetState() == kThinLockBiasable || GetState() == kThinLockNotBiasable);
  return (value_ >> kThinLockOwnerShift) & kThinLockOwnerMask;
}

inline uint32_t LockWord::BiasLockOwner() const {
  DCHECK_EQ(GetState(), kBiasLocked);
  return (value_ >> kBiasLockOwnerShift) & kBiasLockOwnerMask;
}

inline uint32_t LockWord::ThinLockBiasableCount() const {
  DCHECK_EQ(GetState(), kThinLockBiasable);
  return (value_ >> kThinLockBiasableCountShift) & kThinLockBiasableCountMask;
}

inline uint32_t LockWord::ThinLockBiasableProfCount() const {
  DCHECK_EQ(GetState(), kThinLockBiasable);
  return (value_ >> kThinLockBiasableProfShift) & kThinLockBiasableProfMask;
}

inline uint32_t LockWord::ThinLockNotBiasableCount() const {
  DCHECK_EQ(GetState(), kThinLockNotBiasable);
  return (value_ >> kThinLockNotBiasableCountShift) & kThinLockNotBiasableCountMask;
}

inline uint32_t LockWord::BiasLockCount() const {
  DCHECK_EQ(GetState(), kBiasLocked);
  return (value_ >> kBiasLockCountShift) & kBiasLockCountMask;
}

inline bool LockWord::IsThinLockUnlocked() const {
  DCHECK(GetState() == kThinLockBiasable || GetState() == kThinLockNotBiasable);
  if (GetState() == kThinLockBiasable) {
    return ThinLockBiasableCount() == 0;
  } else {
    return ThinLockNotBiasableCount() == 0;
  }
}

inline bool LockWord::IsBiasLockUnlocked() const {
  DCHECK_EQ(GetState(), kBiasLocked);
  return BiasLockCount() == 0;
}

inline Monitor* LockWord::FatLockMonitor() const {
  DCHECK_EQ(GetState(), kFatLocked);
  MonitorId mon_id = value_ & ~(kStateMask << kStateShift);
  return MonitorPool::MonitorFromMonitorId(mon_id);
}

inline size_t LockWord::ForwardingAddress() const {
  DCHECK_EQ(GetState(), kForwardingAddress);
  return value_ << kStateSize;
}

inline LockWord::LockWord() : value_(0) {
  DCHECK_EQ(GetState(), kUnlocked);
}

inline LockWord::LockWord(Monitor* mon)
    : value_(mon->GetMonitorId() | (kStateFat << kStateShift)) {
  DCHECK_EQ(FatLockMonitor(), mon);
  DCHECK_LE(mon->GetMonitorId(), static_cast<uint32_t>(kMaxMonitorId));
}

inline int32_t LockWord::GetHashCode() const {
  DCHECK_EQ(GetState(), kHashCode);
  return (value_ >> kHashShift) & kHashMask;
}

}  // namespace art

#endif  // ART_RUNTIME_LOCK_WORD_INL_H_
