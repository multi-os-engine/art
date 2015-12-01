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

#ifndef ART_RUNTIME_JIT_PROFILE_SAVER_H_
#define ART_RUNTIME_JIT_PROFILE_SAVER_H_

#include "base/mutex.h"
#include "jit_code_cache.h"
#include "offline_profiling_info.h"

namespace art {

class ProfileSaver {
 public:
  // Start a profile thread with the user-supplied arguments.
  // Returns true if the profile was started or if it was already running. Returns false otherwise.
  static void Start(const std::string& output_filename, jit::JitCodeCache* jit_code_cache)
      REQUIRES(!Locks::profiler_lock_, !wait_lock_);

  // NO_THREAD_SAFETY_ANALYSIS for static function calling into member function with excludes lock.
  static void Stop()
      REQUIRES(!Locks::profiler_lock_, !wait_lock_)
      NO_THREAD_SAFETY_ANALYSIS;

 private:
  ProfileSaver(const std::string& output_filename, jit::JitCodeCache* jit_code_cache);

  // NO_THREAD_SAFETY_ANALYSIS for static function calling into member function with excludes lock.
  static void* RunProfileSaverThread(void* arg)
      REQUIRES(!Locks::profiler_lock_, !wait_lock_)
      NO_THREAD_SAFETY_ANALYSIS;

  void Run() REQUIRES(!Locks::profiler_lock_, !wait_lock_);
  void ShutDown() REQUIRES(!Locks::profiler_lock_, !wait_lock_);
  bool ShuttingDown(Thread* self) REQUIRES(!Locks::profiler_lock_);
  bool ProcessProfilingInfo();

  static ProfileSaver* instance_ GUARDED_BY(Locks::profiler_lock_);
  // Profile saver thread. Non
  static pthread_t profiler_pthread_ GUARDED_BY(Locks::profiler_lock_);
  // We need to shut the sample thread down at exit. Setting this to true will do that.
  bool shutting_down_ GUARDED_BY(Locks::profiler_lock_);

  // The name of the file where profile data will be written.
  const std::string output_filename_;
  jit::JitCodeCache* jit_code_cache_;
  OfflineProfilingInfo offline_profiling_info_;

  // Profile condition support.
  Mutex wait_lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  ConditionVariable period_condition_ GUARDED_BY(wait_lock_);

  DISALLOW_COPY_AND_ASSIGN(ProfileSaver);
};

}  // namespace art

#endif  // ART_RUNTIME_JIT_PROFILE_SAVER_H_
