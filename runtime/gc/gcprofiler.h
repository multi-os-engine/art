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

#ifndef ART_RUNTIME_GC_PROFILER_H_
#define ART_RUNTIME_GC_PROFILER_H_

#include "gc/heap.h"

namespace art {
#define MAX_ALLOCRECORD_SIZE 0x4000
#define MAX_GCRECORD_SIZE 0x1000
#define LARGE_OBJECT_SIZE 0x3000
#define SIZE_DISTRIBUTE_COUNT 12
#define OBJECT_TYPE_LENGTH 40

namespace gc {

/* allocation Fail util the following phases
 * e.g. kFailUntilGCConcurrent means allocation succeed after Concurrent GC
 */
enum AllocFailPhase {
  kFailNull,  // not fail
  kFailUntilGCConcurrent,  // background GC
  kFailUntilGCForAlloc,  // GC for alloc
  kFailUntilGCForAllocClearRef, // GC clear references

  kFailUntilAllocGrowHeap,  // heap grown
  kFailUntilGCForAllocWithFragment, // with Fragmentation
  kFailUntilGCConcurrentWithFragment,
  kFailUntilGCForAllocClearRefWithFragment,

  kFailThrowGCOOM, // thrown OOM
};

std::ostream& operator<<(std::ostream& os, const AllocFailPhase& alloc_fail_phase);

class GcProfiler {
  public:
    // record of every GC
    typedef struct GCProfile_GCRecord {
      uint32_t id;  // GC id
      uint32_t reason;  // GcCause
      uint64_t pauseTimeMax;  // max pause time
      uint64_t markTimeMax;  // max mark time
      uint64_t sweepTimeMax;  // max sweep time
      uint64_t gcTime;  // GC duration
      uint32_t freeBytes;
      uint32_t freeObjectCount;
      uint32_t freeLargeObjectCount;
      uint32_t freeLargeObjectBytes;
      uint32_t allocatedSize;  // total bytes allocated
      uint32_t footprintSize;  // foot print
      uint64_t max_wait_time;  // max time wait for this GC
      uint64_t timestamp;
      uint32_t type;  // type of GC
      uint32_t maxAllowedFootprint;
      uint32_t concurrentStartBytes;
      uint32_t allocatedSizeBeforeGc;
      uint32_t totalObjectCountInAllocStackDuringGc;
      double gc_throughput_bpns;
      double gc_throughput_npns;
      uint64_t blocking_time;  // max blocking time caused by this GC
      uint64_t wasted_wait_time_after_gc;  // time exhausted in WaitForGcComplete after this GC and before next GC
    } GCRecord;

    // list of GCRecord
    typedef struct GCProfile_GCRecordList {
      GCRecord records[MAX_GCRECORD_SIZE];
      struct GCProfile_GCRecordList *next;
    } GCRecordList;

    // size distribution of successfully allocated objects
    typedef struct GCProfile_Succ_Alloc {
      uint32_t gc_id;
      uint32_t total_size;
      uint32_t size_dist[SIZE_DISTRIBUTE_COUNT];
    } SuccAllocRecord;

    // list of SuccAllocRecord
    typedef struct GCProfile_SuccAllocList {
      SuccAllocRecord records[MAX_GCRECORD_SIZE];
      struct GCProfile_SuccAllocList *next;
    } SuccAllocRecordList;

    // fail allocation record
    typedef struct GCProfile_Fail_Alloc {
      uint32_t gc_id;
      uint32_t size;
      AllocFailPhase phase;
      collector::GcType last_gc_type;
      char type[OBJECT_TYPE_LENGTH];
    } FailAllocRecord;

    // list of FailAllocRecord
    typedef struct GCProfile_FailAllocList {
      FailAllocRecord records[MAX_GCRECORD_SIZE];
      struct GCProfile_FailAllocList *next;
    } FailAllocRecordList;

    // large object record
    typedef struct GCProfile_Large_Alloc {
      uint32_t gc_id;
      uint32_t size;
      char type[OBJECT_TYPE_LENGTH];
    } LargeAllocRecord;

    // allocation info
    typedef struct GCProfile_Allocation_Info {
      Atomic<uint32_t> number_bytes_alloc_;
      Atomic<uint32_t> number_objects_alloc_;
      double throughput_npns;
      double throughput_bpns;
      uint64_t duration;
    } AllocInfo;

