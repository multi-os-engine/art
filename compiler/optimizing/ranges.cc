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

// Defines a range of the possible values an instruction can have as a closed interval
// with a lower and an upper bound.
template <class T>
class Range {
 public:
  static constexpr T min() { return std::numeric_limits<T>::min(); }
  static constexpr T max() { return std::numeric_limits<T>::max(); }

  T Lower() const { return min_value_; }
  T Upper() const { return max_value_; }

  static constexpr Range Default() {
    return Range(min(), max());
  }

  bool Equals(const Range& rhs) const {
    return min_value_ == rhs.min_value_ && max_value_ == rhs.max_value_;
  }

  // When merging two values, take the lowest lower bound as the new lower bound, and the highest
  // upper bound as the new upper bound.
  static Range<T> Merge(const Range& a, const Range& b) {
    return Range(std::min(a.min_value_, b.min_value_), std::max(a.max_value_, b.max_value_));
  }

  Range(T min_input, T max_input)
    : min_value_(min_input),
      max_value_(max_input) {}

  // Change the upper bound to `val` if `val` is lower than the current value
  Range ReduceUpperBound(T val) const {
    if (UNLIKELY(IsEmpty() || min_value_ > val)) {
      return Empty();
    }

    return Range(min_value_, std::min(val, max_value_));
  }

  // Change the lower bound to `val` if `val` is higher than the current value
  Range ReduceLowerBound(T val) const {
    if (UNLIKELY(IsEmpty() || max_value_ < val)) {
      return Empty();
    }

    return Range(std::max(val, min_value_), max_value_);
  }

  bool Contains(T val) const {
    return min_value_ <= val && val <= max_value_;
  }

  static constexpr Range Empty() { return Range(max(), min()); }

  bool IsEmpty() const {
    return min_value_ > max_value_;
  }

 private:
  T min_value_;
  T max_value_;
};

typedef Range<int32_t> IntRange;

class RangeVisitor : public HContextualizedPass<IntRange, HGraphDelegateVisitor> {
 public:
  explicit RangeVisitor(HGraph* graph)
    : HContextualizedPass(graph),
      always_true_(graph->GetArena(), 0),
      always_false_(graph->GetArena(), 0) {}

  void VisitIntConstant(HIntConstant* cte) OVERRIDE {
    SetProperty(cte, IntRange(cte->GetValue(), cte->GetValue()));
  }

  void VisitPhi(HPhi* phi) OVERRIDE;
  void VisitAdd(HAdd* add) OVERRIDE;
  void VisitSub(HSub* sub) OVERRIDE;
  void VisitDiv(HDiv* div) OVERRIDE;
  void VisitMul(HMul* mul) OVERRIDE;

  void HandleComingFromIf(HBasicBlock* block);
  void BeforeBlock(HBasicBlock* block) OVERRIDE;

  const GrowableArray<HCondition*>& GetAlwaysTrueConditions() const { return always_true_; }
  const GrowableArray<HCondition*>& GetAlwaysFalseConditions() const { return always_false_; }

 private:
  void SetBoundsCheckingForOverflows(HInstruction* instr, int64_t lower, int64_t upper);

  GrowableArray<HCondition*> always_true_;
  GrowableArray<HCondition*> always_false_;
};

// Returns true if the predecessor is an HIf.
static bool IsBranchOfIfInstruction(HBasicBlock* block) {
  if (block->GetPredecessors().Size() != 1) {
    return false;
  }
  return block->GetSinglePredecessor()->GetLastInstruction()->IsIf();
}

// For a predecessor being an HIf, return wether this is the true branch.
static bool IsTrueBranchOfIfInstruction(HBasicBlock* block) {
  DCHECK(IsBranchOfIfInstruction(block));
  HIf* condition = block->GetSinglePredecessor()->GetLastInstruction()->AsIf();
  return condition->IfTrueSuccessor() == block;
}

// For a predecessor being an HIf, return wether this is the false branch.
static bool IsFalseBranchOfIfInstruction(HBasicBlock* block) {
  DCHECK(IsBranchOfIfInstruction(block));
  HIf* condition = block->GetSinglePredecessor()->GetLastInstruction()->AsIf();
  return condition->IfFalseSuccessor() == block;
}

void RangeVisitor::SetBoundsCheckingForOverflows(HInstruction* instr, int64_t lower, int64_t upper) {
  if (lower < IntRange::min() || upper > IntRange::max()) {
    // An overflow / underflow occured, use the maximum range
    SetProperty(instr, IntRange::Default());
  } else {
    SetProperty(instr, IntRange(lower, upper));
  }
}

