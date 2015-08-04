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


/*
 * This defines a range of the possible values an instruction can have as a closed interval
 * with a lower and an upper bound. When merging two values, just take the lowest lower bound as
 * the new lower bound, and the highest upper bound as the new upper bound.
 */
template <class T>
class Range {
 public:
  static T min() { return std::numeric_limits<T>::min(); }
  static T max() { return std::numeric_limits<T>::max(); }

  T Lower() const { return min_value; }
  T Upper() const { return max_value; }

  static Range Default() {
    return Range(min(), max());
  }

  bool Equals(const Range& rhs) const {
    return min_value_ == rhs.min_value_ && max_value_ == rhs.max_value_;
  }

  static Range<T> Merge(const Range& a, const Range& b) {
    bool is_a_valid = a.IsValid();
    bool is_b_valid = b.IsValid();
    if (is_a_valid) {
      if (is_b_valid) {
        return Range(std::min(a.min_value_, b.min_value_), std::max(a.max_value_, b.max_value_));
      } else {
        return a;
      }
    } else {
      return b;
    }
  }

  template <class U>
  Range(U min_input, U max_input) {
    // If the input overflows, just span across the whole T's values range.
    if (min_input < min() || max_input > max()) {
      *this = Default();
    } else if (max < min) {
      *this = Invalid();
    } else {
      min_value_ = min_input;
      max_value_ = max_input;
    }
  }

  // Change the upper bound to `val` if `val` is lower than the current value
  Range ReduceUpperBound(T val) const {
    if (UNLIKELY(!IsValid() || min_value_ > val)) {
      return Invalid();
    }

    return Range(min_value_, std::min(val, max_value_));
  }

  // Change the lower bound to `val` if `val` is higher than the current value
  Range ReduceLowerBound(T val) const {
    if (UNLIKELY(!IsValid() || max_value_ < val)) {
      return Invalid();
    }

    return Range(std::max(val, min_value_), max_value_);
  }

  bool Contains(T val) const {
    if (!IsValid()) {
      return false;
    }

    return min_value_ <= val && val <= max_value_;
  }

  static Range Invalid() { return Range(max(), min()); }

  bool IsValid() const {
    return min_value_ <= max_value_;
  }

 private:
  T min_value_;
  T max_value_;
};

typedef Range<int32_t> IntRange;

class RangeVisitor : public HContextualizedPass<IntRange, HGraphDelegateVisitor> {
 public:
  RangeVisitor(HGraph* graph)
    : HContextualizedPass(graph),
      always_true_(graph->GetArena(), 0),
      always_false_(graph->GetArena(), 0) {}

  void VisitIntConstant(HIntConstant* cte) OVERRIDE {
    SetProperty(cte, IntRange(cte->GetValue(), cte->GetValue()));
  }

  void VisitBoundType(HBoundType* instr) OVERRIDE {
      SetProperty(instr, GetProperty(instr->InputAt(0)));
  }

  void VisitAdd(HAdd* add) OVERRIDE;
  void VisitDiv(HDiv* div) OVERRIDE;
  void VisitMul(HMul* mul) OVERRIDE;

  void HandleComingFromIf(HBasicBlock* block);
  void BeforeBlock(HBasicBlock* block) OVERRIDE;

  const GrowableArray<HInstruction*>& GetAlwaysTrueIfs() const { return always_true_; }
  const GrowableArray<HInstruction*>& GetAlwaysFalseIfs() const { return always_false_; }

  // Returns true if the predecessor is an HIf and this is the true branch and false otherwise.
  static bool IsTrueBranchOfIfInstruction(HBasicBlock* block);

  // Returns true if the predecessor is an HIf and this is the false branch and false otherwise.
  static bool IsFalseBranchOfIfInstruction(HBasicBlock* block);

 private:
  GrowableArray<HInstruction*> always_true_;
  GrowableArray<HInstruction*> always_false_;
};

bool RangeVisitor::IsTrueBranchOfIfInstruction(HBasicBlock* block) {
  if (block->GetPredecessors().Size() != 1) {
    return false;
  }
  HInstruction* last = block->GetSinglePredecessor()->GetLastInstruction();
  if (!last->IsIf()) {
    return false;
  }
  HIf* condition = last->AsIf();
  return condition->IfTrueSuccessor() == block;
}

bool RangeVisitor::IsFalseBranchOfIfInstruction(HBasicBlock* block) {
  if (block->GetPredecessors().Size() != 1) {
    return false;
  }
  HInstruction* last = block->GetSinglePredecessor()->GetLastInstruction();
  if (!last->IsIf()) {
    return false;
  }
  HIf* condition = last->AsIf();
  return condition->IfFalseSuccessor() == block;
}

bool RangeVisitor::VisitAdd(HAdd* add) {
  if (div->GetType() != Primitive::kPrimInt) {
    return;
  }

  IntRange a = GetProperty(div->InputAt(0));
  IntRange b = GetProperty(div->InputAt(1));

  if (!a.IsValid() || !b.IsValid()) {
    SetProperty(div, Range::Invalid());
    return;
  }

  SetProperty(add, Range(a.Lower() - b.Upper(), a.Upper() - b.Lower()));
}

