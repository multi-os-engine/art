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

#include "register_allocator.h"

#include <iostream>
#include <sstream>

#include "base/bit_vector-inl.h"
#include "code_generator.h"
#include "register_allocator_graph_color.h"
#include "register_allocator_linear_scan.h"
#include "ssa_liveness_analysis.h"


namespace art {

RegisterAllocator::RegisterAllocator(ArenaAllocator* allocator,
                                     CodeGenerator* codegen,
                                     const SsaLivenessAnalysis& liveness)
    : allocator_(allocator),
      codegen_(codegen),
      liveness_(liveness) {}

RegisterAllocator* RegisterAllocator::Create(ArenaAllocator* allocator,
                                             CodeGenerator* codegen,
                                             const SsaLivenessAnalysis& analysis,
                                             Strategy strategy) {
  if (strategy == kRegisterAllocatorDefault) {
    strategy = kRegisterAllocatorLinearScan;
  }
  switch (strategy) {
    case kRegisterAllocatorLinearScan:
      return new (allocator) RegisterAllocatorLinearScan(allocator, codegen, analysis);
    case kRegisterAllocatorGraphColor:
      return new (allocator) RegisterAllocatorGraphColor(allocator, codegen, analysis);
    default:
      LOG(FATAL) << "Invalid register allocation strategy: " << strategy;
      UNREACHABLE();
  }
}

bool RegisterAllocator::CanAllocateRegistersFor(const HGraph& graph ATTRIBUTE_UNUSED,
                                                InstructionSet instruction_set) {
  return instruction_set == kArm
      || instruction_set == kArm64
      || instruction_set == kMips
      || instruction_set == kMips64
      || instruction_set == kThumb2
      || instruction_set == kX86
      || instruction_set == kX86_64;
}

class AllRangesIterator : public ValueObject {
 public:
  explicit AllRangesIterator(LiveInterval* interval)
      : current_interval_(interval),
        current_range_(interval->GetFirstRange()) {}

