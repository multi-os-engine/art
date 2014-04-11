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

#include "globals.h"
#include "base/logging.h"  // Logging is required for FATAL in the helper functions.

namespace art {

size_t GetInstructionSetPointerSize(InstructionSet isa) {
  switch (isa) {
    case kArm:
      // Fall-through.
    case kThumb2:
      return kArmPointerSize;
    case kArm64:
      return kArm64PointerSize;
    case kX86:
      return kX86PointerSize;
    case kX86_64:
      return kX86_64PointerSize;
    case kMips:
      return kMipsPointerSize;
    case kNone:
      LOG(FATAL) << "ISA kNone does not have pointer size.";
      return 0;
    default:
      LOG(FATAL) << "Unknown ISA " << isa;
      return 0;
  }
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

bool Is64bInstructionSet(InstructionSet isa) {
  switch (isa) {
    case kArm:
    case kThumb2:
    case kX86:
    case kMips:
      return false;

    case kArm64:
    case kX86_64:
      return true;

    case kNone:
      LOG(FATAL) << "ISA kNone does not have bit width.";
      return 0;
    default:
      LOG(FATAL) << "Unknown ISA " << isa;
      return 0;
  }
}

std::string InstructionSetFeatures::GetFeatureString() const {
  std::string result;
  if ((mask_ & kHwDiv) != 0) {
    result += "div";
  }
  if (result.size() == 0) {
    result = "none";
  }
  return result;
}

}  // namespace art
