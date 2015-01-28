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

#include "instruction_simplifier_arm64.h"

namespace art {
namespace arm64 {

bool InstructionSimplifierArm64::TryFitIntoOperand(HAdd* instr,
                                                   HInstruction* left,
                                                   HInstruction* op) {
  DCHECK(instr->IsAdd());

  if (!op->IsShl()) {
    return false;
  }
  HShl* shl = op->AsShl();

  DCHECK(shl->GetType() == Primitive::kPrimInt || shl->GetType() == Primitive::kPrimLong);

  if (!shl->AsBinaryOperation()->GetRight()->IsConstant()) {
    return false;
  }

  HConstant* cst = shl->GetRight()->AsConstant();
  int shift_amount = cst->IsIntConstant() ? cst->AsIntConstant()->GetValue()
                                          : cst->AsLongConstant()->GetValue();
  HArm64AddLsl* addlsl =
      new (GetGraph()->GetArena()) HArm64AddLsl(instr, left, shl, shift_amount);

  instr->GetBlock()->ReplaceAndRemoveInstructionWith(instr, addlsl);
  shl->GetBlock()->RemoveInstruction(shl);
  return true;
}

// Try to transform a sequence of instructions looking like:
//   lsl x2, x10, #5
//   add x0, x1, x2
// into
//   add x0, x1, x10, LSL #5
void InstructionSimplifierArm64::TryMergeInputIntoOperand(HAdd* instr) {
  DCHECK(instr->IsAdd() || instr->IsAnd() || instr->IsOr() || instr->IsSub() || instr->IsXor());
  Primitive::Type type = instr->GetType();
  if (type != Primitive::kPrimInt && type != Primitive::kPrimLong) {
    return;
  }

  HInstruction* left = instr->InputAt(0);
  HInstruction* right = instr->InputAt(1);

  if (right->HasOnlyOneUse() && !right->HasEnvironmentUses()) {
    if (TryFitIntoOperand(instr, left, right)) {
      return;
    }
  }
  if (instr->IsCommutative() && left->HasOnlyOneUse() && !left->HasEnvironmentUses()) {
    TryFitIntoOperand(instr, right, left);
  }
}

void InstructionSimplifierArm64::VisitAdd(HAdd* instr) {
  TryMergeInputIntoOperand(instr);
}

}  // namespace arm64
}  // namespace art
