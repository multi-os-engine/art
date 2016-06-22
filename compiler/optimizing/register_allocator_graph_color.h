/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ART_COMPILER_OPTIMIZING_REGISTER_ALLOCATOR_GRAPH_COLOR_H_
#define ART_COMPILER_OPTIMIZING_REGISTER_ALLOCATOR_GRAPH_COLOR_H_

#include "arch/instruction_set.h"
#include "base/arena_containers.h"
#include "base/macros.h"
#include "primitive.h"
#include "register_allocator_common.h"
#include <unordered_map>
#include <unordered_set>

namespace art {

class CodeGenerator;
class HBasicBlock;
class HGraph;
class HInstruction;
class HParallelMove;
class Location;
class SsaLivenessAnalysis;

/**
 * A graph coloring register allocator.
 */
class RegisterAllocatorGraphColor : public RegisterAllocatorCommon {
 public:
  RegisterAllocatorGraphColor(ArenaAllocator* allocator,
                              CodeGenerator* codegen,
                              const SsaLivenessAnalysis& analysis);

  void AllocateRegisters() override;

  bool Validate(bool log_fatal_on_failure ATTRIBUTE_UNUSED) override {
    return true;  // TODO
  }

 private:
  // Collect all intervals and prepare for register allocation.
  void ProcessInstructions();
  void ProcessInstruction(HInstruction* instruction);

  // Collect all live intervals associated with the temporary locations
  // needed by an instruction.
  void CollectTempLiveIntervals(HInstruction* instruction);

  // If a safe point is needed, remove the suspend check before register
  // allocation and add interval for the slow path if necessary.
  void CheckLocationsForSafepoint(HInstruction* instruction);

  // If an instruction requires inputs in specific registers, create
  // fixed intervals for those inputs.
  void ProcessFixedInputLocations(HInstruction* instruction);

  // If an instruction requires its output in a specific registers, create
  // a fixed interval for that output.
  void ProcessFixedOutputLocations(HInstruction* instruction);

  // Ensure that the given register cannot be allocated for a given range.
  void BlockRegister(Location location, size_t start, size_t end);
  void BlockRegisters(size_t start, size_t end, bool caller_save_only = false);

  // Use the intervals collected from instructions to construct an
  // interference graph mapping intervals to adjacency lists.
  void BuildInterferenceGraph(ArenaVector<LiveInterval*>& intervals);

  // Simplify the interference graph (i.e., prune nodes to be colored later,
  // pruning low-degree nodes first). Pruned nodes go into pruned_intervals_.
  void PruneInterferenceGraph(size_t num_registers);

  // Use pruned_intervals_ to color the interference graph, spilling when
  // necessary.
  void ColorInterferenceGraph(size_t num_registers);

  // Set final locations in each location summary, and deconstruct SSA form.
  void Resolve();

  // Split `interval` at the position `position`. The new interval starts at `position`.
  LiveInterval* Split(LiveInterval* interval, size_t position);

  // Split `interval` at a position between `from` and `to`. The method will try
  // to find an optimal split position.
  LiveInterval* SplitBetween(LiveInterval* interval, size_t from, size_t to);

  void InsertParallelMoveAt(size_t position,
                            HInstruction* instruction,
                            Location source,
                            Location destination) const;
  void ConnectSiblings(LiveInterval* interval);
  void ConnectSplitSiblings(LiveInterval* interval,
                            HBasicBlock* from,
                            HBasicBlock* to) const;
  void AddMove(HParallelMove* move,
               Location source,
               Location destination,
               HInstruction* instruction,
               Primitive::Type type) const;
  void AddInputMoveFor(HInstruction* input,
                       HInstruction* user,
                       Location source,
                       Location destination) const;
  void InsertMoveAfter(HInstruction* instruction,
                       Location source,
                       Location destination) const;
  void InsertParallelMoveAtEntryOf(HBasicBlock* block,
                                   HInstruction* instruction,
                                   Location source,
                                   Location destination) const;
  void InsertParallelMoveAtExitOf(HBasicBlock* block,
                                  HInstruction* instruction,
                                  Location source,
                                  Location destination) const;

  // Live intervals, split by kind (core and floating point).
  ArenaVector<LiveInterval*> core_intervals_, fp_intervals_;

  // Intervals for temporaries. Such intervals cover the positions
  // where an instruction requires a temporary.
  ArenaVector<LiveInterval*> temp_intervals_;

  // Live intervals for specific registers. These will become pre-colored nodes
  // in the interference graph.
  // TODO: Factor out into register_allocator_common
  ArenaVector<LiveInterval*> physical_core_intervals_, physical_fp_intervals_;

  // The interference graph constructed from live intervals.
  // TODO: We should use an ArenaMap / ArenaSet, but they seem to allow duplicates(?)
  using interference_graph_t =
      std::unordered_map<LiveInterval*, std::unordered_set<LiveInterval*>>;
  interference_graph_t interference_graph_;

  // A stack of intervals pruned from the interference graph, with low-degree
  // nodes pruned first, per the standard graph coloring heuristic.
  ArenaVector<LiveInterval*> pruned_intervals_;
  ArenaVector<LiveInterval*> spilled_intervals_;

  // TODO: Factor out into register_allocator_common;
  size_t reserved_out_slots_;

  DISALLOW_COPY_AND_ASSIGN(RegisterAllocatorGraphColor);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_REGISTER_ALLOCATOR_GRAPH_COLOR_H_
