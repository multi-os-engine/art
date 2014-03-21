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

#define WITH_VIXL_DEBUGGER 1

#include "a64/simulator-a64.h"
#include "a64/macro-assembler-a64.h"

#include "base/macros.h"
#include "thread.h"
#include "thread-inl.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>

using art::Thread;

using vixl::Decoder;
using vixl::Instruction;
using vixl::Reg31IsStackPointer;

// Original trampolines.
extern "C" void art_quick_resolution_trampoline(void);

// Trampoline replacements.
extern "C" void art_foreign_quick_resolution_trampoline(void);


#if WITH_VIXL_DEBUGGER
#  define MY_PARENT_CLASS vixl::Debugger
#else
#  define MY_PARENT_CLASS vixl::Simulator
#endif


// Map between x86 register (as saved in the table) and the corresponding
// A64 register.
static const int a64_from_x86_[] = {0, 1, 2, 3};

// A hacked Debugger/Simulator which allows calling back into x86 code via the
// special recognised sequence "brk #(0x8000 + NN); blr xzr;" which should be
// used as a replacement for "blr xNN".
class MyRunner : public MY_PARENT_CLASS {
 private:
  int64_t saved_br_lr_;  // Value of the lr register before doing a "blr xzr".
  int saved_br_reg_;     // Register containing the target pointer when doing "blr xzr".
  Thread* self_;         // Thread this simulator is associated with.
  intptr_t *x86_regs_;   // x86 register table.
  uint32_t frame_size_;  // Size of the frame for the A64 method.
  void *sp_at_entry_;    // Stack pointer at entry of A64 method.

 public:
  MyRunner(Decoder* decoder, Thread *self, intptr_t *x86_regs, uint32_t frame_size,
           FILE* stream = stdout) : MY_PARENT_CLASS(decoder, stream) {
    saved_br_reg_ = -1;
    saved_br_lr_ = INT64_C(-1);
    self_ = self;
    x86_regs_ = x86_regs;
    frame_size_ = frame_size;
    sp_at_entry_ = NULL;
  }

  // Set the A64 registers from the given set of x86 registers.
  void GetRegsFromX86() {
    for (unsigned int i = 0; i < arraysize(a64_from_x86_); i++) {
      if (a64_from_x86_[i] != 31) {
        set_wreg(a64_from_x86_[i], x86_regs_[i]);
      }
    }
  }

  // Move the A64 registers to the X86 register table.
  void PutRegsToX86() {
    for (unsigned int i = 0; i < arraysize(a64_from_x86_); i++) {
      if (a64_from_x86_[i] != 31) {
        x86_regs_[i] = wreg(a64_from_x86_[i]);
      }
    }
  }

  void VisitException(Instruction* instr) {
    uint32_t instrBits = instr->InstructionBits();
    if ((instrBits & 0xfff00000) == 0xd4300000) {
      saved_br_reg_ = (instrBits & 0x000fffe0) >> 5;
      saved_br_lr_ = xreg(30);
    } else {
      MY_PARENT_CLASS::VisitException(instr);
    }
  }

  void Run() {
    while (true) {
      MY_PARENT_CLASS::Run();

      // Try to determine why we are here.
      if (saved_br_reg_ < 0) {
        // Probably, a "ret" was used. Quit!
        return;
      }

      // A simulated branch was used. A branch "blr xNN" is simulated through
      // the two instructions "brk #(0x1000 + NN); blr xzr;". We handle this by
      // calling a function whose pointer is stored in "xNN".
      int64_t fn_reg = (saved_br_reg_ == 30) ? saved_br_lr_ : xreg(saved_br_reg_);
      int64_t return_pc = xreg(30);
      void *fn = reinterpret_cast<void*>(fn_reg);
      PutRegsToX86();

      self_->quick_entrypoints_.pForeignCodeCallBack(reinterpret_cast<void*>(return_pc),
                                                     sp_at_entry_, frame_size_, x86_regs_, fn);
      GetRegsFromX86();

      // Return executing in A64 code.
      set_pc(reinterpret_cast<Instruction*>(return_pc));
      saved_br_reg_ = -1;
      saved_br_lr_ = INT64_C(-1);
    }
  }

  void RunFrom(Instruction* first, uintptr_t* sp_at_entry) {
    // Set the stack pointer.
    sp_at_entry_ = sp_at_entry;
    set_xreg(9, (uint64_t)self_);
    set_wreg(31, (uintptr_t)sp_at_entry, Reg31IsStackPointer);
    GetRegsFromX86();
    set_pc(first);
    Run();
    PutRegsToX86();
  }
};


extern "C" void
artArm64CodeCall(void* a64Code, intptr_t* x86_regs, uintptr_t* sp_at_entry) {
  uint32_t frame_size = reinterpret_cast<uint32_t*>(a64Code)[-1];
  Decoder decoder;
  MyRunner runner(&decoder, Thread::Current(), x86_regs, frame_size);
  //                        ^^^ TODO(Arm64): get this from the fs register
  runner.RunFrom(reinterpret_cast<Instruction*>(a64Code), sp_at_entry);
}
