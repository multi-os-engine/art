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

#include <stdint.h>

#include "context_arm64.h"

#include "mirror/art_method.h"
#include "mirror/object-inl.h"
#include "stack.h"
#include "thread.h"


namespace art {
namespace arm64 {

static const uint64_t gZero = UINT64_C(0L);

void Arm64Context::Reset() {
  for (size_t i = 0; i < kNumberOfCoreRegisters; i++) {
    gprs_[i] = NULL;
  }
  for (size_t i = 0; i < kNumberOfDRegisters; i++) {
    fprs_[i] = NULL;
  }
  gprs_[SP] = &sp_;
  gprs_[PC] = &pc_;
  // Initialize registers with easy to spot debug values.
  sp_ = Arm64Context::kBadGprBase + SP;
  pc_ = Arm64Context::kBadGprBase + PC;
}

void Arm64Context::FillCalleeSaves(const StackVisitor& fr) {
  mirror::ArtMethod* method = fr.GetMethod();
  uint32_t core_spills = method->GetCoreSpillMask();
  uint32_t fp_core_spills = method->GetFpSpillMask();
  size_t spill_count = __builtin_popcount(core_spills);
  size_t fp_spill_count = __builtin_popcount(fp_core_spills);
  size_t frame_size = method->GetFrameSizeInBytes();
  if (spill_count > 0) {
    // Lowest number spill is farthest away, walk registers and fill into context
    int j = 1;
    for (size_t i = 0; i < kNumberOfCoreRegisters; i++) {
      if (((core_spills >> i) & 1) != 0) {
        gprs_[i] = fr.CalleeSaveAddress(spill_count - j, frame_size);
        j++;
      }
    }
  }
  if (fp_spill_count > 0) {
    // Lowest number spill is farthest away, walk registers and fill into context
    int j = 1;
    for (size_t i = 0; i < kNumberOfDRegisters; i++) {
      if (((fp_core_spills >> i) & 1) != 0) {
        fprs_[i] = fr.CalleeSaveAddress(spill_count + fp_spill_count - j, frame_size);
        j++;
      }
    }
  }
}

void Arm64Context::SetGPR(uint32_t reg, uintptr_t value) {
  DCHECK_LT(reg, static_cast<uint32_t>(kNumberOfCoreRegisters));
  DCHECK_NE(gprs_[reg], &gZero);  // Can't overwrite this static value since they are never reset.
  DCHECK(gprs_[reg] != NULL);
  *gprs_[reg] = value;
}

void Arm64Context::SmashCallerSaves() {
  // This needs to be 0 because we want a null/zero return value.
  gprs_[X0] = const_cast<uint64_t*>(&gZero);
  gprs_[X1] = NULL;
  gprs_[X2] = NULL;
  gprs_[X3] = NULL;
  gprs_[X4] = NULL;
  gprs_[X5] = NULL;
  gprs_[X6] = NULL;
  gprs_[X7] = NULL;
  gprs_[X8] = NULL;
  gprs_[X9] = NULL;
  gprs_[X10] = NULL;
  gprs_[X11] = NULL;
  gprs_[X12] = NULL;
  gprs_[X13] = NULL;
  gprs_[X14] = NULL;
  gprs_[X15] = NULL;
}

extern "C" void art_quick_do_long_jump(uint32_t*, uint32_t*);

void Arm64Context::DoLongJump() {
  uintptr_t gprs[33];
  uint64_t fprs[32];

  // TODO SRDM SP/XZR are confused, only SP needs to be loaded, and there are only 32 actual core registers
  for (size_t i = 0; i < kNumberOfCoreRegisters; ++i) {
    gprs[i] = gprs_[i] != NULL ? *gprs_[i] : Arm64Context::kBadGprBase + i;
  }
  for (size_t i = 0; i < kNumberOfDRegisters; ++i) {
    fprs[i] = fprs_[i] != NULL ? *fprs_[i] : Arm64Context::kBadGprBase + i;
  }
  DCHECK_EQ(reinterpret_cast<uintptr_t>(Thread::Current()), gprs[TR]);
  art_quick_do_long_jump(gprs, fprs);
}

}  // namespace arm64
}  // namespace art
