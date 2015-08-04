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

  T Lower() const { return min_value_; }
  T Upper() const { return max_value_; }

  static Range Default() {
    return Range(min(), max());
  }

  bool Equals(const Range& rhs) const {
    return min_value_ == rhs.min_value_ && max_value_ == rhs.max_value_;
  }

  static Range<T> Merge(const Range& a, const Range& b) {
    bool is_b_valid = b.IsValid();
    if (a.IsValid()) {
      if (is_b_valid) {
        return Range(std::min(a.min_value_, b.min_value_), std::max(a.max_value_, b.max_value_));
      } else {
        return a;
      }
    } else {
      return b;
    }
  }

  Range(T min_input, T max_input)
    : min_value_(min_input),
      max_value_(max_input) {}

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

  void VisitPhi(HPhi* phi) OVERRIDE;
  void VisitSub(HSub* sub) OVERRIDE;
  void VisitDiv(HDiv* div) OVERRIDE;
  void VisitMul(HMul* mul) OVERRIDE;

  void HandleComingFromIf(HBasicBlock* block);
  void BeforeBlock(HBasicBlock* block) OVERRIDE;

  const GrowableArray<HCondition*>& GetAlwaysTrueConditions() const { return always_true_; }
  const GrowableArray<HCondition*>& GetAlwaysFalseConditions() const { return always_false_; }

 private:
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

void RangeVisitor::VisitSub(HSub* sub) {
  if (sub->GetType() != Primitive::kPrimInt) {
    return;
  }

  IntRange a = GetProperty(sub->InputAt(0));
  IntRange b = GetProperty(sub->InputAt(1));

  if (!a.IsValid() || !b.IsValid()) {
    SetProperty(sub, IntRange::Invalid());
    return;
  }

  SetProperty(sub, IntRange(a.Lower() - b.Upper(), a.Upper() - b.Lower()));
}

void RangeVisitor::VisitDiv(HDiv* div) {
  if (div->GetType() != Primitive::kPrimInt) {
    return;
  }

  IntRange a = GetProperty(div->InputAt(0));
  IntRange b = GetProperty(div->InputAt(1));

  if (!a.IsValid() || !b.IsValid()) {
    SetProperty(div, IntRange::Invalid());
    return;
  }

  if (b.Lower() == 0) {
    b = b.ReduceLowerBound(1);
  } else if (b.Upper() == 0) {
    b = b.ReduceUpperBound(-1);
  }

  long val0 = static_cast<long>(a.Lower()) / b.Lower();
  long val1 = static_cast<long>(a.Lower()) / b.Upper();
  long val2 = static_cast<long>(a.Upper()) / b.Lower();
  long val3 = static_cast<long>(a.Upper()) / b.Upper();

  SetProperty(div,
      IntRange(std::min({val0, val1, val2, val3}), std::max({val0, val1, val2, val3})));
}

void RangeVisitor::VisitMul(HMul* mul) {
  if (mul->GetType() != Primitive::kPrimInt) {
    return;
  }

  IntRange a = GetProperty(mul->InputAt(0));
  IntRange b = GetProperty(mul->InputAt(1));

  if (!a.IsValid() || !b.IsValid()) {
    SetProperty(mul, IntRange::Invalid());
    return;
  }

  long val0 = static_cast<long>(a.Lower()) * b.Lower();
  long val1 = static_cast<long>(a.Lower()) * b.Upper();
  long val2 = static_cast<long>(a.Upper()) * b.Lower();
  long val3 = static_cast<long>(a.Upper()) * b.Upper();

  long min = std::min({val0, val1, val2, val3});
  long max = std::max({val0, val1, val2, val3});

  if (min < IntRange::min() || max > IntRange::max()) {
    SetProperty(mul, IntRange::Invalid());
  } else {
    SetProperty(mul, IntRange(min, max));
  }
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
          range = IntRange::Invalid();
        }
        break;
      case kCondNE:
        // `if (a != 3)` does not allow to entail something about the new range of values
        break;
    }

    if (!range.IsValid()) {
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
          range = IntRange::Invalid();
        }
        break;
      case kCondEQ:
        // `if (a == 3)` does not allow to entail something about the new range of values
        break;
    }

    if (!range.IsValid()) {
      // If the range is invalid, the condition is always satisfied.
      always_true_.Add(condition);
    }
  }

  SetProperty(condition->InputAt(0), range);
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
  visitor.Run();

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
