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

#include "register_allocator.h"

#include "arch/instruction_set.h"
#include "base/arena_containers.h"
#include "base/arena_object.h"
#include "base/macros.h"
#include "primitive.h"

namespace art {

class CodeGenerator;
class HBasicBlock;
class HGraph;
class HInstruction;
class HParallelMove;
class Location;
class SsaLivenessAnalysis;
class InterferenceNode;

/**
 * A graph coloring register allocator.
 */
class RegisterAllocatorGraphColor : public RegisterAllocator {
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
  void CheckForTempLiveIntervals(HInstruction* instruction);

  // If any input require specific registers, block those registers
  // at the position of this instruction.
  void CheckForFixedInputs(HInstruction* instruction);

  // If the output of an instruction requires a specific register, let
  // the interval know about it.
  void CheckForFixedOutput(HInstruction* instruction);

  // If a safe point is needed, remove the suspend check before register
  // allocation and add interval for the slow path if necessary.
  void CheckForSafepoint(HInstruction* instruction);

  // Split an interval, but only if `position` is inside of `interval`.
  // Returns either the new interval, or the original interval if not split.
  static LiveInterval* TrySplit(LiveInterval* interval, size_t position);

  // To ensure every graph can be colored, split live intervals
  // at their register defs and uses. (This creates short intervals with low
  // degree in the interference graph.) These intervals are marked as requiring
  // registers so that they can be pruned last from the interference graph.
  void SplitAtRegisterUses(LiveInterval* interval);

  // Process all safe points seen so far, given the current instruction.
  // Currently depends on the ordering of the instructions.
  void ProcessSafepointsFor(HInstruction* instruction);

  // If the given instruction is a catch phi, give it a spill slot.
  void AllocateSpillSlotForCatchPhi(HInstruction* instruction);

  // Ensure that the given register cannot be allocated for a given range.
  void BlockRegister(Location location, size_t start, size_t end);
  void BlockRegisters(size_t start, size_t end, bool caller_save_only = false);

  // Use the intervals collected from instructions to construct an
  // interference graph mapping intervals to adjacency lists.
  void BuildInterferenceGraph(ArenaVector<LiveInterval*>& intervals,
                              ArenaVector<InterferenceNode*>& interference_graph);

  // Prune nodes from the interference graph to be colored later. Returns
  // a stack containing these intervals in an order determined by various
  // heuristics.
  void PruneInterferenceGraph(ArenaVector<InterferenceNode*>& interference_graph,
                              size_t num_registers,
                              ArenaStdStack<InterferenceNode*>& pruned_nodes);

  // Use pruned_intervals_ to color the interference graph, spilling when
  // necessary. Returns true if successful. Else, some intervals have been
  // split, and the interference graph should be rebuilt for another attempt.
  bool ColorInterferenceGraph(ArenaStdStack<InterferenceNode*>& pruned_nodes,
                              size_t num_registers,
                              bool processing_core_regs);

  // If necessary, add the given interval to the list of spilled intervals,
  // and make sure it's ready to be spilled to the stack.
  void AllocateSpillSlotFor(LiveInterval* interval);

  // Set final locations in each location summary, and deconstruct SSA form.
  void Resolve();

  std::string DumpInterval(const LiveInterval* interval) const;

  // TODO: Make some of the member variables below local variables instead
  //       in order to better show the dependencies among different functions.

  // Live intervals, split by kind (core and floating point).
  // These should not contain high intervals, as those are represented by
  // the corresponding low interval throughout register allocation.
  ArenaVector<LiveInterval*> core_intervals_, fp_intervals_;

  // Intervals for temporaries, saved for special handling in the
  // resolution phase.
  ArenaVector<LiveInterval*> temp_intervals_;

  // Safe points are saved for special handling while processing instructions.
  ArenaVector<HInstruction*> safepoints_;

  // Live intervals for specific registers. These will become pre-colored nodes
  // in the interference graph.
  ArenaVector<LiveInterval*> physical_core_intervals_, physical_fp_intervals_;

  // Keeps track of allocated stack slots
  int spill_slot_counter_;

  // Number of stack slots needed for the pointer to the current method.
  size_t reserved_art_method_slots_;

  // Number of stack slots needed for outgoing arguments.
  size_t reserved_out_slots_;

  // The maximum number of registers live at safe points. Needed by
  // codegen_->InitializeCodeGeneration(...)
  size_t max_safepoint_live_core_regs_;
  size_t max_safepoint_live_fp_regs_;

  DISALLOW_COPY_AND_ASSIGN(RegisterAllocatorGraphColor);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_REGISTER_ALLOCATOR_GRAPH_COLOR_H_
