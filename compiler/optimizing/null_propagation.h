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

#ifndef ART_COMPILER_OPTIMIZING_NULL_PROPAGATION_H_
#define ART_COMPILER_OPTIMIZING_NULL_PROPAGATION_H_

#include "nodes.h"
#include "context.h"
#include "optimization.h"

namespace art {

struct NullInfo {
  static NullInfo Default() { return NullInfo(false); }

  bool operator==(const NullInfo& ni) const {
    return can_be_null == ni.can_be_null;
  }

  static NullInfo Merge(const NullInfo& a, const NullInfo& b) {
    return NullInfo(a.can_be_null || b.can_be_null);
  }

  explicit NullInfo(bool cbn) : can_be_null(cbn) {}
  bool can_be_null;
};

class NullVisitor : public HContextualizedPass<NullInfo, HGraphDelegateVisitor> {
 public:
  explicit NullVisitor(HGraph* graph) : HContextualizedPass(graph) {}

  void VisitInvoke(HInvoke* instr) OVERRIDE;
  void VisitPhi(HPhi* phi) OVERRIDE;
  void VisitBoundType(HBoundType* instr) {
      SetProperty(instr, NullInfo(instr->InputAt(0)->CanBeNull()));
  }
  void VisitInstruction(HInstruction* instr) OVERRIDE {
    if (instr->GetType() == Primitive::kPrimNot) {
      SetProperty(instr, NullInfo(instr->CanBeNull()));
    }
  }
};

class NullPropagation : public HOptimization {
 public:
  explicit NullPropagation(HGraph* graph)
    : HOptimization(graph, kNullPropagationName),
      visitor_(graph) {}

  void Run() OVERRIDE { visitor_.RunToConvergence(); }

  static constexpr const char* kNullPropagationName = "null_propagation";

 private:
  NullVisitor visitor_;
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_NULL_PROPAGATION_H_
