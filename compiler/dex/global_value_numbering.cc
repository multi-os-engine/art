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
  if (UNLIKELY(!Good())) {
    return false;
  }
  if (bb->data_flow_info == nullptr) {
    return false;
  }
  std::unique_ptr<LocalValueNumbering> lvn(
      new (allocator_) LocalValueNumbering(this, allocator_, bb->id));
  if (bb->block_type == kEntryBlock || bb->catch_entry) {
    if ((cu_->access_flags & kAccStatic) == 0) {
      // If non-static method, mark "this" as non-null
      int this_reg = cu_->num_dalvik_registers - cu_->num_ins;
      lvn->SetNullChecked(this_reg);
    }
  } else if (bb->predecessors->Size() == 1u) {
    BasicBlock* pred_bb = cu_->mir_graph->GetBasicBlock(bb->predecessors->Get(0));
    const LocalValueNumbering* src = lvns_[pred_bb->id];
    DCHECK(src != nullptr);  // The predecessor must have already been processed at least once.
    lvn->Copy(*src);
    if (pred_bb->block_type == kDalvikByteCode && pred_bb->last_mir_insn != nullptr) {
      // Check to see this is the branch of IF_EQZ/IF_NEZ where the tested sreg can't be null.
      Instruction::Code last_opcode = pred_bb->last_mir_insn->dalvikInsn.opcode;
      if ((last_opcode == Instruction::IF_EQZ && pred_bb->fall_through == bb->id) ||
          (last_opcode == Instruction::IF_NEZ && pred_bb->taken == bb->id)) {
        lvn->SetNullChecked(pred_bb->last_mir_insn->ssa_rep->uses[0]);
      }
    }
  } else {
    // Merge all incoming arcs.
    GrowableArray<BasicBlockId>::Iterator iter(bb->predecessors);
    BasicBlock* pred_bb = cu_->mir_graph->GetBasicBlock(iter.Next());
    CHECK(pred_bb != NULL);
    while (lvns_[pred_bb->id] == nullptr) {
      BasicBlock* pred_bb = cu_->mir_graph->GetBasicBlock(iter.Next());
      // At least one predecessor must have been processed before this bb.
      DCHECK(pred_bb != nullptr);
      DCHECK(pred_bb->data_flow_info != nullptr);
    }
    lvn->Copy(*lvns_[pred_bb->id]);
    while (true) {
      pred_bb = cu_->mir_graph->GetBasicBlock(iter.Next());
      if (pred_bb == nullptr) {
        break;
      }
      if (lvns_[pred_bb->id] == nullptr) {
        continue;
      }
      // Use the unique bb's id as the lvn id.
      COMPILE_ASSERT(sizeof(bb->id) == 2, bb_id_must_be_16_bit);
      lvn->Merge(*lvns_[pred_bb->id]);
    }
  }
  for (MIR* mir = bb->first_mir_insn; mir != NULL; mir = mir->next) {
    lvn->GetValueNumber(mir);
  }

  DCHECK(bb->NumSuccessors() > 1 || lvns_[bb->id] == nullptr);
  bool change = lvns_[bb->id] == nullptr
                ? bb->NumSuccessors() > 1
                : !lvns_[bb->id]->Equals(*lvn);
  if (change) {
    std::unique_ptr<LocalValueNumbering> old_lvn(lvns_[bb->id]);
    lvns_[bb->id] = lvn.release();
  }
  return change;
}

uint16_t GlobalValueNumbering::GetFieldId(const MirFieldInfo& field_info, uint16_t type) {
  FieldReference key = { field_info.DeclaringDexFile(), field_info.DeclaringFieldIndex(), type };
  auto it = field_index_map_.find(key);
  if (it != field_index_map_.end()) {
    return it->second;
  }
  DCHECK_LT(field_index_map_.size(), kNoValue);
  uint16_t id = field_index_map_.size();
  field_index_map_.Put(key, id);
  return id;
}

uint16_t GlobalValueNumbering::GetFieldType(uint16_t field_id) {
  for (auto it = field_index_map_.begin(); ; ++it) {
    DCHECK(it != field_index_map_.end()) << "Unknown field_id: " << field_id;
    if (it->second == field_id) {
      return it->first.type;
    }
  }
}

}  // namespace art
