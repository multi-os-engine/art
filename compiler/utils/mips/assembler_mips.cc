/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "assembler_mips.h"

#include "base/bit_utils.h"
#include "base/casts.h"
#include "entrypoints/quick/quick_entrypoints.h"
#include "memory_region.h"
#include "thread.h"

// Instruction encodings shared by some routines.
namespace {
using namespace art::mips;

static int32_t EncodingR(int opcode, Register rs, Register rt, Register rd, int shamt, int funct) {
  CHECK_NE(rs, kNoRegister);
  CHECK_NE(rt, kNoRegister);
  CHECK_NE(rd, kNoRegister);
  int32_t encoding = opcode << kOpcodeShift |
                     static_cast<int32_t>(rs) << kRsShift |
                     static_cast<int32_t>(rt) << kRtShift |
                     static_cast<int32_t>(rd) << kRdShift |
                     shamt << kShamtShift |
                     funct;
  return encoding;
}

static int32_t EncodingI(int opcode, Register rs, Register rt, uint16_t imm) {
  CHECK_NE(rs, kNoRegister);
  CHECK_NE(rt, kNoRegister);
  int32_t encoding = opcode << kOpcodeShift |
                     static_cast<int32_t>(rs) << kRsShift |
                     static_cast<int32_t>(rt) << kRtShift |
                     imm;
  return encoding;
}

static int32_t EncodingAddu(Register rd, Register rs, Register rt) {
  return EncodingR(0, rs, rt, rd, 0, 0x21);
}

static int32_t EncodingOri(Register rt, Register rs, uint16_t imm16) {
  return EncodingI(0xd, rs, rt, imm16);
}

static int32_t EncodingLui(Register rt, uint16_t imm16) {
  return EncodingI(0xf, static_cast<Register>(0), rt, imm16);
}

static int32_t EncodingB(uint16_t offset) {
  return EncodingI(0x4, static_cast<Register>(0), static_cast<Register>(0), offset);
}

static int32_t EncodingBal(uint16_t offset) {
  return EncodingI(0x1, static_cast<Register>(0), static_cast<Register>(0x11), offset);
}

static int32_t EncodingBeq(Register rt, Register rs, uint16_t offset) {
  return EncodingI(0x4, rs, rt, offset);
}

static int32_t EncodingBne(Register rt, Register rs, uint16_t offset) {
  return EncodingI(0x5, rs, rt, offset);
}

static int32_t EncodingBltz(Register rs, uint16_t offset) {
  return EncodingI(0x1, rs, static_cast<Register>(0), offset);
}

static int32_t EncodingBlez(Register rs, uint16_t offset) {
  return EncodingI(0x6, rs, static_cast<Register>(0), offset);
}

static int32_t EncodingBgtz(Register rs, uint16_t offset) {
  return EncodingI(0x7, rs, static_cast<Register>(0), offset);
}

static int32_t EncodingBgez(Register rs, uint16_t offset) {
  return EncodingI(0x1, rs, static_cast<Register>(1), offset);
}

static int32_t EncodingJalr(Register rd, Register rs) {
  return EncodingR(0, rs, static_cast<Register>(0), rd, 0, 0x09);
}

}  // namespace


