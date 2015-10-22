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

void InstructionSimplifierArm64Visitor::ReplaceOrAndShiftsWithRor(HOr* orr, HUShr* ushr, HShl* shl) {
  HArm64Ror* ror = new (GetGraph()->GetArena()) HArm64Ror(ushr->GetType(),
                                                          ushr->GetLeft(),
                                                          ushr->GetRight());
  orr->GetBlock()->ReplaceAndRemoveInstructionWith(orr, ror);
  ushr->GetBlock()->RemoveInstruction(ushr);
  shl->GetBlock()->RemoveInstruction(shl);
}

// Try replacing code looking like (x >>> #rdist | x <<< #ldist):
//    UShr dst, x,   #rdist
//    Shl  tmp, x,   #ldist
//    Or   dst, dst, tmp
// or like (x >>> #rdist | x << #-ldist):
//    UShr dst, x,   #rdist
//    Shl  tmp, x,   #-ldist
//    Or   dst, dst, tmp
// with
//    Ror  dst, x,   #rdist
void InstructionSimplifierArm64Visitor::TryReplaceOrWithRotateConstantPattern(HOr* orr,
                                                                              HUShr* ushr,
                                                                              HShl* shl) {
  size_t reg_bits = Primitive::ComponentSize(ushr->GetType()) * kBitsPerByte;
  size_t rdist = Int64FromConstant(ushr->GetRight()->AsConstant()) & (reg_bits - 1);
  size_t ldist = Int64FromConstant(shl->GetRight()->AsConstant()) & (reg_bits - 1);
  if (rdist == (reg_bits - ldist)) {
    ReplaceOrAndShiftsWithRor(orr, ushr, shl);
    return;
  }
}

// Try replacing code looking like (x >>> d | x <<< (#bits - d)):
//    UShr dst, x,     d
//    Sub  ld,  #bits, d
//    Shl  tmp, x,     ld
//    Or   dst, dst,   tmp
// with
//    Ror  dst, x,     d
// *** OR ***
// Replace code looking like (x >>> (#bits - d) | x <<< d):
//    Sub  rd,  #bits, d
//    UShr dst, x,     rd
//    Shl  tmp, x,     d
//    Or   dst, dst,   tmp
// with
//    Neg  neg, d
//    Ror  dst, x,     neg
void InstructionSimplifierArm64Visitor::TryReplaceOrWithRotateRegisterSubPattern(HOr* orr,
                                                                                 HUShr* ushr,
                                                                                 HShl* shl) {
  DCHECK(ushr->GetRight()->IsSub() || shl->GetRight()->IsSub());
  bool sub_is_left = shl->GetRight()->IsSub();
  HSub* sub = sub_is_left ? shl->GetRight()->AsSub() : ushr->GetRight()->AsSub();
  size_t reg_bits = Primitive::ComponentSize(ushr->GetType()) * kBitsPerByte;
  // If we are the only user of sub.
  // And the shift distance being sub'd from reg_bits is the distance being
  // shifted the other way.
  // And the shift distance is being sub'd from a multiple of reg_bits.
  if (sub->HasOnlyOneNonEnvironmentUse() &&
      sub->GetLeft()->IsConstant() &&
      sub->GetRight() == (sub_is_left ? ushr->GetRight() : shl->GetRight()) &&
      (Int64FromConstant(sub->GetLeft()->AsConstant()) & (reg_bits - 1)) == 0) {
    if (sub_is_left) {
      ReplaceOrAndShiftsWithRor(orr, ushr, shl);
      sub->GetBlock()->RemoveInstruction(sub);
    } else {
      HNeg* neg = new (GetGraph()->GetArena()) HNeg(shl->GetRight()->GetType(), shl->GetRight());
      orr->GetBlock()->InsertInstructionBefore(neg, orr);
      HArm64Ror* ror = new (GetGraph()->GetArena()) HArm64Ror(ushr->GetType(), shl->GetLeft(), neg);
      orr->GetBlock()->ReplaceAndRemoveInstructionWith(orr, ror);
      ushr->GetBlock()->RemoveInstruction(ushr);
      sub->GetBlock()->RemoveInstruction(sub);
      shl->GetBlock()->RemoveInstruction(shl);
    }
  }
}

// Replace code looking like (x >>> -d | x << d):
//    Neg  neg, d
//    UShr dst, x,   neg
//    Shl  tmp, x,   d
//    Or   dst, dst, tmp
// with
//    Neg  neg, d
//    Ror  dst, x,   neg
// *** OR ***
// Replace code looking like (x >>> d | x << -d):
//    UShr dst, x,   d
//    Neg  neg, d
//    Shl  tmp, x,   neg
//    Or   dst, dst, tmp
// with
//    Ror  dst, x,   d
void InstructionSimplifierArm64Visitor::TryReplaceOrWithRotateRegisterNegPattern(HOr* orr,
                                                                                 HUShr* ushr,
                                                                                 HShl* shl) {
  DCHECK(ushr->GetRight()->IsNeg() || shl->GetRight()->IsNeg());
  bool neg_is_left = shl->GetRight()->IsNeg();
  HNeg* neg = neg_is_left ? shl->GetRight()->AsNeg() : ushr->GetRight()->AsNeg();
  // If we are the only user of neg.
  // And the shift distance being negated is the distance being shifted the other way.
  if (neg->HasOnlyOneNonEnvironmentUse() &&
      neg->InputAt(0) == (neg_is_left ? ushr->GetRight() : shl->GetRight())) {
    if (neg_is_left) {
      ReplaceOrAndShiftsWithRor(orr, ushr, shl);
      neg->GetBlock()->RemoveInstruction(neg);
    } else {
      HArm64Ror* ror = new (GetGraph()->GetArena()) HArm64Ror(ushr->GetType(), ushr->GetLeft(), neg);
      orr->GetBlock()->ReplaceAndRemoveInstructionWith(orr, ror);
      ushr->GetBlock()->RemoveInstruction(ushr);
      shl->GetBlock()->RemoveInstruction(shl);
    }
  }
}

// Try to replace an Or flanked by one UShr and one Shl with a bitfield rotation.
// TODO: Move to platform agnostic instruction_simplifier.cc once proven on ARM64.
void InstructionSimplifierArm64Visitor::TryReplaceOrWithRotate(HOr* instruction) {
  HInstruction* left = instruction->GetLeft();
  HInstruction* right = instruction->GetRight();
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
        // add up to the register size..
        TryReplaceOrWithRotateConstantPattern(instruction, ushr, shl);
      } else if (ushr->GetRight()->IsSub() || shl->GetRight()->IsSub()) {
        // Shift distances are potentially of the form x and (reg_size - x)..
        TryReplaceOrWithRotateRegisterSubPattern(instruction, ushr, shl);
      } else if (ushr->GetRight()->IsNeg() || shl->GetRight()->IsNeg()) {
        // Shift distances are potentially of the form d and -d..
        TryReplaceOrWithRotateRegisterNegPattern(instruction, ushr, shl);
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
  TryReplaceOrWithRotate(instruction);
}

}  // namespace arm64
}  // namespace art
