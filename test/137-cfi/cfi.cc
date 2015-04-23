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
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
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
  // Keep pausing.
  for (;;) {
    pause();
  }
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

static constexpr int SLEEP_TIME_USEC = 50000;          // 0.05 seconds
static constexpr int MAX_TOTAL_SLEEP_USEC = 1000000;  // 10 seconds

int wait_for_sigstop(pid_t tid, int* total_sleep_time_usec, bool* detach_failed ATTRIBUTE_UNUSED) {
#if __linux__
//  bool allow_dead_tid = false;
  for (;;) {
    int status;
    pid_t n = TEMP_FAILURE_RETRY(waitpid(tid, &status, __WALL | WNOHANG));
    if (n == -1) {
      PLOG(ERROR) << "waitpid failed: tid " << tid;
      break;
    } else if (n == tid) {
      if (WIFSTOPPED(status)) {
        return WSTOPSIG(status);
      } else {
        PLOG(ERROR) << "unexpected waitpid response: n=" << n << ", status=" << std::hex << status;
        // This is the only circumstance under which we can allow a detach
        // to fail with ESRCH, which indicates the tid has exited.
//        allow_dead_tid = true;
        break;
      }
    }

    if (*total_sleep_time_usec > MAX_TOTAL_SLEEP_USEC) {
      PLOG(ERROR) << "timed out waiting for stop signal: tid=" << tid;
      break;
    }

    usleep(SLEEP_TIME_USEC);
    *total_sleep_time_usec += SLEEP_TIME_USEC;
  }

//  if (ptrace(PTRACE_DETACH, tid, 0, 0) != 0) {
//    if (allow_dead_tid && errno == ESRCH) {
//      PLOG(ERROR) << "tid exited before attach completed: tid " << tid;
//    } else {
//      *detach_failed = true;
//      PLOG(ERROR) <<  "detach failed: tid " << tid;
//    }
//  }
  return -1;
#else
  return -1;
#endif
}

extern "C" JNIEXPORT jboolean JNICALL Java_Main_unwindOtherProcess(JNIEnv*, jobject, jint pid_int) {
#if __linux__
  // TODO: What to do on Valgrind?
  pid_t pid = static_cast<pid_t>(pid_int);

  // OK, this is painful. debuggerd uses ptrace to unwind other processes.

  LOG(ERROR) << "Trying to attach";

  kill(pid, SIGSTOP);

  if (ptrace(PTRACE_ATTACH, pid, 0, 0)) {
    // Were not able to attach, bad.
    PLOG(ERROR) << "Failed to attach.";
    return JNI_FALSE;
  }

  kill(pid, SIGSTOP);

  bool detach_failed = false;
  int total_sleep_time_usec = 0;
  int signal = wait_for_sigstop(pid, &total_sleep_time_usec, &detach_failed);
  if (signal == -1) {
    LOG(ERROR) << "wait_for_sigstop failed.";
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
