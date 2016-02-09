/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ART_COMPILER_OPTIMIZING_SCHEDULER_ARM64_H_
#define ART_COMPILER_OPTIMIZING_SCHEDULER_ARM64_H_

#include "scheduler.h"

namespace art {
namespace arm64 {

// AArch64 instruction cost.
// We currently assume that all arm64 CPUs share the same instruction cost list.
static constexpr int32_t kArm64IntOpCost = 1;
static constexpr int32_t kArm64FloatOpCost = 4;

static constexpr int32_t kArm64ArrayGetCost = 4;
static constexpr int32_t kArm64ArrayLengthCost = 4;
static constexpr int32_t kArm64ArraySetCost = 2;

// BoundsCheck's successors do not directly depend on it.
static constexpr int32_t kArm64BoundsCheckCost = 1;

static constexpr int32_t kArm64DataProcWithShifterOp = 2;
static constexpr int32_t kArm64DoubleDivCost = 29;
static constexpr int32_t kArm64FloatDivCost = 14;
static constexpr int32_t kArm64FloatMulCost = 5;
static constexpr int32_t kArm64FloatRemCost = 18;  // Call helper.
static constexpr int32_t kArm64InstanceFieldGetCost = 6;
static constexpr int32_t kArm64InstanceOfCost = 18;
static constexpr int32_t kArm64IntDivCost = 4;
static constexpr int32_t kArm64IntMulCost = 5;
static constexpr int32_t kArm64IntRemCost = 5;
static constexpr int32_t kArm64IntermediateAddressCost = 2;
static constexpr int32_t kArm64InvokeCost = 18;
static constexpr int32_t kArm64LoadStringCost = 10;
static constexpr int32_t kArm64MultiplyAccumulate = 5;
static constexpr int32_t kArm64NewArrayCost = 18;  // Call runtime.
static constexpr int32_t kArm64NewInstanceCost = 18;  // Call runtime.
static constexpr int32_t kArm64StaticFieldGetCost = 6;
static constexpr int32_t kArm64TypeConversionCost = 4;  // Fp/integer conversion.

class Arm64SchedulingCostVisitor : public SchedulingCostVisitor {
 public:
  // Default visitor for instructions not handled specifically below.
  void VisitInstruction(HInstruction* instruction);

// We add a second unused parameter to be able to use this macro like the others
// defined in `nodes.h`.
#define FOR_EACH_SCHEDULED_COMMON_INSTRUCTION(M) \
  M(ArrayGet         , unused)                   \
  M(ArrayLength      , unused)                   \
  M(ArraySet         , unused)                   \
  M(BinaryOperation  , unused)                   \
  M(BoundsCheck      , unused)                   \
  M(Div              , unused)                   \
  M(InstanceFieldGet , unused)                   \
  M(InstanceOf       , unused)                   \
  M(Invoke           , unused)                   \
  M(LoadString       , unused)                   \
  M(Mul              , unused)                   \
  M(NewArray         , unused)                   \
  M(NewInstance      , unused)                   \
  M(Rem              , unused)                   \
  M(StaticFieldGet   , unused)                   \
  M(SuspendCheck     , unused)                   \
  M(TypeConversion   , unused)

#define DECLARE_VISIT_INSTRUCTION(type, unused)  \
  void Visit##type(H##type* instruction) OVERRIDE;

  FOR_EACH_SCHEDULED_COMMON_INSTRUCTION(DECLARE_VISIT_INSTRUCTION)
  FOR_EACH_CONCRETE_INSTRUCTION_ARM64(DECLARE_VISIT_INSTRUCTION)

#undef DECLARE_VISIT_INSTRUCTION
};

class HArm64Scheduler : public HScheduler {
 public:
  explicit HArm64Scheduler(ArenaAllocator* arena) : HScheduler(arena, &arm64_cost_visitor_) {}
  ~HArm64Scheduler() OVERRIDE {}

  bool IsSchedulable(const HInstruction* instruction) const OVERRIDE {
    if (HScheduler::IsSchedulable(instruction)) return true;
    // All ARM64-specific instructions can be scheduled.
#define IS_INSTRUCTION(type, unused) instruction->Is##type() ||
    return FOR_EACH_CONCRETE_INSTRUCTION_ARM64(IS_INSTRUCTION) false;
#undef IS_INSTRUCTION
  }

 private:
  Arm64SchedulingCostVisitor arm64_cost_visitor_;
  DISALLOW_COPY_AND_ASSIGN(HArm64Scheduler);
};

}  // namespace arm64
}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_SCHEDULER_ARM64_H_