namespace art {
namespace mips {

std::ostream& operator<<(std::ostream& os, const DRegister& rhs) {
  if (rhs >= D0 && rhs < kNumberOfDRegisters) {
    os << "d" << static_cast<int>(rhs);
  } else {
    os << "DRegister[" << static_cast<int>(rhs) << "]";
  }
  return os;
}

void MipsAssembler::Emit(int32_t value) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  buffer_.Emit<int32_t>(value);
}

void MipsAssembler::EmitR(int opcode, Register rs, Register rt, Register rd, int shamt, int funct) {
  Emit(EncodingR(opcode, rs, rt, rd, shamt, funct));
}

void MipsAssembler::EmitI(int opcode, Register rs, Register rt, uint16_t imm) {
  Emit(EncodingI(opcode, rs, rt, imm));
}

void MipsAssembler::EmitJ(int opcode, int address) {
  int32_t encoding = opcode << kOpcodeShift |
                     address;
  Emit(encoding);
}

void MipsAssembler::EmitFR(int opcode, int fmt, FRegister ft, FRegister fs, FRegister fd, int funct) {
  CHECK_NE(ft, kNoFRegister);
  CHECK_NE(fs, kNoFRegister);
  CHECK_NE(fd, kNoFRegister);
  int32_t encoding = opcode << kOpcodeShift |
                     fmt << kFmtShift |
                     static_cast<int32_t>(ft) << kFtShift |
                     static_cast<int32_t>(fs) << kFsShift |
                     static_cast<int32_t>(fd) << kFdShift |
                     funct;
  Emit(encoding);
}

void MipsAssembler::EmitFI(int opcode, int fmt, FRegister rt, uint16_t imm) {
  CHECK_NE(rt, kNoFRegister);
  int32_t encoding = opcode << kOpcodeShift |
                     fmt << kFmtShift |
                     static_cast<int32_t>(rt) << kRtShift |
                     imm;
  Emit(encoding);
}

void MipsAssembler::EmitUnconditionalBranchFixup(Label* label) {
  uint32_t pc = buffer_.Size();
  EmitBranchFixupHelper(Fixup::UnconditionalBranch(pc), label);
}

void MipsAssembler::EmitConditionalBranchFixup(Register rt, Register rs, Label* label, Condition cond) {
  uint32_t pc = buffer_.Size();
  EmitBranchFixupHelper(Fixup::ConditionalBranch(pc, rt, rs, cond), label);
}

void MipsAssembler::EmitConditionalBranchCompareToZeroFixup(Register rs, Label* label, Condition cond) {
 uint32_t pc = buffer_.Size();
  EmitBranchFixupHelper(Fixup::ConditionalBranchCompareToZero(pc, rs, cond), label);
}

void MipsAssembler::EmitBranchFixupHelper(Fixup fixup, Label* label) {
  uint32_t pc = buffer_.Size();
  FixupId branch_id = AddFixup(fixup);
  if (label->IsBound()) {
    // The branch is to a bound label which means that it's a
    // backwards branch.
    GetFixup(branch_id)->Resolve(label->Position());
    Emit(0);
  } else {
    // Branch target is an unbound label. Add it to a singly-linked
    // list maintained within the code with the label serving as the
    // head.
    Emit(static_cast<int32_t>(label->position_));
    label->LinkTo(branch_id);
  }
  CHECK_EQ(buffer_.Size() - pc, GetFixup(branch_id)->GetSizeInBytes());
  Nop();
}

int32_t MipsAssembler::EncodeBranchOffset(int offset, int32_t inst, bool is_jump) {
  CHECK_ALIGNED(offset, 4);
  // Properly preserve only the bits supported in the instruction.
  offset >>= 2;
  if (is_jump) {
    CHECK(IsInt(POPCOUNT(kBranchOffsetMask), offset)) << offset;
    offset &= kJumpOffsetMask;
    return (inst & ~kJumpOffsetMask) | offset;
  } else {
    CHECK(IsInt(POPCOUNT(kJumpOffsetMask), offset)) << offset;
    offset &= kBranchOffsetMask;
    return (inst & ~kBranchOffsetMask) | offset;
  }
}

int MipsAssembler::DecodeBranchOffset(int32_t inst, bool is_jump) {
  // Sign-extend, then left-shift by 2.
  if (is_jump) {
    return (((inst & kJumpOffsetMask) << 6) >> 4);
  } else {
    return (((inst & kBranchOffsetMask) << 16) >> 14);
  }
}

void MipsAssembler::Bind(Label* label, bool is_jump) {
  CHECK(!label->IsBound());
  int bound_pc = buffer_.Size();
  if (is_jump) {
    while (label->IsLinked()) {
      int32_t position = label->Position();
      int32_t next = buffer_.Load<int32_t>(position);
      int32_t offset = bound_pc - position;
      int32_t encoded = MipsAssembler::EncodeBranchOffset(offset, next, /* is_jump */ true);
      buffer_.Store<int32_t>(position, encoded);
      label->position_ = MipsAssembler::DecodeBranchOffset(next, /* is_jump */ true);
    }
    label->BindTo(bound_pc);
  } else {
    while (label->IsLinked()) {
      FixupId fixup_id = label->Position();                     // The id for linked Fixup.
      Fixup* fixup = GetFixup(fixup_id);                        // Get the Fixup at this id.
      fixup->Resolve(bound_pc);                                 // Fixup can be resolved now.
      uint32_t fixup_location = fixup->GetLocation();
      uint32_t next = buffer_.Load<int32_t>(fixup_location);    // Get next in chain.
      buffer_.Store<int32_t>(fixup_location, 0);
      label->position_ = next;                                  // Move to next.
    }
    label->BindTo(bound_pc);
  }
}

void MipsAssembler::AdjustLabelPosition(Label* label) {
  CHECK(label->IsBound());
  uint32_t old_position = static_cast<uint32_t>(label->Position());
  uint32_t new_position = GetAdjustedPosition(old_position);
  label->Reinitialize();
  CHECK_GE(static_cast<int>(new_position), 0);
  label->BindTo(static_cast<int>(new_position));
}

uint32_t MipsAssembler::GetAdjustedPosition(uint32_t old_position) {
  // We can reconstruct the adjustment by going through all the fixups from the beginning
  // up to the old_position. Since we expect AdjustedPosition() to be called in a loop
  // with increasing old_position, we can use the data from last AdjustedPosition() to
  // continue where we left off and the whole loop should be O(m+n) where m is the number
  // of positions to adjust and n is the number of fixups.
  if (old_position < last_old_position_) {
    last_position_adjustment_ = 0u;
    last_old_position_ = 0u;
    last_fixup_id_ = 0u;
  }
  while (last_fixup_id_ != fixups_.size()) {
    Fixup* fixup = GetFixup(last_fixup_id_);
    if (fixup->GetLocation() >= old_position + last_position_adjustment_) {
      break;
    }
    if (fixup->GetSize() != fixup->GetOriginalSize()) {
      last_position_adjustment_ += fixup->GetSizeInBytes() - fixup->GetOriginalSizeInBytes();
    }
     ++last_fixup_id_;
  }
  last_old_position_ = old_position;
  return old_position + last_position_adjustment_;
}

inline size_t MipsAssembler::Fixup::SizeInBytes(Size size) {
  switch (size) {
    case Size::kShortUnconditionalBranch:
      // A single 32-bit instructions, e.g.:
      //
      //       b    target
      return 1u * kInstructionSize;

    case Size::kShortConditionalBranch:
      // A single 32-bit instructions, e.g.:
      //
      //       beq  rs, rt, target
      return 1u * kInstructionSize;

    case Size::kShortConditionalBranchCompareToZero:
      // A single 32-bit instructions, e.g.:
      //
      //       bltz rs, target
      return 1u * kInstructionSize;

    case Size::kLargeUnconditionalBranch:
      // Five 32-bits instructions, e.g.:
      //       bal  .+8   ; RA <- anchor
      //       lui  AT, High16Bits(target-anchor)
      //   anchor:
      //       ori  AT, AT, Low16Bits(target-anchor)
      //       addu AT, AT, RA
      //       jalr ZERO, AT
      return 5u * kInstructionSize;

    case Size::kLargeConditionalBranch:
      // Six 32-bits instructions, e.g.:
      //       bne  rs, rt, hop
      //       bal  .+8   ; RA <- anchor
      //       lui  AT, High16Bits(target-anchor)
      //   anchor:
      //       ori  AT, AT, Low16Bits(target-anchor)
      //       addu AT, AT, RA
      //       jalr ZERO, AT
      //   hop:
      return 6u * kInstructionSize;

    case Size::kLargeConditionalBranchCompareToZero:
      // Six 32-bits instructions, e.g.:
      //       bgez  rs, hop
      //       bal  .+8   ; RA <- anchor
      //       lui  AT, High16Bits(target-anchor)
      //   anchor:
      //       ori  AT, AT, Low16Bits(target-anchor)
      //       addu AT, AT, RA
      //       jalr ZERO, AT
      //   hop:
      return 6u * kInstructionSize;
  }
  LOG(FATAL) << "Unexpected size: " << static_cast<int>(size);
  UNREACHABLE();
}

void MipsAssembler::Fixup::PrepareDependents(MipsAssembler* assembler) {
  // For each Fixup, it's easy to find the Fixups that it depends on
  // as they are either the following or the preceding Fixups until we
  // find the target. However, for fixup adjustment we need the
  // reverse lookup, i.e. what Fixups depend on a given Fixup.  This
  // function creates a compact representation of this relationship,
  // where we have all the dependents in a single array and Fixups
  // reference their ranges by start index and count.  (Instead of
  // having a per-fixup vector.)

  // Count the number of dependents of each Fixup.
  const FixupId end_id = assembler->fixups_.size();
  Fixup* fixups = assembler->fixups_.data();
  for (FixupId fixup_id = 0u; fixup_id != end_id; ++fixup_id) {
    uint32_t target = fixups[fixup_id].target_;
    if (target > fixups[fixup_id].location_) {
      for (FixupId id = fixup_id + 1u; (id != end_id) && (fixups[id].location_ < target); ++id) {
        fixups[id].dependents_count_ += 1u;
      }
    } else {
      for (FixupId id = fixup_id; (id != 0u) && (fixups[id - 1u].location_ >= target); --id) {
        fixups[id - 1u].dependents_count_ += 1u;
      }
    }
  }
  // Assign index ranges in fixup_dependents_ to individual fixups.
  // Record the end of the range in dependents_start_, we shall later
  // decrement it as we fill in fixup_dependents_.
  uint32_t number_of_dependents = 0u;
  for (FixupId fixup_id = 0u; fixup_id != end_id; ++fixup_id) {
    number_of_dependents += fixups[fixup_id].dependents_count_;
    fixups[fixup_id].dependents_start_ = number_of_dependents;
  }
  if (number_of_dependents == 0u) {
    return;
  }
  // Create and fill in the fixup_dependents_.
  assembler->fixup_dependents_.reset(new FixupId[number_of_dependents]);
  FixupId* dependents = assembler->fixup_dependents_.get();
  for (FixupId fixup_id = 0u; fixup_id != end_id; ++fixup_id) {
    uint32_t target = fixups[fixup_id].target_;
    if (target > fixups[fixup_id].location_) {
      for (FixupId id = fixup_id + 1u; (id != end_id) && (fixups[id].location_ < target); ++id) {
        fixups[id].dependents_start_ -= 1u;
        dependents[fixups[id].dependents_start_] = fixup_id;
      }
    } else {
      for (FixupId id = fixup_id; (id != 0u) && (fixups[id - 1u].location_) >= target; --id) {
        fixups[id - 1u].dependents_start_ -= 1u;
        dependents[fixups[id - 1u].dependents_start_] = fixup_id;
      }
    }
  }
}

inline int32_t MipsAssembler::Fixup::GetOffset() const {
  static constexpr int32_t int32_min = std::numeric_limits<int32_t>::min();
  static constexpr int32_t int32_max = std::numeric_limits<int32_t>::max();
  CHECK_LE(target_, static_cast<uint32_t>(int32_max));
  CHECK_LE(location_, static_cast<uint32_t>(int32_max));
  CHECK_LE(adjustment_, static_cast<uint32_t>(int32_max));
  int32_t diff = static_cast<int32_t>(target_) - static_cast<int32_t>(location_);
  if (target_ > location_) {
    CHECK_LE(adjustment_, static_cast<uint32_t>(int32_max - diff));
    diff += static_cast<int32_t>(adjustment_);
  } else {
    CHECK_LE(int32_min + static_cast<int32_t>(adjustment_), diff);
    diff -= static_cast<int32_t>(adjustment_);
  }
  // The default PC adjustment for MIPS is 4 bytes.
  CHECK_GE(diff, int32_min + 4);
  diff -= 4;
  // Add additional adjustment for instructions preceding the PC usage.
  switch (size_) {
    case Size::kShortUnconditionalBranch:
    case Size::kShortConditionalBranch:
    case Size::kShortConditionalBranchCompareToZero:
      break;

    case Size::kLargeUnconditionalBranch:
      // bal, lui, ori, addu (4 instructions) preceding jalr.
      diff -= (4 * kInstructionSize);
      break;

    case Size::kLargeConditionalBranch:
      // b<RevCond>, bal, lui, ori, addu (5 instructions) preceding jalr.
      diff -= (5 * kInstructionSize);
      break;

    case Size::kLargeConditionalBranchCompareToZero:
      // b<RevCond>z, bal, lui, ori, addu (5 instructions) preceding jalr.
      diff -= (5 * kInstructionSize);
      break;

    default:
      LOG(FATAL) << "Unexpected size " << size_;
  }
  return diff;
}

inline size_t MipsAssembler::Fixup::IncreaseSize(Size new_size) {
  CHECK_NE(target_, kUnresolved);
  Size old_size = size_;
  size_ = new_size;
  CHECK_GT(SizeInBytes(new_size), SizeInBytes(old_size));
  size_t adjustment = SizeInBytes(new_size) - SizeInBytes(old_size);
  if (target_ > location_) {
    adjustment_ += adjustment;
  }
  return adjustment;
}

uint32_t MipsAssembler::Fixup::AdjustSizeIfNeeded(uint32_t current_code_size) {
  uint32_t old_code_size = current_code_size;
  switch (size_) {
    case Size::kShortUnconditionalBranch:
      // This encoding can handle 18-bit branch offsets.
      if (IsInt(POPCOUNT(kBranchOffsetMask), (GetOffset() >> 2) & kBranchOffsetMask)) {
        break;
      }
      current_code_size += IncreaseSize(Size::kLargeUnconditionalBranch);
      FALLTHROUGH_INTENDED;
    case Size::kLargeUnconditionalBranch:
      // This encoding can handle any (32-bit) branch offset.
      break;

    case Size::kShortConditionalBranch:
      // This encoding can handle 18-bit branch offsets.
      if (IsInt(POPCOUNT(kBranchOffsetMask), (GetOffset() >> 2) & kBranchOffsetMask)) {
        break;
      }
      current_code_size += IncreaseSize(Size::kLargeConditionalBranch);
      FALLTHROUGH_INTENDED;
    case Size::kLargeConditionalBranch:
      // This encoding can handle any (32-bit) branch offset.
      break;

    case Size::kShortConditionalBranchCompareToZero:
      // This encoding can handle 18-bit branch offsets.
      if (IsInt(POPCOUNT(kBranchOffsetMask), (GetOffset() >> 2) & kBranchOffsetMask)) {
        break;
      }
      current_code_size += IncreaseSize(Size::kShortConditionalBranchCompareToZero);
      FALLTHROUGH_INTENDED;
    case Size::kLargeConditionalBranchCompareToZero:
      // This encoding can handle any (32-bit) branch offset.
      break;

    default:
      LOG(FATAL) << "Unexpected size " << size_;
  }
  return current_code_size - old_code_size;
}

void MipsAssembler::Fixup::Emit(AssemblerBuffer* buffer) const {
  int32_t offset = GetOffset();
  switch (size_) {
    case Size::kShortUnconditionalBranch: {
      CHECK_EQ(type_, Type::kUnconditionalBranch);
      CHECK_EQ(cond_, Condition::kNoCondition);
      CHECK_EQ(rt_, kNoRegister);
      CHECK_EQ(rs_, kNoRegister);
      // b offset
      uint16_t encoded_offset = (offset >> 2) & kBranchOffsetMask;
      buffer->Store<int32_t>(location_, EncodingB(encoded_offset));
      break;
    }

    case Size::kShortConditionalBranch: {
      CHECK_EQ(type_, Type::kConditionalBranch);
      CHECK_NE(rt_, kNoRegister);
      CHECK_NE(rs_, kNoRegister);
      uint16_t encoded_offset = (offset >> 2) & kBranchOffsetMask;
      switch (cond_) {
        case Condition::kEq:
          // beq rs, rt, offset
          buffer->Store<int32_t>(location_, EncodingBeq(rt_, rs_, encoded_offset));
          break;
        case Condition::kNe:
          // bne rs, rt, offset
          buffer->Store<int32_t>(location_, EncodingBne(rt_, rs_, encoded_offset));
          break;
        default:
          LOG(FATAL) << "Unexpected condition " << cond_;
      }
      break;
    }

    case Size::kShortConditionalBranchCompareToZero: {
      CHECK_EQ(type_, Type::kConditionalBranchCompareToZero);
      CHECK_EQ(rt_, kNoRegister);
      CHECK_NE(rs_, kNoRegister);
      uint16_t encoded_offset = (offset >> 2) & kBranchOffsetMask;
      switch (cond_) {
        case Condition::kLtz:
          // bltz rs, offset
          buffer->Store<int32_t>(location_, EncodingBltz(rs_, encoded_offset));
          break;
        case Condition::kLez:
          // blez rs, offset
          buffer->Store<int32_t>(location_, EncodingBlez(rs_, encoded_offset));
          break;
        case Condition::kGtz:
          // bgtz rs, offset
          buffer->Store<int32_t>(location_, EncodingBgtz(rs_, encoded_offset));
          break;
        case Condition::kGez:
          // bgez rs, offset
          buffer->Store<int32_t>(location_, EncodingBgez(rs_, encoded_offset));
          break;
        default:
          LOG(FATAL) << "Unexpected condition " << cond_;
      }
      break;
    }

    case Size::kLargeUnconditionalBranch: {
      CHECK_EQ(type_, Type::kUnconditionalBranch);
      CHECK_EQ(cond_, Condition::kNoCondition);
      CHECK_EQ(rt_, kNoRegister);
      CHECK_EQ(rs_, kNoRegister);
      uint16_t offset_low = Low16Bits(offset);
      uint16_t offset_high = High16Bits(offset);
      int32_t anchor = 2 * kInstructionSize;
      uint16_t encoded_anchor = (anchor >> 2) & kBranchOffsetMask;
      //       bal  .+8   ; RA <- anchor
      //       lui  AT, offset_high
      //  anchor:
      //       ori  AT, AT, offset_low
      //       addu AT, AT, RA
      //       jalr ZERO, AT
      buffer->Store<int32_t>(location_ + 0 * kInstructionSize, EncodingBal(encoded_anchor));
      buffer->Store<int32_t>(location_ + 1 * kInstructionSize, EncodingLui(AT, offset_high));
      buffer->Store<int32_t>(location_ + 2 * kInstructionSize, EncodingOri(AT, AT, offset_low));
      buffer->Store<int32_t>(location_ + 3 * kInstructionSize, EncodingAddu(AT, AT, RA));
      buffer->Store<int32_t>(location_ + 4 * kInstructionSize, EncodingJalr(ZERO, AT));
      break;
    }

    case Size::kLargeConditionalBranch: {
      CHECK_EQ(type_, Type::kConditionalBranch);
      CHECK_NE(rt_, kNoRegister);
      CHECK_NE(rs_, kNoRegister);
      uint16_t offset_low = Low16Bits(offset);
      uint16_t offset_high = High16Bits(offset);
      int32_t anchor = 2 * kInstructionSize;
      uint16_t encoded_anchor = (anchor >> 2) & kBranchOffsetMask;
      int16_t hop = 6 * kInstructionSize;
      uint16_t encoded_hop = (hop >> 2) & kBranchOffsetMask;
      int32_t reversed_condition_encoding = 0;
      switch (cond_) {
        case Condition::kEq:
          reversed_condition_encoding = EncodingBne(rt_, rs_, encoded_hop);
          break;
        case Condition::kNe:
          reversed_condition_encoding = EncodingBeq(rt_, rs_, encoded_hop);
          break;
        default:
          LOG(FATAL) << "Unexpected condition " << cond_;
      }
      //       b<RevCond> rs, rt, hop
      //       bal        .+8   ; RA <- anchor
      //       lui        AT, offset_high
      //   anchor:
      //       ori        AT, AT, offset_low
      //       addu       AT, AT, RA
      //       jalr       ZERO, AT
      //   hop:
      buffer->Store<int32_t>(location_ + 0 * kInstructionSize, reversed_condition_encoding);
      buffer->Store<int32_t>(location_ + 1 * kInstructionSize, EncodingBal(encoded_anchor));
      buffer->Store<int32_t>(location_ + 2 * kInstructionSize, EncodingLui(AT, offset_high));
      buffer->Store<int32_t>(location_ + 3 * kInstructionSize, EncodingOri(AT, AT, offset_low));
      buffer->Store<int32_t>(location_ + 4 * kInstructionSize, EncodingAddu(AT, AT, RA));
      buffer->Store<int32_t>(location_ + 5 * kInstructionSize, EncodingJalr(ZERO, AT));
      break;
    }

    case Size::kLargeConditionalBranchCompareToZero: {
      CHECK_EQ(type_, Type::kConditionalBranchCompareToZero);
      CHECK_EQ(rt_, kNoRegister);
      CHECK_NE(rs_, kNoRegister);
      uint16_t offset_low = Low16Bits(offset);
      uint16_t offset_high = High16Bits(offset);
      int32_t anchor = 2 * kInstructionSize;
      uint16_t encoded_anchor = (anchor >> 2) & kBranchOffsetMask;
      int16_t hop = 6 * kInstructionSize;
      uint16_t encoded_hop = (hop >> 2) & kBranchOffsetMask;
      int32_t reversed_condition_encoding = 0;
      switch (cond_) {
        case Condition::kLtz:
          reversed_condition_encoding = EncodingBgez(rs_, encoded_hop);
          break;
        case Condition::kLez:
          reversed_condition_encoding = EncodingBgtz(rs_, encoded_hop);
          break;
        case Condition::kGtz:
          reversed_condition_encoding = EncodingBlez(rs_, encoded_hop);
          break;
        case Condition::kGez:
          reversed_condition_encoding = EncodingBltz(rs_, encoded_hop);
          break;
        default:
          LOG(FATAL) << "Unexpected condition " << cond_;
      }
      //       b<RevCond>z rs, hop
      //       bal         .+8   ; RA <- anchor
      //       lui         AT, offset_high
      //   anchor:
      //       ori         AT, AT, offset_low
      //       addu        AT, AT, RA
      //       jalr        ZERO, AT
      //   hop:
      buffer->Store<int32_t>(location_ + 0 * kInstructionSize, reversed_condition_encoding);
      buffer->Store<int32_t>(location_ + 1 * kInstructionSize, EncodingBal(encoded_anchor));
      buffer->Store<int32_t>(location_ + 2 * kInstructionSize, EncodingLui(AT, offset_high));
      buffer->Store<int32_t>(location_ + 3 * kInstructionSize, EncodingOri(AT, AT, offset_low));
      buffer->Store<int32_t>(location_ + 4 * kInstructionSize, EncodingAddu(AT, AT, RA));
      buffer->Store<int32_t>(location_ + 5 * kInstructionSize, EncodingJalr(ZERO, AT));
      break;
    }

    default:
      LOG(FATAL) << "Unexpected size " << size_;
  }
}

void MipsAssembler::AdjustFixupIfNeeded(Fixup* fixup, uint32_t* current_code_size,
                                        std::deque<FixupId>* fixups_to_recalculate) {
  uint32_t adjustment = fixup->AdjustSizeIfNeeded(*current_code_size);
  if (adjustment != 0u) {
    *current_code_size += adjustment;
    for (FixupId dependent_id : fixup->Dependents(*this)) {
      Fixup* dependent = GetFixup(dependent_id);
      dependent->IncreaseAdjustment(adjustment);
      if (buffer_.Load<int32_t>(dependent->GetLocation()) == 0) {
        buffer_.Store<int32_t>(dependent->GetLocation(), 1);
        fixups_to_recalculate->push_back(dependent_id);
      }
    }
  }
}

uint32_t MipsAssembler::AdjustFixups() {
  Fixup::PrepareDependents(this);
  uint32_t current_code_size = buffer_.Size();
  std::deque<FixupId> fixups_to_recalculate;
  if (kIsDebugBuild) {
    // We will use the placeholders in the buffer_ to mark whether the fixup has
    // been added to the fixups_to_recalculate. Make sure we start with zeros.
    for (Fixup& fixup : fixups_) {
      CHECK_EQ(buffer_.Load<int32_t>(fixup.GetLocation()), 0);
    }
  }
  for (Fixup& fixup : fixups_) {
    AdjustFixupIfNeeded(&fixup, &current_code_size, &fixups_to_recalculate);
  }
  while (!fixups_to_recalculate.empty()) {
    // Pop the fixup.
    FixupId fixup_id = fixups_to_recalculate.front();
    fixups_to_recalculate.pop_front();
    Fixup* fixup = GetFixup(fixup_id);
    CHECK_NE(buffer_.Load<int32_t>(fixup->GetLocation()), 0);
    buffer_.Store<int32_t>(fixup->GetLocation(), 0);
    // See if it needs adjustment.
    AdjustFixupIfNeeded(fixup, &current_code_size, &fixups_to_recalculate);
  }
  if (kIsDebugBuild) {
    // Check that no fixup is marked as being in fixups_to_recalculate anymore.
    for (Fixup& fixup : fixups_) {
      CHECK_EQ(buffer_.Load<int32_t>(fixup.GetLocation()), 0);
    }
  }
  return current_code_size;
}

void MipsAssembler::EmitFixups(uint32_t adjusted_code_size) {
  // Move non-fixup code to its final place and emit fixups.
  // Process fixups in reverse order so that we don't repeatedly move the same data.
  size_t src_end = buffer_.Size();
  size_t dest_end = adjusted_code_size;
  buffer_.Resize(dest_end);
  CHECK_GE(dest_end, src_end);
  for (auto i = fixups_.rbegin(), end = fixups_.rend(); i != end; ++i) {
    Fixup* fixup = &*i;
    if (fixup->GetOriginalSize() == fixup->GetSize()) {
      // The size of this Fixup didn't change. To avoid moving the data
      // in small chunks, emit the code to its original position.
      fixup->Emit(&buffer_);
      fixup->Finalize(dest_end - src_end);
    } else {
      // Move the data between the end of the fixup and src_end to its final location.
      size_t old_fixup_location = fixup->GetLocation();
      size_t src_begin = old_fixup_location + fixup->GetOriginalSizeInBytes();
      size_t data_size = src_end - src_begin;
      size_t dest_begin  = dest_end - data_size;
      buffer_.Move(dest_begin, src_begin, data_size);
      src_end = old_fixup_location;
      dest_end = dest_begin - fixup->GetSizeInBytes();
      // Finalize the Fixup and emit the data to the new location.
      fixup->Finalize(dest_end - src_end);
      fixup->Emit(&buffer_);
    }
  }
  CHECK_EQ(src_end, dest_end);
}

void MipsAssembler::FinalizeCode() {
  Assembler::FinalizeCode();
  uint32_t adjusted_code_size = AdjustFixups();
  EmitFixups(adjusted_code_size);
}

void MipsAssembler::Add(Register rd, Register rs, Register rt) {
  EmitR(0, rs, rt, rd, 0, 0x20);
}

void MipsAssembler::Addu(Register rd, Register rs, Register rt) {
  Emit(EncodingAddu(rd, rs, rt));
}

void MipsAssembler::Addi(Register rt, Register rs, uint16_t imm16) {
  EmitI(0x8, rs, rt, imm16);
}

void MipsAssembler::Addiu(Register rt, Register rs, uint16_t imm16) {
  EmitI(0x9, rs, rt, imm16);
}

void MipsAssembler::Sub(Register rd, Register rs, Register rt) {
  EmitR(0, rs, rt, rd, 0, 0x22);
}

void MipsAssembler::Subu(Register rd, Register rs, Register rt) {
  EmitR(0, rs, rt, rd, 0, 0x23);
}

void MipsAssembler::Mult(Register rs, Register rt) {
  EmitR(0, rs, rt, static_cast<Register>(0), 0, 0x18);
}

void MipsAssembler::Multu(Register rs, Register rt) {
  EmitR(0, rs, rt, static_cast<Register>(0), 0, 0x19);
}

void MipsAssembler::Div(Register rs, Register rt) {
  EmitR(0, rs, rt, static_cast<Register>(0), 0, 0x1a);
}

void MipsAssembler::Divu(Register rs, Register rt) {
  EmitR(0, rs, rt, static_cast<Register>(0), 0, 0x1b);
}

void MipsAssembler::And(Register rd, Register rs, Register rt) {
  EmitR(0, rs, rt, rd, 0, 0x24);
}

void MipsAssembler::Andi(Register rt, Register rs, uint16_t imm16) {
  EmitI(0xc, rs, rt, imm16);
}

void MipsAssembler::Or(Register rd, Register rs, Register rt) {
  EmitR(0, rs, rt, rd, 0, 0x25);
}

void MipsAssembler::Ori(Register rt, Register rs, uint16_t imm16) {
  Emit(EncodingOri(rt, rs, imm16));
}

void MipsAssembler::Xor(Register rd, Register rs, Register rt) {
  EmitR(0, rs, rt, rd, 0, 0x26);
}

void MipsAssembler::Xori(Register rt, Register rs, uint16_t imm16) {
  EmitI(0xe, rs, rt, imm16);
}

void MipsAssembler::Nor(Register rd, Register rs, Register rt) {
  EmitR(0, rs, rt, rd, 0, 0x27);
}

void MipsAssembler::Seb(Register rd, Register rt) {
  EmitR(0x1f, static_cast<Register>(0), rt, rd, 0x10, 0x20);
}

void MipsAssembler::Seh(Register rd, Register rt) {
  EmitR(0x1f, static_cast<Register>(0), rt, rd, 0x18, 0x20);
}

void MipsAssembler::Sll(Register rd, Register rs, int shamt) {
  EmitR(0, rs, static_cast<Register>(0), rd, shamt, 0x00);
}

void MipsAssembler::Srl(Register rd, Register rs, int shamt) {
  EmitR(0, rs, static_cast<Register>(0), rd, shamt, 0x02);
}

void MipsAssembler::Sra(Register rd, Register rs, int shamt) {
  EmitR(0, rs, static_cast<Register>(0), rd, shamt, 0x03);
}

void MipsAssembler::Sllv(Register rd, Register rs, Register rt) {
  EmitR(0, rs, rt, rd, 0, 0x04);
}

void MipsAssembler::Srlv(Register rd, Register rs, Register rt) {
  EmitR(0, rs, rt, rd, 0, 0x06);
}

void MipsAssembler::Srav(Register rd, Register rs, Register rt) {
  EmitR(0, rs, rt, rd, 0, 0x07);
}

void MipsAssembler::Lb(Register rt, Register rs, uint16_t imm16) {
  EmitI(0x20, rs, rt, imm16);
}

void MipsAssembler::Lh(Register rt, Register rs, uint16_t imm16) {
  EmitI(0x21, rs, rt, imm16);
}

void MipsAssembler::Lw(Register rt, Register rs, uint16_t imm16) {
  EmitI(0x23, rs, rt, imm16);
}

void MipsAssembler::Lbu(Register rt, Register rs, uint16_t imm16) {
  EmitI(0x24, rs, rt, imm16);
}

void MipsAssembler::Lhu(Register rt, Register rs, uint16_t imm16) {
  EmitI(0x25, rs, rt, imm16);
}

void MipsAssembler::Lui(Register rt, uint16_t imm16) {
  Emit(EncodingLui(rt, imm16));
}

void MipsAssembler::Sync(uint32_t stype) {
  constexpr uint32_t kStypeMask = 0x1f;
  CHECK_LE(stype, kStypeMask);
  EmitR(0, static_cast<Register>(0), static_cast<Register>(0), static_cast<Register>(0),
        stype & kStypeMask, 0xf);
}

void MipsAssembler::Mfhi(Register rd) {
  EmitR(0, static_cast<Register>(0), static_cast<Register>(0), rd, 0, 0x10);
}

void MipsAssembler::Mflo(Register rd) {
  EmitR(0, static_cast<Register>(0), static_cast<Register>(0), rd, 0, 0x12);
}

void MipsAssembler::Sb(Register rt, Register rs, uint16_t imm16) {
  EmitI(0x28, rs, rt, imm16);
}

void MipsAssembler::Sh(Register rt, Register rs, uint16_t imm16) {
  EmitI(0x29, rs, rt, imm16);
}

void MipsAssembler::Sw(Register rt, Register rs, uint16_t imm16) {
  EmitI(0x2b, rs, rt, imm16);
}

void MipsAssembler::Slt(Register rd, Register rs, Register rt) {
  EmitR(0, rs, rt, rd, 0, 0x2a);
}

void MipsAssembler::Sltu(Register rd, Register rs, Register rt) {
  EmitR(0, rs, rt, rd, 0, 0x2b);
}

void MipsAssembler::Slti(Register rt, Register rs, uint16_t imm16) {
  EmitI(0xa, rs, rt, imm16);
}

void MipsAssembler::Sltiu(Register rt, Register rs, uint16_t imm16) {
  EmitI(0xb, rs, rt, imm16);
}

void MipsAssembler::B(uint16_t offset) {
  Emit(EncodingB(offset));
  Nop();
}

void MipsAssembler::Bal(uint16_t offset) {
  Emit(EncodingBal(offset));
  Nop();
}

void MipsAssembler::Beq(Register rt, Register rs, uint16_t offset) {
  Emit(EncodingBeq(rt, rs, offset));
  Nop();
}

void MipsAssembler::Bne(Register rt, Register rs, uint16_t offset) {
  Emit(EncodingBne(rt, rs, offset));
  Nop();
}

void MipsAssembler::Bltz(Register rs, uint16_t offset) {
  Emit(EncodingBltz(rs, offset));
  Nop();
}

void MipsAssembler::Blez(Register rs, uint16_t offset) {
  Emit(EncodingBlez(rs, offset));
  Nop();
}

void MipsAssembler::Bgtz(Register rs, uint16_t offset) {
  Emit(EncodingBgtz(rs, offset));
  Nop();
}

void MipsAssembler::Bgez(Register rs, uint16_t offset) {
  Emit(EncodingBgez(rs, offset));
  Nop();
}

void MipsAssembler::J(uint32_t address) {
  EmitJ(0x2, address);
  Nop();
}

void MipsAssembler::Jal(uint32_t address) {
  EmitJ(0x3, address);
  Nop();
}

void MipsAssembler::Jr(Register rs) {
  EmitR(0, rs, static_cast<Register>(0), static_cast<Register>(0), 0, 0x09);  // Jalr zero, rs
  Nop();
}

void MipsAssembler::Jalr(Register rd, Register rs) {
  Emit(EncodingJalr(rd, rs));
  Nop();
}

void MipsAssembler::Jalr(Register rs) {
  Jalr(RA, rs);
}

void MipsAssembler::B(Label* label) {
  EmitUnconditionalBranchFixup(label);
}

void MipsAssembler::Bal(Label* label) {
  // Note: Out-of-range (fixup-based) BAL is not supported because
  // we do not expect to need it.
  int offset;
  if (label->IsBound()) {
    offset = label->Position() - buffer_.Size();
  } else {
    // Use the offset field of the jump instruction for linking the sites.
    offset = label->position_;
    label->LinkTo(buffer_.Size());
  }
  Bal((offset >> 2) & kBranchOffsetMask);
}

void MipsAssembler::Beq(Register rt, Register rs, Label* label) {
  EmitConditionalBranchFixup(rt, rs, label, Condition::kEq);
}

void MipsAssembler::Bne(Register rt, Register rs, Label* label) {
  EmitConditionalBranchFixup(rt, rs, label, Condition::kNe);
}

void MipsAssembler::Bltz(Register rs, Label* label) {
  EmitConditionalBranchCompareToZeroFixup(rs, label, Condition::kLtz);
}

void MipsAssembler::Blez(Register rs, Label* label) {
  EmitConditionalBranchCompareToZeroFixup(rs, label, Condition::kLez);
}

void MipsAssembler::Bgtz(Register rs, Label* label) {
  EmitConditionalBranchCompareToZeroFixup(rs, label, Condition::kGtz);
}

void MipsAssembler::Bgez(Register rs, Label* label) {
  EmitConditionalBranchCompareToZeroFixup(rs, label, Condition::kGez);
}

void MipsAssembler::BranchOnLowerThan(Register rt, Register rs, Label* label) {
  Slt(AT, rt, rs);
  Bne(AT, ZERO, label);
}

void MipsAssembler::BranchOnLowerThanOrEqual(Register rt, Register rs, Label* label) {
  // Implement `rt <= rs` as `!(rs < rt)` since there is no SLE instruction.
  Slt(AT, rs, rt);
  Beq(AT, ZERO, label);
}

void MipsAssembler::BranchOnGreaterThan(Register rt, Register rs, Label* label) {
  // Implement `rt > rs` as `(rs < rt)` since there is no SGT instruction.
  Slt(AT, rs, rt);
  Bne(AT, ZERO, label);
}

void MipsAssembler::BranchOnGreaterThanOrEqual(Register rt, Register rs, Label* label) {
  // Implement `rt >= rs` as `!(rt < rs)` since there is no SGE instruction.
  Slt(AT, rt, rs);
  Beq(AT, ZERO, label);
}

void MipsAssembler::BranchOnLowerThanUnsigned(Register rt, Register rs, Label* label) {
  Sltu(AT, rt, rs);
  Bne(AT, ZERO, label);
}

void MipsAssembler::BranchOnLowerThanOrEqualUnsigned(Register rt, Register rs, Label* label) {
  // Implement `rt <= rs` as `!(rs < rt)` since there is no SLE instruction.
  Sltu(AT, rs, rt);
  Beq(AT, ZERO, label);
}

void MipsAssembler::BranchOnGreaterThanUnsigned(Register rt, Register rs, Label* label) {
  // Implement `rt > rs` as `(rs < rt)` since there is no SGT instruction.
  Sltu(AT, rs, rt);
  Bne(AT, ZERO, label);
}

void MipsAssembler::BranchOnGreaterThanOrEqualUnsigned(Register rt, Register rs, Label* label) {
  // Implement `rt >= rs` as `!(rt < rs)` since there is no SGE instruction.
  Sltu(AT, rt, rs);
  Beq(AT, ZERO, label);
}

void MipsAssembler::AddS(FRegister fd, FRegister fs, FRegister ft) {
  EmitFR(0x11, 0x10, ft, fs, fd, 0x0);
}

void MipsAssembler::SubS(FRegister fd, FRegister fs, FRegister ft) {
  EmitFR(0x11, 0x10, ft, fs, fd, 0x1);
}

void MipsAssembler::MulS(FRegister fd, FRegister fs, FRegister ft) {
  EmitFR(0x11, 0x10, ft, fs, fd, 0x2);
}

void MipsAssembler::DivS(FRegister fd, FRegister fs, FRegister ft) {
  EmitFR(0x11, 0x10, ft, fs, fd, 0x3);
}

void MipsAssembler::AddD(DRegister fd, DRegister fs, DRegister ft) {
  EmitFR(0x11, 0x11, static_cast<FRegister>(ft), static_cast<FRegister>(fs),
         static_cast<FRegister>(fd), 0x0);
}

void MipsAssembler::SubD(DRegister fd, DRegister fs, DRegister ft) {
  EmitFR(0x11, 0x11, static_cast<FRegister>(ft), static_cast<FRegister>(fs),
         static_cast<FRegister>(fd), 0x1);
}

void MipsAssembler::MulD(DRegister fd, DRegister fs, DRegister ft) {
  EmitFR(0x11, 0x11, static_cast<FRegister>(ft), static_cast<FRegister>(fs),
         static_cast<FRegister>(fd), 0x2);
}

void MipsAssembler::DivD(DRegister fd, DRegister fs, DRegister ft) {
  EmitFR(0x11, 0x11, static_cast<FRegister>(ft), static_cast<FRegister>(fs),
         static_cast<FRegister>(fd), 0x3);
}

void MipsAssembler::MovS(FRegister fd, FRegister fs) {
  EmitFR(0x11, 0x10, static_cast<FRegister>(0), fs, fd, 0x6);
}

void MipsAssembler::MovD(DRegister fd, DRegister fs) {
  EmitFR(0x11, 0x11, static_cast<FRegister>(0), static_cast<FRegister>(fs),
         static_cast<FRegister>(fd), 0x6);
}

void MipsAssembler::NegS(FRegister fd, FRegister fs) {
  EmitFR(0x11, 0x10, static_cast<FRegister>(0), fs, fd, 0x7);
}

void MipsAssembler::NegD(DRegister fd, DRegister fs) {
  EmitFR(0x11, 0x11, static_cast<FRegister>(0), static_cast<FRegister>(fs),
         static_cast<FRegister>(fd), 0x7);
}

void MipsAssembler::Cvtsw(FRegister fd, FRegister fs) {
  EmitFR(0x11, 0x14, static_cast<FRegister>(0), fs, fd, 0x20);
}

void MipsAssembler::Cvtdw(DRegister fd, FRegister fs) {
  EmitFR(0x11, 0x14, static_cast<FRegister>(0), fs, static_cast<FRegister>(fd), 0x21);
}

void MipsAssembler::Cvtsd(FRegister fd, DRegister fs) {
  EmitFR(0x11, 0x11, static_cast<FRegister>(0), static_cast<FRegister>(fs), fd, 0x20);
}

void MipsAssembler::Cvtds(DRegister fd, FRegister fs) {
  EmitFR(0x11, 0x10, static_cast<FRegister>(0), fs, static_cast<FRegister>(fd), 0x21);
}

void MipsAssembler::Mfc1(Register rt, FRegister fs) {
  EmitFR(0x11, 0x00, static_cast<FRegister>(rt), fs, static_cast<FRegister>(0), 0x0);
}

void MipsAssembler::Mtc1(FRegister ft, Register rs) {
  EmitFR(0x11, 0x04, ft, static_cast<FRegister>(rs), static_cast<FRegister>(0), 0x0);
}

void MipsAssembler::Lwc1(FRegister ft, Register rs, uint16_t imm16) {
  EmitI(0x31, rs, static_cast<Register>(ft), imm16);
}

void MipsAssembler::Ldc1(DRegister ft, Register rs, uint16_t imm16) {
  EmitI(0x35, rs, static_cast<Register>(ft), imm16);
}

void MipsAssembler::Swc1(FRegister ft, Register rs, uint16_t imm16) {
  EmitI(0x39, rs, static_cast<Register>(ft), imm16);
}

void MipsAssembler::Sdc1(DRegister ft, Register rs, uint16_t imm16) {
  EmitI(0x3d, rs, static_cast<Register>(ft), imm16);
}

void MipsAssembler::Break() {
  EmitR(0, static_cast<Register>(0), static_cast<Register>(0),
        static_cast<Register>(0), 0, 0xD);
}

void MipsAssembler::Nop() {
  EmitR(0x0, static_cast<Register>(0), static_cast<Register>(0), static_cast<Register>(0), 0, 0x0);
}

void MipsAssembler::Move(Register rt, Register rs) {
  EmitI(0x9, rs, rt, 0);    // Addiu
}

void MipsAssembler::Clear(Register rt) {
  EmitR(0, static_cast<Register>(0), static_cast<Register>(0), rt, 0, 0x20);
}

void MipsAssembler::Not(Register rt, Register rs) {
  EmitR(0, static_cast<Register>(0), rs, rt, 0, 0x27);
}

void MipsAssembler::Mul(Register rd, Register rs, Register rt) {
  Mult(rs, rt);
  Mflo(rd);
}

void MipsAssembler::Div(Register rd, Register rs, Register rt) {
  Div(rs, rt);
  Mflo(rd);
}

void MipsAssembler::Rem(Register rd, Register rs, Register rt) {
  Div(rs, rt);
  Mfhi(rd);
}

void MipsAssembler::AddConstant(Register rt, Register rs, int32_t value) {
  Addiu(rt, rs, value);
}

void MipsAssembler::LoadImmediate(Register rt, int32_t value) {
  Addiu(rt, ZERO, value);
}

void MipsAssembler::LoadSImmediate(FRegister rt, float value) {
  int32_t int_value = bit_cast<int32_t, float>(value);
  if (int_value == bit_cast<int32_t, float>(0.0f)) {
    Mtc1(rt, ZERO);
  } else {
    LoadImmediate(AT, int_value);
    Mtc1(rt, AT);
  }
}

void MipsAssembler::LoadDImmediate(DRegister rt, double value) {
  uint64_t int_value = bit_cast<uint64_t, double>(value);
  FRegister low = static_cast<FRegister>(rt * 2);
  FRegister high = static_cast<FRegister>(low + 1);
  if (int_value == bit_cast<uint64_t, double>(0.0)) {
    Mtc1(low, ZERO);
    Mtc1(high, ZERO);
  } else {
    LoadSImmediate(low, bit_cast<float, uint32_t>(Low32Bits(int_value)));
    if (High32Bits(int_value) == Low32Bits(int_value)) {
      MovS(high, low);
    } else {
      LoadSImmediate(high, bit_cast<float, uint32_t>(High32Bits(int_value)));
    }
  }
}

void MipsAssembler::EmitLoad(ManagedRegister m_dst, Register src_register, int32_t src_offset,
                             size_t size) {
  MipsManagedRegister dst = m_dst.AsMips();
  if (dst.IsNoRegister()) {
    CHECK_EQ(0u, size) << dst;
  } else if (dst.IsCoreRegister()) {
    CHECK_EQ(4u, size) << dst;
    LoadFromOffset(kLoadWord, dst.AsCoreRegister(), src_register, src_offset);
  } else if (dst.IsRegisterPair()) {
    CHECK_EQ(8u, size) << dst;
    LoadFromOffset(kLoadWord, dst.AsRegisterPairLow(), src_register, src_offset);
    LoadFromOffset(kLoadWord, dst.AsRegisterPairHigh(), src_register, src_offset + 4);
  } else if (dst.IsFRegister()) {
    LoadSFromOffset(dst.AsFRegister(), src_register, src_offset);
  } else {
    CHECK(dst.IsDRegister()) << dst;
    LoadDFromOffset(dst.AsDRegister(), src_register, src_offset);
  }
}

void MipsAssembler::LoadFromOffset(LoadOperandType type, Register reg, Register base,
                                   int32_t offset) {
  if (!IsInt<16>(offset)) {
    CHECK_NE(base, AT);
    LoadImmediate(AT, offset);
    Addu(AT, AT, base);
    base = AT;
    offset = 0;
  }
  CHECK(IsInt<16>(offset));
  switch (type) {
    case kLoadSignedByte:
      Lb(reg, base, offset);
      break;
    case kLoadUnsignedByte:
      Lbu(reg, base, offset);
      break;
    case kLoadSignedHalfword:
      Lh(reg, base, offset);
      break;
    case kLoadUnsignedHalfword:
      Lhu(reg, base, offset);
      break;
    case kLoadWord:
      Lw(reg, base, offset);
      break;
    case kLoadWordPair:
      LOG(FATAL) << "UNREACHABLE";
      break;
    default:
      LOG(FATAL) << "UNREACHABLE";
  }
}

void MipsAssembler::LoadSFromOffset(FRegister reg, Register base, int32_t offset) {
  if (!IsInt<16>(offset)) {
    CHECK_NE(base, AT);
    LoadImmediate(AT, offset);
    Addu(AT, AT, base);
    base = AT;
    offset = 0;
  }
  CHECK(IsInt<16>(offset));
  Lwc1(reg, base, offset);
}

void MipsAssembler::LoadDFromOffset(DRegister reg, Register base, int32_t offset) {
  if (!IsInt<16>(offset)) {
    CHECK_NE(base, AT);
    LoadImmediate(AT, offset);
    Addu(AT, AT, base);
    base = AT;
    offset = 0;
  }
  CHECK(IsInt<16>(offset));
  Ldc1(reg, base, offset);
}

void MipsAssembler::StoreToOffset(StoreOperandType type, Register reg, Register base,
                                  int32_t offset) {
  Register tmp_reg = kNoRegister;
  if (!IsInt<16>(offset)) {
    CHECK_NE(base, AT);
    if (reg != AT) {
      tmp_reg = AT;
    } else {
      // Be careful not to use AT twice (for `reg` and `base`) in the
      // store instruction below).  Instead, save S0 on the stack (or
      // S1 if S0 is already used by `base`), use it as secondary
      // temporary register, and restore it after the store
      // instruction has been emitted.
      tmp_reg = (base != S0) ? S0 : S1;
      Push(tmp_reg);
      if (base == SP) {
        offset += kRegisterSize;
      }
    }
    LoadImmediate(tmp_reg, offset);
    Addu(tmp_reg, tmp_reg, base);
    base = tmp_reg;
    offset = 0;
  }
  CHECK(IsInt<16>(offset));
  switch (type) {
    case kStoreByte:
      Sb(reg, base, offset);
      break;
    case kStoreHalfword:
      Sh(reg, base, offset);
      break;
    case kStoreWord:
      Sw(reg, base, offset);
      break;
    case kStoreWordPair:
      LOG(FATAL) << "UNREACHABLE";
      break;
    default:
      LOG(FATAL) << "UNREACHABLE";
  }
  if (tmp_reg != kNoRegister && tmp_reg != AT) {
    CHECK((tmp_reg == S0) || (tmp_reg == S1));
    Pop(tmp_reg);
  }
}

void MipsAssembler::StoreSToOffset(FRegister reg, Register base, int32_t offset) {
  if (!IsInt<16>(offset)) {
    CHECK_NE(base, AT);
    LoadImmediate(AT, offset);
    Addu(AT, AT, base);
    base = AT;
    offset = 0;
  }
  CHECK(IsInt<16>(offset));
  Swc1(reg, base, offset);
}

void MipsAssembler::StoreDToOffset(DRegister reg, Register base, int32_t offset) {
  if (!IsInt<16>(offset)) {
    CHECK_NE(base, AT);
    LoadImmediate(AT, offset);
    Addu(AT, AT, base);
    base = AT;
    offset = 0;
  }
  CHECK(IsInt<16>(offset));
  Sdc1(reg, base, offset);
}

void MipsAssembler::Push(Register reg) {
  IncreaseFrameSize(kRegisterSize);
  Sw(Register(reg), SP, 0);
}

void MipsAssembler::Pop(Register reg) {
  Lw(Register(reg), SP, 0);
  DecreaseFrameSize(kRegisterSize);
}

static dwarf::Reg DWARFReg(Register reg) {
  return dwarf::Reg::MipsCore(static_cast<int>(reg));
}

constexpr size_t kFramePointerSize = 4;

void MipsAssembler::BuildFrame(size_t frame_size, ManagedRegister method_reg,
                               const std::vector<ManagedRegister>& callee_save_regs,
                               const ManagedRegisterEntrySpills& entry_spills) {
  CHECK_ALIGNED(frame_size, kStackAlignment);

  // Increase frame to required size.
  IncreaseFrameSize(frame_size);

  // Push callee saves and return address
  int stack_offset = frame_size - kFramePointerSize;
  StoreToOffset(kStoreWord, RA, SP, stack_offset);
  cfi_.RelOffset(DWARFReg(RA), stack_offset);
  for (int i = callee_save_regs.size() - 1; i >= 0; --i) {
    stack_offset -= kFramePointerSize;
    Register reg = callee_save_regs.at(i).AsMips().AsCoreRegister();
    StoreToOffset(kStoreWord, reg, SP, stack_offset);
    cfi_.RelOffset(DWARFReg(reg), stack_offset);
  }

  // Write out Method*.
  StoreToOffset(kStoreWord, method_reg.AsMips().AsCoreRegister(), SP, 0);

  // Write out entry spills.
  for (size_t i = 0; i < entry_spills.size(); ++i) {
    Register reg = entry_spills.at(i).AsMips().AsCoreRegister();
    StoreToOffset(kStoreWord, reg, SP, frame_size + kFramePointerSize + (i * kFramePointerSize));
  }
}

void MipsAssembler::RemoveFrame(size_t frame_size,
                                const std::vector<ManagedRegister>& callee_save_regs) {
  CHECK_ALIGNED(frame_size, kStackAlignment);
  cfi_.RememberState();

  // Pop callee saves and return address
  int stack_offset = frame_size - (callee_save_regs.size() * kFramePointerSize) - kFramePointerSize;
  for (size_t i = 0; i < callee_save_regs.size(); ++i) {
    Register reg = callee_save_regs.at(i).AsMips().AsCoreRegister();
    LoadFromOffset(kLoadWord, reg, SP, stack_offset);
    cfi_.Restore(DWARFReg(reg));
    stack_offset += kFramePointerSize;
  }
  LoadFromOffset(kLoadWord, RA, SP, stack_offset);
  cfi_.Restore(DWARFReg(RA));

  // Decrease frame to required size.
  DecreaseFrameSize(frame_size);

  // Then jump to the return address.
  Jr(RA);

  // The CFI should be restored for any code that follows the exit block.
  cfi_.RestoreState();
  cfi_.DefCFAOffset(frame_size);
}

void MipsAssembler::IncreaseFrameSize(size_t adjust) {
  // TODO: Is this required?
#if 0
  CHECK_ALIGNED(adjust, kStackAlignment);
#endif
  AddConstant(SP, SP, -adjust);
  cfi_.AdjustCFAOffset(adjust);
}

void MipsAssembler::DecreaseFrameSize(size_t adjust) {
  // TODO: Is this required?
#if 0
  CHECK_ALIGNED(adjust, kStackAlignment);
#endif
  AddConstant(SP, SP, adjust);
  cfi_.AdjustCFAOffset(-adjust);
}

void MipsAssembler::Store(FrameOffset dest, ManagedRegister msrc, size_t size) {
  MipsManagedRegister src = msrc.AsMips();
  if (src.IsNoRegister()) {
    CHECK_EQ(0u, size);
  } else if (src.IsCoreRegister()) {
    CHECK_EQ(4u, size);
    StoreToOffset(kStoreWord, src.AsCoreRegister(), SP, dest.Int32Value());
  } else if (src.IsRegisterPair()) {
    CHECK_EQ(8u, size);
    StoreToOffset(kStoreWord, src.AsRegisterPairLow(), SP, dest.Int32Value());
    StoreToOffset(kStoreWord, src.AsRegisterPairHigh(),
                  SP, dest.Int32Value() + 4);
  } else if (src.IsFRegister()) {
    StoreSToOffset(src.AsFRegister(), SP, dest.Int32Value());
  } else {
    CHECK(src.IsDRegister());
    StoreDToOffset(src.AsDRegister(), SP, dest.Int32Value());
  }
}

void MipsAssembler::StoreRef(FrameOffset dest, ManagedRegister msrc) {
  MipsManagedRegister src = msrc.AsMips();
  CHECK(src.IsCoreRegister());
  StoreToOffset(kStoreWord, src.AsCoreRegister(), SP, dest.Int32Value());
}

void MipsAssembler::StoreRawPtr(FrameOffset dest, ManagedRegister msrc) {
  MipsManagedRegister src = msrc.AsMips();
  CHECK(src.IsCoreRegister());
  StoreToOffset(kStoreWord, src.AsCoreRegister(), SP, dest.Int32Value());
}

void MipsAssembler::StoreImmediateToFrame(FrameOffset dest, uint32_t imm,
                                          ManagedRegister mscratch) {
  MipsManagedRegister scratch = mscratch.AsMips();
  CHECK(scratch.IsCoreRegister()) << scratch;
  LoadImmediate(scratch.AsCoreRegister(), imm);
  StoreToOffset(kStoreWord, scratch.AsCoreRegister(), SP, dest.Int32Value());
}

void MipsAssembler::StoreImmediateToThread32(ThreadOffset<4> dest, uint32_t imm,
                                           ManagedRegister mscratch) {
  MipsManagedRegister scratch = mscratch.AsMips();
  CHECK(scratch.IsCoreRegister()) << scratch;
  LoadImmediate(scratch.AsCoreRegister(), imm);
  StoreToOffset(kStoreWord, scratch.AsCoreRegister(), S1, dest.Int32Value());
}

void MipsAssembler::StoreStackOffsetToThread32(ThreadOffset<4> thr_offs,
                                             FrameOffset fr_offs,
                                             ManagedRegister mscratch) {
  MipsManagedRegister scratch = mscratch.AsMips();
  CHECK(scratch.IsCoreRegister()) << scratch;
  AddConstant(scratch.AsCoreRegister(), SP, fr_offs.Int32Value());
  StoreToOffset(kStoreWord, scratch.AsCoreRegister(),
                S1, thr_offs.Int32Value());
}

void MipsAssembler::StoreStackPointerToThread32(ThreadOffset<4> thr_offs) {
  StoreToOffset(kStoreWord, SP, S1, thr_offs.Int32Value());
}

void MipsAssembler::StoreSpanning(FrameOffset dest, ManagedRegister msrc,
                                  FrameOffset in_off, ManagedRegister mscratch) {
  MipsManagedRegister src = msrc.AsMips();
  MipsManagedRegister scratch = mscratch.AsMips();
  StoreToOffset(kStoreWord, src.AsCoreRegister(), SP, dest.Int32Value());
  LoadFromOffset(kLoadWord, scratch.AsCoreRegister(), SP, in_off.Int32Value());
  StoreToOffset(kStoreWord, scratch.AsCoreRegister(), SP, dest.Int32Value() + 4);
}

void MipsAssembler::Load(ManagedRegister mdest, FrameOffset src, size_t size) {
  return EmitLoad(mdest, SP, src.Int32Value(), size);
}

void MipsAssembler::LoadFromThread32(ManagedRegister mdest, ThreadOffset<4> src, size_t size) {
  return EmitLoad(mdest, S1, src.Int32Value(), size);
}

void MipsAssembler::LoadRef(ManagedRegister mdest, FrameOffset src) {
  MipsManagedRegister dest = mdest.AsMips();
  CHECK(dest.IsCoreRegister());
  LoadFromOffset(kLoadWord, dest.AsCoreRegister(), SP, src.Int32Value());
}

void MipsAssembler::LoadRef(ManagedRegister mdest, ManagedRegister base, MemberOffset offs,
                            bool unpoison_reference) {
  MipsManagedRegister dest = mdest.AsMips();
  CHECK(dest.IsCoreRegister() && dest.IsCoreRegister());
  LoadFromOffset(kLoadWord, dest.AsCoreRegister(),
                 base.AsMips().AsCoreRegister(), offs.Int32Value());
  if (kPoisonHeapReferences && unpoison_reference) {
    Subu(dest.AsCoreRegister(), ZERO, dest.AsCoreRegister());
  }
}

void MipsAssembler::LoadRawPtr(ManagedRegister mdest, ManagedRegister base,
                               Offset offs) {
  MipsManagedRegister dest = mdest.AsMips();
  CHECK(dest.IsCoreRegister() && dest.IsCoreRegister()) << dest;
  LoadFromOffset(kLoadWord, dest.AsCoreRegister(),
                 base.AsMips().AsCoreRegister(), offs.Int32Value());
}

void MipsAssembler::LoadRawPtrFromThread32(ManagedRegister mdest,
                                         ThreadOffset<4> offs) {
  MipsManagedRegister dest = mdest.AsMips();
  CHECK(dest.IsCoreRegister());
  LoadFromOffset(kLoadWord, dest.AsCoreRegister(), S1, offs.Int32Value());
}

void MipsAssembler::SignExtend(ManagedRegister mreg ATTRIBUTE_UNUSED,
                               size_t size ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL) << "no sign extension necessary for mips";
}

