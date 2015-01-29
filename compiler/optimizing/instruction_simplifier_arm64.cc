/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include "instruction_simplifier_arm64.h"

namespace art {
namespace arm64 {

bool InstructionSimplifierArm64::TryFitIntoOperand(HInstruction* instr,
                                                   HInstruction* left,
                                                   HInstruction* op) {
  DCHECK(instr->IsAdd() || instr->IsAnd() || instr->IsOr() || instr->IsSub() || instr->IsXor());
  if (!op->IsArm64BitfieldMove()) {
    return false;
  }

  HArm64BitfieldMove* xbfm = op->AsArm64BitfieldMove();
  HArm64BitfieldMove::BitfieldMoveType bfm_type = xbfm->bitfield_move_type();
  if (bfm_type == HArm64BitfieldMove::kBFM) {
    // We can't perform bitfield insertion in the operand.
    return false;
  }
  DCHECK(bfm_type == HArm64BitfieldMove::kSBFM || bfm_type == HArm64BitfieldMove::kUBFM);

  HArm64ArithWithOp::OpKind op_kind = HArm64ArithWithOp::kInvalidOp;
  int shift_amount = 0;
  HArm64ArithWithOp::GetOpInfoFromEncoding(xbfm, &op_kind, &shift_amount);

  if (op_kind == HArm64ArithWithOp::kInvalidOp) {
    // The operation is not suitable to be merged.
    return false;
  }

  if (HArm64ArithWithOp::IsExtensionOp(op_kind) &&
      !(instr->IsAdd() || instr->IsAnd() || instr->IsSub())) {
    // Extension is not supported by the current instruction.
    return false;
  }

  HArm64ArithWithOp* alu_with_op = new (GetGraph()->GetArena()) HArm64ArithWithOp(instr,
                                                                                  left,
                                                                                  xbfm->InputAt(0),
                                                                                  op_kind,
                                                                                  shift_amount);
  instr->GetBlock()->ReplaceAndRemoveInstructionWith(instr, alu_with_op);
  op->GetBlock()->RemoveInstruction(op);
  return true;
}

// Try to transform a sequence of instructions looking like:
//   lsl x2, x10, #5
//   add x0, x1, x2
// into
//   add x0, x1, x10, LSL #5
void InstructionSimplifierArm64::TryMergeInputIntoOperand(HBinaryOperation* instr) {
  // TODO: Support for the `cmp` instruction. This requires tracking of
  // condition flags dependencies.
  // TODO: Support for load and store instructions.
  DCHECK(instr->IsAdd() || instr->IsAnd() || instr->IsOr() || instr->IsSub() || instr->IsXor());
  Primitive::Type type = instr->GetType();
  if (type != Primitive::kPrimInt && type != Primitive::kPrimLong) {
    return;
  }

  HInstruction* left = instr->InputAt(0);
  HInstruction* right = instr->InputAt(1);

  if (right->GetUses().HasOnlyOneUse() && !right->HasEnvironmentUses()) {
    if (TryFitIntoOperand(instr, left, right)) {
      return;
    }
  }
  if (instr->IsCommutative() && left->GetUses().HasOnlyOneUse() && !left->HasEnvironmentUses()) {
    TryFitIntoOperand(instr, right, left);
  }
}

void InstructionSimplifierArm64::VisitAdd(HAdd* instr) {
  TryMergeInputIntoOperand(instr);
}

void InstructionSimplifierArm64::VisitAnd(HAnd* instr) {
  TryMergeInputIntoOperand(instr);
}

void InstructionSimplifierArm64::VisitOr(HOr* instr) {
  TryMergeInputIntoOperand(instr);
}

void InstructionSimplifierArm64::VisitXor(HXor* instr) {
  TryMergeInputIntoOperand(instr);
}

void InstructionSimplifierArm64::VisitSub(HSub* instr) {
  TryMergeInputIntoOperand(instr);
}

}  // namespace arm64
}  // namespace art
