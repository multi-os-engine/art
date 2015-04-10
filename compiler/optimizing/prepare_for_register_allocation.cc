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

#include "prepare_for_register_allocation.h"

namespace art {

void PrepareForRegisterAllocation::Run() {
  // Order does not matter.
  for (HReversePostOrderIterator it(*GetGraph()); !it.Done(); it.Advance()) {
    HBasicBlock* block = it.Current();
    // No need to visit the phis.
    for (HInstructionIterator inst_it(block->GetInstructions()); !inst_it.Done();
         inst_it.Advance()) {
      inst_it.Current()->Accept(this);
    }
  }
}

void PrepareForRegisterAllocation::VisitNullCheck(HNullCheck* check) {
  check->ReplaceWith(check->InputAt(0));
}

void PrepareForRegisterAllocation::VisitDivZeroCheck(HDivZeroCheck* check) {
  check->ReplaceWith(check->InputAt(0));
}

void PrepareForRegisterAllocation::VisitBoundsCheck(HBoundsCheck* check) {
  check->ReplaceWith(check->InputAt(0));
}

void PrepareForRegisterAllocation::VisitBoundType(HBoundType* bound_type) {
  bound_type->ReplaceWith(bound_type->InputAt(0));
  bound_type->GetBlock()->RemoveInstruction(bound_type);
}

void PrepareForRegisterAllocation::VisitClinitCheck(HClinitCheck* check) {
  HLoadClass* cls = check->GetLoadClass();
  check->ReplaceWith(cls);
  if (check->GetPrevious() == cls) {
    // Pass the initialization duty to the `HLoadClass` instruction,
    // and remove the instruction from the graph.
    cls->SetMustGenerateClinitCheck();
    check->GetBlock()->RemoveInstruction(check);
  }
}

void PrepareForRegisterAllocation::VisitCondition(HCondition* condition) {
  bool needs_materialization = false;
  if (!condition->GetUses().HasOnlyOneUse() || !condition->GetEnvUses().IsEmpty()) {
    needs_materialization = true;
  } else {
    HInstruction* user = condition->GetUses().GetFirst()->GetUser();
    if (!user->IsIf() && !user->IsDeoptimize()) {
      needs_materialization = true;
    } else {
      // TODO: if there is no intervening instructions with side-effect between this condition
      // and the If instruction, we should move the condition just before the If.
      if (condition->GetNext() != user) {
        needs_materialization = true;
      }
    }
  }
  if (!needs_materialization) {
    condition->ClearNeedsMaterialization();

    // Try to fold an HCompare into this HCondition.
    HInstruction* left = condition->GetLeft();
    HInstruction* right = condition->GetRight();
    // We can only replace an HCondition which compares a Compare to 0.
    // 'dx' code generation always generates a compare to 0.
    if (!left->IsCompare() || !right->IsConstant() || right->AsIntConstant()->GetValue() != 0) {
      // Conversion is not possible.
      return;
    }

    // Is the Compare only used for this purpose?
    if (!left->GetUses().HasOnlyOneUse()) {
      // Someone else also wants the result of the compare.
      return;
    }

    if (!left->GetEnvUses().IsEmpty()) {
      // There is a reference to the compare result in an environment.  Do we really need it?
      if (left->GetBlock()->GetGraph()->IsDebuggable()) {
        return;
      }
    }

    // We have to ensure that there are no deopt points in the sequence.
    if (left->HasAnyEnvironmentUseBefore(condition)) {
      return;
    }

    // Clean up any environment uses from the HCompare, if any.
    // We do this at the end to ensure that we don't remove them unless we commit to this change.
    left->RemoveEnvironmentUsers();

    // We have decided to fold the HCompare into the HCondition.  Transfer the information.
    condition->SetBias(left->AsCompare()->GetBias());

    // Replace the operands of the HCondition.
    condition->ReplaceInput(left->InputAt(0), 0);
    condition->ReplaceInput(left->InputAt(1), 1);

    // Remove the HCompare.
    left->GetBlock()->RemoveInstruction(left, false);
  }
}

void PrepareForRegisterAllocation::VisitInvokeStaticOrDirect(HInvokeStaticOrDirect* invoke) {
  if (invoke->IsStaticWithExplicitClinitCheck()) {
    size_t last_input_index = invoke->InputCount() - 1;
    HInstruction* last_input = invoke->InputAt(last_input_index);
    DCHECK(last_input->IsLoadClass()) << last_input->DebugName();

    // Remove a load class instruction as last input of a static
    // invoke, which has been added (along with a clinit check,
    // removed by PrepareForRegisterAllocation::VisitClinitCheck
    // previously) by the graph builder during the creation of the
    // static invoke instruction, but is no longer required at this
    // stage (i.e., after inlining has been performed).
    invoke->RemoveLoadClassAsLastInput();

    // If the load class instruction is no longer used, remove it from
    // the graph.
    if (!last_input->HasUses()) {
      last_input->GetBlock()->RemoveInstruction(last_input);
    }
  }
}

}  // namespace art
