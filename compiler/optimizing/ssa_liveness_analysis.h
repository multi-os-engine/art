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

#ifndef ART_COMPILER_OPTIMIZING_SSA_LIVENESS_ANALYSIS_H_
#define ART_COMPILER_OPTIMIZING_SSA_LIVENESS_ANALYSIS_H_

#include "nodes.h"
#include <iostream>

namespace art {

class CodeGenerator;

static constexpr int kNoRegister = -1;

class BlockInfo : public ArenaObject<kArenaAllocMisc> {
 public:
  BlockInfo(ArenaAllocator* allocator, const HBasicBlock& block, size_t number_of_ssa_values)
      : block_(block),
        live_in_(allocator, number_of_ssa_values, false),
        live_out_(allocator, number_of_ssa_values, false),
        kill_(allocator, number_of_ssa_values, false) {
    UNUSED(block_);
    live_in_.ClearAllBits();
    live_out_.ClearAllBits();
    kill_.ClearAllBits();
  }

 private:
  const HBasicBlock& block_;
  ArenaBitVector live_in_;
  ArenaBitVector live_out_;
  ArenaBitVector kill_;

  friend class SsaLivenessAnalysis;

  DISALLOW_COPY_AND_ASSIGN(BlockInfo);
};

/**
 * A live range contains the start and end of a range where an instruction or a temporary
 * is live.
 */
class LiveRange FINAL : public ArenaObject<kArenaAllocMisc> {
 public:
  LiveRange(size_t start, size_t end, LiveRange* next) : start_(start), end_(end), next_(next) {
    DCHECK_LT(start, end);
    DCHECK(next_ == nullptr || next_->GetStart() > GetEnd());
  }

  size_t GetStart() const { return start_; }
  size_t GetEnd() const { return end_; }
  LiveRange* GetNext() const { return next_; }

  bool IntersectsWith(const LiveRange& other) const {
    return (start_ >= other.start_ && start_ < other.end_)
        || (other.start_ >= start_ && other.start_ < end_);
  }

  bool IsBefore(const LiveRange& other) const {
    return end_ <= other.start_;
  }

  void Dump(std::ostream& stream) const {
    stream << "[" << start_ << ", " << end_ << ")";
  }

  LiveRange* Dup(ArenaAllocator* allocator) const {
    return new (allocator) LiveRange(
        start_, end_, next_ == nullptr ? nullptr : next_->Dup(allocator));
  }

  LiveRange* GetLastRange() {
    return next_ == nullptr ? this : next_->GetLastRange();
  }

 private:
  size_t start_;
  size_t end_;
  LiveRange* next_;

  friend class LiveInterval;

  DISALLOW_COPY_AND_ASSIGN(LiveRange);
};

/**
 * A use position represents a live interval use at a given position.
 */
class UsePosition : public ArenaObject<kArenaAllocMisc> {
 public:
  UsePosition(HInstruction* user,
              size_t input_index,
              bool is_environment,
              size_t position,
              UsePosition* next)
      : user_(user),
        input_index_(input_index),
        is_environment_(is_environment),
        position_(position),
        next_(next) {
    DCHECK(user->IsPhi()
        || (GetPosition() == user->GetLifetimePosition() + 1)
        || (GetPosition() == user->GetLifetimePosition()));
    DCHECK(next_ == nullptr || next->GetPosition() >= GetPosition());
  }

  size_t GetPosition() const { return position_; }

  UsePosition* GetNext() const { return next_; }
  void SetNext(UsePosition* next) { next_ = next; }

  HInstruction* GetUser() const { return user_; }

  bool GetIsEnvironment() const { return is_environment_; }

  size_t GetInputIndex() const { return input_index_; }

  void Dump(std::ostream& stream) const {
    stream << position_;
  }

  UsePosition* Dup(ArenaAllocator* allocator) const {
    return new (allocator) UsePosition(
        user_, input_index_, is_environment_, position_,
        next_ == nullptr ? nullptr : next_->Dup(allocator));
  }

 private:
  HInstruction* const user_;
  const size_t input_index_;
  const bool is_environment_;
  const size_t position_;
  UsePosition* next_;

  DISALLOW_COPY_AND_ASSIGN(UsePosition);
};

class SafepointPosition : public ArenaObject<kArenaAllocMisc> {
 public:
  explicit SafepointPosition(HInstruction* instruction)
      : instruction_(instruction),
        next_(nullptr) {}

  void SetNext(SafepointPosition* next) {
    next_ = next;
  }

  size_t GetPosition() const {
    return instruction_->GetLifetimePosition();
  }

  SafepointPosition* GetNext() const {
    return next_;
  }

