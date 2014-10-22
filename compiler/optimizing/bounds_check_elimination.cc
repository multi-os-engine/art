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
  // We want to visit in the dominator-based order since if a value is known to
  // be bounded by a range at one instruction, it must be true that all uses of
  // that value dominated by that instruction fits in that range. Range of that
  // value can be narrowed further down in the dominator tree.
  VisitReversePostOrder();
}

void BoundsCheckElimination::VisitPhi(HPhi* phi) {
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
          ValueRange* range = new (GetGraph()->GetArena()) MonotonicValueRange(
              GetGraph()->GetArena(),
              ValueBound::MakeValueBound(initial_value, 0),
              ValueBound::Max(),
              right->AsIntConstant()->GetValue());
          GetValueRangeMap(phi->GetBlock())->Add(phi, range);
        } else if (right->AsIntConstant()->GetValue() < 0) {
          // Monotonically decreasing. Upper bound can be established.
          ValueRange* range = new (GetGraph()->GetArena()) MonotonicValueRange(
              GetGraph()->GetArena(),
              ValueBound::Min(),
              ValueBound::MakeValueBound(initial_value, 0),
              right->AsIntConstant()->GetValue());
          GetValueRangeMap(phi->GetBlock())->Add(phi, range);
        } else {
          // Add constant 0.
          ValueRange* range = new (GetGraph()->GetArena()) ValueRange(
              GetGraph()->GetArena(),
              ValueBound::MakeValueBound(initial_value, 0),
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
  ValueRange* existing_range = LookupValueRange(instruction, basic_block);
  ValueRange* new_range = new (GetGraph()->GetArena()) ValueRange(GetGraph()->GetArena(), bound, ValueBound::Max());
  ValueRange* narrowed_range = (existing_range == nullptr) ?
      new_range : existing_range->Narrow(new_range);
  if (narrowed_range != nullptr && narrowed_range->IsUseful()) {
    GetValueRangeMap(successor)->Add(instruction, narrowed_range);
  }
}

void BoundsCheckElimination::ApplyUpperBound(HInstruction* instruction,
                                             HBasicBlock* basic_block,
                                             HBasicBlock* successor,
                                             ValueBound bound) {
  ValueRange* existing_range = LookupValueRange(instruction, basic_block);
  ValueRange* new_range = new (GetGraph()->GetArena()) ValueRange(GetGraph()->GetArena(), ValueBound::Min(), bound);
  ValueRange* narrowed_range = (existing_range == nullptr) ?
      new_range : existing_range->Narrow(new_range);
  if (narrowed_range != nullptr && narrowed_range->IsUseful()) {
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
  // There should be no critical edge at this point.
  DCHECK_EQ(true_successor->GetPredecessors().Size(), 1u);

  HBasicBlock* false_successor = instruction->IfFalseSuccessor();
  // There should be no critical edge at this point.
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

void BoundsCheckElimination::VisitIf(HIf* instruction) {
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

void BoundsCheckElimination::VisitAdd(HAdd* add) {
  HInstruction* right = add->GetRight();
  if (right->IsIntConstant()) {
    ValueRange* left_range = LookupValueRange(add->GetLeft(), add->GetBlock());
    if (left_range == nullptr) {
      return;
    }
    ValueRange* range = left_range->Add(right->AsIntConstant()->GetValue());
    if (range != nullptr && range->IsUseful()) {
      GetValueRangeMap(add->GetBlock())->Add(add, range);
    }
  }
}

void BoundsCheckElimination::HandleArrayAccess(HBoundsCheck* bounds_check) {
  HInstruction* index = bounds_check->InputAt(0);
  // Once we have an array access like 'array[5] = 1', we record array.length >= 6.
  if (index->IsIntConstant()) {
    int constant = index->AsIntConstant()->GetValue();
    HInstruction* array_length = bounds_check->InputAt(1);
    ValueBound lower = ValueBound::MakeValueBound(nullptr, constant + 1);
    ValueBound upper = ValueBound::Max();
    ValueRange* range = new (GetGraph()->GetArena()) ValueRange(GetGraph()->GetArena(), lower, upper);

    HBasicBlock* block = bounds_check->GetBlock();
    ValueRange* existing_range = LookupValueRange(array_length, block);
    ValueRange* new_range = range;
    if (existing_range != nullptr && existing_range->IsUseful()) {
      new_range = range->Narrow(existing_range);
    }
    GetValueRangeMap(block)->Add(array_length, new_range);
  }
}

void BoundsCheckElimination::VisitArrayGet(HArrayGet* array_get) {
  HInstruction* index = array_get->InputAt(1);
  if (index->IsBoundsCheck()) {
    HandleArrayAccess(index->AsBoundsCheck());
  }
}

void BoundsCheckElimination::VisitArraySet(HArraySet* array_set) {
  HInstruction* index = array_set->InputAt(1);
  if (index->IsBoundsCheck()) {
    HandleArrayAccess(index->AsBoundsCheck());
  }
}

void BoundsCheckElimination::ReplaceBoundsCheck(HInstruction* bounds_check,
                                                HInstruction* index) {
  bounds_check->ReplaceWith(index);
  bounds_check->GetBlock()->RemoveInstruction(bounds_check);
}

void BoundsCheckElimination::VisitBoundsCheck(HBoundsCheck* bounds_check) {
  HBasicBlock* block = bounds_check->GetBlock();
  HInstruction* index = bounds_check->InputAt(0);
  HInstruction* array_length = bounds_check->InputAt(1);
  ValueRange* index_range = LookupValueRange(index, block);

  if (index_range != nullptr && index_range->IsUseful()) {
    ValueBound lower = ValueBound::MakeValueBound(nullptr, 0);        // constant 0
    ValueBound upper = ValueBound::MakeValueBound(array_length, -1);  // array_length - 1
    ValueRange* array_range = new (GetGraph()->GetArena())
        ValueRange(GetGraph()->GetArena(), lower, upper);
    if (index_range->FitsIn(array_range)) {
      ReplaceBoundsCheck(bounds_check, index);
      return;
    }
  }

  if (index->IsIntConstant()) {
    ValueRange* array_length_range = LookupValueRange(array_length, block);
    if (array_length_range != nullptr &&
        array_length_range->GetLower().IsConstant()) {
      if (index->AsIntConstant()->GetValue() <
          array_length_range->GetLower().GetConstant()) {
        ReplaceBoundsCheck(bounds_check, index);
      }
    }
  }
}

}  // namespace art
