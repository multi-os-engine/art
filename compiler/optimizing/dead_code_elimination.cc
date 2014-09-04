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

#include "dead_code_elimination.h"

namespace art {

void DeadCodeElimination::Run() {
  // Collect all variables.
  const GrowableArray<HBasicBlock*>& blocks = graph_->GetBlocks();
  for (size_t i = 0 ; i < blocks.Size(); i++) {
    HBasicBlock* block = blocks.Get(i);
    // Only process instructions (not phis).
    for (HInstructionIterator it(block->GetInstructions()); !it.Done();
         it.Advance()) {
      HInstruction* inst = it.Current();
      if (!inst->HasSideEffects()) {
        worklist_.Insert(inst);
      }
    }
  }

  while (!worklist_.IsEmpty()) {
    HInstruction* inst = worklist_.Pop();
    if (!inst->HasUses()) {
      // Add variables (inputs) used by `inst` to the work-list.
      for (HInputIterator it(inst); !it.Done(); it.Advance()) {
        HInstruction* input = it.Current();
        // If `input` is not part of `worklist_`, insert it.
        bool found = false;
        for (size_t i = 0; i < worklist_.Size(); ++i) {
          if (worklist_.Get(i) == input) {
            found = true;
            break;
          }
        }
        if (!found && !input->IsPhi() && !input->HasSideEffects()) {
          worklist_.Insert(input);
        }
      }
      // Remove `inst` from the graph.
      HBasicBlock* block = inst->GetBlock();
      block->RemoveInstruction(inst);
    }
  }
}

}  // namespace art
