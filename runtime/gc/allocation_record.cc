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

#include "allocation_record.h"

#include "art_method-inl.h"
#include "base/stl_util.h"
#include "stack.h"

#ifdef HAVE_ANDROID_OS
#include "cutils/properties.h"
#endif

namespace art {
namespace gc {

static const size_t kDefaultNumAllocRecords = 512*1024;

size_t AllocRecordObjectMap::alloc_record_max_ = 0;
pid_t AllocRecordObjectMap::alloc_ddm_thread_id_ = 0;

int32_t AllocRecordStackTraceElement::ComputeLineNumber() const {
  DCHECK(method_ != nullptr);
  return method_->GetLineNumFromDexPC(dex_pc_);
}

void AllocRecordObjectMap::SetMaxSize() {
  alloc_record_max_ = kDefaultNumAllocRecords;
#ifdef HAVE_ANDROID_OS
  // Check whether there's a system property overriding the number of records.
  const char* propertyName = "dalvik.vm.allocTrackerMax";
  char allocRecordMaxString[PROPERTY_VALUE_MAX];
  if (property_get(propertyName, allocRecordMaxString, "") > 0) {
    char* end;
    size_t value = strtoul(allocRecordMaxString, &end, 10);
    if (*end != '\0') {
      LOG(ERROR) << "Ignoring  " << propertyName << " '" << allocRecordMaxString
                 << "' --- invalid";
      return;
    }
    alloc_record_max_ = value;
  }
#endif
}

AllocRecordObjectMap::~AllocRecordObjectMap() {
  STLDeleteValues(&entries_);
}

void AllocRecordObjectMap::SweepAllocationRecords(IsMarkedCallback* callback, void* arg) {
  VLOG(heap) << "Start SweepAllocationRecords()";
  size_t count_deleted = 0, count_moved = 0;
  for (auto it = entries_.begin(), end = entries_.end(); it != end;) {
    mirror::Object* old_object = it->first;
    AllocRecord* record = it->second;
    mirror::Object* new_object = callback(old_object, arg);
    if (new_object == nullptr) {
      delete record;
      it = entries_.erase(it);
      ++count_deleted;
    } else {
      if (old_object != new_object) {
        it->first = new_object;
        ++count_moved;
      }
      ++it;
    }
  }
  VLOG(heap) << "Deleted " << count_deleted << " allocation records";
  VLOG(heap) << "Updated " << count_moved << " allocation records";
}

struct AllocRecordStackVisitor : public StackVisitor {
  AllocRecordStackVisitor(Thread* thread, AllocRecordStackTrace* trace_in)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : StackVisitor(thread, nullptr, StackVisitor::StackWalkKind::kIncludeInlinedFrames),
        trace(trace_in),
        depth(0) {}

  // TODO: Enable annotalysis. We know lock is held in constructor, but abstraction confuses
  // annotalysis.
  bool VisitFrame() OVERRIDE NO_THREAD_SAFETY_ANALYSIS {
    if (depth >= AllocRecordStackTrace::kMaxAllocRecordStackDepth) {
      return false;
    }
    ArtMethod* m = GetMethod();
    if (!m->IsRuntimeMethod()) {
      trace->SetStackElementAt(depth, m, GetDexPc());
      ++depth;
    }
    return true;
  }

  ~AllocRecordStackVisitor() {
    trace->SetDepth(depth);
  }

  AllocRecordStackTrace* trace;
  size_t depth;
};

void AllocRecordObjectMap::SetAllocTrackingEnabled(bool enable) {
  Thread* self = Thread::Current();
  Heap* heap = Runtime::Current()->GetHeap();
  if (enable) {
    {
      MutexLock mu(self, *Locks::alloc_tracker_lock_);
      if (heap->IsAllocTrackingEnabled()) {
        return;  // Already enabled, bail.
      }
      SetMaxSize();
      // TODO: size of the following message is inaccurate, it doesn't account for stack traces
      LOG(INFO) << "Enabling alloc tracker (" << alloc_record_max_ << " entries of "
                << AllocRecordStackTrace::kMaxAllocRecordStackDepth << " frames, taking up to "
                << PrettySize(sizeof(AllocRecord) * alloc_record_max_) << ")";
      DCHECK_EQ(alloc_ddm_thread_id_, 0);
      std::string self_name;
      self->GetThreadName(self_name);
      if (self_name == "JDWP") {
        alloc_ddm_thread_id_ = self->GetTid();
      }
      AllocRecordObjectMap* records = new AllocRecordObjectMap();
      CHECK(records != nullptr);
      heap->SetAllocationRecords(records);
      heap->SetAllocTrackingEnabled(true);
    }
    Runtime::Current()->GetInstrumentation()->InstrumentQuickAllocEntryPoints();
  } else {
    {
      ScopedObjectAccess soa(self);  // For type_cache_.Clear();
      MutexLock mu(self, *Locks::alloc_tracker_lock_);
      if (!heap->IsAllocTrackingEnabled()) {
        return;  // Already disabled, bail.
      }
      heap->SetAllocTrackingEnabled(false);
      LOG(INFO) << "Disabling alloc tracker";
      heap->SetAllocationRecords(nullptr);
      alloc_ddm_thread_id_ = 0;
    }
    // If an allocation comes in before we uninstrument, we will safely drop it on the floor.
    Runtime::Current()->GetInstrumentation()->UninstrumentQuickAllocEntryPoints();
  }
}

void AllocRecordObjectMap::RecordAllocation(Thread* self, mirror::Object* obj, size_t byte_count) {
  MutexLock mu(self, *Locks::alloc_tracker_lock_);
  Heap* heap = Runtime::Current()->GetHeap();
  if (!heap->IsAllocTrackingEnabled()) {
    // In the process of shutting down recording, bail.
    return;
  }

  // Do not record for DDM thread
  if (alloc_ddm_thread_id_ == self->GetTid()) {
    return;
  }

  AllocRecordObjectMap* records = heap->GetAllocationRecords();
  DCHECK(records != nullptr);

  DCHECK_LE(records->Size(), alloc_record_max_);

  // Remove oldest record.
  if (records->Size() == alloc_record_max_) {
    records->RemoveOldest();
  }

  // Get stack trace.
  AllocRecordStackTrace* trace = new AllocRecordStackTrace(self->GetTid());
  AllocRecordStackVisitor visitor(self, trace);
  visitor.WalkStack();

  // Fill in the basics.
  AllocRecord* record = new AllocRecord(byte_count, trace);

  records->Put(obj, record);
  DCHECK_LE(records->Size(), alloc_record_max_);
}

}  // namespace gc
}  // namespace art
