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

#ifndef ART_COMPILER_OPTIMIZING_NODES_ARM64_H_
#define ART_COMPILER_OPTIMIZING_NODES_ARM64_H_

#include "nodes_common.h"

namespace art {

class HArm64ArithWithOp : public HExpression<2> {
 public:
  enum OpKind {
    kInvalidOp,
    kLSL,   // Logical shift left.
    kLSR,   // Logical shift right.
    kASR,   // Arithmetic shift right.
    kUXTB,  // Unsigned extend byte.
    kUXTH,  // Unsigned extend half-word.
    kUXTW,  // Unsigned extend word.
    kSXTB,  // Signed extend byte.
    kSXTH,  // Signed extend half-word.
    kSXTW,  // Signed extend word.

    // Aliases.
    kFirstShiftOp = kLSL,
    kLastShiftOp = kASR,
    kFirstExtensionOp = kUXTB,
    kLastExtensionOp = kSXTW
  };
  HArm64ArithWithOp(HInstruction* instr,
                    HInstruction* left,
                    HInstruction* right,
                    OpKind op,
                    int shift = 0)
      : HExpression(instr->GetType(), instr->GetSideEffects()),
        instr_kind_(instr->GetKind()), op_kind_(op), shift_amount_(shift) {
    SetRawInputAt(0, left);
    SetRawInputAt(1, right);
  }

  virtual bool CanBeMoved() const { return true; }
  virtual bool InstructionDataEquals(HInstruction* other_instr) const {
    HArm64ArithWithOp* other = other_instr->AsArm64ArithWithOp();
    return instr_kind_ == other->instr_kind_ &&
        op_kind_ == other->op_kind_ &&
        shift_amount_ == other->shift_amount_;
  }

  static bool IsShiftOp(OpKind op_kind) {
    return kFirstShiftOp <= op_kind && op_kind <= kLastShiftOp;
  }

  static bool IsExtensionOp(OpKind op_kind) {
    return kFirstExtensionOp <= op_kind && op_kind <= kLastExtensionOp;
  }

  // Find the operation kind and shift amount from a bitfield move instruction.
  static void GetOpInfoFromEncoding(HArm64BitfieldMove* xbfm,
                                    OpKind* op_kind, int* shift_amount);

  InstructionKind instr_kind() const { return instr_kind_; }
  OpKind op_kind() const { return op_kind_; }
  int shift_amount() const { return shift_amount_; }
  static const char* OpKindDesc(OpKind op) {
    switch (op) {
      case kLSL:  return "LSL";
      case kLSR:  return "LSR";
      case kASR:  return "ASR";
      case kUXTB: return "UXTB";
      case kUXTH: return "UXTH";
      case kUXTW: return "UXTW";
      case kSXTB: return "SXTB";
      case kSXTH: return "SXTH";
      case kSXTW: return "SXTW";
      default:
        LOG(FATAL) << "Unexpected op kind";
        return nullptr;
    }
  }

  DECLARE_INSTRUCTION(Arm64ArithWithOp);

 private:
  InstructionKind instr_kind_;
  OpKind op_kind_;
  int shift_amount_;

  DISALLOW_COPY_AND_ASSIGN(HArm64ArithWithOp);
};

class HArm64ArrayAccessAddress : public HExpression<2> {
 public:
  explicit HArm64ArrayAccessAddress(HArrayGet* array_get)
      : HExpression(Primitive::kPrimNot, SideEffects::DependsOnSomething()),
        component_type_(array_get->GetType()) {
    SetRawInputAt(0, array_get->GetArray());
    SetRawInputAt(1, array_get->GetIndex());
  }
  explicit HArm64ArrayAccessAddress(HArraySet* array_set)
      : HExpression(Primitive::kPrimNot, SideEffects::DependsOnSomething()),
        component_type_(array_set->GetComponentType()) {
    SetRawInputAt(0, array_set->GetArray());
    SetRawInputAt(1, array_set->GetIndex());
  }

