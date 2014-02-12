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

#ifndef ART_COMPILER_DEX_DEBUG_LIFO_H_
#define ART_COMPILER_DEX_DEBUG_LIFO_H_

#include "base/logging.h"
#include "base/macros.h"
#include "globals.h"

namespace art {

// Helper classes for reference counting in debug mode with no overhead in release mode.

// Reference counter. No references allowed in destructor or in explicitly called CheckNoRefs().
template <bool IsDebug>
class DebugLifoRefCounterImpl;
// Reference. Allows an explicit check that it's the top reference.
template <bool IsDebug>
class DebugLifoReferenceImpl;
// Indirect top reference. Checks that the reference is the top reference when used.
template <bool IsDebug>
class DebugLifoIndirectTopRefImpl;

typedef DebugLifoRefCounterImpl<kIsDebugBuild> DebugLifoRefCounter;
typedef DebugLifoReferenceImpl<kIsDebugBuild> DebugLifoReference;
typedef DebugLifoIndirectTopRefImpl<kIsDebugBuild> DebugLifoIndirectTopRef;

// Release mode specializations. This should be optimized away.

template <>
class DebugLifoRefCounterImpl<false> {
 public:
  size_t IncrementRefCount() { return 0u; }
  void DecrementRefCount() { }
  size_t GetRefCount() const { return 0u; }
  void CheckNoRefs() const { }
};

template <>
class DebugLifoReferenceImpl<false> {
 public:
  explicit DebugLifoReferenceImpl(DebugLifoRefCounterImpl<false>* counter) { UNUSED(counter); }
  DebugLifoReferenceImpl(const DebugLifoReferenceImpl& other) = default;
  DebugLifoReferenceImpl& operator=(const DebugLifoReferenceImpl& other) = default;
  void CheckTop() { }
};

template <>
class DebugLifoIndirectTopRefImpl<false> {
 public:
  explicit DebugLifoIndirectTopRefImpl(DebugLifoReferenceImpl<false>* ref) { UNUSED(ref); }
  DebugLifoIndirectTopRefImpl(const DebugLifoIndirectTopRefImpl& other) = default;
  DebugLifoIndirectTopRefImpl& operator=(const DebugLifoIndirectTopRefImpl& other) = default;
  void CheckTop() { }
};

// Debug mode versions.

template <bool IsDebug>
class DebugLifoRefCounterImpl {
 public:
  DebugLifoRefCounterImpl() : ref_count_(0u) { }
  ~DebugLifoRefCounterImpl() { CheckNoRefs(); }
  size_t IncrementRefCount() { return ++ref_count_; }
  void DecrementRefCount() { --ref_count_; }
  size_t GetRefCount() const { return ref_count_; }
  void CheckNoRefs() const { CHECK_EQ(ref_count_, 0u); }

 private:
  size_t ref_count_;
};

template <bool IsDebug>
class DebugLifoReferenceImpl {
 public:
  explicit DebugLifoReferenceImpl(DebugLifoRefCounterImpl<IsDebug>* counter)
    : counter_(counter), ref_count_(counter->IncrementRefCount()) {
  }
  DebugLifoReferenceImpl(const DebugLifoReferenceImpl& other)
    : counter_(other.counter_), ref_count_(counter_->IncrementRefCount()) {
  }
  DebugLifoReferenceImpl& operator=(const DebugLifoReferenceImpl& other) {
    CHECK(counter_ == other.counter_);
    return *this;
  }
  ~DebugLifoReferenceImpl() { counter_->DecrementRefCount(); }
  void CheckTop() { CHECK_EQ(counter_->GetRefCount(), ref_count_); }

 private:
  DebugLifoRefCounterImpl<true>* counter_;
  size_t ref_count_;
};

template <bool IsDebug>
class DebugLifoIndirectTopRefImpl {
 public:
  explicit DebugLifoIndirectTopRefImpl(DebugLifoReferenceImpl<IsDebug>* ref)
      : ref_(ref) {
    CheckTop();
  }
  DebugLifoIndirectTopRefImpl(const DebugLifoIndirectTopRefImpl& other)
      : ref_(other.ref_) {
    CheckTop();
  }
  DebugLifoIndirectTopRefImpl& operator=(const DebugLifoIndirectTopRefImpl& other) {
    CHECK(ref_ == other->ref_);
    CheckTop();
    return *this;
  }
  ~DebugLifoIndirectTopRefImpl() {
    CheckTop();
  }
  void CheckTop() {
    ref_->CheckTop();
  }

 private:
  DebugLifoReferenceImpl<IsDebug>* ref_;
};

}  // namespace art

#endif  // ART_COMPILER_DEX_DEBUG_LIFO_H_
