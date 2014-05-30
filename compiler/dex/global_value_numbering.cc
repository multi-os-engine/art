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

#include "global_value_numbering.h"

#include "local_value_numbering.h"

namespace art {

GlobalValueNumbering::GlobalValueNumbering(CompilationUnit* cu, ScopedArenaAllocator* allocator)
    : cu_(cu),
      allocator_(allocator),
      last_value_(0u),
      modifications_allowed_(false),
      global_value_map_(std::less<uint64_t>(), allocator->Adapter()),
      field_index_map_(FieldReferenceComparator(), allocator->Adapter()),
      lvns_(cu_->mir_graph->GetNumBlocks(), nullptr, allocator->Adapter()) {
}

GlobalValueNumbering::~GlobalValueNumbering() {
  STLDeleteElements(&lvns_);
}

bool GlobalValueNumbering::ProcessBasicBlock(BasicBlock* bb) {
  if (bb->data_flow_info == nullptr) {
    return false;
  }
  std::unique_ptr<LocalValueNumbering> lvn(new (allocator_) LocalValueNumbering(this, allocator_));
  if (bb->block_type == kEntryBlock || bb->catch_entry) {
    // TODO: Initialize lvn for entry block.
  } else if (bb->predecessors->Size() == 1u) {
    // TODO: Initialize lvn with a single predecessor.
  } else {
    // TODO: Initialize lvn with multiple predecessors.
  }
  for (MIR* mir = bb->first_mir_insn; mir != NULL; mir = mir->next) {
    lvn->GetValueNumber(mir);
  }
  return false;  // TODO: rerun on change when we handle multiple predecessors.
}

uint16_t GlobalValueNumbering::GetFieldId(const MirFieldInfo& field_info) {
  FieldReference key = { field_info.DeclaringDexFile(), field_info.DeclaringFieldIndex() };
  auto it = field_index_map_.find(key);
  if (it != field_index_map_.end()) {
    return it->second;
  }
  DCHECK_LT(field_index_map_.size(), kNoValue);
  uint16_t id = field_index_map_.size();
  field_index_map_.Put(key, id);
  return id;
}

}  // namespace art
