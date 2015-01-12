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
      bool needs_check = NeedsImplicitNullCheck(instr, instr->GetNullCheck());
      DCHECK(!needs_check);
      instr->RemoveImplicitNullCheckNeed();
    }
  }

  template<typename T> void Visit(T i) {
    if (!NeedsImplicitNullCheck(i, i->GetNullCheck())) {
      i->RemoveImplicitNullCheckNeed();
    }
  }

  void VisitInvokeVirtual(HInvokeVirtual* instr) OVERRIDE {
    Visit(instr);
  }

  void VisitInvokeInterface(HInvokeInterface* instr) OVERRIDE {
    Visit(instr);
  }

  void VisitInstanceFieldSet(HInstanceFieldSet* instr) OVERRIDE {
    Visit(instr);
  }

  void VisitInstanceFieldGet(HInstanceFieldGet* instr) OVERRIDE {
    Visit(instr);
  }

  void VisitArrayLength(HArrayLength* instr) OVERRIDE {
    Visit(instr);
  }

  void VisitArraySet(HArraySet* instr) OVERRIDE {
    Visit(instr);
  }

  void VisitArrayGet(HArrayGet* instr) OVERRIDE {
    Visit(instr);
  }

  bool NeedsImplicitNullCheck(HInstruction* instruction, HNullCheck* null_check) {
    if (null_check == nullptr) {
      return false;
    }

    auto bbs = inc_map.find(null_check);
    HBasicBlock* instruction_bb = instruction->GetBlock();

    if (bbs == inc_map.end()) {
      // It's the first time we see the null check: add it to the map.
      std::set<HBasicBlock*> check_locations;
      check_locations.insert(instruction_bb);
      inc_map[null_check] = check_locations;

      // For invoke direct we don't need an implicit null check. An explicit
      // one will be performed at the NullCheck node.
      if (instruction->IsInvokeStaticOrDirect()) {
        inc_map[null_check].insert(null_check->GetBlock());
        return false;
      }
      // For all the others we do need a check if one hasn't been generated. The
      // NullCheck becomes unneeded.
      null_check->SetNeeded(false);
      return true;
    }

    // The null check was already handled. If that happened in a dominating block
    // we don't need an implicit check,
    for (auto check_location = bbs->second.begin();
         check_location != bbs->second.end(); check_location++) {
      if ((*check_location)->Dominates(instruction_bb)) {
        return false;
      }
    }

    // The null check was handled but it was in a block who doesn't dominate us.
    // So we need to enable the check for our current block.
    if (instruction->IsInvokeStaticOrDirect()) {
      inc_map[null_check].insert(null_check->GetBlock());
      // Mark the null check as needed.
      // TODO: This is not optimal because now some implicit check might become
      // redundant. Consider it's worth updating the other sites.
      null_check->SetNeeded(true);
      return false;
    }
    bbs->second.insert(instruction_bb);
    return true;
  }

 private:
  // Map from null checks to the basic blocks of the instructions who record
  // the implicit null check.
  std::map<HNullCheck*, std::set<HBasicBlock*>> inc_map;
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