  LocationSummary* GetLocations() const {
    return instruction_->GetLocations();
  }

  HInstruction* GetInstruction() const {
    return instruction_;
  }

 private:
  HInstruction* const instruction_;
  SafepointPosition* next_;

  DISALLOW_COPY_AND_ASSIGN(SafepointPosition);
};

/**
 * An interval is a list of disjoint live ranges where an instruction is live.
 * Each instruction that has uses gets an interval.
 */
class LiveInterval : public ArenaObject<kArenaAllocMisc> {
 public:
  static LiveInterval* MakeInterval(ArenaAllocator* allocator,
                                    Primitive::Type type,
                                    HInstruction* instruction = nullptr) {
    return new (allocator) LiveInterval(allocator, type, instruction);
  }

  static LiveInterval* MakeSlowPathInterval(ArenaAllocator* allocator, HInstruction* instruction) {
    return new (allocator) LiveInterval(
        allocator, Primitive::kPrimVoid, instruction, false, kNoRegister, false, true);
  }

  static LiveInterval* MakeFixedInterval(ArenaAllocator* allocator, int reg, Primitive::Type type) {
    return new (allocator) LiveInterval(allocator, type, nullptr, true, reg, false);
  }

  static LiveInterval* MakeTempInterval(ArenaAllocator* allocator, Primitive::Type type) {
    return new (allocator) LiveInterval(allocator, type, nullptr, false, kNoRegister, true);
  }

  bool IsFixed() const { return is_fixed_; }
  bool IsTemp() const { return is_temp_; }
  bool IsSlowPathSafepoint() const { return is_slow_path_safepoint_; }
  // This interval is the result of a split.
  bool IsSplit() const { return parent_ != this; }

  void AddTempUse(HInstruction* instruction, size_t temp_index) {
    DCHECK(IsTemp());
    DCHECK(first_use_ == nullptr) << "A temporary can only have one user";
    size_t position = instruction->GetLifetimePosition();
    first_use_ = new (allocator_) UsePosition(
        instruction, temp_index, /* is_environment */ false, position, first_use_);
    AddRange(position, position + 1);
  }

  void AddUse(HInstruction* instruction,
              size_t input_index,
              bool is_environment,
              bool keep_alive = false) {
    // Set the use within the instruction.
    size_t position = instruction->GetLifetimePosition() + 1;
    LocationSummary* locations = instruction->GetLocations();
    if (!is_environment) {
      if (locations->IsFixedInput(input_index) || locations->OutputUsesSameAs(input_index)) {
        // For fixed inputs and output same as input, the register allocator
        // requires to have inputs die at the instruction, so that input moves use the
        // location of the input just before that instruction (and not potential moves due
        // to splitting).
        position = instruction->GetLifetimePosition();
      }
    }

    DCHECK(position == instruction->GetLifetimePosition()
           || position == instruction->GetLifetimePosition() + 1);

    if ((first_use_ != nullptr)
        && (first_use_->GetUser() == instruction)
        && (first_use_->GetPosition() < position)) {
      // The user uses the instruction multiple times, and one use dies before the other.
      // We update the use list so that the latter is first.
      DCHECK(!is_environment);
      UsePosition* cursor = first_use_;
      while ((cursor->GetNext() != nullptr) && (cursor->GetNext()->GetPosition() < position)) {
        cursor = cursor->GetNext();
      }
      DCHECK(first_use_->GetPosition() + 1 == position);
      UsePosition* new_use = new (allocator_) UsePosition(
          instruction, input_index, is_environment, position, cursor->GetNext());
      cursor->SetNext(new_use);
      if (first_range_->GetEnd() == first_use_->GetPosition()) {
        first_range_->end_ = position;
      }
      return;
    }

    first_use_ = new (allocator_) UsePosition(
        instruction, input_index, is_environment, position, first_use_);

    if (is_environment && !keep_alive) {
      // If this environment use does not keep the instruction live, it does not
      // affect the live range of that instruction.
      return;
    }

    size_t start_block_position = instruction->GetBlock()->GetLifetimeStart();
    if (first_range_ == nullptr) {
      // First time we see a use of that interval.
      first_range_ = last_range_ = new (allocator_) LiveRange(
          start_block_position, position, nullptr);
    } else if (first_range_->GetStart() == start_block_position) {
      // There is a use later in the same block or in a following block.
      // Note that in such a case, `AddRange` for the whole blocks has been called
      // before arriving in this method, and this is the reason the start of
      // `first_range_` is before the given `position`.
      DCHECK_LE(position, first_range_->GetEnd());
    } else {
      DCHECK(first_range_->GetStart() > position);
      // There is a hole in the interval. Create a new range.
      // Note that the start of `first_range_` can be equal to `end`: two blocks
      // having adjacent lifetime positions are not necessarily
      // predecessor/successor. When two blocks are predecessor/successor, the
      // liveness algorithm has called `AddRange` before arriving in this method,
      // and the check line 205 would succeed.
      first_range_ = new (allocator_) LiveRange(start_block_position, position, first_range_);
    }
  }

