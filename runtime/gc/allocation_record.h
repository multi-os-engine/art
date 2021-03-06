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
#include "gc_root.h"

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

  int32_t ComputeLineNumber() const SHARED_REQUIRES(Locks::mutator_lock_);

  ArtMethod* GetMethod() const {
    return method_;
  }

  void SetMethod(ArtMethod* m) {
    method_ = m;
  }

  uint32_t GetDexPc() const {
    return dex_pc_;
  }

  void SetDexPc(uint32_t pc) {
    dex_pc_ = pc;
  }

  bool operator==(const AllocRecordStackTraceElement& other) const {
    if (this == &other) return true;
    return method_ == other.method_ && dex_pc_ == other.dex_pc_;
  }

 private:
  ArtMethod* method_;
  uint32_t dex_pc_;
};

class AllocRecordStackTrace {
 public:
  static constexpr size_t kHashMultiplier = 17;

  explicit AllocRecordStackTrace(size_t max_depth)
      : tid_(0), depth_(0), stack_(new AllocRecordStackTraceElement[max_depth]) {}

  AllocRecordStackTrace(const AllocRecordStackTrace& r)
      : tid_(r.tid_), depth_(r.depth_), stack_(new AllocRecordStackTraceElement[r.depth_]) {
    for (size_t i = 0; i < depth_; ++i) {
      stack_[i] = r.stack_[i];
    }
  }

  ~AllocRecordStackTrace() {
    delete[] stack_;
  }

  pid_t GetTid() const {
    return tid_;
  }

  void SetTid(pid_t t) {
    tid_ = t;
  }

  size_t GetDepth() const {
    return depth_;
  }

  void SetDepth(size_t depth) {
    depth_ = depth;
  }

  const AllocRecordStackTraceElement& GetStackElement(size_t index) const {
    DCHECK_LT(index, depth_);
    return stack_[index];
  }

  void SetStackElementAt(size_t index, ArtMethod* m, uint32_t dex_pc) {
    stack_[index].SetMethod(m);
    stack_[index].SetDexPc(dex_pc);
  }

  bool operator==(const AllocRecordStackTrace& other) const {
    if (this == &other) return true;
    if (tid_ != other.tid_) return false;
    if (depth_ != other.depth_) return false;
    for (size_t i = 0; i < depth_; ++i) {
      if (!(stack_[i] == other.stack_[i])) return false;
    }
    return true;
  }

 private:
  pid_t tid_;
  size_t depth_;
  AllocRecordStackTraceElement* const stack_;
};

struct HashAllocRecordTypes {
  size_t operator()(const AllocRecordStackTraceElement& r) const {
    return std::hash<void*>()(reinterpret_cast<void*>(r.GetMethod())) *
        AllocRecordStackTrace::kHashMultiplier + std::hash<uint32_t>()(r.GetDexPc());
  }

  size_t operator()(const AllocRecordStackTrace& r) const {
    size_t depth = r.GetDepth();
    size_t result = r.GetTid() * AllocRecordStackTrace::kHashMultiplier + depth;
    for (size_t i = 0; i < depth; ++i) {
      result = result * AllocRecordStackTrace::kHashMultiplier + (*this)(r.GetStackElement(i));
    }
    return result;
  }
};

template <typename T> struct HashAllocRecordTypesPtr {
  size_t operator()(const T* r) const {
    if (r == nullptr) return 0;
    return HashAllocRecordTypes()(*r);
  }
};

template <typename T> struct EqAllocRecordTypesPtr {
  bool operator()(const T* r1, const T* r2) const {
    if (r1 == r2) return true;
    if (r1 == nullptr || r2 == nullptr) return false;
    return *r1 == *r2;
  }
};

class AllocRecord {
 public:
  // All instances of AllocRecord should be managed by an instance of AllocRecordObjectMap.
  AllocRecord(size_t count, mirror::Class* klass, AllocRecordStackTrace* trace)
      : byte_count_(count), klass_(klass), trace_(trace) {}

  ~AllocRecord() {
    delete trace_;
  }

  size_t GetDepth() const {
    return trace_->GetDepth();
  }

  const AllocRecordStackTrace* GetStackTrace() const {
    return trace_;
  }

  size_t ByteCount() const {
    return byte_count_;
  }

  pid_t GetTid() const {
    return trace_->GetTid();
  }

  mirror::Class* GetClass() const SHARED_REQUIRES(Locks::mutator_lock_) {
    return klass_.Read();
  }

  const char* GetClassDescriptor(std::string* storage) const
      SHARED_REQUIRES(Locks::mutator_lock_);

  GcRoot<mirror::Class>& GetClassGcRoot() SHARED_REQUIRES(Locks::mutator_lock_) {
    return klass_;
  }

  const AllocRecordStackTraceElement& StackElement(size_t index) const {
    return trace_->GetStackElement(index);
  }

 private:
  const size_t byte_count_;
  // The klass_ could be a strong or weak root for GC
  GcRoot<mirror::Class> klass_;
  // TODO: Currently trace_ is like a std::unique_ptr,
  // but in future with deduplication it could be a std::shared_ptr.
  const AllocRecordStackTrace* const trace_;
};

