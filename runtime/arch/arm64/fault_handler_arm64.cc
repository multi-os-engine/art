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
#include <sys/ucontext.h>
#include "base/macros.h"
#include "base/hex_dump.h"
#include "globals.h"
#include "base/logging.h"
#include "base/hex_dump.h"
#include "mirror/art_method.h"
#include "mirror/art_method-inl.h"
#include "thread.h"
#include "thread-inl.h"

//
// ARM64 specific fault handler functions.
//

namespace art {

extern "C" void art_quick_throw_null_pointer_exception();
extern "C" void art_quick_throw_stack_overflow_from_signal();
extern "C" void art_quick_implicit_suspend();

void FaultManager::GetMethodAndReturnPCAndSP(void* context, mirror::ArtMethod** out_method,
                                             uintptr_t* out_return_pc, uintptr_t* out_sp) {
  struct ucontext *uc = (struct ucontext *)context;
  struct sigcontext *sc = reinterpret_cast<struct sigcontext*>(&uc->uc_mcontext);
  *out_sp = static_cast<uintptr_t>(sc->sp);
  VLOG(signals) << "sp: " << *out_sp;
  if (*out_sp == 0) {
    return;
  }

  // In the case of a stack overflow, the stack is not valid and we can't
  // get the method from the top of the stack.  However it's in x0.
  uintptr_t* fault_addr = reinterpret_cast<uintptr_t*>(sc->fault_address);
  uintptr_t* overflow_addr = reinterpret_cast<uintptr_t*>(
      reinterpret_cast<uint8_t*>(*out_sp) - Thread::kStackOverflowReservedBytes);
  if (overflow_addr == fault_addr) {
    *out_method = reinterpret_cast<mirror::ArtMethod*>(sc->regs[0]);
  } else {
    // The method is at the top of the stack.
    *out_method = reinterpret_cast<mirror::ArtMethod*>(reinterpret_cast<uintptr_t*>(*out_sp)[0]);
  }

  // Work out the return PC.  This will be the address of the instruction
  // following the faulting ldr/str instruction.

  // Need to work out the size of the instruction that caused the exception.
  uint8_t* ptr = reinterpret_cast<uint8_t*>(sc->pc);
  VLOG(signals) << "pc: " << std::hex << static_cast<void*>(ptr);

  *out_return_pc = sc->pc + 4;
}

bool NullPointerHandler::Action(int sig, siginfo_t* info, void* context) {
  // The code that looks for the catch location needs to know the value of the
  // ARM PC at the point of call.  For Null checks we insert a GC map that is immediately after
  // the load/store instruction that might cause the fault.  However the mapping table has
  // the low bits set for thumb mode so we need to set the bottom bit for the LR
  // register in order to find the mapping.

  // Need to work out the size of the instruction that caused the exception.
  struct ucontext *uc = reinterpret_cast<struct ucontext*>(context);
  struct sigcontext *sc = reinterpret_cast<struct sigcontext*>(&uc->uc_mcontext);

  sc->regs[30] = sc->pc + 4;        // Set link register.
  sc->pc = reinterpret_cast<uintptr_t>(art_quick_throw_null_pointer_exception);
  VLOG(signals) << "Generating null pointer exception";
  return true;
}

// TODO: implement this.
bool SuspensionHandler::Action(int sig, siginfo_t* info, void* context) {
  return false;
}

// Stack overflow fault handler.
//
// This checks that the fault address is equal to the current stack pointer
// minus the overflow region size (16K typically).  The instruction sequence
// that generates this signal is:
//
// sub x17,sp,#16384
// ldr x17,[x17,#0]
//
// The second instruction will fault if x17 is inside the protected region
// on the stack.
//
// If we determine this is a stack overflow we need to move the stack pointer
// to the overflow region below the protected region.

bool StackOverflowHandler::Action(int sig, siginfo_t* info, void* context) {
  struct ucontext *uc = (struct ucontext *)context;
  struct sigcontext *sc = reinterpret_cast<struct sigcontext*>(&uc->uc_mcontext);
  VLOG(signals) << "stack overflow handler with sp at " << std::hex << &uc;
  VLOG(signals) << "sigcontext: " << std::hex << sc;

  uintptr_t sp = sc->sp;
  VLOG(signals) << "sp: " << std::hex << sp;

  uintptr_t fault_addr = sc->fault_address;
  VLOG(signals) << "fault_addr: " << std::hex << fault_addr;
  VLOG(signals) << "checking for stack overflow, sp: " << std::hex << sp <<
    ", fault_addr: " << fault_addr;

  uintptr_t overflow_addr = sp - Thread::kStackOverflowReservedBytes;

  Thread* self = reinterpret_cast<Thread*>(sc->regs[18]);
  CHECK_EQ(self, Thread::Current());
  uintptr_t pregion = reinterpret_cast<uintptr_t>(self->GetStackEnd()) -
      Thread::kStackOverflowProtectedSize;

  // Check that the fault address is the value expected for a stack overflow.
  if (fault_addr != overflow_addr) {
    VLOG(signals) << "Not a stack overflow";
    return false;
  }

  // We know this is a stack overflow.  We need to move the sp to the overflow region
  // the exists below the protected region.  Determine the address of the next
  // available valid address below the protected region.
  uintptr_t prevsp = sp;
  sp = pregion;
  VLOG(signals) << "setting sp to overflow region at " << std::hex << sp;

  // Since the compiler puts the implicit overflow
  // check before the callee save instructions, the SP is already pointing to
  // the previous frame.
  VLOG(signals) << "previous frame: " << std::hex << prevsp;

  // Now establish the stack pointer for the signal return.
  sc->sp = prevsp;

  // Tell the stack overflow code where the new stack pointer should be.
  sc->regs[17] = sp;      // aka ip1

  // Now arrange for the signal handler to return to art_quick_throw_stack_overflow_from_signal.
  // The value of LR must be the same as it was when we entered the code that
  // caused this fault.  This will be inserted into a callee save frame by
  // the function to which this handler returns (art_quick_throw_stack_overflow_from_signal).
  sc->pc = reinterpret_cast<uintptr_t>(art_quick_throw_stack_overflow_from_signal);

  // The kernel will now return to the address in sc->pc.
  return true;
}
}       // namespace art
