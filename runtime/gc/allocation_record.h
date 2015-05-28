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

#ifndef ART_RUNTIME_GC_ALLOCATION_RECORD_H_
#define ART_RUNTIME_GC_ALLOCATION_RECORD_H_

#include <list>

#include "base/mutex.h"
#include "object_callbacks.h"

namespace art {

class ArtMethod;
class Thread;

namespace mirror {
  class Class;
  class Object;
}

namespace gc {

class AllocRecordStackTraceElement {
 public:
  AllocRecordStackTraceElement() : method_(nullptr), dex_pc_(0) {}

  int32_t LineNumber() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtMethod* Method() {
    return method_;
  }

  void SetMethod(ArtMethod* m) {
    method_ = m;
  }

  void SetDexPc(uint32_t pc) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    dex_pc_ = pc;
  }

  bool operator==(const AllocRecordStackTraceElement& other) const {
    if (this == &other) return true;
    return method_ == other.method_ && dex_pc_ == other.dex_pc_;
  }

  friend struct HashAllocRecordStackTraceElement;

 private:
  ArtMethod* method_;
  uint32_t dex_pc_;
};

class AllocRecordStackTrace {
 public:
  static constexpr size_t kHashMultiplier = 17;
  static constexpr size_t kMaxAllocRecordStackDepth = 4;  // Max 255.

  explicit AllocRecordStackTrace(pid_t tid) : tid_(tid), depth_(0) {}

  pid_t GetTid() const {
    return tid_;
  }

  size_t GetDepth() const {
    return depth_;
  }

  void SetDepth(size_t depth) {
    depth_ = depth;
  }

  AllocRecordStackTraceElement* StackElement(size_t index) {
    DCHECK_LT(index, kMaxAllocRecordStackDepth);
    return &stack_[index];
  }

  bool operator==(const AllocRecordStackTrace& other) const {
    if (this == &other) return true;
    if (depth_ != other.depth_) return false;
    for (size_t i = 0; i < depth_; ++i) {
      if (!(stack_[i] == other.stack_[i])) return false;
    }
    return true;
  }

  friend struct HashAllocRecordStackTrace;

 private:
  const pid_t tid_;
  size_t depth_;
  AllocRecordStackTraceElement stack_[kMaxAllocRecordStackDepth];
};

struct HashAllocRecordStackTraceElement {
  size_t operator()(const AllocRecordStackTraceElement& r) const {
    return std::hash<void*>()(reinterpret_cast<void*>(r.method_)) *
        AllocRecordStackTrace::kHashMultiplier + std::hash<uint32_t>()(r.dex_pc_);
  }
};

struct HashAllocRecordStackTrace {
  size_t operator()(const AllocRecordStackTrace& r) const {
    size_t result = r.depth_;
    for (size_t i = 0; i < r.depth_; ++i) {
      result = result * AllocRecordStackTrace::kHashMultiplier
               + HashAllocRecordStackTraceElement()(r.stack_[i]);
    }
    return result;
  }
};

class AllocRecord {
 public:
  // all instances of AllocRecord should be managed by an instance of AllocRecordObjectMap.
  explicit AllocRecord(size_t count, AllocRecordStackTrace* trace)
      : byte_count_(count), trace_(trace) {}

  ~AllocRecord() {
    delete trace_;
  }

  size_t GetDepth() const {
    return trace_->GetDepth();
  }

  AllocRecordStackTrace* GetStackTrace() const {
    return trace_;
  }

  size_t ByteCount() const {
    return byte_count_;
  }

  pid_t GetTid() const {
    return trace_->GetTid();
  }

  AllocRecordStackTraceElement* StackElement(size_t index) {
    return trace_->StackElement(index);
  }

 private:
  const size_t byte_count_;
  // Manc: TODO: currently trace_ is like a std::unique_ptr,
  // but in future with deduplication it could be a std::shared_ptr
  AllocRecordStackTrace* const trace_;
};

class AllocRecordObjectMap {
 public:
  static void RecordAllocation(Thread* self, mirror::Object* obj, size_t byte_count)
      LOCKS_EXCLUDED(Locks::alloc_tracker_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static void SetAllocTrackingEnabled(bool enabled) LOCKS_EXCLUDED(Locks::alloc_tracker_lock_);

  ~AllocRecordObjectMap();

  void Put(mirror::Object* obj, AllocRecord* record)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::alloc_tracker_lock_) {
    entries_.emplace_back(obj, record);
  }

  size_t Size() const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_, Locks::alloc_tracker_lock_) {
    return entries_.size();
  }

  void SweepAllocationRecords(IsMarkedCallback* callback, void* arg)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::alloc_tracker_lock_);

  void RemoveOldest()
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::alloc_tracker_lock_) {
    DCHECK(!entries_.empty());
    delete entries_.front().second;
    entries_.pop_front();
  }

  // Manc: TODO: Is there a better way to hide the entries_'s type?
  std::list<std::pair<mirror::Object*, AllocRecord*>>::iterator Begin()
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::alloc_tracker_lock_) {
    return entries_.begin();
  }

  std::list<std::pair<mirror::Object*, AllocRecord*>>::iterator End()
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::alloc_tracker_lock_) {
    return entries_.end();
  }

  std::list<std::pair<mirror::Object*, AllocRecord*>>::reverse_iterator RBegin()
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::alloc_tracker_lock_) {
    return entries_.rbegin();
  }

  std::list<std::pair<mirror::Object*, AllocRecord*>>::reverse_iterator REnd()
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::alloc_tracker_lock_) {
    return entries_.rend();
  }

 private:
  static size_t alloc_record_max_ GUARDED_BY(Locks::alloc_tracker_lock_);
  static pid_t alloc_ddm_thread_id_ GUARDED_BY(Locks::alloc_tracker_lock_);
  std::list<std::pair<mirror::Object*, AllocRecord*>> entries_
      GUARDED_BY(Locks::alloc_tracker_lock_);

  static void SetMaxSize() EXCLUSIVE_LOCKS_REQUIRED(Locks::alloc_tracker_lock_);
};

}  // namespace gc
}  // namespace art
#endif  // ART_RUNTIME_GC_ALLOCATION_RECORD_H_
