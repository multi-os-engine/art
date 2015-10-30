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

// Version of HNeg with access to the constant table for FP types.
class HX86FPNeg : public HExpression<2> {
 public:
  HX86FPNeg(Primitive::Type result_type,
            HInstruction* input,
            HX86ComputeBaseMethodAddress* method_base,
            uint32_t dex_pc)
      : HExpression(result_type, SideEffects::None(), dex_pc) {
    DCHECK(Primitive::IsFloatingPointType(result_type));
    SetRawInputAt(0, input);
    SetRawInputAt(1, method_base);
  }

  DECLARE_INSTRUCTION(X86FPNeg);

 private:
  DISALLOW_COPY_AND_ASSIGN(HX86FPNeg);
};

// Version of HInvokeStaticOrDirect that handles intrinsics that need access to
// the constant area.
class HX86IntrinsicWithConstantArea : public HInvokeStaticOrDirect {
 public:
  HX86IntrinsicWithConstantArea(ArenaAllocator* arena,
                                HInvokeStaticOrDirect* intrinsic,
                                HX86ComputeBaseMethodAddress* method_base)
      : HInvokeStaticOrDirect(arena,
                              intrinsic->GetNumberOfArguments() + 1,
                              intrinsic->GetType(),
                              intrinsic->GetDexPc(),
                              intrinsic->GetDexMethodIndex(),
                              intrinsic->GetTargetMethod(),
                              intrinsic->GetDispatchInfo(),
                              intrinsic->GetOriginalInvokeType(),
                              intrinsic->GetInvokeType(),
                              intrinsic->GetClinitCheckRequirement()) {
    SetIntrinsic(intrinsic->GetIntrinsic(), kNoEnvironmentOrCache);

    // Copy the user arguments.
    size_t num_args = intrinsic->GetNumberOfArguments();
    size_t i;
    for (i = 0; i < num_args; i++) {
      SetArgumentAt(i, intrinsic->InputAt(i));
    }

    // Add the new method base.
    SetArgumentAt(i++, method_base);

    // Copy the extra arguments.
    size_t num_extra_args = intrinsic->InputCount() - num_args;
    for (size_t j = 0; j < num_extra_args; j++, i++) {
      SetArgumentAt(i, intrinsic->InputAt(j + num_args));
    }
  }

  DECLARE_INSTRUCTION(X86IntrinsicWithConstantArea);

 private:
  DISALLOW_COPY_AND_ASSIGN(HX86IntrinsicWithConstantArea);
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

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_NODES_X86_H_
