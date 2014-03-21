/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "calling_convention_x86_64.h"

#include "base/logging.h"
#include "utils/x86_64/managed_register_x86_64.h"
#include "utils.h"

namespace art {
namespace x86_64 {

// Calling convention

ManagedRegister X86_64ManagedRuntimeCallingConvention::InterproceduralScratchRegister() {
  return X86_64ManagedRegister::FromCpuRegister(RCX);
}

ManagedRegister X86_64JniCallingConvention::InterproceduralScratchRegister() {
  return X86_64ManagedRegister::FromCpuRegister(RCX);
}

ManagedRegister X86_64JniCallingConvention::ReturnScratchRegister() const {
  return ManagedRegister::NoRegister();  // No free regs, so assembler uses push/pop
}

static ManagedRegister ReturnRegisterForShorty(const char* shorty, bool jni) {
  if (shorty[0] == 'F' || shorty[0] == 'D') {
    if (jni) {
      return X86_64ManagedRegister::FromX87Register(ST0);
    } else {
      return X86_64ManagedRegister::FromXmmRegister(_XMM0);
    }
  } else if (shorty[0] == 'J') {
    return X86_64ManagedRegister::FromRegisterPair(RAX_RDX);
  } else if (shorty[0] == 'V') {
    return ManagedRegister::NoRegister();
  } else {
    return X86_64ManagedRegister::FromCpuRegister(RAX);
  }
}

ManagedRegister X86_64ManagedRuntimeCallingConvention::ReturnRegister() {
  return ReturnRegisterForShorty(GetShorty(), false);
}

ManagedRegister X86_64JniCallingConvention::ReturnRegister() {
  return ReturnRegisterForShorty(GetShorty(), true);
}

ManagedRegister X86_64JniCallingConvention::IntReturnRegister() {
  return X86_64ManagedRegister::FromCpuRegister(RAX);
}

// Managed runtime calling convention

ManagedRegister X86_64ManagedRuntimeCallingConvention::MethodRegister() {
  return X86_64ManagedRegister::FromCpuRegister(RAX);
}

bool X86_64ManagedRuntimeCallingConvention::IsCurrentParamInRegister() {
  return false;  // Everything is passed by stack
}

bool X86_64ManagedRuntimeCallingConvention::IsCurrentParamOnStack() {
  return true;  // Everything is passed by stack
}

ManagedRegister X86_64ManagedRuntimeCallingConvention::CurrentParamRegister() {
  LOG(FATAL) << "Should not reach here";
  return ManagedRegister::NoRegister();
}

FrameOffset X86_64ManagedRuntimeCallingConvention::CurrentParamStackOffset() {
  return FrameOffset(displacement_.Int32Value() +   // displacement
                     kPointerSize +                 // Method*
                     (itr_slots_ * kPointerSize));  // offset into in args
}

const std::vector<ManagedRegister>& X86_64ManagedRuntimeCallingConvention::EntrySpills() {
  // We spill the argument registers on X86 to free them up for scratch use, we then assume
  // all arguments are on the stack.
  if (entry_spills_.size() == 0) {
    size_t num_spills = NumArgs() + NumLongOrDoubleArgs();
    if (num_spills > 0) {
      entry_spills_.push_back(X86_64ManagedRegister::FromCpuRegister(RCX));
      if (num_spills > 1) {
        entry_spills_.push_back(X86_64ManagedRegister::FromCpuRegister(RDX));
        if (num_spills > 2) {
          entry_spills_.push_back(X86_64ManagedRegister::FromCpuRegister(RBX));
        }
      }
    }
  }
  return entry_spills_;
}

// JNI calling convention

X86_64JniCallingConvention::X86_64JniCallingConvention(bool is_static, bool is_synchronized,
                                                 const char* shorty)
    : JniCallingConvention(is_static, is_synchronized, shorty) {
  callee_save_regs_.push_back(X86_64ManagedRegister::FromCpuRegister(RBP));
  callee_save_regs_.push_back(X86_64ManagedRegister::FromCpuRegister(RSI));
  callee_save_regs_.push_back(X86_64ManagedRegister::FromCpuRegister(RDI));
}

uint32_t X86_64JniCallingConvention::CoreSpillMask() const {
  return 1 << RBP | 1 << RSI | 1 << RDI | 1 << kNumberOfCpuRegisters;
}

size_t X86_64JniCallingConvention::FrameSize() {
  // Method*, return address and callee save area size, local reference segment state
  size_t frame_data_size = (3 + CalleeSaveRegisters().size()) * kPointerSize;
  // References plus 2 words for SIRT header
  size_t sirt_size = (ReferenceCount() + 2) * kPointerSize;
  // Plus return value spill area size
  return RoundUp(frame_data_size + sirt_size + SizeOfReturnValue(), kStackAlignment);
}

size_t X86_64JniCallingConvention::OutArgSize() {
  return RoundUp(NumberOfOutgoingStackArgs() * kPointerSize, kStackAlignment);
}

bool X86_64JniCallingConvention::IsCurrentParamInRegister() {
  return false;  // Everything is passed by stack.
}

bool X86_64JniCallingConvention::IsCurrentParamOnStack() {
  return true;  // Everything is passed by stack.
}

ManagedRegister X86_64JniCallingConvention::CurrentParamRegister() {
  LOG(FATAL) << "Should not reach here";
  return ManagedRegister::NoRegister();
}

FrameOffset X86_64JniCallingConvention::CurrentParamStackOffset() {
  return FrameOffset(displacement_.Int32Value() - OutArgSize() + (itr_slots_ * kPointerSize));
}

size_t X86_64JniCallingConvention::NumberOfOutgoingStackArgs() {
  size_t static_args = IsStatic() ? 1 : 0;  // count jclass
  // regular argument parameters and this
  size_t param_args = NumArgs() + NumLongOrDoubleArgs();
  // count JNIEnv* and return pc (pushed after Method*)
  size_t total_args = static_args + param_args + 2;
  return total_args;
}

}  // namespace x86_64
}  // namespace art