    // start the profiling
    void Start();
    // stop the profiling
    void Stop(bool dropResult);
    // update the max waiting time and blocking time
    void updateMaxWaitForGcTimeAndBlockingTime(uint64_t wait_time, bool update_wait_time=true,
                                               bool update_block_time=true);
    // sumup the time exhausted in unnecessary WaitForGcComplete
    void updateWastedWaitTime(uint64_t wait_time);
    // create new GCRecord and add to list
    void insertNewGcRecord(const GcCause gc_cause, const collector::GcType gc_type,
                           uint64_t gc_start_time_ns, uint32_t bytes_allocated);
    // fill GCRecord fields
    void fillGcRecordInfo(collector::GarbageCollector* collector,
                          uint32_t max_allowed_footprint,
                          uint32_t concurrent_start_bytes_,
                          uint32_t alloc_stack_size,
                          uint32_t total_memory,
                          uint32_t bytes_allocated);
    // create a new FailRecord
    void createFailRecord(mirror::Class* klass,
                          uint32_t bytes_allocated,
                          uint32_t max_allowed_footprint,
                          uint32_t alloc_size,
                          collector::GcType gc_type,
                          AllocFailPhase fail_phase);
    // update profiler's heap information
    void updateHeapUsageInfo(uint32_t bytes_allocated, uint32_t max_allowed_footprint_);

    // only one GcProfiler instance exist
    static  GcProfiler* getInstance() {
      return &s_instance;
    }
    // add allocation info
    void addAllocInfo(uint32_t bytes_allocated);
    // create record for successful allocation
    void createSuccAllocRecord(mirror::Object* obj, uint32_t byte_count, mirror::Class* klass);
    // update GcProfiler's GC times
    void setGCTimes(uint64_t pause, uint64_t mark, uint64_t sweep);
    // update GcProfiler's data dump dir path
    void setDir(std::string gcprofile_dir_) {
      data_dir = gcprofile_dir_;
    }
    ~GcProfiler(){}
 private:
    uint32_t gc_id;
    int gcProfInfoFd;
    // duration of the profiling, used for calculate allocation thruput
    int64_t profile_duration;

    AllocInfo *alloc_info;
    GCRecordList *gc_record_head;
    GCRecordList *curr_gc_record;
    FailAllocRecord* curr_fail_record;
    SuccAllocRecordList* succ_alloc_record_head;
    SuccAllocRecordList* curr_succ_alloc_record;
    FailAllocRecordList* fail_alloc_record_head;
    FailAllocRecordList* curr_fail_alloc_record;
    LargeAllocRecord* large_alloc_records;
    uint32_t gc_record_idx;
    uint32_t succ_alloc_idx;
    uint32_t fail_alloc_idx;
    uint32_t large_alloc_idx;
    uint32_t curr_allocated;
    uint32_t curr_footprint;
    uint32_t free_size;
    bool gcProfRunning;
    uint32_t pause_max;
    uint32_t mark_max;
    uint32_t sweep_max;
    static GcProfiler s_instance;
    std::string data_dir;
    void dumpLargeAllocRecords();
    void dumpAllocationInfo();
    void dumpRecords();
    void releaseRecords();
    GcProfiler();
    GcProfiler(const GcProfiler&);
    GcProfiler& operator=(const GcProfiler&);
    // set Fail phase to the FailRecord
    void setFailRecordPhase(AllocFailPhase fail_phase, uint32_t alloc_size);
     // create new GCRecord
    GCRecord* getNextGCRecord();
    // get the current GCRecord
    GCRecord* getCurrentGCRecord();
    // get next SuccAllocRecord
    SuccAllocRecord* getNextSuccAllocRecord();
    // get next FailAllocRecord
    FailAllocRecord* getNextFailAllocRecord();
    // get next LargeAllocRecord
    LargeAllocRecord* getNextLargeAllocRecord();
    // insert the size distribution to allocinfo
    void insert_size_dist(SuccAllocRecord* alloc_info, uint32_t size);
    // clear all related fields
    void clearAllRecords();
};
}   // namespace gc
}   // namespace art

#endif
