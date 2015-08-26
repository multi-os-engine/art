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

#include "common_arm64.h"
#include "mirror/array-inl.h"

namespace art {
namespace arm64 {

using helpers::CanFitInShifterOperand;
using helpers::HasShifterOperand;
using helpers::ShifterOperandSupportsExtension;

void InstructionSimplifierArm64Visitor::TryExtractArrayAccessAddress(HInstruction* access,
                                                                     HInstruction* array,
                                                                     HInstruction* index,
                                                                     int access_size) {
  if (index->IsConstant() ||
      (index->IsBoundsCheck() && index->AsBoundsCheck()->GetIndex()->IsConstant())) {
    // When the index is a constant all the addressing can be fitted in the
    // memory access instruction, so do not split the access.
    return;
  }
  if (access->IsArraySet() && access->AsArraySet()->NeedsTypeCheck()) {
    // This requires a runtime call.
    return;
  }

  // Proceed to extract the base address computation.
  ArenaAllocator* arena = GetGraph()->GetArena();

  HIntConstant* offset =
      GetGraph()->GetIntConstant(mirror::Array::DataOffset(access_size).Uint32Value());
  HArm64IntermediateAddress* address = new (arena) HArm64IntermediateAddress(array, offset);
  access->GetBlock()->InsertInstructionBefore(address, access);
  access->ReplaceInput(address, 0);
  // Both instructions must depend on GC to prevent any instruction that can
  // trigger GC to be inserted between the two.
  access->AddSideEffects(SideEffects::DependsOnGC());
  DCHECK(address->GetSideEffects().Includes(SideEffects::DependsOnGC()));
  DCHECK(access->GetSideEffects().Includes(SideEffects::DependsOnGC()));
  // TODO: Code generation for HArrayGet and HArraySet will check whether the input address
  // is an HArm64IntermediateAddress and generate appropriate code.
  // We would like to replace the `HArrayGet` and `HArraySet` with custom instructions (maybe
  // `HArm64Load` and `HArm64Store`). We defer these changes because these new instructions would
  // not bring any advantages yet.
  // Also see the comments in
  // `InstructionCodeGeneratorARM64::VisitArrayGet()` and
  // `InstructionCodeGeneratorARM64::VisitArraySet()`.
  RecordSimplification();
}

bool InstructionSimplifierArm64Visitor::TryMergeIntoShifterOperand(HBinaryOperation* binop,
                                                                   HInstruction* bitfield_op,
                                                                   bool do_merge) {
  DCHECK(HasShifterOperand(binop));
  DCHECK(CanFitInShifterOperand(bitfield_op));
  DCHECK(!bitfield_op->HasEnvironmentUses());

  Primitive::Type type = binop->GetType();
  if (type != Primitive::kPrimInt && type != Primitive::kPrimLong) {
    return false;
  }

  HInstruction* left = binop->InputAt(0);
  HInstruction* right = binop->InputAt(1);
  DCHECK(left == bitfield_op || right == bitfield_op);

  if (left == right) {
    // TODO: Handle special transformations in this situation?
    // For example should we transform `(x << 1) + (x << 1)` into `(x << 2)`?
    // Or should this be part of a separate transformation logic?
    return false;
  }

  HInstruction* other_input;
  if (bitfield_op == right) {
    other_input = left;
  } else {
    if (binop->IsCommutative()) {
      other_input = right;
    } else {
      return false;
    }
  }

  HArm64DataProcWithShifterOp::OpKind op_kind;
  int shift_amount = 0;
  HArm64DataProcWithShifterOp::GetOpInfoFromInstruction(bitfield_op, &op_kind, &shift_amount);

  if (HArm64DataProcWithShifterOp::IsExtensionOp(op_kind) &&
      !ShifterOperandSupportsExtension(binop)) {
    return false;
  }

  if (do_merge) {
    HArm64DataProcWithShifterOp* alu_with_op =
        new (GetGraph()->GetArena()) HArm64DataProcWithShifterOp(binop,
                                                                 other_input,
                                                                 bitfield_op->InputAt(0),
                                                                 op_kind,
                                                                 shift_amount);
    binop->GetBlock()->ReplaceAndRemoveInstructionWith(binop, alu_with_op);
    if (bitfield_op->GetUses().IsEmpty()) {
      bitfield_op->GetBlock()->RemoveInstruction(bitfield_op);
    }
  }

  return true;
}

// Merge a bitfield move instruction into its uses if it can be merged in all of them.
bool InstructionSimplifierArm64Visitor::TryMergeIntoUsersShifterOperand(HInstruction* bitfield_op) {
  DCHECK(CanFitInShifterOperand(bitfield_op));

  if (bitfield_op->HasEnvironmentUses()) {
    return false;
  }

  const HUseList<HInstruction*>& uses = bitfield_op->GetUses();

  // Check whether we can merge the instruction in all its users' shifter operand.
  for (HUseIterator<HInstruction*> it_use(uses); !it_use.Done(); it_use.Advance()) {
    HInstruction* use = it_use.Current()->GetUser();
    if (!HasShifterOperand(use) ||
        !CanMergeIntoShifterOperand(use->AsBinaryOperation(), bitfield_op)) {
      return false;
    }
  }

  // Merge the instruction into its uses.
  for (HUseIterator<HInstruction*> it_use(uses); !it_use.Done(); it_use.Advance()) {
    HInstruction* use = it_use.Current()->GetUser();
    bool merged = MergeIntoShifterOperand(use->AsBinaryOperation(), bitfield_op);
    DCHECK(merged);
  }

  return true;
}

void InstructionSimplifierArm64Visitor::VisitArrayGet(HArrayGet* instruction) {
  TryExtractArrayAccessAddress(instruction,
                               instruction->GetArray(),
                               instruction->GetIndex(),
                               Primitive::ComponentSize(instruction->GetType()));
}

void InstructionSimplifierArm64Visitor::VisitArraySet(HArraySet* instruction) {
  TryExtractArrayAccessAddress(instruction,
                               instruction->GetArray(),
                               instruction->GetIndex(),
                               Primitive::ComponentSize(instruction->GetComponentType()));
}

void InstructionSimplifierArm64Visitor::VisitShl(HShl* instruction) {
  if (instruction->InputAt(1)->IsConstant()) {
    TryMergeIntoUsersShifterOperand(instruction);
  }
}

void InstructionSimplifierArm64Visitor::VisitShr(HShr* instruction) {
  if (instruction->InputAt(1)->IsConstant()) {
    TryMergeIntoUsersShifterOperand(instruction);
  }
}

void InstructionSimplifierArm64Visitor::VisitTypeConversion(HTypeConversion* instruction) {
  Primitive::Type result_type = instruction->GetResultType();
  Primitive::Type input_type = instruction->GetInputType();

  if (input_type == result_type) {
    // We let the arch-independent code handle this.
    return;
  }

  if (Primitive::IsIntegralType(result_type) && Primitive::IsIntegralType(input_type)) {
    TryMergeIntoUsersShifterOperand(instruction);
  }
}

void InstructionSimplifierArm64Visitor::VisitUShr(HUShr* instruction) {
  if (instruction->InputAt(1)->IsConstant()) {
    TryMergeIntoUsersShifterOperand(instruction);
  }
}

}  // namespace arm64
}  // namespace art
