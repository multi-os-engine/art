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

void Arm64SchedulingLatencyVisitor::VisitInstruction(HInstruction* ATTRIBUTE_UNUSED) {
  last_visited_latency_ = kArm64IntOpLatency;
}

void Arm64SchedulingLatencyVisitor::VisitArm64DataProcWithShifterOp(
    HArm64DataProcWithShifterOp* ATTRIBUTE_UNUSED) {
  last_visited_latency_ = kArm64DataProcWithShifterOpLatency;
}

void Arm64SchedulingLatencyVisitor::VisitArm64BitwiseNegatedRight(
    HArm64BitwiseNegatedRight* ATTRIBUTE_UNUSED) {
  last_visited_latency_ = kArm64BitwiseNegatedRightLatency;
}

void Arm64SchedulingLatencyVisitor::VisitArm64IntermediateAddress(
    HArm64IntermediateAddress* ATTRIBUTE_UNUSED) {
  last_visited_latency_ = kArm64IntermediateAddressLatency;
}

void Arm64SchedulingLatencyVisitor::VisitArrayGet(HArrayGet* ATTRIBUTE_UNUSED) {
  last_visited_latency_ = kArm64ArrayGetLatency;
}

void Arm64SchedulingLatencyVisitor::VisitArraySet(HArraySet* ATTRIBUTE_UNUSED) {
  last_visited_latency_ = kArm64ArraySetLatency;
}

void Arm64SchedulingLatencyVisitor::VisitArrayLength(HArrayLength* ATTRIBUTE_UNUSED) {
  last_visited_latency_ = kArm64ArrayLengthLatency;
}

void Arm64SchedulingLatencyVisitor::VisitBinaryOperation(HBinaryOperation* instr) {
  last_visited_latency_ = Primitive::IsFloatingPointType(instr->GetResultType())
      ? kArm64FloatOpLatency
      : kArm64IntOpLatency;
}

void Arm64SchedulingLatencyVisitor::VisitBoundsCheck(HBoundsCheck* ATTRIBUTE_UNUSED) {
  last_visited_latency_ = kArm64BoundsCheckLatency;
}

void Arm64SchedulingLatencyVisitor::VisitDiv(HDiv* instr) {
  switch (instr->GetResultType()) {
    case Primitive::kPrimFloat:
      last_visited_latency_ = kArm64FloatDivLatency;
      break;
    case Primitive::kPrimDouble:
      last_visited_latency_ = kArm64DoubleDivLatency;
      break;
    default:
      last_visited_latency_ = kArm64IntDivLatency;
      return;
  }
}

void Arm64SchedulingLatencyVisitor::VisitInstanceFieldGet(HInstanceFieldGet* ATTRIBUTE_UNUSED) {
  last_visited_latency_ = kArm64InstanceFieldGetLatency;
}

void Arm64SchedulingLatencyVisitor::VisitInstanceOf(HInstanceOf* ATTRIBUTE_UNUSED) {
  last_visited_latency_ = kArm64InstanceOfLatency;
}

void Arm64SchedulingLatencyVisitor::VisitInvoke(HInvoke* ATTRIBUTE_UNUSED) {
  last_visited_latency_ = kArm64InvokeLatency;
}

void Arm64SchedulingLatencyVisitor::VisitLoadString(HLoadString* ATTRIBUTE_UNUSED) {
  last_visited_latency_ = kArm64LoadStringLatency;
}

void Arm64SchedulingLatencyVisitor::VisitMul(HMul* instr) {
  last_visited_latency_ = Primitive::IsFloatingPointType(instr->GetResultType())
      ? kArm64FloatMulLatency
      : kArm64IntMulLatency;
}

void Arm64SchedulingLatencyVisitor::VisitMultiplyAccumulate(HMultiplyAccumulate* ATTRIBUTE_UNUSED) {
  last_visited_latency_ = kArm64MultiplyAccumulateLatency;
}

void Arm64SchedulingLatencyVisitor::VisitNewArray(HNewArray* ATTRIBUTE_UNUSED) {
  last_visited_latency_ = kArm64NewArrayLatency;
}

void Arm64SchedulingLatencyVisitor::VisitNewInstance(HNewInstance* ATTRIBUTE_UNUSED) {
  last_visited_latency_ = kArm64NewInstanceLatency;
}

void Arm64SchedulingLatencyVisitor::VisitRem(HRem* instr) {
  last_visited_latency_ = Primitive::IsFloatingPointType(instr->GetResultType())
      ? kArm64FloatRemLatency
      : kArm64IntRemLatency;
}

void Arm64SchedulingLatencyVisitor::VisitStaticFieldGet(HStaticFieldGet* ATTRIBUTE_UNUSED) {
  last_visited_latency_ = kArm64StaticFieldGetLatency;
}

void Arm64SchedulingLatencyVisitor::VisitSuspendCheck(HSuspendCheck* ATTRIBUTE_UNUSED) {
  last_visited_latency_ = kArm64SuspendCheckLatency;
}

void Arm64SchedulingLatencyVisitor::VisitTypeConversion(HTypeConversion* instr) {
  if (Primitive::IsFloatingPointType(instr->GetResultType()) ||
      Primitive::IsFloatingPointType(instr->GetInputType())) {
    last_visited_latency_ = kArm64TypeConversionLatency;
  } else {
    last_visited_latency_ = kArm64IntOpLatency;
  }
}

}  // namespace arm64
}  // namespace art
