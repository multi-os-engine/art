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

#include "nodes.h"
#include "context.h"
#include "optimization.h"

namespace art {

struct Range {
  static Range Default() {
    return Range(INT_MIN, INT_MAX);
  }

  bool operator==(const Range& ni) const {
    return min_value == ni.min_value && max_value == ni.max_value;
  }

  static Range Merge(const Range& a, const Range& b);

  Range(int min, int max) : min_value(min), max_value(max) {}

  static int IntRange(long val) {
    if (val < INT_MIN) {
      return INT_MIN;
    } else if (val > INT_MAX) {
      return INT_MAX;
    }
    return val;
  }

  static Range FromLongs(long min, long max) {
    return Range(IntRange(min), IntRange(max));
  }

  void NarrowUpperBound(int val);

  void NarrowLowerBound(int val);

  bool Contains(int val) {
    if (!IsValid()) {
      return false;
    }

    return min_value <= val && val <= max_value;
  }

  static Range Invalid() { return Range(INT_MAX, INT_MIN); }

  void Invalidate() {
    min_value = INT_MAX;
    max_value = INT_MIN;
  }

  bool IsValid() const {
    return min_value <= max_value;
  }

  int min_value;
  int max_value;
};

class RangeVisitor : public HContextualizedPass<Range, HGraphDelegateVisitor> {
 public:
  RangeVisitor(HGraph* graph) : HContextualizedPass(graph) {}

  void VisitIntConstant(HIntConstant* cte) OVERRIDE {
    SetProperty(cte, Range(cte->GetValue(), cte->GetValue()));
  }

  void VisitBoundType(HBoundType* instr) {
      SetProperty(instr, GetProperty(instr->InputAt(0)));
  }

  void HandleComingFromIf(HBasicBlock* block);

  void BeforeBlock(HBasicBlock* block) OVERRIDE;
};

class RangePropagation : public HOptimization {
 public:
  RangePropagation(HGraph* graph) : HOptimization(graph, kNullPropagationName), visitor_(graph) {}

  void Run() OVERRIDE { visitor_.Run(); }

  static constexpr const char* kNullPropagationName = "range_propagation";

 private:
  RangeVisitor visitor_;
};

}  // namespace art
