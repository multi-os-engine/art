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
    if (instr->GetInvokeType() == InvokeType::kDirect) {
      bool needs_check = NeedsImplicitNullCheck(instr);
      DCHECK(!needs_check);
      instr->RemoveImplicitNullCheckNeed();
    }
  }

  void VisitInvokeVirtual(HInvokeVirtual* instr) OVERRIDE {
    if (!NeedsImplicitNullCheck(instr)) {
      instr->RemoveImplicitNullCheckNeed();
    }
  }

  void VisitInvokeInterface(HInvokeInterface* instr) OVERRIDE {
    if (!NeedsImplicitNullCheck(instr)) {
      instr->RemoveImplicitNullCheckNeed();
    }
  }

  void VisitInstanceFieldSet(HInstanceFieldSet* instr) OVERRIDE {
    if (!NeedsImplicitNullCheck(instr)) {
      instr->RemoveImplicitNullCheckNeed();
    }
  }

  void VisitInstanceFieldGet(HInstanceFieldGet* instr) OVERRIDE {
    if (!NeedsImplicitNullCheck(instr)) {
      instr->RemoveImplicitNullCheckNeed();
    }
  }

  void VisitArrayLength(HArrayLength* instr) OVERRIDE {
    if (!NeedsImplicitNullCheck(instr)) {
      instr->RemoveImplicitNullCheckNeed();
    }
  }

  void VisitArraySet(HArraySet* instr) OVERRIDE {
    if (!NeedsImplicitNullCheck(instr)) {
      instr->RemoveImplicitNullCheckNeed();
    }
  }

  void VisitArrayGet(HArrayGet* instr) OVERRIDE {
    if (!NeedsImplicitNullCheck(instr)) {
      instr->RemoveImplicitNullCheckNeed();
    }
  }

  bool NeedsImplicitNullCheck(const HInstruction* instruction) {
    HInstruction* arg = instruction->InputAt(0);
    if (arg->IsNewArray()) {
      // ArraySet may get a NewArray as input, in which case we don't need
      // a null check.
      DCHECK(instruction->IsArraySet());
      return false;
    }
    DCHECK(arg->IsNullCheck()) << instruction->GetKind() << " has first input: " << arg->GetKind();
    if (checked_for_npe.find(arg) == checked_for_npe.end()) {
      HNullCheck* null_check = arg->AsNullCheck();
      checked_for_npe.insert(null_check);
      // For invoke direct we don't need an implicit null check. An explicit
      // one will be performed at the NullCheck node.
      if (instruction->IsInvokeStaticOrDirect()) {
        return false;
      }
      // For all the others we do need a check if one hasn't been generated. The
      // NullCheck becomes unneeded.
      null_check->NotNeeded();
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
