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

#include "instruction_simplifier_arm.h"

#include "mirror/array-inl.h"

namespace art {
namespace arm {

void InstructionSimplifierArmVisitor::VisitMul(HMul* instruction) {
  Primitive::Type type = instruction->GetType();
  if (type != Primitive::kPrimInt) {
    return;
  }

  HInstruction* use = instruction->HasNonEnvironmentUses()
      ? instruction->GetUses().GetFirst()->GetUser()
      : nullptr;

  if (instruction->HasOnlyOneNonEnvironmentUse() && (use->IsAdd() || use->IsSub())) {
    // Replace code looking like
    //    MUL tmp, x, y
    //    SUB dst, acc, tmp
    // with
    //    MULSUB dst, acc, x, y
    // Note that we do not want to (unconditionally) perform the merge when the
    // multiplication has multiple uses and it can be merged in all of them.
    // Multiple uses could happen on the same control-flow path, and we would
    // then increase the amount of work. In the future we could try to evaluate
    // whether all uses are on different control-flow paths (using dominance and
    // reverse-dominance information) and only perform the merge when they are.
    HInstruction* accumulator = nullptr;
    HBinaryOperation* binop = use->AsBinaryOperation();
    HInstruction* binop_left = binop->GetLeft();
    HInstruction* binop_right = binop->GetRight();
    // Be careful after GVN. This should not happen since the `HMul` has only
    // one use.
    DCHECK_NE(binop_left, binop_right);
    if (binop_right == instruction) {
      accumulator = binop_left;
    } else if (use->IsAdd()) {
      DCHECK_EQ(binop_left, instruction);
      accumulator = binop_right;
    }

    if (accumulator != nullptr) {
      HArmMultiplyAccumulate* mulacc =
          new (GetGraph()->GetArena()) HArmMultiplyAccumulate(type,
                                                              binop->GetKind(),
                                                              accumulator,
                                                              instruction->GetLeft(),
                                                              instruction->GetRight());

      binop->GetBlock()->ReplaceAndRemoveInstructionWith(binop, mulacc);
      DCHECK(!instruction->HasUses());
      instruction->GetBlock()->RemoveInstruction(instruction);
      RecordSimplification();
      return;
    }
  }
}

}  // namespace arm
}  // namespace art