void MipsAssembler::ZeroExtend(ManagedRegister mreg ATTRIBUTE_UNUSED,
                               size_t size ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL) << "no zero extension necessary for mips";
}

void MipsAssembler::Move(ManagedRegister mdest,
                         ManagedRegister msrc,
                         size_t size ATTRIBUTE_UNUSED) {
  MipsManagedRegister dest = mdest.AsMips();
  MipsManagedRegister src = msrc.AsMips();
  if (!dest.Equals(src)) {
    if (dest.IsCoreRegister()) {
      CHECK(src.IsCoreRegister()) << src;
      Move(dest.AsCoreRegister(), src.AsCoreRegister());
    } else if (dest.IsFRegister()) {
      CHECK(src.IsFRegister()) << src;
      MovS(dest.AsFRegister(), src.AsFRegister());
    } else if (dest.IsDRegister()) {
      CHECK(src.IsDRegister()) << src;
      MovD(dest.AsDRegister(), src.AsDRegister());
    } else {
      CHECK(dest.IsRegisterPair()) << dest;
      CHECK(src.IsRegisterPair()) << src;
      // Ensure that the first move doesn't clobber the input of the second
      if (src.AsRegisterPairHigh() != dest.AsRegisterPairLow()) {
        Move(dest.AsRegisterPairLow(), src.AsRegisterPairLow());
        Move(dest.AsRegisterPairHigh(), src.AsRegisterPairHigh());
      } else {
        Move(dest.AsRegisterPairHigh(), src.AsRegisterPairHigh());
        Move(dest.AsRegisterPairLow(), src.AsRegisterPairLow());
      }
    }
  }
}

