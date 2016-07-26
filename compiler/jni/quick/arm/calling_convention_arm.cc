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

#include "base/logging.h"
#include "calling_convention_arm.h"
#include "handle_scope-inl.h"
#include "utils/arm/managed_register_arm.h"

namespace art {
namespace arm {

// Used by hard float.
static const Register kHFCoreArgumentRegisters[] = {
  R0, R1, R2, R3
};

static const SRegister kHFSArgumentRegisters[] = {
  S0, S1, S2, S3, S4, S5, S6, S7, S8, S9, S10, S11, S12, S13, S14, S15
};

static const DRegister kHFDArgumentRegisters[] = {
  D0, D1, D2, D3, D4, D5, D6, D7
};

static_assert(arraysize(kHFDArgumentRegisters) * 2 == arraysize(kHFSArgumentRegisters),
    "ks d argument registers mismatch");

static constexpr ManagedRegister kCalleeSaveRegisters[] = {
    // Core registers.
    ArmManagedRegister::FromCoreRegister(R5),
    ArmManagedRegister::FromCoreRegister(R6),
    ArmManagedRegister::FromCoreRegister(R7),
    ArmManagedRegister::FromCoreRegister(R8),
    ArmManagedRegister::FromCoreRegister(R10),
    ArmManagedRegister::FromCoreRegister(R11),
    // Hard float registers.
    ArmManagedRegister::FromSRegister(S16),
    ArmManagedRegister::FromSRegister(S17),
    ArmManagedRegister::FromSRegister(S18),
    ArmManagedRegister::FromSRegister(S19),
    ArmManagedRegister::FromSRegister(S20),
    ArmManagedRegister::FromSRegister(S21),
    ArmManagedRegister::FromSRegister(S22),
    ArmManagedRegister::FromSRegister(S23),
    ArmManagedRegister::FromSRegister(S24),
    ArmManagedRegister::FromSRegister(S25),
    ArmManagedRegister::FromSRegister(S26),
    ArmManagedRegister::FromSRegister(S27),
    ArmManagedRegister::FromSRegister(S28),
    ArmManagedRegister::FromSRegister(S29),
    ArmManagedRegister::FromSRegister(S30),
    ArmManagedRegister::FromSRegister(S31)
};

static constexpr uint32_t CalculateCoreCalleeSpillMask() {
  // LR is a special callee save which is not reported by CalleeSaveRegisters().
  uint32_t result = 1 << LR;
  for (auto&& r : kCalleeSaveRegisters) {
    if (r.AsArm().IsCoreRegister()) {
      result |= (1 << r.AsArm().AsCoreRegister());
    }
  }
  return result;
}

static constexpr uint32_t CalculateFpCalleeSpillMask() {
  uint32_t result = 0;
  for (auto&& r : kCalleeSaveRegisters) {
    if (r.AsArm().IsSRegister()) {
      result |= (1 << r.AsArm().AsSRegister());
    }
  }
  return result;
}

static constexpr uint32_t kCoreCalleeSpillMask = CalculateCoreCalleeSpillMask();
static constexpr uint32_t kFpCalleeSpillMask = CalculateFpCalleeSpillMask();

// Calling convention

ManagedRegister ArmManagedRuntimeCallingConvention::InterproceduralScratchRegister() {
  return ArmManagedRegister::FromCoreRegister(IP);  // R12
}

ManagedRegister ArmJniCallingConvention::InterproceduralScratchRegister() {
  return ArmManagedRegister::FromCoreRegister(IP);  // R12
}

ManagedRegister ArmManagedRuntimeCallingConvention::ReturnRegister() {
  if (kArm32QuickCodeUseSoftFloat) {
    switch (GetShorty()[0]) {
    case 'V':
      return ArmManagedRegister::NoRegister();
    case 'D':
    case 'J':
      return ArmManagedRegister::FromRegisterPair(R0_R1);
    default:
      return ArmManagedRegister::FromCoreRegister(R0);
    }
  } else {
    switch (GetShorty()[0]) {
    case 'V':
      return ArmManagedRegister::NoRegister();
    case 'D':
      return ArmManagedRegister::FromDRegister(D0);
    case 'F':
      return ArmManagedRegister::FromSRegister(S0);
    case 'J':
      return ArmManagedRegister::FromRegisterPair(R0_R1);
    default:
      return ArmManagedRegister::FromCoreRegister(R0);
    }
  }
}

ManagedRegister ArmJniCallingConvention::ReturnRegister() {
  switch (GetShorty()[0]) {
  case 'V':
    return ArmManagedRegister::NoRegister();
  case 'D':
  case 'J':
    return ArmManagedRegister::FromRegisterPair(R0_R1);
  default:
    return ArmManagedRegister::FromCoreRegister(R0);
  }
}

ManagedRegister ArmJniCallingConvention::IntReturnRegister() {
  return ArmManagedRegister::FromCoreRegister(R0);
}

// Managed runtime calling convention

ManagedRegister ArmManagedRuntimeCallingConvention::MethodRegister() {
  return ArmManagedRegister::FromCoreRegister(R0);
}

bool ArmManagedRuntimeCallingConvention::IsCurrentParamInRegister() {
  return false;  // Everything moved to stack on entry.
}

bool ArmManagedRuntimeCallingConvention::IsCurrentParamOnStack() {
  return true;
}

ManagedRegister ArmManagedRuntimeCallingConvention::CurrentParamRegister() {
  LOG(FATAL) << "Should not reach here";
  return ManagedRegister::NoRegister();
}

FrameOffset ArmManagedRuntimeCallingConvention::CurrentParamStackOffset() {
  CHECK(IsCurrentParamOnStack());
  FrameOffset result =
      FrameOffset(displacement_.Int32Value() +        // displacement
                  kFramePointerSize +                 // Method*
                  (itr_slots_ * kFramePointerSize));  // offset into in args
  return result;
}

const ManagedRegisterEntrySpills& ArmManagedRuntimeCallingConvention::EntrySpills() {
  // We spill the argument registers on ARM to free them up for scratch use, we then assume
  // all arguments are on the stack.
  if (kArm32QuickCodeUseSoftFloat) {
    if (entry_spills_.size() == 0) {
      size_t num_spills = NumArgs() + NumLongOrDoubleArgs();
      if (num_spills > 0) {
        entry_spills_.push_back(ArmManagedRegister::FromCoreRegister(R1));
        if (num_spills > 1) {
          entry_spills_.push_back(ArmManagedRegister::FromCoreRegister(R2));
          if (num_spills > 2) {
            entry_spills_.push_back(ArmManagedRegister::FromCoreRegister(R3));
          }
        }
      }
    }
  } else {
    if ((entry_spills_.size() == 0) && (NumArgs() > 0)) {
      uint32_t gpr_index = 1;  // R0 ~ R3. Reserve r0 for ArtMethod*.
      uint32_t fpr_index = 0;  // S0 ~ S15.
      uint32_t fpr_double_index = 0;  // D0 ~ D7.

      ResetIterator(FrameOffset(0));
      while (HasNext()) {
        if (IsCurrentParamAFloatOrDouble()) {
          if (IsCurrentParamADouble()) {  // Double.
            // Double should not overlap with float.
            fpr_double_index = (std::max(fpr_double_index * 2, RoundUp(fpr_index, 2))) / 2;
            if (fpr_double_index < arraysize(kHFDArgumentRegisters)) {
              entry_spills_.push_back(
                  ArmManagedRegister::FromDRegister(kHFDArgumentRegisters[fpr_double_index++]));
            } else {
              entry_spills_.push_back(ManagedRegister::NoRegister(), 8);
            }
          } else {  // Float.
            // Float should not overlap with double.
            if (fpr_index % 2 == 0) {
              fpr_index = std::max(fpr_double_index * 2, fpr_index);
            }
            if (fpr_index < arraysize(kHFSArgumentRegisters)) {
              entry_spills_.push_back(
                  ArmManagedRegister::FromSRegister(kHFSArgumentRegisters[fpr_index++]));
            } else {
              entry_spills_.push_back(ManagedRegister::NoRegister(), 4);
            }
          }
        } else {
          // FIXME: Pointer this returns as both reference and long.
          if (IsCurrentParamALong() && !IsCurrentParamAReference()) {  // Long.
            if (gpr_index < arraysize(kHFCoreArgumentRegisters) - 1) {
              // Skip R1, and use R2_R3 if the long is the first parameter.
              if (gpr_index == 1) {
                gpr_index++;
              }
            }

            // If it spans register and memory, we must use the value in memory.
            if (gpr_index < arraysize(kHFCoreArgumentRegisters) - 1) {
              entry_spills_.push_back(
                  ArmManagedRegister::FromCoreRegister(kHFCoreArgumentRegisters[gpr_index++]));
            } else if (gpr_index == arraysize(kHFCoreArgumentRegisters) - 1) {
              gpr_index++;
              entry_spills_.push_back(ManagedRegister::NoRegister(), 4);
            } else {
              entry_spills_.push_back(ManagedRegister::NoRegister(), 4);
            }
          }
          // High part of long or 32-bit argument.
          if (gpr_index < arraysize(kHFCoreArgumentRegisters)) {
            entry_spills_.push_back(
                ArmManagedRegister::FromCoreRegister(kHFCoreArgumentRegisters[gpr_index++]));
          } else {
            entry_spills_.push_back(ManagedRegister::NoRegister(), 4);
          }
        }
        Next();
      }
    }
  }
  return entry_spills_;
}
// JNI calling convention

ArmJniCallingConvention::ArmJniCallingConvention(bool is_static, bool is_synchronized,
                                                 const char* shorty)
    : JniCallingConvention(is_static, is_synchronized, shorty, PointerSize::kPointerSize32) {
  // Compute padding to ensure longs and doubles are not split in AAPCS. Ignore the 'this' jobject
  // or jclass for static methods and the JNIEnv. We start at the aligned register r2.
  size_t padding = 0;
  for (size_t cur_arg = IsStatic() ? 0 : 1, cur_reg = 2; cur_arg < NumArgs(); cur_arg++) {
    if (IsParamALongOrDouble(cur_arg)) {
      if ((cur_reg & 1) != 0) {
        padding += 4;
        cur_reg++;  // additional bump to ensure alignment
      }
      cur_reg++;  // additional bump to skip extra long word
    }
    cur_reg++;  // bump the iterator for every argument
  }
  padding_ = padding;
}

uint32_t ArmJniCallingConvention::CoreSpillMask() const {
  // Compute spill mask to agree with callee saves initialized in the constructor
  return kCoreCalleeSpillMask;
}

uint32_t ArmJniCallingConvention::FpSpillMask() const {
  return kFpCalleeSpillMask;
}

ManagedRegister ArmJniCallingConvention::ReturnScratchRegister() const {
  return ArmManagedRegister::FromCoreRegister(R2);
}

size_t ArmJniCallingConvention::FrameSize() {
  // Method*, LR and callee save area size, local reference segment state
  size_t frame_data_size = static_cast<size_t>(kArmPointerSize)
      + (2 + CalleeSaveRegisters().size()) * kFramePointerSize;
  // References plus 2 words for HandleScope header
  size_t handle_scope_size = HandleScope::SizeOf(kArmPointerSize, ReferenceCount());
  // Plus return value spill area size
  return RoundUp(frame_data_size + handle_scope_size + SizeOfReturnValue(), kStackAlignment);
}

size_t ArmJniCallingConvention::OutArgSize() {
  return RoundUp(NumberOfOutgoingStackArgs() * kFramePointerSize + padding_,
                 kStackAlignment);
}

ArrayRef<const ManagedRegister> ArmJniCallingConvention::CalleeSaveRegisters() const {
  return ArrayRef<const ManagedRegister>(kCalleeSaveRegisters);
}

// JniCallingConvention ABI follows AAPCS where longs and doubles must occur
// in even register numbers and stack slots
void ArmJniCallingConvention::Next() {
  JniCallingConvention::Next();
  size_t arg_pos = itr_args_ - NumberOfExtraArgumentsForJni();
  if ((itr_args_ >= 2) &&
      (arg_pos < NumArgs()) &&
      IsParamALongOrDouble(arg_pos)) {
    // itr_slots_ needs to be an even number, according to AAPCS.
    if ((itr_slots_ & 0x1u) != 0) {
      itr_slots_++;
    }
  }
}

bool ArmJniCallingConvention::IsCurrentParamInRegister() {
  return itr_slots_ < 4;
}

bool ArmJniCallingConvention::IsCurrentParamOnStack() {
  return !IsCurrentParamInRegister();
}

static const Register kJniArgumentRegisters[] = {
  R0, R1, R2, R3
};
ManagedRegister ArmJniCallingConvention::CurrentParamRegister() {
  CHECK_LT(itr_slots_, 4u);
  int arg_pos = itr_args_ - NumberOfExtraArgumentsForJni();
  if ((itr_args_ >= 2) && IsParamALongOrDouble(arg_pos)) {
    CHECK_EQ(itr_slots_, 2u);
    return ArmManagedRegister::FromRegisterPair(R2_R3);
  } else {
    return
      ArmManagedRegister::FromCoreRegister(kJniArgumentRegisters[itr_slots_]);
  }
}

FrameOffset ArmJniCallingConvention::CurrentParamStackOffset() {
  CHECK_GE(itr_slots_, 4u);
  size_t offset = displacement_.Int32Value() - OutArgSize() + ((itr_slots_ - 4) * kFramePointerSize);
  CHECK_LT(offset, OutArgSize());
  return FrameOffset(offset);
}

size_t ArmJniCallingConvention::NumberOfOutgoingStackArgs() {
  size_t static_args = IsStatic() ? 1 : 0;  // count jclass
  // regular argument parameters and this
  size_t param_args = NumArgs() + NumLongOrDoubleArgs();
  // count JNIEnv* less arguments in registers
  return static_args + param_args + 1 - 4;
}

}  // namespace arm
}  // namespace art
