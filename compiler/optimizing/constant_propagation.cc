/*
 * Copyright (C) 2014 The Android Open Source Project
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

#include "constant_propagation.h"

namespace art {

void ConstantPropagation::Run() {
  // Collect all statements.
  const GrowableArray<HBasicBlock*>& blocks = graph_->GetBlocks();
  for (size_t i = 0 ; i < blocks.Size(); i++) {
    HBasicBlock* block = blocks.Get(i);
    for (HInstructionIterator it(block->GetPhis()); !it.Done();
         it.Advance()) {
      HInstruction* inst = it.Current();
      worklist_.Insert(inst);
    }
    for (HInstructionIterator it(block->GetInstructions()); !it.Done();
         it.Advance()) {
      HInstruction* inst = it.Current();
      worklist_.Insert(inst);
    }
  }

  while (!worklist_.IsEmpty()) {
    HInstruction* inst = worklist_.Pop();

    /* Constant folding: replace `c <- a op b' with a compile-time
       evaluation of `a op b' if `a' and `b' are constant.  */
    if (inst != nullptr && inst->IsAdd()) {
      HAdd* add = inst->AsAdd();
      if (add->GetLeft()->IsIntConstant()
          && add->GetRight()->IsIntConstant()) {
        // Replace `inst` with a compile-time constant.
        int32_t lhs_val = add->GetLeft()->AsIntConstant()->GetValue();
        int32_t rhs_val = add->GetRight()->AsIntConstant()->GetValue();
        int32_t value = lhs_val + rhs_val;
        HIntConstant* constant = new(graph_->GetArena()) HIntConstant(value);
        inst->GetBlock()->InsertInstructionBefore(constant, inst);
        inst->ReplaceWith(constant);
        inst->GetBlock()->RemoveInstruction(inst);
        // Add users of `constant` to the work-list.
        for (HUseIterator<HInstruction> it(constant->GetUses()); !it.Done();
             it.Advance()) {
          Push(it.Current()->GetUser());
        }
      }
    }
  }
}

void ConstantPropagation::Push(HInstruction* inst) {
  // If `inst` is not yet part of `worklist_`, insert it.
  bool found = false;
  for (size_t i = 0; i < worklist_.Size(); ++i) {
    if (worklist_.Get(i) == inst) {
      found = true;
      break;
    }
  }
  if (!found) {
    worklist_.Insert(inst);
  }
}

}  // namespace art
