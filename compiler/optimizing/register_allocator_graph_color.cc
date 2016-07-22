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
  if (interval->GetParent()->GetDefinedBy() != nullptr) {
    s << interval->GetParent()->GetDefinedBy()->GetKind()
        << "(" << interval->GetParent()->GetDefinedBy()->GetLifetimePosition() << ") ";
  } else {
    s << "[unknown] ";
  }
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
  return s.str();
}

enum CoalescePhase {
  kWorklist,  // Currently in the coalesce worklist.
  kActive,    // Not in a worklist, but could be in the future.
  kDefunct    // No longer a valid coalesce opportunity.
};

// Represents a coalesce opportunity between two nodes.
struct CoalesceOpportunity : public ArenaObject<kArenaAllocRegisterAllocator> {
  CoalesceOpportunity(InterferenceNode* a_in, InterferenceNode* b_in);

  InterferenceNode* const a;
  InterferenceNode* const b;

  CoalescePhase phase;

  // TODO: Add priority, possibly based on loop information.
  const size_t priority;
};

enum InterferenceNodePhase {
  kInitial,
  kPrecolored,
  kSafepoint,
  kPrunable,
  kSimplifyWorklist,
  kFreezeWorklist,
  kSpillWorklist,
  kPruned
};

// Interference nodes make up the interference graph, which is the primary
// data structure in graph coloring register allocation.
class InterferenceNode : public ArenaObject<kArenaAllocRegisterAllocator> {
 public:
  InterferenceNode(ArenaAllocator* allocator,
                   LiveInterval* interval,
                   size_t id)
        : phase(kInitial),
          interval_(interval),
          adj_(CmpPtr, allocator->Adapter(kArenaAllocRegisterAllocator)),
          coalesce_opportunities_(allocator->Adapter(kArenaAllocRegisterAllocator)),
          degree_(0),
          id_(id),
          alias_(this) {
    DCHECK(!interval->IsHighInterval())
        << "Pair nodes should be represented by the low interval";
  }

  // Use to maintain determinism when storing InterferenceNode pointers in sets.
  static bool CmpPtr(const InterferenceNode* lhs, const InterferenceNode* rhs) {
    return lhs->id_ < rhs->id_;
  }

  void AddInterference(InterferenceNode* other) {
    DCHECK(!Precolored()) << "To save memory, fixed nodes should not have outgoing interferences";
    DCHECK_NE(this, other) << "Should not create self loops in the interference graph";
    DCHECK_EQ(this, alias_) << "Should not add interferences to a node that aliases another";
    DCHECK(phase != kPruned && other->phase != kPruned);
    if (adj_.insert(other).second) {
      degree_ += EdgeWeightWith(other);
    }
  }
  void RemoveInterference(InterferenceNode* other) {
    DCHECK_EQ(this, alias_) << "Should not remove interferences from a coalesced node";
    DCHECK_EQ(other->phase, kPruned) << "Should only remove interferences when pruning";
    if (adj_.erase(other) > 0) {
      degree_ -= EdgeWeightWith(other);
    }
  }
  bool HasInterference(InterferenceNode* other) const {
    DCHECK(!Precolored()) << "Should not query fixed nodes for interferences";
    DCHECK_EQ(this, alias_) << "Should not query a coalesced node for interferences";
    return adj_.count(other) > 0;
  }

  void AddCoalesceOpportunity(CoalesceOpportunity* other) {
    coalesce_opportunities_.push_back(other);
  }
  bool MoveRelated() const {
    for (CoalesceOpportunity* opportunity : coalesce_opportunities_) {
      if (opportunity->phase != kDefunct) {
        return true;
      }
    }
    return false;
  }

  bool Precolored() const {
    return interval_->HasRegister();
  }
  bool IsPair() const {
    return interval_->HasHighInterval();
  }

  void SetAlias(InterferenceNode* rep) {
    DCHECK_NE(rep->phase, kPruned);
    DCHECK_EQ(this, alias_) << "Should only set a node's alias once";
    alias_ = rep;
  }
  InterferenceNode* Alias() {
    if (alias_ != this) {
      // Recurse in order to flatten tree of alias pointers.
      alias_ = alias_->Alias();
    }
    return alias_;
  }

  // TODO: Reduce these getters.
  LiveInterval* Interval() const {
    return interval_;
  }
  const ArenaSet<InterferenceNode*, decltype(&CmpPtr)>& Adj() const {
    return adj_;
  }
  const ArenaVector<CoalesceOpportunity*>& CoalesceOpportunities() const {
    return coalesce_opportunities_;
  }
  size_t Degree() const {
    // Pre-colored nodes have infinite degree.
    return interval_->HasRegister() ? std::numeric_limits<size_t>::max() : degree_;
  }
  size_t Id() const {
    return id_;
  }

  // In order to model the constraints imposed by register pairs, we give
  // extra weight to edges adjacent to register pair nodes.
  size_t EdgeWeightWith(const InterferenceNode* other) const {
    return (IsPair() || other->IsPair()) ? 2 : 1;
  }

  // The current phase of this node, indicating which worklist it belongs to.
  InterferenceNodePhase phase;

 private:
  // The live interval that this node represents.
  LiveInterval* const interval_;

  // All nodes interfering with this one.
  // To save memory, we do not keep track of interferences for fixed nodes.
  // TUNING: There is potential to use a cheaper data structure here.
  ArenaSet<InterferenceNode*, decltype(&CmpPtr)> adj_;

