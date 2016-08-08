/*
 * Copyright (C) 2014 The Android Open Source Project
 * Copyright 2014-2016 Intel Corporation
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

#ifndef ART_RUNTIME_MIRROR_OBJECT_REFERENCE_H_
#define ART_RUNTIME_MIRROR_OBJECT_REFERENCE_H_

#include "base/mutex.h"  // For Locks::mutator_lock_.
#include "globals.h"

namespace art {
namespace mirror {

class Object;

// Classes shared with the managed side of the world need to be packed so that they don't have
// extra platform specific padding.
#define MANAGED PACKED(4)

// Value type representing a reference to a mirror::Object of type MirrorType.
#ifdef MOE_WINDOWS
#pragma pack(push, 1)
#endif
template<bool kPoisonReferences, class MirrorType>
class MANAGED ObjectReference {
 public:
  MirrorType* AsMirrorPtr() const SHARED_REQUIRES(Locks::mutator_lock_) {
    return UnCompress();
  }

  void Assign(MirrorType* other) SHARED_REQUIRES(Locks::mutator_lock_) {
    reference_ = Compress(other);
  }

  void Clear() {
    reference_ = 0;
    DCHECK(IsNull());
  }

  bool IsNull() const {
    return reference_ == 0;
  }

#ifndef MOE
  uint32_t AsVRegValue() const {
    return reference_;
  }
#else
  uintptr_t AsVRegValue() const {
    return reinterpret_cast<uintptr_t>(reference_);
  }
#endif

 protected:
  ObjectReference<kPoisonReferences, MirrorType>(MirrorType* mirror_ptr)
      SHARED_REQUIRES(Locks::mutator_lock_)
      : reference_(Compress(mirror_ptr)) {
  }

#ifndef MOE
  // Compress reference to its bit representation.
  static uint32_t Compress(MirrorType* mirror_ptr) SHARED_REQUIRES(Locks::mutator_lock_) {
    uintptr_t as_bits = reinterpret_cast<uintptr_t>(mirror_ptr);
    return static_cast<uint32_t>(kPoisonReferences ? -as_bits : as_bits);
  }
#else
  static MirrorType* Compress(MirrorType* mirror_ptr) SHARED_REQUIRES(Locks::mutator_lock_) {
    return mirror_ptr;
  }
#endif

#ifndef MOE
  // Uncompress an encoded reference from its bit representation.
  MirrorType* UnCompress() const SHARED_REQUIRES(Locks::mutator_lock_) {
    uintptr_t as_bits = kPoisonReferences ? -reference_ : reference_;
    return reinterpret_cast<MirrorType*>(as_bits);
  }
#else
  MirrorType* UnCompress() const SHARED_REQUIRES(Locks::mutator_lock_) {
    return reference_;
  }
#endif

  friend class Object;

#ifndef MOE
  // The encoded reference to a mirror::Object.
  uint32_t reference_;
#else
  MirrorType* reference_;
#endif
};
#ifdef MOE_WINDOWS
#pragma pack(pop)
#endif

// References between objects within the managed heap.
#ifdef MOE_WINDOWS
#pragma pack(push, 1)
#endif
template<class MirrorType>
class MANAGED HeapReference : public ObjectReference<kPoisonHeapReferences, MirrorType> {
 public:
  static HeapReference<MirrorType> FromMirrorPtr(MirrorType* mirror_ptr)
      SHARED_REQUIRES(Locks::mutator_lock_) {
    return HeapReference<MirrorType>(mirror_ptr);
  }
 private:
  HeapReference<MirrorType>(MirrorType* mirror_ptr) SHARED_REQUIRES(Locks::mutator_lock_)
      : ObjectReference<kPoisonHeapReferences, MirrorType>(mirror_ptr) {}
};
#ifdef MOE_WINDOWS
#pragma pack(pop)
#endif

// Standard compressed reference used in the runtime. Used for StackReference and GC roots.
#ifdef MOE_WINDOWS
#pragma pack(push, 1)
#endif
template<class MirrorType>
class MANAGED CompressedReference : public mirror::ObjectReference<false, MirrorType> {
 public:
#ifndef MOE_WINDOWS
  CompressedReference<MirrorType>() SHARED_REQUIRES(Locks::mutator_lock_)
#else
  // MOE: This is needed to make Atomic<CompressedReference<...>> compatible with MSVC's C++
  // libraries.
  CompressedReference<MirrorType>() _NOEXCEPT SHARED_REQUIRES(Locks::mutator_lock_)
#endif
      : mirror::ObjectReference<false, MirrorType>(nullptr) {}

  static CompressedReference<MirrorType> FromMirrorPtr(MirrorType* p)
      SHARED_REQUIRES(Locks::mutator_lock_) {
    return CompressedReference<MirrorType>(p);
  }

 private:
  CompressedReference<MirrorType>(MirrorType* p) SHARED_REQUIRES(Locks::mutator_lock_)
      : mirror::ObjectReference<false, MirrorType>(p) {}
};
#ifdef MOE_WINDOWS
#pragma pack(pop)
#endif

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_OBJECT_REFERENCE_H_
