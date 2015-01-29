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

static bool MaybeSuitableForCsel(HBasicBlock* block) {
  if (!block->HasOnlyOneSuccessor() || !block->HasOnlyOnePredecessor()) {
    return false;
  }
  // We want the block to only have one HGoto instruction.
  HInstruction* last = block->GetLastInstruction();
  if (last != block->GetFirstInstruction()) {
    return false;
  }
  return last->IsGoto();
}

void InstructionSimplifierArm64::ReplacePhiWithCsel(HIf* instr_if, HPhi* phi) {
  HArm64ConditionalSelect* csel =
      new (GetGraph()->GetArena()) HArm64ConditionalSelect(instr_if, phi);
  // Insert the new instruction and use it instead of the phi.
  instr_if->GetBlock()->InsertInstructionBefore(csel, instr_if);
  phi->ReplaceWith(csel);
  phi->GetBlock()->RemovePhi(phi);
}

void InstructionSimplifierArm64::VisitIf(HIf* instr) {
  HBasicBlock* true_block = instr->IfTrueSuccessor();
  HBasicBlock* false_block = instr->IfFalseSuccessor();

  if ((true_block == nullptr || !MaybeSuitableForCsel(true_block)) ||
      (false_block == nullptr || !MaybeSuitableForCsel(false_block))) {
    return;
  }
  HBasicBlock* true_block_successor = true_block->GetSuccessors().Get(0);
  HBasicBlock* false_block_successor = false_block->GetSuccessors().Get(0);
  if (true_block_successor != false_block_successor) {
    return;
  }

  HBasicBlock* successor = true_block_successor;
  if (successor->GetPredecessors().Size() != 2) {
    return;
  }

  // We simplify if-then-else block structures using a up to a certain number of
  // phis.
  // TODO: For now we settle on handling only one phi. More phis mean multiple
  // uses for the HIf's condition; this forces the HIf's condition to be
  // materialized, and the code generated is bad. The code generated in that
  // situation could be optimized.  We need to find out if we should handle more
  // phis. If we decide to only handle one, the code below can be simplified.
  const HInstructionList& phis = successor->GetPhis();
  int n_phis = 0;
  static constexpr int kMaxNumberOfPhisHandled = 1;
  for (HInstructionIterator inst_it(phis); !inst_it.Done(); inst_it.Advance()) {
    if (n_phis > kMaxNumberOfPhisHandled) {
      return;
    }
    n_phis++;
  }

  // Translate each phi to a conditional select.
  for (HInstructionIterator inst_it(phis); !inst_it.Done(); inst_it.Advance()) {
    ReplacePhiWithCsel(instr, inst_it.Current()->AsPhi());
  }

  // Clean-up the block structure.
  // The 'then' and 'else' blocks are discarded, and the 'if' and 'successor'
  // blocks are merged.
  HGraph* graph = GetGraph();
  graph->UnlinkBlock(true_block);
  graph->UnlinkBlock(false_block);
  HBasicBlock* if_block = instr->GetBlock();
  if_block->RemoveInstruction(instr);
  if_block->AddSuccessor(successor);
  if_block->AddInstruction(new (GetGraph()->GetArena()) HGoto());
  graph->MergeBlocks(if_block, successor);
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
