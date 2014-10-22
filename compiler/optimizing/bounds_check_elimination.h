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

#ifndef ART_COMPILER_OPTIMIZING_BOUNDS_CHECK_ELIMINATION_H_
#define ART_COMPILER_OPTIMIZING_BOUNDS_CHECK_ELIMINATION_H_

#include "nodes.h"
#include "utils/arena_containers.h"

namespace art {

class MonotonicValueRange;

/**
 * A value bound is represented as a pair of value and constant,
 * e.g. array.length - 1.
 */
class ValueBound : public ValueObject {
 public:
  static ValueBound Create(HInstruction* instruction, int constant) {
    if (instruction == nullptr) {
      return ValueBound(nullptr, constant);
    }
    if (instruction->IsIntConstant()) {
      return ValueBound(nullptr, instruction->AsIntConstant()->GetValue() + constant);
    }
    return ValueBound(instruction, constant);
  }

  HInstruction* GetInstruction() const { return instruction_; }
  int GetConstant() const { return constant_; }

  bool IsRelativeToArrayLength() const {
    return instruction_ != nullptr && instruction_->IsArrayLength();
  }

  bool IsConstant() const {
    return instruction_ == nullptr;
  }

  static ValueBound Min() { return ValueBound(nullptr, INT_MIN); }
  static ValueBound Max() { return ValueBound(nullptr, INT_MAX); }

  bool Equals(ValueBound bound2) const {
    return instruction_ == bound2.instruction_ &&
           constant_ == bound2.constant_;
  }

  // Returns if it's certain bound1 >= bound2.
  bool GreaterThanOrEqual(ValueBound bound) const;

  // Returns if it's certain bound1 <= bound2.
  bool LessThanOrEqual(ValueBound bound) const;

  // Try to narrow lower bound. Returns the greatest of the two.
  ValueBound NarrowLowerBound(ValueBound bound) const {
    if (instruction_ == bound.instruction_) {
      // Same instruction, compare the constant part.
      return ValueBound(bound.instruction_,
                        std::max(constant_, bound.constant_));
    }

    // Not comparable. Just pick one. We may lose some info, but that's ok.
    // Favor constant as lower bound.
    return bound.IsConstant() ? bound : *this;
  }

  // Try to narrow upper bound. Returns the lowest of the two.
  ValueBound NarrowUpperBound(ValueBound bound) const {
    if (instruction_ == bound.instruction_) {
      // Same instruction, compare the constant part.
      return ValueBound(instruction_,
                        std::min(constant_, bound.constant_));
    }

    // Not comparable. Just pick one. We may lose some info, but that's ok.
    // Favor array length as upper bound.
    return bound.IsRelativeToArrayLength() ? bound : *this;
  }

  ValueBound Add(int c) const;

 private:
  ValueBound(HInstruction* instruction, int constant)
      : instruction_(instruction), constant_(constant) {}

  HInstruction* instruction_;
  int constant_;
};

/**
 * Represent a range of lower bound and upper bound, both being inclusive.
 */
class ValueRange : public ArenaObject<kArenaAllocMisc> {
 public:
  ValueRange(ArenaAllocator* allocator, ValueBound lower, ValueBound upper)
      : allocator_(allocator), lower_(lower), upper_(upper) {}

  virtual ~ValueRange() {}

  virtual const MonotonicValueRange* AsMonotonicValueRange() const { return nullptr; }
  bool IsMonotonicValueRange() const {
    return AsMonotonicValueRange() != nullptr;
  }

  ArenaAllocator* GetAllocator() const { return allocator_; }
  ValueBound GetLower() const { return lower_; }
  ValueBound GetUpper() const { return upper_; }

  // If it's certain that this value range fits in other_range.
  virtual bool FitsIn(ValueRange* other_range) const {
    DCHECK(!other_range->IsMonotonicValueRange());

    if (other_range == nullptr) {
      return true;
    }

    return lower_.GreaterThanOrEqual(other_range->lower_) &&
           upper_.LessThanOrEqual(other_range->upper_);
  }

  virtual ValueRange* Narrow(ValueRange* range) {
    if (range == nullptr) {
      return this;
    }

    if (range->IsMonotonicValueRange()) {
      return this;
    }

    return new (allocator_) ValueRange(
        allocator_,
        lower_.NarrowLowerBound(range->lower_),
        upper_.NarrowUpperBound(range->upper_));
  }

