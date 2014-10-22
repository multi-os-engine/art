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

bool ValueBound::GreaterThanOrEqual(ValueBound bound) const {
  if (instruction_ == bound.instruction_) {
    if (instruction_ == nullptr) {
      // Pure constant.
      return constant_ >= bound.constant_;
    }
    // There might be overflow/underflow. Be conservative for now.
    return false;
  }
  // Not comparable. Just return false.
  return false;
}

bool ValueBound::LessThanOrEqual(ValueBound bound) const {
  if (instruction_ == bound.instruction_) {
    if (instruction_ == nullptr) {
      // Pure constant.
      return constant_ <= bound.constant_;
    }
    if (IsRelativeToArrayLength()) {
      // Array length is guaranteed to be no less than 0.
      // No overflow/underflow can happen if both constants are negative.
      if (constant_ <= 0 && bound.constant_ <= 0) {
        return constant_ <= bound.constant_;
      }
      // There might be overflow/underflow. Be conservative for now.
      return false;
    }
  }

  // In case the array length is some constant, we can
  // still compare.
  if (IsConstant() && bound.IsRelativeToArrayLength()) {
    HInstruction* array = bound.GetInstruction()->AsArrayLength()->InputAt(0);
    if (array->IsNullCheck()) {
      array = array->AsNullCheck()->InputAt(0);
    }
    if (array->IsNewArray()) {
      HInstruction* len = array->InputAt(0);
      if (len->IsIntConstant()) {
        int len_const = len->AsIntConstant()->GetValue();
        return constant_ <= len_const + bound.GetConstant();
      }
    }
  }

  // Not comparable. Just return false.
  return false;
}

ValueBound ValueBound::Add(int c) const {
  if (c == 0 || constant_ == INT_MAX || constant_ == INT_MIN) {
    // INT_MAX/INT_MIN are essentially infinity so adding a
    // constant doesn't add information.
    return *this;
  }

  int new_constant;
  if (c > 0) {
    if (constant_ >= INT_MAX - c) {
      // Constant part overflows.
      // Set it to INT_MAX, which isn't treated as useful anyway.
      new_constant = INT_MAX;
    } else {
      new_constant = constant_ + c;
    }
  } else {
    if (constant_ <= INT_MIN - c) {
      // Constant part underflows.
      // Set it to INT_MIN, which isn't treated as useful anyway.
      new_constant = INT_MIN;
    } else {
      new_constant = constant_ + c;
    }
  }
  return ValueBound(instruction_, new_constant);
}

ValueRange* MonotonicValueRange::Narrow(ValueRange* range) {
  if (range == nullptr) {
    return this;
  }
  DCHECK(!range->IsMonotonicValueRange());

  if (increment_ > 0) {
    // Monotonically increasing.
    ValueBound lower = ValueBound::Create(initial_, 0)
        .NarrowLowerBound(range->GetLower());

    // Need to take care of overflow. Basically need to be able to prove
    // for any value that's <= range->GetUpper(), it won't be negative with value+increment.

    if (range->GetUpper().IsConstant()) {
      int constant = range->GetUpper().GetConstant();
      if (constant <= INT_MAX - increment_) {
        return new (GetAllocator()) ValueRange(GetAllocator(), lower, range->GetUpper());
      }
    }

    if (range->GetUpper().IsRelativeToArrayLength()) {
      ValueBound next_bound = range->GetUpper().Add(increment_);
      DCHECK(next_bound.IsRelativeToArrayLength());
      int constant = next_bound.GetConstant();
      if (constant <= 0) {
        // A value of (array.length + constant) won't overflow. Luckily this covers
        // the very common case of incrementing by 1, e.g.
        // 'for (int i=0; i<array.length; i++)', even if we don't make any assumptions
        // about the max array length. If we can make assumptions about the max array length,
        // e.g. if we can establish an upper bound of the max array length, due to the max
        // heap size, divided by the element size (such as 4 bytes for each integer array),
        // we can be more aggressive about the constant part that we know
        // (array.length + constant) won't overflow.
        return new (GetAllocator()) ValueRange(GetAllocator(), lower, range->GetUpper());
      }
    }

    // There might be overflow. Just pick range.
    return range;
  } else if (increment_ < 0) {
    // Monotonically decreasing.
    ValueBound upper = ValueBound::Create(initial_, 0)
        .NarrowUpperBound(range->GetUpper());

    // Need to take care of underflow. Basically need to be able to prove
    // for any value that's >= range->GetLower(), it won't be positive with value+increment.

    if (range->GetLower().IsConstant()) {
      int constant = range->GetLower().GetConstant();
      if (constant >= INT_MIN - increment_) {
        return new (GetAllocator()) ValueRange(GetAllocator(), range->GetLower(), upper);
      }
    }

    // There might be underflow. Just pick range.
    return range;
  } else {
    LOG(FATAL) << "Unreachable";
    return this;
  }
}

