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
#include "register_allocator_common.h"

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
                              ArenaVector<InterferenceNode*>& pruned_nodes);

  // Use pruned_intervals_ to color the interference graph, spilling when
  // necessary. Returns true if successful. Else, some intervals have been
  // split, and the interference graph should be rebuilt for another attempt.
  bool ColorInterferenceGraph(ArenaVector<InterferenceNode*>& pruned_nodes,
                              size_t num_registers,
                              bool processing_core_regs);

  // If necessary, add the given interval to the list of spilled intervals,
  // and make sure it's ready to be spilled to the stack.
  void AllocateSpillSlotFor(LiveInterval* interval);

  // Set final locations in each location summary, and deconstruct SSA form.
  void Resolve();

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

  std::string DumpInterval(const LiveInterval* interval) const;
  std::string DumpInterferenceGraph() const;

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

// Interference nodes make up the interference graph, which is the primary
// data structure in graph coloring register allocation.
class InterferenceNode : public ArenaObject<kArenaAllocRegisterAllocator> {
 public:
  InterferenceNode(ArenaAllocator* allocator,
                   LiveInterval* interval,
                   size_t id)
        : interval_(interval),
          adj_(CmpPtr, allocator->Adapter(kArenaAllocRegisterAllocator)),
          degree_(0),
          id_(id) {}

  // Use to maintain determinism when storing InterferenceNode pointers in sets.
  static bool CmpPtr(const InterferenceNode* lhs, const InterferenceNode* rhs) {
    return lhs->id_ < rhs->id_;
  }

  void AddInterference(InterferenceNode* other) {
    if (adj_.insert(other).second) {
      degree_ += EdgeWeightWith(other);
    }
  }
  void RemoveInterference(InterferenceNode* other) {
    if (adj_.erase(other) > 0) {
      degree_ -= EdgeWeightWith(other);
    }
  }
  bool ContainsInterference(InterferenceNode* other) const {
    return adj_.count(other) > 0;
  }

  LiveInterval* Interval() const {
    return interval_;
  }
  const ArenaSet<InterferenceNode*, decltype(&CmpPtr)>& Adj() const {
    return adj_;
  }
  size_t Degree() const {
    return degree_;
  }
  size_t Id() const {
    return id_;
  }

 private:
  // In order to model the constraints imposed by register pairs, we give
  // extra weight to edges adjacent to register pair nodes.
  size_t EdgeWeightWith(InterferenceNode* other) const;

  // The live interval that this node represents.
  LiveInterval* const interval_;

  // All nodes interfering with this one.
  ArenaSet<InterferenceNode*, decltype(&CmpPtr)> adj_;

  // We cannot use adjacency set size for degree, since that ignores nodes
  // representing pair intervals.
  size_t degree_;

  // A unique identifier for this node.
  const size_t id_;
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_REGISTER_ALLOCATOR_GRAPH_COLOR_H_