  // Interference nodes that this node should be coalesced with to reduce moves.
  ArenaVector<CoalesceOpportunity*> coalesce_opportunities_;

  // We cannot use adjacency set size for degree, since that ignores nodes
  // representing pair intervals.
  size_t degree_;

  // A unique identifier for this node.
  const size_t id_;

  // If nodes are coalesced, this will be set to indicate which node represents this one.
  // Initially set to `this`.
  InterferenceNode* alias_;

  // TODO: Cache RequiresRegister for the live interval here.
  DISALLOW_COPY_AND_ASSIGN(InterferenceNode);
};

CoalesceOpportunity::CoalesceOpportunity(InterferenceNode* a_in, InterferenceNode* b_in)
      : a(a_in), b(b_in), phase(kWorklist), priority(0) {
  DCHECK_EQ(a->IsPair(), b->IsPair())
      << "A pair node cannot be coalesced with a non-pair node";
}

RegisterAllocatorGraphColor::RegisterAllocatorGraphColor(ArenaAllocator* allocator,
                                                         CodeGenerator* codegen,
                                                         const SsaLivenessAnalysis& liveness,
                                                         bool iterative_move_coalescing)
      : RegisterAllocator(allocator, codegen, liveness),
        iterative_move_coalescing_(iterative_move_coalescing),
        core_intervals_(allocator->Adapter(kArenaAllocRegisterAllocator)),
        fp_intervals_(allocator->Adapter(kArenaAllocRegisterAllocator)),
        temp_intervals_(allocator->Adapter(kArenaAllocRegisterAllocator)),
        safepoints_(allocator->Adapter(kArenaAllocRegisterAllocator)),
        physical_core_intervals_(allocator->Adapter(kArenaAllocRegisterAllocator)),
        physical_fp_intervals_(allocator->Adapter(kArenaAllocRegisterAllocator)),
        physical_core_nodes_(allocator->Adapter(kArenaAllocRegisterAllocator)),
        physical_fp_nodes_(allocator->Adapter(kArenaAllocRegisterAllocator)),
        int_spill_slot_counter_(0),
        double_spill_slot_counter_(0),
        float_spill_slot_counter_(0),
        long_spill_slot_counter_(0),
        catch_phi_spill_slot_counter_(0),
        reserved_art_method_slots_(InstructionSetPointerSize(codegen->GetInstructionSet())
                                   / kVRegSize),
        reserved_out_slots_(codegen->GetGraph()->GetMaximumNumberOfOutVRegs()),
        max_safepoint_live_core_regs_(0),
        max_safepoint_live_fp_regs_(0),
        node_id_counter_(0),
        interval_node_map_(allocator_->Adapter(kArenaAllocRegisterAllocator)),
        prunable_nodes_(allocator_->Adapter(kArenaAllocRegisterAllocator)),
        pruned_nodes_(allocator_->Adapter(kArenaAllocRegisterAllocator)),
        simplify_worklist_(allocator_->Adapter(kArenaAllocRegisterAllocator)),
        freeze_worklist_(allocator_->Adapter(kArenaAllocRegisterAllocator)),
        spill_worklist_(ChooseHigherPriorityNode,
                        allocator_->Adapter(kArenaAllocRegisterAllocator)),
        coalesce_worklist_(CmpCoalesceOpportunity,
                           allocator_->Adapter(kArenaAllocRegisterAllocator)) {
  // Before we ask for blocked registers, set them up in the code generator.
  codegen->SetupBlockedRegisters();

  // Initialize physical core register live intervals and blocked registers.
  // This includes globally blocked registers, such as the stack pointer.
  physical_core_intervals_.resize(codegen->GetNumberOfCoreRegisters(), nullptr);
  physical_core_nodes_.resize(codegen->GetNumberOfCoreRegisters(), nullptr);
  for (size_t i = 0; i < codegen->GetNumberOfCoreRegisters(); ++i) {
    LiveInterval* interval = LiveInterval::MakeFixedInterval(allocator_, i, Primitive::kPrimInt);
    physical_core_intervals_[i] = interval;
    physical_core_nodes_[i] =
        new (allocator_) InterferenceNode(allocator_, interval, node_id_counter_++);
    physical_core_nodes_[i]->phase = kPrecolored;
    core_intervals_.push_back(interval);
    if (codegen_->GetBlockedCoreRegisters()[i]) {
      interval->AddRange(0, liveness.GetMaxLifetimePosition());
    }
  }
  // Initialize physical floating point register live intervals and blocked registers.
  physical_fp_intervals_.resize(codegen->GetNumberOfFloatingPointRegisters(), nullptr);
  physical_fp_nodes_.resize(codegen->GetNumberOfFloatingPointRegisters(), nullptr);
  for (size_t i = 0; i < codegen->GetNumberOfFloatingPointRegisters(); ++i) {
    LiveInterval* interval = LiveInterval::MakeFixedInterval(allocator_, i, Primitive::kPrimFloat);
    physical_fp_intervals_[i] = interval;
    physical_fp_nodes_[i] =
        new (allocator_) InterferenceNode(allocator_, interval, node_id_counter_++);
    physical_fp_nodes_[i]->phase = kPrecolored;
    fp_intervals_.push_back(interval);
    if (codegen_->GetBlockedFloatingPointRegisters()[i]) {
      interval->AddRange(0, liveness.GetMaxLifetimePosition());
    }
  }
}

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
  // (1) Collect and prepare live intervals.
  ProcessInstructions();

  for (bool processing_core_regs : {true, false}) {
    ArenaVector<LiveInterval*>& intervals = processing_core_regs
        ? core_intervals_
        : fp_intervals_;
    size_t num_registers = processing_core_regs
        ? codegen_->GetNumberOfCoreRegisters()
        : codegen_->GetNumberOfFloatingPointRegisters();

    size_t attempt = 0;
    while (true) {
      ++attempt;
      DCHECK(attempt <= kMaxGraphColoringAttemptsDebug)
          << "Exceeded debug max graph coloring register allocation attempts";
      CHECK(attempt <= kMaxGraphColoringAttemptsRelease)
          << "Exceeded max graph coloring register allocation attempts";

      // Clear data structures.
      // TODO: Ideally there would be cleaner way to clear stacks and priority queues.
      interval_node_map_.Clear();
      prunable_nodes_.clear();
      pruned_nodes_ = ArenaStdStack<InterferenceNode*>(
          allocator_->Adapter(kArenaAllocRegisterAllocator));
      simplify_worklist_.clear();
      freeze_worklist_.clear();
      spill_worklist_ = ArenaPriorityQueue<InterferenceNode*, decltype(&ChooseHigherPriorityNode)>(
          ChooseHigherPriorityNode, allocator_->Adapter(kArenaAllocRegisterAllocator));
      coalesce_worklist_ =
          ArenaPriorityQueue<CoalesceOpportunity*, decltype(&CmpCoalesceOpportunity)>(
              CmpCoalesceOpportunity, allocator_->Adapter(kArenaAllocRegisterAllocator));

      // (2) Build the interference graph. Also gather safepoints and build the interval node map.
      ArenaVector<InterferenceNode*> safepoints(
          allocator_->Adapter(kArenaAllocRegisterAllocator));
      ArenaVector<InterferenceNode*>& physical_nodes = processing_core_regs
          ? physical_core_nodes_
          : physical_fp_nodes_;
      BuildInterferenceGraph(intervals, physical_nodes, safepoints);

      // (3) Add coalesce opportunities.
      if (iterative_move_coalescing_) {
        FindCoalesceOpportunities();
      }

      // (4) Prune all uncolored nodes from interference graph.
      PruneInterferenceGraph(num_registers);

      // (5) Color pruned nodes based on interferences.
      bool successful = ColorInterferenceGraph(num_registers,
                                               processing_core_regs);

      if (successful) {
        // Compute the maximum number of live registers across safepoints.
        size_t& max_safepoint_live_regs = processing_core_regs
            ? max_safepoint_live_core_regs_
            : max_safepoint_live_fp_regs_;
        ComputeMaxSafepointLiveRegisters(safepoints, max_safepoint_live_regs);

        // Tell the code generator which registers were allocated.
        // We only look at prunable_nodes because we already told the code generator about
        // fixed intervals while processing instructions. We also ignore the fixed intervals
        // placed at the top of catch blocks.
        for (InterferenceNode* node : prunable_nodes_) {
          LiveInterval* interval = node->Interval();
          if (interval->HasRegister()) {
            Location low_reg = processing_core_regs
                ? Location::RegisterLocation(interval->GetRegister())
                : Location::FpuRegisterLocation(interval->GetRegister());
            codegen_->AddAllocatedRegister(low_reg);
            if (interval->HasHighInterval()) {
              LiveInterval* high = interval->GetHighInterval();
              DCHECK(high->HasRegister());
              Location high_reg = processing_core_regs
                  ? Location::RegisterLocation(high->GetRegister())
                  : Location::RegisterLocation(high->GetRegister());
              codegen_->AddAllocatedRegister(high_reg);
            }
          } else {
            DCHECK(!interval->HasHighInterval() || !interval->GetHighInterval()->HasRegister());
          }
        }

        break;
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
      codegen_->AddAllocatedRegister(input);
    } else if (input.IsPair()) {
      BlockRegister(input.ToLow(), position, position + 1);
      BlockRegister(input.ToHigh(), position, position + 1);
      codegen_->AddAllocatedRegister(input.ToLow());
      codegen_->AddAllocatedRegister(input.ToHigh());
    }
  }
}

