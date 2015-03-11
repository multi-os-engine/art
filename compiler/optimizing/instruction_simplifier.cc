/*
 * Copyright (C) 2014 The Android Open Source Project
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

#include "instruction_simplifier.h"

#include "mirror/class-inl.h"
#include "scoped_thread_state_change.h"

namespace art {

class InstructionSimplifierVisitor : public HGraphVisitor {
 public:
  InstructionSimplifierVisitor(HGraph* graph, OptimizingCompilerStats* stats)
      : HGraphVisitor(graph), stats_(stats), revisit_current_(false) {}

  bool revisit_current() const { return revisit_current_; }
  void SetRevisitCurrent(bool revisit) { revisit_current_ = revisit; }

 private:
  bool TryMoveNegOnInputsAfterBinop(HBinaryOperation* binop);
  void VisitShift(HBinaryOperation* shift);

  void VisitSuspendCheck(HSuspendCheck* check) OVERRIDE;
  void VisitEqual(HEqual* equal) OVERRIDE;
  void VisitArraySet(HArraySet* equal) OVERRIDE;
  void VisitTypeConversion(HTypeConversion* instruction) OVERRIDE;
  void VisitNullCheck(HNullCheck* instruction) OVERRIDE;
  void VisitArrayLength(HArrayLength* instruction) OVERRIDE;
  void VisitCheckCast(HCheckCast* instruction) OVERRIDE;
  void VisitAdd(HAdd* instruction) OVERRIDE;
  void VisitAnd(HAnd* instruction) OVERRIDE;
  void VisitDiv(HDiv* instruction) OVERRIDE;
  void VisitMul(HMul* instruction) OVERRIDE;
  void VisitNeg(HNeg* instruction) OVERRIDE;
  void VisitNot(HNot* instruction) OVERRIDE;
  void VisitOr(HOr* instruction) OVERRIDE;
  void VisitShl(HShl* instruction) OVERRIDE;
  void VisitShr(HShr* instruction) OVERRIDE;
  void VisitSub(HSub* instruction) OVERRIDE;
  void VisitUShr(HUShr* instruction) OVERRIDE;
  void VisitXor(HXor* instruction) OVERRIDE;

  OptimizingCompilerStats* stats_;
  bool revisit_current_;
};

void InstructionSimplifier::Run() {
  InstructionSimplifierVisitor visitor(graph_, stats_);
  for (HReversePostOrderIterator it(*graph_); !it.Done();) {
    // The simplification of an instruction to another instruction may yield
    // possibilities for other simplifications. So although we perform a reverse
    // post order visit, we sometimes need to revisit an instruction index.
    visitor.VisitBasicBlock(it.Current());
    if (visitor.revisit_current()) {
      // New simplifications may be applicable to the instruction at the
      // current index, so don't advance the iterator.
      visitor.SetRevisitCurrent(false);
    } else {
      it.Advance();
    }
  }
}

namespace {

bool AreAllBitsSet(HConstant* constant) {
  return Int64FromConstant(constant) == -1;
}

}  // namespace

// Returns true if the code was simplified to use only one negation operation
// after the binary operation instead of one on each of the inputs.
bool InstructionSimplifierVisitor::TryMoveNegOnInputsAfterBinop(HBinaryOperation* binop) {
  DCHECK(binop->IsAdd() || binop->IsSub());
  DCHECK(binop->GetLeft()->IsNeg() && binop->GetRight()->IsNeg());
  HNeg* left_neg = binop->GetLeft()->AsNeg();
  HNeg* right_neg = binop->GetRight()->AsNeg();
  if (!left_neg->GetUses().HasOnlyOneUse() || left_neg->HasEnvironmentUses() ||
      !right_neg->GetUses().HasOnlyOneUse() || right_neg->HasEnvironmentUses()) {
    return false;
  }
  // Replace code looking like
  //    NEG tmp1, a
  //    NEG tmp2, b
  //    ADD dst, tmp1, tmp2
  // with
  //    ADD tmp, a, b
  //    NEG dst, tmp
  binop->ReplaceInput(left_neg->GetInput(), 0);
  binop->ReplaceInput(right_neg->GetInput(), 1);
  left_neg->GetBlock()->RemoveInstruction(left_neg);
  right_neg->GetBlock()->RemoveInstruction(right_neg);
  HNeg* neg = new (GetGraph()->GetArena()) HNeg(binop->GetType(), binop);
  binop->GetBlock()->InsertInstructionBefore(neg, binop->GetNext());
  // This also replaces the input of `neg` by `neg`, which is of course not what
  // we want, so we manually fix the input.
  binop->ReplaceWith(neg);
  neg->ReplaceInput(binop, 0);
  SetRevisitCurrent(true);
  return true;
}

void InstructionSimplifierVisitor::VisitShift(HBinaryOperation* instruction) {
  DCHECK(instruction->IsShl() || instruction->IsShr() || instruction->IsUShr());
  HConstant* input_cst = instruction->GetConstantRight();
  HInstruction* input_other = instruction->GetLeastConstantLeft();

  if ((input_cst != nullptr) && input_cst->IsZero()) {
    // Replace code looking like
    //    SHL dst, src, 0
    // with
    //    src
    instruction->ReplaceWith(input_other);
    instruction->GetBlock()->RemoveInstruction(instruction);
  }
}

void InstructionSimplifierVisitor::VisitNullCheck(HNullCheck* null_check) {
  HInstruction* obj = null_check->InputAt(0);
  if (!obj->CanBeNull()) {
    null_check->ReplaceWith(obj);
    null_check->GetBlock()->RemoveInstruction(null_check);
    if (stats_ != nullptr) {
      stats_->RecordStat(MethodCompilationStat::kRemovedNullCheck);
    }
  }
}

void InstructionSimplifierVisitor::VisitCheckCast(HCheckCast* check_cast) {
  HLoadClass* load_class = check_cast->InputAt(1)->AsLoadClass();
  if (!load_class->IsResolved()) {
    // If the class couldn't be resolve it's not safe to compare against it. It's
    // default type would be Top which might be wider that the actual class type
    // and thus producing wrong results.
    return;
  }
  ReferenceTypeInfo obj_rti = check_cast->InputAt(0)->GetReferenceTypeInfo();
  ReferenceTypeInfo class_rti = load_class->GetLoadedClassRTI();
  ScopedObjectAccess soa(Thread::Current());
  if (class_rti.IsSupertypeOf(obj_rti)) {
    check_cast->GetBlock()->RemoveInstruction(check_cast);
    if (stats_ != nullptr) {
      stats_->RecordStat(MethodCompilationStat::kRemovedCheckedCast);
    }
  }
}

void InstructionSimplifierVisitor::VisitSuspendCheck(HSuspendCheck* check) {
  HBasicBlock* block = check->GetBlock();
  // Currently always keep the suspend check at entry.
  if (block->IsEntryBlock()) return;

  // Currently always keep suspend checks at loop entry.
  if (block->IsLoopHeader() && block->GetFirstInstruction() == check) {
    DCHECK(block->GetLoopInformation()->GetSuspendCheck() == check);
    return;
  }

  // Remove the suspend check that was added at build time for the baseline
  // compiler.
  block->RemoveInstruction(check);
}

void InstructionSimplifierVisitor::VisitEqual(HEqual* equal) {
  HInstruction* input1 = equal->InputAt(0);
  HInstruction* input2 = equal->InputAt(1);
  if (input1->GetType() == Primitive::kPrimBoolean && input2->IsIntConstant()) {
    if (input2->AsIntConstant()->GetValue() == 1) {
      // Replace (bool_value == 1) with bool_value
      equal->ReplaceWith(equal->InputAt(0));
      equal->GetBlock()->RemoveInstruction(equal);
    } else {
      // We should replace (bool_value == 0) with !bool_value, but we unfortunately
      // do not have such instruction.
      DCHECK_EQ(input2->AsIntConstant()->GetValue(), 0);
    }
  }
}

void InstructionSimplifierVisitor::VisitArrayLength(HArrayLength* instruction) {
  HInstruction* input = instruction->InputAt(0);
  // If the array is a NewArray with constant size, replace the array length
  // with the constant instruction. This helps the bounds check elimination phase.
  if (input->IsNewArray()) {
    input = input->InputAt(0);
    if (input->IsIntConstant()) {
      instruction->ReplaceWith(input);
    }
  }
}

void InstructionSimplifierVisitor::VisitArraySet(HArraySet* instruction) {
  HInstruction* value = instruction->GetValue();
  if (value->GetType() != Primitive::kPrimNot) return;

  if (value->IsArrayGet()) {
    if (value->AsArrayGet()->GetArray() == instruction->GetArray()) {
      // If the code is just swapping elements in the array, no need for a type check.
      instruction->ClearNeedsTypeCheck();
    }
  }
}

void InstructionSimplifierVisitor::VisitTypeConversion(HTypeConversion* instruction) {
  if (instruction->GetResultType() == instruction->GetInputType()) {
    // Remove the instruction if it's converting to the same type.
    instruction->ReplaceWith(instruction->GetInput());
    instruction->GetBlock()->RemoveInstruction(instruction);
  }
}

void InstructionSimplifierVisitor::VisitAdd(HAdd* instruction) {
  HConstant* input_cst = instruction->GetConstantRight();
  HInstruction* input_other = instruction->GetLeastConstantLeft();
  if ((input_cst != nullptr) && input_cst->IsZero()) {
    // Replace code looking like
    //    ADD dst, src, 0
    // with
    //    src
    instruction->ReplaceWith(input_other);
    instruction->GetBlock()->RemoveInstruction(instruction);
    return;
  }

  HInstruction* left = instruction->GetLeft();
  HInstruction* right = instruction->GetRight();
  HNeg* left_neg = left->AsNeg();
  HNeg* right_neg = right->AsNeg();
  bool left_is_neg = left->IsNeg();
  bool right_is_neg = right->IsNeg();

  if (left_is_neg && right_is_neg) {
    if (TryMoveNegOnInputsAfterBinop(instruction)) {
      return;
    }
  }

  HNeg* neg = left_is_neg ? left_neg : right_neg;
  if ((left_is_neg ^ right_is_neg) && !neg->HasEnvironmentUses() && neg->GetUses().HasOnlyOneUse()) {
    // Replace code looking like
    //    NEG tmp, b
    //    ADD dst, a, tmp
    // with
    //    SUB dst, a, b
    // We do not perform the optimisation if the input negation has environment
    // uses or multiple non-environment uses as it could lead to worse code. In
    // particular, we do not want the live range of `b` to be extended if we are
    // not sure the initial 'NEG' instruction can be removed.
    HInstruction* other = left_is_neg ? right : left;
    HSub* sub = new(GetGraph()->GetArena()) HSub(instruction->GetType(), other, neg->GetInput());
    instruction->GetBlock()->ReplaceAndRemoveInstructionWith(instruction, sub);
    SetRevisitCurrent(true);
    neg->GetBlock()->RemoveInstruction(neg);
  }
}

void InstructionSimplifierVisitor::VisitAnd(HAnd* instruction) {
  HConstant* input_cst = instruction->GetConstantRight();
  HInstruction* input_other = instruction->GetLeastConstantLeft();

  if ((input_cst != nullptr) && AreAllBitsSet(input_cst)) {
    // Replace code looking like
    //    AND dst, src, 0xFFF...FF
    // with
    //    src
    instruction->ReplaceWith(input_other);
    instruction->GetBlock()->RemoveInstruction(instruction);
    return;
  }

  // We assume that GVN has run before, so we only perform a pointer comparison.
  // If for some reason the values are equal but the pointers are different, we
  // are still correct and only miss an optimisation opportunity.
  if (instruction->GetLeft() == instruction->GetRight()) {
    // Replace code looking like
    //    AND dst, src, src
    // with
    //    src
    instruction->ReplaceWith(instruction->GetLeft());
    instruction->GetBlock()->RemoveInstruction(instruction);
  }
}

void InstructionSimplifierVisitor::VisitDiv(HDiv* instruction) {
  HConstant* input_cst = instruction->GetConstantRight();
  HInstruction* input_other = instruction->GetLeastConstantLeft();
  Primitive::Type type = instruction->GetType();

  if ((input_cst != nullptr) && input_cst->IsOne()) {
    // Replace code looking like
    //    DIV dst, src, 1
    // with
    //    src
    instruction->ReplaceWith(input_other);
    instruction->GetBlock()->RemoveInstruction(instruction);
    return;
  }

  if ((input_cst != nullptr) && input_cst->IsMinusOne() &&
      (Primitive::IsFloatingPointType(type) || Primitive::IsIntOrLongType(type))) {
    // Replace code looking like
    //    DIV dst, src, -1
    // with
    //    NEG dst, src
    instruction->GetBlock()->ReplaceAndRemoveInstructionWith(
        instruction, (new (GetGraph()->GetArena()) HNeg(type, input_other)));
    SetRevisitCurrent(true);
  }
}

void InstructionSimplifierVisitor::VisitMul(HMul* instruction) {
  HConstant* input_cst = instruction->GetConstantRight();
  HInstruction* input_other = instruction->GetLeastConstantLeft();
  Primitive::Type type = instruction->GetType();
  HBasicBlock* block = instruction->GetBlock();
  ArenaAllocator* allocator = GetGraph()->GetArena();

  if (input_cst == nullptr) {
    return;
  }

  if (input_cst->IsOne()) {
    // Replace code looking like
    //    MUL dst, src, 1
    // with
    //    src
    instruction->ReplaceWith(input_other);
    instruction->GetBlock()->RemoveInstruction(instruction);
    return;
  }

  if (input_cst->IsMinusOne() &&
      (Primitive::IsFloatingPointType(type) || Primitive::IsIntOrLongType(type))) {
    // Replace code looking like
    //    MUL dst, src, -1
    // with
    //    NEG dst, src
    HNeg* neg = new (allocator) HNeg(type, input_other);
    block->ReplaceAndRemoveInstructionWith(instruction, neg);
    SetRevisitCurrent(true);
    return;
  }

  if (Primitive::IsFloatingPointType(type) &&
      ((input_cst->IsFloatConstant() && input_cst->AsFloatConstant()->GetValue() == 2.0f) ||
       (input_cst->IsDoubleConstant() && input_cst->AsDoubleConstant()->GetValue() == 2.0))) {
    // Replace code looking like
    //    FP_MUL dst, src, 2.0
    // with
    //    FP_ADD dst, src, src
    // The 'int' and 'long' cases are handled below.
    block->ReplaceAndRemoveInstructionWith(instruction,
                                           new (allocator) HAdd(type, input_other, input_other));
    SetRevisitCurrent(true);
    return;
  }

  if (Primitive::IsIntOrLongType(type)) {
    int64_t factor = Int64FromConstant(input_cst);
    // We expect the `0` case to have been handled in the constant folding pass.
    DCHECK_NE(factor, 0);
    if (IsPowerOfTwo(factor)) {
      // Replace code looking like
      //    MUL dst, src, pow_of_2
      // with
      //    SHL dst, src, log2(pow_of_2)
      HIntConstant* shift = new (allocator) HIntConstant(WhichPowerOf2(factor));
      block->InsertInstructionBefore(shift, instruction);
      HShl* shl = new(allocator) HShl(type, input_other, shift);
      block->ReplaceAndRemoveInstructionWith(instruction, shl);
      SetRevisitCurrent(true);
    }
  }
}

void InstructionSimplifierVisitor::VisitNeg(HNeg* instruction) {
  HInstruction* input = instruction->GetInput();
  if (input->IsNeg()) {
    // Replace code looking like
    //    NEG tmp, src
    //    NEG dst, tmp
    // with
    //    src
    HNeg* previous_neg = input->AsNeg();
    instruction->ReplaceWith(previous_neg->GetInput());
    instruction->GetBlock()->RemoveInstruction(instruction);
    // We perform the optimisation even if the input negation has environment
    // uses since it allows removing the current instruction. But we only delete
    // the input negation only if it is does not have any uses left.
    if (!previous_neg->HasUses()) {
      previous_neg->GetBlock()->RemoveInstruction(previous_neg);
    }
    return;
  }

  if (input->IsSub() && !input->HasEnvironmentUses() && input->GetUses().HasOnlyOneUse()) {
    // Replace code looking like
    //    SUB tmp, a, b
    //    NEG dst, tmp
    // with
    //    SUB dst, b, a
    // We do not perform the optimisation if the input subtraction has
    // environment uses or multiple non-environment uses as it could lead to
    // worse code. In particular, we do not want the live ranges of `a` and `b`
    // to be extended if we are not sure the initial 'SUB' instruction can be
    // removed.
    HSub* sub = input->AsSub();
    HInstruction* left = sub->GetRight();
    HInstruction* right = sub->GetLeft();
    HSub* new_sub = new (GetGraph()->GetArena()) HSub(instruction->GetType(), left, right);
    instruction->GetBlock()->ReplaceAndRemoveInstructionWith(instruction, new_sub);
    if (!sub->HasUses()) {
      sub->GetBlock()->RemoveInstruction(sub);
    }
  }
}

void InstructionSimplifierVisitor::VisitNot(HNot* instruction) {
  HInstruction* input = instruction->GetInput();
  if (input->IsNot()) {
    // Replace code looking like
    //    NOT tmp, src
    //    NOT dst, tmp
    // with
    //    src
    // We perform the optimisation even if the input negation has environment
    // uses since it allows removing the current instruction. But we only delete
    // the input negation only if it is does not have any uses left.
    HNot* previous_not = input->AsNot();
    instruction->ReplaceWith(previous_not->GetInput());
    instruction->GetBlock()->RemoveInstruction(instruction);
    if (!previous_not->HasUses()) {
      previous_not->GetBlock()->RemoveInstruction(previous_not);
    }
  }
}

void InstructionSimplifierVisitor::VisitOr(HOr* instruction) {
  HConstant* input_cst = instruction->GetConstantRight();
  HInstruction* input_other = instruction->GetLeastConstantLeft();

  if ((input_cst != nullptr) && input_cst->IsZero()) {
    // Replace code looking like
    //    OR dst, src, 0
    // with
    //    src
    instruction->ReplaceWith(input_other);
    instruction->GetBlock()->RemoveInstruction(instruction);
    return;
  }

  // We assume that GVN has run before, so we only perform a pointer comparison.
  // If for some reason the values are equal but the pointers are different, we
  // are still correct and only miss an optimisation opportunity.
  if (instruction->GetLeft() == instruction->GetRight()) {
    // Replace code looking like
    //    OR dst, src, src
    // with
    //    src
    instruction->ReplaceWith(instruction->GetLeft());
    instruction->GetBlock()->RemoveInstruction(instruction);
  }
}

void InstructionSimplifierVisitor::VisitShl(HShl* instruction) {
  VisitShift(instruction);
}

void InstructionSimplifierVisitor::VisitShr(HShr* instruction) {
  VisitShift(instruction);
}

void InstructionSimplifierVisitor::VisitSub(HSub* instruction) {
  Primitive::Type type = instruction->GetType();
  HConstant* input_cst = instruction->GetConstantRight();
  HInstruction* input_other = instruction->GetLeastConstantLeft();

  if ((input_cst != nullptr) && input_cst->IsZero()) {
    // Replace code looking like
    //    SUB dst, src, 0
    // with
    //    src
    instruction->ReplaceWith(input_other);
    instruction->GetBlock()->RemoveInstruction(instruction);
    return;
  }

  if (!Primitive::IsIntegralType(type)) {
    return;
  }

  HBasicBlock* block = instruction->GetBlock();
  ArenaAllocator* allocator = GetGraph()->GetArena();

  HInstruction* left = instruction->GetLeft();
  HInstruction* right = instruction->GetRight();
  if (left->IsConstant()) {
    if (Int64FromConstant(left->AsConstant()) == 0) {
      // Replace code looking like
      //    SUB dst, 0, src
      // with
      //    NEG dst, src
      // Note that we cannot optimise `0.0 - x` to `-x` for floating-point. When
      // `x` is `0.0`, the former expression yields `0.0`, while the later
      // yields `-0.0`.
      HNeg* neg = new (allocator) HNeg(type, right);
      block->ReplaceAndRemoveInstructionWith(instruction, neg);
      SetRevisitCurrent(true);
      return;
    }
  }

  if (left->IsNeg() && right->IsNeg()) {
    if (TryMoveNegOnInputsAfterBinop(instruction)) {
      return;
    }
  }

  if (right->IsNeg() && !right->HasEnvironmentUses() && right->GetUses().HasOnlyOneUse()) {
    // Replace code looking like
    //    NEG tmp, b
    //    SUB dst, a, tmp
    // with
    //    ADD dst, a, b
    HAdd* add = new(GetGraph()->GetArena()) HAdd(type, left, right->AsNeg()->GetInput());
    instruction->GetBlock()->ReplaceAndRemoveInstructionWith(instruction, add);
    SetRevisitCurrent(true);
    right->GetBlock()->RemoveInstruction(right);
    return;
  }

  if (left->IsNeg() && !left->HasEnvironmentUses() && left->GetUses().HasOnlyOneUse()) {
    // Replace code looking like
    //    NEG tmp, a
    //    SUB dst, tmp, b
    // with
    //    ADD tmp, a, b
    //    NEG dst, tmp
    // The second version is not intrinsically better, but enables more
    // transformations.

    HAdd* add = new(GetGraph()->GetArena()) HAdd(type, left->AsNeg()->GetInput(), right);
    instruction->GetBlock()->InsertInstructionBefore(add, instruction);
    HNeg* neg = new (GetGraph()->GetArena()) HNeg(instruction->GetType(), add);
    instruction->GetBlock()->InsertInstructionBefore(neg, instruction);
    instruction->ReplaceWith(neg);
    instruction->GetBlock()->RemoveInstruction(instruction);
    SetRevisitCurrent(true);
    left->GetBlock()->RemoveInstruction(left);
  }
}

void InstructionSimplifierVisitor::VisitUShr(HUShr* instruction) {
  VisitShift(instruction);
}

void InstructionSimplifierVisitor::VisitXor(HXor* instruction) {
  HConstant* input_cst = instruction->GetConstantRight();
  HInstruction* input_other = instruction->GetLeastConstantLeft();

  if ((input_cst != nullptr) && input_cst->IsZero()) {
    // Replace code looking like
    //    XOR dst, src, 0
    // with
    //    src
    instruction->ReplaceWith(input_other);
    instruction->GetBlock()->RemoveInstruction(instruction);
    return;
  }

  if ((input_cst != nullptr) && AreAllBitsSet(input_cst)) {
    // Replace code looking like
    //    XOR dst, src, 0xFFF...FF
    // with
    //    NOT dst, src
    HNot* bitwise_not = new (GetGraph()->GetArena()) HNot(instruction->GetType(), input_other);
    instruction->GetBlock()->ReplaceAndRemoveInstructionWith(instruction, bitwise_not);
    SetRevisitCurrent(true);
    return;
  }
}

}  // namespace art
