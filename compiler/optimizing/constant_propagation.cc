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
  // Process nodes (basic blocks) in post-order in the dominator tree.
  for (HPostOrderIterator it(*graph_); !it.Done(); it.Advance()) {
    HBasicBlock* block = it.Current();
    // Traverse this block's instructions in (forward) order and
    // replace the ones that can be statically evaluated by a
    // compile-time counterpart.
    for (HInstructionIterator it(block->GetInstructions());
         !it.Done(); it.Advance()) {
      HInstruction* inst = it.Current();

      /* Constant folding: replace `c <- a op b' with a compile-time
         evaluation of `a op b' if `a' and `b' are constant.  */
      if (inst != nullptr && inst->IsAdd()) {
        HAdd* add = inst->AsAdd();
        HConstant* constant = add->TryStaticEvaluation();
        if (constant != nullptr) {
          // Replace `inst` with a compile-time constant.
          inst->GetBlock()->InsertInstructionBefore(constant, inst);
          inst->ReplaceWith(constant);
          inst->GetBlock()->RemoveInstruction(inst);
        }
      }
    }
  }
}

}  // namespace art
