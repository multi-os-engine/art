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

#include "simulator/code_simulator_arm64.h"
// For fmod and fmodf.
#include "asm_support.h"
#include "class_linker-inl.h"
#include "oat_file-inl.h"
#include "simulator/code_simulator_arm64.h"
#include <cstring>

namespace art {
namespace arm64 {

  // Special registers defined in asm_support_arm64.s.
  // Register holding Thread::current().
  static const unsigned kSelf = 19;
  // Frame Pointer.
  static const unsigned kFp = 29;
  // Stack Pointer.
  static const unsigned kSp = 31;

class CustomSimulator FINAL: public vixl::Simulator {
 public:
  explicit CustomSimulator(vixl::Decoder* decoder) : vixl::Simulator(decoder) {}
  virtual ~CustomSimulator() {}

  // Override vixl::Simulator::VisitUnconditionalBranchToRegister to handle any runtime invokes
  // we know can be simulated.
  virtual void VisitUnconditionalBranchToRegister(const vixl::Instruction* instr) OVERRIDE
      SHARED_REQUIRES(Locks::mutator_lock_) {
    // For branching to fixed addresses or labels, nothing has changed.
    if (instr->Mask(vixl::UnconditionalBranchToRegisterMask) != vixl::BLR) {
      Simulator::VisitUnconditionalBranchToRegister(instr);
      return;
    }

    //  Otherwise, intercept any invokes to runtime entrypoints we know we can simulate.
    const void* target = reinterpret_cast<const void*>(xreg(instr->Rn()));

    // TODO(simulator): Handle all invocations of runtime.
    if (target == fmod) {
      SimulateFmod();
    } else if (target == fmodf) {
      SimulateFmodf();
    } else {
      // In other cases, the target code should be quick code.
      UNIMPLEMENTED(FATAL);
    }
  }

  // Simulate execution of fmod: double fmod(double, double);
  // TODO(simulator): Parse signatures in entrypoints/quick/quick_entrypoints_list.h
  void SimulateFmod() {
    double x = dreg(0);
    double y = dreg(1);
    double z = fmod(x, y);
    set_dreg(0, z);
    set_pc(pc_->NextInstruction());
  }

  // Simulate execution of fmodf: float fmodf(float, float);
  // TODO(simulator): Parse signatures in entrypoints/quick/quick_entrypoints_list.h
  void SimulateFmodf() {
    float x = sreg(0);
    float y = sreg(1);
    float z = fmodf(x, y);
    set_sreg(0, z);
    set_pc(pc_->NextInstruction());
  }

  // TODO(simulator): Maybe integrate these into vixl?
  int64_t get_sp() const {
    return reg<int64_t>(kSp, vixl::Reg31IsStackPointer);
  }

  int64_t get_lr() const {
    return reg<int64_t>(vixl::kLinkRegCode);
  }