  bool Done() const { return current_interval_ == nullptr; }
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

bool RegisterAllocator::ValidateIntervals(const ArenaVector<LiveInterval*>& intervals,
                                          size_t number_of_spill_slots,
                                          size_t number_of_out_slots,
                                          const CodeGenerator& codegen,
                                          ArenaAllocator* allocator,
                                          bool processing_core_registers,
                                          bool log_fatal_on_failure) {
  size_t number_of_registers = processing_core_registers
      ? codegen.GetNumberOfCoreRegisters()
      : codegen.GetNumberOfFloatingPointRegisters();
  ArenaVector<ArenaBitVector*> liveness_of_values(
      allocator->Adapter(kArenaAllocRegisterAllocatorValidate));
  liveness_of_values.reserve(number_of_registers + number_of_spill_slots);

  size_t max_end = 0u;
  for (LiveInterval* start_interval : intervals) {
    for (AllRangesIterator it(start_interval); !it.Done(); it.Advance()) {
      max_end = std::max(max_end, it.CurrentRange()->GetEnd());
    }
  }

  // Allocate a bit vector per register. A live interval that has a register
  // allocated will populate the associated bit vector based on its live ranges.
  for (size_t i = 0; i < number_of_registers + number_of_spill_slots; ++i) {
    liveness_of_values.push_back(
        ArenaBitVector::Create(allocator, max_end, false, kArenaAllocRegisterAllocatorValidate));
  }

  for (LiveInterval* start_interval : intervals) {
    for (AllRangesIterator it(start_interval); !it.Done(); it.Advance()) {
      LiveInterval* current = it.CurrentInterval();
      HInstruction* defined_by = current->GetParent()->GetDefinedBy();
      if (current->GetParent()->HasSpillSlot()
           // Parameters and current method have their own stack slot.
           && !(defined_by != nullptr && (defined_by->IsParameterValue()
                                          || defined_by->IsCurrentMethod()))) {
        BitVector* liveness_of_spill_slot = liveness_of_values[number_of_registers
            + current->GetParent()->GetSpillSlot() / kVRegSize
            - number_of_out_slots];
        for (size_t j = it.CurrentRange()->GetStart(); j < it.CurrentRange()->GetEnd(); ++j) {
          if (liveness_of_spill_slot->IsBitSet(j)) {
            if (log_fatal_on_failure) {
              std::ostringstream message;
              message << "Spill slot conflict at " << j;
              LOG(FATAL) << message.str();
            } else {
              return false;
            }
          } else {
            liveness_of_spill_slot->SetBit(j);
          }
        }
      }

      if (current->HasRegister()) {
        if (kIsDebugBuild && log_fatal_on_failure && !current->IsFixed()) {
          // Only check when an error is fatal. Only tests code ask for non-fatal failures
          // and test code may not properly fill the right information to the code generator.
          CHECK(codegen.HasAllocatedRegister(processing_core_registers, current->GetRegister()));
        }
        BitVector* liveness_of_register = liveness_of_values[current->GetRegister()];
        for (size_t j = it.CurrentRange()->GetStart(); j < it.CurrentRange()->GetEnd(); ++j) {
          if (liveness_of_register->IsBitSet(j)) {
            if (current->IsUsingInputRegister() && current->CanUseInputRegister()) {
              continue;
            }
            if (log_fatal_on_failure) {
              std::ostringstream message;
              message << "Register conflict at " << j << " ";
              if (defined_by != nullptr) {
                message << "(" << defined_by->DebugName() << ")";
              }
              message << "for ";
              if (processing_core_registers) {
                codegen.DumpCoreRegister(message, current->GetRegister());
              } else {
                codegen.DumpFloatingPointRegister(message, current->GetRegister());
              }
              LOG(FATAL) << message.str();
            } else {
              return false;
            }
          } else {
            liveness_of_register->SetBit(j);
          }
        }
      }
    }
  }
  return true;
}

LiveInterval* RegisterAllocator::Split(LiveInterval* interval, size_t position) {
  DCHECK_GE(position, interval->GetStart());
  DCHECK(!interval->IsDeadAt(position));
  if (position == interval->GetStart()) {
    // Spill slot will be allocated when handling `interval` again.
    interval->ClearRegister();
    if (interval->HasHighInterval()) {
      interval->GetHighInterval()->ClearRegister();
    } else if (interval->HasLowInterval()) {
      interval->GetLowInterval()->ClearRegister();
    }
    return interval;
  } else {
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
}

LiveInterval* RegisterAllocator::SplitBetween(LiveInterval* interval, size_t from, size_t to) {
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

static bool IsValidDestination(Location destination) {
  return destination.IsRegister()
      || destination.IsRegisterPair()
      || destination.IsFpuRegister()
      || destination.IsFpuRegisterPair()
      || destination.IsStackSlot()
      || destination.IsDoubleStackSlot();
}

void RegisterAllocator::AddMove(HParallelMove* move,
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

void RegisterAllocator::AddInputMoveFor(HInstruction* input,
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

static bool IsInstructionStart(size_t position) {
  return (position & 1) == 0;
}

static bool IsInstructionEnd(size_t position) {
  return (position & 1) == 1;
}

void RegisterAllocator::InsertParallelMoveAt(size_t position,
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

void RegisterAllocator::InsertParallelMoveAtExitOf(HBasicBlock* block,
                                                   HInstruction* instruction,
                                                   Location source,
                                                   Location destination) const {
  DCHECK(IsValidDestination(destination)) << destination;
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

void RegisterAllocator::InsertParallelMoveAtEntryOf(HBasicBlock* block,
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

void RegisterAllocator::InsertMoveAfter(HInstruction* instruction,
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

void RegisterAllocator::ConnectSiblings(LiveInterval* interval,
                                        size_t max_safepoint_live_regs) {
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
          if (kIsDebugBuild && locations->OnlyCallsOnSlowPath()) {
            DCHECK_LE(locations->GetNumberOfLiveRegisters(),
                      max_safepoint_live_regs);
          }
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

static bool IsMaterializableEntryBlockInstructionOfGraphWithIrreducibleLoop(
    HInstruction* instruction) {
  return instruction->GetBlock()->GetGraph()->HasIrreducibleLoops() &&
         (instruction->IsConstant() || instruction->IsCurrentMethod());
}

void RegisterAllocator::ConnectSplitSiblings(LiveInterval* interval,
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

void RegisterAllocator::ResolveNonlinearControlFlow() {
  // Order does not matter.
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
}

void RegisterAllocator::ResolvePhiInputs() {
  // Order does not matter.
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
}

void RegisterAllocator::ResolveTempLocations(ArenaVector<LiveInterval*>& temp_intervals) {
  for (LiveInterval* temp : temp_intervals) {
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
