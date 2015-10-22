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

#include "mirror/array-inl.h"

namespace art {
namespace arm64 {

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
  if (access->IsArraySet() &&
      access->AsArraySet()->GetValue()->GetType() == Primitive::kPrimNot) {
    // The access may require a runtime call or the original array pointer.
    return;
  }

  // Proceed to extract the base address computation.
  ArenaAllocator* arena = GetGraph()->GetArena();

  HIntConstant* offset =
      GetGraph()->GetIntConstant(mirror::Array::DataOffset(access_size).Uint32Value());
  HArm64IntermediateAddress* address =
      new (arena) HArm64IntermediateAddress(array, offset, kNoDexPc);
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

void InstructionSimplifierArm64Visitor::ReplaceRotateWithRor(HBinaryOperation* op,
                                                             HUShr* ushr,
                                                             HShl* shl,
                                                             HInstruction* dist) {
  DCHECK(op->IsAdd() || op->IsXor() || op->IsOr());
  HArm64Ror* ror = new (GetGraph()->GetArena()) HArm64Ror(ushr->GetType(),
                                                          ushr->GetLeft(),
                                                          dist);
  op->GetBlock()->ReplaceAndRemoveInstructionWith(op, ror);
  ushr->GetBlock()->RemoveInstruction(ushr);
  if (!ushr->GetRight()->HasUses()) {
    ushr->GetRight()->GetBlock()->RemoveInstruction(ushr->GetRight());
  }
  shl->GetBlock()->RemoveInstruction(shl);
  if (!shl->GetRight()->HasUses()) {
    shl->GetRight()->GetBlock()->RemoveInstruction(shl->GetRight());
  }
}

// Try replacing code looking like (x >>> #rdist OP x << #ldist):
//    UShr dst, x,   #rdist
//    Shl  tmp, x,   #ldist
//    OP   dst, dst, tmp
// or like (x >>> #rdist OP x << #-ldist):
//    UShr dst, x,   #rdist
//    Shl  tmp, x,   #-ldist
//    OP   dst, dst, tmp
// with
//    Ror  dst, x,   #rdist
void InstructionSimplifierArm64Visitor::TryReplaceWithRotateConstantPattern(HBinaryOperation* op,
                                                                            HUShr* ushr,
                                                                            HShl* shl) {
  DCHECK(op->IsAdd() || op->IsXor() || op->IsOr());
  size_t reg_bits = Primitive::ComponentSize(ushr->GetType()) * kBitsPerByte;
  size_t rdist = Int64FromConstant(ushr->GetRight()->AsConstant());
  size_t ldist = Int64FromConstant(shl->GetRight()->AsConstant());
  if (((ldist + rdist) & (reg_bits - 1)) == 0) {
    ReplaceRotateWithRor(op, ushr, shl, ushr->GetRight());
    return;
  }
}

// Try replacing code looking like (x >>> d OP x << (#bits - d)):
//    UShr dst, x,     d
//    Sub  ld,  #bits, d
//    Shl  tmp, x,     ld
//    OP   dst, dst,   tmp
// with
//    Ror  dst, x,     d
// *** OR ***
// Replace code looking like (x >>> (#bits - d) OP x << d):
//    Sub  rd,  #bits, d
//    UShr dst, x,     rd
//    Shl  tmp, x,     d
//    OP   dst, dst,   tmp
// with
//    Neg  neg, d
//    Ror  dst, x,     neg
void InstructionSimplifierArm64Visitor::TryReplaceWithRotateRegisterSubPattern(HBinaryOperation* op,
                                                                               HUShr* ushr,
                                                                               HShl* shl) {
  DCHECK(op->IsAdd() || op->IsXor() || op->IsOr());
  DCHECK(ushr->GetRight()->IsSub() || shl->GetRight()->IsSub());
  bool sub_is_left = shl->GetRight()->IsSub();
  bool sub_is_right = ushr->GetRight()->IsSub();
  size_t reg_bits = Primitive::ComponentSize(ushr->GetType()) * kBitsPerByte;
  if (sub_is_left && sub_is_right) {
    // Both shift distances are the result of subtractions. Replace with a
    // rotate only if one sub equals the register size minus the other sub.
    HSub* sub_left = shl->GetRight()->AsSub();
    HSub* sub_right = ushr->GetRight()->AsSub();
    if (IsSubRegBitsMinusOther(sub_right, reg_bits, sub_left)) {
      // TODO: Neg of a Sub IR will be simplified when this all moves to the
      // generic instruction simplifier.
      ReplaceRotateWithNegRor(op, ushr, shl);
    } else if (IsSubRegBitsMinusOther(sub_left, reg_bits, sub_right)) {
      ReplaceRotateWithRor(op, ushr, shl, ushr->GetRight());
    }
  } else if (sub_is_left != sub_is_right) {
    // Only one shift distance is the result of a subtraction. Replace with a
    // rotate if it equals the register size minus the other shift distance.
    HSub* sub = sub_is_left ? shl->GetRight()->AsSub() : ushr->GetRight()->AsSub();
    if (sub->GetLeft()->IsConstant() &&
        sub->GetRight() == (sub_is_left ? ushr->GetRight() : shl->GetRight()) &&
        (Int64FromConstant(sub->GetLeft()->AsConstant()) & (reg_bits - 1)) == 0) {
      if (sub_is_left) {
        ReplaceRotateWithRor(op, ushr, shl, ushr->GetRight());
      } else {
        ReplaceRotateWithNegRor(op, ushr, shl);
      }
    }
  }
}

bool InstructionSimplifierArm64Visitor::IsSubRegBitsMinusOther(HSub* sub,
                                                               size_t reg_bits,
                                                               HSub* other) {
    return (sub->GetRight() == other &&
            sub->GetLeft()->IsConstant() &&
            (Int64FromConstant(sub->GetLeft()->AsConstant()) & (reg_bits -1)) == 0);
}

void InstructionSimplifierArm64Visitor::ReplaceRotateWithNegRor(HBinaryOperation* op,
                                                                HBinaryOperation* ushr,
                                                                HBinaryOperation* shl) {
  DCHECK(op->IsAdd() || op->IsXor() || op->IsOr());
  HNeg* neg = new (GetGraph()->GetArena()) HNeg(shl->GetRight()->GetType(), shl->GetRight());
  op->GetBlock()->InsertInstructionBefore(neg, op);
  HArm64Ror* ror = new (GetGraph()->GetArena()) HArm64Ror(ushr->GetType(), shl->GetLeft(), neg);
  op->GetBlock()->ReplaceAndRemoveInstructionWith(op, ror);
  ushr->GetBlock()->RemoveInstruction(ushr);
  if (!ushr->GetRight()->HasUses()) {
    ushr->GetRight()->GetBlock()->RemoveInstruction(ushr->GetRight());
  }
  shl->GetBlock()->RemoveInstruction(shl);
  if (!shl->GetRight()->HasUses()) {
    shl->GetRight()->GetBlock()->RemoveInstruction(shl->GetRight());
  }
}

// Replace code looking like (x >>> -d OP x << d):
//    Neg  neg, d
//    UShr dst, x,   neg
//    Shl  tmp, x,   d
//    OP   dst, dst, tmp
// with
//    Neg  neg, d
//    Ror  dst, x,   neg
// *** OR ***
// Replace code looking like (x >>> d OP x << -d):
//    UShr dst, x,   d
//    Neg  neg, d
//    Shl  tmp, x,   neg
//    OP   dst, dst, tmp
// with
//    Ror  dst, x,   d
void InstructionSimplifierArm64Visitor::TryReplaceWithRotateRegisterNegPattern(HBinaryOperation* op,
                                                                               HUShr* ushr,
                                                                               HShl* shl) {
  DCHECK(op->IsAdd() || op->IsXor() || op->IsOr());
  DCHECK(ushr->GetRight()->IsNeg() || shl->GetRight()->IsNeg());
  bool neg_is_left = shl->GetRight()->IsNeg();
  HNeg* neg = neg_is_left ? shl->GetRight()->AsNeg() : ushr->GetRight()->AsNeg();
  // And the shift distance being negated is the distance being shifted the other way.
  if (neg->InputAt(0) == (neg_is_left ? ushr->GetRight() : shl->GetRight())) {
    if (neg_is_left) {
      ReplaceRotateWithRor(op, ushr, shl, ushr->GetRight());
    } else {
      ReplaceRotateWithRor(op, ushr, shl, neg);
    }
  }
}

// Try to replace a binary operation flanked by one UShr and one Shl with a bitfield rotation.
void InstructionSimplifierArm64Visitor::TryReplaceWithRotate(HBinaryOperation* op) {
  DCHECK(op->IsAdd() || op->IsXor() || op->IsOr());
  HInstruction* left = op->GetLeft();
  HInstruction* right = op->GetRight();
  // If we have an UShr and a Shl (in either order).
  if ((left->IsUShr() && right->IsShl()) || (left->IsShl() && right->IsUShr())) {
    HUShr* ushr = left->IsUShr() ? left->AsUShr() : right->AsUShr();
    HShl* shl = left->IsShl() ? left->AsShl() : right->AsShl();
    DCHECK(Primitive::IsIntOrLongType(ushr->GetType()));
    if (ushr->GetType() == shl->GetType() &&
        ushr->GetLeft() == shl->GetLeft() &&
        ushr->HasOnlyOneNonEnvironmentUse() &&
        shl->HasOnlyOneNonEnvironmentUse()) {
      if (ushr->GetRight()->IsConstant() && shl->GetRight()->IsConstant()) {
        // Shift distances are both constant, try replacing with Ror if they
        // add up to the register size.
        TryReplaceWithRotateConstantPattern(op, ushr, shl);
      } else if (ushr->GetRight()->IsSub() || shl->GetRight()->IsSub()) {
        // Shift distances are potentially of the form x and (reg_size - x).
        TryReplaceWithRotateRegisterSubPattern(op, ushr, shl);
      } else if (ushr->GetRight()->IsNeg() || shl->GetRight()->IsNeg()) {
        // Shift distances are potentially of the form d and -d.
        TryReplaceWithRotateRegisterNegPattern(op, ushr, shl);
      }
    }
  }
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

void InstructionSimplifierArm64Visitor::VisitOr(HOr* instruction) {
  TryReplaceWithRotate(instruction);
}

void InstructionSimplifierArm64Visitor::VisitXor(HXor* instruction) {
  TryReplaceWithRotate(instruction);
}

void InstructionSimplifierArm64Visitor::VisitAdd(HAdd* instruction) {
  TryReplaceWithRotate(instruction);
}

}  // namespace arm64
}  // namespace art
