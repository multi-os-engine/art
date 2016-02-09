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

#include "scheduler_arm64.h"

namespace art {
namespace arm64 {

void Arm64SchedulingCostVisitor::VisitInstruction(HInstruction* ATTRIBUTE_UNUSED) {
  last_visited_cost_ = kArm64IntOpCost;
}

void Arm64SchedulingCostVisitor::VisitArm64DataProcWithShifterOp(
    HArm64DataProcWithShifterOp* ATTRIBUTE_UNUSED) {
  last_visited_cost_ = kArm64DataProcWithShifterOp;
}

void Arm64SchedulingCostVisitor::VisitArm64MultiplyAccumulate(
    HArm64MultiplyAccumulate* ATTRIBUTE_UNUSED) {
  last_visited_cost_ = kArm64MultiplyAccumulate;
}

void Arm64SchedulingCostVisitor::VisitArm64IntermediateAddress(
    HArm64IntermediateAddress* ATTRIBUTE_UNUSED) {
  last_visited_cost_ = kArm64IntermediateAddressCost;
}

void Arm64SchedulingCostVisitor::VisitArrayGet(HArrayGet* ATTRIBUTE_UNUSED) {
  last_visited_cost_ = kArm64ArrayGetCost;
}

void Arm64SchedulingCostVisitor::VisitArraySet(HArraySet* ATTRIBUTE_UNUSED) {
  last_visited_cost_ = kArm64ArraySetCost;
}

void Arm64SchedulingCostVisitor::VisitArrayLength(HArrayLength* ATTRIBUTE_UNUSED) {
  last_visited_cost_ = kArm64ArrayLengthCost;
}

void Arm64SchedulingCostVisitor::VisitBinaryOperation(HBinaryOperation* instr) {
  last_visited_cost_ = Primitive::IsFloatingPointType(instr->GetResultType()) ? kArm64FloatOpCost
                                                                              : kArm64IntOpCost;
}

void Arm64SchedulingCostVisitor::VisitBoundsCheck(HBoundsCheck* ATTRIBUTE_UNUSED) {
  last_visited_cost_ = kArm64BoundsCheckCost;
}

void Arm64SchedulingCostVisitor::VisitDiv(HDiv* instr) {
  switch (instr->GetResultType()) {
    case Primitive::kPrimFloat:
      last_visited_cost_ = kArm64FloatDivCost;
      break;
    case Primitive::kPrimDouble:
      last_visited_cost_ = kArm64DoubleDivCost;
      break;
    default:
      last_visited_cost_ = kArm64IntDivCost;
      return;
  }
}

void Arm64SchedulingCostVisitor::VisitInstanceFieldGet(HInstanceFieldGet* ATTRIBUTE_UNUSED) {
  last_visited_cost_ = kArm64InstanceFieldGetCost;
}

void Arm64SchedulingCostVisitor::VisitInstanceOf(HInstanceOf* ATTRIBUTE_UNUSED) {
  last_visited_cost_ = kArm64InstanceOfCost;
}

void Arm64SchedulingCostVisitor::VisitInvoke(HInvoke* ATTRIBUTE_UNUSED) {
  last_visited_cost_ = kArm64InvokeCost;
}

void Arm64SchedulingCostVisitor::VisitLoadString(HLoadString* ATTRIBUTE_UNUSED) {
  last_visited_cost_ = kArm64LoadStringCost;
}

void Arm64SchedulingCostVisitor::VisitMul(HMul* instr) {
  last_visited_cost_ = Primitive::IsFloatingPointType(instr->GetResultType()) ? kArm64FloatMulCost
                                                                              : kArm64IntMulCost;
}

void Arm64SchedulingCostVisitor::VisitNewArray(HNewArray* ATTRIBUTE_UNUSED) {
  last_visited_cost_ = kArm64NewArrayCost;
}

void Arm64SchedulingCostVisitor::VisitNewInstance(HNewInstance* ATTRIBUTE_UNUSED) {
  last_visited_cost_ = kArm64NewInstanceCost;
}

void Arm64SchedulingCostVisitor::VisitRem(HRem* instr) {
  last_visited_cost_ = Primitive::IsFloatingPointType(instr->GetResultType()) ? kArm64FloatRemCost
                                                                              : kArm64IntRemCost;
}

void Arm64SchedulingCostVisitor::VisitStaticFieldGet(HStaticFieldGet* ATTRIBUTE_UNUSED) {
  last_visited_cost_ = kArm64StaticFieldGetCost;
}

void Arm64SchedulingCostVisitor::VisitSuspendCheck(HSuspendCheck* ATTRIBUTE_UNUSED) {
  // TODO: Remove codegen dependency of Goto and SuspendCheck, and lower SuspendCheck
  // instruction to make it more scheduler friendly.
  LOG(FATAL) << "Unexpected SuspendCheck.";
  UNREACHABLE();
}

void Arm64SchedulingCostVisitor::VisitTypeConversion(HTypeConversion* instr) {
  if (Primitive::IsFloatingPointType(instr->GetResultType()) ||
      Primitive::IsFloatingPointType(instr->GetInputType())) {
    last_visited_cost_ = kArm64TypeConversionCost;
  } else {
    last_visited_cost_ = kArm64IntOpCost;
  }
}

}  // namespace arm64
}  // namespace art