  int64_t get_fp() const {
    return xreg(kFp);
  }
};

static const void* GetQuickCodeFromArtMethod(ArtMethod* method)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  DCHECK(!method->IsAbstract());
  ClassLinker* linker = Runtime::Current()->GetClassLinker();
  bool found;
  OatFile::OatMethod oat_method = linker->FindOatMethodFor(method, &found);
  DCHECK(found) << "Failed to find quick code for art method: " << method->GetName();
  return oat_method.GetQuickCode();
}

// VIXL has not been tested on 32bit architectures, so vixl::Simulator is not always
// available. To avoid linker error on these architectures, we check if we can simulate
// in the beginning of following methods, with compile time constant `kCanSimulate`.
// TODO: when vixl::Simulator is always available, remove the these checks.

CodeSimulatorArm64* CodeSimulatorArm64::CreateCodeSimulatorArm64() {
  if (kCanSimulate) {
    return new CodeSimulatorArm64();
  } else {
    return nullptr;
  }
}

CodeSimulatorArm64::CodeSimulatorArm64()
    : CodeSimulator(), decoder_(nullptr), simulator_(nullptr) {
  DCHECK(kCanSimulate);
  decoder_ = new vixl::Decoder();
  simulator_ = new CustomSimulator(decoder_);
}

CodeSimulatorArm64::~CodeSimulatorArm64() {
  DCHECK(kCanSimulate);
  delete simulator_;
  delete decoder_;
}

void CodeSimulatorArm64::RunFrom(intptr_t code_buffer) {
  DCHECK(kCanSimulate);
  if (VLOG_IS_ON(simulator)) {
    simulator_->set_trace_parameters(vixl::LOG_ALL);
  }
  simulator_->RunFrom(reinterpret_cast<const vixl::Instruction*>(code_buffer));
}

bool CodeSimulatorArm64::GetCReturnBool() const {
  DCHECK(kCanSimulate);
  return simulator_->wreg(0);
}

int32_t CodeSimulatorArm64::GetCReturnInt32() const {
  DCHECK(kCanSimulate);
  return simulator_->wreg(0);
}

int64_t CodeSimulatorArm64::GetCReturnInt64() const {
  DCHECK(kCanSimulate);
  return simulator_->xreg(0);
}

void CodeSimulatorArm64::Invoke(ArtMethod* method, uint32_t* args, uint32_t args_size_in_bytes,
                                Thread* self, JValue* result, const char* shorty, bool isStatic)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  DCHECK(kCanSimulate);
  // ARM64 simulator only supports 64-bit host machines. Because:
  //   1) vixl simulator is not tested on 32-bit host machines.
  //   2) Data structures in ART have different representations for 32/64-bit machines.
  DCHECK(sizeof(args) == sizeof(int64_t));

  InitRegistersForInvokeStub(method, args, args_size_in_bytes, self, result, shorty, isStatic);

  int64_t quick_code = reinterpret_cast<int64_t>(GetQuickCodeFromArtMethod(method));
  RunFrom(quick_code);

  GetResultFromShorty(result, shorty);

  // Ensure simulation state is not carried over from one method to another.
  simulator_->ResetState();
}

void CodeSimulatorArm64::GetResultFromShorty(JValue* result, const char* shorty) {
  switch (shorty[0]) {
    case 'V':
      return;
    case 'D':
      *reinterpret_cast<double*>(result) = simulator_->dreg(0);
      return;
    case 'F':
      *reinterpret_cast<float*>(result) = simulator_->sreg(0);
      return;
    default:
      // Just store x0. Doesn't matter if it is 64 or 32 bits.
      *reinterpret_cast<int64_t*>(result) = simulator_->xreg(0);
      return;
  }
}

