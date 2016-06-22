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
#include "base/arena_object.h"
#include "base/macros.h"
#include "primitive.h"
#include "register_allocator.h"

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
  ~RegisterAllocatorGraphColor() OVERRIDE {}

  void AllocateRegisters() OVERRIDE;

  bool Validate(bool log_fatal_on_failure);

 private:
  // Collect all intervals and prepare for register allocation.
  void ProcessInstructions();
  void ProcessInstruction(HInstruction* instruction);

  // Collect all live intervals associated with the temporary locations
  // needed by an instruction.
  void CheckForTempLiveIntervals(HInstruction* instruction);

  // If any inputs require specific registers, block those registers
  // at the position of this instruction.
  void CheckForFixedInputs(HInstruction* instruction);

  // If the output of an instruction requires a specific register, split
  // the interval and assign the register to the first part.
  void CheckForFixedOutput(HInstruction* instruction);

  // If a safe point is needed, add a synthesized interval to later record
  // the number of live registers at this point.
  void CheckForSafepoint(HInstruction* instruction);

  // Add all applicable safepoints to a live interval.
  // Currently depends on instruction processing order.
  void AddSafepointsFor(HInstruction* instruction);

  // Split an interval, but only if `position` is inside of `interval`.
  // Returns either the new interval, or the original interval if not split.
  static LiveInterval* TrySplit(LiveInterval* interval, size_t position);

  // To ensure every graph can be colored, split live intervals
  // at their register defs and uses. This creates short intervals with low
  // degree in the interference graph, which are prioritized during graph
  // coloring.
  void SplitAtRegisterUses(LiveInterval* interval);

  // If the given instruction is a catch phi, give it a spill slot.
  void AllocateSpillSlotForCatchPhi(HInstruction* instruction);

  // Ensure that the given register cannot be allocated for a given range.
  void BlockRegister(Location location, size_t start, size_t end);
  void BlockRegisters(size_t start, size_t end, bool caller_save_only = false);

  // Use the intervals collected from instructions to construct an
  // interference graph mapping intervals to adjacency lists.
  // Also, collect synthesized safepoint nodes, used to keep
  // track of live intervals across safepoints.
  void BuildInterferenceGraph(const ArenaVector<LiveInterval*>& intervals,
                              ArenaVector<InterferenceNode*>* prunable_nodes,
                              ArenaVector<InterferenceNode*>* safepoints);

  // Prune nodes from the interference graph to be colored later. Builds
  // a stack (pruned_nodes) containing these intervals in an order determined
  // by various heuristics.
  void PruneInterferenceGraph(const ArenaVector<InterferenceNode*>& prunable_nodes,
                              size_t num_registers,
                              ArenaStdStack<InterferenceNode*>* pruned_nodes);

  // Process pruned_intervals_ to color the interference graph, spilling when
  // necessary. Returns true if successful. Else, some intervals have been
  // split, and the interference graph should be rebuilt for another attempt.
  bool ColorInterferenceGraph(ArenaStdStack<InterferenceNode*>* pruned_nodes,
                              size_t num_registers);

  // Updates max safepoint live registers based on the outgoing interference
  // edges of safepoint nodes, after assigning colors.
  size_t ComputeMaxSafepointLiveRegisters(const ArenaVector<InterferenceNode*>& safepoints);

  // If necessary, add the given interval to the list of spilled intervals,
  // and make sure it's ready to be spilled to the stack.
  void AllocateSpillSlotFor(LiveInterval* interval);

  // Live intervals, split by kind (core and floating point).
  // These should not contain high intervals, as those are represented by
  // the corresponding low interval throughout register allocation.
  ArenaVector<LiveInterval*> core_intervals_, fp_intervals_;

  // Intervals for temporaries, saved for special handling in the
  // resolution phase.
  ArenaVector<LiveInterval*> temp_intervals_;

  // Safepoints are saved for special handling while processing instructions.
  ArenaVector<HInstruction*> safepoints_;

  // Live intervals for specific registers. These will become pre-colored nodes
  // in the interference graph.
  ArenaVector<LiveInterval*> physical_core_intervals_, physical_fp_intervals_;

  // Keeps track of allocated stack slots.
  size_t int_spill_slot_counter_;
  size_t double_spill_slot_counter_;
  size_t float_spill_slot_counter_;
  size_t long_spill_slot_counter_;
  size_t catch_phi_spill_slot_counter_;

  // Number of stack slots needed for the pointer to the current method.
  // This is 1 for 32-bit architectures, and 2 for 64-bit architectures.
  const size_t reserved_art_method_slots_;

  // Number of stack slots needed for outgoing arguments.
  const size_t reserved_out_slots_;

  // The maximum number of registers live at safe points. Needed by the
  // code generator.
  size_t max_safepoint_live_core_regs_;
  size_t max_safepoint_live_fp_regs_;

  // An arena allocator used for each round of graph coloring, since there could be several.
  ArenaAllocator* coloring_attempt_allocator_;

  DISALLOW_COPY_AND_ASSIGN(RegisterAllocatorGraphColor);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_REGISTER_ALLOCATOR_GRAPH_COLOR_H_
