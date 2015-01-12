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

#include "implicit_null_check_elimination.h"

#include <set>

namespace art {

class INCEVisitor : public HGraphVisitor {
 public:
  explicit INCEVisitor(HGraph* graph) : HGraphVisitor(graph) {}

 private:
  void VisitInvokeStaticOrDirect(HInvokeStaticOrDirect* instr) OVERRIDE {
    if (!NeedsNullCheck(instr)) {
      instr->RemoveImplicitNullCheckNeed();
    }
  }

  void VisitInvokeVirtual(HInvokeVirtual* instr) OVERRIDE {
    if (!NeedsNullCheck(instr)) {
      instr->RemoveImplicitNullCheckNeed();
    }
  }

  void VisitInvokeInterface(HInvokeInterface* instr) OVERRIDE {
    if (!NeedsNullCheck(instr)) {
      instr->RemoveImplicitNullCheckNeed();
    }
  }

  void VisitInstanceFieldSet(HInstanceFieldSet* instr) OVERRIDE {
    if (!NeedsNullCheck(instr)) {
      instr->RemoveImplicitNullCheckNeed();
    }
  }

  void VisitInstanceFieldGet(HInstanceFieldGet* instr) OVERRIDE {
    if (!NeedsNullCheck(instr)) {
      instr->RemoveImplicitNullCheckNeed();
    }
  }

  void VisitArraySet(HArraySet* instr) OVERRIDE {
    if (!NeedsNullCheck(instr)) {
      instr->RemoveImplicitNullCheckNeed();
    }
  }

  void VisitArrayGet(HArrayGet* instr) OVERRIDE {
    if (!NeedsNullCheck(instr)) {
      instr->RemoveImplicitNullCheckNeed();
    }
  }

  void VisitNullCheck(HNullCheck* instr) OVERRIDE {
    if (instr->ForceCheck()) {
      checked_for_npe.insert(instr);
      // Add the input as well in case the null check was replaced with its
      // input at use sites.
      checked_for_npe.insert(instr->InputAt(0));
    }
  }

  bool NeedsNullCheck(const HInstruction* instruction) {
    HInstruction* input = instruction->InputAt(0);
    if (checked_for_npe.find(input) == checked_for_npe.end()) {
      checked_for_npe.insert(input);
      return true;
    }
    return false;
  }

 private:
  std::set<HInstruction*> checked_for_npe;
};

void ImplicitNullCheckElimination::Run() {
  INCEVisitor visitor(graph_);
  // Reverse post order guarantees a node's dominators are visited first.
  // We want to visit in the dominator-based order since if we already performed
  // a null check in the dominating invoke we don't need to do it again for the
  // dominated.
  visitor.VisitReversePostOrder();
}

}  // namespace art