void RegisterAllocatorGraphColor::CheckForFixedOutput(HInstruction* instruction) {
  LiveInterval* interval = instruction->GetLiveInterval();
  Location out = interval->GetDefinedBy()->GetLocations()->Out();
  size_t position = instruction->GetLifetimePosition();
  DCHECK_GE(interval->GetEnd() - position, 2u);

  if (out.IsUnallocated() && out.GetPolicy() == Location::kSameAsFirstInput) {
    out = instruction->GetLocations()->InAt(0);
  }

  if (out.IsRegister() || out.IsFpuRegister()) {
    interval->SetRegister(out.reg());
    codegen_->AddAllocatedRegister(out);
    Split(interval, position + 1);
  } else if (out.IsPair()) {
    interval->SetRegister(out.low());
    interval->GetHighInterval()->SetRegister(out.high());
    codegen_->AddAllocatedRegister(out.ToLow());
    codegen_->AddAllocatedRegister(out.ToHigh());
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
      codegen_->AddAllocatedRegister(temp);
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

void RegisterAllocatorGraphColor::AddPotentialInterference(InterferenceNode* from,
                                                           InterferenceNode* to,
                                                           bool both_directions) {
  if (from->Precolored()) {
    // We save space by ignoring outgoing edges from fixed nodes.
  } else if (to->Interval()->IsSlowPathSafepoint()) {
    // Safepoint intervals are only there to count max live registers,
    // so no need to give them incoming interference edges.
    // This is also necessary for correctness, because we don't want nodes
    // to remove themselves from safepoint adjacency sets when they're pruned.
  } else if (to->Precolored()) {
    // It is important that only a single node represents a given fixed register in the
    // interference graph. We retrieve that node here.
    const ArenaVector<InterferenceNode*>& physical_nodes =
        to->Interval()->IsFloatingPoint() ? physical_fp_nodes_ : physical_core_nodes_;
    InterferenceNode* physical_node = physical_nodes[to->Interval()->GetRegister()];
    from->AddInterference(physical_node);
    DCHECK_EQ(to->Interval()->GetRegister(), physical_node->Interval()->GetRegister());
    DCHECK_EQ(to->Alias(), physical_node) << "Fixed nodes should alias the canonical fixed node";

    // TODO
    // If an uncolored singular node interferes with a fixed pair node, the weight of the edge
    // is inaccurate after using the alias of the pair node, because the alias of the pair node
    // is a singular node.
    // We could make special pair fixed nodes, but that ends up being too conservative because
    // a node could then interfere with both {r1} and {r1,r2}, leading to a degree of
    // three rather than two.
    // Instead, we explicitly add an interference with the high node of the fixed pair node.
    if (to->IsPair()) {
      InterferenceNode* high_node =
          physical_nodes[to->Interval()->GetHighInterval()->GetRegister()];
      DCHECK_EQ(to->Interval()->GetHighInterval()->GetRegister(),
                high_node->Interval()->GetRegister());
      from->AddInterference(high_node);
    }
  } else {
    from->AddInterference(to);
  }

  if (both_directions) {
    AddPotentialInterference(to, from, false);
  }
}

// TODO: See locations->OutputCanOverlapWithInputs(); we will want to consider
//       this when building the interference graph.
void RegisterAllocatorGraphColor::BuildInterferenceGraph(
    const ArenaVector<LiveInterval*>& intervals,
    const ArenaVector<InterferenceNode*>& physical_nodes,
    ArenaVector<InterferenceNode*>& safepoints) {
  DCHECK(interval_node_map_.Empty() && prunable_nodes_.empty());

  // Build the interference graph efficiently by ordering range endpoints
  // by position and doing a line sweep to find interferences.
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
            allocator_, sibling, node_id_counter_++);
        interval_node_map_.Insert(std::make_pair(sibling, node));

        if (sibling->HasRegister()) {
          // Fixed nodes should alias the canonical node for the corresponding register.
          node->phase = kPrecolored;
          InterferenceNode* physical_node = physical_nodes[sibling->GetRegister()];
          node->SetAlias(physical_node);
          DCHECK_EQ(node->Interval()->GetRegister(), physical_node->Interval()->GetRegister());
        } else if (sibling->IsSlowPathSafepoint()) {
          // Safepoint intervals are synthesized to count max live registers.
          // They will be processed separately after coloring.
          node->phase = kSafepoint;
          safepoints.push_back(node);
        } else {
          node->phase = kPrunable;
          prunable_nodes_.push_back(node);
        }

        while (range != nullptr) {
          range_endpoints.push_back(std::make_tuple(range->GetStart(), true, node));
          range_endpoints.push_back(std::make_tuple(range->GetEnd(), false, node));
          range = range->GetNext();
        }
      }
    }
  }
  std::sort(range_endpoints.begin(), range_endpoints.end());

  // Nodes covering the current position in the line sweep.
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
        AddPotentialInterference(node, conflicting);
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

