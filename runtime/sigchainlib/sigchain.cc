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

#include <android/log.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

union kernel_sigset_t {
  kernel_sigset_t() {
    clear();
  }

  kernel_sigset_t(const sigset_t* value) {
    clear();
    set(value);
  }

  void clear() {
    __builtin_memset(this, 0, sizeof(*this));
  }

  void set(const sigset_t* value) {
    bionic = *value;
  }

  sigset_t* get() {
    return &bionic;
  }

  sigset_t bionic;
#ifndef __mips__
  uint32_t kernel[2];
#endif
};


namespace art {

extern "C" {
#if __LP64__
void __rt_sigreturn(void);
int __rt_sigaction(int, const struct __kernel_sigaction*, struct __kernel_sigaction*, size_t);
#else
int __sigaction(int, const struct sigaction*, struct sigaction*);
#endif

int __rt_sigprocmask(int, const kernel_sigset_t*, kernel_sigset_t*, size_t);
}   // extern "C"

class SignalAction {
 public:
  SignalAction() : claimed_(false) {
  }

  // Claim the signal and keep the action specified.
  void Claim(const struct sigaction& action) {
    action_ = action;
    claimed_ = true;
  }

  // Unclaim the signal and restore the old action.
  void Unclaim(int signal) {
    claimed_ = false;
    sigaction(signal, &action_, NULL);        // Restore old action.
  }

  // Get the action associated with this signal.
  const struct sigaction& GetAction() const {
    return action_;
  }

  // Is the signal claimed?
  bool IsClaimed() const {
    return claimed_;
  }

  // Change the recorded action to that specified.
  void SetAction(const struct sigaction& action) {
    action_ = action;
  }

 private:
  struct sigaction action_;     // Action to be performed.
  bool claimed_;                // Whether signal is claimed or not.
};

// User's signal handlers
static SignalAction user_sigactions[_NSIG];

static void log(const char* format, ...) {
  char buf[256];
  va_list ap;
  va_start(ap, format);
  vsnprintf(buf, sizeof(buf), format, ap);
  __android_log_write(ANDROID_LOG_ERROR, "libsigchain", buf);
  va_end(ap);
}

static void CheckSignalValid(int signal) {
  if (signal <= 0 || signal >= _NSIG) {
    log("Invalid signal %d", signal);
    abort();
  }
}

// Claim a signal chain for a particular signal.
void ClaimSignalChain(int signal, struct sigaction* oldaction) {
  CheckSignalValid(signal);
  user_sigactions[signal].Claim(*oldaction);
}

void UnclaimSignalChain(int signal) {
  CheckSignalValid(signal);

  user_sigactions[signal].Unclaim(signal);
}

// Invoke the user's signal handler.
void InvokeUserSignalHandler(int sig, siginfo_t* info, void* context) {
  // Check the arguments.
  CheckSignalValid(sig);

  // The signal must have been claimed in order to get here.  Check it.
  if (!user_sigactions[sig].IsClaimed()) {
    abort();
  }

  const struct sigaction& action = user_sigactions[sig].GetAction();

  // Only deliver the signal if the signal was not masked out.
  if (sigismember(&action.sa_mask, sig)) {
     return;
  }
  if ((action.sa_flags & SA_SIGINFO) == 0) {
    if (action.sa_handler != NULL) {
      action.sa_handler(sig);
    }
  } else {
    if (action.sa_sigaction != NULL) {
      action.sa_sigaction(sig, info, context);
    }
  }
}

extern "C" {
// These functions are C linkage since they replace the functions in libc.

int sigaction(int signal, const struct sigaction* new_action, struct sigaction* old_action) {
  // If this signal has been claimed as a signal chain, record the user's
  // action but don't pass it on to the kernel.
  // Note that we check that the signal number is in range here.  An out of range signal
  // number should behave exactly as the libc sigaction.
  if (signal > 0 && signal < _NSIG && user_sigactions[signal].IsClaimed()) {
    if (old_action != NULL) {
      *old_action = user_sigactions[signal].GetAction();
    }
    if (new_action != NULL) {
      user_sigactions[signal].SetAction(*new_action);
    }
    return 0;
  }

  // Will only get here if the signal chain has not been claimed.  We want
  // to pass the sigaction on to the kernel.

#if __LP64__
  __kernel_sigaction kernel_new_action;
  if (new_action != NULL) {
    kernel_new_action.sa_flags = new_action->sa_flags;
    kernel_new_action.sa_handler = new_action->sa_handler;
    kernel_new_action.sa_mask = new_action->sa_mask;
#ifdef SA_RESTORER
    kernel_new_action.sa_restorer = new_action->sa_restorer;

    if (!(kernel_new_action.sa_flags & SA_RESTORER)) {
      kernel_new_action.sa_flags |= SA_RESTORER;
      kernel_new_action.sa_restorer = &__rt_sigreturn;
    }
#endif
  }

  __kernel_sigaction kernel_old_action;
  int result = __rt_sigaction(signal,
                              (new_action != NULL) ? &kernel_new_action : NULL,
                              &kernel_old_action,
  if (old_action != NULL) {
    old_action->sa_flags = kernel_old_action.sa_flags;
    old_action->sa_handler = kernel_old_action.sa_handler;
    old_action->sa_mask = kernel_old_action.sa_mask;
#ifdef SA_RESTORER
    old_action->sa_restorer = kernel_old_action.sa_restorer;

    if (old_action->sa_restorer == &__rt_sigreturn) {
      old_action->sa_flags &= ~SA_RESTORER;
    }
#endif
  }

#else
  // The 32-bit ABI is broken. struct sigaction includes a too-small sigset_t.
  // TODO: if we also had correct struct sigaction definitions available, we could copy in and out.
  int result = __sigaction(signal, new_action, old_action);
#endif

  return result;
}

int sigprocmask(int how, const sigset_t* bionic_new_set, sigset_t* bionic_old_set) {
  kernel_sigset_t new_set;
  kernel_sigset_t* new_set_ptr = NULL;
  if (bionic_new_set != NULL) {
    sigset_t tmpset = *bionic_new_set;
    if (how == SIG_BLOCK) {
      // Don't allow claimed signals in the mask.  If a signal chain has been claimed
      // we can't allow the user to block that signal.
      for (int i = 0 ; i < _NSIG; ++i) {
        if (user_sigactions[i].IsClaimed() && sigismember(&tmpset, i)) {
            sigdelset(&tmpset, i);
        }
      }
    }
    new_set.set(&tmpset);
    new_set_ptr = &new_set;
  }

  kernel_sigset_t old_set;
  if (__rt_sigprocmask(how, new_set_ptr, &old_set, sizeof(old_set)) == -1) {
    return -1;
  }

  if (bionic_old_set != NULL) {
    *bionic_old_set = old_set.bionic;
  }

  return 0;
}
}   // extern "C"
}   // namespace art

