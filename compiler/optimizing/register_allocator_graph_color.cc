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
#include "register_allocation_resolver.h"
#include "ssa_liveness_analysis.h"
#include "thread-inl.h"

namespace art {

static bool IsCoreInterval(LiveInterval* interval) {
  return interval->GetType() != Primitive::kPrimFloat
      && interval->GetType() != Primitive::kPrimDouble;
}

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
        int_spill_slot_counter_(0),
        double_spill_slot_counter_(0),
        float_spill_slot_counter_(0),
        long_spill_slot_counter_(0),
        catch_phi_spill_slot_counter_(0),
        reserved_art_method_slots_(InstructionSetPointerSize(codegen->GetInstructionSet()) / kVRegSize),
        reserved_out_slots_(codegen->GetGraph()->GetMaximumNumberOfOutVRegs()),
        max_safepoint_live_core_regs_(0),
        max_safepoint_live_fp_regs_(0) {
  // Before we ask for blocked registers, set them up in the code generator.
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

bool RegisterAllocatorGraphColor::Validate(bool log_fatal_on_failure) {
  for (bool processing_core_regs : {true, false}) {
    ArenaVector<LiveInterval*> intervals(
        allocator_->Adapter(kArenaAllocRegisterAllocatorValidate));
    for (size_t i = 0; i < liveness_.GetNumberOfSsaValues(); ++i) {
      HInstruction* instruction = liveness_.GetInstructionFromSsaIndex(i);
      LiveInterval* interval = instruction->GetLiveInterval();
      if (interval != nullptr) {
        if (IsCoreInterval(interval) == processing_core_regs) {
          intervals.push_back(instruction->GetLiveInterval());
        }
      }
    }

    ArenaVector<LiveInterval*>& physical_intervals = processing_core_regs
        ? physical_core_intervals_
        : physical_fp_intervals_;
    for (LiveInterval* fixed : physical_intervals) {
      if (fixed->GetFirstRange() != nullptr) {
        intervals.push_back(fixed);
      }
    }

    for (LiveInterval* temp : temp_intervals_) {
      if (IsCoreInterval(temp) == processing_core_regs) {
        intervals.push_back(temp);
      }
    }

    size_t spill_slots = int_spill_slot_counter_
                       + long_spill_slot_counter_
                       + float_spill_slot_counter_
                       + double_spill_slot_counter_;
    bool ok = ValidateIntervals(intervals,
                                spill_slots,
                                reserved_art_method_slots_ + reserved_out_slots_,
                                *codegen_,
                                allocator_,
                                processing_core_regs,
                                log_fatal_on_failure);
    if (!ok) {
      return false;
    }
  }

  return true;
}

// TODO: Decide on limits.
static constexpr size_t kMaxGraphColoringAttemptsDebug = 100;
static constexpr size_t kMaxGraphColoringAttemptsRelease = 1000;

void RegisterAllocatorGraphColor::AllocateRegisters() {
  // (1) Collect and set up live intervals.
  ProcessInstructions();

  for (bool processing_core_regs : {true, false}) {
    ArenaVector<LiveInterval*>& intervals = processing_core_regs
        ? core_intervals_
        : fp_intervals_;
    size_t num_registers = processing_core_regs
        ? codegen_->GetNumberOfCoreRegisters()
        : codegen_->GetNumberOfFloatingPointRegisters();

    size_t attempt = 0;
    bool successful = false;
    while (!successful && ++attempt <= kMaxGraphColoringAttemptsRelease) {
      // (2) Build interference graph.
      ArenaVector<InterferenceNode*> interference_graph(
          allocator_->Adapter(kArenaAllocRegisterAllocator));
      BuildInterferenceGraph(intervals, interference_graph);

      // (3) Prune all uncolored nodes from interference graph.
      ArenaStdStack<InterferenceNode*> pruned_nodes(
          allocator_->Adapter(kArenaAllocRegisterAllocator));
      PruneInterferenceGraph(interference_graph, num_registers, pruned_nodes);

      // (4) Color pruned nodes based on interferences.
      successful = ColorInterferenceGraph(pruned_nodes,
                                          num_registers,
                                          processing_core_regs);

      DCHECK(attempt <= kMaxGraphColoringAttemptsDebug)
          << "Graph coloring register allocation is taking too long to "
          << "allocate registers";
    }

    CHECK(successful) << "Exceeded max graph coloring register allocation attempts";

    // Tell the code generator which registers have been allocated.
    // TODO: Right now this will also include blocked registers, yet
    //       the linear scan implementation does not seem to include these
    //       at times (e.g., fixed inputs, blocked regs for calls, etc.).
    //       What is the correct approach here?
    //       Note that if we go back to excluding blocked registers, we
    //       will still need to include blocked temporaries as a special case.
    for (LiveInterval* parent : intervals) {
      for (LiveInterval* sibling = parent; sibling != nullptr; sibling = sibling->GetNextSibling()) {
        if (sibling->HasRegister() && sibling->GetFirstRange() != nullptr) {
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
  }

  // (6) Resolve locations and deconstruct SSA form.
  RegisterAllocationResolver(allocator_, codegen_, liveness_)
      .Resolve(max_safepoint_live_core_regs_,
               max_safepoint_live_fp_regs_,
               reserved_art_method_slots_ + reserved_out_slots_,
               int_spill_slot_counter_,
               long_spill_slot_counter_,
               float_spill_slot_counter_,
               double_spill_slot_counter_,
               catch_phi_spill_slot_counter_,
               temp_intervals_);
}

void RegisterAllocatorGraphColor::ProcessInstructions() {
  for (HLinearPostOrderIterator it(*codegen_->GetGraph()); !it.Done(); it.Advance()) {
    HBasicBlock* block = it.Current();

    // Note that we currently depend on this ordering, since some helper
    // code is designed for linear scan register allocation.
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
    // We do this here because we do not want the suspend check to artificially
    // create live registers.
    DCHECK(instruction->IsSuspendCheckEntry());
    DCHECK_EQ(locations->GetTempCount(), 0u);
    instruction->GetBlock()->RemoveInstruction(instruction);
    return;
  }

  CheckForTempLiveIntervals(instruction);
  CheckForSafepoint(instruction);
  if (instruction->GetLocations()->WillCall()) {
    // If a call will happen, create fixed intervals for caller-save registers.
    // TODO: Note that it may be beneficial to later split intervals at this point,
    //       so that we allow last-minute moves from a caller-save register
    //       to a callee-save register.
    BlockRegisters(instruction->GetLifetimePosition(),
                   instruction->GetLifetimePosition() + 1,
                   /*caller_save_only*/ true);
  }
  CheckForFixedInputs(instruction);

  LiveInterval* interval = instruction->GetLiveInterval();
  if (interval == nullptr) {
    return;
  }

  // Low intervals act as representatives for their corresponding high interval.
  DCHECK(!interval->IsHighInterval());
  if (codegen_->NeedsTwoRegisters(interval->GetType())) {
    interval->AddHighInterval();
  }
  AddSafepointsFor(instruction);
  CheckForFixedOutput(instruction);
  AllocateSpillSlotForCatchPhi(instruction);

  ArenaVector<LiveInterval*>& intervals = IsCoreInterval(interval)
      ? core_intervals_
      : fp_intervals_;
  if (interval->HasSpillSlot() || instruction->IsConstant()) {
    if (!interval->RequiresRegister()) {
      // We won't allocate a register for this value.
    } else {
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
  DCHECK_GE(interval->GetEnd() - position, 2u);
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

void RegisterAllocatorGraphColor::AddSafepointsFor(HInstruction* instruction) {
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

LiveInterval* RegisterAllocatorGraphColor::TrySplit(LiveInterval* interval, size_t position) {
  if (interval->GetStart() < position && position < interval->GetEnd()) {
    return Split(interval, position);
  } else {
    return interval;
  }
}

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
        // at the use. Split just after instead.
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
      interval->SetSpillSlot(catch_phi_spill_slot_counter_);
      catch_phi_spill_slot_counter_ += interval->NeedsTwoSpillSlots() ? 2 : 1;
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
    // range inside another range violates the preconditions of AddRange).
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

// TODO: See locations->OutputCanOverlapWithInputs(); we may want to consider
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

  // Nodes currently live at the current position in the line sweep.
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

// The order in which we color nodes is vital to both correctness (forward
// progress) and code quality.
// TODO: May also want to consider:
// - Loop depth
// - Constants (since they can be rematerialized)
// - Allocated spill slots
static bool ChooseHigherPriority(const InterferenceNode* lhs,
                                 const InterferenceNode* rhs) {
  LiveInterval* lhs_interval = lhs->Interval();
  LiveInterval* rhs_interval = rhs->Interval();
  // (1) Choose the interval that requires a register.
  // (2) Choose the interval that has a shorter life span.
  // (3) Just choose the interval based on a deterministic ordering.
  if (lhs_interval->RequiresRegister() != rhs_interval->RequiresRegister()) {
    return lhs_interval->RequiresRegister();
  } else if (lhs_interval->GetLength() != rhs_interval->GetLength()) {
    return lhs_interval->GetLength() < rhs_interval->GetLength();
  } else {
    return InterferenceNode::CmpPtr(lhs, rhs);
  }
}

void RegisterAllocatorGraphColor::PruneInterferenceGraph(
      ArenaVector<InterferenceNode*>& interference_graph,
      size_t num_regs,
      ArenaStdStack<InterferenceNode*>& pruned_nodes) {
  // We use a deque for low degree nodes, since we need to be able to insert
  // safepoint intervals at the front to be processed first.
  ArenaDeque<InterferenceNode*> low_degree_worklist(
      allocator_->Adapter(kArenaAllocRegisterAllocator));

  // If we have to prune from the high-degree worklist, we cannot guarantee
  // the pruned node a color. So, we order the worklist by priority.
  ArenaSet<InterferenceNode*, decltype(&ChooseHigherPriority)> high_degree_worklist(
      ChooseHigherPriority, allocator_->Adapter(kArenaAllocRegisterAllocator));

  // Build worklists.
  for (InterferenceNode* node : interference_graph) {
    if (node->Interval()->HasRegister()) {
      // Never prune physical register intervals.
    } else if (node->Interval()->IsSlowPathSafepoint()) {
      // This is a synthesized safepoint interval. We need to prune it
      // before anything else so that it is popped from pruned_intervals last,
      // allowing us to count the number of intervals live at this point.
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
    pruned_nodes.push(node);
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
      // We prune the lowest-priority node, because pruning a node earlier
      // gives it a higher chance of being spilled.
      InterferenceNode* node = *high_degree_worklist.rbegin();
      high_degree_worklist.erase(node);
      prune_node(node);
    }
  }
}

bool RegisterAllocatorGraphColor::ColorInterferenceGraph(
      ArenaStdStack<InterferenceNode*>& pruned_nodes,
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
    InterferenceNode* node = pruned_nodes.top();
    pruned_nodes.pop();
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
      } else {
        DCHECK(!conflicting->HasHighInterval()
            || !conflicting->GetHighInterval()->HasRegister());
      }
    }

    // Verify that we are not allocating registers blocked globally by
    // the code generator, such as the stack pointer.
    if (kIsDebugBuild) {
      bool* blocked_regs = processing_core_regs
          ? codegen_->GetBlockedCoreRegisters()
          : codegen_->GetBlockedFloatingPointRegisters();
      for (size_t i = 0; i < num_regs; ++i) {
        if (blocked_regs[i]) {
          DCHECK(conflict_mask & (1u << i));
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
      // And knowing the full stack size requires knowing the maximum number of live
      // registers at calls in slow paths.
      // TODO: This counts code-generator-blocked registers such as the stack
      //       pointer. Is this necessary?
      size_t live_regs = static_cast<size_t>(POPCOUNT(conflict_mask));
      max_safepoint_live_regs =
          std::max(max_safepoint_live_regs, live_regs);
      continue;
    }

    // Search for free register(s).
    size_t reg = 0;
    if (interval->HasHighInterval()) {
      while (reg < num_regs - 1
          && ((conflict_mask & (1u << reg)) || (conflict_mask & (1u << (reg + 1))))) {
        reg += 2;
      }
    } else {
      // Flip the bits of conflict_mask and find the first 1,
      // indicating a free register. Note that CTZ(0) is undefined.
      reg = (~conflict_mask == 0u) ? 64u : CTZ(~conflict_mask);
    }

    if (reg < (interval->HasHighInterval() ? num_regs - 1 : num_regs)) {
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
    // The current method is always at spill slot 0.
    parent->SetSpillSlot(0);
  } else if (defined_by->IsConstant()) {
    // Constants don't need a spill slot.
  } else {
    // Allocate a spill slot based on type.
    size_t* spill_slot_counter;
    switch (interval->GetType()) {
      case Primitive::kPrimDouble:
        spill_slot_counter = &double_spill_slot_counter_;
        break;
      case Primitive::kPrimLong:
        spill_slot_counter = &long_spill_slot_counter_;
        break;
      case Primitive::kPrimFloat:
        spill_slot_counter = &float_spill_slot_counter_;
        break;
      case Primitive::kPrimNot:
      case Primitive::kPrimInt:
      case Primitive::kPrimChar:
      case Primitive::kPrimByte:
      case Primitive::kPrimBoolean:
      case Primitive::kPrimShort:
        spill_slot_counter = &int_spill_slot_counter_;
        break;
      case Primitive::kPrimVoid:
        LOG(FATAL) << "Unexpected type for interval " << interval->GetType();
        UNREACHABLE();
    }

    parent->SetSpillSlot(*spill_slot_counter);
    *spill_slot_counter += parent->NeedsTwoSpillSlots() ? 2 : 1;
    // TODO: Could color stack slots if we wanted to, even if
    //       it's just a trivial coloring. See the linear scan implementation,
    //       which simply reuses spill slots for values whose live intervals
    //       have already ended.
  }
}

}  // namespace art
