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

#ifndef ART_COMPILER_OPTIMIZING_NODES_ARM64_H_
#define ART_COMPILER_OPTIMIZING_NODES_ARM64_H_

#include "nodes_common.h"

namespace art {

class HArm64IntermediateAddress : public HExpression<2> {
 public:
  explicit HArm64IntermediateAddress(HInstruction* base_address, HInstruction* offset)
      : HExpression(Primitive::kPrimNot, SideEffects::DependsOnGC()) {
    SetRawInputAt(0, base_address);
    SetRawInputAt(1, offset);
  }

  bool CanBeMoved() const OVERRIDE { return true; }
  bool InstructionDataEquals(HInstruction* other ATTRIBUTE_UNUSED) const OVERRIDE { return true; }

  HInstruction* GetBaseAddress() const { return InputAt(0); }
  HInstruction* GetOffset() const { return InputAt(1); }

  DECLARE_INSTRUCTION(Arm64IntermediateAddress);

 private:
  DISALLOW_COPY_AND_ASSIGN(HArm64IntermediateAddress);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_NODES_ARM64_H_