  ValueRange* Add(int constant) const {
    return new (allocator_) ValueRange(
        allocator_, lower_.Add(constant), upper_.Add(constant));
  }

 private:
  ArenaAllocator* const allocator_;
  const ValueBound lower_;  // inclusive
  const ValueBound upper_;  // inclusive

  DISALLOW_COPY_AND_ASSIGN(ValueRange);
};

/**
 * A monotonically incrementing/decrementing value range, e.g.
 * the variable i in "for (int i=0; i<array.length; i++)".
 * Special care needs to be taken to account for overflow/underflow
 * of such value ranges.
 */
class MonotonicValueRange : public ValueRange {
 public:
  static MonotonicValueRange* Create(ArenaAllocator* allocator,
                                     HInstruction* initial, int increment) {
    DCHECK_NE(increment, 0);
    // To be conservative, give it full range [INT_MIN, INT_MAX] in case it's
    // used as a regular value range, due to possible overflow/underflow.
    return new (allocator) MonotonicValueRange(
        allocator, ValueBound::Min(), ValueBound::Max(), initial, increment);
  }

  virtual ~MonotonicValueRange() {}

  const MonotonicValueRange* AsMonotonicValueRange() const OVERRIDE { return this; }

  // If it's certain that this value range fits in other_range.
  bool FitsIn(ValueRange* other_range) const OVERRIDE {
    DCHECK(!other_range->IsMonotonicValueRange());
    if (other_range == nullptr) {
      return true;
    }
    return false;
  }

  ValueRange* Narrow(ValueRange* range) OVERRIDE;

 private:
  MonotonicValueRange(ArenaAllocator* allocator, ValueBound lower,
                      ValueBound upper, HInstruction* initial, int increment)
      : ValueRange(allocator, lower, upper),
        initial_(initial),
        increment_(increment) {}

  HInstruction* const initial_;
  const int increment_;

  DISALLOW_COPY_AND_ASSIGN(MonotonicValueRange);
};

class BoundsCheckElimination : public HGraphVisitor {
 public:
  BoundsCheckElimination(HGraph* graph)
      : HGraphVisitor(graph),
        maps_(graph->GetBlocks().Size()) {}

  void Run();

 private:
  ArenaSafeMap<HInstruction*, ValueRange*>* GetValueRangeMap(HBasicBlock* basic_block) {
    int block_id = basic_block->GetBlockId();
    if (maps_.at(block_id) == nullptr) {
      std::unique_ptr<ArenaSafeMap<HInstruction*, ValueRange*>> map(
          new ArenaSafeMap<HInstruction*, ValueRange*>(
              std::less<HInstruction*>(), GetGraph()->GetArena()->Adapter()));
      maps_.at(block_id) = std::move(map);
    }
    return maps_.at(block_id).get();
  }

  // Traverse up the dominator tree to look for value range info.
  ValueRange* LookupValueRange(HInstruction* instruction, HBasicBlock* basic_block) {
    while (basic_block != nullptr) {
      ArenaSafeMap<HInstruction*, ValueRange*>* map = GetValueRangeMap(basic_block);
      if (map->find(instruction) != map->end()) {
        return GetValueRangeMap(basic_block)->Get(instruction);
      }
      basic_block = basic_block->GetDominator();
    }
    // Didn't find any.
    return nullptr;
  }

  std::pair<ValueBound, bool> GetValueBoundFromValue(HInstruction* instruction);

  void ApplyLowerBound(HInstruction* instruction, HBasicBlock* basic_block,
                       HBasicBlock* successor, ValueBound bound);
  void ApplyUpperBound(HInstruction* instruction, HBasicBlock* basic_block,
                       HBasicBlock* successor, ValueBound bound);

  void VisitBoundsCheck(HBoundsCheck* bounds_check);
  void ReplaceBoundsCheck(HInstruction* bounds_check, HInstruction* index);
  void VisitPhi(HPhi* phi);
  void VisitIf(HIf* instruction);
  void HandleIf(HIf* instruction, HInstruction* left, HInstruction* right, IfCondition cond);
  void VisitAdd(HAdd* add);
  void VisitSub(HSub* sub);

  std::vector<std::unique_ptr<ArenaSafeMap<HInstruction*, ValueRange*>>> maps_;

  DISALLOW_COPY_AND_ASSIGN(BoundsCheckElimination);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_BOUNDS_CHECK_ELIMINATION_H_