void RegisterAllocatorGraphColor::CreateCoalesceOpportunity(InterferenceNode* a,
                                                            InterferenceNode* b) {
  DCHECK_EQ(a->IsPair(), b->IsPair())
      << "Nodes of different memory widths should never be coalesced";
  CoalesceOpportunity* opportunity = new (allocator_) CoalesceOpportunity(a, b);
  a->AddCoalesceOpportunity(opportunity);
  b->AddCoalesceOpportunity(opportunity);
  coalesce_worklist_.push(opportunity);
}

//  static bool OnlyOverlapWhereOutputCannotOverlapWithInputs(LiveInterval* input,
//                                                            LiveInterval* output) {
//    for (LiveRange* range_in = input->GetFirstRange();
//         range_in != nullptr; range_in = range_in->GetNext()) {
//      for (LiveRange* range_out = output->GetFirstRange();
//           range_out != nullptr; range_out = range_out->GetNext()) {
//        if (range_in->IntersectsWith(*range_out)) {
//          if (range_out->GetStart() == output->GetStart()
//              && range_in->GetEnd() == range_out->GetStart() + 1) {
//            // This overlap is ok, since it occurs at the instruction
//            // where outputs cannot overlap with inputs.
//          } else {
//            return false;
//          }
//        }
//      }
//    }
//    return true;
//  }