void BoundsCheckElimination::Run() {
  // Reverse post order guarantees a node's dominators are visited first.
  // We want to visit in the dominator-based order since if a value is known to
  // be bounded by a range at one instruction, it must be true that all uses of
  // that value dominated by that instruction fits in that range. Range of that
  // value can be narrowed further down in the dominator tree.
  //
  // TODO: only visit blocks that dominate some array accesses.
  VisitReversePostOrder();
}

// Detect phi that's monotonically increasing/decreasing by a constant.
void BoundsCheckElimination::VisitPhi(HPhi* phi) {
  if (phi->InputCount() == 2 && phi->GetType() == Primitive::kPrimInt) {
    HInstruction* instruction = phi->InputAt(1);
    if (instruction->IsAdd()) {
      HAdd* add = instruction->AsAdd();
      HInstruction* left = add->GetLeft();
      HInstruction* right = add->GetRight();
      if (left == phi && right->IsIntConstant()) {
        HInstruction* initial_value = phi->InputAt(0);
        ValueRange* range = nullptr;
        if (right->AsIntConstant()->GetValue() == 0) {
          // Add constant 0.
          range = new (GetGraph()->GetArena()) ValueRange(
              GetGraph()->GetArena(),
              ValueBound::Create(initial_value, 0),
              ValueBound::Create(initial_value, 0));
        } else {
          // Monotonically increasing/decreasing.
          range = MonotonicValueRange::Create(
              GetGraph()->GetArena(),
              initial_value,
              right->AsIntConstant()->GetValue());
        }
        GetValueRangeMap(phi->GetBlock())->Overwrite(phi, range);
      }
    }
  }
}

std::pair<ValueBound, bool> BoundsCheckElimination::GetValueBoundFromValue(
    HInstruction* instruction) {
  if (instruction->IsIntConstant()) {
    ValueBound bound = ValueBound::Create(nullptr, instruction->AsIntConstant()->GetValue());
    return std::make_pair(bound, true);
  }

  if (instruction->IsArrayLength()) {
    ValueBound bound = ValueBound::Create(instruction, 0);
    return std::make_pair(bound, true);
  }
  // Try to detect (array.length + c) format.
  if (instruction->IsAdd()) {
    HAdd* add = instruction->AsAdd();
    HInstruction* left = add->GetLeft();
    HInstruction* right = add->GetRight();
    if (left->IsArrayLength() && right->IsIntConstant()) {
      ValueBound bound = ValueBound::Create(left, right->AsIntConstant()->GetValue());
      return std::make_pair(bound, true);
    }
  }

  // No useful bound detected.
  return std::make_pair(ValueBound::Max(), false);
}

void BoundsCheckElimination::ApplyLowerBound(HInstruction* instruction,
                                             HBasicBlock* basic_block,
                                             HBasicBlock* successor,
                                             ValueBound bound) {
  ValueRange* existing_range = LookupValueRange(instruction, basic_block);
  ValueRange* new_range = new (GetGraph()->GetArena())
      ValueRange(GetGraph()->GetArena(), bound, ValueBound::Max());
  ValueRange* narrowed_range = (existing_range == nullptr) ?
      new_range : existing_range->Narrow(new_range);
  if (narrowed_range != nullptr) {
    GetValueRangeMap(successor)->Overwrite(instruction, narrowed_range);
  }
}

void BoundsCheckElimination::ApplyUpperBound(HInstruction* instruction,
                                             HBasicBlock* basic_block,
                                             HBasicBlock* successor,
                                             ValueBound bound) {
  ValueRange* existing_range = LookupValueRange(instruction, basic_block);
  ValueRange* new_range = new (GetGraph()->GetArena())
      ValueRange(GetGraph()->GetArena(), ValueBound::Min(), bound);
  ValueRange* narrowed_range = (existing_range == nullptr) ?
      new_range : existing_range->Narrow(new_range);
  if (narrowed_range != nullptr) {
    GetValueRangeMap(successor)->Overwrite(instruction, narrowed_range);
  }
}

