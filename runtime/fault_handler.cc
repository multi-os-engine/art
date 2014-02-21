/*
 * Copyright (C) 2008 The Android Open Source Project
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

#include "fault_handler.h"
#include <sys/mman.h>
#include <sys/ucontext.h>
#include "base/macros.h"
#include "globals.h"
#include "base/logging.h"
#include "base/hex_dump.h"
#include "thread.h"
#include "mirror/art_method-inl.h"
#include "mirror/class-inl.h"
#include "mirror/dex_cache.h"
#include "mirror/object_array-inl.h"
#include "mirror/object-inl.h"
#include "object_utils.h"
#include "scoped_thread_state_change.h"

#ifdef HAVE_ANDROID_OS
extern "C" {
void android_invoke_user_segv_handler(int sig, siginfo_t* info, void* context);
extern void android_claim_segv_signal();
}
#endif

namespace art {
// Static fault manger object accessed by signal handler.
FaultManager fault_manager;

// Signal handler called on SIGSEGV.
static void art_fault_handler(int sig, siginfo_t* info, void* context) {
  fault_manager.HandleFault(sig, info, context);
}

FaultManager::FaultManager() {
  oldaction_.sa_sigaction = nullptr;
}

FaultManager::~FaultManager() {
  // TODO: remove segv handler
}

void FaultManager::Init() {
  struct sigaction action;
  action.sa_sigaction = art_fault_handler;
  sigemptyset(&action.sa_mask);
  action.sa_flags = SA_SIGINFO | SA_ONSTACK;
  action.sa_restorer = nullptr;
  sigaction(SIGSEGV, &action, &oldaction_);
#ifdef HAVE_ANDROID_OS
  android_claim_segv_signal();
#endif
}

void FaultManager::HandleFault(int sig, siginfo_t* info, void* context) {
  bool handled = false;
  if (IsInGeneratedCode(context)) {
    for (auto& handler : handlers_) {
      handled = handler->Action(sig, info, context);
      if (handled) {
        return;
      }
    }
  }
  // No need to block SEGV here since the mask will be reset when the
  // signal handler returns.
  if (!handled) {
    LOG(INFO)<< "Caught unknown SIGSEGV in ART fault handler";
#ifdef HAVE_ANDROID_OS
    android_invoke_user_segv_handler(sig, info, context);
#else
    oldaction_.sa_sigaction(sig, info, context);
#endif
  }
}

void FaultManager::AddHandler(FaultHandler* handler) {
  handlers_.push_back(handler);
}

void FaultManager::RemoveHandler(FaultHandler* handler) {
  for (Handlers::iterator i = handlers_.begin(); i != handlers_.end(); ++i) {
    FaultHandler* h = *i;
    if (h == handler) {
      handlers_.erase(i);
      return;
    }
  }
}


bool FaultManager::IsInGeneratedCode(void *context) {
  // We can only be running Java code in the current thread if it
  // is in Runnable state.
  Thread* thread = Thread::Current();
  if (thread == nullptr) {
    return false;
  }

  ThreadState state = thread->GetState();
  if (state != kRunnable) {
    return false;
  }

  // Current thread is runnable.
  uintptr_t potential_method = 0;
  uintptr_t return_pc = 0;

  // Get the architecture specific method address and return address.  These
  // are in architecture specific files in arch/<arch>/fault_handler_<arch>.cc
  GetMethodAndReturnPC(context, /*out*/potential_method, /*out*/return_pc);

  // If we don't have a potential method, we're outta here.
  if (potential_method == 0) {
    return false;
  }

  // Verify that the potential method is indeed a method.
  // TODO: check the GC maps to make sure it's an object.

  mirror::Object* method_obj =
      reinterpret_cast<mirror::Object*>(potential_method);

  // Check that the class pointer inside the object is not null and is aligned.
  const byte* classaddr = reinterpret_cast<const byte*>(method_obj)
      + mirror::Object::ClassOffset().Int32Value();
  const mirror::Class* cls =
      *reinterpret_cast<mirror::Class* const *>(classaddr);
  if (cls == nullptr) {
    return false;
  }
  if (!IsAligned<kObjectAlignment>(cls)) {
    return false;
  }


  // Check that the class pointer is indeed a class.
  classaddr = reinterpret_cast<const byte*>(cls)
      + mirror::Object::ClassOffset().Int32Value();

  const mirror::Class* class_class =
      *reinterpret_cast<mirror::Class* const *>(classaddr);
  classaddr = reinterpret_cast<const byte*>(class_class)
      + mirror::Object::ClassOffset().Int32Value();
  const mirror::Class* class_class_class =
      *reinterpret_cast<mirror::Class* const *>(classaddr);
  if (class_class != class_class_class) {
    return false;
  }

  // Now make sure the class is a mirror::ArtMethod.
  if (!const_cast<mirror::Class*>(cls)->IsArtMethodClass()) {
    return false;
  }

  // We can be certain that this is a method now.  Check if we have a GC map
  // at the return PC address.

  // Grab the mutator lock.  TODO: don't grab the mutator lock.
  ScopedObjectAccess soa(thread);
  mirror::ArtMethod* method =
      reinterpret_cast<mirror::ArtMethod*>(potential_method);
  return method->ToDexPc(return_pc, false) != DexFile::kDexNoIndex;
}

//
// Null pointer fault handler
//

NullPointerHandler::NullPointerHandler(FaultManager* manager) {
  manager->AddHandler(this);
}

NullPointerHandler::~NullPointerHandler() {
  manager_->RemoveHandler(this);
}

//
// Suspension fault handler
//

SuspensionHandler::SuspensionHandler(FaultManager* manager) {
  manager->AddHandler(this);
}

SuspensionHandler::~SuspensionHandler() {
  manager_->RemoveHandler(this);
}

}       // namespace art

