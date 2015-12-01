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

#include "profile_saver.h"

#include <sys/file.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include <fstream>

#include "art_method-inl.h"
#include "base/stl_util.h"
#include "base/time_utils.h"
#include "base/unix_file/fd_file.h"
#include "class_linker.h"
#include "common_throws.h"
#include "dex_file-inl.h"
#include "instrumentation.h"
#include "mirror/class-inl.h"
#include "mirror/dex_cache.h"
#include "mirror/object_array-inl.h"
#include "mirror/object-inl.h"
#include "os.h"
#include "scoped_thread_state_change.h"
#include "ScopedLocalRef.h"
#include "thread.h"
#include "thread_list.h"
#include "utils.h"
#include "entrypoints/quick/quick_entrypoints.h"
#include "oat_file_manager.h"

namespace art {

ProfileSaver* ProfileSaver::instance_ = nullptr;
pthread_t ProfileSaver::profiler_pthread_ = 0U;

ProfileSaver::ProfileSaver(
    const std::string& output_filename, jit::JitCodeCache* jit_code_cache)
    : output_filename_(output_filename),
      jit_code_cache_(jit_code_cache),
      wait_lock_("ProfileSaver wait lock"),
      period_condition_("ProfileSaver condition", wait_lock_) {
}

void* ProfileSaver::RunProfileSaverThread(void* arg) {
  Runtime* runtime = Runtime::Current();
  ProfileSaver* profile_saver = reinterpret_cast<ProfileSaver*>(arg);

  CHECK(runtime->AttachCurrentThread("ProfileSaver", true, runtime->GetSystemThreadGroup(),
                                     !runtime->IsAotCompiler()));
  profile_saver->Run();
  LOG(INFO) << "Profiler shutdown";
  runtime->DetachCurrentThread();
  return nullptr;
}

void ProfileSaver::Start(const std::string& output_filename,
                         jit::JitCodeCache* jit_code_cache) {
  DCHECK(!output_filename.empty());

  Thread* self = Thread::Current();
  MutexLock mu(self, *Locks::profiler_lock_);
  // Don't start two profile saver threads.
  if (instance_ != nullptr) {
    DCHECK(false) << "Tried to start two profile savers";
    return;
  }

  VLOG(profiler) << "Starting profile saver using output file: " << output_filename;

  instance_ = new ProfileSaver(output_filename, jit_code_cache);

  CHECK_PTHREAD_CALL(
      pthread_create,
      (&profiler_pthread_, nullptr, &RunProfileSaverThread, reinterpret_cast<void*>(instance_)),
      "Profiler thread");
}

void ProfileSaver::Stop() {
  ProfileSaver* profile_saver = nullptr;
  pthread_t profiler_pthread = 0U;

  {
    MutexLock profiler_mutex(Thread::Current(), *Locks::profiler_lock_);
    profile_saver = instance_;
    profiler_pthread = profiler_pthread_;
    if (instance_->shutting_down_) {
      // Already shutting down.
      DCHECK(false) << "Tried to stop the profile_saver twice.";
      return;
    }
    instance_->shutting_down_ = true;
  }

  {
    MutexLock wait_mutex(Thread::Current(), profile_saver->wait_lock_);
    profile_saver->period_condition_.Signal(Thread::Current());
  }

  // Wait for the saving thread to stop.
  CHECK_PTHREAD_CALL(pthread_join, (profiler_pthread, nullptr), "profile saver thread shutdown");

  {
    MutexLock profiler_mutex(Thread::Current(), *Locks::profiler_lock_);
    instance_ = nullptr;
    profiler_pthread_ = 0U;
  }
  delete profile_saver;
}

void ProfileSaver::Run() {
  // Add a random delay for the first time run so that we don't hammer the CPU
  // with all profiles running at the same time.
  const uint64_t kRandomDelayMaxMs = 1000;  // 1 second;
  const uint64_t kMaxBackoffMs = 20 * 1000;  // 20 seconds;
  const uint64_t kSavePeriodMs = 4 * 1000;  // 4 seconds;
  const double kBackOffCoef = 1.5;

  srand(MicroTime() * getpid());
  Thread* self = Thread::Current();

  uint64_t save_period_ms = kSavePeriodMs;
  while (true) {
    if (ShuttingDown(self)) {
      break;
    }

    uint64_t random_sleep_delay_ms = rand() % kRandomDelayMaxMs;
    uint64_t sleep_time_ms = save_period_ms + random_sleep_delay_ms;

    {
      MutexLock mu(self, wait_lock_);
      period_condition_.TimedWait(self, sleep_time_ms * 1000, 0);
    }

    if (ShuttingDown(self)) {
      break;
    }

    if (ProcessProfilingInfo()) {
      save_period_ms =
          std::min(kMaxBackoffMs, static_cast<uint64_t>(kBackOffCoef * save_period_ms));
    }
  }
}

bool ProfileSaver::ShuttingDown(Thread* self) {
  MutexLock mu(self, *Locks::profiler_lock_);
  return shutting_down_;
}

bool ProfileSaver::ProcessProfilingInfo() {
  uint64_t last_update_ns = jit_code_cache_->GetLastUpdateTimeNs();
  if (offline_profiling_info_.NeedsSaving(last_update_ns)) {
    VLOG(profiler) << "Initiate save profiling information to: " << output_filename_;
    const OatFile* primary_oat_file = Runtime::Current()->GetOatFileManager().GetPrimaryOatFile();
    std::set<ArtMethod*> methods;
    {
      ScopedObjectAccess soa(Thread::Current());
      jit_code_cache_->GetCompiledArtMethods(primary_oat_file, methods);
    }
    offline_profiling_info_.SaveProfilingInfo(output_filename_, last_update_ns, methods);
    return true;
  } else {
    return false;
    VLOG(profiler) << "No need to save profiling information to: " << output_filename_;
  }
}

}   // namespace art