  void AddPhiUse(HInstruction* instruction, size_t input_index, HBasicBlock* block) {
    DCHECK(instruction->IsPhi());
    first_use_ = new (allocator_) UsePosition(
        instruction, input_index, false, block->GetLifetimeEnd(), first_use_);
  }

  void AddRange(size_t start, size_t end) {
    if (first_range_ == nullptr) {
      first_range_ = last_range_ = new (allocator_) LiveRange(start, end, first_range_);
    } else if (first_range_->GetStart() == end) {
      // There is a use in the following block.
      first_range_->start_ = start;
    } else if (first_range_->GetStart() == start && first_range_->GetEnd() == end) {
      DCHECK(is_fixed_);
    } else {
      DCHECK_GT(first_range_->GetStart(), end);
      // There is a hole in the interval. Create a new range.
      first_range_ = new (allocator_) LiveRange(start, end, first_range_);
    }
  }

  void AddLoopRange(size_t start, size_t end) {
    DCHECK(first_range_ != nullptr);
    DCHECK_LE(start, first_range_->GetStart());
    // Find the range that covers the positions after the loop.
    LiveRange* after_loop = first_range_;
    LiveRange* last_in_loop = nullptr;
    while (after_loop != nullptr && after_loop->GetEnd() < end) {
      DCHECK_LE(start, after_loop->GetStart());
      last_in_loop = after_loop;
      after_loop = after_loop->GetNext();
    }
    if (after_loop == nullptr) {
      // Uses are only in the loop.
      first_range_ = last_range_ = new (allocator_) LiveRange(start, end, nullptr);
    } else if (after_loop->GetStart() <= end) {
      first_range_ = after_loop;
      // There are uses after the loop.
      first_range_->start_ = start;
    } else {
      // The use after the loop is after a lifetime hole.
      DCHECK(last_in_loop != nullptr);
      first_range_ = last_in_loop;
      first_range_->start_ = start;
      first_range_->end_ = end;
    }
  }

  bool HasSpillSlot() const { return spill_slot_ != kNoSpillSlot; }
  void SetSpillSlot(int slot) {
    DCHECK(!is_fixed_);
    DCHECK(!is_temp_);
    spill_slot_ = slot;
  }
  int GetSpillSlot() const { return spill_slot_; }

  void SetFrom(size_t from) {
    if (first_range_ != nullptr) {
      first_range_->start_ = from;
    } else {
      // Instruction without uses.
      DCHECK(!defined_by_->HasNonEnvironmentUses());
      DCHECK(from == defined_by_->GetLifetimePosition());
      first_range_ = last_range_ = new (allocator_) LiveRange(from, from + 2, nullptr);
    }
  }

  LiveInterval* GetParent() const { return parent_; }

  LiveRange* GetFirstRange() const { return first_range_; }
  LiveRange* GetLastRange() const { return last_range_; }

  int GetRegister() const { return register_; }
  void SetRegister(int reg) { register_ = reg; }
  void ClearRegister() { register_ = kNoRegister; }
  bool HasRegister() const { return register_ != kNoRegister; }

  bool IsDefinedAt(size_t position) const {
    return GetStart() <= position && !IsDeadAt(position);
  }

  bool IsDeadAt(size_t position) const {
    return GetEnd() <= position;
  }

  // Returns true if the interval contains a LiveRange covering `position`.
  // Since this can be expensive, only ranges at or after the current linear
  // scan position (set with AdvanceScanPosition) are tested. Use Covers_Slow
  // if `position` may be smaller.
  bool Covers(size_t position) const {
    return FindRangeAt(position, range_search_start_) != nullptr;
  }

  // Same as Covers but tests all ranges, regardless of the position currently
  // inspected by linear scan.
  bool Covers_Slow(size_t position) const {
    return FindRangeAt(position, first_range_) != nullptr;
  }