// Init registers for invoking art_quick_invoke_stub:
//
//  extern"C" void art_quick_invoke_stub(ArtMethod *method,   x0
//                                       uint32_t  *args,     x1
//                                       uint32_t argsize,    w2
//                                       Thread *self,        x3
//                                       JValue *result,      x4
//                                       char   *shorty);     x5
//
// See art/runtime/arch/arm64/quick_entrypoints_arm64.S
//
//  +----------------------+
//  |                      |
//  |  C/C++ frame         |
//  |       LR''           |
//  |       FP''           | <- SP'
//  +----------------------+
//  +----------------------+
//  |        X28           |
//  |        :             |
//  |        X19(*self)    |
//  |        SP'           |        Saved registers
//  |        X5(*shorty)   |
//  |        X4(*result)   |
//  |        LR'           |
//  |        FP'           | <- FP
//  +----------------------+
//  | uint32_t out[n-1]    |
//  |    :      :          |        Outs
//  | uint32_t out[0]      |
//  | ArtMethod*           | <- SP  value=null
//  +----------------------+
//
// Outgoing registers:
//  x0    - Method*
//  x1-x7 - integer parameters.
//  d0-d7 - Floating point parameters.
//  xSELF = self
//  SP = & of ArtMethod*
//  x1    - "this" pointer (for non-static method)
void CodeSimulatorArm64::InitRegistersForInvokeStub(ArtMethod* method, uint32_t* args,
                                                    uint32_t args_save_size_in_bytes, Thread* self,
                                                    JValue* result, const char* shorty,
                                                    bool isStatic)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  DCHECK(kCanSimulate);
  VLOG(simulator) << "Invoke InitRegistersForInvokeStub";

  // Set registers x0. Registers x1, w2, x3, and x4 will be over written, so skip them.
  simulator_->set_xreg(0, reinterpret_cast<int64_t>(method));

  // Stack Pointer here is not the real one in hardware. This will break stack overflow check.
  // Also note that the simulator stack is limited.
  const int64_t saved_sp = simulator_->get_sp();
  // x4, x5, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, SP, LR, FP saved (15 in total).
  const int64_t regs_save_size_in_bytes = vixl::kXRegSizeInBytes * 15;
  const int64_t frame_save_size = regs_save_size_in_bytes +
                                  vixl::kXRegSizeInBytes +  // Method*
                                  args_save_size_in_bytes;
  // Comply with 16-byte alignment requirement for SP.
  void** new_sp = reinterpret_cast<void**>((saved_sp - frame_save_size) & (~0xfUL));

  simulator_->set_sp(new_sp);

  // Store null into ArtMethod* at bottom of frame.
  *new_sp++ = nullptr;
  // Copy arguments into stack frame.
  std::memcpy(new_sp, args, args_save_size_in_bytes * sizeof(uint32_t));

  // TODO: Restore these callee-saved regs in ::Invoke?
  int64_t* save_registers = reinterpret_cast<int64_t*>(saved_sp) + 3;
  save_registers[0] = simulator_->get_fp();
  save_registers[1] = simulator_->get_lr();
  save_registers[2] = reinterpret_cast<int64_t>(result);
  save_registers[3] = reinterpret_cast<int64_t>(shorty);
  save_registers[4] = saved_sp;
  save_registers[5] = reinterpret_cast<int64_t>(self);
  for (unsigned int i = 6; i < 15; i++) {
    save_registers[i] = simulator_->xreg(i + 13);
  }

  // Use xFP (Frame Pointer) now, as it's callee-saved.
  simulator_->set_xreg(kFp, saved_sp - regs_save_size_in_bytes);
  // Move thread pointer into SELF register.
  simulator_->set_xreg(kSelf, reinterpret_cast<int64_t>(self));

  // Fill registers.
  static const unsigned kRegisterIndexLimit = 8;
  unsigned fpr_index = 0;
  unsigned gpr_index = 1;
  shorty++;  // Skip the return value.
  // For non-static method, load "this" parameter, and increment args pointer.
  if (!isStatic) {
    simulator_->set_wreg(gpr_index++, *args++);
  }
  // Loop to fill other registers.
  for (const char* s = shorty; *s != '\0'; s++) {
    switch (*s) {
      case 'D':
        if (fpr_index < kRegisterIndexLimit) {
          simulator_->set_dreg(fpr_index++, *reinterpret_cast<double*>(args));
          args += 2;
        } else {
          // TODO: Handle register spill.
          UNREACHABLE();
        }
        break;
      case 'J':
        if (gpr_index < kRegisterIndexLimit) {
          simulator_->set_xreg(gpr_index++, *reinterpret_cast<int64_t*>(args));
          args += 2;
        } else {
          // TODO: Handle register spill.
          UNREACHABLE();
        }
        break;
      case 'F':
        if (fpr_index < kRegisterIndexLimit) {
          simulator_->set_sreg(fpr_index++, *reinterpret_cast<float*>(args));
          args++;
        } else {
          // TODO: Handle register spill.
          UNREACHABLE();
        }
        break;
      default:
        // Everything else takes one vReg.
        if (gpr_index < kRegisterIndexLimit) {
          simulator_->set_wreg(gpr_index++, *reinterpret_cast<int32_t*>(args));
          args++;
        } else {
          UNREACHABLE();
        }
        break;
    }
  }
}

void CodeSimulatorArm64::InitEntryPoints(QuickEntryPoints* qpoints) {
  // Currently, only two entry points are initialized.
  // TODO: initialize all entry points.
  qpoints->pFmod = fmod;
  qpoints->pFmodf = fmodf;
}

}  // namespace arm64
}  // namespace art