  bool CanBeMoved() const OVERRIDE { return true; }
  bool InstructionDataEquals(HInstruction* other ATTRIBUTE_UNUSED) const OVERRIDE {
    return component_type_ == other->AsArm64ArrayAccessAddress()->component_type_;
  }

  HInstruction* GetArray() const { return InputAt(0); }
  HInstruction* GetIndex() const { return InputAt(1); }

  Primitive::Type GetComponentType() const { return component_type_; }

  DECLARE_INSTRUCTION(Arm64ArrayAccessAddress);

 private:
  Primitive::Type component_type_;

  DISALLOW_COPY_AND_ASSIGN(HArm64ArrayAccessAddress);
};

// This instruction covers arm64 instructions BFM, SBFM, UBFM, and their
// aliases. The properties and fields follow naming from the ARM architecture
// reference manual.
class HArm64BitfieldMove : public HExpression<1> {
 public:
  enum BitfieldMoveType {
    kSBFM = 0,
    kBFM = 1,
    kUBFM = 2,
    kUnallocated = 3
  };

  explicit HArm64BitfieldMove(HInstruction* shift)
      : HExpression(shift->GetType(), shift->GetSideEffects()) {
        SetRawInputAt(0, shift->InputAt(0));
        DCHECK(shift->IsShl() || shift->IsShr() || shift->IsUShr());

        bool is64bit = Requires64bitOperation();
        int shift_mask =  is64bit ? kMaxLongShiftValue : kMaxIntShiftValue;
        int nbits = is64bit ? 64 : 32;

        if (shift->IsShl()) {
          HShl* shl = shift->AsShl();
          int shift_amount = shl->InputAt(1)->AsIntConstant()->GetValue() & shift_mask;
          if (shift_amount < 0) {
            shift_amount += nbits;
          }
          bitfield_move_type_ = kUBFM;
          immr_ = -shift_amount % nbits;
          if (immr_ < 0) {
            immr_ += nbits;
          }
          imms_ = nbits - 1 - shift_amount;
        } else {
          if (shift->IsShr()) {
            bitfield_move_type_ = kSBFM;
            immr_ = shift->AsShr()->InputAt(1)->AsIntConstant()->GetValue() & shift_mask;
          } else {
            bitfield_move_type_ = kUBFM;
            immr_ = shift->AsUShr()->InputAt(1)->AsIntConstant()->GetValue() & shift_mask;
          }
          if (immr_ < 0) {
            immr_ += nbits;
          }
          imms_ = nbits - 1;
        }
  }

  explicit HArm64BitfieldMove(HTypeConversion* conversion)
      : HExpression(conversion->GetType(), conversion->GetSideEffects()) {
    Primitive::Type result_type = conversion->GetResultType();
    Primitive::Type input_type = conversion->GetInputType();
    size_t result_size = Primitive::ComponentSize(result_type);
    size_t input_size = Primitive::ComponentSize(input_type);
    size_t min_size = std::min(result_size, input_size);
    DCHECK_NE(input_type, result_type);

    SetRawInputAt(0, conversion->GetInput());
    immr_ = 0;
    if ((result_type == Primitive::kPrimChar) && (input_size < result_size)) {
      bitfield_move_type_ = kUBFM;
      imms_ = result_size * kBitsPerByte - 1;
    } else if ((result_type == Primitive::kPrimChar) ||
               ((input_type == Primitive::kPrimChar) && (result_size > input_size))) {
      bitfield_move_type_ = kUBFM;
      imms_ = min_size * kBitsPerByte - 1;
    } else {
      bitfield_move_type_ = kSBFM;
      imms_ = min_size * kBitsPerByte - 1;
    }
  }

  bool Requires64bitOperation() const {
    return GetType() == Primitive::kPrimLong || InputAt(0)->GetType() == Primitive::kPrimLong;
  }

