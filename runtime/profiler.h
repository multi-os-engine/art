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

#ifndef ART_RUNTIME_PROFILER_H_
#define ART_RUNTIME_PROFILER_H_

#include <ostream>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "globals.h"
#include "instrumentation.h"
#include "os.h"
#include "safe_map.h"
#include "base/mutex.h"
#include "locks.h"
#include "UniquePtr.h"
#include "barrier.h"

namespace art {

namespace mirror {
  class ArtMethod;
  class Class;
}  // namespace mirror
class Thread;


class ProfileTable {
 public:
  explicit ProfileTable(Mutex& lock);
  ~ProfileTable();

  void put(mirror::ArtMethod* method);
  uint32_t write(std::ostream &os);
  void clear();
  uint32_t get_num_samples() { return num_samples_; }
  void NullMethod() { ++num_null_methods_;}
  void BootMethod() { ++num_boot_methods_; }
 private:
  uint32_t hash(mirror::ArtMethod* method);
  static constexpr int kHashSize = 17;
  Mutex& lock_;
  uint32_t num_samples_;
  uint32_t num_null_methods_;
  uint32_t num_boot_methods_;
  typedef std::map<mirror::ArtMethod*, uint32_t> Map;
  Map *table[kHashSize];
};

class Profiler {
 public:
  static void Start(int period, int duration, std::string profile_filename, int interval_us,
                    double backoff_coefficient, bool startImmediately)
  LOCKS_EXCLUDED(Locks::mutator_lock_,
                 Locks::thread_list_lock_,
                 Locks::thread_suspend_count_lock_,
                 Locks::profiler_lock_);

  static void Stop() LOCKS_EXCLUDED(Locks::profiler_lock_);
  static void Shutdown() LOCKS_EXCLUDED(Locks::profiler_lock_);

  void RecordMethod(mirror::ArtMethod *method) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  Barrier& GetBarrier() {
    return *profiler_barrier_;
  }

 private:
  explicit Profiler(int period, int duration, std::string profile_filename,
                 double backoff_coefficient, int interval_us, bool startImmediately);

  // The sampling interval in microseconds is passed as an argument.
  static void* RunProfilerThread(void* arg) LOCKS_EXCLUDED(Locks::profiler_lock_);

  uint32_t WriteProfile() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void CleanProfile();
  uint32_t DumpProfile(std::ostream& os) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static bool ShuttingDown(Thread* self);

  static Profiler* profiler_ GUARDED_BY(Locks::profiler_lock_);

  // We need to shut the sample thread down at exit.  Setting this to true will do that.
  static volatile bool shutting_down_ GUARDED_BY(Locks::profiler_lock_);

  // Sampling thread, non-zero when sampling.
  static pthread_t profiler_pthread_;

  // Some measure of the number of samples that are significant
  static constexpr uint32_t kSignificantSamples = 10;

  // File to write profile data out to.  Cannot be NULL if we are profiling.
  std::string profile_file_name_;

  // Number of seconds between profile runs.
  int period_;

  bool start_immediately_;

  int interval_us_;

  // A backoff coefficent to adjust the profile period based on time.
  double backoff_factor_;

  // How much to increase the backoff by on each profile iteration.
  double backoff_coefficient_;

  // Duration of each profile run.  The profile file will be written at the end
  // of each run.
  int duration_;

  // Profile condition support.
  Mutex wait_lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  ConditionVariable period_condition_ GUARDED_BY(wait_lock_);

  ProfileTable profile_table_;

  UniquePtr<Barrier> profiler_barrier_;

  // Set of methods to be filtered out.  This will probably be rare because
  // most of the methods we want to be filtered reside in the boot path and
  // are automatically filtered.
  typedef std::set<std::string> FilteredMethods;
  FilteredMethods filtered_methods_;

  DISALLOW_COPY_AND_ASSIGN(Profiler);
};

}  // namespace art

#endif  // ART_RUNTIME_PROFILER_H_