void MipsAssembler::CopyRef(FrameOffset dest, FrameOffset src,
                            ManagedRegister mscratch) {
  MipsManagedRegister scratch = mscratch.AsMips();
  CHECK(scratch.IsCoreRegister()) << scratch;
  LoadFromOffset(kLoadWord, scratch.AsCoreRegister(), SP, src.Int32Value());
  StoreToOffset(kStoreWord, scratch.AsCoreRegister(), SP, dest.Int32Value());
}

void MipsAssembler::CopyRawPtrFromThread32(FrameOffset fr_offs,
                                         ThreadOffset<4> thr_offs,
                                         ManagedRegister mscratch) {
  MipsManagedRegister scratch = mscratch.AsMips();
  CHECK(scratch.IsCoreRegister()) << scratch;
  LoadFromOffset(kLoadWord, scratch.AsCoreRegister(),
                 S1, thr_offs.Int32Value());
  StoreToOffset(kStoreWord, scratch.AsCoreRegister(),
                SP, fr_offs.Int32Value());
}

void MipsAssembler::CopyRawPtrToThread32(ThreadOffset<4> thr_offs,
                                       FrameOffset fr_offs,
                                       ManagedRegister mscratch) {
  MipsManagedRegister scratch = mscratch.AsMips();
  CHECK(scratch.IsCoreRegister()) << scratch;
  LoadFromOffset(kLoadWord, scratch.AsCoreRegister(),
                 SP, fr_offs.Int32Value());
  StoreToOffset(kStoreWord, scratch.AsCoreRegister(),
                S1, thr_offs.Int32Value());
}

