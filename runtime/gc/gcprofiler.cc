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

#include <limits>
#include <vector>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include "base/histogram-inl.h"
#include "base/stl_util.h"
#include "common_throws.h"
#include "cutils/sched_policy.h"
#include "debugger.h"
#include "gc/accounting/atomic_stack.h"
#include "gc/accounting/card_table-inl.h"
#include "gc/accounting/heap_bitmap-inl.h"
#include "gc/accounting/mod_union_table.h"
#include "gc/accounting/mod_union_table-inl.h"
#include "gc/accounting/remembered_set.h"
#include "gc/accounting/space_bitmap-inl.h"
#include "gc/collector/concurrent_copying.h"
#include "gc/collector/mark_sweep-inl.h"
#include "gc/collector/partial_mark_sweep.h"
#include "gc/collector/semi_space.h"
#include "gc/collector/sticky_mark_sweep.h"
#include "gc/reference_processor.h"
#include "gc/space/bump_pointer_space.h"
#include "gc/space/dlmalloc_space-inl.h"
#include "gc/space/image_space.h"
#include "gc/space/large_object_space.h"
#include "gc/space/rosalloc_space-inl.h"
#include "gc/space/space-inl.h"
#include "gc/space/zygote_space.h"
#include "entrypoints/quick/quick_alloc_entrypoints.h"
#include "heap-inl.h"
#include "image.h"
#include "mirror/art_field-inl.h"
#include "mirror/class-inl.h"
#include "mirror/object.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/reference-inl.h"
#include "object_utils.h"
#include "os.h"
#include "reflection.h"
#include "runtime.h"
#include "ScopedLocalRef.h"
#include "scoped_thread_state_change.h"
#include "thread_list.h"
#include "UniquePtr.h"
#include "well_known_classes.h"
#include "gc/gcprofiler.h"

