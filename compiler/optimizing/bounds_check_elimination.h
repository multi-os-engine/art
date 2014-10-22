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

namespace art {

/**
 * A value bound is represented as a pair of value and constant,
 * e.g. array.length - 1.
 */
class ValueBound : public ValueObject {
 public:
  static ValueBound MakeValueBound(HInstruction* instruction, int constant_) {
    if (instruction == nullptr) {
      return ValueBound(nullptr, constant_);
    }
    if (instruction->IsIntConstant()) {
      return ValueBound(nullptr, instruction->AsIntConstant()->GetValue() + constant_);
    }
    return ValueBound(instruction, constant_);
  }

  HInstruction* GetInstruction() { return instruction_; }
  int GetConstant() { return constant_; }

  bool HasArrayLength() {
    return instruction_ != nullptr && instruction_->IsArrayLength();
  }

  bool IsConstant() {
    return instruction_ == nullptr;
  }

  bool IsUseful() { return constant_ != INT_MAX && constant_ != INT_MIN; }

  static ValueBound Min() { return ValueBound(nullptr, INT_MIN); }
  static ValueBound Max() { return ValueBound(nullptr, INT_MAX); }

  static bool Equals(ValueBound bound1, ValueBound bound2) {
    return bound1.instruction_ == bound2.instruction_ &&
           bound1.constant_ == bound2.constant_;
  }

  // Returns if it's certain bound1 >= bound2.
  static bool GreaterThanOrEqual(ValueBound bound1, ValueBound bound2) {
    if (bound1.instruction_ == bound2.instruction_) {
      return bound1.constant_ >= bound2.constant_;
    }
    // Not comparable. Just return false.
    return false;
  }

  // Returns if it's certain bound1 <= bound2.
  static bool LessThanOrEqual(ValueBound bound1, ValueBound bound2) {
    if (bound1.instruction_ == bound2.instruction_) {
      return bound1.constant_ <= bound2.constant_;
    }
    // Not comparable. Just return false.
    return false;
  }

  // Try to narrow lower bound.
  static ValueBound NarrowLowerBound(ValueBound bound1, ValueBound bound2) {
    if (bound1.instruction_ == bound2.instruction_) {
      // Same instruction, compare the constant part.
      return ValueBound(bound1.instruction_,
                        std::max(bound1.constant_, bound2.constant_));
    }

    // Not comparable. Just pick one. We may lose some info, but that's ok.
    // Favor constant as lower bound.
    if (bound2.IsConstant()) {
      return bound2;
    }
    return bound1;
  }

  // Try to narrow upper bound.
  static ValueBound NarrowUpperBound(ValueBound bound1, ValueBound bound2) {
    if (bound1.instruction_ == bound2.instruction_) {
      // Same instruction, compare the constant part.
      return ValueBound(bound1.instruction_,
                        std::min(bound1.constant_, bound2.constant_));
    }

    // Not comparable. Just pick one. We may lose some info, but that's ok.
    // Favor array length as upper bound.
    if (bound2.HasArrayLength()) {
      return bound2;
    }
    return bound1;
  }

  ValueBound Add(int c) {
    if (c == 0) {
      return ValueBound(instruction_, constant_);
    }
    int new_constant;
    if (c > 0) {
      if (constant_ >= INT_MAX - c) {
        // Overflows
        new_constant = INT_MAX;
      } else {
        new_constant = constant_ + c;
      }
    } else {
      if (constant_ <= INT_MIN - c) {
        // Underflows
        new_constant = INT_MIN;
      } else {
        new_constant = constant_ + c;
      }
    }
    return ValueBound(instruction_, new_constant);
  }

 private:
  ValueBound(HInstruction* instruction, int constant)
      : instruction_(instruction), constant_(constant) {}

  HInstruction* instruction_;
  int constant_;
};

/**
 * Represent a range of lower bound and upper bound, both being inclusive.
 */
class ValueRange : public ValueObject {
 public:
  ValueRange(ValueBound lower, ValueBound upper)
      : lower_(lower), upper_(upper) {}

  // If it's certain that this value range fits in other_range.
  bool FitsIn(ValueRange other_range) {
    return ValueBound::GreaterThanOrEqual(lower_, other_range.lower_) &&
           ValueBound::LessThanOrEqual(upper_, other_range.upper_);
  }

  ValueRange Narrow(ValueRange range) {
    return ValueRange(ValueBound::NarrowLowerBound(lower_, range.lower_),
                      ValueBound::NarrowUpperBound(upper_, range.upper_));
  }

  bool IsUseful() {
    return lower_.GetConstant() != INT_MIN || upper_.GetConstant() != INT_MAX;
  }

  ValueRange Add(int constant) {
    return ValueRange(lower_.Add(constant), upper_.Add(constant));
  }

  static ValueRange FullRangeValueRange() {
    return ValueRange(ValueBound::Min(), ValueBound::Max());
  }

 private:
  ValueBound lower_;  // inclusive
  ValueBound upper_;  // inclusive
};

class ValueRangeMapEntry : public ArenaObject {
 public:
  ValueRangeMapEntry(HInstruction* instruction, ValueRange range)
      : instruction_(instruction), range_(range) {}

  HInstruction* GetInstruction() { return instruction_; }
  ValueRange GetValueRange() { return range_; }

 private:
  HInstruction* instruction_;
  ValueRange range_;

