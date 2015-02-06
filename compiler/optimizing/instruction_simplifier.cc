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

namespace art {

class InstructionSimplifierVisitor : public HGraphVisitor {
 public:
  explicit InstructionSimplifierVisitor(HGraph* graph) : HGraphVisitor(graph) {}

 private:
  // These helpers return true if they managed to optimize the operation.
  bool TrySimplifyOpWithIdentityElement(HBinaryOperation* bin_op);
  bool TrySimplifyOpWithAbsorbingElement(HBinaryOperation* bin_op);
  bool TryReplaceByZeroIfLeftIsZero(HBinaryOperation* instruction);

  void VisitSuspendCheck(HSuspendCheck* check) OVERRIDE;
  void VisitEqual(HEqual* equal) OVERRIDE;
  void VisitArraySet(HArraySet* equal) OVERRIDE;
  void VisitTypeConversion(HTypeConversion* instruction) OVERRIDE;
  void VisitNullCheck(HNullCheck* instruction) OVERRIDE;
  void VisitAdd(HAdd* instruction) OVERRIDE;
  void VisitAnd(HAnd* instruction) OVERRIDE;
  void VisitDiv(HDiv* instruction) OVERRIDE;
  void VisitMul(HMul* instruction) OVERRIDE;
  void VisitOr(HOr* instruction) OVERRIDE;
  void VisitShl(HShl* instruction) OVERRIDE;
  void VisitShr(HShr* instruction) OVERRIDE;
  void VisitSub(HSub* instruction) OVERRIDE;
  void VisitUShr(HUShr* instruction) OVERRIDE;
  void VisitXor(HXor* instruction) OVERRIDE;
};

void InstructionSimplifier::Run() {
  InstructionSimplifierVisitor visitor(graph_);
  visitor.VisitInsertionOrder();
}

bool InstructionSimplifierVisitor::TrySimplifyOpWithIdentityElement(HBinaryOperation* instruction) {
  HConstant* input_cst = instruction->GetConstantRight();
  HInstruction* input_other = instruction->GetLeastConstantLeft();

  if ((input_cst != nullptr) && instruction->IsIdentityElement(input_cst)) {
    instruction->ReplaceWith(input_other);
    instruction->GetBlock()->RemoveInstruction(instruction);
    return true;
  }
  return false;
}

bool InstructionSimplifierVisitor::TrySimplifyOpWithAbsorbingElement(HBinaryOperation* instruction) {
  HConstant* input_cst = instruction->GetConstantRight();

  if ((input_cst != nullptr) && instruction->IsAbsorbingElement(input_cst)) {
    instruction->ReplaceWith(input_cst);
    instruction->GetBlock()->RemoveInstruction(instruction);
    return true;
  }
  return false;
}

bool InstructionSimplifierVisitor::TryReplaceByZeroIfLeftIsZero(HBinaryOperation* instruction) {
  HInstruction* left = instruction->GetLeft();
  if (left->IsIntConstant() || left->IsLongConstant()) {
    int64_t left_val = left->IsIntConstant() ? left->AsIntConstant()->GetValue()
                                             : left->AsLongConstant()->GetValue();
    if (left_val == 0) {
      instruction->ReplaceWith(left);
      instruction->GetBlock()->RemoveInstruction(instruction);
      return true;
    }
  }
  return false;
}

void InstructionSimplifierVisitor::VisitNullCheck(HNullCheck* null_check) {
  HInstruction* obj = null_check->InputAt(0);
  if (!obj->CanBeNull()) {
    null_check->ReplaceWith(obj);
    null_check->GetBlock()->RemoveInstruction(null_check);
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
  TrySimplifyOpWithIdentityElement(instruction);
}

void InstructionSimplifierVisitor::VisitAnd(HAnd* instruction) {
  if (TrySimplifyOpWithIdentityElement(instruction)) {
    return;
  } else if (TrySimplifyOpWithAbsorbingElement(instruction)) {
    return;
  } else if (instruction->GetLeft() == instruction->GetRight()) {
    instruction->ReplaceWith(instruction->GetLeft());
    instruction->GetBlock()->RemoveInstruction(instruction);
  }
}

void InstructionSimplifierVisitor::VisitDiv(HDiv* instruction) {
  TrySimplifyOpWithIdentityElement(instruction);
}

void InstructionSimplifierVisitor::VisitMul(HMul* instruction) {
  if (TrySimplifyOpWithIdentityElement(instruction)) {
    return;
  } else if (TrySimplifyOpWithAbsorbingElement(instruction)) {
    return;
  }

  HConstant* input_cst = instruction->GetConstantRight();
  HInstruction* input_other = instruction->GetLeastConstantLeft();

  if (input_cst == nullptr) {
    return;
  }

  Primitive::Type type = instruction->GetType();
  HBasicBlock* block = instruction->GetBlock();
  ArenaAllocator* allocator = GetGraph()->GetArena();

  if (input_cst->IsMinusOne() &&
      (Primitive::IsFloatingPointType(type) ||
       type == Primitive::kPrimInt || type == Primitive::kPrimLong)) {
    // Transform code looking like
    //    MUL dst, src, -1
    // into
    //    NEG dst, src
    HNeg* neg = new (allocator) HNeg(type, input_other);
    block->ReplaceAndRemoveInstructionWith(instruction, neg);
    return;
  }

  if (Primitive::IsFloatingPointType(type)) {
    if ((input_cst->IsFloatConstant() && input_cst->AsFloatConstant()->GetValue() == 2.0) ||
        (input_cst->IsDoubleConstant() && input_cst->AsDoubleConstant()->GetValue() == 2.0)) {
      // Transform code looking like
      //    FP_MUL dst, src, 2.0
      // into
      //    FP_ADD dst, src, src
      // The 'int' and 'long' cases are handled below.
      block->ReplaceAndRemoveInstructionWith(instruction,
                                             new (allocator) HAdd(type, input_other, input_other));
    }

  } else if ((type == Primitive::kPrimInt) || (type == Primitive::kPrimLong)) {
    int64_t factor = input_cst->IsIntConstant() ? input_cst->AsIntConstant()->GetValue()
                                                : input_cst->AsLongConstant()->GetValue();
    // We expect the `0` case to have been handled by the 'absorbing' element helper.
    DCHECK_NE(factor, 0);
    if (IsPowerOfTwo(factor)) {
      // Transform code looking like
      //    MUL dst, src, pow_of_2
      // into
      //    SHL dst, src, log2(pow_of_2)
      HIntConstant* shift = new (allocator) HIntConstant(WhichPowerOf2(factor));
      block->InsertInstructionBefore(shift, instruction);
      HShl* shl = new(allocator) HShl(type, input_other, shift);
      block->ReplaceAndRemoveInstructionWith(instruction, shl);
    }
  }
}

void InstructionSimplifierVisitor::VisitOr(HOr* instruction) {
  if (TrySimplifyOpWithIdentityElement(instruction)) {
    return;
  } else if (TrySimplifyOpWithAbsorbingElement(instruction)) {
    return;
  } else if (instruction->GetLeft() == instruction->GetRight()) {
    instruction->ReplaceWith(instruction->GetLeft());
    instruction->GetBlock()->RemoveInstruction(instruction);
  }
}

void InstructionSimplifierVisitor::VisitShl(HShl* instruction) {
  if (TrySimplifyOpWithIdentityElement(instruction)) {
    return;
  } else {
    TryReplaceByZeroIfLeftIsZero(instruction);
  }
}

void InstructionSimplifierVisitor::VisitShr(HShr* instruction) {
  if (TrySimplifyOpWithIdentityElement(instruction)) {
    return;
  } else {
    TryReplaceByZeroIfLeftIsZero(instruction);
  }
}

void InstructionSimplifierVisitor::VisitSub(HSub* instruction) {
  if (TrySimplifyOpWithIdentityElement(instruction)) {
    return;
  }

  Primitive::Type type = instruction->GetType();
  if (Primitive::IsIntegralType(type)) {
    HBasicBlock* block = instruction->GetBlock();
    ArenaAllocator* allocator = GetGraph()->GetArena();

    if (instruction->GetLeft() == instruction->GetRight()) {
      // Transform code looking like
      //    SUB dst, src, src
      // into
      //    CONSTANT 0
      // Note that we cannot optimise `x - x` to `0` for floating-point. It does
      // not work when `x` is an infinity.
      HConstant* constant;
      if (type == Primitive::kPrimInt) {
        constant = new (allocator) HIntConstant(0);
      } else {
        constant = new (allocator) HLongConstant(0);
      }

      block->ReplaceAndRemoveInstructionWith(instruction, constant);

    } else if (instruction->GetLeft()->IsConstant()) {
      // Transform code looking like
      //    SUB dst, 0, src
      // into
      //    NEG dst, src
      // Note that we cannot optimise `0.0 - x` to `-x` for floating-point. When
      // `x` is `0.0`, the two expressions respectively yield `0.0` and `-0.0`.
      HInstruction* constant = instruction->GetLeft();
      int64_t left = constant->IsIntConstant() ? constant->AsIntConstant()->GetValue()
                                               : constant->AsLongConstant()->GetValue();
      if (left == 0) {
        HNeg* neg = new (allocator) HNeg(type, instruction->GetRight());
        block->ReplaceAndRemoveInstructionWith(instruction, neg);
      }
    }
  }
}

void InstructionSimplifierVisitor::VisitUShr(HUShr* instruction) {
  if (TrySimplifyOpWithIdentityElement(instruction)) {
    return;
  } else {
    TryReplaceByZeroIfLeftIsZero(instruction);
  }
}

void InstructionSimplifierVisitor::VisitXor(HXor* instruction) {
  if (TrySimplifyOpWithIdentityElement(instruction)) {
    return;
  }

  if (instruction->GetLeft() == instruction->GetRight()) {
    // Transform code looking like
    //    XOR dst, src, src
    // into
    //    CONSTANT 0
    Primitive::Type type = instruction->GetType();
    DCHECK(type == Primitive::kPrimInt || type == Primitive::kPrimLong);
    ArenaAllocator* allocator = GetGraph()->GetArena();
    HConstant* constant;
    if (type == Primitive::kPrimInt) {
      constant = new (allocator) HIntConstant(0);
    } else {
      constant = new (allocator) HLongConstant(0);
    }
    instruction->GetBlock()->ReplaceAndRemoveInstructionWith(instruction, constant);
    return;
  }

  HConstant* input_cst = instruction->GetConstantRight();

  if ((input_cst != nullptr) && input_cst->IsAllOnes()) {
    // Transform code looking like
    //    XOR dst, src, 0xFFF...FF
    // into
    //    NOT dst, src
    HInstruction* input_other = instruction->GetLeastConstantLeft();
    HNot* bitwise_not = new (GetGraph()->GetArena()) HNot(instruction->GetType(), input_other);
    instruction->GetBlock()->ReplaceAndRemoveInstructionWith(instruction, bitwise_not);
    return;
  }
}


}  // namespace art