void MipsAssembler::Copy(FrameOffset dest, FrameOffset src,
                         ManagedRegister mscratch, size_t size) {
  MipsManagedRegister scratch = mscratch.AsMips();
  CHECK(scratch.IsCoreRegister()) << scratch;
  if (size == 4u) {
    LoadFromOffset(kLoadWord, scratch.AsCoreRegister(), SP, src.Int32Value());
    StoreToOffset(kStoreWord, scratch.AsCoreRegister(), SP, dest.Int32Value());
  } else {
    CHECK_EQ(size, 8u);
    LoadFromOffset(kLoadWord, scratch.AsCoreRegister(), SP, src.Int32Value());
    StoreToOffset(kStoreWord, scratch.AsCoreRegister(), SP, dest.Int32Value());
    LoadFromOffset(kLoadWord, scratch.AsCoreRegister(), SP, src.Int32Value() + 4);
    StoreToOffset(kStoreWord, scratch.AsCoreRegister(), SP, dest.Int32Value() + 4);
  }
}

void MipsAssembler::Copy(FrameOffset dest, ManagedRegister src_base, Offset src_offset,
                         ManagedRegister mscratch, size_t size) {
  Register scratch = mscratch.AsMips().AsCoreRegister();
  CHECK_EQ(size, 4u);
  LoadFromOffset(kLoadWord, scratch, src_base.AsMips().AsCoreRegister(), src_offset.Int32Value());
  StoreToOffset(kStoreWord, scratch, SP, dest.Int32Value());
}