  // Returns the first intersection of this interval with `other`.
  // `other` must be an interval starting at the same or later position than
  // the current interval of the linear scan (see AdvanceScanPosition).
  size_t FirstIntersectionWith(LiveInterval* other) const {
    // Advance both intervals and find the first matching range start in
    // this interval.
    LiveRange* other_range = other->first_range_;
    LiveRange* my_range = FindRangeAtOrAfter(other->GetStart(), range_search_start_);
    if (my_range == nullptr) {
      return kNoLifetime;
    }

    do {
      if (my_range->IsBefore(*other_range)) {
        my_range = my_range->GetNext();
        if (my_range == nullptr) {
          return kNoLifetime;
        }
      } else if (other_range->IsBefore(*my_range)) {
        other_range = other_range->GetNext();
        if (other_range == nullptr) {
          return kNoLifetime;
        }
      } else {
        DCHECK(my_range->IntersectsWith(*other_range));
        return std::max(my_range->GetStart(), other_range->GetStart());
      }
    } while (true);
  }

  size_t GetStart() const {
    return first_range_->GetStart();
  }

  size_t GetEnd() const {
    return last_range_->GetEnd();
  }

  size_t FirstRegisterUseAfter(size_t position) const {
    if (is_temp_) {
      return position == GetStart() ? position : kNoLifetime;
    }
    if (position == GetStart() && defined_by_ != nullptr) {
      LocationSummary* locations = defined_by_->GetLocations();
      Location location = locations->Out();
      // This interval is the first interval of the instruction. If the output
      // of the instruction requires a register, we return the position of that instruction
      // as the first register use.
      if (location.IsUnallocated()) {
        if ((location.GetPolicy() == Location::kRequiresRegister)
             || (location.GetPolicy() == Location::kSameAsFirstInput
                 && (locations->InAt(0).IsRegister()
                     || locations->InAt(0).IsRegisterPair()
                     || locations->InAt(0).GetPolicy() == Location::kRequiresRegister))) {
          return position;
        } else if ((location.GetPolicy() == Location::kRequiresFpuRegister)
                   || (location.GetPolicy() == Location::kSameAsFirstInput
                       && locations->InAt(0).GetPolicy() == Location::kRequiresFpuRegister)) {
          return position;
        }
      } else if (location.IsRegister() || location.IsRegisterPair()) {
        return position;
      }
    }

    UsePosition* use = first_use_;
    size_t end = GetEnd();
    while (use != nullptr && use->GetPosition() <= end) {
      size_t use_position = use->GetPosition();
      if (use_position > position && !use->GetIsEnvironment()) {
        Location location = use->GetUser()->GetLocations()->InAt(use->GetInputIndex());
        if (location.IsUnallocated()
            && (location.GetPolicy() == Location::kRequiresRegister
                || location.GetPolicy() == Location::kRequiresFpuRegister)) {
          return use_position;
        }
      }
      use = use->GetNext();
    }
    return kNoLifetime;
  }

  size_t FirstRegisterUse() const {
    return FirstRegisterUseAfter(GetStart());
  }

  size_t FirstUseAfter(size_t position) const {
    if (is_temp_) {
      return position == GetStart() ? position : kNoLifetime;
    }

    UsePosition* use = first_use_;
    size_t end = GetEnd();
    while (use != nullptr && use->GetPosition() <= end) {
      if (!use->GetIsEnvironment()) {
        size_t use_position = use->GetPosition();
        if (use_position > position) {
          return use_position;
        }
      }
      use = use->GetNext();
    }
    return kNoLifetime;
  }

  UsePosition* GetFirstUse() const {
    return first_use_;
  }

  Primitive::Type GetType() const {
    return type_;
  }

  HInstruction* GetDefinedBy() const {
    return defined_by_;
  }

  SafepointPosition* FindSafepointJustBefore(size_t position) const {
    for (SafepointPosition* safepoint = first_safepoint_, *previous = nullptr;
         safepoint != nullptr;
         previous = safepoint, safepoint = safepoint->GetNext()) {
      if (safepoint->GetPosition() >= position) return previous;
    }
    return last_safepoint_;
  }