namespace art {

namespace gc {

const int recordType_GC = 0;
const int recordType_Succ = 1;
const int recordType_Fail = 2;
const int recordType_Large = 3;
const int recordType_Alloc = 4;

GcProfiler GcProfiler::s_instance;

GcProfiler::GcProfiler()
          : gc_id(0),
            gcProfInfoFd(-1),
            profile_duration(0),
            alloc_info(nullptr),
            gc_record_head(nullptr),
            curr_gc_record(nullptr),
            curr_fail_record(nullptr),
            succ_alloc_record_head(nullptr),
            curr_succ_alloc_record(nullptr),
            fail_alloc_record_head(nullptr),
            curr_fail_alloc_record(nullptr),
            large_alloc_records(nullptr),
            gc_record_idx(0),
            succ_alloc_idx(0),
            fail_alloc_idx(0),
            large_alloc_idx(0),
            curr_allocated(0),
            curr_footprint(0),
            free_size(0),
            gcProfRunning(false),
            pause_max(0),
            mark_max(0),
            sweep_max(0),
            data_dir("data/local/tmp/gcprofile/") { }

/**
 *  @brief start GC Profiler, init related variables
 */
void GcProfiler::Start() {
  LOG(INFO) << "GCProfile: Start";
  // if profile already start, return
  if (gcProfRunning) {
    return;
  }
  char file_name[100];
  int tail=0;
  int err = 0;
  struct stat buf;
  for (tail = 0; ;tail++ ) {
    snprintf(file_name, sizeof(file_name), "%s/alloc_free_log_%d.%d", data_dir.c_str(), getpid(), tail);
    err = stat(file_name, &buf);
    if (err != 0) {
      LOG(INFO) << file_name <<" will be used";
      break;
    }
  }
  // open profile log file
  gcProfInfoFd = open(file_name, O_CREAT|O_APPEND|O_WRONLY, 0644);
  if (gcProfInfoFd == -1) {
    LOG(ERROR) << "GCProfileStart: cannot open " << file_name << strerror(errno);
    return;
  }
  // alloc gc_record_head
  gc_record_head = new GCRecordList();
  if (gc_record_head == nullptr) {
    LOG(ERROR)<<"Cannot initialize GC message list for GC profiling";
    return;
  }
  gc_record_head->next = nullptr;
  curr_gc_record = gc_record_head;
  gc_record_idx = 0;
  // alloc succ_alloc_record_head
  succ_alloc_record_head = new SuccAllocRecordList();
  if (succ_alloc_record_head == nullptr) {
    LOG(ERROR)<<"Cannot initialize allocation messages for succeeded allocations for GC profiling";
    return;
  }
  succ_alloc_record_head->next = nullptr;
  curr_succ_alloc_record = succ_alloc_record_head;
  succ_alloc_idx = 0;
  // alloc fail_alloc_record_head
  fail_alloc_record_head = new FailAllocRecordList();
  if (fail_alloc_record_head == nullptr) {
    LOG(ERROR)<<"Cannot initialize allocation messages for failed allocations for GC profiling";
    return;
  }
  fail_alloc_record_head->next = nullptr;
  curr_fail_alloc_record = fail_alloc_record_head;
  fail_alloc_idx = 0;
  // alloc LargeAllocRecord
  large_alloc_records = new LargeAllocRecord[MAX_ALLOCRECORD_SIZE];
  if (large_alloc_records == nullptr) {
    LOG(ERROR)<<"Cannot initialize allocation messages for large objects for GC profiling";
    return;
  }
  // alloc alloc_info
  alloc_info = new AllocInfo();
  if (alloc_info == nullptr) {
    LOG(ERROR) << "Cannot initialize allocation info for GC profiling";
    return;
  }
  memset(alloc_info, 0, sizeof(AllocInfo));
  large_alloc_idx = 0;
  profile_duration = NsToMs(NanoTime());
  gcProfRunning = true;
}

/**
 * @brief stop GC profiling
 * @param dropResult if true the profiling data are't dump to file
 */
void GcProfiler::Stop(bool dropResult) {
  // return if gc profiling not running
  if (!gcProfRunning)  return;
  // print log if not drop result
  if (!dropResult) {
    LOG(INFO)<<"GCProfileEnd";
  }
  // don't need the result
  if (dropResult) {
    releaseRecords();
    close(gcProfInfoFd);
    clearAllRecords();
    gcProfRunning = false;
    return;
  }
  // profile duration is used for calculating allocation thruput
  profile_duration = NsToMs(NanoTime()) - profile_duration;
  dumpRecords();
  releaseRecords();
  close(gcProfInfoFd);
  clearAllRecords();
  LOG(INFO)<<"GCProfile: Finish!";
  gcProfRunning = false;
}

/**
 * @brief dump allocation info to file
 */
void GcProfiler::dumpAllocationInfo() {
  // AllocInfo *alloc_info = alloc_info;
  uint64_t duration = profile_duration;
  uint32_t count = 1;

  if (profile_duration != 0) {
    alloc_info->throughput_bpns = (double)alloc_info->number_bytes_alloc_.LoadSequentiallyConsistent() / (double)profile_duration;
    alloc_info->throughput_npns = (double)alloc_info->number_objects_alloc_.LoadSequentiallyConsistent() / (double)profile_duration;
  } else {
    // if profile_duration is too small, thruputs are set to 0.0
    alloc_info->throughput_bpns = 0.0;
    alloc_info->throughput_npns = 0.0;
  }
  alloc_info->duration = duration;
  write(gcProfInfoFd, &count, sizeof(uint32_t));
  write(gcProfInfoFd, &recordType_Alloc, sizeof(uint32_t));
  write(gcProfInfoFd, alloc_info, sizeof(AllocInfo));
}

/**
 * @brief dump large object info to file
 */
void GcProfiler::dumpLargeAllocRecords() {
  uint32_t count = large_alloc_idx;
  write(gcProfInfoFd, &count, sizeof(uint32_t));
  write(gcProfInfoFd, &recordType_Large, sizeof(uint32_t));
  write(gcProfInfoFd, large_alloc_records, count * sizeof(LargeAllocRecord));
}

/**
 * @brief dump GC Records
 */
void GcProfiler::dumpRecords() {
  GCRecordList *gcList = gc_record_head;
  uint32_t count = MAX_GCRECORD_SIZE;
  // dump GC Record List first
  while (gcList->next != nullptr) {
    write(gcProfInfoFd, &count, sizeof(uint32_t));
    write(gcProfInfoFd, &recordType_GC, sizeof(uint32_t));
    write(gcProfInfoFd, gcList->records, count * sizeof(GCRecord));
    gcList = gcList->next;
  }
  if (gcList != nullptr) {
    count = gc_record_idx;
    write(gcProfInfoFd, &count, sizeof(uint32_t));
    write(gcProfInfoFd, &recordType_GC, sizeof(uint32_t));
    write(gcProfInfoFd, gcList->records, count * sizeof(GCRecord));
  }

  // dump successfuly allocation records
  SuccAllocRecordList *succAllocList = succ_alloc_record_head;
  count = MAX_GCRECORD_SIZE;
  while (succAllocList->next != nullptr) {
    write(gcProfInfoFd, &count, sizeof(uint32_t));
    write(gcProfInfoFd, &recordType_Succ, sizeof(uint32_t));
    write(gcProfInfoFd, succAllocList->records, count * sizeof(SuccAllocRecord));
    succAllocList = succAllocList->next;
  }
  if (succAllocList != nullptr) {
    count = succ_alloc_idx;
    write(gcProfInfoFd, &count, sizeof(uint32_t));
    write(gcProfInfoFd, &recordType_Succ, sizeof(uint32_t));
    write(gcProfInfoFd, succAllocList->records, count * sizeof(SuccAllocRecord));
  }

  // dump fail allocation records
  FailAllocRecordList *failAllocList = fail_alloc_record_head;
  count = MAX_GCRECORD_SIZE;
  while (failAllocList->next != nullptr) {
    write(gcProfInfoFd, &count, sizeof(uint32_t));
    write(gcProfInfoFd, &recordType_Fail, sizeof(uint32_t));
    write(gcProfInfoFd, failAllocList->records, count * sizeof(FailAllocRecord));
    failAllocList = failAllocList->next;
  }
  if (failAllocList != nullptr) {
    count = fail_alloc_idx;
    write(gcProfInfoFd, &count, sizeof(uint32_t));
    write(gcProfInfoFd, &recordType_Fail, sizeof(uint32_t));
    write(gcProfInfoFd, failAllocList->records, count * sizeof(FailAllocRecord));
  }

  // dump large object allocation record
  dumpLargeAllocRecords();
  // dump allocation info
  dumpAllocationInfo();
}

/**
 * @brief release all records memory
 */
void GcProfiler::releaseRecords() {
  // release the large allocation records
  delete []large_alloc_records;

  // release GC records
  GCRecordList *gcList = gc_record_head, *t1;
  while (gcList != nullptr) {
    t1 = gcList;
    gcList = gcList->next;
    delete t1;
  }

  // release successful allocation records
  SuccAllocRecordList *succList = succ_alloc_record_head, *t2;
  while (succList != nullptr) {
    t2 = succList;
    succList = succList->next;
    delete t2;
  }

  // release fail allocation records
  FailAllocRecordList *failList = fail_alloc_record_head, *t3;
  while (failList != nullptr) {
    t3 = failList;
    failList = failList->next;
    delete t3;
  }

  // release allocation info
  delete alloc_info;
}

/**
 * @brief get the current using GC records
 * @return GCRecord* ptr to current using records
 */
GcProfiler::GCRecord* GcProfiler::getCurrentGCRecord() {
  if (!gcProfRunning || gc_record_idx == 0) return nullptr;
  return &(curr_gc_record->records[gc_record_idx - 1]);
}

/**
 * @brief get one record from list, new one if full
 * @return the record ptr or nullptr
 */
GcProfiler::SuccAllocRecord* GcProfiler::getNextSuccAllocRecord() {
  if (!gcProfRunning) return nullptr;
  if (succ_alloc_idx == MAX_GCRECORD_SIZE) {
    //new one if list is full
    SuccAllocRecordList *t = new SuccAllocRecordList();
    if (t == nullptr) {
      LOG(ERROR)<<"Cannot new succeeded object allocation record list";
      return nullptr;
    }
    t->next = nullptr;
    curr_succ_alloc_record->next = t;
    curr_succ_alloc_record = t;
    succ_alloc_idx = 0;
  }
  SuccAllocRecord *succAlloc = &(curr_succ_alloc_record->records[succ_alloc_idx++]);
  memset(succAlloc, 0, sizeof(SuccAllocRecord));
  succAlloc->gc_id = gc_id;
  return succAlloc;
}

/**
 * @brief get one fail record from list, new one if full
 * @return the record ptr or nullptr
 */
GcProfiler::FailAllocRecord* GcProfiler::getNextFailAllocRecord() {
  if (!gcProfRunning) return nullptr;
  if (fail_alloc_idx == MAX_GCRECORD_SIZE) {
    //new one and add to list
    FailAllocRecordList *t = new FailAllocRecordList();
    if (t == nullptr) {
      LOG(ERROR) << "Cannot new failed object allocation record list";
      return nullptr;
    }
    t->next = nullptr;
    curr_fail_alloc_record->next = t;
    curr_fail_alloc_record = t;
    fail_alloc_idx = 0;
  }
  FailAllocRecord *failAlloc = &(curr_fail_alloc_record->records[fail_alloc_idx++]);
  failAlloc->gc_id = gc_id;
  return failAlloc;
}

/**
 * @brief get one large object allocation record from array.
 *        if array is full, dump it and reuse it.
 * @return the record ptr or nullptr
 */
GcProfiler::LargeAllocRecord* GcProfiler::getNextLargeAllocRecord() {
  if (!gcProfRunning) return nullptr;
  if (large_alloc_idx == MAX_ALLOCRECORD_SIZE) {
    dumpLargeAllocRecords();
    large_alloc_idx = 0;
  }
  LargeAllocRecord *alloc = &(large_alloc_records[large_alloc_idx++]);
  alloc->gc_id = gc_id;
  return alloc;
}

/**
 * @brief insert the size to distribution in allocation info record
 * @param allocinfo: allocation info record
 *        size: size to insert
 */
void GcProfiler::insert_size_dist(SuccAllocRecord* allocinfo, uint32_t size) {
  if (size >= Heap::kDefaultLargeObjectThreshold) {
    allocinfo->size_dist[11]++;
  } else {
    uint32_t t = (size - 1) >> 4;
    uint32_t i = 0;
    while (t > 0) {
      i++;
      t = t >> 1;
    }
    allocinfo->size_dist[i]++;
  }
}

/**
 * @brief get one GC record from list, new one if full
 * @return the record ptr or nullptr
 */
GcProfiler::GCRecord* GcProfiler::getNextGCRecord() {
  if (!gcProfRunning) return nullptr;
  if (gc_record_idx == MAX_GCRECORD_SIZE) {
    GCRecordList *t = new GCRecordList();
    if (t == nullptr) {
      LOG(ERROR)<<"Cannot new GC Record list";
      return nullptr;
    }
    t->next = nullptr;
    curr_gc_record->next = t;
    curr_gc_record = t;
    gc_record_idx = 0;
  }
  return &(curr_gc_record->records[gc_record_idx++]);
}

/**
 * @brief update max wait time and blocking time in GC record
 * @param wait_time: the time other threads waited for this GC complete
 *        update_wait_time: true if wait time need to update
 *        update_block_time: true if block time need to update
 */
void GcProfiler::updateMaxWaitForGcTimeAndBlockingTime(uint64_t wait_time,
                                                       bool update_wait_time,
                                                       bool update_block_time) {
  if (gcProfRunning) {
    GCRecord* gcRecord = getCurrentGCRecord();
    if (gcRecord != nullptr) {
      if (update_wait_time) {
        gcRecord->max_wait_time = wait_time > gcRecord->max_wait_time ? wait_time : gcRecord->max_wait_time;
      }
      if (update_block_time) {
        gcRecord->blocking_time = wait_time > gcRecord->blocking_time ? wait_time : gcRecord->blocking_time;
      }
   }
  }
}

/**
 * @brief sum up the time wasted in WaitForGcComplete duration two GCs
 * @param wait_time: the wait time in WaitForGcComplete but actually no GC happening
 */
void GcProfiler::updateWastedWaitTime(uint64_t wait_time) {
  if (gcProfRunning) {
    GCRecord* gcRecord = getCurrentGCRecord();
    if (gcRecord != nullptr) {
      gcRecord->wasted_wait_time_after_gc += wait_time;
    }
  }
}
/**
 * @brief insert new GC record into record list
 * @param gc_cause: the cause of GC
 *        gc_type: GC type
 *        gc_start_time_ns: the timestamp of GC
 *        bytes_allocated: allocated size before GC
 */
void GcProfiler::insertNewGcRecord(const GcCause gc_cause, const collector::GcType gc_type,
                                   uint64_t gc_start_time_ns, uint32_t bytes_allocated) {
  if (gcProfRunning) {
    GCRecord* gcRecord = getNextGCRecord();
    if (gcRecord != nullptr) {
      gcRecord->id = ++gc_id;
      gcRecord->reason = (uint32_t)gc_cause;
      gcRecord->type = (uint32_t)gc_type;
      gcRecord->timestamp = NsToMs(gc_start_time_ns);
      gcRecord->allocatedSizeBeforeGc = bytes_allocated;
    }
    getNextSuccAllocRecord();
  }
}

/**
 * @brief fill record info for the GC
 * @param collector: collector used by GC
 *        max_allowed_footprint: max footprint
 *        concurrent_start_bytes: total bytes allocated before concurrent GC
 *        alloc_stack_size: size of allocation stack
 *        total_memory: total memory
 *        bytes_allocated: total bytes allocated after GC
 */
void GcProfiler::fillGcRecordInfo(collector::GarbageCollector* collector,
                                  uint32_t max_allowed_footprint,
                                  uint32_t concurrent_start_bytes_,
                                  uint32_t alloc_stack_size,
                                  uint32_t total_memory,
                                  uint32_t bytes_allocated) {
  GCRecord* gcRecord = getCurrentGCRecord();
  if (gcProfRunning && gcRecord != nullptr) {
    gcRecord->freeBytes = collector->GetFreedBytes();
    gcRecord->freeObjectCount = collector->GetFreedObjects();
    gcRecord->freeLargeObjectCount = collector->GetFreedLargeObjects();
    gcRecord->freeLargeObjectBytes = collector->GetFreedLargeObjectBytes();
    gcRecord->gcTime = collector->GetDurationNs();
    gcRecord->pauseTimeMax = pause_max;
    gcRecord->markTimeMax = mark_max;
    gcRecord->sweepTimeMax = sweep_max;
    gcRecord->footprintSize = total_memory;
    gcRecord->allocatedSize = bytes_allocated;
    gcRecord->maxAllowedFootprint = max_allowed_footprint;
    gcRecord->concurrentStartBytes = concurrent_start_bytes_;
    gcRecord->totalObjectCountInAllocStackDuringGc = alloc_stack_size;

    if (gcRecord->gcTime != 0) {
      gcRecord->gc_throughput_bpns = (double)(gcRecord->freeBytes + gcRecord->freeLargeObjectBytes) / (double)gcRecord->gcTime;
      gcRecord->gc_throughput_npns = (double)(gcRecord->freeObjectCount + gcRecord->freeLargeObjectCount) / (double)gcRecord->gcTime;
    } else {
      gcRecord->gc_throughput_bpns = 0.0;
      gcRecord->gc_throughput_npns = 0.0;
    }
  }
}

/**
 * @brief create fail allocation result
 * @param klass: class ptr of the object
 *        bytes_allocated: total bytes allocated before creating the record
 *        max_allowed_footprint_: max footprint
 *        alloc_size: size needs to be allocated
 *        gc_type: last GC type
 */
void GcProfiler::createFailRecord(mirror::Class* klass,
                                  uint32_t bytes_allocated,
                                  uint32_t max_allowed_footprint_,
                                  uint32_t alloc_size, collector::GcType gc_type,
                                  AllocFailPhase fail_phase) {
  if (gcProfRunning) {
    curr_allocated = bytes_allocated;
    curr_footprint = max_allowed_footprint_;
    free_size = curr_footprint - curr_allocated;
    curr_fail_record = getNextFailAllocRecord();
    if (curr_fail_record != nullptr) {
      curr_fail_record->size = alloc_size;
      curr_fail_record->phase = kFailNull;
      curr_fail_record->last_gc_type = gc_type;
      int length = Runtime::Current()->GetHeap()->SafeGetClassDescriptor(klass).size() + 1;
      if (length > 0) {
         // if klass descriptor is too long, truncate it
         if (length > OBJECT_TYPE_LENGTH) {
           strncpy(curr_fail_record->type, Runtime::Current()->GetHeap()->SafeGetClassDescriptor(klass).c_str(), length - OBJECT_TYPE_LENGTH);
         } else {
           strncpy(curr_fail_record->type, Runtime::Current()->GetHeap()->SafeGetClassDescriptor(klass).c_str(), length);
         }
      }
    }
    setFailRecordPhase(fail_phase, alloc_size);
  }
}

/**
 * @brief set the phase until which allocation failed
 * @param gc_type: last GC type
 *        fail_phase: phase until which allocation failed
 *        alloc_size: size need to be allocated
 */
void GcProfiler::setFailRecordPhase(AllocFailPhase fail_phase, uint32_t alloc_size) {
  if (gcProfRunning && (curr_fail_record != nullptr)) {
    if (fail_phase >= kFailUntilGCForAlloc && fail_phase <= kFailUntilGCForAllocClearRef) {
      // if there is segmentation, set corresponding phase
      if (free_size > alloc_size) {
        curr_fail_record->phase = (AllocFailPhase)(fail_phase + 4);
      } else {
        curr_fail_record->phase = fail_phase;
      }
    } else {
      curr_fail_record->phase = fail_phase;
    }
  }
}

/**
 * @brief update profiler's heap info record, used after heap grown
 * @param bytes_allocated: total bytes allocated
 *        max_allowed_footprint_: max footprint
 */
void GcProfiler::updateHeapUsageInfo(uint32_t bytes_allocated,
                                     uint32_t max_allowed_footprint_) {
  if (gcProfRunning) {
    curr_allocated = bytes_allocated;
    curr_footprint = max_allowed_footprint_;
    free_size = curr_footprint - curr_allocated;
  }
}

/**
 * @brief create record for successful allocation
 * @param obj: the objects just allocated
 *        byte_count: the bytes just allocated
 *        klass: the class ptr of the object
 */
void GcProfiler::createSuccAllocRecord(mirror::Object* obj, uint32_t byte_count,
                                       mirror::Class* klass) {
    if (gcProfRunning) {
      SuccAllocRecord *allocRecord;
      if (succ_alloc_idx == 0) {
        // if there is no record, create one
        allocRecord = getNextSuccAllocRecord();
      } else {
        // get the current using record
        allocRecord = &(curr_succ_alloc_record->records[succ_alloc_idx-1]);
      }

      if (allocRecord) {
        allocRecord->total_size += byte_count;
        insert_size_dist(allocRecord, byte_count);
      }

      // if it is large object, add to large object record
      if (byte_count >= Heap::kDefaultLargeObjectThreshold)  {
        LargeAllocRecord *largeRecord = getNextLargeAllocRecord();
        if (largeRecord) {
          largeRecord->size = byte_count;
          int length = Runtime::Current()->GetHeap()->SafeGetClassDescriptor(klass).size()+1;
          if (length > 0) {
            // if class descriptor is large, truncate it
            if (length > OBJECT_TYPE_LENGTH) {
              strncpy(largeRecord->type, Runtime::Current()->GetHeap()->SafeGetClassDescriptor(klass).c_str(), OBJECT_TYPE_LENGTH );
            } else {
              strcpy(largeRecord->type, Runtime::Current()->GetHeap()->SafeGetClassDescriptor(klass).c_str());
            }
          }
        }
      }
    }
}

/**
 * @brief set max pause time, mark time and sweep time
 * @prarm pause: pause time
 *        mark: mark time
 *        sweep: sweep time
 */
void GcProfiler::setGCTimes(uint64_t pause, uint64_t mark, uint64_t sweep) {
  pause_max = pause;
  mark_max = mark;
  sweep_max = sweep;
}

/**
 * @brief sum up the number of bytes and counts of allocted objects
 * @param bytes: the bytes needs to sum, usually the object size allocated
 */
void GcProfiler::addAllocInfo(uint32_t bytes) {
  if (gcProfRunning) {
    alloc_info->number_bytes_alloc_.FetchAndAddSequentiallyConsistent(bytes);
    alloc_info->number_objects_alloc_++;
  }
}

/**
 * @brief clear all related field of GcProfiler
 *        used in stop() in case of dirty status for next start()
 */
void GcProfiler::clearAllRecords() {
  gc_id = 0;
  gcProfInfoFd = -1;
  profile_duration = 0;
  alloc_info = nullptr;
  gc_record_head = nullptr;
  curr_gc_record = nullptr;
  curr_fail_record = nullptr;
  succ_alloc_record_head = nullptr;
  curr_succ_alloc_record = nullptr;
  fail_alloc_record_head = nullptr;
  curr_fail_alloc_record = nullptr;
  large_alloc_records = nullptr;
  gc_record_idx = 0;
  succ_alloc_idx = 0;
  fail_alloc_idx = 0;
  large_alloc_idx = 0;
  curr_allocated = 0;
  curr_footprint = 0;
  free_size = 0;
  pause_max = 0;
  mark_max = 0;
  sweep_max = 0;
}

}  // namespace gc
}  // namespace art
