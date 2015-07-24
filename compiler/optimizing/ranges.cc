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

#include "ranges.h"

namespace art {

Range Range::Merge(const Range& a, const Range& b) {
  if (!a.IsValid() && b.IsValid()) {
    return b;
  }
  if (a.IsValid() && !b.IsValid()) {
    return a;
  }
  if (!a.IsValid() && !b.IsValid()) {
    return Range::Invalid();
  }

  return Range(std::min(a.min_value, b.min_value), std::max(a.max_value, b.max_value));
}

void Range::NarrowUpperBound(int val) {
  if (!IsValid()) {
    return;
  }

  if (min_value > val) {
    Invalidate();
    return;
  }

  max_value = std::min(val, max_value);
}

void Range::NarrowLowerBound(int val) {
  if (!IsValid()) {
    return;
  }

  if (max_value < val) {
    Invalidate();
    return;
  }

  min_value = std::max(val, min_value);
}

void RangeVisitor::HandleComingFromIf(HBasicBlock* block) {
  HInstruction* condition =
    block->GetSinglePredecessor()->GetLastInstruction()->AsIf()->InputAt(0);

  if (!condition->IsCondition()) {
    return;
  }

  // We expect constant on the right, like 'a < 3'
  if (!condition->InputAt(1)->IsIntConstant()) {
    return;
  }

  int value = condition->InputAt(1)->AsIntConstant()->GetValue();

  Range range = GetProperty(condition->InputAt(0));

  if (block->IsTrueBranch()) {
    Range old_range = range;

    if (condition->IsGreaterThan()) {
      range.NarrowLowerBound(value + 1);
    } else if (condition->IsGreaterThanOrEqual()) {
      range.NarrowLowerBound(value);
    } else if (condition->IsLessThan()) {
      range.NarrowUpperBound(value - 1);
    } else if (condition->IsLessThanOrEqual()) {
      range.NarrowUpperBound(value);
    } else if (condition->IsEqual()) {
      range.NarrowLowerBound(value);
      range.NarrowUpperBound(value);
    } else if (condition->IsNotEqual()) {
      if (range.Contains(value)) {
        return;
      }
    }

    // TODO: Handle loops a little better.
    if (!block->GetSinglePredecessor()->IsLoopHeader()) {
      // If the range doesn't change, the condition is useless.
      // If it's invalid, the condition can't be satisfied.
      if (!range.IsValid()) {
        condition->ReplaceWith(GetGraph()->GetIntConstant(0));
      } else if (old_range == range) {
        condition->ReplaceWith(GetGraph()->GetIntConstant(1));
      }
    }
  } else if (block->IsFalseBranch()) {
    if (condition->IsGreaterThan()) {
      range.NarrowUpperBound(value);
    } else if (condition->IsGreaterThanOrEqual()) {
      range.NarrowUpperBound(value - 1);
    } else if (condition->IsLessThan()) {
      range.NarrowLowerBound(value);
    } else if (condition->IsLessThanOrEqual()) {
      range.NarrowLowerBound(value + 1);
    } else if (condition->IsNotEqual()) {
      if (!range.Contains(value)) {
        range.Invalidate();
      }
    }
  }

  SetProperty(condition->InputAt(0), range);
}

void RangeVisitor::BeforeBlock(HBasicBlock* block) {
  if (block->IsLoopHeader()) {
    // TODO: Improve. We don't try to estime value of variables spanning across loop iterations
    // for computability reasons.
    for (HInstructionIterator it(block->GetPhis());
        !it.Done();
        it.Advance()) {
      SetProperty(it.Current(), Range::Default());
    }
  } else if (block->IsTrueBranch() || block->IsFalseBranch()) {
    HandleComingFromIf(block);
  }
}

}  // namespace art
