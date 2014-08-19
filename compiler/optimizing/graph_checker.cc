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

#include "graph_checker.h"

namespace art {

void GraphChecker::VisitBasicBlock(HBasicBlock* block) {
  // Check consistency wrt predecessors of `block`.
  const GrowableArray<HBasicBlock*>& predecessors = block->GetPredecessors();
  for (size_t i = 0, e = predecessors.Size(); i < e; ++i) {
    const HBasicBlock* p = predecessors.Get(i);
    const GrowableArray<HBasicBlock*>& p_successors = p->GetSuccessors();
    bool block_found_in_p_successors = false;
    for (size_t j = 0, f = p_successors.Size(); j < f; ++j) {
      if (p_successors.Get(j) == block) {
        block_found_in_p_successors = true;
        break;
      }
    }
    if (!block_found_in_p_successors) {
      std::stringstream error;
      error << "Block " << block->GetBlockId() << " lists block "
            << p->GetBlockId() << " as predecessor, but block "
            << p->GetBlockId() << " does not list block "
            << block->GetBlockId() << " as successor.";
      errors_.Insert(error.str());
    }
  }

  // Check consistency wrt successors of `block`.
  const GrowableArray<HBasicBlock*>& successors = block->GetSuccessors();
  for (size_t i = 0, e = successors.Size(); i < e; ++i) {
    const HBasicBlock* s = successors.Get(i);
    const GrowableArray<HBasicBlock*>& s_predecessors = s->GetPredecessors();
    bool block_found_in_s_predecessors = false;
    for (size_t j = 0, f = s_predecessors.Size(); j < f; ++j) {
      if (s_predecessors.Get(j) == block) {
        block_found_in_s_predecessors = true;
        break;
      }
    }
    if (!block_found_in_s_predecessors) {
      std::stringstream error;
      error << "Block " << block->GetBlockId() << " lists block "
            << s->GetBlockId() << " as successor, but block "
            << s->GetBlockId() << " does not list block "
            << block->GetBlockId() << " as predecessor.";
      errors_.Insert(error.str());
    }
  }
}

bool GraphChecker::IsValid() const {
  return errors_.IsEmpty();
}

const GrowableArray<std::string>& GraphChecker::GetErrors() const {
  return errors_;
}

}  // namespace art
