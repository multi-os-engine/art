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

namespace art {
// Static fault manger object accessed by signal handler.
FaultManager fault_manager;

// Signal handler called on SIGSEGV.
static void art_fault_handler(int sig, siginfo_t* info, void* context) {
  fault_manager.HandleFault(sig, info, context);
}

FaultManager::FaultManager() {
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
  sigaction(SIGSEGV, &action, nullptr);
}

void FaultManager::HandleFault(int sig, siginfo_t* info, void* context) {
  bool handled = false;
  if (IsInGeneratedCode(context)) {
    for (auto& handler : handlers_) {
      handled = handler->Action(sig, info, context);
      if (handled) {
        break;
      }
    }
  }
  if (!handled) {
    LOG(FATAL) << "Caught unknown SIGSEGV in ART fault handler";
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

#ifdef __arm__
  struct ucontext *uc = (struct ucontext *)context;
  struct sigcontext *sc = reinterpret_cast<struct sigcontext*>(&uc->uc_mcontext);
  uintptr_t* sp = reinterpret_cast<uint32_t*>(sc->arm_sp);
  if (sp == nullptr) {
    return false;
  }
  // The method is at the top of the stack.
  potential_method = sp[0];

  // Work out the return PC.  This will be the address of the instruction
  // following the faulting ldr/str instruction.  This is in thumb mode so
  // the instruction might be a 16 or 32 bit one.  Also, the GC map always
  // has the bottom bit of the PC set so we also need to set that.

  // Need to work out the size of the instruction that caused the exception.
  uint8_t* ptr = reinterpret_cast<uint8_t*>(sc->arm_pc);

  uint16_t instr = ptr[0] | ptr[1] << 8;;
  bool is_32bit = ((instr & 0xF000) == 0xF000) || ((instr & 0xF800) == 0xE800);
  uint32_t instr_size = is_32bit ? 4 : 2;
  return_pc = (sc->arm_pc + instr_size) | 1;
#endif

  // If we don't have a potential method, we're outta here.
  if (potential_method == 0) {
    return false;
  }

  // Verify that the potential method is indeed a method.
  mirror::Object* method_obj = reinterpret_cast<mirror::Object*>(potential_method);

  // Check that the class pointer inside the object is not null and is aligned.
  const byte* classaddr = reinterpret_cast<const byte*>(method_obj) +
    mirror::Object::ClassOffset().Int32Value();
  const mirror::Class* cls = *reinterpret_cast<mirror::Class* const *>(classaddr);
  if (cls == nullptr) {
    return false;
  }
  if (!IsAligned<kObjectAlignment>(cls)) {
    return false;
  }

  // Check that the class pointer is indeed a class.
  classaddr = reinterpret_cast<const byte*>(cls) +
    mirror::Object::ClassOffset().Int32Value();

  const mirror::Class* class_class = *reinterpret_cast<mirror::Class* const *>(classaddr);
  classaddr = reinterpret_cast<const byte*>(class_class) +
    mirror::Object::ClassOffset().Int32Value();
  const mirror::Class* class_class_class = *reinterpret_cast<mirror::Class* const *>(classaddr);
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
  mirror::ArtMethod* method = reinterpret_cast<mirror::ArtMethod*>(potential_method);
  return method->ToDexPc(return_pc, false) != DexFile::kDexNoIndex;
}

//
// Null pointer fault handler
//

extern "C" void art_quick_throw_null_pointer_exception();

NullPointerHandler::NullPointerHandler(FaultManager* manager) {
  manager->AddHandler(this);
}

NullPointerHandler::~NullPointerHandler() {
  manager_->RemoveHandler(this);
}

bool NullPointerHandler::Action(int sig, siginfo_t* info, void* context) {
#ifdef __arm__
  // The code that looks for the catch location needs to know the value of the
  // ARM PC at the point of call.  For Null checks we insert a GC map that is immediately after
  // the load/store instruction that might cause the fault.  However the mapping table has
  // the low bits set for thumb mode so we need to set the bottom bit for the LR
  // register in order to find the mapping.

  // Need to work out the size of the instruction that caused the exception.
  struct ucontext *uc = (struct ucontext *)context;
  struct sigcontext *sc = reinterpret_cast<struct sigcontext*>(&uc->uc_mcontext);
  uint8_t* ptr = reinterpret_cast<uint8_t*>(sc->arm_pc);

  uint16_t instr = ptr[0] | ptr[1] << 8;;
  bool is_32bit = ((instr & 0xF000) == 0xF000) || ((instr & 0xF800) == 0xE800);
  uint32_t instr_size = is_32bit ? 4 : 2;
  sc->arm_lr = (sc->arm_pc + instr_size) | 1;      // LR needs to point to gc map location
  sc->arm_pc = reinterpret_cast<uintptr_t>(art_quick_throw_null_pointer_exception);
  LOG(DEBUG) << "Generating null pointer exception";
  return true;
#else
  return false;
#endif
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

bool SuspensionHandler::Action(int sig, siginfo_t* info, void* context) {
  return false;
}

}       // namespace art