  DISALLOW_COPY_AND_ASSIGN(ValueRangeMapEntry);
};

/**
 * A node in the collision list of a ValueRangeMap.
 */
class ValueRangeCollisionNode : public ArenaObject {
 public:
  ValueRangeCollisionNode(ValueRangeMapEntry* entry, ValueRangeCollisionNode* next)
      : value_range_map_entry_(entry), next_(next) {}

  ValueRangeMapEntry* GetValueRangeMapEntry() const { return value_range_map_entry_; }
  ValueRangeCollisionNode* GetNext() const { return next_; }

 private:
  ValueRangeMapEntry* const value_range_map_entry_;
  ValueRangeCollisionNode* next_;

  DISALLOW_COPY_AND_ASSIGN(ValueRangeCollisionNode);
};

class ValueRangeMap : public ArenaObject {
 public:
  explicit ValueRangeMap(ArenaAllocator* allocator)
      : allocator_(allocator), number_of_entries_(0), collisions_(nullptr) {
    for (size_t i = 0; i < kDefaultNumberOfEntries; ++i) {
      table_[i] = nullptr;
    }
  }

  ValueRange Lookup(HInstruction* instruction) const {
    if (number_of_entries_ == 0) {
      return ValueRange::FullRangeValueRange();
    }

    size_t hash_code = instruction->ComputeHashCode();
    size_t index = hash_code % kDefaultNumberOfEntries;
    ValueRangeMapEntry* entry = table_[index];
    if (entry != nullptr && entry->GetInstruction() == instruction) {
      return entry->GetValueRange();
    }

    for (ValueRangeCollisionNode* node = collisions_; node != nullptr; node = node->GetNext()) {
      entry = node->GetValueRangeMapEntry();
      if (entry->GetInstruction() == instruction) {
        return entry->GetValueRange();
      }
    }

    return ValueRange::FullRangeValueRange();
  }

  // Adds an instruction to the map.
  void Add(HInstruction* instruction, ValueRange value_range) {
    size_t hash_code = instruction->ComputeHashCode();
    size_t index = hash_code % kDefaultNumberOfEntries;
    ValueRangeMapEntry* entry = new (allocator_)
        ValueRangeMapEntry(instruction, value_range);
    if (table_[index] == nullptr) {
      table_[index] = entry;
      ++number_of_entries_;
    } else {
      if (table_[index]->GetInstruction() == instruction) {
        DCHECK(false);
        // Update the entry.
        table_[index] = entry;
      } else {
        collisions_ = new (allocator_) ValueRangeCollisionNode(entry, collisions_);
        ++number_of_entries_;
      }
    }
  }

  bool IsEmpty() const { return number_of_entries_ == 0; }
  size_t GetNumberOfEntries() const { return number_of_entries_; }

 private:
  static constexpr size_t kDefaultNumberOfEntries = 16;

  ArenaAllocator* const allocator_;

  // The number of entries in the set.
  size_t number_of_entries_;

  // The internal implementation of the map. It uses a combination of a hash code based
  // fixed-size list, and a linked list to handle hash code collisions.
  // TODO: Tune the fixed size list original size, and support growing it.
  ValueRangeCollisionNode* collisions_;
  ValueRangeMapEntry* table_[kDefaultNumberOfEntries];

  DISALLOW_COPY_AND_ASSIGN(ValueRangeMap);
};

class BoundsCheckElimination : public ValueObject {
 public:
  BoundsCheckElimination(ArenaAllocator* allocator, HGraph* graph)
      : allocator_(allocator), graph_(graph),
        value_range_maps_(allocator, graph->GetBlocks().Size()) {
    value_range_maps_.SetSize(graph->GetBlocks().Size());
  }

  void Run();

 private:
  ValueRangeMap* GetValueRangeMap(HBasicBlock* basic_block) {
    int block_id = basic_block->GetBlockId();
    if (value_range_maps_.Get(block_id) == nullptr) {
      value_range_maps_.Put(block_id, new (allocator_) ValueRangeMap(allocator_));
    }
    return value_range_maps_.Get(block_id);
  }

  // Traverse up the dominator tree to look for value range info.
  ValueRange LookupValueRange(HInstruction* instruction, HBasicBlock* basic_block) {
    while (basic_block != nullptr) {
      ValueRange range = GetValueRangeMap(basic_block)->Lookup(instruction);
      if (range.IsUseful()) {
        return range;
      }
      basic_block = basic_block->GetDominator();
    }
    // Didn't find any.
    return ValueRange::FullRangeValueRange();
  }

  ValueBound GetValueBoundFromValue(HInstruction* instruction);
  void ApplyLowerBound(HInstruction* instruction, HBasicBlock* basic_block,
                       HBasicBlock* successor, ValueBound bound);
  void ApplyUpperBound(HInstruction* instruction, HBasicBlock* basic_block,
                       HBasicBlock* successor, ValueBound bound);

  void HandlePhis(HPhi* phi);
  void HandleIf(HIf* instruction);
  void HandleIf(HIf* instruction, HInstruction* left, HInstruction* right, IfCondition cond);
  void HandleAdd(HAdd* add);
  void HandleSub(HSub* sub);

  void VisitBasicBlock(HBasicBlock* basic_block);

  ArenaAllocator* const allocator_;
  HGraph* const graph_;
  GrowableArray<ValueRangeMap*> value_range_maps_;

  DISALLOW_COPY_AND_ASSIGN(BoundsCheckElimination);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_BOUNDS_CHECK_ELIMINATION_H_
