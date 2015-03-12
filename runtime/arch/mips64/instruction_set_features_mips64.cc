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
    const std::string& variant, std::string* error_msg ATTRIBUTE_UNUSED) {
  if ((variant != "mips64r6") && (variant != "default")) {
    LOG(WARNING) << "Unexpected CPU variant for Mips64 using defaults: " << variant;
  }
  bool smp = true;  // Conservative default.
  bool r6 = true;
  return new Mips64InstructionSetFeatures(smp, r6);
}

const Mips64InstructionSetFeatures* Mips64InstructionSetFeatures::FromBitmap(uint32_t bitmap) {
  bool smp = (bitmap & kSmpBitfield) != 0;
  bool r6 = (bitmap & kR6Bitfield) != 0;
  return new Mips64InstructionSetFeatures(smp, r6);
}

const Mips64InstructionSetFeatures* Mips64InstructionSetFeatures::FromCppDefines() {
  const bool smp = true;
  bool r6 = true;

  return new Mips64InstructionSetFeatures(smp, r6);
}

const Mips64InstructionSetFeatures* Mips64InstructionSetFeatures::FromCpuInfo() {
  // Look in /proc/cpuinfo for features we need.  Only use this when we can guarantee that
  // the kernel puts the appropriate feature flags in here.  Sometimes it doesn't.
  bool smp = false;
  bool r6 = true;

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
  return new Mips64InstructionSetFeatures(smp, r6);
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
      (r6_ == other_as_mips64->r6_);
}

uint32_t Mips64InstructionSetFeatures::AsBitmap() const {
  return (IsSmp() ? kSmpBitfield : 0) |
      (r6_ ? kR6Bitfield : 0);
}

std::string Mips64InstructionSetFeatures::GetFeatureString() const {
  std::string result;
  if (IsSmp()) {
    result += "smp";
  } else {
    result += "-smp";
  }
  if (r6_) {
    result += ",r6";
  }  // Suppress non-r6.
  return result;
}

const InstructionSetFeatures* Mips64InstructionSetFeatures::AddFeaturesFromSplitString(
    const bool smp, const std::vector<std::string>& features, std::string* error_msg) const {
  bool r6 = r6_;
  for (auto i = features.begin(); i != features.end(); i++) {
    std::string feature = Trim(*i);
    if (feature == "r6") {
      r6 = true;
    } else if (feature == "-r6") {
      r6 = false;
    } else {
      *error_msg = StringPrintf("Unknown instruction set feature: '%s'", feature.c_str());
      return nullptr;
    }
  }
  return new Mips64InstructionSetFeatures(smp, r6);
}

}  // namespace art
