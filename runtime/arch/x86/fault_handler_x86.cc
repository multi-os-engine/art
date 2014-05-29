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
#include "globals.h"
#include "base/logging.h"
#include "base/hex_dump.h"
#include "mirror/art_method.h"
#include "mirror/art_method-inl.h"
#include "thread.h"
#include "thread-inl.h"


//
// X86 specific fault handler functions.
//

namespace art {

extern "C" void art_quick_throw_null_pointer_exception();
extern "C" void art_quick_throw_stack_overflow_from_signal();
extern "C" void art_quick_implicit_suspend();

// Get the size of an instruction in bytes.
// This should only be called for instructions we know about.
static uint32_t GetInstructionSize(uint8_t* pc) {
  uint8_t modrm = pc[1];
  uint8_t mod = (modrm >> 6) & 0b11;
  uint8_t reg = (modrm >> 3) & 0b111;
  switch (mod) {
  case 0:
    break;
  case 1:
    if (reg == 4) {
      // SIB + 1 byte displacement.
      return 4;
    } else {
      return 3;
    }
  case 2:
    // SIB + 4 byte displacement.
    return 7;
  case 3:
    break;
  }
  return 2;     // Just opcode + modrm.
}

void FaultManager::GetMethodAndReturnPCAndSP(siginfo_t* siginfo, void* context,
                                             mirror::ArtMethod** out_method,
                                             uintptr_t* out_return_pc, uintptr_t* out_sp) {
  struct ucontext* uc = reinterpret_cast<struct ucontext*>(context);
  *out_sp = static_cast<uintptr_t>(uc->uc_mcontext.gregs[REG_ESP]);
  VLOG(signals) << "sp: " << std::hex << *out_sp;
  if (*out_sp == 0) {
    return;
  }

  // In the case of a stack overflow, the stack is not valid and we can't
  // get the method from the top of the stack.  However it's in EAX.
  uintptr_t* fault_addr = reinterpret_cast<uintptr_t*>(siginfo->si_addr);
  uintptr_t* overflow_addr = reinterpret_cast<uintptr_t*>(
      reinterpret_cast<uint8_t*>(*out_sp) - Thread::kStackOverflowReservedBytes);
  if (overflow_addr == fault_addr) {
    *out_method = reinterpret_cast<mirror::ArtMethod*>(uc->uc_mcontext.gregs[REG_EAX]);
  } else {
    // The method is at the top of the stack.
    *out_method = reinterpret_cast<mirror::ArtMethod*>(reinterpret_cast<uintptr_t*>(*out_sp)[0]);
  }

  uint8_t* pc = reinterpret_cast<uint8_t*>(uc->uc_mcontext.gregs[REG_EIP]);
  VLOG(signals) << "pc: " << std::hex << static_cast<void*>(pc);

  LOG(INFO) << HexDump(pc, 32, true, "PC ");

  uint32_t instr_size = GetInstructionSize(pc);
  *out_return_pc = reinterpret_cast<uintptr_t>(pc + instr_size);
}

bool NullPointerHandler::Action(int sig, siginfo_t* info, void* context) {
  struct ucontext *uc = reinterpret_cast<struct ucontext*>(context);
  uint8_t* pc = reinterpret_cast<uint8_t*>(uc->uc_mcontext.gregs[REG_EIP]);
  uint8_t* sp = reinterpret_cast<uint8_t*>(uc->uc_mcontext.gregs[REG_ESP]);

  uint32_t instr_size = GetInstructionSize(pc);
  // We need to arrange for the signal handler to return to the null pointer
  // exception generator.  The return address must be the address of the
  // next instruction (this instruction + instruction size).  The return address
  // is on the stack at the top address of the current frame.
#if 0

  // Get current method
  mirror::ArtMethod* method = reinterpret_cast<mirror::ArtMethod*>(
            reinterpret_cast<uintptr_t*>(sp)[0]);

  // We need to get the frame size from the method.
  uint32_t frame_size = GetFrameSizeInBytes<false>();
  uint8_t* next_frame = sp + frame_size;

  // The next_frame is the address of the next frame on the stack.  The return address
  // is just below this.  We overwrite the return address with the address of the
  // next instruction.
  uintptr_t* retaddr = reinterpret_cast<uintptr_t*>(next_frame - 4);

  *retaddr = pc + instr_size;      // return addr needs to point to gc map location
#else
  // Push the return address onto the stack.
  uint32_t retaddr = reinterpret_cast<uint32_t>(pc + instr_size);
  uint32_t* next_sp = reinterpret_cast<uint32_t*>(sp - 4);
  *next_sp = retaddr;
  uc->uc_mcontext.gregs[REG_ESP] = reinterpret_cast<uint32_t>(next_sp);
#endif

  uc->uc_mcontext.gregs[REG_EIP] =
        reinterpret_cast<uintptr_t>(art_quick_throw_null_pointer_exception);
  VLOG(signals) << "Generating null pointer exception";
  return true;
}

bool SuspensionHandler::Action(int sig, siginfo_t* info, void* context) {
  return false;
}

bool StackOverflowHandler::Action(int sig, siginfo_t* info, void* context) {
  return false;
}
}       // namespace art