  /**
   * Split this interval at `position`. This interval is changed to:
   * [start ... position).
   *
   * The new interval covers:
   * [position ... end)
   */
  LiveInterval* SplitAt(size_t position) {
    DCHECK(!is_temp_);
    DCHECK(!is_fixed_);
    DCHECK_GT(position, GetStart());

    if (IsDeadAt(position)) {
      // This range dies before `position`, no need to split.
      return nullptr;
    }

    LiveInterval* new_interval = new (allocator_) LiveInterval(allocator_, type_);
    SafepointPosition* new_last_safepoint = FindSafepointJustBefore(position);
    if (new_last_safepoint == nullptr) {
      new_interval->first_safepoint_ = first_safepoint_;
      new_interval->last_safepoint_ = last_safepoint_;
      first_safepoint_ = last_safepoint_ = nullptr;
    } else if (last_safepoint_ != new_last_safepoint) {
      new_interval->last_safepoint_ = last_safepoint_;
      new_interval->first_safepoint_ = new_last_safepoint->GetNext();
      DCHECK(new_interval->first_safepoint_ != nullptr);
      last_safepoint_ = new_last_safepoint;
      last_safepoint_->SetNext(nullptr);
    }

    new_interval->next_sibling_ = next_sibling_;
    next_sibling_ = new_interval;
    new_interval->parent_ = parent_;

    new_interval->first_use_ = first_use_;
    LiveRange* current = first_range_;
    LiveRange* previous = nullptr;
    // Iterate over the ranges, and either find a range that covers this position, or
    // two ranges in between this position (that is, the position is in a lifetime hole).
    do {
      if (position >= current->GetEnd()) {
        // Move to next range.
        previous = current;
        current = current->next_;
      } else if (position <= current->GetStart()) {
        // If the previous range did not cover this position, we know position is in
        // a lifetime hole. We can just break the first_range_ and last_range_ links
        // and return the new interval.
        DCHECK(previous != nullptr);
        DCHECK(current != first_range_);
        new_interval->last_range_ = last_range_;
        last_range_ = previous;
        previous->next_ = nullptr;
        new_interval->first_range_ = current;
        if (range_search_start_ != nullptr && range_search_start_->GetEnd() >= current->GetEnd()) {
          // Search start point is inside `new_interval`. Change it to nullptr,
          // i.e. the end of the interval.
          range_search_start_ = nullptr;
        }
        return new_interval;
      } else {
        // This range covers position. We create a new last_range_ for this interval
        // that covers last_range_->Start() and position. We also shorten the current
        // range and make it the first range of the new interval.
        DCHECK(position < current->GetEnd() && position > current->GetStart());
        new_interval->last_range_ = last_range_;
        last_range_ = new (allocator_) LiveRange(current->start_, position, nullptr);
        if (previous != nullptr) {
          previous->next_ = last_range_;
        } else {
          first_range_ = last_range_;
        }
        new_interval->first_range_ = current;
        current->start_ = position;
        if (range_search_start_ != nullptr && range_search_start_->GetEnd() >= current->GetEnd()) {
          // Search start point is inside `new_interval`. Change it to `last_range`.
          // This is slightly conservative but always correct.
          range_search_start_ = last_range_;
        }
        return new_interval;
      }
    } while (current != nullptr);

    LOG(FATAL) << "Unreachable";
    return nullptr;
  }

  bool StartsBeforeOrAt(LiveInterval* other) const {
    return GetStart() <= other->GetStart();
  }

  bool StartsAfter(LiveInterval* other) const {
    return GetStart() > other->GetStart();
  }

  void Dump(std::ostream& stream) const {
    stream << "ranges: { ";
    LiveRange* current = first_range_;
    while (current != nullptr) {
      current->Dump(stream);
      stream << " ";
      current = current->GetNext();
    }
    stream << "}, uses: { ";
    UsePosition* use = first_use_;
    if (use != nullptr) {
      do {
        use->Dump(stream);
        stream << " ";
      } while ((use = use->GetNext()) != nullptr);
    }
    stream << "}";
    stream << " is_fixed: " << is_fixed_ << ", is_split: " << IsSplit();
    stream << " is_high: " << IsHighInterval();
    stream << " is_low: " << IsLowInterval();
  }

  LiveInterval* GetNextSibling() const { return next_sibling_; }
  LiveInterval* GetLastSibling() {
    LiveInterval* result = this;
    while (result->next_sibling_ != nullptr) {
      result = result->next_sibling_;
    }
    return result;
  }

  // Returns the first register hint that is at least free before
  // the value contained in `free_until`. If none is found, returns
  // `kNoRegister`.
  int FindFirstRegisterHint(size_t* free_until) const;

  // If there is enough at the definition site to find a register (for example
  // it uses the same input as the first input), returns the register as a hint.
  // Returns kNoRegister otherwise.
  int FindHintAtDefinition() const;

  // Returns whether the interval needs two (Dex virtual register size `kVRegSize`)
  // slots for spilling.
  bool NeedsTwoSpillSlots() const;

  bool IsFloatingPoint() const {
    return type_ == Primitive::kPrimFloat || type_ == Primitive::kPrimDouble;
  }

  // Converts the location of the interval to a `Location` object.
  Location ToLocation() const;

  // Returns the location of the interval following its siblings at `position`.
  Location GetLocationAt(size_t position);

  // Finds the interval that covers `position`.
  LiveInterval* GetSiblingAt(size_t position);

