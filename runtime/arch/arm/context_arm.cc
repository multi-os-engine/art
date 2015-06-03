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

#include "context_arm.h"

#include "art_method-inl.h"
#include "base/bit_utils.h"
#include "quick/quick_method_frame_info.h"

namespace art {
namespace arm {

static constexpr uint32_t gZero = 0;

void ArmContext::Reset() {
  std::fill_n(registers_, arraysize(registers_), nullptr);
  registers_[SP] = &sp_;
  registers_[PC] = &pc_;
  registers_[R0] = &arg0_;
  // Initialize registers with easy to spot debug values.
  sp_ = ArmContext::kBadGprBase + SP;
  pc_ = ArmContext::kBadGprBase + PC;
  arg0_ = 0;
}

void ArmContext::FillCalleeSaves(const StackVisitor& fr) {
  ArtMethod* method = fr.GetMethod();
  const QuickMethodFrameInfo frame_info = method->GetQuickFrameInfo();
  int spill_pos = 0;

  // Core registers come first, from the highest down to the lowest.
  uint32_t core_regs = frame_info.CoreSpillMask();
  DCHECK_EQ(0u, core_regs & (static_cast<uint32_t>(-1) << kNumberOfCoreRegisters));
  for (uint32_t core_reg : HighToLowBits(core_regs)) {
    LOG(INFO) << "Loading core reg " << core_reg;
    registers_[core_reg] = fr.CalleeSaveAddress(spill_pos, frame_info.FrameSizeInBytes());
    ++spill_pos;
  }
  DCHECK_EQ(spill_pos, POPCOUNT(frame_info.CoreSpillMask()));

  // FP registers come second, from the highest down to the lowest.
  for (uint32_t fp_reg : HighToLowBits(frame_info.FpSpillMask())) {
    registers_[kNumberOfCoreRegisters + fp_reg] =
        fr.CalleeSaveAddress(spill_pos, frame_info.FrameSizeInBytes());
    ++spill_pos;
  }
  DCHECK_EQ(spill_pos, POPCOUNT(frame_info.CoreSpillMask()) + POPCOUNT(frame_info.FpSpillMask()));
}

void ArmContext::SetGPR(uint32_t reg, uintptr_t value) {
  DCHECK_LT(reg, static_cast<uint32_t>(kNumberOfCoreRegisters));
  DCHECK(IsAccessibleGPR(reg));
  DCHECK_NE(registers_[reg], &gZero);  // Can't overwrite this static value since they are never
                                       // reset.
  *registers_[reg] = value;
}

void ArmContext::SetFPR(uint32_t reg, uintptr_t value) {
  DCHECK_LT(reg, static_cast<uint32_t>(kNumberOfSRegisters));
  DCHECK(IsAccessibleFPR(reg));
  DCHECK_NE(registers_[kNumberOfCoreRegisters + reg], &gZero);  // Can't overwrite this static value
                                                                // since they are never reset.
  *registers_[kNumberOfCoreRegisters + reg] = value;
}

void ArmContext::SmashCallerSaves() {
  // This needs to be 0 because we want a null/zero return value.
  registers_[R0] = const_cast<uint32_t*>(&gZero);
  registers_[R1] = const_cast<uint32_t*>(&gZero);
  registers_[R2] = nullptr;
  registers_[R3] = nullptr;

  registers_[kNumberOfCoreRegisters + S0] = nullptr;
  registers_[kNumberOfCoreRegisters + S1] = nullptr;
  registers_[kNumberOfCoreRegisters + S2] = nullptr;
  registers_[kNumberOfCoreRegisters + S3] = nullptr;
  registers_[kNumberOfCoreRegisters + S4] = nullptr;
  registers_[kNumberOfCoreRegisters + S5] = nullptr;
  registers_[kNumberOfCoreRegisters + S6] = nullptr;
  registers_[kNumberOfCoreRegisters + S7] = nullptr;
  registers_[kNumberOfCoreRegisters + S8] = nullptr;
  registers_[kNumberOfCoreRegisters + S9] = nullptr;
  registers_[kNumberOfCoreRegisters + S10] = nullptr;
  registers_[kNumberOfCoreRegisters + S11] = nullptr;
  registers_[kNumberOfCoreRegisters + S12] = nullptr;
  registers_[kNumberOfCoreRegisters + S13] = nullptr;
  registers_[kNumberOfCoreRegisters + S14] = nullptr;
  registers_[kNumberOfCoreRegisters + S15] = nullptr;
}

extern "C" NO_RETURN void art_quick_do_long_jump(uintptr_t, uintptr_t*);

void ArmContext::DoLongJump() {
  uintptr_t registers[kNumberOfCoreRegisters + kNumberOfSRegisters];
  for (size_t i = 0; i < kNumberOfCoreRegisters; ++i) {
    registers[i] = registers_[i] != nullptr ? *registers_[i] : ArmContext::kBadGprBase + i;
  }
  for (size_t i = 0; i < kNumberOfSRegisters; ++i) {
    registers[i + kNumberOfCoreRegisters] =
        registers_[kNumberOfCoreRegisters + i] != nullptr
            ? *registers_[kNumberOfCoreRegisters + i]
            : ArmContext::kBadFprBase + i;
  }
  DCHECK_EQ(reinterpret_cast<uintptr_t>(Thread::Current()), registers[TR]);
  art_quick_do_long_jump(arg0_, registers);
}

}  // namespace arm
}  // namespace art