// TODO: Ideally all intervals would be in the interval_node_map.
void RegisterAllocatorGraphColor::FindCoalesceOpportunities() {
  DCHECK(coalesce_worklist_.empty());

  // TODO: Maybe iterate over fixed nodes too.
  for (InterferenceNode* node : prunable_nodes_) {
    LiveInterval* interval = node->Interval();

    // TODO: Verify that we should ignore intervals not in the interval_node_map.

    // Coalesce siblings.
    LiveInterval* next_sibling = interval->GetNextSibling();
    if (next_sibling != nullptr) {
      auto it = interval_node_map_.Find(next_sibling);
      if (it != interval_node_map_.end()) {
        InterferenceNode* sibling_node = it->second;
        CreateCoalesceOpportunity(node, sibling_node);
      }
    }

    // Coalesce fixed outputs with this interval if this interval is an adjacent sibling.
    // TODO: This could be cleaner.
    LiveInterval* parent = interval->GetParent();
    if (parent->HasRegister()
        && parent->GetNextSibling() == interval
        && parent->GetEnd() == interval->GetStart()) {
      auto it = interval_node_map_.Find(parent);
      if (it != interval_node_map_.end()) {
        InterferenceNode* parent_node = it->second;
        CreateCoalesceOpportunity(node, parent_node);
      }
    }

    // TODO: The following three blocks are partially copied from liveness analysis.
    //       Can we share this code?

    // Try to prevent moves across blocks.
    if (interval->IsSplit() && liveness_.IsAtBlockBoundary(interval->GetStart() / 2)) {
      // If the start of this interval is at a block boundary, we look at the
      // location of the interval in blocks preceding the block this interval
      // starts at. This can avoid a move between the two blocks.
      HBasicBlock* block = liveness_.GetBlockFromPosition(interval->GetStart() / 2);
      for (HBasicBlock* predecessor : block->GetPredecessors()) {
        size_t position = predecessor->GetLifetimeEnd() - 1;
        LiveInterval* existing = interval->GetParent()->GetSiblingAt(position);
        if (existing != nullptr) {
          auto it = interval_node_map_.Find(existing);
          if (it != interval_node_map_.end()) {
            InterferenceNode* existing_node = it->second;
            CreateCoalesceOpportunity(node, existing_node);
          }
        }
      }
    }

    // Coalesce phi inputs with the corresponding output.
    HInstruction* defined_by = interval->GetDefinedBy();
    if (defined_by != nullptr && defined_by->IsPhi()) {
      const ArenaVector<HBasicBlock*>& predecessors = defined_by->GetBlock()->GetPredecessors();
      HInputsRef inputs = defined_by->GetInputs();

      for (size_t i = 0, e = inputs.size(); i < e; ++i) {
        // We want the sibling at the end of the appropriate predecessor block.
        size_t end = predecessors[i]->GetLifetimeEnd();
        LiveInterval* input_interval = inputs[i]->GetLiveInterval()->GetSiblingAt(end - 1);

        auto it = interval_node_map_.Find(input_interval);
        if (it != interval_node_map_.end()) {
          InterferenceNode* input_node = it->second;
          CreateCoalesceOpportunity(node, input_node);
        }
      }
    }

    // An interval that starts an instruction (that is, it is not split), may
    // re-use the registers used by the inputs of that instruction, based on the
    // location summary.
//    if (defined_by != nullptr && !interval->IsSplit()) {
//      LocationSummary* locations = defined_by->GetLocations();
//      if (!locations->OutputCanOverlapWithInputs() && locations->Out().IsUnallocated()) {
//        HInputsRef inputs = defined_by->GetInputs();
//        for (size_t i = 0; i < inputs.size(); ++i) {
//          LiveInterval* input_interval =
//              inputs[i]->GetLiveInterval()->GetSiblingAt(interval->GetStart());
//          if (locations->InAt(i).IsValid()
//              && input_interval->SameRegisterKind(*interval)
//              && input_interval->HasHighInterval() == interval->HasHighInterval()) {
//            auto it = interval_node_map_.Find(input_interval);
//            if (it != interval_node_map_.end()) {
//              DCHECK(input_interval->CoversSlow(defined_by->GetLifetimePosition()));
//              size_t position = defined_by->GetLifetimePosition() + 1;
//              // TODO: This check is too conservative.
//              if (input_interval->IsDeadAt(position)) {
//                InterferenceNode* input_node = it->second;
//                CreateCoalesceOpportunity(node, input_node);
//                // Right now, these live intervals overlap, so we explicitly remove interferences.
//                // TODO: Ideally we should just produce correct intervals in liveness analysis.
//                //       May need to refactor the current live interval layout to do so.
//                input_node->RemoveInterference(node);
//                node->RemoveInterference(input_node);
//              }
//            }
//          }
//        }
//      }
//    }

    // TODO: Could coalesce intervals with fixed register uses. Especially useful for
    //       lifetimes ending at calls.
    // TODO: Handle OutputSameAsFirstInput coalescing.
  }
}

// The order in which we color nodes is vital to both correctness (forward
// progress) and code quality.
// TODO: May also want to consider:
// - Loop depth
// - Constants (since they can be rematerialized)
// - Allocated spill slots
bool RegisterAllocatorGraphColor::ChooseHigherPriorityNode(const InterferenceNode* lhs,
                                                           const InterferenceNode* rhs) {
  LiveInterval* lhs_interval = lhs->Interval();
  LiveInterval* rhs_interval = rhs->Interval();

  // (1) Choose the interval that requires a register.
  if (lhs_interval->RequiresRegister() != rhs_interval->RequiresRegister()) {
    return lhs_interval->RequiresRegister();
  }

  // (2) Choose the interval that has a shorter life span.
  if (lhs_interval->GetLength() != rhs_interval->GetLength()) {
    return lhs_interval->GetLength() < rhs_interval->GetLength();
  }

  // (3) Just choose the interval based on a deterministic ordering.
  // TODO: Tie breaker not necessary for a deque.
  return InterferenceNode::CmpPtr(lhs, rhs);
}