class AllocRecordObjectMap {
 public:
  // GcRoot<mirror::Object> pointers in the list are weak roots, and the last recent_record_max_
  // number of AllocRecord::klass_ pointers are strong roots (and the rest of klass_ pointers are
  // weak roots). The last recent_record_max_ number of pairs in the list are always kept for DDMS's
  // recent allocation tracking, but GcRoot<mirror::Object> pointers in these pairs can become null.
  // Both types of pointers need read barriers, do not directly access them.
  typedef std::list<std::pair<GcRoot<mirror::Object>, AllocRecord*>> EntryList;

  // "static" because it is part of double-checked locking. It needs to check a bool first,
  // in order to make sure the AllocRecordObjectMap object is not null.
  static void RecordAllocation(Thread* self, mirror::Object* obj, mirror::Class* klass,
                               size_t byte_count)
      REQUIRES(!Locks::alloc_tracker_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  static void SetAllocTrackingEnabled(bool enabled) REQUIRES(!Locks::alloc_tracker_lock_);

  AllocRecordObjectMap() REQUIRES(Locks::alloc_tracker_lock_)
      : alloc_record_max_(kDefaultNumAllocRecords),
        recent_record_max_(kDefaultNumRecentRecords),
        max_stack_depth_(kDefaultAllocStackDepth),
        scratch_trace_(kMaxSupportedStackDepth),
        alloc_ddm_thread_id_(0),
        allow_new_record_(true),
        new_record_condition_("New allocation record condition", *Locks::alloc_tracker_lock_) {}

  ~AllocRecordObjectMap();

  void Put(mirror::Object* obj, AllocRecord* record)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(Locks::alloc_tracker_lock_) {
    if (entries_.size() == alloc_record_max_) {
      delete entries_.front().second;
      entries_.pop_front();
    }
    entries_.emplace_back(GcRoot<mirror::Object>(obj), record);
  }

  size_t Size() const SHARED_REQUIRES(Locks::alloc_tracker_lock_) {
    return entries_.size();
  }

  size_t GetRecentAllocationSize() const SHARED_REQUIRES(Locks::alloc_tracker_lock_) {
    CHECK_LE(recent_record_max_, alloc_record_max_);
    size_t sz = entries_.size();
    return std::min(recent_record_max_, sz);
  }

  void VisitRoots(RootVisitor* visitor)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(Locks::alloc_tracker_lock_);

  void SweepAllocationRecords(IsMarkedVisitor* visitor)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(Locks::alloc_tracker_lock_);

  // Allocation tracking could be enabled by user in between DisallowNewAllocationRecords() and
  // AllowNewAllocationRecords(), in which case new allocation records can be added although they
  // should be disallowed. However, this is GC-safe because new objects are not processed in this GC
  // cycle. The only downside of not handling this case is that such new allocation records can be
  // swept from the list. But missing the first few records is acceptable for using the button to
  // enable allocation tracking.
  void DisallowNewAllocationRecords()
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(Locks::alloc_tracker_lock_);
  void AllowNewAllocationRecords()
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(Locks::alloc_tracker_lock_);
  void BroadcastForNewAllocationRecords()
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(Locks::alloc_tracker_lock_);

  // TODO: Is there a better way to hide the entries_'s type?
  EntryList::iterator Begin()
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(Locks::alloc_tracker_lock_) {
    return entries_.begin();
  }

  EntryList::iterator End()
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(Locks::alloc_tracker_lock_) {
    return entries_.end();
  }

  EntryList::reverse_iterator RBegin()
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(Locks::alloc_tracker_lock_) {
    return entries_.rbegin();
  }

  EntryList::reverse_iterator REnd()
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(Locks::alloc_tracker_lock_) {
    return entries_.rend();
  }

 private:
  static constexpr size_t kDefaultNumAllocRecords = 512 * 1024;
  static constexpr size_t kDefaultNumRecentRecords = 64 * 1024 - 1;
  static constexpr size_t kDefaultAllocStackDepth = 16;
  static constexpr size_t kMaxSupportedStackDepth = 128;
  size_t alloc_record_max_ GUARDED_BY(Locks::alloc_tracker_lock_);
  size_t recent_record_max_ GUARDED_BY(Locks::alloc_tracker_lock_);
  size_t max_stack_depth_ GUARDED_BY(Locks::alloc_tracker_lock_);
  AllocRecordStackTrace scratch_trace_ GUARDED_BY(Locks::alloc_tracker_lock_);
  pid_t alloc_ddm_thread_id_ GUARDED_BY(Locks::alloc_tracker_lock_);
  bool allow_new_record_ GUARDED_BY(Locks::alloc_tracker_lock_);
  ConditionVariable new_record_condition_ GUARDED_BY(Locks::alloc_tracker_lock_);
  // see the comment in typedef of EntryList
  EntryList entries_ GUARDED_BY(Locks::alloc_tracker_lock_);

  void SetProperties() REQUIRES(Locks::alloc_tracker_lock_);
};

}  // namespace gc
}  // namespace art
#endif  // ART_RUNTIME_GC_ALLOCATION_RECORD_H_