void RangeVisitor::VisitAdd(HAdd* add) {
  if (add->GetType() != Primitive::kPrimInt) {
    return;
  }
  IntRange a = GetProperty(add->InputAt(0));
  IntRange b = GetProperty(add->InputAt(1));

  if (a.IsEmpty() || b.IsEmpty()) {
    SetProperty(add, IntRange::Empty());
    return;
  }

  int64_t lower = static_cast<int64_t>(a.Lower()) + b.Lower();
  int64_t upper = static_cast<int64_t>(a.Upper()) + b.Upper();

  SetBoundsCheckingForOverflows(add, lower, upper);
}

void RangeVisitor::VisitSub(HSub* sub) {
  if (sub->GetType() != Primitive::kPrimInt) {
    return;
  }

  IntRange a = GetProperty(sub->InputAt(0));
  IntRange b = GetProperty(sub->InputAt(1));

  if (a.IsEmpty() || b.IsEmpty()) {
    SetProperty(sub, IntRange::Empty());
    return;
  }

  int64_t lower = static_cast<int64_t>(a.Lower()) - b.Upper();
  int64_t upper = static_cast<int64_t>(a.Upper()) - b.Lower();

  SetBoundsCheckingForOverflows(sub, lower, upper);
}

void RangeVisitor::VisitDiv(HDiv* div) {
  if (div->GetType() != Primitive::kPrimInt) {
    return;
  }

  IntRange a = GetProperty(div->InputAt(0));
  IntRange b = GetProperty(div->InputAt(1));

  if (a.IsEmpty() || b.IsEmpty()) {
    SetProperty(div, IntRange::Empty());
    return;
  }

  if (b.Lower() == 0) {
    b = b.ReduceLowerBound(1);
  } else if (b.Upper() == 0) {
    b = b.ReduceUpperBound(-1);
  }

  int64_t val0 = static_cast<int64_t>(a.Lower()) / b.Lower();
  int64_t val1 = static_cast<int64_t>(a.Lower()) / b.Upper();
  int64_t val2 = static_cast<int64_t>(a.Upper()) / b.Lower();
  int64_t val3 = static_cast<int64_t>(a.Upper()) / b.Upper();

  SetBoundsCheckingForOverflows(div,
      std::min({val0, val1, val2, val3}), std::max({val0, val1, val2, val3}));
}

void RangeVisitor::VisitMul(HMul* mul) {
  if (mul->GetType() != Primitive::kPrimInt) {
    return;
  }

  IntRange a = GetProperty(mul->InputAt(0));
  IntRange b = GetProperty(mul->InputAt(1));

  if (a.IsEmpty() || b.IsEmpty()) {
    SetProperty(mul, IntRange::Empty());
    return;
  }

  int64_t val0 = static_cast<int64_t>(a.Lower()) * b.Lower();
  int64_t val1 = static_cast<int64_t>(a.Lower()) * b.Upper();
  int64_t val2 = static_cast<int64_t>(a.Upper()) * b.Lower();
  int64_t val3 = static_cast<int64_t>(a.Upper()) * b.Upper();

  int64_t min = std::min({val0, val1, val2, val3});
  int64_t max = std::max({val0, val1, val2, val3});

  SetBoundsCheckingForOverflows(mul, min, max);
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
        if (range.Contains(value)) {
          range = IntRange(value, value);
        } else {
          range = IntRange::Empty();
        }
        break;
      case kCondNE:
        // `if (a != 3)` does not allow to entail something about the new range of values
        break;
    }

    if (range.IsEmpty()) {
      // If the range is invalid, the condition can't be satisfied.
      always_false_.Add(condition);
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
          range = IntRange::Empty();
        }
        break;
      case kCondEQ:
        // `if (a == 3)` does not allow to entail something about the new range of values
        break;
    }

    if (range.IsEmpty()) {
      // If the range is invalid, the condition is always satisfied.
      always_true_.Add(condition);
    }
  }

  SetProperty(condition->GetLeastConstantLeft(), range);
}

void RangeVisitor::VisitPhi(HPhi* phi) {
  if (phi->GetBlock()->IsLoopHeader()) {
    // TODO: Improve. We don't try to estimate value of variables spanning across loop iterations
    // for computability reasons.
    SetProperty(phi, IntRange::Default());
  }
}

void RangeVisitor::BeforeBlock(HBasicBlock* block) {
  if (IsBranchOfIfInstruction(block)) {
    HandleComingFromIf(block);
  }
}

void RangePropagation::Run() {
  RangeVisitor visitor(graph_);
  visitor.RunOnce();

  const GrowableArray<HCondition*>& trues = visitor.GetAlwaysTrueConditions();
  for (size_t i = 0, size = trues.Size(); i < size; ++i) {
    trues.Get(i)->ReplaceWith(graph_->GetIntConstant(1));
  }

  const GrowableArray<HCondition*>& falses = visitor.GetAlwaysFalseConditions();
  for (size_t i = 0, size = falses.Size(); i < size; ++i) {
    falses.Get(i)->ReplaceWith(graph_->GetIntConstant(0));
  }
}

}  // namespace art
