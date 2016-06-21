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

#ifndef ART_COMPILER_OPTIMIZING_REGISTER_ALLOCATOR_LINEAR_SCAN_H_
#define ART_COMPILER_OPTIMIZING_REGISTER_ALLOCATOR_LINEAR_SCAN_H_

#include <optimizing/register_allocator.h>
#include "arch/instruction_set.h"
#include "base/arena_containers.h"
#include "base/macros.h"
#include "primitive.h"

namespace art {

class CodeGenerator;
class HBasicBlock;
class HGraph;
class HInstruction;
class HPhi;
class LiveInterval;
class SsaLivenessAnalysis;

/**
 * An implementation of a linear scan register allocator on an `HGraph` with SSA form.
 */
class RegisterAllocatorLinearScan : public RegisterAllocator {
 public:
  RegisterAllocatorLinearScan(ArenaAllocator* allocator,
                              CodeGenerator* codegen,
                              const SsaLivenessAnalysis& analysis);

 private:
  // Main methods of the allocator.
  void AllocateRegistersInternal() override;
  void LinearScan();
  bool TryAllocateFreeReg(LiveInterval* interval);
  bool AllocateBlockedReg(LiveInterval* interval);

  // Helper methods.
  void DumpAllIntervals(std::ostream& stream) const;
  int FindAvailableRegisterPair(size_t* next_use, size_t starting_at) const;
  int FindAvailableRegister(size_t* next_use, LiveInterval* current) const;

  // Try splitting an active non-pair or unaligned pair interval at the given `position`.
  // Returns whether it was successful at finding such an interval.
  bool TrySplitNonPairOrUnalignedPairIntervalAt(size_t position,
                                                size_t first_register_use,
                                                size_t* next_use);

  // List of intervals for core registers that must be processed, ordered by start
  // position. Last entry is the interval that has the lowest start position.
  // This list is initially populated before doing the linear scan.
  ArenaVector<LiveInterval*> unhandled_core_intervals_;

  // List of intervals for floating-point registers. Same comments as above.
  ArenaVector<LiveInterval*> unhandled_fp_intervals_;

  // Currently processed list of unhandled intervals. Either `unhandled_core_intervals_`
  // or `unhandled_fp_intervals_`.
  ArenaVector<LiveInterval*>* unhandled_;

  // List of intervals that have been processed.
  ArenaVector<LiveInterval*> handled_;

  // List of intervals that are currently active when processing a new live interval.
  // That is, they have a live range that spans the start of the new interval.
  ArenaVector<LiveInterval*> active_;

  // List of intervals that are currently inactive when processing a new live interval.
  // That is, they have a lifetime hole that spans the start of the new interval.
  ArenaVector<LiveInterval*> inactive_;

  ART_FRIEND_TEST(RegisterAllocatorTest, FreeUntil);
  ART_FRIEND_TEST(RegisterAllocatorTest, SpillInactive);

 private:
  DISALLOW_COPY_AND_ASSIGN(RegisterAllocatorLinearScan);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_REGISTER_ALLOCATOR_LINEAR_SCAN_H_