void MipsAssembler::Copy(ManagedRegister dest_base, Offset dest_offset, FrameOffset src,
                         ManagedRegister mscratch, size_t size) {
  Register scratch = mscratch.AsMips().AsCoreRegister();
  CHECK_EQ(size, 4u);
  LoadFromOffset(kLoadWord, scratch, SP, src.Int32Value());
  StoreToOffset(kStoreWord, scratch, dest_base.AsMips().AsCoreRegister(), dest_offset.Int32Value());
}

void MipsAssembler::Copy(FrameOffset dest ATTRIBUTE_UNUSED,
                         FrameOffset src_base ATTRIBUTE_UNUSED,
                         Offset src_offset ATTRIBUTE_UNUSED,
                         ManagedRegister mscratch ATTRIBUTE_UNUSED,
                         size_t size ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL) << "no mips implementation";
}

void MipsAssembler::Copy(ManagedRegister dest, Offset dest_offset,
                         ManagedRegister src, Offset src_offset,
                         ManagedRegister mscratch, size_t size) {
  CHECK_EQ(size, 4u);
  Register scratch = mscratch.AsMips().AsCoreRegister();
  LoadFromOffset(kLoadWord, scratch, src.AsMips().AsCoreRegister(), src_offset.Int32Value());
  StoreToOffset(kStoreWord, scratch, dest.AsMips().AsCoreRegister(), dest_offset.Int32Value());
}

