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

#include <limits.h>

#include "induction_var_range.h"

namespace art {

static bool ValidConstant32(int32_t c) {
  return INT_MIN < c && c < INT_MAX;
}

static bool ValidConstant64(int64_t c) {
  return INT_MIN < c && c < INT_MAX;
}

/** Returns true if 32-bit addition can be done safely (and is not an unknown range). */
static bool safe_add(int32_t c1, int32_t c2) {
  if (ValidConstant32(c1) && ValidConstant32(c2)) {
    return ValidConstant64(static_cast<int64_t>(c1) + static_cast<int64_t>(c2));
  }
  return false;
}

/** Returns true if 32-bit subtraction can be done safely (and is not an unknown range). */
static bool safe_sub(int32_t c1, int32_t c2) {
  if (ValidConstant32(c1) && ValidConstant32(c2)) {
    return ValidConstant64(static_cast<int64_t>(c1) - static_cast<int64_t>(c2));
  }
  return false;
}

/** Returns true if 32-bit multiplication can be done safely (and is not an unknown range). */
static bool safe_mul(int32_t c1, int32_t c2) {
  if (ValidConstant32(c1) && ValidConstant32(c2)) {
    return ValidConstant64(static_cast<int64_t>(c1) * static_cast<int64_t>(c2));
  }
  return false;
}

/** Returns true if 32-bit division can be done safely (and is not an unknown range). */
static bool safe_div(int32_t c1, int32_t c2) {
  if (ValidConstant32(c1) && ValidConstant32(c2) && c2 != 0) {
    return ValidConstant64(static_cast<int64_t>(c1) / static_cast<int64_t>(c2));
  }
  return false;
}

/** Returns true for 32/64-bit integral constant within known range. */
static bool IsIntAndGet(HInstruction* instruction, int32_t* value) {
  if (instruction->IsIntConstant()) {
    const int32_t c = instruction->AsIntConstant()->GetValue();
    if (ValidConstant32(c)) {
      *value = c;
      return true;
    }
  } else if (instruction->IsLongConstant()) {
    const int64_t c = instruction->AsLongConstant()->GetValue();
    if (ValidConstant64(c)) {
      *value = c;
      return true;
    }
  }
  return false;
}

//
// Public class methods.
//

InductionVarRange::InductionVarRange(HInductionVarAnalysis* induction) : induction_(induction) {
}

InductionVarRange::Value InductionVarRange::GetMinInduction(HInstruction* context,
                                                            HInstruction* instruction) {
  HLoopInformation* loop = context->GetBlock()->GetLoopInformation();
  if (loop != nullptr && induction_ != nullptr) {
    return GetMin(induction_->LookupInfo(loop, instruction), GetTripCount(loop, context));
  }
  return Value(INT_MIN);
}

InductionVarRange::Value InductionVarRange::GetMaxInduction(HInstruction* context,
                                                            HInstruction* instruction) {
  HLoopInformation* loop = context->GetBlock()->GetLoopInformation();
  if (loop != nullptr && induction_ != nullptr) {
    return GetMax(induction_->LookupInfo(loop, instruction), GetTripCount(loop, context));
  }
  return Value(INT_MAX);
}

//
// Private class methods.
//

HInductionVarAnalysis::InductionInfo* InductionVarRange::GetTripCount(HLoopInformation* loop,
                                                                      HInstruction* context) {
  // The trip-count expression is only valid when the top-test is taken at least once,
  // that means, when the analyzed context appears outside the loop header itself.
  // Early-exit loops are okay, since in those cases, the trip-count is conservative.
  if (context->GetBlock() != loop->GetHeader()) {
    HInductionVarAnalysis::InductionInfo* induc =
        induction_->LookupInfo(loop, loop->GetHeader()->GetLastInstruction());
    if (induc != nullptr) {
      // Wrap the trip-count representation in its own unusual NOP node, so that range analysis
      // is able to determine the [0, TC - 1] interval without having to construct constants.
      return induction_->CreateInvariantOp(HInductionVarAnalysis::kNop, induc, induc);
    }
  }
  return nullptr;
}

InductionVarRange::Value InductionVarRange::GetFetch(HInstruction* instruction,
                                                     int32_t fail_value) {
  // Detect constants and chase the fetch a bit deeper into the HIR tree, so that it becomes
  // more likely range analysis will compare the same instructions as terminal nodes.
  int32_t value;
  if (IsIntAndGet(instruction, &value)) {
    return Value(value);
  } else if (instruction->IsAdd()) {
    if (IsIntAndGet(instruction->InputAt(0), &value)) {
      return AddValue(Value(value), GetFetch(instruction->InputAt(1), fail_value), fail_value);
    } else if (IsIntAndGet(instruction->InputAt(1), &value)) {
      return AddValue(GetFetch(instruction->InputAt(0), fail_value), Value(value), fail_value);
    }
  }
  return Value(instruction, 1, 0);
}

InductionVarRange::Value InductionVarRange::GetMin(HInductionVarAnalysis::InductionInfo* info,
                                                   HInductionVarAnalysis::InductionInfo* induc) {
  if (info != nullptr) {
    switch (info->induction_class) {
      case HInductionVarAnalysis::kInvariant:
        // Invariants.
        switch (info->operation) {
          case HInductionVarAnalysis::kNop:  // normalized: 0
            DCHECK_EQ(induc->op_a, induc->op_b);
            return Value(0);
          case HInductionVarAnalysis::kAdd:
            return AddValue(GetMin(info->op_a, induc), GetMin(info->op_b, induc), INT_MIN);
          case HInductionVarAnalysis::kSub:  // second max!
            return SubValue(GetMin(info->op_a, induc), GetMax(info->op_b, induc), INT_MIN);
          case HInductionVarAnalysis::kNeg:  // second max!
            return SubValue(Value(0), GetMax(info->op_b, induc), INT_MIN);
          case HInductionVarAnalysis::kMul:
            return GetMul(info->op_a, info->op_b, induc, INT_MIN);
          case HInductionVarAnalysis::kDiv:
            return GetDiv(info->op_a, info->op_b, induc, INT_MIN);
          case HInductionVarAnalysis::kFetch:
            return GetFetch(info->fetch, INT_MIN);
        }
        break;
      case HInductionVarAnalysis::kLinear:
        // Minimum over linear induction a * i + b, for normalized 0 <= i < TC.
        return AddValue(GetMul(info->op_a, induc, induc, INT_MIN),
                        GetMin(info->op_b, induc), INT_MIN);
      case HInductionVarAnalysis::kWrapAround:
      case HInductionVarAnalysis::kPeriodic:
        // Minimum over all values in the wrap-around/periodic.
        return MinValue(GetMin(info->op_a, induc), GetMin(info->op_b, induc));
    }
  }
  return Value(INT_MIN);
}

InductionVarRange::Value InductionVarRange::GetMax(HInductionVarAnalysis::InductionInfo* info,
                                                   HInductionVarAnalysis::InductionInfo* induc) {
  if (info != nullptr) {
    switch (info->induction_class) {
      case HInductionVarAnalysis::kInvariant:
        // Invariants.
        switch (info->operation) {
          case HInductionVarAnalysis::kNop:    // normalized: TC - 1
            DCHECK_EQ(induc->op_a, induc->op_b);
            return SubValue(GetMax(info->op_b, induc), Value(1), INT_MAX);
          case HInductionVarAnalysis::kAdd:
            return AddValue(GetMax(info->op_a, induc), GetMax(info->op_b, induc), INT_MAX);
          case HInductionVarAnalysis::kSub:  // second min!
            return SubValue(GetMax(info->op_a, induc), GetMin(info->op_b, induc), INT_MAX);
          case HInductionVarAnalysis::kNeg:  // second min!
            return SubValue(Value(0), GetMin(info->op_b, induc), INT_MAX);
          case HInductionVarAnalysis::kMul:
            return GetMul(info->op_a, info->op_b, induc, INT_MAX);
          case HInductionVarAnalysis::kDiv:
            return GetDiv(info->op_a, info->op_b, induc, INT_MAX);
          case HInductionVarAnalysis::kFetch:
            return GetFetch(info->fetch, INT_MAX);
        }
        break;
      case HInductionVarAnalysis::kLinear:
        // Maximum over linear induction a * i + b, for normalized 0 <= i < TC.
        return AddValue(GetMul(info->op_a, induc, induc, INT_MAX),
                        GetMax(info->op_b, induc), INT_MAX);
      case HInductionVarAnalysis::kWrapAround:
      case HInductionVarAnalysis::kPeriodic:
        // Maximum over all values in the wrap-around/periodic.
        return MaxValue(GetMax(info->op_a, induc), GetMax(info->op_b, induc));
    }
  }
  return Value(INT_MAX);
}

InductionVarRange::Value InductionVarRange::GetMul(HInductionVarAnalysis::InductionInfo* info1,
                                                   HInductionVarAnalysis::InductionInfo* info2,
                                                   HInductionVarAnalysis::InductionInfo* induc,
                                                   int32_t fail_value) {
  Value v1_min = GetMin(info1, induc);
  Value v1_max = GetMax(info1, induc);
  Value v2_min = GetMin(info2, induc);
  Value v2_max = GetMax(info2, induc);
  if (v1_min.a_constant == 0 && v1_min.b_constant >= 0) {
    // Positive range vs. positive or negative range.
    if (v2_min.a_constant == 0 && v2_min.b_constant >= 0) {
      return (fail_value < 0) ? MulValue(v1_min, v2_min, fail_value)
                              : MulValue(v1_max, v2_max, fail_value);
    } else if (v2_max.a_constant == 0 && v2_max.b_constant <= 0) {
      return (fail_value < 0) ? MulValue(v1_max, v2_min, fail_value)
                              : MulValue(v1_min, v2_max, fail_value);
    }
  } else if (v1_min.a_constant == 0 && v1_min.b_constant <= 0) {
    // Negative range vs. positive or negative range.
    if (v2_min.a_constant == 0 && v2_min.b_constant >= 0) {
      return (fail_value < 0) ? MulValue(v1_min, v2_max, fail_value)
                              : MulValue(v1_max, v2_min, fail_value);
    } else if (v2_max.a_constant == 0 && v2_max.b_constant <= 0) {
      return (fail_value < 0) ? MulValue(v1_max, v2_max, fail_value)
                              : MulValue(v1_min, v2_min, fail_value);
    }
  }
  return Value(fail_value);
}

InductionVarRange::Value InductionVarRange::GetDiv(HInductionVarAnalysis::InductionInfo* info1,
                                                   HInductionVarAnalysis::InductionInfo* info2,
                                                   HInductionVarAnalysis::InductionInfo* induc,
                                                   int32_t fail_value) {
  Value v1_min = GetMin(info1, induc);
  Value v1_max = GetMax(info1, induc);
  Value v2_min = GetMin(info2, induc);
  Value v2_max = GetMax(info2, induc);
  if (v1_min.a_constant == 0 && v1_min.b_constant >= 0) {
    // Positive range vs. positive or negative range.
    if (v2_min.a_constant == 0 && v2_min.b_constant >= 0) {
      return (fail_value < 0) ? DivValue(v1_min, v2_max, fail_value)
                              : DivValue(v1_max, v2_min, fail_value);
    } else if (v2_max.a_constant == 0 && v2_max.b_constant <= 0) {
      return (fail_value < 0) ? DivValue(v1_max, v2_max, fail_value)
                              : DivValue(v1_min, v2_min, fail_value);
    }
  } else if (v1_min.a_constant == 0 && v1_min.b_constant <= 0) {
    // Negative range vs. positive or negative range.
    if (v2_min.a_constant == 0 && v2_min.b_constant >= 0) {
      return (fail_value < 0) ? DivValue(v1_min, v2_min, fail_value)
                              : DivValue(v1_max, v2_max, fail_value);
    } else if (v2_max.a_constant == 0 && v2_max.b_constant <= 0) {
      return (fail_value < 0) ? DivValue(v1_max, v2_min, fail_value)
                              : DivValue(v1_min, v2_max, fail_value);
    }
  }
  return Value(fail_value);
}

InductionVarRange::Value InductionVarRange::AddValue(Value v1, Value v2, int32_t fail_value) {
  if (safe_add(v1.b_constant, v2.b_constant)) {
    const int32_t b = v1.b_constant + v2.b_constant;
    if (v1.a_constant == 0) {
      return Value(v2.instruction, v2.a_constant, b);
    } else if (v2.a_constant == 0) {
      return Value(v1.instruction, v1.a_constant, b);
    } else if (v1.instruction == v2.instruction && safe_add(v1.a_constant, v2.a_constant)) {
      return Value(v1.instruction, v1.a_constant + v2.a_constant, b);
    }
  }
  return Value(fail_value);
}

InductionVarRange::Value InductionVarRange::SubValue(Value v1, Value v2, int32_t fail_value) {
  if (safe_sub(v1.b_constant, v2.b_constant)) {
    const int32_t b = v1.b_constant - v2.b_constant;
    if (v2.a_constant == 0) {
      return Value(v1.instruction, v1.a_constant, b);
    } else if (v1.instruction == v2.instruction && safe_sub(v1.a_constant, v2.a_constant)) {
      return Value(v1.instruction, v1.a_constant - v2.a_constant, b);
    }
  }
  return Value(fail_value);
}

InductionVarRange::Value InductionVarRange::MulValue(Value v1, Value v2, int32_t fail_value) {
  if (v1.a_constant == 0) {
    if (safe_mul(v1.b_constant, v2.a_constant) && safe_mul(v1.b_constant, v2.b_constant)) {
      return Value(v2.instruction, v1.b_constant * v2.a_constant, v1.b_constant * v2.b_constant);
    }
  } else if (v2.a_constant == 0) {
    if (safe_mul(v1.a_constant, v2.b_constant) && safe_mul(v1.b_constant, v2.b_constant)) {
      return Value(v1.instruction, v1.a_constant * v2.b_constant, v1.b_constant * v2.b_constant);
    }
  }
  return Value(fail_value);
}

InductionVarRange::Value InductionVarRange::DivValue(Value v1, Value v2, int32_t fail_value) {
  if (v2.a_constant == 0) {
    if (safe_div(v1.a_constant, v2.b_constant) && safe_div(v1.b_constant, v2.b_constant)) {
      return Value(v1.instruction, v1.a_constant / v2.b_constant, v1.b_constant / v2.b_constant);
    }
  }
  return Value(fail_value);
}

InductionVarRange::Value InductionVarRange::MinValue(Value v1, Value v2) {
  if (v1.instruction == v2.instruction && v1.a_constant == v2.a_constant) {
    return Value(v1.instruction, v1.a_constant, std::min(v1.b_constant, v2.b_constant));
  }
  return Value(INT_MIN);
}

InductionVarRange::Value InductionVarRange::MaxValue(Value v1, Value v2) {
  if (v1.instruction == v2.instruction && v1.a_constant == v2.a_constant) {
    return Value(v1.instruction, v1.a_constant, std::max(v1.b_constant, v2.b_constant));
  }
  return Value(INT_MAX);
}

}  // namespace art
