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

#ifndef ART_RUNTIME_ARCH_X86_64_INSTRUCTION_SET_FEATURES_X86_64_H_
#define ART_RUNTIME_ARCH_X86_64_INSTRUCTION_SET_FEATURES_X86_64_H_

#include "arch/x86/instruction_set_features_x86.h"

namespace art {

// Instruction set features relevant to the X86_64 architecture.
class X86_64InstructionSetFeatures FINAL : public X86InstructionSetFeatures {
  // Process a CPU variant string like "atom" or "nehalem" and create InstructionSetFeatures.
  static const X86_64InstructionSetFeatures* FromVariant(const std::string& variant,
                                                         std::string* error_msg);

  // Parse a bitmap and create an InstructionSetFeatures.
  static const X86_64InstructionSetFeatures* FromBitmap(uint32_t bitmap);

  // Turn C pre-processor #defines into the equivalent instruction set features.
  static const X86_64InstructionSetFeatures* FromCppDefines();

  // Process /proc/cpuinfo and use kRuntimeISA to produce InstructionSetFeatures.
  static const X86_64InstructionSetFeatures* FromCpuInfo();

  // Process the auxiliary vector AT_HWCAP entry and use kRuntimeISA to produce
  // InstructionSetFeatures.
  static const X86_64InstructionSetFeatures* FromHwcap();

  // Use assembly tests of the current runtime (ie kRuntimeISA) to determine the
  // InstructionSetFeatures. This works around kernel bugs in AT_HWCAP and /proc/cpuinfo.
  static const X86_64InstructionSetFeatures* FromAssembly();

  // Parse a string of the form "a53" adding these to a new InstructionSetFeatures.
  const InstructionSetFeatures* AddFeaturesFromString(const std::string& feature_list,
                                                      std::string* error_msg) const OVERRIDE;

  InstructionSet GetInstructionSet() const OVERRIDE {
    return kX86_64;
  }

  virtual ~X86_64InstructionSetFeatures() {}

 private:
  X86_64InstructionSetFeatures(bool smp, bool has_SSSE3, bool has_SSE4_1, bool has_SSE4_2,
                               bool has_AVX, bool has_AVX2)
      : X86InstructionSetFeatures(smp, has_SSSE3, has_SSE4_1, has_SSE4_2, has_AVX, has_AVX2) {
  }

  DISALLOW_COPY_AND_ASSIGN(X86_64InstructionSetFeatures);
};

}  // namespace art

#endif  // ART_RUNTIME_ARCH_X86_64_INSTRUCTION_SET_FEATURES_X86_64_H_
