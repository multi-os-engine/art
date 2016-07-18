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

#include "ssa_deconstruction.h"

#include "code_generator.h"
#include "ssa_liveness_analysis.h"

namespace art {

SsaDeconstruction::SsaDeconstruction(ArenaAllocator* allocator,
                                     CodeGenerator* codegen,
                                     const SsaLivenessAnalysis& liveness,
                                     size_t max_safepoint_live_registers)
      : allocator_(allocator),
        codegen_(codegen),
        liveness_(liveness),
        max_safepoint_live_registers_(max_safepoint_live_registers) {}

void SsaDeconstruction::DeconstructSsa() {
  // Connect siblings.
  for (size_t i = 0, e = liveness_.GetNumberOfSsaValues(); i < e; ++i) {
    HInstruction* instruction = liveness_.GetInstructionFromSsaIndex(i);
    ConnectSiblings(instruction->GetLiveInterval(), max_safepoint_live_registers_);
  }

  ResolveNonlinearControlFlow();
  ResolvePhiInputs();
}

void SsaDeconstruction::ResolveNonlinearControlFlow() {
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

void SsaDeconstruction::ResolvePhiInputs() {
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

void SsaDeconstruction::ConnectSiblings(LiveInterval* interval,
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

void SsaDeconstruction::ConnectSplitSiblings(LiveInterval* interval,
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

static bool IsValidDestination(Location destination) {
  return destination.IsRegister()
      || destination.IsRegisterPair()
      || destination.IsFpuRegister()
      || destination.IsFpuRegisterPair()
      || destination.IsStackSlot()
      || destination.IsDoubleStackSlot();
}

void SsaDeconstruction::AddMove(HParallelMove* move,
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

void SsaDeconstruction::AddInputMoveFor(HInstruction* input,
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

void SsaDeconstruction::InsertParallelMoveAt(size_t position,
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

void SsaDeconstruction::InsertParallelMoveAtExitOf(HBasicBlock* block,
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

void SsaDeconstruction::InsertParallelMoveAtEntryOf(HBasicBlock* block,
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

void SsaDeconstruction::InsertMoveAfter(HInstruction* instruction,
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