  // Returns whether `other` and `this` share the same kind of register.
  bool SameRegisterKind(Location other) const;
  bool SameRegisterKind(const LiveInterval& other) const {
    return IsFloatingPoint() == other.IsFloatingPoint();
  }

  bool HasHighInterval() const {
    return IsLowInterval();
  }

  bool HasLowInterval() const {
    return IsHighInterval();
  }

  LiveInterval* GetLowInterval() const {
    DCHECK(HasLowInterval());
    return high_or_low_interval_;
  }

  LiveInterval* GetHighInterval() const {
    DCHECK(HasHighInterval());
    return high_or_low_interval_;
  }

  bool IsHighInterval() const {
    return GetParent()->is_high_interval_;
  }

  bool IsLowInterval() const {
    return !IsHighInterval() && (GetParent()->high_or_low_interval_ != nullptr);
  }

  void SetLowInterval(LiveInterval* low) {
    DCHECK(IsHighInterval());
    high_or_low_interval_ = low;
  }

  void SetHighInterval(LiveInterval* high) {
    DCHECK(IsLowInterval());
    high_or_low_interval_ = high;
  }

  void AddHighInterval(bool is_temp = false) {
    DCHECK_EQ(GetParent(), this);
    DCHECK(!HasHighInterval());
    DCHECK(!HasLowInterval());
    high_or_low_interval_ = new (allocator_) LiveInterval(
        allocator_, type_, defined_by_, false, kNoRegister, is_temp, false, true);
    high_or_low_interval_->high_or_low_interval_ = this;
    if (first_range_ != nullptr) {
      high_or_low_interval_->first_range_ = first_range_->Dup(allocator_);
      high_or_low_interval_->last_range_ = first_range_->GetLastRange();
    }
    if (first_use_ != nullptr) {
      high_or_low_interval_->first_use_ = first_use_->Dup(allocator_);
    }
  }

  // Returns whether an interval, when it is non-split, is using
  // the same register of one of its input.
  bool IsUsingInputRegister() const {
    DCHECK(kIsDebugBuild) << "Function should be called only as a DCHECK";
    if (defined_by_ != nullptr && !IsSplit()) {
      for (HInputIterator it(defined_by_); !it.Done(); it.Advance()) {
        const LiveInterval* interval = it.Current()->GetLiveInterval();

        // Find the interval that covers `defined_by`_. Calls to this function
        // are made outside the linear scan, hence we need to use Covers_Slow.
        while (interval != nullptr && !interval->Covers_Slow(defined_by_->GetLifetimePosition())) {
          interval = interval->GetNextSibling();
        }

        // Check if both intervals have the same register of the same kind.
        if (interval != nullptr
            && interval->SameRegisterKind(*this)
            && interval->GetRegister() == GetRegister()) {
          return true;
        }
      }
    }
    return false;
  }

  // Returns whether an interval, when it is non-split, can safely use
  // the same register of one of its input. Note that this method requires
  // IsUsingInputRegister() to be true.
  bool CanUseInputRegister() const {
    DCHECK(kIsDebugBuild) << "Function should be called only as a DCHECK";
    DCHECK(IsUsingInputRegister());
    if (defined_by_ != nullptr && !IsSplit()) {
      LocationSummary* locations = defined_by_->GetLocations();
      if (locations->OutputCanOverlapWithInputs()) {
        return false;
      }
      for (HInputIterator it(defined_by_); !it.Done(); it.Advance()) {
        const LiveInterval* interval = it.Current()->GetLiveInterval();

        // Find the interval that covers `defined_by`_. Calls to this function
        // are made outside the linear scan, hence we need to use Covers_Slow.
        while (interval != nullptr && !interval->Covers_Slow(defined_by_->GetLifetimePosition())) {
          interval = interval->GetNextSibling();
        }

        if (interval != nullptr
            && interval->SameRegisterKind(*this)
            && interval->GetRegister() == GetRegister()) {
          // We found the input that has the same register. Check if it is live after
          // `defined_by`_.
          return !interval->Covers_Slow(defined_by_->GetLifetimePosition() + 1);
        }
      }
    }
    LOG(FATAL) << "Unreachable";
    UNREACHABLE();
  }

  void AddSafepoint(HInstruction* instruction) {
    SafepointPosition* safepoint = new (allocator_) SafepointPosition(instruction);
    if (first_safepoint_ == nullptr) {
      first_safepoint_ = last_safepoint_ = safepoint;
    } else {
      DCHECK_LT(last_safepoint_->GetPosition(), safepoint->GetPosition());
      last_safepoint_->SetNext(safepoint);
      last_safepoint_ = safepoint;
    }
  }

