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

#include "register_allocator_graph_color.h"

#include "code_generator.h"
#include "ssa_liveness_analysis.h"
#include "thread-inl.h"

namespace art {

// TODO
std::string RegisterAllocatorGraphColor::DumpInterval(const LiveInterval* interval) const {
  std::stringstream s;
  interval->Dump(s);
  if (interval->IsFixed()) {
    s << ", register:" << interval->GetRegister() << "(";
    if (interval->IsFloatingPoint()) {
      codegen_->DumpFloatingPointRegister(s, interval->GetRegister());
    } else {
      codegen_->DumpCoreRegister(s, interval->GetRegister());
    }
    s << ")";
  } else {
    s << ", spill slot:" << interval->GetSpillSlot();
  }
  s << ", requires_register:" << (interval->GetDefinedBy() != nullptr && interval->RequiresRegister());
  if (interval->GetParent()->GetDefinedBy() != nullptr) {
    s << ", defined_by:" << interval->GetParent()->GetDefinedBy()->GetKind()
        << "(" << interval->GetParent()->GetDefinedBy()->GetLifetimePosition() << ")";
  }
  return s.str();
}

RegisterAllocatorGraphColor::RegisterAllocatorGraphColor(ArenaAllocator* allocator,
                                                         CodeGenerator* codegen,
                                                         const SsaLivenessAnalysis& liveness)
      : RegisterAllocator(allocator, codegen, liveness),
        core_intervals_(allocator->Adapter(kArenaAllocRegisterAllocator)),
        fp_intervals_(allocator->Adapter(kArenaAllocRegisterAllocator)),
        temp_intervals_(allocator->Adapter(kArenaAllocRegisterAllocator)),
        safepoints_(allocator->Adapter(kArenaAllocRegisterAllocator)),
        physical_core_intervals_(allocator->Adapter(kArenaAllocRegisterAllocator)),
        physical_fp_intervals_(allocator->Adapter(kArenaAllocRegisterAllocator)),
        spill_slot_counter_(0),
        max_safepoint_live_core_regs_(0),
        max_safepoint_live_fp_regs_(0) {
  codegen->SetupBlockedRegisters();

  // Initialize physical core register live intervals and blocked registers.
  // This includes globally blocked registers, such as the stack pointer.
  physical_core_intervals_.resize(codegen->GetNumberOfCoreRegisters(), nullptr);
  for (size_t i = 0; i < codegen->GetNumberOfCoreRegisters(); ++i) {
    LiveInterval* interval = LiveInterval::MakeFixedInterval(allocator_, i, Primitive::kPrimInt);
    physical_core_intervals_[i] = interval;
    core_intervals_.push_back(interval);
    if (codegen_->GetBlockedCoreRegisters()[i]) {
      interval->AddRange(0, liveness.GetMaxLifetimePosition());
    }
  }
  // Initialize physical floating point register live intervals and blocked registers.
  physical_fp_intervals_.resize(codegen->GetNumberOfFloatingPointRegisters(), nullptr);
  for (size_t i = 0; i < codegen->GetNumberOfFloatingPointRegisters(); ++i) {
    LiveInterval* interval = LiveInterval::MakeFixedInterval(allocator_, i, Primitive::kPrimFloat);
    physical_fp_intervals_[i] = interval;
    fp_intervals_.push_back(interval);
    if (codegen_->GetBlockedFloatingPointRegisters()[i]) {
      interval->AddRange(0, liveness.GetMaxLifetimePosition());
    }
  }

  reserved_art_method_slots_ = InstructionSetPointerSize(codegen->GetInstructionSet()) / kVRegSize;
  reserved_out_slots_ = codegen->GetGraph()->GetMaximumNumberOfOutVRegs();
}

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
  size_t EdgeWeightWith(InterferenceNode* other) const {
    return (interval_->HasHighInterval() || other->interval_->HasHighInterval()) ? 2 : 1;
  }

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

// TODO: Decide on a limit.
static constexpr int kMaxGraphColoringAttempts = 100;

void RegisterAllocatorGraphColor::AllocateRegisters() {
  ProcessInstructions();

  for (bool processing_core_regs : {true, false}) {
    ArenaVector<LiveInterval*>& intervals = processing_core_regs
        ? core_intervals_
        : fp_intervals_;
    size_t num_registers = processing_core_regs
        ? codegen_->GetNumberOfCoreRegisters()
        : codegen_->GetNumberOfFloatingPointRegisters();

    int attempt = 0;
    bool successful = false;
    while (!successful && attempt++ < kMaxGraphColoringAttempts) {
      // (1) Build interference graph.
      ArenaVector<InterferenceNode*> interference_graph(
          allocator_->Adapter(kArenaAllocRegisterAllocator));
      BuildInterferenceGraph(intervals, interference_graph);

      // (2) Prune all uncolored nodes from interference graph.
      // TODO: This should have a stack interface.
      ArenaVector<InterferenceNode*> pruned_nodes(
          allocator_->Adapter(kArenaAllocRegisterAllocator));
      PruneInterferenceGraph(interference_graph, num_registers, pruned_nodes);

      // (3) Color pruned nodes based on interferences.
      successful = ColorInterferenceGraph(pruned_nodes,
                                          num_registers,
                                          processing_core_regs);
    }

    // Tell the code generator which registers have been allocated.
    // TODO: Right now this will also include blocked registers, yet
    //       the linear scan implementation does not seem to include these
    //       at times (e.g., fixed inputs, blocked regs for calls, etc.).
    //       What is the correct approach here?
    //       Note that if we go back to excluding blocked registers, we
    //       will still need to include blocked temporaries as a special case.
    for (LiveInterval* parent : intervals) {
      for (LiveInterval* sibling = parent; sibling != nullptr; sibling = sibling->GetNextSibling()) {
        if (sibling->HasRegister()) {
          Location low_reg = processing_core_regs
              ? Location::RegisterLocation(sibling->GetRegister())
              : Location::FpuRegisterLocation(sibling->GetRegister());
          codegen_->AddAllocatedRegister(low_reg);
          if (sibling->HasHighInterval()) {
            LiveInterval* high = sibling->GetHighInterval();
            DCHECK(high->HasRegister());
            Location high_reg = processing_core_regs
                ? Location::RegisterLocation(high->GetRegister())
                : Location::RegisterLocation(high->GetRegister());
            codegen_->AddAllocatedRegister(high_reg);
          }
        }
      }
    }

    DCHECK(successful) << "Exceeded max register allocation graph coloring attempts";
  }

  Resolve();
}

// Split `interval` at the position `position`. The new interval starts at `position`.
static LiveInterval* Split(LiveInterval* interval,
                           size_t position) {
  DCHECK_GT(position, interval->GetStart());
  DCHECK(!interval->IsDeadAt(position)) << position;
  LiveInterval* new_interval = interval->SplitAt(position);
  DCHECK(new_interval != nullptr);
  if (interval->HasHighInterval()) {
    LiveInterval* high = interval->GetHighInterval()->SplitAt(position);
    new_interval->SetHighInterval(high);
    high->SetLowInterval(new_interval);
  } else if (interval->HasLowInterval()) {
    LiveInterval* low = interval->GetLowInterval()->SplitAt(position);
    new_interval->SetLowInterval(low);
    low->SetHighInterval(new_interval);
  }
  return new_interval;
}

void RegisterAllocatorGraphColor::ProcessInstructions() {
  for (HLinearPostOrderIterator it(*codegen_->GetGraph()); !it.Done(); it.Advance()) {
    HBasicBlock* block = it.Current();

    // TODO: Might be able to iterate using just Ssa indices.
    //       Right now, though, some helper code (e.g., AddRange in
    //       ssa_liveness_analysis, and also safepoint handling,
    //       depends on the ordering).

    for (HBackwardInstructionIterator instr_it(block->GetInstructions());
          !instr_it.Done(); instr_it.Advance()) {
      ProcessInstruction(instr_it.Current());
    }

    for (HInstructionIterator phi_it(block->GetPhis()); !phi_it.Done(); phi_it.Advance()) {
      ProcessInstruction(phi_it.Current());
    }

    if (block->IsCatchBlock() || (block->IsLoopHeader() && block->GetLoopInformation()->IsIrreducible())) {
      // By blocking all registers at the top of each catch block or irreducible loop, we force
      // intervals belonging to the live-in set of the catch/header block to be spilled.
      // TODO(ngeoffray): Phis in this block could be allocated in register.
      size_t position = block->GetLifetimeStart();
      BlockRegisters(position, position + 1);
    }
  }
}

void RegisterAllocatorGraphColor::ProcessInstruction(HInstruction* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  if (locations == nullptr) {
    return;
  }
  if (locations->NeedsSafepoint() && codegen_->IsLeafMethod()) {
    // TODO: We do this here because we do not want the suspend check to artificially
    // create live registers. We should find another place, but this is currently the
    // simplest.
    DCHECK(instruction->IsSuspendCheckEntry());
    DCHECK_EQ(locations->GetTempCount(), 0u);
    instruction->GetBlock()->RemoveInstruction(instruction);
    return;
  }

  CheckForTempLiveIntervals(instruction);
  CheckForSafepoint(instruction);

  // If a call will happen, create fixed intervals for caller-save registers.
  if (instruction->GetLocations()->WillCall()) {
    BlockRegisters(instruction->GetLifetimePosition(),
                   instruction->GetLifetimePosition() + 1,
                   /*caller_save_only*/ true);
  }

  CheckForFixedInputs(instruction);

  LiveInterval* interval = instruction->GetLiveInterval();
  if (interval == nullptr) {
    return;
  }

  DCHECK(!interval->IsHighInterval());
  if (codegen_->NeedsTwoRegisters(interval->GetType())) {
    interval->AddHighInterval();
  }

  ProcessSafepointsFor(instruction);
  CheckForFixedOutput(instruction);
  AllocateSpillSlotForCatchPhi(instruction);

  bool core_register = (instruction->GetType() != Primitive::kPrimDouble)
                    && (instruction->GetType() != Primitive::kPrimFloat);
  ArenaVector<LiveInterval*>& intervals = core_register
      ? core_intervals_
      : fp_intervals_;

  if (interval->HasSpillSlot() || instruction->IsConstant()) {
    if (interval->FirstRegisterUse() == kNoLifetime) {
      // We won't allocate a register for this value.
    } else {
      // TODO: Review this.
      LiveInterval* split = SplitBetween(interval,
                                         interval->GetStart(),
                                         interval->FirstRegisterUse() - 1);
      intervals.push_back(split);
    }
  } else {
    intervals.push_back(interval);
  }
}

void RegisterAllocatorGraphColor::CheckForFixedInputs(HInstruction* instruction) {
  // We simply block physical registers where necessary.
  // TODO: Ideally we would coalesce the physical register with the register
  //       allocated to the input value, but this can be tricky if, e.g., there
  //       could be multiple physical register uses of the same value at the
  //       same instruction. Need to think about it more.
  //       One idea is to just assign the interval (after we split it at uses)
  //       to one of the physical registers, then just block the other.
  //       ConnectSiblings should then take care of the rest.
  LocationSummary* locations = instruction->GetLocations();
  size_t position = instruction->GetLifetimePosition();
  for (size_t i = 0; i < locations->GetInputCount(); ++i) {
    Location input = locations->InAt(i);
    if (input.IsRegister() || input.IsFpuRegister()) {
      BlockRegister(input, position, position + 1);
    } else if (input.IsPair()) {
      BlockRegister(input.ToLow(), position, position + 1);
      BlockRegister(input.ToHigh(), position, position + 1);
    }
  }
}

void RegisterAllocatorGraphColor::CheckForFixedOutput(HInstruction* instruction) {
  LiveInterval* interval = instruction->GetLiveInterval();
  Location out = interval->GetDefinedBy()->GetLocations()->Out();
  size_t position = instruction->GetLifetimePosition();
  DCHECK_GE(interval->GetEnd() - position, 2u);  // TODO: Valid assumption?
  if (out.IsUnallocated() && out.GetPolicy() == Location::kSameAsFirstInput) {
    Location first = instruction->GetLocations()->InAt(0);
    if (first.IsRegister() || first.IsFpuRegister()) {
      interval->SetRegister(first.reg());
      Split(interval, position + 1);
    } else if (first.IsPair()) {
      interval->SetRegister(first.low());
      LiveInterval* high = interval->GetHighInterval();
      high->SetRegister(first.high());
      Split(interval, position + 1);
    }
  } else if (out.IsRegister() || out.IsFpuRegister()) {
    interval->SetRegister(out.reg());
    Split(interval, position + 1);
  } else if (out.IsPair()) {
    interval->SetRegister(out.low());
    LiveInterval* high = interval->GetHighInterval();
    high->SetRegister(out.high());
    Split(interval, position + 1);
  } else if (out.IsStackSlot() || out.IsDoubleStackSlot()) {
    interval->SetSpillSlot(out.GetStackIndex());
  } else {
    DCHECK(out.IsUnallocated() || out.IsConstant());
  }
}

// TODO: Factor out into register_allocator_common, or (more likely), change
//       it here so it's not dependent on instruction order.
void RegisterAllocatorGraphColor::ProcessSafepointsFor(HInstruction* instruction) {
  LiveInterval* interval = instruction->GetLiveInterval();
  for (size_t safepoint_index = safepoints_.size(); safepoint_index > 0; --safepoint_index) {
    HInstruction* safepoint = safepoints_[safepoint_index - 1u];
    size_t safepoint_position = safepoint->GetLifetimePosition();

    // Test that safepoints_ are ordered in the optimal way.
    DCHECK(safepoint_index == safepoints_.size() ||
           safepoints_[safepoint_index]->GetLifetimePosition() < safepoint_position);

    if (safepoint_position == interval->GetStart()) {
      // The safepoint is for this instruction, so the location of the instruction
      // does not need to be saved.
      DCHECK_EQ(safepoint_index, safepoints_.size());
      DCHECK_EQ(safepoint, instruction);
      continue;
    } else if (interval->IsDeadAt(safepoint_position)) {
      break;
    } else if (!interval->Covers(safepoint_position)) {
      // Hole in the interval.
      continue;
    }
    interval->AddSafepoint(safepoint);
  }
  interval->ResetSearchCache();
}

// TODO: Factor out into register_allocator_common
LiveInterval* RegisterAllocatorGraphColor::SplitBetween(LiveInterval* interval,
                                                        size_t from,
                                                        size_t to) {
  HBasicBlock* block_from = liveness_.GetBlockFromPosition(from / 2);
  HBasicBlock* block_to = liveness_.GetBlockFromPosition(to / 2);
  DCHECK(block_from != nullptr);
  DCHECK(block_to != nullptr);

  // Both locations are in the same block. We split at the given location.
  if (block_from == block_to) {
    return Split(interval, to);
  }

  /*
   * Non-linear control flow will force moves at every branch instruction to the new location.
   * To avoid having all branches doing the moves, we find the next non-linear position and
   * split the interval at this position. Take the following example (block number is the linear
   * order position):
   *
   *     B1
   *    /  \
   *   B2  B3
   *    \  /
   *     B4
   *
   * B2 needs to split an interval, whose next use is in B4. If we were to split at the
   * beginning of B4, B3 would need to do a move between B3 and B4 to ensure the interval
   * is now in the correct location. It makes performance worst if the interval is spilled
   * and both B2 and B3 need to reload it before entering B4.
   *
   * By splitting at B3, we give a chance to the register allocator to allocate the
   * interval to the same register as in B1, and therefore avoid doing any
   * moves in B3.
   */
  if (block_from->GetDominator() != nullptr) {
    for (HBasicBlock* dominated : block_from->GetDominator()->GetDominatedBlocks()) {
      size_t position = dominated->GetLifetimeStart();
      if ((position > from) && (block_to->GetLifetimeStart() > position)) {
        // Even if we found a better block, we continue iterating in case
        // a dominated block is closer.
        // Note that dominated blocks are not sorted in liveness order.
        block_to = dominated;
        DCHECK_NE(block_to, block_from);
      }
    }
  }

  // If `to` is in a loop, find the outermost loop header which does not contain `from`.
  for (HLoopInformationOutwardIterator it(*block_to); !it.Done(); it.Advance()) {
    HBasicBlock* header = it.Current()->GetHeader();
    if (block_from->GetLifetimeStart() >= header->GetLifetimeStart()) {
      break;
    }
    block_to = header;
  }

  // Split at the start of the found block, to piggy back on existing moves
  // due to resolution if non-linear control flow (see `ConnectSplitSiblings`).
  return Split(interval, block_to->GetLifetimeStart());
}

// TODO: Factor out into register_allocator_common if possible
void RegisterAllocatorGraphColor::CheckForTempLiveIntervals(HInstruction* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  size_t position = instruction->GetLifetimePosition();
  for (size_t i = 0; i < locations->GetTempCount(); ++i) {
    Location temp = locations->GetTemp(i);
    if (temp.IsRegister() || temp.IsFpuRegister()) {
      BlockRegister(temp, position, position + 1);
    } else {
      DCHECK(temp.IsUnallocated());
      switch (temp.GetPolicy()) {
        case Location::kRequiresRegister: {
          LiveInterval* interval =
              LiveInterval::MakeTempInterval(allocator_, Primitive::kPrimInt);
          interval->AddTempUse(instruction, i);
          core_intervals_.push_back(interval);
          temp_intervals_.push_back(interval);
          break;
        }

        case Location::kRequiresFpuRegister: {
          LiveInterval* interval =
              LiveInterval::MakeTempInterval(allocator_, Primitive::kPrimDouble);
          interval->AddTempUse(instruction, i);
          fp_intervals_.push_back(interval);
          temp_intervals_.push_back(interval);
          if (codegen_->NeedsTwoRegisters(Primitive::kPrimDouble)) {
            interval->AddHighInterval(/*is_temp*/ true);
            temp_intervals_.push_back(interval->GetHighInterval());
          }
          break;
        }

        default:
          LOG(FATAL) << "Unexpected policy for temporary location "
                     << temp.GetPolicy();
      }
    }
  }
}

// TODO: Factor out into register_allocator_common
void RegisterAllocatorGraphColor::CheckForSafepoint(HInstruction* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  size_t position = instruction->GetLifetimePosition();

  if (locations->NeedsSafepoint()) {
    safepoints_.push_back(instruction);
    if (locations->OnlyCallsOnSlowPath()) {
      // We add a synthesized range at this position to record the live registers
      // at this position. Ideally, we could just update the safepoints when locations
      // are updated, but we currently need to know the full stack size before updating
      // locations (because of parameters and the fact that we don't have a frame pointer).
      // And knowing the full stack size requires to know the maximum number of live
      // registers at calls in slow paths.
      // By adding the following interval in the algorithm, we can compute this
      // maximum before updating locations.
      LiveInterval* interval = LiveInterval::MakeSlowPathInterval(allocator_, instruction);
      interval->AddRange(position, position + 1);
      core_intervals_.push_back(interval);
      fp_intervals_.push_back(interval);
    }
  }
}

// Split an interval, but only if `position` is inside of `interval`.
// Returns either the new interval, or the original interval if not split.
static LiveInterval* TrySplit(LiveInterval* interval, size_t position) {
  if (interval->GetStart() < position && position < interval->GetEnd()) {
    return Split(interval, position);
  } else {
    return interval;
  }
}

// TODO: Review for off-by-one errors.
// TODO: Do we need to handle "environment" uses here?
void RegisterAllocatorGraphColor::SplitAtRegisterUses(LiveInterval* interval) {
  DCHECK(!interval->IsHighInterval());

  // Split just after a register definition.
  if (interval->IsParent() && interval->DefinitionRequiresRegister()) {
    interval = TrySplit(interval, interval->GetStart() + 1);
  }

  UsePosition* use = interval->GetFirstUse();
  while (use != nullptr && use->GetPosition() < interval->GetStart()) {
    use = use->GetNext();
  }

  // Split around register uses.
  size_t end = interval->GetEnd();
  while (use != nullptr && use->GetPosition() <= end) {
    if (use->RequiresRegister()) {
      size_t position = use->GetPosition();
      interval = TrySplit(interval, position - 1);
      if (liveness_.GetInstructionFromPosition(position / 2)->IsControlFlow()) {
        // If we are at the very end of a basic block, we cannot split right
        // at the use.
        // TODO: Review this.
        interval = TrySplit(interval, position + 1);
      } else {
        interval = TrySplit(interval, position);
      }
    }
    use = use->GetNext();
  }
}

void RegisterAllocatorGraphColor::AllocateSpillSlotForCatchPhi(HInstruction* instruction) {
  if (instruction->IsPhi() && instruction->AsPhi()->IsCatchPhi()) {
    HPhi* phi = instruction->AsPhi();
    LiveInterval* interval = phi->GetLiveInterval();

    HInstruction* previous_phi = phi->GetPrevious();
    DCHECK(previous_phi == nullptr ||
           previous_phi->AsPhi()->GetRegNumber() <= phi->GetRegNumber())
        << "Phis expected to be sorted by vreg number, "
        << "so that equivalent phis are adjacent.";

    if (phi->IsVRegEquivalentOf(previous_phi)) {
      // Assign the same spill slot.
      DCHECK(previous_phi->GetLiveInterval()->HasSpillSlot());
      interval->SetSpillSlot(previous_phi->GetLiveInterval()->GetSpillSlot());
    } else {
      AllocateSpillSlotFor(interval);
    }
  }
}

void RegisterAllocatorGraphColor::BlockRegister(Location location,
                                                size_t start,
                                                size_t end) {
  DCHECK(location.IsRegister() || location.IsFpuRegister());
  int reg = location.reg();
  LiveInterval* interval = location.IsRegister()
      ? physical_core_intervals_[reg]
      : physical_fp_intervals_[reg];
  DCHECK(interval->GetRegister() == reg);
  bool blocked_by_codegen = location.IsRegister()
      ? codegen_->GetBlockedCoreRegisters()[reg]
      : codegen_->GetBlockedFloatingPointRegisters()[reg];
  if (blocked_by_codegen) {
    // We've already blocked this register for the entire method. (And adding a
    // range within another range violates the preconditions of AddRange(...)).
  } else {
    interval->AddRange(start, end);
  }
}

void RegisterAllocatorGraphColor::BlockRegisters(size_t start, size_t end, bool caller_save_only) {
  for (size_t i = 0; i < codegen_->GetNumberOfCoreRegisters(); ++i) {
    if (!caller_save_only || !codegen_->IsCoreCalleeSaveRegister(i)) {
      BlockRegister(Location::RegisterLocation(i), start, end);
    }
  }
  for (size_t i = 0; i < codegen_->GetNumberOfFloatingPointRegisters(); ++i) {
    if (!caller_save_only || !codegen_->IsFloatingPointCalleeSaveRegister(i)) {
      BlockRegister(Location::FpuRegisterLocation(i), start, end);
    }
  }
}

// TODO: See locations->OutputCanOverlapWithInputs(...); we may want to consider
//       this when building the interference graph.
void RegisterAllocatorGraphColor::BuildInterferenceGraph(ArenaVector<LiveInterval*>& intervals,
                                                         ArenaVector<InterferenceNode*>& interference_graph) {
  // Build the interference graph efficiently by ordering range endpoints
  // by position and doing a line-sweep to find interferences.
  // We order by both position and (secondarily) by whether the endpoint
  // begins or ends a range; we want to process range endings before range
  // beginnings at the same position because they should not conflict.
  // Tuple contents: (position, is_range_beginning, node).
  ArenaVector<std::tuple<size_t, bool, InterferenceNode*>> range_endpoints(
      allocator_->Adapter(kArenaAllocRegisterAllocator));
  for (LiveInterval* parent : intervals) {
    for (LiveInterval* sibling = parent; sibling != nullptr; sibling = sibling->GetNextSibling()) {
      LiveRange* range = sibling->GetFirstRange();
      if (range != nullptr) {
        InterferenceNode* node = new (allocator_) InterferenceNode(
            allocator_, sibling, interference_graph.size());
        interference_graph.push_back(node);
        while (range != nullptr) {
          range_endpoints.push_back(std::make_tuple(range->GetStart(), true, node));
          range_endpoints.push_back(std::make_tuple(range->GetEnd(), false, node));
          range = range->GetNext();
        }
      }
    }
  }
  std::sort(range_endpoints.begin(), range_endpoints.end());

  // Node currently live at the current position in the line sweep.
  ArenaSet<InterferenceNode*, decltype(&InterferenceNode::CmpPtr)> live(
      InterferenceNode::CmpPtr, allocator_->Adapter(kArenaAllocRegisterAllocator));

  // Line sweep.
  for (auto it = range_endpoints.begin(); it != range_endpoints.end(); ++it) {
    bool is_range_beginning;
    InterferenceNode* node;
    std::tie(std::ignore, is_range_beginning, node) = *it;

    if (is_range_beginning) {
      for (InterferenceNode* conflicting : live) {
        DCHECK_NE(node, conflicting);
        node->AddInterference(conflicting);
        if (conflicting->Interval()->HasRegister()) {
          // Save space by ignoring out-edges for pre-colored nodes.
        } else {
          conflicting->AddInterference(node);
        }
      }
      DCHECK_EQ(live.count(node), 0u);
      live.insert(node);
    } else /*is_range_end*/ {
      DCHECK_EQ(live.count(node), 1u);
      live.erase(node);
    }
  }
  DCHECK(live.empty());
}

// The order in which we prune intervals from the interference graph
// is vital to both correctness and code quality. Note that pruning an
// interval later gives it a higher chance of being colored.
// We choose lhs by returning true, and rhs by returning false.
// Note that this ordering only applies to the high degree worklist.
// TODO: Use loops to decide spill weight.
// TODO: May want to choose constants if possible, since they
//       can be rematerialized.
// TODO: May want to choose intervals that already have spill slots before
//       those that don't.
static bool ChooseNextToPrune(const InterferenceNode* lhs_node,
                              const InterferenceNode* rhs_node) {
  LiveInterval* lhs = lhs_node->Interval();
  LiveInterval* rhs = rhs_node->Interval();
  // TODO: This could be made more readable.
  if (lhs->RequiresRegister() != rhs->RequiresRegister()) {
    // Choose the interval that does not require a register.
    return !lhs->RequiresRegister();
  } else if (lhs->GetEnd() - lhs->GetStart() != rhs->GetEnd() - rhs->GetStart()) {
    // Choose the interval that has a longer life span. That way, if we need
    // to spill, we spill the intervals that can be split up the most.
    return lhs->GetEnd() - lhs->GetStart() > rhs->GetEnd() - rhs->GetStart();
  } else {
    // Just choose the interval based on a deterministic ordering.
    return InterferenceNode::CmpPtr(lhs_node, rhs_node);
  }
}

void RegisterAllocatorGraphColor::PruneInterferenceGraph(
      ArenaVector<InterferenceNode*>& interference_graph,
      size_t num_regs,
      ArenaVector<InterferenceNode*>& pruned_nodes) {
  // We use a deque for low degree nodes, since we need to be able to insert
  // safepoint intervals at the front to be processed first.
  ArenaDeque<InterferenceNode*> low_degree_worklist(
      allocator_->Adapter(kArenaAllocRegisterAllocator));

  // TODO: Can use a priority queue instead if we start storing state in
  //       interference nodes.
  ArenaSet<InterferenceNode*, decltype(&ChooseNextToPrune)> high_degree_worklist(
      ChooseNextToPrune, allocator_->Adapter(kArenaAllocRegisterAllocator));

  // Build worklists.
  for (InterferenceNode* node : interference_graph) {
    if (node->Interval()->HasRegister()) {
      // Never prune physical register intervals.
    } else if (node->Interval()->IsSlowPathSafepoint()) {
      // This is a synthesized safepoint interval. We need to prune it
      // before anything else so that it is popped from pruned_intervals last,
      // allowing us to count the number of intervals live at this point.
      // (Otherwise, safepoint intervals would not be counting intervals
      // pruned before them.)
      low_degree_worklist.push_front(node);
    } else if (node->Degree() < num_regs) {
      low_degree_worklist.push_back(node);
    } else {
      high_degree_worklist.insert(node);
    }
  }

  // Helper function to prune an interval from the interference graph,
  // which includes updating the worklists.
  auto prune_node = [this, num_regs,
                     &pruned_nodes,
                     &low_degree_worklist,
                     &high_degree_worklist] (InterferenceNode* node) {
    DCHECK(!node->Interval()->HasRegister());
    pruned_nodes.push_back(node);
    for (InterferenceNode* adj : node->Adj()) {
      if (adj->Interval()->HasRegister()) {
        // No effect on pre-colored nodes; they're never pruned.
      } else {
        bool was_high_degree = adj->Degree() >= num_regs;
        DCHECK(adj->ContainsInterference(node));
        adj->RemoveInterference(node);
        if (was_high_degree && adj->Degree() < num_regs) {
          // This is a transition from high degree to low degree.
          DCHECK_EQ(high_degree_worklist.count(adj), 1u);
          high_degree_worklist.erase(adj);
          low_degree_worklist.push_back(adj);
        }
      }
    }
  };

  // Prune graph.
  while (!low_degree_worklist.empty() || !high_degree_worklist.empty()) {
    while (!low_degree_worklist.empty()) {
      InterferenceNode* node = low_degree_worklist.front();
      low_degree_worklist.pop_front();
      prune_node(node);
    }
    if (!high_degree_worklist.empty()) {
      InterferenceNode* node = *high_degree_worklist.begin();
      high_degree_worklist.erase(node);
      prune_node(node);
    }
  }
}

bool RegisterAllocatorGraphColor::ColorInterferenceGraph(
      ArenaVector<InterferenceNode*>& pruned_nodes,
      size_t num_regs,
      bool processing_core_regs) {
  DCHECK_LE(num_regs, 64u);
  size_t& max_safepoint_live_regs = processing_core_regs
      ? max_safepoint_live_core_regs_
      : max_safepoint_live_fp_regs_;
  ArenaVector<LiveInterval*> colored_intervals(
      allocator_->Adapter(kArenaAllocRegisterAllocator));
  bool successful = true;

  while (!pruned_nodes.empty()) {
    InterferenceNode* node = pruned_nodes.back();
    pruned_nodes.pop_back();
    LiveInterval* interval = node->Interval();

    uint64_t conflict_mask = 0;
    for (InterferenceNode* adj : node->Adj()) {
      LiveInterval* conflicting = adj->Interval();
      if (conflicting->HasRegister()) {
        conflict_mask |= (1u << conflicting->GetRegister());
        if (conflicting->HasHighInterval()) {
          DCHECK(conflicting->GetHighInterval()->HasRegister());
          conflict_mask |= (1u << conflicting->GetHighInterval()->GetRegister());
        }
      }
    }

    // Verify that we are not allocating registers blocked globally by
    // the code generator (such as the stack pointer).
    if (kIsDebugBuild) {
      bool* blocked_regs = processing_core_regs
          ? codegen_->GetBlockedCoreRegisters()
          : codegen_->GetBlockedFloatingPointRegisters();
      for (size_t i = 0; i < num_regs; ++i) {
        if (blocked_regs[i]) {
          DCHECK(conflict_mask & (1u << i)) << conflict_mask;
        }
      }
    }

    // Update the maximum number of live registers at safepoints.
    if (node->Interval()->IsSlowPathSafepoint()) {
      // TODO: This comment is originally from register_allocator_linear_scan.
      //       Are circumstances different for graph coloring?
      // We added a synthesized range to record the live registers
      // at this position. Ideally, we could just update the safepoints when locations
      // are updated, but we currently need to know the full stack size before updating
      // locations (because of parameters and the fact that we don't have a frame pointer).
      // And knowing the full stack size requires to know the maximum number of live
      // registers at calls in slow paths.
      // By adding the following interval in the algorithm, we can compute this
      // maximum before updating locations.
      // TODO: This counts code-generator-blocked registers such as the stack
      //       pointer. Is this necessary?
      size_t live_regs = static_cast<size_t>(POPCOUNT(conflict_mask));
      max_safepoint_live_regs =
          std::max(max_safepoint_live_regs, live_regs);
      continue;
    }

    // Search for free register(s)
    size_t reg = 0;
    if (interval->HasHighInterval()) {
      // TODO: We can likely improve coloring for high intervals by considering
      //       extra constraints during pruning.
      //       Also, must the low interval have an even-indexed register?
      while (reg < num_regs - 1
          && ((conflict_mask & (1u << reg)) || (conflict_mask & (1u << (reg + 1))))) {
        reg += 2;
      }
    } else {
      while (reg < num_regs && (conflict_mask & (1u << reg))) {
        reg += 1;
      }
    }

    if (reg < num_regs) {
      // Assign register.
      DCHECK(!interval->HasRegister());
      interval->SetRegister(reg);
      colored_intervals.push_back(interval);
      if (interval->HasHighInterval()) {
        DCHECK(!interval->GetHighInterval()->HasRegister());
        interval->GetHighInterval()->SetRegister(reg + 1);
        colored_intervals.push_back(interval->GetHighInterval());
      }
    } else {
      if (interval->RequiresRegister()) {
        // The interference graph is too dense to color. Make it sparser by
        // splitting this live interval.
        successful = false;
        SplitAtRegisterUses(interval);
      } else {
        // Spill.
        AllocateSpillSlotFor(interval);
      }
    }
  }

  // If unsuccessful, reset all register assignments.
  if (!successful) {
    max_safepoint_live_regs = 0;
    for (LiveInterval* interval : colored_intervals) {
      interval->ClearRegister();
    }
  }

  return successful;
}

void RegisterAllocatorGraphColor::AllocateSpillSlotFor(LiveInterval* interval) {
  LiveInterval* parent = interval->GetParent();
  HInstruction* defined_by = parent->GetDefinedBy();
  if (parent->HasSpillSlot()) {
    // We already have a spill slot for this value that we can reuse.
  } else if (defined_by->IsParameterValue()) {
    // Parameters already have a stack slot.
    parent->SetSpillSlot(codegen_->GetStackSlotOfParameter(defined_by->AsParameterValue()));
  } else if (defined_by->IsCurrentMethod()) {
    parent->SetSpillSlot(0);
  } else if (defined_by->IsConstant()) {
    // Constants don't need a spill slot.
  } else {
    parent->SetSpillSlot(spill_slot_counter_);
    spill_slot_counter_ += parent->NeedsTwoSpillSlots() ? 2 : 1;
    // TODO: Could technically color stack slots if we wanted to, even if
    //       it's just a trivial coloring. See the linear scan implementation,
    //       which simply reuses spill slots for values whose live intervals
    //       have already ended.
    //       Careful: comments in the linear scan allocator suggest that
    //       the parallel move resolver may depend on there not being any
    //       moves between stack slots of different types.
  }
}

// TODO: Factor out into register_allocator_common (but first refactor into several methods).
void RegisterAllocatorGraphColor::Resolve() {
  codegen_->InitializeCodeGeneration(spill_slot_counter_,
                                     max_safepoint_live_core_regs_,
                                     max_safepoint_live_fp_regs_,
                                     reserved_art_method_slots_ + reserved_out_slots_,  // TODO: Refactor this parameter into two
                                     codegen_->GetGraph()->GetLinearOrder());

  // Adjust the out location of instructions.
  for (size_t i = 0, e = liveness_.GetNumberOfSsaValues(); i < e; ++i) {
    HInstruction* instruction = liveness_.GetInstructionFromSsaIndex(i);
    LiveInterval* interval = instruction->GetLiveInterval();
    LocationSummary* locations = instruction->GetLocations();
    Location out = locations->Out();

    DCHECK(!instruction->IsPhi() || !instruction->AsPhi()->IsCatchPhi() || interval->HasSpillSlot())
        << "A catch phi has not been assigned a spill slot";

    if (instruction->IsParameterValue()) {
      // Now that we know the frame size, adjust the parameter's location.
      if (out.IsStackSlot()) {
        out = Location::StackSlot(out.GetStackIndex() + codegen_->GetFrameSize());
        interval->SetSpillSlot(out.GetStackIndex());
        locations->UpdateOut(out);
      } else if (out.IsDoubleStackSlot()) {
        out = Location::DoubleStackSlot(out.GetStackIndex() + codegen_->GetFrameSize());
        interval->SetSpillSlot(out.GetStackIndex());
        locations->UpdateOut(out);
      } else if (interval->HasSpillSlot()) {
        interval->SetSpillSlot(interval->GetSpillSlot() + codegen_->GetFrameSize());
      }
    } else if (instruction->IsCurrentMethod()) {
      // The current method is always at offset 0.
      DCHECK(!interval->HasSpillSlot() || (interval->GetSpillSlot() == 0));
    } else if (interval->HasSpillSlot()) {
      // Set final spill slot.
      // Stack layout: [ parameters         ]
      //               [ spill slots        ]
      //               [ outgoing arguments ]
      //               [ art method         ]
      size_t slot =
          reserved_art_method_slots_
          + reserved_out_slots_
          + interval->GetSpillSlot();
      interval->SetSpillSlot(slot * kVRegSize);
    }

    Location source = interval->ToLocation();

    if (out.IsUnallocated()) {
      if (out.GetPolicy() == Location::kSameAsFirstInput) {
        if (locations->InAt(0).IsUnallocated()) {
          locations->SetInAt(0, source);
        } else {
          DCHECK(locations->InAt(0).Equals(source));
        }
      }
      locations->UpdateOut(source);
    } else {
      DCHECK(source.Equals(out));
    }
  }

  // Connect siblings.
  for (size_t i = 0, e = liveness_.GetNumberOfSsaValues(); i < e; ++i) {
    HInstruction* instruction = liveness_.GetInstructionFromSsaIndex(i);
    ConnectSiblings(instruction->GetLiveInterval(),
                    max_safepoint_live_core_regs_
                    + max_safepoint_live_fp_regs_);
  }

  // Resolve non-linear control flow across branches. Order does not matter.
  for (HLinearOrderIterator it(*codegen_->GetGraph()); !it.Done(); it.Advance()) {
    HBasicBlock* block = it.Current();
    if (block->IsCatchBlock() ||
        (block->IsLoopHeader() && block->GetLoopInformation()->IsIrreducible())) {
      // Instructions live at the top of catch blocks or irreducible loop header
      // were forced to spill.
      if (kIsDebugBuild) {
        BitVector* live = liveness_.GetLiveInSet(*block);
        for (uint32_t idx : live->Indexes()) {
          LiveInterval* interval = liveness_.GetInstructionFromSsaIndex(idx)->GetLiveInterval();
          LiveInterval* sibling = interval->GetSiblingAt(block->GetLifetimeStart());
          // `GetSiblingAt` returns the sibling that contains a position, but there could be
          // a lifetime hole in it. `CoversSlow` returns whether the interval is live at that
          // position.
          if ((sibling != nullptr) && sibling->CoversSlow(block->GetLifetimeStart())) {
            DCHECK(!sibling->HasRegister());
          }
        }
      }
    } else {
      BitVector* live = liveness_.GetLiveInSet(*block);
      for (uint32_t idx : live->Indexes()) {
        LiveInterval* interval = liveness_.GetInstructionFromSsaIndex(idx)->GetLiveInterval();
        for (HBasicBlock* predecessor : block->GetPredecessors()) {
          ConnectSplitSiblings(interval, predecessor, block);
        }
      }
    }
  }

  // Resolve phi inputs. Order does not matter.
  for (HLinearOrderIterator it(*codegen_->GetGraph()); !it.Done(); it.Advance()) {
    HBasicBlock* current = it.Current();
    if (current->IsCatchBlock()) {
      // Catch phi values are set at runtime by the exception delivery mechanism.
    } else {
      for (HInstructionIterator inst_it(current->GetPhis()); !inst_it.Done(); inst_it.Advance()) {
        HInstruction* phi = inst_it.Current();
        for (size_t i = 0, e = current->GetPredecessors().size(); i < e; ++i) {
          HBasicBlock* predecessor = current->GetPredecessors()[i];
          DCHECK_EQ(predecessor->GetNormalSuccessors().size(), 1u);
          HInstruction* input = phi->InputAt(i);
          Location source = input->GetLiveInterval()->GetLocationAt(
              predecessor->GetLifetimeEnd() - 1);
          Location destination = phi->GetLiveInterval()->ToLocation();
          InsertParallelMoveAtExitOf(predecessor, phi, source, destination);
        }
      }
    }
  }

  // Assign temp locations.
  for (LiveInterval* temp : temp_intervals_) {
    if (temp->IsHighInterval()) {
      // High intervals can be skipped, they are already handled by the low interval.
      continue;
    }
    HInstruction* at = liveness_.GetTempUser(temp);
    size_t temp_index = liveness_.GetTempIndex(temp);
    LocationSummary* locations = at->GetLocations();
    switch (temp->GetType()) {
      case Primitive::kPrimInt:
        locations->SetTempAt(temp_index, Location::RegisterLocation(temp->GetRegister()));
        break;

      case Primitive::kPrimDouble:
        if (codegen_->NeedsTwoRegisters(Primitive::kPrimDouble)) {
          Location location = Location::FpuRegisterPairLocation(
              temp->GetRegister(), temp->GetHighInterval()->GetRegister());
          locations->SetTempAt(temp_index, location);
        } else {
          locations->SetTempAt(temp_index, Location::FpuRegisterLocation(temp->GetRegister()));
        }
        break;

      default:
        LOG(FATAL) << "Unexpected type for temporary location "
                   << temp->GetType();
    }
  }
}

}  // namespace art
