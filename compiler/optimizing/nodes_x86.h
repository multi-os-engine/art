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

#ifndef ART_COMPILER_OPTIMIZING_NODES_X86_H_
#define ART_COMPILER_OPTIMIZING_NODES_X86_H_

namespace art {

// Compute the address of the method for X86 Constant area support.
class HX86ComputeBaseMethodAddress : public HExpression<0> {
 public:
  // Treat the value as an int32_t, but it is really a 32 bit native pointer.
  HX86ComputeBaseMethodAddress()
      : HExpression(Primitive::kPrimInt, SideEffects::None(), kNoDexPc) {}

  DECLARE_INSTRUCTION(X86ComputeBaseMethodAddress);

 private:
  DISALLOW_COPY_AND_ASSIGN(HX86ComputeBaseMethodAddress);
};

// Load a constant value from the constant table.
class HX86LoadFromConstantTable : public HExpression<2> {
 public:
  HX86LoadFromConstantTable(HX86ComputeBaseMethodAddress* method_base,
                            HConstant* constant,
                            bool needs_materialization = true)
      : HExpression(constant->GetType(), SideEffects::None(), kNoDexPc),
        needs_materialization_(needs_materialization) {
    SetRawInputAt(0, method_base);
    SetRawInputAt(1, constant);
  }

  bool NeedsMaterialization() const { return needs_materialization_; }

  HX86ComputeBaseMethodAddress* GetBaseMethodAddress() const {
    return InputAt(0)->AsX86ComputeBaseMethodAddress();
  }

  HConstant* GetConstant() const {
    return InputAt(1)->AsConstant();
  }

  DECLARE_INSTRUCTION(X86LoadFromConstantTable);

 private:
  const bool needs_materialization_;

  DISALLOW_COPY_AND_ASSIGN(HX86LoadFromConstantTable);
};

// X86 version of HPackedSwitch that holds a pointer to the base method address.
class HX86PackedSwitch : public HTemplateInstruction<2> {
 public:
  HX86PackedSwitch(int32_t start_value,
                   int32_t num_entries,
                   HInstruction* input,
                   HX86ComputeBaseMethodAddress* method_base,
                   uint32_t dex_pc)
    : HTemplateInstruction(SideEffects::None(), dex_pc),
      start_value_(start_value),
      num_entries_(num_entries) {
    SetRawInputAt(0, input);
    SetRawInputAt(1, method_base);
  }

  bool IsControlFlow() const OVERRIDE { return true; }

  int32_t GetStartValue() const { return start_value_; }

  int32_t GetNumEntries() const { return num_entries_; }

  HX86ComputeBaseMethodAddress* GetBaseMethodAddress() const {
    return InputAt(1)->AsX86ComputeBaseMethodAddress();
  }

  HBasicBlock* GetDefaultBlock() const {
    // Last entry is the default block.
    return GetBlock()->GetSuccessors()[num_entries_];
  }

  DECLARE_INSTRUCTION(X86PackedSwitch);

 private:
  const int32_t start_value_;
  const int32_t num_entries_;

  DISALLOW_COPY_AND_ASSIGN(HX86PackedSwitch);
};

// X86/X86-64 version of HBoundsCheck that checks length in array descriptor.
class HX86BoundsCheckMemory : public HExpression<2> {
 public:
  HX86BoundsCheckMemory(HInstruction* index, HInstruction* array, uint32_t dex_pc)
      : HExpression(index->GetType(), SideEffects::None(), dex_pc) {
    DCHECK_EQ(array->GetType(), Primitive::kPrimNot);
    DCHECK_EQ(index->GetType(), Primitive::kPrimInt);
    SetRawInputAt(0, index);
    SetRawInputAt(1, array);
  }

  bool CanBeMoved() const OVERRIDE { return true; }
  bool InstructionDataEquals(HInstruction* other ATTRIBUTE_UNUSED) const OVERRIDE {
    return true;
  }

  bool CanDoImplicitNullCheckOn(HInstruction* obj) const OVERRIDE {
    return obj == InputAt(1);
  }

  bool NeedsEnvironment() const OVERRIDE { return true; }

  bool CanThrow() const OVERRIDE { return true; }

  virtual size_t GetBaseInputIndex() const OVERRIDE { return 1; }

  HInstruction* GetIndex() const { return InputAt(0); }

  HInstruction* GetArray() const { return InputAt(1); }

  DECLARE_INSTRUCTION(X86BoundsCheckMemory);

 private:
  DISALLOW_COPY_AND_ASSIGN(HX86BoundsCheckMemory);
};


}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_NODES_X86_H_