void RangeVisitor::VisitDiv(HDiv* div) {
  if (div->GetType() != Primitive::kPrimInt) {
    return;
  }

  IntRange a = GetProperty(div->InputAt(0));
  IntRange b = GetProperty(div->InputAt(1));

  if (!a.IsValid() || !b.IsValid()) {
    SetProperty(div, Range::Invalid());
    return;
  }

  if (b.min_value == 0) {
    b = b.NarrowLowerBound(1);
  } else if (b.max_value == 0) {
    b = b.NarrowUpperBound(-1);
  }

  long val0 = static_cast<long>(a.min_value) / b.min_value;
  long val1 = static_cast<long>(a.min_value) / b.max_value;
  long val2 = static_cast<long>(a.max_value) / b.min_value;
  long val3 = static_cast<long>(a.max_value) / b.max_value;

  SetProperty(div, Range(std::min({val0, val1, val2, val3}), std::max({val0, val1, val2, val3})));
}

void RangeVisitor::VisitMul(HMul* mul) {
  if (mul->GetType() != Primitive::kPrimInt) {
    return;
  }

  IntRange a = GetProperty(mul->InputAt(0));
  IntRange b = GetProperty(mul->InputAt(1));

  if (!a.IsValid() || !b.IsValid()) {
    SetProperty(mul, Range::Invalid());
    return;
  }

  long val0 = static_cast<long>(a.min_value) * b.min_value;
  long val1 = static_cast<long>(a.min_value) * b.max_value;
  long val2 = static_cast<long>(a.max_value) * b.min_value;
  long val3 = static_cast<long>(a.max_value) * b.max_value;

  SetProperty(div, Range(std::min({val0, val1, val2, val3}), std::max({val0, val1, val2, val3})));
}

void RangeVisitor::HandleComingFromIf(HBasicBlock* block) {
  DCHECK(block->GetSinglePredecessor()->GetLastInstruction()->IsIf());
  HInstruction* cond =
    block->GetSinglePredecessor()->GetLastInstruction()->AsIf()->InputAt(0);

  if (!cond->IsCondition()) {
    return;
  }

  HCondition* condition = cond->AsCondition();

  if (!condition->GetConstantRight() || !condition->GetConstantRight()->IsIntConstant()) {
    return;
  }

  int value = condition->GetConstantRight()->AsIntConstant()->GetValue();

  IntRange range = GetProperty(condition->InputAt(0));

  if (IsTrueBranchOfIfInstruction(block)) {
    IntRange old_range = range;

    switch (condition->AsCondition()->GetCondition()) {
      case kCondGT:
        range = range.ReduceLowerBound(value + 1);
        break;
      case kCondGE:
        range = range.ReduceLowerBound(value);
        break;
      case kCondLT:
        range = range.ReduceUpperBound(value - 1);
        break;
      case kCondLE:
        range = range.ReduceUpperBound(value);
        break;
      case kCondEQ:
        range = range.ReduceLowerBound(value);
        range = range.ReduceUpperBound(value);
        break;
      case kCondNE:
        if (range.Contains(value)) {
          return;
        }
        break;
    }

    // If the range doesn't change, the condition is useless.
    // If it's invalid, the condition can't be satisfied.
    if (!range.IsValid()) {
      always_false_.Add(condition);
    } else if (old_range.Equals(range)) {
      always_true_.Add(condition);
    }
  } else {
    DCHECK(IsFalseBranchOfIfInstruction(block));

    switch (condition->AsCondition()->GetCondition()) {
      case kCondGT:
        range = range.ReduceUpperBound(value);
        break;
      case kCondGE:
        range = range.ReduceUpperBound(value - 1);
        break;
      case kCondLT:
        range = range.ReduceLowerBound(value);
        break;
      case kCondLE:
        range = range.ReduceLowerBound(value + 1);
        break;
      case kCondNE:
        if (!range.Contains(value)) {
          range = IntRange::Invalid();
        }
        break;
      default:
        break;
    }
  }

  SetProperty(condition->InputAt(0), range);
}

void RangeVisitor::BeforeBlock(HBasicBlock* block) {
  if (block->IsLoopHeader()) {
    // TODO: Improve. We don't try to estimate value of variables spanning across loop iterations
    // for computability reasons.
    for (HInstructionIterator it(block->GetPhis()); !it.Done(); it.Advance()) {
      SetProperty(it.Current(), IntRange::Default());
    }
  } else if (IsTrueBranchOfIfInstruction(block) || IsFalseBranchOfIfInstruction(block)) {
    HandleComingFromIf(block);
  }
}

void RangePropagation::Run() {
  RangeVisitor visitor(graph_);
  visitor.Run();

  const GrowableArray<HInstruction*>& trues = visitor.GetAlwaysTrueIfs();
  for (size_t i = 0, size = trues.Size(); i < size; ++i) {
    trues.Get(i)->ReplaceWith(graph_->GetIntConstant(1));
  }

  const GrowableArray<HInstruction*>& falses = visitor.GetAlwaysFalseIfs();
  for (size_t i = 0, size = falses.Size(); i < size; ++i) {
    falses.Get(i)->ReplaceWith(graph_->GetIntConstant(0));
  }
}

}  // namespace art
