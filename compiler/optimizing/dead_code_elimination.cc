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

#include "base/bit_vector-inl.h"

namespace art {

void DeadCodeElimination::Run() {
  // Process basic blocks in post-order in the dominator tree, so that
  // a dead instruction depending on another dead instruction is
  // removed.
  for (HPostOrderIterator it(*graph_); !it.Done(); it.Advance()) {
    HBasicBlock* block = it.Current();
    // Traverse this block's instructions in backward order and remove
    // the unused ones.
    for (HBackwardInstructionIterator it(block->GetInstructions());
         !it.Done(); it.Advance()) {
      HInstruction* inst = it.Current();
      if (!inst->IsControlFlow()
          && !inst->HasSideEffects()
          && !inst->HasUses()) {
        block->RemoveInstruction(inst);
      }
    }
  }
}

}  // namespace art
