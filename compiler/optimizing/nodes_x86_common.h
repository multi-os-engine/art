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

#ifndef ART_COMPILER_OPTIMIZING_NODES_X86_COMMON_H_
#define ART_COMPILER_OPTIMIZING_NODES_X86_COMMON_H_

namespace art {

class HX86SelectValue : public HExpression<4> {
 public:
  HX86SelectValue(HCondition* cond, HInstruction* left, HInstruction* right)
      : HExpression(left->GetType(), SideEffects::None()), condition_(cond->GetCondition()) {
    DCHECK_EQ(HPhi::ToPhiType(left->GetType()), HPhi::ToPhiType(right->GetType()));
    DCHECK(!Primitive::IsFloatingPointType(left->GetType()));
    SetRawInputAt(0, left);
    SetRawInputAt(1, right);
    SetRawInputAt(2, cond->GetLeft());
    SetRawInputAt(3, cond->GetRight());
  }


  HInstruction* GetLeft() const { return InputAt(0); }
  HInstruction* GetRight() const { return InputAt(1); }
  HInstruction* GetCompareLeft() const { return InputAt(2); }
  HInstruction* GetCompareRight() const { return InputAt(3); }
  Primitive::Type GetResultType() const { return GetType(); }
  IfCondition GetCondition() const { return condition_; }

  bool CanBeMoved() const OVERRIDE { return true; }
  bool CanBeNull() const OVERRIDE { return GetLeft()->CanBeNull() || GetRight()->CanBeNull(); }
  bool InstructionDataEquals(HInstruction* other) const OVERRIDE {
    return condition_ == other->AsX86SelectValue()->GetCondition();
  }

  DECLARE_INSTRUCTION(X86SelectValue);

 private:
  IfCondition condition_;

  DISALLOW_COPY_AND_ASSIGN(HX86SelectValue);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_NODES_X86_COMMON_H_
