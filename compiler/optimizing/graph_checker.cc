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
  // Check consistency with respect to predecessors of `block`.
  const GrowableArray<HBasicBlock*>& predecessors = block->GetPredecessors();
  std::map<HBasicBlock*, size_t> predecessors_count;
  for (size_t i = 0, e = predecessors.Size(); i < e; ++i) {
    HBasicBlock* p = predecessors.Get(i);
    ++predecessors_count[p];
  }
  for (auto& pc : predecessors_count) {
    HBasicBlock* p = pc.first;
    size_t p_count_in_block_predecessors = pc.second;
    const GrowableArray<HBasicBlock*>& p_successors = p->GetSuccessors();
    size_t block_count_in_p_successors = 0;
    for (size_t j = 0, f = p_successors.Size(); j < f; ++j) {
      if (p_successors.Get(j) == block) {
        ++block_count_in_p_successors;
      }
    }
    if (p_count_in_block_predecessors != block_count_in_p_successors) {
      std::stringstream error;
      error << "Block " << block->GetBlockId()
            << " lists " << p_count_in_block_predecessors
            << " occurrences of block " << p->GetBlockId()
            << " in its predecessors, whereas block " << p->GetBlockId()
            << " lists " << block_count_in_p_successors
            << " occurrences of block " << block->GetBlockId()
            << " in its successors.";
      errors_.Insert(error.str());
    }
  }

  // Check consistency with respect to successors of `block`.
  const GrowableArray<HBasicBlock*>& successors = block->GetSuccessors();
  std::map<HBasicBlock*, size_t> successors_count;
  for (size_t i = 0, e = successors.Size(); i < e; ++i) {
    HBasicBlock* s = successors.Get(i);
    ++successors_count[s];
  }
  for (auto& sc : successors_count) {
    HBasicBlock* s = sc.first;
    size_t s_count_in_block_successors = sc.second;
    const GrowableArray<HBasicBlock*>& s_predecessors = s->GetPredecessors();
    size_t block_count_in_s_predecessors = 0;
    for (size_t j = 0, f = s_predecessors.Size(); j < f; ++j) {
      if (s_predecessors.Get(j) == block) {
        ++block_count_in_s_predecessors;
      }
    }
    if (s_count_in_block_successors != block_count_in_s_predecessors) {
      std::stringstream error;
      error << "Block " << block->GetBlockId()
            << " lists " << s_count_in_block_successors
            << " occurrences of block " << s->GetBlockId()
            << " in its successors, whereas block " << s->GetBlockId()
            << " lists " << block_count_in_s_predecessors
            << " occurrences of block " << block->GetBlockId()
            << " in its predecessors.";
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