void MipsAssembler::Copy(FrameOffset dest ATTRIBUTE_UNUSED,
                         Offset dest_offset ATTRIBUTE_UNUSED,
                         FrameOffset src ATTRIBUTE_UNUSED,
                         Offset src_offset ATTRIBUTE_UNUSED,
                         ManagedRegister mscratch ATTRIBUTE_UNUSED,
                         size_t size ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL) << "no mips implementation";
}

void MipsAssembler::MemoryBarrier(ManagedRegister) {
  UNIMPLEMENTED(FATAL) << "no mips implementation";
}

void MipsAssembler::CreateHandleScopeEntry(ManagedRegister mout_reg,
                                           FrameOffset handle_scope_offset,
                                           ManagedRegister min_reg,
                                           bool null_allowed) {
  MipsManagedRegister out_reg = mout_reg.AsMips();
  MipsManagedRegister in_reg = min_reg.AsMips();
  CHECK(in_reg.IsNoRegister() || in_reg.IsCoreRegister()) << in_reg;
  CHECK(out_reg.IsCoreRegister()) << out_reg;
  if (null_allowed) {
    Label null_arg;
    // Null values get a handle scope entry value of 0.  Otherwise, the handle scope entry is
    // the address in the handle scope holding the reference.
    // e.g. out_reg = (handle == 0) ? 0 : (SP+handle_offset)
    if (in_reg.IsNoRegister()) {
      LoadFromOffset(kLoadWord, out_reg.AsCoreRegister(),
                     SP, handle_scope_offset.Int32Value());
      in_reg = out_reg;
    }
    if (!out_reg.Equals(in_reg)) {
      LoadImmediate(out_reg.AsCoreRegister(), 0);
    }
    Beq(in_reg.AsCoreRegister(), ZERO, &null_arg);
    AddConstant(out_reg.AsCoreRegister(), SP, handle_scope_offset.Int32Value());
    Bind(&null_arg, false);
  } else {
    AddConstant(out_reg.AsCoreRegister(), SP, handle_scope_offset.Int32Value());
  }
}