bool RegisterAllocatorGraphColor::CmpCoalesceOpportunity(const CoalesceOpportunity* lhs,
                                                         const CoalesceOpportunity* rhs) {
  return lhs->priority < rhs->priority;
}

void RegisterAllocatorGraphColor::PruneInterferenceGraph(size_t num_regs) {
  DCHECK(pruned_nodes_.empty()
      && simplify_worklist_.empty()
      && freeze_worklist_.empty()
      && spill_worklist_.empty());

  // Build worklists. Note that the coalesce worklist has already been
  // filled by FindCoalesceOpportunities().
  for (InterferenceNode* node : prunable_nodes_) {
    DCHECK(!node->Precolored()) << "Fixed nodes should never be pruned";
    DCHECK(!node->Interval()->IsSlowPathSafepoint()) << "Safepoint nodes should never be pruned";
    if (node->Degree() < num_regs) {
      if (node->CoalesceOpportunities().empty()) {
        // Simplify Worklist.
        node->phase = kSimplifyWorklist;
        simplify_worklist_.push_back(node);
      } else {
        // Freeze Worklist.
        node->phase = kFreezeWorklist;
        freeze_worklist_.push_back(node);
      }
    } else {
      // Spill worklist.
      node->phase = kSpillWorklist;
      spill_worklist_.push(node);
    }
  }

  // Prune graph.
  // Note that we do not remove nodes from worklists, so they may be in multiple worklists at
  // once; the node's `phase` says which worklist it is really in.
  while (true) {
    // TODO: Could assert things about node here.
    if (!simplify_worklist_.empty()) {
      // Prune low-degree nodes.
      // TODO: pop_back() should work as well, but it didn't; we get a
      //       failed check while pruning. We should look into this.
      InterferenceNode* node = simplify_worklist_.front();
      simplify_worklist_.pop_front();
      DCHECK_EQ(node->phase, kSimplifyWorklist) << "Cannot transition away from simplify worklist";
      DCHECK_LT(node->Degree(), num_regs) << "Nodes in simplify worklist should be low degree";
      DCHECK(!node->MoveRelated()) << "Nodes in the simplify worklist should not be move related";
      PruneNode(node, num_regs);
    } else if (!coalesce_worklist_.empty()) {
      // Coalesce.
      CoalesceOpportunity* opportunity = coalesce_worklist_.top();
      coalesce_worklist_.pop();
      if (opportunity->phase == kWorklist) {
        Coalesce(opportunity, num_regs);
      }
    } else if (!freeze_worklist_.empty()) {
      // Freeze moves and prune a low-degree move-related node.
      InterferenceNode* node = freeze_worklist_.front();
      freeze_worklist_.pop_front();
      if (node->phase == kFreezeWorklist) {
        DCHECK_LT(node->Degree(), num_regs) << "Nodes in the freeze worklist should be low degree";
        DCHECK(node->MoveRelated()) << "Nodes in the freeze worklist should be move related";
        FreezeMoves(node);
        PruneNode(node, num_regs);
      }
    } else if (!spill_worklist_.empty()) {
      // We spill the lowest-priority node, because pruning a node earlier
      // gives it a higher chance of being spilled.
      InterferenceNode* node = spill_worklist_.top();
      spill_worklist_.pop();
      if (node->phase == kSpillWorklist) {
        DCHECK_GE(node->Degree(), num_regs) << "Nodes in the spill worklist should be high degree";
        FreezeMoves(node);
        PruneNode(node, num_regs);
      }
    } else {
      // Pruning complete.
      break;
    }
  }
  DCHECK_EQ(prunable_nodes_.size(), pruned_nodes_.size());
}

void RegisterAllocatorGraphColor::EnableCoalesceOpportunities(InterferenceNode* node) {
  for (CoalesceOpportunity* opportunity : node->CoalesceOpportunities()) {
    if (opportunity->phase == kActive) {
      opportunity->phase = kWorklist;
      coalesce_worklist_.push(opportunity);
    }
  }
}

void RegisterAllocatorGraphColor::PruneNode(InterferenceNode* node,
                                            size_t num_regs) {
  DCHECK_NE(node->phase, kPruned);
  DCHECK(!node->Precolored());
  node->phase = kPruned;
  pruned_nodes_.push(node);
  // TODO: Appel doesn't do this, but if high degree, enable moves for all neighbors?

  for (InterferenceNode* adj : node->Adj()) {
    DCHECK(!adj->Interval()->IsSlowPathSafepoint())
        << "Nodes should never interfere with synthesized safepoint nodes";
    DCHECK_NE(!adj->phase, kPruned) << "Should be no interferences with pruned nodes";

    if (adj->Precolored()) {
      // No effect on pre-colored nodes; they're never pruned.
    } else {
      // Remove the interference.
      bool was_high_degree = adj->Degree() >= num_regs;
      DCHECK(adj->HasInterference(node))
          << "Missing reflexive interference from non-fixed node";
      adj->RemoveInterference(node);

      // Handle transitions from high degree to low degree.
      if (was_high_degree && adj->Degree() < num_regs) {
        EnableCoalesceOpportunities(adj);
        for (InterferenceNode* adj_adj : adj->Adj()) {
          EnableCoalesceOpportunities(adj_adj);
        }

        DCHECK_EQ(adj->phase, kSpillWorklist);
        if (adj->MoveRelated()) {
          adj->phase = kFreezeWorklist;
          freeze_worklist_.push_back(adj);
        } else {
          adj->phase = kSimplifyWorklist;
          simplify_worklist_.push_back(adj);
        }
      }
    }
  }
}