  SafepointPosition* GetFirstSafepoint() const {
    return first_safepoint_;
  }

  // Resets the starting point for range-searching queries to the first range.
  // Intervals must be reset prior to starting a new linear scan over them.
  void StartNewScan() {
    range_search_start_ = first_range_;
  }

  // Updates the starting point for range-searching queries to the first range
  // at or after `position`. Intervals must be updated every time linear scan
  // advances to its next liveness position.
  void AdvanceScanPosition(size_t position) {
    // Calling FindRangeAtOrAfter with the current search cache ensures we do
    // not move backwards.
    range_search_start_ = FindRangeAtOrAfter(position, range_search_start_);
  }

 private:
  LiveInterval(ArenaAllocator* allocator,
               Primitive::Type type,
               HInstruction* defined_by = nullptr,
               bool is_fixed = false,
               int reg = kNoRegister,
               bool is_temp = false,
               bool is_slow_path_safepoint = false,
               bool is_high_interval = false)
      : allocator_(allocator),
        first_range_(nullptr),
        last_range_(nullptr),
        first_safepoint_(nullptr),
        last_safepoint_(nullptr),
        range_search_start_(nullptr),
        first_use_(nullptr),
        type_(type),
        next_sibling_(nullptr),
        parent_(this),
        register_(reg),
        spill_slot_(kNoSpillSlot),
        is_fixed_(is_fixed),
        is_temp_(is_temp),
        is_slow_path_safepoint_(is_slow_path_safepoint),
        is_high_interval_(is_high_interval),
        high_or_low_interval_(nullptr),
        defined_by_(defined_by) {}

  // Searches for a LiveRange that either covers the given position or is the
  // first next LiveRange. Returns nullptr if no such LiveRange exists. Ranges
  // known to end before `position` can be skipped with `search_start`.
  LiveRange* FindRangeAtOrAfter(size_t position, LiveRange* search_start) const {
    if (kIsDebugBuild) {
      if (search_start != first_range_) {
        // If we are not searching the entire list of ranges, make sure we do
        // not skip the range we are searching for.
        DCHECK((search_start != nullptr && search_start->GetStart() <= position)
               || search_start == FindRangeAtOrAfter(position, first_range_));
      }
    }

    LiveRange* range;
    for (range = search_start;
         range != nullptr && range->GetEnd() <= position;
         range = range->GetNext()) {
      continue;
    }
    return range;
  }

  LiveRange* FindRangeAt(size_t position, LiveRange* search_start) const {
    LiveRange* candidate = FindRangeAtOrAfter(position, search_start);
    if (candidate != nullptr && candidate->GetStart() <= position) {
      return candidate;
    } else {
      return nullptr;
    }
  }

  ArenaAllocator* const allocator_;

  // Ranges of this interval. We need a quick access to the last range to test
  // for liveness (see `IsDeadAt`).
  LiveRange* first_range_;
  LiveRange* last_range_;

  // Safepoints where this interval is live.
  SafepointPosition* first_safepoint_;
  SafepointPosition* last_safepoint_;

  // The first range at or after the current position of a linear scan. It is
  // used to optimize range-searching queries.
  LiveRange* range_search_start_;

  // Uses of this interval. Note that this linked list is shared amongst siblings.
  UsePosition* first_use_;

  // The instruction type this interval corresponds to.
  const Primitive::Type type_;

  // Live interval that is the result of a split.
  LiveInterval* next_sibling_;

  // The first interval from which split intervals come from.
  LiveInterval* parent_;

  // The register allocated to this interval.
  int register_;

  // The spill slot allocated to this interval.
  int spill_slot_;

  // Whether the interval is for a fixed register.
  const bool is_fixed_;

  // Whether the interval is for a temporary.
  const bool is_temp_;

  // Whether the interval is for a safepoint that calls on slow path.
  const bool is_slow_path_safepoint_;

  // Whether this interval is a synthesized interval for register pair.
  const bool is_high_interval_;

  // If this interval needs a register pair, the high or low equivalent.
  // `is_high_interval_` tells whether this holds the low or the high.
  LiveInterval* high_or_low_interval_;

  // The instruction represented by this interval.
  HInstruction* const defined_by_;

  static constexpr int kNoRegister = -1;
  static constexpr int kNoSpillSlot = -1;

  ART_FRIEND_TEST(RegisterAllocatorTest, SpillInactive);