void MipsAssembler::CreateHandleScopeEntry(FrameOffset out_off,
                                           FrameOffset handle_scope_offset,
                                           ManagedRegister mscratch,
                                           bool null_allowed) {
  MipsManagedRegister scratch = mscratch.AsMips();
  CHECK(scratch.IsCoreRegister()) << scratch;
  if (null_allowed) {
    Label null_arg;
    LoadFromOffset(kLoadWord, scratch.AsCoreRegister(), SP,
                   handle_scope_offset.Int32Value());
    // Null values get a handle scope entry value of 0.  Otherwise, the handle scope entry is
    // the address in the handle scope holding the reference.
    // e.g. scratch = (scratch == 0) ? 0 : (SP+handle_scope_offset)
    Beq(scratch.AsCoreRegister(), ZERO, &null_arg);
    AddConstant(scratch.AsCoreRegister(), SP, handle_scope_offset.Int32Value());
    Bind(&null_arg, false);
  } else {
    AddConstant(scratch.AsCoreRegister(), SP, handle_scope_offset.Int32Value());
  }
  StoreToOffset(kStoreWord, scratch.AsCoreRegister(), SP, out_off.Int32Value());
}

// Given a handle scope entry, load the associated reference.
void MipsAssembler::LoadReferenceFromHandleScope(ManagedRegister mout_reg,
                                                 ManagedRegister min_reg) {
  MipsManagedRegister out_reg = mout_reg.AsMips();
  MipsManagedRegister in_reg = min_reg.AsMips();
  CHECK(out_reg.IsCoreRegister()) << out_reg;
  CHECK(in_reg.IsCoreRegister()) << in_reg;
  Label null_arg;
  if (!out_reg.Equals(in_reg)) {
    LoadImmediate(out_reg.AsCoreRegister(), 0);
  }
  Beq(in_reg.AsCoreRegister(), ZERO, &null_arg);
  LoadFromOffset(kLoadWord, out_reg.AsCoreRegister(),
                 in_reg.AsCoreRegister(), 0);
  Bind(&null_arg, false);
}

void MipsAssembler::VerifyObject(ManagedRegister src ATTRIBUTE_UNUSED,
                                 bool could_be_null ATTRIBUTE_UNUSED) {
  // TODO: not validating references
}

void MipsAssembler::VerifyObject(FrameOffset src ATTRIBUTE_UNUSED,
                                 bool could_be_null ATTRIBUTE_UNUSED) {
  // TODO: not validating references
}

void MipsAssembler::Call(ManagedRegister mbase, Offset offset, ManagedRegister mscratch) {
  MipsManagedRegister base = mbase.AsMips();
  MipsManagedRegister scratch = mscratch.AsMips();
  CHECK(base.IsCoreRegister()) << base;
  CHECK(scratch.IsCoreRegister()) << scratch;
  LoadFromOffset(kLoadWord, scratch.AsCoreRegister(),
                 base.AsCoreRegister(), offset.Int32Value());
  Jalr(scratch.AsCoreRegister());
  // TODO: place reference map on call
}

void MipsAssembler::Call(FrameOffset base, Offset offset, ManagedRegister mscratch) {
  MipsManagedRegister scratch = mscratch.AsMips();
  CHECK(scratch.IsCoreRegister()) << scratch;
  // Call *(*(SP + base) + offset)
  LoadFromOffset(kLoadWord, scratch.AsCoreRegister(),
                 SP, base.Int32Value());
  LoadFromOffset(kLoadWord, scratch.AsCoreRegister(),
                 scratch.AsCoreRegister(), offset.Int32Value());
  Jalr(scratch.AsCoreRegister());
  // TODO: place reference map on call
}

void MipsAssembler::CallFromThread32(ThreadOffset<4> offset ATTRIBUTE_UNUSED,
                                     ManagedRegister mscratch ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL) << "no mips implementation";
}

void MipsAssembler::GetCurrentThread(ManagedRegister tr) {
  Move(tr.AsMips().AsCoreRegister(), S1);
}

void MipsAssembler::GetCurrentThread(FrameOffset offset,
                                     ManagedRegister mscratch ATTRIBUTE_UNUSED) {
  StoreToOffset(kStoreWord, S1, SP, offset.Int32Value());
}

void MipsAssembler::ExceptionPoll(ManagedRegister mscratch, size_t stack_adjust) {
  MipsManagedRegister scratch = mscratch.AsMips();
  MipsExceptionSlowPath* slow = new MipsExceptionSlowPath(scratch, stack_adjust);
  buffer_.EnqueueSlowPath(slow);
  LoadFromOffset(kLoadWord, scratch.AsCoreRegister(),
                 S1, Thread::ExceptionOffset<4>().Int32Value());
  Bne(scratch.AsCoreRegister(), ZERO, slow->Entry());
}

void MipsExceptionSlowPath::Emit(Assembler* sasm) {
  MipsAssembler* sp_asm = down_cast<MipsAssembler*>(sasm);
#define __ sp_asm->
  __ Bind(&entry_, false);
  if (stack_adjust_ != 0) {  // Fix up the frame.
    __ DecreaseFrameSize(stack_adjust_);
  }
  // Pass exception object as argument
  // Don't care about preserving A0 as this call won't return
  __ Move(A0, scratch_.AsCoreRegister());
  // Set up call to Thread::Current()->pDeliverException
  __ LoadFromOffset(kLoadWord, T9, S1, QUICK_ENTRYPOINT_OFFSET(4, pDeliverException).Int32Value());
  __ Jr(T9);
  // Call never returns
  __ Break();
#undef __
}

}  // namespace mips
}  // namespace art
