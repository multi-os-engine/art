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

namespace art {

// TODO: Factor out into register_allocator_common
static int GetHighForLowRegister(int reg) { return reg + 1; }

RegisterAllocatorGraphColor::RegisterAllocatorGraphColor(ArenaAllocator* allocator,
                                                         CodeGenerator* codegen,
                                                         const SsaLivenessAnalysis& liveness)
      : RegisterAllocatorCommon(allocator, codegen, liveness),
        core_intervals_(allocator->Adapter(kArenaAllocRegisterAllocator)),
        fp_intervals_(allocator->Adapter(kArenaAllocRegisterAllocator)),
        temp_intervals_(allocator->Adapter(kArenaAllocRegisterAllocator)),
        safepoints_(allocator->Adapter(kArenaAllocRegisterAllocator)),
        physical_core_intervals_(allocator->Adapter(kArenaAllocRegisterAllocator)),
        physical_fp_intervals_(allocator->Adapter(kArenaAllocRegisterAllocator)),
        pruned_intervals_(allocator->Adapter(kArenaAllocRegisterAllocator)),
        spilled_intervals_(allocator->Adapter(kArenaAllocRegisterAllocator)) {
  // Initialize physical core register live intervals.
  physical_core_intervals_.resize(codegen->GetNumberOfCoreRegisters(), nullptr);
  for (size_t i = 0; i < codegen->GetNumberOfCoreRegisters(); ++i) {
    LiveInterval* interval = LiveInterval::MakeFixedInterval(allocator_, i, Primitive::kPrimInt);
    physical_core_intervals_[i] = interval;
    fp_intervals_.push_back(interval);
  }
  // Initialize physical floating point register live intervals.
  physical_fp_intervals_.resize(codegen->GetNumberOfFloatingPointRegisters(), nullptr);
  for (size_t i = 0; i < codegen->GetNumberOfFloatingPointRegisters(); ++i) {
    LiveInterval* interval = LiveInterval::MakeFixedInterval(allocator_, i, Primitive::kPrimFloat);
    physical_fp_intervals_[i] = interval;
    fp_intervals_.push_back(interval);
  }

  // TODO: Factor out into register_allocator_common
  reserved_out_slots_ = InstructionSetPointerSize(codegen->GetInstructionSet()) / kVRegSize +
        codegen->GetGraph()->GetMaximumNumberOfOutVRegs();
}

void RegisterAllocatorGraphColor::AllocateRegisters() {
  ProcessInstructions();

  for (bool processing_core_regs : {true, false}) {
    ArenaVector<LiveInterval*>& intervals = processing_core_regs
        ? core_intervals_
        : fp_intervals_;
    size_t num_registers = processing_core_regs
        ? codegen_->GetNumberOfCoreRegisters()
        : codegen_->GetNumberOfFloatingPointRegisters();
    BuildInterferenceGraph(intervals);
    PruneInterferenceGraph(num_registers);
    ColorInterferenceGraph(num_registers, processing_core_regs);
  }

  Resolve();

  // TODO
//  for (LiveInterval* interval : core_intervals_) {
//    if (interval->GetDefinedBy() != nullptr)
//      LOG(ERROR) << interval->GetDefinedBy()->GetKind();
//    interval->Dump(LOG(ERROR));
//    LOG(ERROR) << interval->GetRegister();
//  }
//  for (LiveInterval* interval : fp_intervals_) {
//    if (interval->GetDefinedBy() != nullptr)
//      LOG(ERROR) << interval->GetDefinedBy()->GetKind();
//    interval->Dump(LOG(ERROR));
//    LOG(ERROR) << interval->GetRegister();
//  }
}

// TODO: Factor out into register_allocator_common
void RegisterAllocatorGraphColor::ProcessInstructions() {
  for (HLinearPostOrderIterator it(*codegen_->GetGraph()); !it.Done(); it.Advance()) {
    HBasicBlock* block = it.Current();

    // TODO: Might be able to iterate using just Ssa indices.
    //       Right now, though, some helper code (e.g., AddRange in
    //       ssa_liveness_analysis, depends on the ordering).

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
  if (locations == nullptr)
    return;
  if (locations->NeedsSafepoint() && codegen_->IsLeafMethod()) {
    // TODO: We do this here because we do not want the suspend check to artificially
    // create live registers. We should find another place, but this is currently the
    // simplest.
    DCHECK(instruction->IsSuspendCheckEntry());
    DCHECK_EQ(locations->GetTempCount(), 0u);
    instruction->GetBlock()->RemoveInstruction(instruction);
    return;
  }

  CollectTempLiveIntervals(instruction);
  CheckLocationsForSafepoint(instruction);

  // If a call will happen, create fixed intervals for caller-save registers.
  if (instruction->GetLocations()->WillCall()) {
    BlockRegisters(instruction->GetLifetimePosition(),
                   instruction->GetLifetimePosition() + 1,
                   /*caller_save_only*/ true);
  }

  ProcessFixedInputLocationsFor(instruction);

  LiveInterval* interval = instruction->GetLiveInterval();
  if (interval == nullptr)
    return;

  if (codegen_->NeedsTwoRegisters(interval->GetType())) {
    interval->AddHighInterval();
  }

  ProcessSafepointsFor(instruction);
  ProcessFixedOutputLocationFor(instruction);

  // TODO: Catch phi spill slots

  bool core_register = (instruction->GetType() != Primitive::kPrimDouble)
                    && (instruction->GetType() != Primitive::kPrimFloat);
  ArenaVector<LiveInterval*>& intervals = core_register
      ? core_intervals_
      : fp_intervals_;

  // TODO: Factor out into register_allocator_common
  if (interval->HasSpillSlot() || instruction->IsConstant()) {
    // Split just before first register use. TODO: Why?
    size_t first_register_use = interval->FirstRegisterUse();
    if (first_register_use != kNoLifetime) {
      LiveInterval* split = SplitBetween(interval, interval->GetStart(), first_register_use - 1);
      intervals.push_back(split);
    } else {
      // Nothing to do, we won't allocate a register for this value.
    }
  } else {
    intervals.push_back(interval);
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

LiveInterval* RegisterAllocatorGraphColor::Split(LiveInterval* interval,
                                                 size_t position) {
  DCHECK_GT(position, interval->GetStart());
  DCHECK(!interval->IsDeadAt(position));
  LiveInterval* new_interval = interval->SplitAt(position);
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

// TODO: Factor out into register_allocator_common
void RegisterAllocatorGraphColor::CollectTempLiveIntervals(HInstruction* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  for (size_t i = 0; i < locations->GetTempCount(); ++i) {
    Location temp = locations->GetTemp(i);
    if (temp.IsRegister() || temp.IsFpuRegister()) {
      // Ensure that an explicit temporary register is marked as being allocated.
      // TODO: It would be nice to do this elsewhere
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
          if (codegen_->NeedsTwoRegisters(Primitive::kPrimDouble)) {
            interval->AddHighInterval(/*is_temp*/ true);
            LiveInterval* high = interval->GetHighInterval();
            fp_intervals_.push_back(high);
            temp_intervals_.push_back(high);
          }
          fp_intervals_.push_back(interval);
          temp_intervals_.push_back(interval);
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
void RegisterAllocatorGraphColor::CheckLocationsForSafepoint(HInstruction* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  size_t position = instruction->GetLifetimePosition();

  if (locations->NeedsSafepoint()) {
    // TODO: safepoints_.push_back(instruction);
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

// TODO: Factor out into register_allocator_common
void RegisterAllocatorGraphColor::ProcessFixedInputLocationsFor(HInstruction* instruction) {
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

// TODO: What links the physical intervals with the instruction interval?
//       Perhaps for graph coloring we'll just want to do a split...
void RegisterAllocatorGraphColor::ProcessFixedOutputLocationFor(HInstruction* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  LiveInterval* interval = instruction->GetLiveInterval();
  size_t position = instruction->GetLifetimePosition();
  Location output = locations->Out();
  if (output.IsUnallocated() && output.GetPolicy() == Location::kSameAsFirstInput) {
    Location first = locations->InAt(0);
    if (first.IsRegister() || first.IsFpuRegister()) {
      interval->SetFrom(position + 1);
      interval->SetRegister(first.reg());
    } else if (first.IsPair()) {
      interval->SetFrom(position + 1);
      interval->SetRegister(first.low());
      LiveInterval* high = interval->GetHighInterval();
      high->SetRegister(first.high());
      high->SetFrom(position + 1);
    }
  } else if (output.IsRegister() || output.IsFpuRegister()) {
    // Shift the interval's start by one to account for the blocked register.
    interval->SetFrom(position + 1);
    interval->SetRegister(output.reg());
    BlockRegister(output, position, position + 1);
  } else if (output.IsPair()) {
    interval->SetFrom(position + 1);
    interval->SetRegister(output.low());
    LiveInterval* high = interval->GetHighInterval();
    high->SetRegister(output.high());
    high->SetFrom(position + 1);
    BlockRegister(output.ToLow(), position, position + 1);
    BlockRegister(output.ToHigh(), position, position + 1);
  } else if (output.IsStackSlot() || output.IsDoubleStackSlot()) {
    interval->SetSpillSlot(output.GetStackIndex());
  } else {
    DCHECK(output.IsUnallocated() || output.IsConstant());
  }
}

// TODO: Factor out into register_allocator_common
void RegisterAllocatorGraphColor::BlockRegister(Location location,
                                                size_t start,
                                                size_t end) {
  DCHECK(location.IsRegister() || location.IsFpuRegister());
  int reg = location.reg();
  LiveInterval* interval = location.IsRegister()
      ? physical_core_intervals_[reg]
      : physical_fp_intervals_[reg];
  DCHECK(interval->GetRegister() == reg);
  interval->AddRange(start, end);
}

// TODO: Factor out into register_allocator_common
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

// TODO: Move to definition of LiveInterval class
class AllRangesIterator : public ValueObject {
 public:
  explicit AllRangesIterator(LiveInterval* interval)
      : current_interval_(interval),
        current_range_(interval->GetFirstRange()) {}

  // TODO: This assumes that an interval with no live ranges does not have a sibling
  bool Done() const { return current_interval_ == nullptr || current_range_ == nullptr; }
  LiveRange* CurrentRange() const { return current_range_; }
  LiveInterval* CurrentInterval() const { return current_interval_; }

  void Advance() {
    current_range_ = current_range_->GetNext();
    if (current_range_ == nullptr) {
      current_interval_ = current_interval_->GetNextSibling();
      if (current_interval_ != nullptr) {
        current_range_ = current_interval_->GetFirstRange();
      }
    }
  }

 private:
  LiveInterval* current_interval_;
  LiveRange* current_range_;

  DISALLOW_COPY_AND_ASSIGN(AllRangesIterator);
};

void RegisterAllocatorGraphColor::BuildInterferenceGraph(ArenaVector<LiveInterval*>& intervals) {
  // Build the interference graph efficiently by ordering range endpoints
  // by position and doing a line-sweep to find interferences.
  // We order by both position and (secondarily) by whether the endpoint
  // begins or ends a range; we want to process range endings before range
  // beginnings at the same position because they should not conflict.
  // Tuple contents: (position, is_range_beginning, interval).
  ArenaVector<std::tuple<size_t, bool, LiveInterval*>> range_endpoints(
      allocator_->Adapter(kArenaAllocRegisterAllocator));
  for (LiveInterval* interval : intervals) {
    for (AllRangesIterator it(interval); !it.Done(); it.Advance()) {
      LiveRange* range = it.CurrentRange();
      DCHECK_NE(range->GetStart(), range->GetEnd());
      range_endpoints.push_back(std::make_tuple(range->GetStart(), true, interval));
      range_endpoints.push_back(std::make_tuple(range->GetEnd(), false, interval));
    }
  }
  std::sort(range_endpoints.begin(), range_endpoints.end());

  // Line sweep.
  ArenaHashSet<LiveInterval*> live(allocator_->Adapter(kArenaAllocRegisterAllocator));
  for (auto it = range_endpoints.begin(); it != range_endpoints.end(); ++it) {
    bool is_range_beginning;
    LiveInterval* current;
    std::tie(std::ignore, is_range_beginning, current) = *it;
    if (is_range_beginning) {
      DCHECK(live.Find(current) == live.end());
      for (LiveInterval* conflicting : live) {
        interference_graph_[current].insert(conflicting);
        interference_graph_[conflicting].insert(current);
      }
      live.Insert(current);
    } else {
      DCHECK(live.Find(current) != live.end());
      live.Erase(live.Find(current));
    }
  }
  DCHECK(live.Empty());
}

void RegisterAllocatorGraphColor::PruneInterferenceGraph(size_t num_regs) {
  ArenaDeque<LiveInterval*> low_degree_worklist_(
      allocator_->Adapter(kArenaAllocRegisterAllocator));
  ArenaHashSet<LiveInterval*> high_degree_worklist_(
      allocator_->Adapter(kArenaAllocRegisterAllocator));

  for (auto& node_and_adj : interference_graph_) {
    LiveInterval* interval = node_and_adj.first;
    auto& adj = node_and_adj.second;
    if (interval->HasRegister()) {
      // Never prune physical register intervals
    } else if (adj.size() < num_regs) {
      low_degree_worklist_.push_back(interval);
    } else {
      high_degree_worklist_.Insert(interval);
    }
  }

  auto prune_interval = [&] (LiveInterval* interval) {
    pruned_intervals_.push_back(interval);
    for (LiveInterval* adj : interference_graph_[interval]) {
      auto& adj_set = interference_graph_[adj];
      if (adj_set.size() == num_regs) {
        // This is a transition from high degree to low degree.
        high_degree_worklist_.Erase(high_degree_worklist_.Find(adj));
        low_degree_worklist_.push_back(adj);
      }
      adj_set.erase(interval);
    }
  };

  while (!low_degree_worklist_.empty() || !high_degree_worklist_.Empty()) {
    while (!low_degree_worklist_.empty()) {
      LiveInterval* interval = low_degree_worklist_.back();
      prune_interval(interval);
      low_degree_worklist_.pop_back();
    }
    if (!high_degree_worklist_.Empty()) {
      auto cmp_degree = [this] (LiveInterval* lhs, LiveInterval* rhs) {
        return interference_graph_[lhs].size() < interference_graph_[rhs].size();
      };
      // TODO: Perhaps picking the highest degree node is too slow, and unnecessary.
      LiveInterval* interval = *std::max_element(high_degree_worklist_.begin(),
                                                 high_degree_worklist_.end(),
                                                 cmp_degree);
      prune_interval(interval);
      high_degree_worklist_.Erase(high_degree_worklist_.Find(interval));
    }
  }
}

void RegisterAllocatorGraphColor::ColorInterferenceGraph(size_t num_regs,
                                                         bool processing_core_regs) {
  std::vector<bool> free_regs(num_regs);  // TODO: Always use ArenaVector?
  while (!pruned_intervals_.empty()) {
    // TODO: Need to handle high/low intervals separately?
    LiveInterval* interval = pruned_intervals_.back();
    pruned_intervals_.pop_back();
    std::fill(free_regs.begin(), free_regs.end(), true);
    for (LiveInterval* adj : interference_graph_[interval]) {
      if (adj->HasRegister()) {
        free_regs[adj->GetRegister()] = false;
      } else {
        DCHECK(adj->HasSpillSlot() || adj->GetParent()->GetDefinedBy()->IsConstant());
      }
    }

    size_t i = 0;
    while (i < num_regs && !free_regs[i]) {
      ++i;
    }

    if (i < num_regs) {
      interval->SetRegister(i);
      if (interval->HasHighInterval() && !interval->GetHighInterval()->HasRegister()) {
        interval->GetHighInterval()->SetRegister(GetHighForLowRegister(i));
      }
      Location location = processing_core_regs
          ? Location::RegisterLocation(i)
          : Location::FpuRegisterLocation(i);
      codegen_->AddAllocatedRegister(location);
    } else {
      interval->SetSpillSlot(0);  // TODO: See AllocateSpillSlotFor in register_allocator_linear_scan
      DCHECK(false);
    }
  }
}

// TODO: Factor out into register_allocator_common (but first refactor into several methods)
// TODO: Verify that there is no linear-scan-specific code here (hint: there is)
void RegisterAllocatorGraphColor::Resolve() {
  codegen_->InitializeCodeGeneration(0,  // TODO
                                     0,  // TODO
                                     0,  // TODO
                                     reserved_out_slots_,
                                     codegen_->GetGraph()->GetLinearOrder());

  // Adjust the Out Location of instructions.
  // TODO: Use pointers of Location inside LiveInterval to avoid doing another iteration.
  for (size_t i = 0, e = liveness_.GetNumberOfSsaValues(); i < e; ++i) {
    HInstruction* instruction = liveness_.GetInstructionFromSsaIndex(i);
    LiveInterval* interval = instruction->GetLiveInterval();
    LocationSummary* locations = instruction->GetLocations();
    Location out = locations->Out();
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
    } else if (instruction->IsPhi() && instruction->AsPhi()->IsCatchPhi()) {
      DCHECK(interval->HasSpillSlot());
//      size_t slot = interval->GetSpillSlot()
//                    + GetNumberOfSpillSlots()
//                    + reserved_out_slots_
//                    - catch_phi_spill_slots_;
      size_t slot = 0;  // TODO
      interval->SetSpillSlot(slot * kVRegSize);
    } else if (interval->HasSpillSlot()) {
      // Adjust the stack slot, now that we know the number of them for each type.
      // The way this implementation lays out the stack is the following:
      // [parameter slots       ]
      // [catch phi spill slots ]
      // [double spill slots    ]
      // [long spill slots      ]
      // [float spill slots     ]
      // [int/ref values        ]
      // [maximum out values    ] (number of arguments for calls)
      // [art method            ].
      size_t slot = interval->GetSpillSlot();
      switch (interval->GetType()) {
        case Primitive::kPrimDouble:
          // slot += long_spill_slots_.size();  // TODO
          FALLTHROUGH_INTENDED;
        case Primitive::kPrimLong:
          // slot += float_spill_slots_.size();  // TODO
          FALLTHROUGH_INTENDED;
        case Primitive::kPrimFloat:
          // slot += int_spill_slots_.size();  // TODO
          FALLTHROUGH_INTENDED;
        case Primitive::kPrimNot:
        case Primitive::kPrimInt:
        case Primitive::kPrimChar:
        case Primitive::kPrimByte:
        case Primitive::kPrimBoolean:
        case Primitive::kPrimShort:
          slot += reserved_out_slots_;
          break;
        case Primitive::kPrimVoid:
          LOG(FATAL) << "Unexpected type for interval " << interval->GetType();
      }
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
      DCHECK(source.Equals(out)) << "Source: " << source << ", Out: " << out
                                 << ", Instruction: " << instruction->GetKind();
    }
  }

  // Connect siblings.
  for (size_t i = 0, e = liveness_.GetNumberOfSsaValues(); i < e; ++i) {
    HInstruction* instruction = liveness_.GetInstructionFromSsaIndex(i);
    ConnectSiblings(instruction->GetLiveInterval());
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

// TODO: Factor out into ssa_deconstruction
void RegisterAllocatorGraphColor::ConnectSiblings(LiveInterval* interval) {
  LiveInterval* current = interval;
  if (current->HasSpillSlot()
      && current->HasRegister()
      // Currently, we spill unconditionnally the current method in the code generators.
      && !interval->GetDefinedBy()->IsCurrentMethod()) {
    // We spill eagerly, so move must be at definition.
    InsertMoveAfter(interval->GetDefinedBy(),
                    interval->ToLocation(),
                    interval->NeedsTwoSpillSlots()
                        ? Location::DoubleStackSlot(interval->GetParent()->GetSpillSlot())
                        : Location::StackSlot(interval->GetParent()->GetSpillSlot()));
  }
  UsePosition* use = current->GetFirstUse();
  UsePosition* env_use = current->GetFirstEnvironmentUse();

  // Walk over all siblings, updating locations of use positions, and
  // connecting them when they are adjacent.
  do {
    Location source = current->ToLocation();

    // Walk over all uses covered by this interval, and update the location
    // information.

    LiveRange* range = current->GetFirstRange();
    while (range != nullptr) {
      while (use != nullptr && use->GetPosition() < range->GetStart()) {
        DCHECK(use->IsSynthesized());
        use = use->GetNext();
      }
      while (use != nullptr && use->GetPosition() <= range->GetEnd()) {
        DCHECK(!use->GetIsEnvironment());
        DCHECK(current->CoversSlow(use->GetPosition()) || (use->GetPosition() == range->GetEnd()));
        if (!use->IsSynthesized()) {
          LocationSummary* locations = use->GetUser()->GetLocations();
          Location expected_location = locations->InAt(use->GetInputIndex());
          // The expected (actual) location may be invalid in case the input is unused. Currently
          // this only happens for intrinsics.
          if (expected_location.IsValid()) {
            if (expected_location.IsUnallocated()) {
              locations->SetInAt(use->GetInputIndex(), source);
            } else if (!expected_location.IsConstant()) {
              AddInputMoveFor(interval->GetDefinedBy(), use->GetUser(), source, expected_location);
            }
          } else {
            DCHECK(use->GetUser()->IsInvoke());
            DCHECK(use->GetUser()->AsInvoke()->GetIntrinsic() != Intrinsics::kNone);
          }
        }
        use = use->GetNext();
      }

      // Walk over the environment uses, and update their locations.
      while (env_use != nullptr && env_use->GetPosition() < range->GetStart()) {
        env_use = env_use->GetNext();
      }

      while (env_use != nullptr && env_use->GetPosition() <= range->GetEnd()) {
        DCHECK(current->CoversSlow(env_use->GetPosition())
               || (env_use->GetPosition() == range->GetEnd()));
        HEnvironment* environment = env_use->GetEnvironment();
        environment->SetLocationAt(env_use->GetInputIndex(), source);
        env_use = env_use->GetNext();
      }

      range = range->GetNext();
    }

    // If the next interval starts just after this one, and has a register,
    // insert a move.
    LiveInterval* next_sibling = current->GetNextSibling();
    if (next_sibling != nullptr
        && next_sibling->HasRegister()
        && current->GetEnd() == next_sibling->GetStart()) {
      Location destination = next_sibling->ToLocation();
      InsertParallelMoveAt(current->GetEnd(), interval->GetDefinedBy(), source, destination);
    }

    for (SafepointPosition* safepoint_position = current->GetFirstSafepoint();
         safepoint_position != nullptr;
         safepoint_position = safepoint_position->GetNext()) {
      DCHECK(current->CoversSlow(safepoint_position->GetPosition()));

      LocationSummary* locations = safepoint_position->GetLocations();
      if ((current->GetType() == Primitive::kPrimNot) && current->GetParent()->HasSpillSlot()) {
        DCHECK(interval->GetDefinedBy()->IsActualObject())
            << interval->GetDefinedBy()->DebugName()
            << "@" << safepoint_position->GetInstruction()->DebugName();
        locations->SetStackBit(current->GetParent()->GetSpillSlot() / kVRegSize);
      }

      switch (source.GetKind()) {
        case Location::kRegister: {
          locations->AddLiveRegister(source);
          // TODO
//          if (kIsDebugBuild && locations->OnlyCallsOnSlowPath()) {
//            DCHECK_LE(locations->GetNumberOfLiveRegisters(),
//                      maximum_number_of_live_core_registers_ +
//                      maximum_number_of_live_fp_registers_);
//          }
          if (current->GetType() == Primitive::kPrimNot) {
            DCHECK(interval->GetDefinedBy()->IsActualObject())
                << interval->GetDefinedBy()->DebugName()
                << "@" << safepoint_position->GetInstruction()->DebugName();
            locations->SetRegisterBit(source.reg());
          }
          break;
        }
        case Location::kFpuRegister: {
          locations->AddLiveRegister(source);
          break;
        }

        case Location::kRegisterPair:
        case Location::kFpuRegisterPair: {
          locations->AddLiveRegister(source.ToLow());
          locations->AddLiveRegister(source.ToHigh());
          break;
        }
        case Location::kStackSlot:  // Fall-through
        case Location::kDoubleStackSlot:  // Fall-through
        case Location::kConstant: {
          // Nothing to do.
          break;
        }
        default: {
          LOG(FATAL) << "Unexpected location for object";
        }
      }
    }
    current = next_sibling;
  } while (current != nullptr);

  if (kIsDebugBuild) {
    // Following uses can only be synthesized uses.
    while (use != nullptr) {
      DCHECK(use->IsSynthesized());
      use = use->GetNext();
    }
  }
}

// TODO: Factor out into ssa_deconstruction
static bool IsMaterializableEntryBlockInstructionOfGraphWithIrreducibleLoop(
    HInstruction* instruction) {
  return instruction->GetBlock()->GetGraph()->HasIrreducibleLoops() &&
         (instruction->IsConstant() || instruction->IsCurrentMethod());
}

// TODO: Factor out into ssa_deconstruction
void RegisterAllocatorGraphColor::ConnectSplitSiblings(LiveInterval* interval,
                                                       HBasicBlock* from,
                                                       HBasicBlock* to) const {
  if (interval->GetNextSibling() == nullptr) {
    // Nothing to connect. The whole range was allocated to the same location.
    return;
  }

  // Find the intervals that cover `from` and `to`.
  size_t destination_position = to->GetLifetimeStart();
  size_t source_position = from->GetLifetimeEnd() - 1;
  LiveInterval* destination = interval->GetSiblingAt(destination_position);
  LiveInterval* source = interval->GetSiblingAt(source_position);

  if (destination == source) {
    // Interval was not split.
    return;
  }

  LiveInterval* parent = interval->GetParent();
  HInstruction* defined_by = parent->GetDefinedBy();
  if (codegen_->GetGraph()->HasIrreducibleLoops() &&
      (destination == nullptr || !destination->CoversSlow(destination_position))) {
    // Our live_in fixed point calculation has found that the instruction is live
    // in the `to` block because it will eventually enter an irreducible loop. Our
    // live interval computation however does not compute a fixed point, and
    // therefore will not have a location for that instruction for `to`.
    // Because the instruction is a constant or the ArtMethod, we don't need to
    // do anything: it will be materialized in the irreducible loop.
    DCHECK(IsMaterializableEntryBlockInstructionOfGraphWithIrreducibleLoop(defined_by))
        << defined_by->DebugName() << ":" << defined_by->GetId()
        << " " << from->GetBlockId() << " -> " << to->GetBlockId();
    return;
  }

  if (!destination->HasRegister()) {
    // Values are eagerly spilled. Spill slot already contains appropriate value.
    return;
  }

  Location location_source;
  // `GetSiblingAt` returns the interval whose start and end cover `position`,
  // but does not check whether the interval is inactive at that position.
  // The only situation where the interval is inactive at that position is in the
  // presence of irreducible loops for constants and ArtMethod.
  if (codegen_->GetGraph()->HasIrreducibleLoops() &&
      (source == nullptr || !source->CoversSlow(source_position))) {
    DCHECK(IsMaterializableEntryBlockInstructionOfGraphWithIrreducibleLoop(defined_by));
    if (defined_by->IsConstant()) {
      location_source = defined_by->GetLocations()->Out();
    } else {
      DCHECK(defined_by->IsCurrentMethod());
      location_source = parent->NeedsTwoSpillSlots()
          ? Location::DoubleStackSlot(parent->GetSpillSlot())
          : Location::StackSlot(parent->GetSpillSlot());
    }
  } else {
    DCHECK(source != nullptr);
    DCHECK(source->CoversSlow(source_position));
    DCHECK(destination->CoversSlow(destination_position));
    location_source = source->ToLocation();
  }

  // If `from` has only one successor, we can put the moves at the exit of it. Otherwise
  // we need to put the moves at the entry of `to`.
  if (from->GetNormalSuccessors().size() == 1) {
    InsertParallelMoveAtExitOf(from,
                               defined_by,
                               location_source,
                               destination->ToLocation());
  } else {
    DCHECK_EQ(to->GetPredecessors().size(), 1u);
    InsertParallelMoveAtEntryOf(to,
                                defined_by,
                                location_source,
                                destination->ToLocation());
  }
}

// TODO: Factor out into ssa_deconstruction
static bool IsInstructionStart(size_t position) {
  return (position & 1) == 0;
}

// TODO: Factor out into ssa_deconstruction
static bool IsInstructionEnd(size_t position) {
  return (position & 1) == 1;
}

// TODO: Factor out into ssa_deconstruction
static bool IsValidDestination(Location destination) {
  return destination.IsRegister()
      || destination.IsRegisterPair()
      || destination.IsFpuRegister()
      || destination.IsFpuRegisterPair()
      || destination.IsStackSlot()
      || destination.IsDoubleStackSlot();
}

// TODO: Factor out into ssa_deconstruction
void RegisterAllocatorGraphColor::InsertParallelMoveAt(size_t position,
                                                       HInstruction* instruction,
                                                       Location source,
                                                       Location destination) const {
  DCHECK(IsValidDestination(destination)) << destination;
  if (source.Equals(destination)) return;

  HInstruction* at = liveness_.GetInstructionFromPosition(position / 2);
  HParallelMove* move;
  if (at == nullptr) {
    if (IsInstructionStart(position)) {
      // Block boundary, don't do anything the connection of split siblings will handle it.
      return;
    } else {
      // Move must happen before the first instruction of the block.
      at = liveness_.GetInstructionFromPosition((position + 1) / 2);
      // Note that parallel moves may have already been inserted, so we explicitly
      // ask for the first instruction of the block: `GetInstructionFromPosition` does
      // not contain the `HParallelMove` instructions.
      at = at->GetBlock()->GetFirstInstruction();

      if (at->GetLifetimePosition() < position) {
        // We may insert moves for split siblings and phi spills at the beginning of the block.
        // Since this is a different lifetime position, we need to go to the next instruction.
        DCHECK(at->IsParallelMove());
        at = at->GetNext();
      }

      if (at->GetLifetimePosition() != position) {
        DCHECK_GT(at->GetLifetimePosition(), position);
        move = new (allocator_) HParallelMove(allocator_);
        move->SetLifetimePosition(position);
        at->GetBlock()->InsertInstructionBefore(move, at);
      } else {
        DCHECK(at->IsParallelMove());
        move = at->AsParallelMove();
      }
    }
  } else if (IsInstructionEnd(position)) {
    // Move must happen after the instruction.
    DCHECK(!at->IsControlFlow());
    move = at->GetNext()->AsParallelMove();
    // This is a parallel move for connecting siblings in a same block. We need to
    // differentiate it with moves for connecting blocks, and input moves.
    if (move == nullptr || move->GetLifetimePosition() > position) {
      move = new (allocator_) HParallelMove(allocator_);
      move->SetLifetimePosition(position);
      at->GetBlock()->InsertInstructionBefore(move, at->GetNext());
    }
  } else {
    // Move must happen before the instruction.
    HInstruction* previous = at->GetPrevious();
    if (previous == nullptr
        || !previous->IsParallelMove()
        || previous->GetLifetimePosition() != position) {
      // If the previous is a parallel move, then its position must be lower
      // than the given `position`: it was added just after the non-parallel
      // move instruction that precedes `instruction`.
      DCHECK(previous == nullptr
             || !previous->IsParallelMove()
             || previous->GetLifetimePosition() < position);
      move = new (allocator_) HParallelMove(allocator_);
      move->SetLifetimePosition(position);
      at->GetBlock()->InsertInstructionBefore(move, at);
    } else {
      move = previous->AsParallelMove();
    }
  }
  DCHECK_EQ(move->GetLifetimePosition(), position);
  AddMove(move, source, destination, instruction, instruction->GetType());
}

// TODO: Factor out into ssa_deconstruction
void RegisterAllocatorGraphColor::InsertParallelMoveAtEntryOf(HBasicBlock* block,
                                                              HInstruction* instruction,
                                                              Location source,
                                                              Location destination) const {
  DCHECK(IsValidDestination(destination)) << destination;
  if (source.Equals(destination)) return;

  HInstruction* first = block->GetFirstInstruction();
  HParallelMove* move = first->AsParallelMove();
  size_t position = block->GetLifetimeStart();
  // This is a parallel move for connecting blocks. We need to differentiate
  // it with moves for connecting siblings in a same block, and input moves.
  if (move == nullptr || move->GetLifetimePosition() != position) {
    move = new (allocator_) HParallelMove(allocator_);
    move->SetLifetimePosition(position);
    block->InsertInstructionBefore(move, first);
  }
  AddMove(move, source, destination, instruction, instruction->GetType());
}

// TODO: Factor out into ssa_deconstruction
void RegisterAllocatorGraphColor::InsertParallelMoveAtExitOf(HBasicBlock* block,
                                                             HInstruction* instruction,
                                                             Location source,
                                                             Location destination) const {
  DCHECK(IsValidDestination(destination)) << destination
                                          << " " << instruction->GetKind();
  if (source.Equals(destination)) return;

  DCHECK_EQ(block->GetNormalSuccessors().size(), 1u);
  HInstruction* last = block->GetLastInstruction();
  // We insert moves at exit for phi predecessors and connecting blocks.
  // A block ending with an if or a packed switch cannot branch to a block
  // with phis because we do not allow critical edges. It can also not connect
  // a split interval between two blocks: the move has to happen in the successor.
  DCHECK(!last->IsIf() && !last->IsPackedSwitch());
  HInstruction* previous = last->GetPrevious();
  HParallelMove* move;
  // This is a parallel move for connecting blocks. We need to differentiate
  // it with moves for connecting siblings in a same block, and output moves.
  size_t position = last->GetLifetimePosition();
  if (previous == nullptr || !previous->IsParallelMove()
      || previous->AsParallelMove()->GetLifetimePosition() != position) {
    move = new (allocator_) HParallelMove(allocator_);
    move->SetLifetimePosition(position);
    block->InsertInstructionBefore(move, last);
  } else {
    move = previous->AsParallelMove();
  }
  AddMove(move, source, destination, instruction, instruction->GetType());
}

// TODO: Factor out into ssa_deconstruction
void RegisterAllocatorGraphColor::AddMove(HParallelMove* move,
                                          Location source,
                                          Location destination,
                                          HInstruction* instruction,
                                          Primitive::Type type) const {
  if (type == Primitive::kPrimLong
      && codegen_->ShouldSplitLongMoves()
      // The parallel move resolver knows how to deal with long constants.
      && !source.IsConstant()) {
    move->AddMove(source.ToLow(), destination.ToLow(), Primitive::kPrimInt, instruction);
    move->AddMove(source.ToHigh(), destination.ToHigh(), Primitive::kPrimInt, nullptr);
  } else {
    move->AddMove(source, destination, type, instruction);
  }
}

// TODO: Factor out into ssa_deconstruction
void RegisterAllocatorGraphColor::AddInputMoveFor(HInstruction* input,
                                                  HInstruction* user,
                                                  Location source,
                                                  Location destination) const {
  if (source.Equals(destination)) return;

  DCHECK(!user->IsPhi());

  HInstruction* previous = user->GetPrevious();
  HParallelMove* move = nullptr;
  if (previous == nullptr
      || !previous->IsParallelMove()
      || previous->GetLifetimePosition() < user->GetLifetimePosition()) {
    move = new (allocator_) HParallelMove(allocator_);
    move->SetLifetimePosition(user->GetLifetimePosition());
    user->GetBlock()->InsertInstructionBefore(move, user);
  } else {
    move = previous->AsParallelMove();
  }
  DCHECK_EQ(move->GetLifetimePosition(), user->GetLifetimePosition());
  AddMove(move, source, destination, nullptr, input->GetType());
}

// TODO: Factor out into ssa_deconstruction
void RegisterAllocatorGraphColor::InsertMoveAfter(HInstruction* instruction,
                                                  Location source,
                                                  Location destination) const {
  DCHECK(IsValidDestination(destination)) << destination;
  if (source.Equals(destination)) return;

  if (instruction->IsPhi()) {
    InsertParallelMoveAtEntryOf(instruction->GetBlock(), instruction, source, destination);
    return;
  }

  size_t position = instruction->GetLifetimePosition() + 1;
  HParallelMove* move = instruction->GetNext()->AsParallelMove();
  // This is a parallel move for moving the output of an instruction. We need
  // to differentiate with input moves, moves for connecting siblings in a
  // and moves for connecting blocks.
  if (move == nullptr || move->GetLifetimePosition() != position) {
    move = new (allocator_) HParallelMove(allocator_);
    move->SetLifetimePosition(position);
    instruction->GetBlock()->InsertInstructionBefore(move, instruction->GetNext());
  }
  AddMove(move, source, destination, instruction, instruction->GetType());
}

}  // namespace art
