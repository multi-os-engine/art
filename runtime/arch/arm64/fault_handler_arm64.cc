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


#include "fault_handler.h"
#include <sys/ucontext.h>
#include "base/macros.h"
#include "globals.h"
#include "base/logging.h"
#include "base/hex_dump.h"
#include "registers_arm64.h"
#include "mirror/art_method.h"
#include "mirror/art_method-inl.h"
#include "thread.h"
#include "thread-inl.h"

extern "C" void art_quick_throw_stack_overflow();
extern "C" void art_quick_throw_null_pointer_exception();
extern "C" void art_quick_implicit_suspend();

//
// ARM64 specific fault handler functions.
//

namespace art {

void FaultManager::HandleNestedSignal(int sig, siginfo_t* info, void* context) {
  // To match the case used in ARM we return directly to the longjmp function
  // rather than through a trivial assembly language stub.

  struct ucontext *uc = reinterpret_cast<struct ucontext*>(context);
  struct sigcontext *sc = reinterpret_cast<struct sigcontext*>(&uc->uc_mcontext);
  Thread* self = Thread::Current();
  CHECK(self != nullptr);       // This will cause a SIGABRT if self is nullptr.

  sc->regs[0] = reinterpret_cast<uintptr_t>(*self->GetNestedSignalState());
  sc->regs[1] = 1;
  sc->pc = reinterpret_cast<uintptr_t>(longjmp);
}

void FaultManager::GetMethodAndReturnPcAndSp(siginfo_t* siginfo, void* context,
                                             mirror::ArtMethod** out_method,
                                             uintptr_t* out_return_pc, uintptr_t* out_sp) {
  struct ucontext *uc = reinterpret_cast<struct ucontext *>(context);
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
      reinterpret_cast<uint8_t*>(*out_sp) - GetStackOverflowReservedBytes(kArm64));
  if (overflow_addr == fault_addr) {
    *out_method = reinterpret_cast<mirror::ArtMethod*>(sc->regs[0]);
  } else {
    // The method is at the top of the stack.
    *out_method = (reinterpret_cast<StackReference<mirror::ArtMethod>* >(*out_sp)[0]).AsMirrorPtr();
  }

  // Work out the return PC.  This will be the address of the instruction
  // following the faulting ldr/str instruction.
  VLOG(signals) << "pc: " << std::hex
      << static_cast<void*>(reinterpret_cast<uint8_t*>(sc->pc));

  *out_return_pc = sc->pc + 4;
}

bool NullPointerHandler::Action(int sig, siginfo_t* info, void* context) {
  // The code that looks for the catch location needs to know the value of the
  // PC at the point of call.  For Null checks we insert a GC map that is immediately after
  // the load/store instruction that might cause the fault.

  struct ucontext *uc = reinterpret_cast<struct ucontext*>(context);
  struct sigcontext *sc = reinterpret_cast<struct sigcontext*>(&uc->uc_mcontext);

  sc->regs[30] = sc->pc + 4;      // LR needs to point to gc map location

  sc->pc = reinterpret_cast<uintptr_t>(art_quick_throw_null_pointer_exception);
  VLOG(signals) << "Generating null pointer exception";
  return true;
}

// A suspend check is done using the following instruction:
//      0xf725a228: f9400273  ldr x19, [x19]

bool SuspensionHandler::Action(int sig, siginfo_t* info, void* context) {
  uint32_t checkinst = 0xf9400273;

  struct ucontext *uc = reinterpret_cast<struct ucontext *>(context);
  struct sigcontext *sc = reinterpret_cast<struct sigcontext*>(&uc->uc_mcontext);
  uint8_t* ptr = reinterpret_cast<uint8_t*>(sc->pc);

  uint32_t inst = *reinterpret_cast<uint32_t*>(ptr);
  VLOG(signals) << "inst: " << std::hex << inst << " checkinst: " << checkinst;
  if (inst != checkinst) {
    // Instruction is not good, not ours.
    return false;
  }

  VLOG(signals) << "suspend check match";
  // This is a suspend check.  Arrange for the signal handler to return to
  // art_quick_implicit_suspend.  Also set LR so that after the suspend check it
  // will resume the instruction (current PC + 4).  PC points to the
  // ldr x0,[x0,#0] instruction (r0 will be 0, set by the trigger).

  sc->regs[30] = sc->pc + 4;
  sc->pc = reinterpret_cast<uintptr_t>(art_quick_implicit_suspend);

  // Now remove the suspend trigger that caused this fault.
  Thread::Current()->RemoveSuspendTrigger();
  VLOG(signals) << "removed suspend trigger invoking test suspend";
  return true;
}

bool StackOverflowHandler::Action(int sig, siginfo_t* info, void* context) {
  struct ucontext *uc = reinterpret_cast<struct ucontext *>(context);
  struct sigcontext *sc = reinterpret_cast<struct sigcontext*>(&uc->uc_mcontext);
  VLOG(signals) << "stack overflow handler with sp at " << std::hex << &uc;
  VLOG(signals) << "sigcontext: " << std::hex << sc;

  uintptr_t sp = sc->sp;
  VLOG(signals) << "sp: " << std::hex << sp;

  uintptr_t fault_addr = sc->fault_address;
  VLOG(signals) << "fault_addr: " << std::hex << fault_addr;
  VLOG(signals) << "checking for stack overflow, sp: " << std::hex << sp <<
      ", fault_addr: " << fault_addr;

  uintptr_t overflow_addr = sp - GetStackOverflowReservedBytes(kArm64);

  // Check that the fault address is the value expected for a stack overflow.
  if (fault_addr != overflow_addr) {
    VLOG(signals) << "Not a stack overflow";
    return false;
  }

  VLOG(signals) << "Stack overflow found";

  // Now arrange for the signal handler to return to art_quick_throw_stack_overflow.
  // The value of LR must be the same as it was when we entered the code that
  // caused this fault.  This will be inserted into a callee save frame by
  // the function to which this handler returns (art_quick_throw_stack_overflow).
  sc->pc = reinterpret_cast<uintptr_t>(art_quick_throw_stack_overflow);

  // The kernel will now return to the address in sc->pc.
  return true;
}
}       // namespace art

