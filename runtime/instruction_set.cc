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

#include "instruction_set.h"

#include <fstream>

#include "base/casts.h"
#include "base/stringprintf.h"
#include "utils.h"

namespace art {

const char* GetInstructionSetString(const InstructionSet isa) {
  switch (isa) {
    case kArm:
    case kThumb2:
      return "arm";
    case kArm64:
      return "arm64";
    case kX86:
      return "x86";
    case kX86_64:
      return "x86_64";
    case kMips:
      return "mips";
    case kNone:
      return "none";
    default:
      LOG(FATAL) << "Unknown ISA " << isa;
      return nullptr;
  }
}

InstructionSet GetInstructionSetFromString(const char* isa_str) {
  CHECK(isa_str != nullptr);

  if (strcmp("arm", isa_str) == 0) {
    return kArm;
  } else if (strcmp("arm64", isa_str) == 0) {
    return kArm64;
  } else if (strcmp("x86", isa_str) == 0) {
    return kX86;
  } else if (strcmp("x86_64", isa_str) == 0) {
    return kX86_64;
  } else if (strcmp("mips", isa_str) == 0) {
    return kMips;
  }

  return kNone;
}

size_t GetInstructionSetAlignment(InstructionSet isa) {
  switch (isa) {
    case kArm:
      // Fall-through.
    case kThumb2:
      return kArmAlignment;
    case kArm64:
      return kArm64Alignment;
    case kX86:
      // Fall-through.
    case kX86_64:
      return kX86Alignment;
    case kMips:
      return kMipsAlignment;
    case kNone:
      LOG(FATAL) << "ISA kNone does not have alignment.";
      return 0;
    default:
      LOG(FATAL) << "Unknown ISA " << isa;
      return 0;
  }
}


static constexpr size_t kDefaultStackOverflowReservedBytes = 16 * KB;
static constexpr size_t kMipsStackOverflowReservedBytes = kDefaultStackOverflowReservedBytes;

static constexpr size_t kArmStackOverflowReservedBytes =    8 * KB;
static constexpr size_t kArm64StackOverflowReservedBytes =  8 * KB;
static constexpr size_t kX86StackOverflowReservedBytes =    8 * KB;
static constexpr size_t kX86_64StackOverflowReservedBytes = 8 * KB;

size_t GetStackOverflowReservedBytes(InstructionSet isa) {
  switch (isa) {
    case kArm:      // Intentional fall-through.
    case kThumb2:
      return kArmStackOverflowReservedBytes;

    case kArm64:
      return kArm64StackOverflowReservedBytes;

    case kMips:
      return kMipsStackOverflowReservedBytes;

    case kX86:
      return kX86StackOverflowReservedBytes;

    case kX86_64:
      return kX86_64StackOverflowReservedBytes;

    case kNone:
      LOG(FATAL) << "kNone has no stack overflow size";
      return 0;

    default:
      LOG(FATAL) << "Unknown instruction set" << isa;
      return 0;
  }
}

const InstructionSetFeatures* InstructionSetFeatures::FromFeatureString(InstructionSet isa,
                                                                        const std::string& feature_list,
                                                                        std::string* error_msg) {
  const InstructionSetFeatures* result;
  switch (isa) {
    case kArm:
    case kThumb2:
      result = ArmInstructionSetFeatures::FromFeatureString(feature_list, error_msg);
      break;
    default:
      result = UnknownInstructionSetFeatures::Unknown(isa);
      break;
  }
  // TODO: warn if feature_list doesn't agree with result's GetFeatureList().
  return result;
}

const InstructionSetFeatures* InstructionSetFeatures::FromBitmap(InstructionSet isa,
                                                                 uint32_t bitmap) {
  const InstructionSetFeatures* result;
  switch (isa) {
    case kArm:
    case kThumb2:
      result = ArmInstructionSetFeatures::FromBitmap(bitmap);
      break;
    default:
      result = UnknownInstructionSetFeatures::Unknown(isa);
      break;
  }
  CHECK_EQ(bitmap, result->AsBitmap());
  return result;
}

const InstructionSetFeatures* InstructionSetFeatures::FromCpuInfo() {
  const InstructionSetFeatures* result;
  switch (kRuntimeISA) {
    case kArm:
    case kThumb2:
      result = ArmInstructionSetFeatures::FromCpuInfo();
      break;
    default:
      result = UnknownInstructionSetFeatures::Unknown(kRuntimeISA);
      break;
  }
  return result;
}

const InstructionSetFeatures* InstructionSetFeatures::FromHwcap() {
  const InstructionSetFeatures* result;
  switch (kRuntimeISA) {
    case kArm:
    case kThumb2:
      result = ArmInstructionSetFeatures::FromHwcap();
      break;
    default:
      result = UnknownInstructionSetFeatures::Unknown(kRuntimeISA);
      break;
  }
  return result;
}

const ArmInstructionSetFeatures* InstructionSetFeatures::AsArmInstructionSetFeatures() const {
  DCHECK_EQ(kArm, GetInstructionSet());
  return down_cast<const ArmInstructionSetFeatures*>(this);
}

std::ostream& operator<<(std::ostream& os, const InstructionSetFeatures& rhs) {
  os << "ISA: " << rhs.GetInstructionSet() << " Feature string: " << rhs.GetFeatureString();
  return os;
}

const ArmInstructionSetFeatures* ArmInstructionSetFeatures::FromFeatureString(
    const std::string& feature_list, std::string* error_msg) {
  std::vector<std::string> features;
  Split(feature_list, ',', &features);
  bool has_lpae = false;
  bool has_div = false;
  for (auto i = features.begin(); i != features.end(); i++) {
    std::string feature = Trim(*i);
    if (feature == "default" || feature == "none") {
      // Nothing to do.
    } else if (feature == "div") {
      has_div = true;
    } else if (feature == "nodiv") {
      has_div = false;
    } else if (feature == "lpae") {
      has_lpae = true;
    } else if (feature == "nolpae") {
      has_lpae = false;
    } else {
      *error_msg = StringPrintf("Unknown instruction set feature: '%s'", feature.c_str());
      return nullptr;
    }
  }
  return new ArmInstructionSetFeatures(has_lpae, has_div);
}

const ArmInstructionSetFeatures* ArmInstructionSetFeatures::FromBitmap(uint32_t bitmap) {
  bool has_lpae = (bitmap & kLpaeBitfield) != 0;
  bool has_div = (bitmap & kDivBitfield) != 0;
  return new ArmInstructionSetFeatures(has_lpae, has_div);
}

const ArmInstructionSetFeatures* ArmInstructionSetFeatures::FromCppDefines() {
#if defined(__ARM_ARCH_EXT_IDIV__)
  bool has_div = true;
#else
  bool has_div = false;
#endif
#if defined(__ARM_FEATURE_LPAE)
  bool has_lpae = true;
#else
  bool has_lpae = false;
#endif
  return new ArmInstructionSetFeatures(has_lpae, has_div);
}

const ArmInstructionSetFeatures* ArmInstructionSetFeatures::FromCpuInfo() {
  // Look in /proc/cpuinfo for features we need.  Only use this when we can guarantee that
  // the kernel puts the appropriate feature flags in here.  Sometimes it doesn't.
  bool has_lpae = false;
  bool has_div = false;

  std::ifstream in("/proc/cpuinfo");
  if (!in.fail()) {
    while (!in.eof()) {
      std::string line;
      std::getline(in, line);
      if (!in.eof()) {
        LOG(INFO) << "cpuinfo line: " << line;
        if (line.find("Features") != std::string::npos) {
          LOG(INFO) << "found features";
          if (line.find("idivt") != std::string::npos) {
            // We always expect both ARM and Thumb divide instructions to be available or not
            // available.
            CHECK_NE(line.find("idiva"), std::string::npos);
            has_div = true;
          }
          if (line.find("lpae") != std::string::npos) {
            has_lpae = true;
          }
        }
      }
    }
    in.close();
  } else {
    LOG(INFO) << "Failed to open /proc/cpuinfo";
  }
  return new ArmInstructionSetFeatures(has_lpae, has_div);
}

#if defined(HAVE_ANDROID_OS) && defined(__arm__)
#include <sys/auxv.h>
#include <asm/hwcap.h>
#endif

const ArmInstructionSetFeatures* ArmInstructionSetFeatures::FromHwcap() {
  bool has_lpae = false;
  bool has_div = false;

#if defined(HAVE_ANDROID_OS) && defined(__arm__)
  uint64_t hwcaps = getauxval(AT_HWCAP);
  LOG(INFO) << "hwcaps=" << hwcaps;
  if ((hwcaps & HWCAP_IDIVT) != 0) {
    // We always expect both ARM and Thumb divide instructions to be available or not
    // available.
    CHECK_NE(hwcaps & HWCAP_IDIVA, 0U);
    has_div = true;
  }
  if ((hwcaps & HWCAP_LPAE) != 0) {
    has_lpae = true;
  }
#endif

  return new ArmInstructionSetFeatures(has_lpae, has_div);
}


bool ArmInstructionSetFeatures::Equals(const InstructionSetFeatures* other) const {
  if (kArm != other->GetInstructionSet()) {
    return false;
  }
  const ArmInstructionSetFeatures* other_as_arm = other->AsArmInstructionSetFeatures();
  return has_lpae_ == other_as_arm->has_lpae_ && has_div_ == other_as_arm->has_div_;
}

uint32_t ArmInstructionSetFeatures::AsBitmap() const {
  return (has_lpae_ ? kLpaeBitfield : 0) | (has_div_ ? kDivBitfield : 0);
}

std::string ArmInstructionSetFeatures::GetFeatureString() const {
  std::string result;
  if (has_div_) {
    result += ",div";
  }
  if (has_lpae_) {
    result += ",lpae";
  }
  if (result.size() == 0) {
    return "none";
  } else {
    // Strip leading comma.
    return result.substr(1, result.size());
  }
}

}  // namespace art
