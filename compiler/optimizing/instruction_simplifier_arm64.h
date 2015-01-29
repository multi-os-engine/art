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

#ifndef ART_COMPILER_OPTIMIZING_INSTRUCTION_SIMPLIFIER_ARM64_H_
#define ART_COMPILER_OPTIMIZING_INSTRUCTION_SIMPLIFIER_ARM64_H_

#include "nodes.h"
#include "optimization.h"

namespace art {
namespace arm64 {

class InstructionSimplifierArm64 : public HGraphVisitor {
 public:
  explicit InstructionSimplifierArm64(HGraph* graph) : HGraphVisitor(graph) {}

 private:
  void ReplacePhiWithCsel(HIf* instr_if, HPhi* phi);
  void TryMergeInputIntoOperand(HBinaryOperation* instruction);
  bool TryFitIntoOperand(HInstruction* instr, HInstruction* left, HInstruction* op);

  void VisitAdd(HAdd* instruction) OVERRIDE;
  void VisitAnd(HAnd* instruction) OVERRIDE;
  void VisitIf(HIf* instruction) OVERRIDE;
  void VisitOr(HOr* instruction) OVERRIDE;
  void VisitSub(HSub* instruction) OVERRIDE;
  void VisitXor(HXor* instruction) OVERRIDE;
};

}  // namespace arm64
}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_INSTRUCTION_SIMPLIFIER_ARM64_H_
