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

class HArm64AddLsl : public HExpression<2> {
 public:
  HArm64AddLsl(HAdd* instr, HInstruction* left, HShl* shl, int shift)
      : HExpression(instr->GetType(), instr->GetSideEffects()),
        shift_amount_(shift) {
    SetRawInputAt(0, left);
    SetRawInputAt(1, shl->InputAt(0));
  }

  virtual bool CanBeMoved() const { return true; }
  virtual bool InstructionDataEquals(HInstruction* other_instr) const {
    HArm64AddLsl* other = other_instr->AsArm64AddLsl();
    return shift_amount_ == other->shift_amount_;
  }

  int shift_amount() const { return shift_amount_; }

  DECLARE_INSTRUCTION(Arm64AddLsl);

 private:
  int shift_amount_;

  DISALLOW_COPY_AND_ASSIGN(HArm64AddLsl);
};

class HArm64ArrayAccessAddress : public HExpression<2> {
 public:
  explicit HArm64ArrayAccessAddress(HArrayGet* array_get)
      : HExpression(Primitive::kPrimNot, SideEffects::DependsOnSomething()),
        component_type_(array_get->GetType()) {
    SetRawInputAt(0, array_get->GetArray());
    SetRawInputAt(1, array_get->GetIndex());
  }
  explicit HArm64ArrayAccessAddress(HArraySet* array_set)
      : HExpression(Primitive::kPrimNot, SideEffects::DependsOnSomething()),
        component_type_(array_set->GetComponentType()) {
    SetRawInputAt(0, array_set->GetArray());
    SetRawInputAt(1, array_set->GetIndex());
  }

  bool CanBeMoved() const OVERRIDE { return true; }
  bool InstructionDataEquals(HInstruction* other ATTRIBUTE_UNUSED) const OVERRIDE {
    return component_type_ == other->AsArm64ArrayAccessAddress()->component_type_;
  }

  HInstruction* GetArray() const { return InputAt(0); }
  HInstruction* GetIndex() const { return InputAt(1); }

  Primitive::Type GetComponentType() const { return component_type_; }

  DECLARE_INSTRUCTION(Arm64ArrayAccessAddress);

 private:
  Primitive::Type component_type_;

  DISALLOW_COPY_AND_ASSIGN(HArm64ArrayAccessAddress);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_NODES_ARM64_H_
