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

#include "bounds_check_elimination.h"

namespace art {

void BoundsCheckElimination::Run() {
  // Reverse post order guarantees a node's dominators are visited first.
  for (HReversePostOrderIterator it(*graph_); !it.Done(); it.Advance()) {
    VisitBasicBlock(it.Current());
  }
}

void BoundsCheckElimination::HandlePhis(HPhi* phi) {
  if (phi->InputCount() == 2 && phi->GetType() == Primitive::kPrimInt) {
    HInstruction* instruction = phi->InputAt(1);
    if (instruction->IsAdd()) {
      HAdd* add = instruction->AsAdd();
      HInstruction* left = add->GetLeft();
      HInstruction* right = add->GetRight();
      if (left == phi && right->IsIntConstant()) {
        HInstruction* initial_value = phi->InputAt(0);
        if (right->AsIntConstant()->GetValue() > 0) {
          // Monotonically increasing. Lower bound can be established.
          // TODO: add overflow handling.
          ValueRange range = ValueRange(
              ValueBound::MakeValueBound(initial_value, 0),
              ValueBound::Max());
          GetValueRangeMap(phi->GetBlock())->Add(phi, range);
        } else {
          // Monotonically decreasing. Upper bound can be established.
          // TODO: add underflow handling.
          ValueRange range = ValueRange(
              ValueBound::Min(),
              ValueBound::MakeValueBound(initial_value, 0));
          GetValueRangeMap(phi->GetBlock())->Add(phi, range);
        }
      }
    }
  }
}

ValueBound BoundsCheckElimination::GetValueBoundFromValue(HInstruction* instruction) {
  // Try to detect array.length + c format.
  if (instruction->IsArrayLength()) {
    return ValueBound::MakeValueBound(instruction, 0);
  }

  if (instruction->IsAdd()) {
    HAdd* add = instruction->AsAdd();
    HInstruction* left = add->GetLeft();
    HInstruction* right = add->GetRight();
    if (left->IsArrayLength() && right->IsIntConstant()) {
      return ValueBound::MakeValueBound(left, right->AsIntConstant()->GetValue());
    }
  }

  if (instruction->IsIntConstant()) {
    return ValueBound::MakeValueBound(nullptr, instruction->AsIntConstant()->GetValue());
  }

  // No useful bound detected.
  return ValueBound::Max();
}

void BoundsCheckElimination::ApplyLowerBound(HInstruction* instruction,
                                             HBasicBlock* basic_block,
                                             HBasicBlock* successor,
                                             ValueBound bound) {
  ValueRange existing_range = LookupValueRange(instruction, basic_block);
  ValueRange new_range = ValueRange(bound, ValueBound::Max());
  ValueRange narrowed_range = existing_range.Narrow(new_range);
  if (narrowed_range.IsUseful()) {
    GetValueRangeMap(successor)->Add(instruction, narrowed_range);
  }
}

void BoundsCheckElimination::ApplyUpperBound(HInstruction* instruction,
                                             HBasicBlock* basic_block,
                                             HBasicBlock* successor,
                                             ValueBound bound) {
  ValueRange existing_range = LookupValueRange(instruction, basic_block);
  ValueRange new_range = ValueRange(ValueBound::Min(), bound);
  ValueRange narrowed_range = existing_range.Narrow(new_range);
  if (narrowed_range.IsUseful()) {
    GetValueRangeMap(successor)->Add(instruction, narrowed_range);
  }
}

// Handle "if (left cmp_cond right)"
void BoundsCheckElimination::HandleIf(HIf* instruction,
                                      HInstruction* left,
                                      HInstruction* right,
                                      IfCondition cond) {
  ValueBound new_bound = GetValueBoundFromValue(right);
  if (!new_bound.IsUseful()) {
    return;
  }

  HBasicBlock* true_successor = instruction->IfTrueSuccessor();
  // There should be critical edge at this point.
  DCHECK_EQ(true_successor->GetPredecessors().Size(), 1u);

  HBasicBlock* false_successor = instruction->IfFalseSuccessor();
  // There should be critical edge at this point.
  DCHECK_EQ(false_successor->GetPredecessors().Size(), 1u);

  if (cond == kCondLT || cond == kCondLE) {
    int compensation = (cond == kCondLT) ? -1 : 0;
    ValueBound new_upper = new_bound.Add(compensation);  // upper bound is inclusive
    ApplyUpperBound(left, instruction->GetBlock(), true_successor, new_upper);

    compensation = (cond == kCondLE) ? 1 : 0;
    ValueBound new_lower = new_bound.Add(compensation);  // lower bound is inclusive
    ApplyLowerBound(left, instruction->GetBlock(), false_successor, new_lower);
  } else if (cond == kCondGT || cond == kCondGE) {
    int compensation = (cond == kCondGT) ? 1 : 0;
    ValueBound new_lower = new_bound.Add(compensation);  // lower bound is inclusive
    ApplyLowerBound(left, instruction->GetBlock(), true_successor, new_lower);

    compensation = (cond == kCondGE) ? -1 : 0;
    ValueBound new_upper = new_bound.Add(compensation);  // upper bound is inclusive
    ApplyUpperBound(left, instruction->GetBlock(), false_successor, new_upper);
  }
}

void BoundsCheckElimination::HandleIf(HIf* instruction) {
  HInstruction* comparison = instruction->InputAt(0);
  if (comparison->IsLessThan()) {
    HLessThan* less_than = comparison->AsLessThan();
    HInstruction* left = less_than->GetLeft();
    HInstruction* right = less_than->GetRight();
    HandleIf(instruction, left, right, kCondLT);
  } else if (comparison->IsLessThanOrEqual()) {
    HLessThanOrEqual* less_than_or_equal = comparison->AsLessThanOrEqual();
    HInstruction* left = less_than_or_equal->GetLeft();
    HInstruction* right = less_than_or_equal->GetRight();
    HandleIf(instruction, left, right, kCondLE);
  } else if (comparison->IsGreaterThan()) {
    HGreaterThan* greater_than = comparison->AsGreaterThan();
    HInstruction* left = greater_than->GetLeft();
    HInstruction* right = greater_than->GetRight();
    HandleIf(instruction, left, right, kCondGT);
  } else if (comparison->IsGreaterThanOrEqual()) {
    HGreaterThanOrEqual* greater_than_or_equal = comparison->AsGreaterThanOrEqual();
    HInstruction* left = greater_than_or_equal->GetLeft();
    HInstruction* right = greater_than_or_equal->GetRight();
    HandleIf(instruction, left, right, kCondGE);
  }
}

void BoundsCheckElimination::HandleAdd(HAdd* add) {
  HInstruction* right = add->GetRight();
  if (right->IsIntConstant()) {
    ValueRange left_range = LookupValueRange(add->GetLeft(), add->GetBlock());
    ValueRange range = left_range.Add(right->AsIntConstant()->GetValue());
    if (range.IsUseful()) {
      GetValueRangeMap(add->GetBlock())->Add(add, range);
    }
  }
}

void BoundsCheckElimination::HandleSub(HSub* sub) {
  HInstruction* right = sub->GetRight();
  if (sub->IsIntConstant()) {
    ValueRange left_range = LookupValueRange(sub->GetLeft(), sub->GetBlock());
    ValueRange range = left_range.Add(0 - right->AsIntConstant()->GetValue());
    if (range.IsUseful()) {
      GetValueRangeMap(sub->GetBlock())->Add(sub, range);
    }
  }
}

void BoundsCheckElimination::VisitBasicBlock(HBasicBlock* block) {
  // TODO: Only process blocks that dominate some array accesses.

  for (HInstructionIterator it(block->GetPhis()); !it.Done(); it.Advance()) {
    HandlePhis(it.Current()->AsPhi());
  }

  HInstruction* current = block->GetFirstInstruction();
  while (current != nullptr) {
    // Save the next instruction in case `current` is removed from the graph.
    HInstruction* next = current->GetNext();

    if (current->IsBoundsCheck()) {
      HInstruction* index = current->InputAt(0);
      HInstruction* array_length = current->InputAt(1);
      ValueRange index_range = LookupValueRange(index, block);
      if (index_range.IsUseful()) {
        ValueBound lower = ValueBound::MakeValueBound(nullptr, 0);        // constant 0
        ValueBound upper = ValueBound::MakeValueBound(array_length, -1);  // array_length - 1
        ValueRange array_range = ValueRange(lower, upper);
        if (index_range.FitsIn(array_range)) {
          // Use the index directly.
          current->ReplaceWith(index);
          current->GetBlock()->RemoveInstruction(current);
        }
      }
    } else if (current->IsIf()) {
      HandleIf(current->AsIf());
    } else if (current->IsAdd()) {
      HandleAdd(current->AsAdd());
    } else if (current->IsSub()) {
      HandleSub(current->AsSub());
    }
    current = next;
  }
}

}  // namespace art