// Handle "if (left cmp_cond right)"
void BoundsCheckElimination::HandleIf(HIf* instruction,
                                      HInstruction* left,
                                      HInstruction* right,
                                      IfCondition cond) {
  HBasicBlock* true_successor = instruction->IfTrueSuccessor();
  // There should be no critical edge at this point.
  DCHECK_EQ(true_successor->GetPredecessors().Size(), 1u);

  HBasicBlock* false_successor = instruction->IfFalseSuccessor();
  // There should be no critical edge at this point.
  DCHECK_EQ(false_successor->GetPredecessors().Size(), 1u);

  std::pair<ValueBound, bool> pair = GetValueBoundFromValue(right);
  ValueBound bound = pair.first;
  bool found = pair.second;

  ValueBound lower = bound;
  ValueBound upper = bound;
  if (!found) {
    // No constant or array.length+c bound found.
    // For i<j, we can still use j's upper bound as i's upper bound. Same for lower.
    ValueRange* range = LookupValueRange(right, instruction->GetBlock());
    if (range != nullptr) {
      lower = range->GetLower();
      upper = range->GetUpper();
    } else {
      lower = ValueBound::Min();
      upper = ValueBound::Max();
    }
  }

  if (cond == kCondLT || cond == kCondLE) {
    if (!upper.Equals(ValueBound::Max())) {
      int compensation = (cond == kCondLT) ? -1 : 0;
      ValueBound new_upper = upper.Add(compensation);  // upper bound is inclusive
      ApplyUpperBound(left, instruction->GetBlock(), true_successor, new_upper);
    }

    // array.length as a lower bound isn't considered useful.
    if (!lower.Equals(ValueBound::Min()) && !lower.IsRelativeToArrayLength()) {
      int compensation = (cond == kCondLE) ? 1 : 0;
      ValueBound new_lower = lower.Add(compensation);  // lower bound is inclusive
      ApplyLowerBound(left, instruction->GetBlock(), false_successor, new_lower);
    }
  } else if (cond == kCondGT || cond == kCondGE) {
    // array.length as a lower bound isn't considered useful.
    if (!lower.Equals(ValueBound::Min()) && !lower.IsRelativeToArrayLength()) {
      int compensation = (cond == kCondGT) ? 1 : 0;
      ValueBound new_lower = lower.Add(compensation);  // lower bound is inclusive
      ApplyLowerBound(left, instruction->GetBlock(), true_successor, new_lower);
    }

    if (!upper.Equals(ValueBound::Max())) {
      int compensation = (cond == kCondGE) ? -1 : 0;
      ValueBound new_upper = upper.Add(compensation);  // upper bound is inclusive
      ApplyUpperBound(left, instruction->GetBlock(), false_successor, new_upper);
    }
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
    if (range != nullptr) {
      GetValueRangeMap(add->GetBlock())->Overwrite(add, range);
    }
  }
}

// The '- c' case geneates dex code of '+ (-c)' so it's handled by VisitAdd().
// Here we are interested in the typical triangular case of nested loops,
// such as the inner loop 'for (int j=0; j<array.length-i; j++)' where i
// is the index for outer loop. In this case, we know j is bounded by array.length-1.
void BoundsCheckElimination::VisitSub(HSub* sub) {
  HInstruction* left = sub->GetLeft();
  if (left->IsArrayLength()) {
    HInstruction* array_length = left->AsArrayLength();
    HInstruction* right = sub->GetRight();
    ValueRange* right_range = LookupValueRange(right, sub->GetBlock());
    if (right_range != nullptr) {
      ValueBound lower = right_range->GetLower();
      ValueBound upper = right_range->GetUpper();
      if (lower.IsConstant() && upper.IsRelativeToArrayLength()) {
        HInstruction* upper_inst = upper.GetInstruction();
        if (upper_inst->IsArrayLength() &&
            upper_inst->AsArrayLength() == array_length) {
          // (array.length - v) where v is in [c1, array.length + c2]
          // gets [-c2, array.length - c1] as its value range.
          ValueRange* range = new (GetGraph()->GetArena()) ValueRange(
              GetGraph()->GetArena(),
              ValueBound::Create(nullptr, - upper.GetConstant()),
              ValueBound::Create(array_length, - lower.GetConstant()));
          GetValueRangeMap(sub->GetBlock())->Overwrite(sub, range);
        }
      }
    }
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

  if (index_range != nullptr) {
    ValueBound lower = ValueBound::Create(nullptr, 0);        // constant 0
    ValueBound upper = ValueBound::Create(array_length, -1);  // array_length - 1
    ValueRange* array_range = new (GetGraph()->GetArena())
        ValueRange(GetGraph()->GetArena(), lower, upper);
    if (index_range->FitsIn(array_range)) {
      ReplaceBoundsCheck(bounds_check, index);
      return;
    }
  }

  if (index->IsIntConstant()) {
    ValueRange* array_length_range = LookupValueRange(array_length, block);
    int constant = index->AsIntConstant()->GetValue();
    if (array_length_range != nullptr &&
        array_length_range->GetLower().IsConstant()) {
      if (constant < array_length_range->GetLower().GetConstant()) {
        ReplaceBoundsCheck(bounds_check, index);
      }
    }

    // Once we have an array access like 'array[5] = 1', we record array.length >= 6.
    ValueBound lower = ValueBound::Create(nullptr, constant + 1);
    ValueBound upper = ValueBound::Max();
    ValueRange* range = new (GetGraph()->GetArena())
        ValueRange(GetGraph()->GetArena(), lower, upper);
    ValueRange* existing_range = LookupValueRange(array_length, block);
    ValueRange* new_range = range;
    if (existing_range != nullptr) {
      new_range = range->Narrow(existing_range);
    }
    GetValueRangeMap(block)->Overwrite(array_length, new_range);
  }
}

}  // namespace art