void RegisterAllocatorGraphColor::FreezeMoves(InterferenceNode* node) {
  for (CoalesceOpportunity* opportunity : node->CoalesceOpportunities()) {
    opportunity->phase = kDefunct;
    InterferenceNode* other = opportunity->a->Alias() == node
        ? opportunity->b->Alias()
        : opportunity->a->Alias();
    if (other != node && other->phase == kFreezeWorklist && !other->MoveRelated()) {
      other->phase = kSimplifyWorklist;
      simplify_worklist_.push_back(other);
    }
  }
}

void RegisterAllocatorGraphColor::CheckTransitionFromFreezeWorklist(InterferenceNode* node,
                                                                    size_t num_regs) {
  if (node->Degree() < num_regs && !node->MoveRelated()) {
    DCHECK_EQ(node->phase, kFreezeWorklist);
    node->phase = kSimplifyWorklist;
    simplify_worklist_.push_back(node);
  }
}

bool RegisterAllocatorGraphColor::PrecoloredHeuristic(InterferenceNode* from,
                                                      InterferenceNode* into,
                                                      size_t num_regs) {
  if (!into->Precolored()) {
    // The uncolored heuristic will cover this case.
    return false;
  }
  if (from->IsPair() || into->IsPair()) {
    // TODO: Merging from a pair node is currently not supported, since fixed pair nodes
    //       are currently represented as two single fixed nodes in the graph, and `into` is
    //       only one of them. It would probably be best to create special fixed pair nodes
    //       to fix this situation.
    return false;
  }

  // Reasons an adjacent node can be "ok":
  // (1) If `adj` is low degree, interference with `into` will not affect its existing
  //     colorable guarantee. (Notice that coalescing cannot increase its degree.)
  // (2) If `adj` is pre-colored, it already interferes with `into`. See (3).
  // (3) If there's already an interference with `into`, coalescing will not add interferences.
  for (InterferenceNode* adj : from->Adj()) {
    if (adj->Degree() < num_regs || adj->Precolored() || adj->HasInterference(into)) {
      // Ok.
    } else {
      return false;
    }
  }
  return true;
}

bool RegisterAllocatorGraphColor::UncoloredHeuristic(InterferenceNode* from,
                                                     InterferenceNode* into,
                                                     size_t num_regs) {
  if (into->Precolored()) {
    // The pre-colored heuristic will handle this case.
    return false;
  }

  // It's safe to coalesce two nodes if the resulting node has fewer than `num_regs` interferences
  // with nodes of high degree.
  size_t high_degree_interferences = 0;
  for (InterferenceNode* adj : from->Adj()) {
    if (adj->Degree() >= num_regs) {
      high_degree_interferences += from->EdgeWeightWith(adj);
    }
  }
  for (InterferenceNode* adj : into->Adj()) {
    if (from->HasInterference(adj)) {
      // We've already counted this adjacent node.
      // Furthermore, its degree will decrease if coalescing succeeds. Thus, it's possible that
      // we should not have counted it at all. (This extends the original Briggs coalescing test,
      // but remains conservative.)
      if (adj->Degree() - into->EdgeWeightWith(adj) < num_regs) {
        // TODO: Doesn't work right now; within degree 2 of being sufficiently conservative.
        // high_degree_interferences -= into->EdgeWeightWith(adj);
      }
    } else {
      high_degree_interferences += into->EdgeWeightWith(adj);
    }
  }

  return high_degree_interferences < num_regs;
}

void RegisterAllocatorGraphColor::Combine(InterferenceNode* from,
                                          InterferenceNode* into,
                                          size_t num_regs) {
  from->SetAlias(into);

  // Add interferences.
  for (InterferenceNode* adj : from->Adj()) {
    bool was_low_degree = adj->Degree() < num_regs;
    AddPotentialInterference(adj, into);
    if (was_low_degree && adj->Degree() >= num_regs) {
      // This is a (temporary) transition to a high degree node. Its degree will decrease again
      // when we prune `from`, but it's best to be consistent about the current worklist.
      // TUNING: Could remove this.
      adj->phase = kSpillWorklist;
      spill_worklist_.push(adj);
    }
  }

  // Add coalesce opportunities.
  for (CoalesceOpportunity* opportunity : from->CoalesceOpportunities()) {
    if (opportunity->phase != kDefunct) {
      into->AddCoalesceOpportunity(opportunity);
    }
  }
  EnableCoalesceOpportunities(from);

  // Prune and update worklists.
  PruneNode(from, num_regs);
  if (into->Degree() < num_regs) {
    // Coalesce(...) takes care of checking for a transition to the simplify worklist.
    DCHECK_EQ(into->phase, kFreezeWorklist);
  } else if (into->phase == kFreezeWorklist) {
    // This is a transition to a high degree node.
    into->phase = kSpillWorklist;
    spill_worklist_.push(into);
  } else {
    DCHECK(into->phase == kSpillWorklist || into->phase == kPrecolored);
  }
}