  DISALLOW_COPY_AND_ASSIGN(LiveInterval);
};

/**
 * Analysis that computes the liveness of instructions:
 *
 * (a) Non-environment uses of an instruction always make
 *     the instruction live.
 * (b) Environment uses of an instruction whose type is
 *     object (that is, non-primitive), make the instruction live.
 *     This is due to having to keep alive objects that have
 *     finalizers deleting native objects.
 * (c) When the graph has the debuggable property, environment uses
 *     of an instruction that has a primitive type make the instruction live.
 *     If the graph does not have the debuggable property, the environment
 *     use has no effect, and may get a 'none' value after register allocation.
 *
 * (b) and (c) are implemented through SsaLivenessAnalysis::ShouldBeLiveForEnvironment.
 */
class SsaLivenessAnalysis : public ValueObject {
 public:
  SsaLivenessAnalysis(HGraph* graph, CodeGenerator* codegen)
      : graph_(graph),
        codegen_(codegen),
        block_infos_(graph->GetArena(), graph->GetBlocks().Size()),
        instructions_from_ssa_index_(graph->GetArena(), 0),
        instructions_from_lifetime_position_(graph->GetArena(), 0),
        number_of_ssa_values_(0) {
    block_infos_.SetSize(graph->GetBlocks().Size());
  }

  void Analyze();

  BitVector* GetLiveInSet(const HBasicBlock& block) const {
    return &block_infos_.Get(block.GetBlockId())->live_in_;
  }

  BitVector* GetLiveOutSet(const HBasicBlock& block) const {
    return &block_infos_.Get(block.GetBlockId())->live_out_;
  }

  BitVector* GetKillSet(const HBasicBlock& block) const {
    return &block_infos_.Get(block.GetBlockId())->kill_;
  }

  HInstruction* GetInstructionFromSsaIndex(size_t index) const {
    return instructions_from_ssa_index_.Get(index);
  }

  HInstruction* GetInstructionFromPosition(size_t index) const {
    return instructions_from_lifetime_position_.Get(index);
  }

  HInstruction* GetTempUser(LiveInterval* temp) const {
    // A temporary shares the same lifetime start as the instruction that requires it.
    DCHECK(temp->IsTemp());
    HInstruction* user = GetInstructionFromPosition(temp->GetStart() / 2);
    DCHECK_EQ(user, temp->GetFirstUse()->GetUser());
    return user;
  }

  size_t GetTempIndex(LiveInterval* temp) const {
    // We use the input index to store the index of the temporary in the user's temporary list.
    DCHECK(temp->IsTemp());
    return temp->GetFirstUse()->GetInputIndex();
  }

  size_t GetMaxLifetimePosition() const {
    return instructions_from_lifetime_position_.Size() * 2 - 1;
  }

  size_t GetNumberOfSsaValues() const {
    return number_of_ssa_values_;
  }

  static constexpr const char* kLivenessPassName = "liveness";

 private:
  // Linearize the graph so that:
  // (1): a block is always after its dominator,
  // (2): blocks of loops are contiguous.
  // This creates a natural and efficient ordering when visualizing live ranges.
  void LinearizeGraph();

  // Give an SSA number to each instruction that defines a value used by another instruction,
  // and setup the lifetime information of each instruction and block.
  void NumberInstructions();

  // Compute live ranges of instructions, as well as live_in, live_out and kill sets.
  void ComputeLiveness();

  // Compute the live ranges of instructions, as well as the initial live_in, live_out and
  // kill sets, that do not take into account backward branches.
  void ComputeLiveRanges();

  // After computing the initial sets, this method does a fixed point
  // calculation over the live_in and live_out set to take into account
  // backwards branches.
  void ComputeLiveInAndLiveOutSets();

  // Update the live_in set of the block and returns whether it has changed.
  bool UpdateLiveIn(const HBasicBlock& block);

  // Update the live_out set of the block and returns whether it has changed.
  bool UpdateLiveOut(const HBasicBlock& block);

  static bool ShouldBeLiveForEnvironment(HInstruction* instruction) {
    if (instruction == nullptr) return false;
    if (instruction->GetBlock()->GetGraph()->IsDebuggable()) return true;
    return instruction->GetType() == Primitive::kPrimNot;
  }

  HGraph* const graph_;
  CodeGenerator* const codegen_;
  GrowableArray<BlockInfo*> block_infos_;

  // Temporary array used when computing live_in, live_out, and kill sets.
  GrowableArray<HInstruction*> instructions_from_ssa_index_;

  // Temporary array used when inserting moves in the graph.
  GrowableArray<HInstruction*> instructions_from_lifetime_position_;
  size_t number_of_ssa_values_;

  DISALLOW_COPY_AND_ASSIGN(SsaLivenessAnalysis);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_SSA_LIVENESS_ANALYSIS_H_
