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

#if __linux__
#include <sys/ptrace.h>
#endif

#include "jni.h"

#include <backtrace/Backtrace.h>

#include "base/logging.h"
#include "base/macros.h"
#include "utils.h"

namespace art {

// For testing debuggerd. We do not have expected-death tests, so can't test this by default.
// Code for this is copied from SignalTest.
static constexpr bool kCauseSegfault = false;
char *go_away_compiler_cfi = nullptr;

static void CauseSegfault() {
#if defined(__arm__) || defined(__i386__) || defined(__x86_64__) || defined(__aarch64__)
  // On supported architectures we cause a real SEGV.
  *go_away_compiler_cfi = 'a';
#else
  // On other architectures we simulate SEGV.
  kill(getpid(), SIGSEGV);
#endif
}

extern "C" JNIEXPORT jboolean JNICALL Java_Main_sleep(JNIEnv*, jobject, jint, jboolean) {
  // We're the spawned-off process. Just go to sleep for a long while (100s).
  NanoSleep(MsToNs(100U * 1000U));
  LOG(FATAL) << "Didn't expect to get here.";
  UNREACHABLE();
}

extern "C" JNIEXPORT jboolean JNICALL Java_Main_unwindInProcess(JNIEnv*, jobject, jint, jboolean) {
#if __linux__
  // TODO: What to do on Valgrind?

  std::unique_ptr<Backtrace> bt(Backtrace::Create(BACKTRACE_CURRENT_PROCESS, GetTid()));
  if (!bt->Unwind(0, nullptr)) {
    return JNI_TRUE;
  } else if (bt->NumFrames() == 0) {
    return JNI_TRUE;
  }

  for (Backtrace::const_iterator it = bt->begin(); it != bt->end(); ++it) {
    if (!BacktraceMap::IsValid(it->map)) {
      LOG(ERROR) << "???" << it->pc;
    } else {
      LOG(ERROR) << it->pc - it->map.start
                 << it->map.name << " "
                 << it->func_name;
    }
  }
#endif

  if (kCauseSegfault) {
    CauseSegfault();
  }

  return JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL Java_Main_unwindOtherProcess(JNIEnv*, jobject, jint pid_int) {
#if __linux__
  // TODO: What to do on Valgrind?
  pid_t pid = static_cast<pid_t>(pid_int);

  // OK, this is painful. debuggerd uses ptrace to unwind other processes.

  LOG(ERROR) << "Trying to attach";

  if (ptrace(PTRACE_ATTACH, pid, 0, 0)) {
    // Were not able to attach, bad.
    PLOG(ERROR) << "Failed to attach.";
    return JNI_FALSE;
  }

  LOG(ERROR) << "Creating backtrace and unwinding.";
  std::unique_ptr<Backtrace> bt(Backtrace::Create(pid, BACKTRACE_CURRENT_THREAD));
  bool result = true;
  if (!bt->Unwind(0, nullptr)) {
    result = false;
  } else if (bt->NumFrames() == 0) {
    result = false;
  }

  LOG(ERROR) << "Iterating through unwind data.";
  for (Backtrace::const_iterator it = bt->begin(); it != bt->end(); ++it) {
    if (!BacktraceMap::IsValid(it->map)) {
      LOG(ERROR) << "???" << it->pc;
    } else {
      LOG(ERROR) << it->pc - it->map.start
                 << it->map.name << " "
                 << it->func_name;
    }
  }

  LOG(ERROR) << "Detaching.";
  if (ptrace(PTRACE_DETACH, pid, 0, 0) != 0) {
    PLOG(ERROR) << "Detach failed";
  }

  return result ? JNI_TRUE : JNI_FALSE;
#else
  return JNI_FALSE;
#endif
}

}  // namespace art
