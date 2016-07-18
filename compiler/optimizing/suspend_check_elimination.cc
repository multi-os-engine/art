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

#include "suspend_check_elimination.h"

#include "code_generator.h"

namespace art {

SuspendCheckElimination::SuspendCheckElimination(const CodeGenerator* codegen)
      : HOptimization(codegen->GetGraph(), kSuspendCheckEliminationPassName),
        codegen_(codegen) {}

// Removes suspend checks from a method.
class HSuspendCheckEliminationVisitor : public HGraphDelegateVisitor {
 public:
  explicit HSuspendCheckEliminationVisitor(HGraph* graph)
      : HGraphDelegateVisitor(graph) {}

 private:
  void VisitSuspendCheck(HSuspendCheck* suspend_check) OVERRIDE {
    if (suspend_check->IsSuspendCheckEntry()) {
      // We have already verified that this is a leaf method, so remove
      // the suspend check.
      suspend_check->GetBlock()->RemoveInstruction(suspend_check);
    }
  }

  DISALLOW_COPY_AND_ASSIGN(HSuspendCheckEliminationVisitor);
};

void SuspendCheckElimination::Run() {
  if (codegen_->IsLeafMethod()) {
    HSuspendCheckEliminationVisitor visitor(codegen_->GetGraph());
    visitor.VisitInsertionOrder();
  } else {
    // Suspend checks should only be removed from leaf methods.
  }
}

}  // namespace art
