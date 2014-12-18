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

#ifndef ART_RUNTIME_GC_TASK_PROCESSOR_H_
#define ART_RUNTIME_GC_TASK_PROCESSOR_H_

#include <memory>
#include <queue>

#include "base/mutex.h"
#include "globals.h"
#include "thread_pool.h"

namespace art {
namespace gc {

class HeapTask : public SelfDeletingTask {
 public:
  explicit HeapTask(uint64_t target_run_time) : target_run_time_(target_run_time),
      updated_target_run_time_(target_run_time) {
  }

  // Update the updated_target_run_time_, the task processor will re-insert the task when it is
  // popped and update the target_run_time_. This also means that we can't decrease the target run
  // time, only increase it.
  void SetTargetRunTime(uint64_t new_target_run_time) {
    CHECK_GE(new_target_run_time, target_run_time_);
    // Mark that
    updated_target_run_time_ = new_target_run_time;
  }
  uint64_t GetTargetRunTime() const {
    return target_run_time_;
  }
  uint64_t GetUpdatedTargetTime() const {
    return updated_target_run_time_;
  }

 private:
  void UpdateTargetTime() {
    target_run_time_ = updated_target_run_time_;
  }
  uint64_t target_run_time_;
  uint64_t updated_target_run_time_;

  friend class TaskProcessor;
};

// Used to process GC tasks (heap trim, heap transitions, concurrent GC).
class TaskProcessor {
 public:
  TaskProcessor();
  ~TaskProcessor();
  void AddTask(Thread* self, HeapTask* task) LOCKS_EXCLUDED(lock_);
  HeapTask* GetTask(Thread* self) LOCKS_EXCLUDED(lock_);
  void Interrupt(Thread* self) LOCKS_EXCLUDED(lock_);
  void RunTasksUntilInterrupted(Thread* self) LOCKS_EXCLUDED(lock_);
  bool IsRunning() const LOCKS_EXCLUDED(lock_);

 private:
  class CompareByTargetRunTime {
   public:
    bool operator()(const HeapTask* a, const HeapTask* b) const {
      // Sort by reverse order since we want smallest values of GetTargetRunTime() first and
      // priority queue has largest elements first.
      return a->GetTargetRunTime() > b->GetTargetRunTime();
    }
  };

  mutable Mutex* lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  bool is_running_ GUARDED_BY(lock_);
  std::unique_ptr<ConditionVariable> cond_ GUARDED_BY(lock_);
  std::priority_queue<HeapTask*, std::vector<HeapTask*>, CompareByTargetRunTime> tasks_
      GUARDED_BY(lock_);
};

}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_TASK_PROCESSOR_H_