void RegisterAllocatorGraphColor::Coalesce(CoalesceOpportunity* opportunity,
                                           size_t num_regs) {
  InterferenceNode* from = opportunity->a->Alias();
  InterferenceNode* into = opportunity->b->Alias();
  DCHECK(from->phase != kPruned && into->phase != kPruned);

  if (from->Precolored()) {
    // If we have one pre-colored node, make sure it's the `into` node.
    std::swap(from, into);
  }

  if (from == into) {
    // These nodes have already been coalesced.
    opportunity->phase = kDefunct;
    CheckTransitionFromFreezeWorklist(from, num_regs);
  } else if (from->Precolored() || from->HasInterference(into)) {
    // These nodes interfere.
    opportunity->phase = kDefunct;
    CheckTransitionFromFreezeWorklist(from, num_regs);
    CheckTransitionFromFreezeWorklist(into, num_regs);
  } else if (PrecoloredHeuristic(from, into, num_regs)
          || UncoloredHeuristic(from, into, num_regs)) {
    // We can coalesce these nodes.
    opportunity->phase = kDefunct;
    Combine(from, into, num_regs);
    CheckTransitionFromFreezeWorklist(into, num_regs);
  } else {
    // We cannot coalesce, but we may be able to later.
    opportunity->phase = kActive;
  }
}

// Build a mask with a bit set for each register assigned to some
// interval in `intervals`.
template <typename Container>
static uint64_t BuildConflictMask(Container& intervals) {
  uint64_t conflict_mask = 0;
  for (InterferenceNode* adj : intervals) {
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
  return conflict_mask;
}

bool RegisterAllocatorGraphColor::ColorInterferenceGraph(size_t num_regs,
                                                         bool processing_core_regs) {
  DCHECK_LE(num_regs, 64u);
  size_t& max_safepoint_live_regs = processing_core_regs
      ? max_safepoint_live_core_regs_
      : max_safepoint_live_fp_regs_;
  ArenaVector<LiveInterval*> colored_intervals(
      allocator_->Adapter(kArenaAllocRegisterAllocator));
  bool successful = true;

  while (!pruned_nodes_.empty()) {
    InterferenceNode* node = pruned_nodes_.top();
    pruned_nodes_.pop();
    LiveInterval* interval = node->Interval();
    DCHECK(!interval->HasRegister());
    size_t reg = 0;

    InterferenceNode* alias = node->Alias();
    if (alias != node) {
      // This node was coalesced with another.
      LiveInterval* alias_interval = alias->Interval();
      if (alias_interval->HasRegister()) {
        reg = alias_interval->GetRegister();
        DCHECK_EQ(0u, (1u << reg) & BuildConflictMask(node->Adj()))
            << "This node conflicts with the register it was coalesced with";
      } else {
        DCHECK(false) << node->Degree() << " " << alias->Degree() << " "
            << "Move coalescing was not conservative, causing a node to be coalesced "
            << "with another node that could not be colored";
        if (interval->RequiresRegister()) {
          successful = false;
        }
      }
    } else {
      // Search for free register(s).
      uint64_t conflict_mask = BuildConflictMask(node->Adj());
      if (interval->HasHighInterval()) {
        // TODO: We had assumed that pair intervals were always aligned and possibly even needed
        //       to be aligned, yet some fixed pair intervals are not. Still, the graph coloring
        //       algorithm assumes that *uncolored* nodes will be aligned, so if we change
        //       the alignment requirements here, we will have to update the algorithm (e.g.,
        //       be more conservative about the weight of edges adjacent to pair nodes.)
        while (reg < num_regs - 1
            && ((conflict_mask & (1u << reg)) || (conflict_mask & (1u << (reg + 1))))) {
          reg += 2;
        }
      } else {
        // Flip the bits of conflict_mask and find the first 1,
        // indicating a free register. Note that CTZ(0) is undefined.
        // TODO: We could do more to preserve free register pairs here (if targeting 32-bit).
        reg = (~conflict_mask == 0u) ? 64u : CTZ(~conflict_mask);

        // Last-chance coalescing.
        // TODO: May be beneficial to pick the register with the highest count among
        //       coalesce candidates.
        // TODO: We should do the same for pair intervals, but first attempts to do so
        //       led to an extreme number of attempts needed to color the graph on 32-bit.
        //       Need to look into this.
        for (CoalesceOpportunity* opportunity : node->CoalesceOpportunities()) {
          LiveInterval* other_interval = opportunity->a->Alias() == node
              ? opportunity->b->Alias()->Interval()
              : opportunity->a->Alias()->Interval();
          if (other_interval->HasRegister()) {
            size_t coalesce_register = other_interval->GetRegister();
            if (~conflict_mask & (1u << coalesce_register)) {
              reg = coalesce_register;
              break;
            }
          }
        }
      }
    }

    if (reg < (interval->HasHighInterval() ? num_regs - 1 : num_regs)) {
      // Assign register.
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

void RegisterAllocatorGraphColor::ComputeMaxSafepointLiveRegisters(
    ArenaVector<InterferenceNode*>& safepoints,
    size_t& max_safepoint_live_regs) {
  for (InterferenceNode* safepoint : safepoints) {
    // We added a synthesized range to record the live registers
    // at this position. Ideally, we could just update the safepoints when locations
    // are updated, but we currently need to know the full stack size before updating
    // locations (because of parameters and the fact that we don't have a frame pointer).
    // registers at calls in slow paths.
    // TODO: This counts code-generator-blocked registers such as the stack
    //       pointer. Is this necessary?
    DCHECK(safepoint->Interval()->IsSlowPathSafepoint());
    size_t conflict_mask = BuildConflictMask(safepoint->Adj());
    size_t live_regs = static_cast<size_t>(POPCOUNT(conflict_mask));
    max_safepoint_live_regs = std::max(max_safepoint_live_regs, live_regs);
  }
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