  bool CanBeMoved() const OVERRIDE { return true; }
  bool InstructionDataEquals(HInstruction* other) const OVERRIDE {
    HArm64BitfieldMove* other_bfm = other->AsArm64BitfieldMove();
    return (bitfield_move_type_ == other_bfm->bitfield_move_type_) &&
        (immr_ == other_bfm->immr_) && (imms_ == other_bfm->imms_);
  }

  static const char* BitfieldMoveTypeDesc(BitfieldMoveType type) {
    switch (type) {
      case kSBFM:  return "SBFM";
      case kBFM:  return "BFM";
      case kUBFM:  return "UBFM";
      default:
        LOG(FATAL) << "Unexpected bitfield move type";
        return nullptr;
    }
  }

  BitfieldMoveType bitfield_move_type() const { return bitfield_move_type_; }
  int immr() const { return immr_; }
  int imms() const { return imms_; }

  DECLARE_INSTRUCTION(Arm64BitfieldMove);

 private:
  BitfieldMoveType bitfield_move_type_;
  int immr_;
  int imms_;

  DISALLOW_COPY_AND_ASSIGN(HArm64BitfieldMove);
};

class HArm64ConditionalSelect : public HTemplateInstruction<5> {
 public:
  HArm64ConditionalSelect(HIf* instr_if, HPhi* phi)
      : HTemplateInstruction<5>(SideEffects::None()),
        input_condition_(nullptr),
        type_(phi->GetType()) {
    size_t true_predecessor_index =
        (phi->GetBlock()->GetPredecessors().Get(0) == instr_if->IfTrueSuccessor()) ? 0 : 1;
    SetRawInputAt(kInputConditionIndex, instr_if->InputAt(0));
    SetRawInputAt(kInputTrueResIndex, phi->InputAt(true_predecessor_index));
    SetRawInputAt(kInputFalseResIndex, phi->InputAt(true_predecessor_index ^ 1));
  }

  virtual size_t InputCount() const {
    return ((input_condition_ != nullptr) && !input_condition_->NeedsMaterialization()) ? 5 : 3;
  }

  virtual void SetRawInputAt(size_t i, HInstruction* instruction) {
    if (i == kInputConditionIndex) {
      if (instruction->IsCondition()) {
        // This instruction can handle a non-materialized condition. To be able
        // to correctly evaluate the condition in that situation, it must
        // maintain the inputs of the condition live. At this point we do not
        // know yet whether the condition requires materialisation so, we take
        // its inputs as input here anyway. The LocationsBuilder will know if
        // the condition must be materialised and set the constraints
        // appropriately for inputs.
        input_condition_ = instruction->AsCondition();
        HTemplateInstruction::SetRawInputAt(kInputCondLeftIndex, input_condition_->InputAt(0));
        HTemplateInstruction::SetRawInputAt(kInputCondRightIndex, input_condition_->InputAt(1));
      } else {
        input_condition_ = nullptr;
        HTemplateInstruction::SetRawInputAt(kInputCondLeftIndex, nullptr);
        HTemplateInstruction::SetRawInputAt(kInputCondRightIndex, nullptr);
      }
    }
    HTemplateInstruction::SetRawInputAt(i, instruction);
  }

  HInstruction* GetCondition() const { return InputAt(kInputConditionIndex); }
  HInstruction* GetCondInputRight() const { return InputAt(kInputCondRightIndex); }

  virtual Primitive::Type GetType() const { return type_; }

  bool CanBeMoved() const OVERRIDE { return true; }
  virtual bool InstructionDataEquals(HInstruction* other) const {
    UNUSED(other);
    return true;
  }

  static constexpr int kInputConditionIndex = 0;
  static constexpr int kInputTrueResIndex = 1;
  static constexpr int kInputFalseResIndex = 2;
  static constexpr int kInputCondLeftIndex = 3;
  static constexpr int kInputCondRightIndex = 4;

  DECLARE_INSTRUCTION(Arm64ConditionalSelect);

 private:
  HCondition* input_condition_;
  Primitive::Type type_;

  DISALLOW_COPY_AND_ASSIGN(HArm64ConditionalSelect);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_NODES_ARM64_H_
