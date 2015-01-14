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

#include "instruction_set_features_mips64.h"

#include <fstream>
#include <sstream>

#include "base/stringprintf.h"
#include "utils.h"  // For Trim.

namespace art {

const Mips64InstructionSetFeatures* Mips64InstructionSetFeatures::FromVariant(
    const std::string& variant ATTRIBUTE_UNUSED, std::string* error_msg ATTRIBUTE_UNUSED) {
  // TODO: r6 variant.
  if (variant != "default") {
    std::ostringstream os;
    LOG(WARNING) << "Unexpected CPU variant for Mips64 using defaults: " << variant;
  }
  bool smp = true;  // Conservative default.
  bool fpu_32bit = false;
  return new Mips64InstructionSetFeatures(smp, fpu_32bit);
}

const Mips64InstructionSetFeatures* Mips64InstructionSetFeatures::FromBitmap(uint32_t bitmap) {
  bool smp = (bitmap & kSmpBitfield) != 0;
  bool fpu_32bit = (bitmap & kFpu32Bitfield) != 0;
  return new Mips64InstructionSetFeatures(smp, fpu_32bit);
}

const Mips64InstructionSetFeatures* Mips64InstructionSetFeatures::FromCppDefines() {
  const bool smp = true;

  // TODO: here we assume the FPU is always 64-bit.
  const bool fpu_32bit = false;

  return new Mips64InstructionSetFeatures(smp, fpu_32bit);
}

const Mips64InstructionSetFeatures* Mips64InstructionSetFeatures::FromCpuInfo() {
  // Look in /proc/cpuinfo for features we need.  Only use this when we can guarantee that
  // the kernel puts the appropriate feature flags in here.  Sometimes it doesn't.
  bool smp = false;

  // TODO: here we assume the FPU is always 64-bit.
  const bool fpu_32bit = true;

  std::ifstream in("/proc/cpuinfo");
  if (!in.fail()) {
    while (!in.eof()) {
      std::string line;
      std::getline(in, line);
      if (!in.eof()) {
        LOG(INFO) << "cpuinfo line: " << line;
        if (line.find("processor") != std::string::npos && line.find(": 1") != std::string::npos) {
          smp = true;
        }
      }
    }
    in.close();
  } else {
    LOG(ERROR) << "Failed to open /proc/cpuinfo";
  }
  return new Mips64InstructionSetFeatures(smp, fpu_32bit);
}

const Mips64InstructionSetFeatures* Mips64InstructionSetFeatures::FromHwcap() {
  UNIMPLEMENTED(WARNING);
  return FromCppDefines();
}

const Mips64InstructionSetFeatures* Mips64InstructionSetFeatures::FromAssembly() {
  UNIMPLEMENTED(WARNING);
  return FromCppDefines();
}

bool Mips64InstructionSetFeatures::Equals(const InstructionSetFeatures* other) const {
  if (kMips64 != other->GetInstructionSet()) {
    return false;
  }
  const Mips64InstructionSetFeatures* other_as_mips64 = other->AsMips64InstructionSetFeatures();
  return (IsSmp() == other->IsSmp()) &&
      (fpu_32bit_ == other_as_mips64->fpu_32bit_);
}

uint32_t Mips64InstructionSetFeatures::AsBitmap() const {
  return (IsSmp() ? kSmpBitfield : 0) |
      (fpu_32bit_ ? kFpu32Bitfield : 0);
}

std::string Mips64InstructionSetFeatures::GetFeatureString() const {
  std::string result;
  if (IsSmp()) {
    result += "smp";
  } else {
    result += "-smp";
  }
  if (fpu_32bit_) {
    result += ",fpu32";
  } else {
    result += ",-fpu32";
  }
  return result;
}

const InstructionSetFeatures* Mips64InstructionSetFeatures::AddFeaturesFromSplitString(
    const bool smp, const std::vector<std::string>& features, std::string* error_msg) const {
  bool fpu_32bit = fpu_32bit_;
  for (auto i = features.begin(); i != features.end(); i++) {
    std::string feature = Trim(*i);
    if (feature == "fpu32") {
      fpu_32bit = true;
    } else if (feature == "-fpu32") {
      fpu_32bit = false;
    } else {
      *error_msg = StringPrintf("Unknown instruction set feature: '%s'", feature.c_str());
      return nullptr;
    }
  }
  return new Mips64InstructionSetFeatures(smp, fpu_32bit);
}

}  // namespace art
