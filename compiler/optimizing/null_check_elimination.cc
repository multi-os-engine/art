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

#include "null_check_elimination.h"

namespace art {

// TODO: follow dominators to see if a value has already been NullChecked
// and thus guaranteed not to be null.
// TODO: investigate if it is worth keeping track of field-set/field-get patterns.
class NullCheckEliminationVisitor : public HGraphVisitor {
 public:
  explicit NullCheckEliminationVisitor(HGraph* graph) : HGraphVisitor(graph) {}

 private:
  void VisitNullCheck(HNullCheck* null_check) OVERRIDE {
    HInstruction* obj = null_check->InputAt(0);
    if (!obj->CanBeNull()) {
      // TODO: add ReplaceWithAndRemove
      null_check->ReplaceWith(obj);
      null_check->GetBlock()->RemoveInstruction(null_check);
    }
  }

  void VisitPhi(HPhi* phi) OVERRIDE {
    bool can_be_null = false;
    for (size_t i = 0; i < phi->InputCount(); i++) {
      can_be_null |= phi->InputAt(i)->CanBeNull();
    }
    phi->SetCanBeNull(can_be_null);
  }

  void VisitStoreLocal(HStoreLocal* store) OVERRIDE {
    UNUSED(store);
    DCHECK(false) << "Store locals should have been removed.";
  }
};

void NullCheckElimination::Run() {
  NullCheckEliminationVisitor visitor(graph_);
  // To properly propagate not-null info we need to visit in the dominator-based order.
  // Reverse post order guarantees a node's dominators are visited first.
  visitor.VisitReversePostOrder();
}

}  // namespace art
