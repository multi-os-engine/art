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
      field_index_reverse_map_(allocator->Adapter()),
      sreg_value_map_(std::less<uint16_t>(), allocator->Adapter()),
      sreg_wide_value_map_(std::less<uint16_t>(), allocator->Adapter()),
      ref_set_map_(std::less<ValueNameSet>(), allocator->Adapter()),
      lvns_(cu_->mir_graph->GetNumBlocks(), nullptr, allocator->Adapter()),
      work_lvn_(nullptr),
      merge_lvns_(allocator->Adapter()),
      change_(false) {
}

GlobalValueNumbering::~GlobalValueNumbering() {
  STLDeleteElements(&lvns_);
}

LocalValueNumbering* GlobalValueNumbering::PrepareBasicBlock(BasicBlock* bb) {
  if (UNLIKELY(!Good())) {
    return nullptr;
  }
  if (bb->data_flow_info == nullptr) {
    return nullptr;
  }
  DCHECK(!change_);
  DCHECK(work_lvn_.get() == nullptr);
  work_lvn_.reset(new (allocator_) LocalValueNumbering(this, bb->id));
  if (bb->block_type == kEntryBlock || bb->catch_entry) {
    if (bb->catch_entry) {
      work_lvn_->SetCatchEntry();
    }
    if ((cu_->access_flags & kAccStatic) == 0) {
      // If non-static method, mark "this" as non-null
      int this_reg = cu_->num_dalvik_registers - cu_->num_ins;
      uint16_t value_name = GetOperandValue(this_reg);
      work_lvn_->SetValueNullChecked(value_name);
    }
  } else if (bb->predecessors->Size() == 1u) {
    BasicBlock* pred_bb = cu_->mir_graph->GetBasicBlock(bb->predecessors->Get(0));
    const LocalValueNumbering* src = lvns_[pred_bb->id];
    DCHECK(src != nullptr);  // The predecessor must have already been processed at least once.
    work_lvn_->Copy(*src);
    if (HasNullCheckLastInsn(pred_bb, bb->id)) {
      uint16_t value_name = GetOperandValue(pred_bb->last_mir_insn->ssa_rep->uses[0]);
      work_lvn_->SetValueNullChecked(value_name);
    }
  } else {
    // Merge all incoming arcs.
    // To avoid repeated allocation on the ArenaStack, reuse a single vector kept as a member.
    DCHECK(merge_lvns_.empty());
    GrowableArray<BasicBlockId>::Iterator iter(bb->predecessors);
    for (BasicBlock* pred_bb = cu_->mir_graph->GetBasicBlock(iter.Next());
         pred_bb != nullptr; pred_bb = cu_->mir_graph->GetBasicBlock(iter.Next())) {
      if (lvns_[pred_bb->id] != nullptr) {
        merge_lvns_.push_back(lvns_[pred_bb->id]);
      }
    }
    // At least one predecessor must have been processed before this bb.
    CHECK(!merge_lvns_.empty());
    if (merge_lvns_.size() == 1u) {
      work_lvn_->Copy(*merge_lvns_[0]);
    } else {
      work_lvn_->Merge();
    }
  }
  return work_lvn_.get();
}

bool GlobalValueNumbering::FinishBasicBlock(BasicBlock* bb) {
  DCHECK(work_lvn_ != nullptr);
  DCHECK(bb->id == work_lvn_->Id());
  merge_lvns_.clear();

  bool change = false;
  // Look for a branch to self or an already processed child.
  // (No need to repeat the LVN if all children are processed later.)
  ChildBlockIterator iter(bb, cu_->mir_graph.get());
  for (BasicBlock* child = iter.Next(); child != nullptr; child = iter.Next()) {
    if (child == bb || lvns_[child->id] != nullptr) {
      // If we found an already processed child, check if the LVN actually differs.
      change = (change_ || lvns_[bb->id] == nullptr || !lvns_[bb->id]->Equals(*work_lvn_));
      break;
    }
  }

  std::unique_ptr<const LocalValueNumbering> old_lvn(lvns_[bb->id]);
  lvns_[bb->id] = work_lvn_.release();

  change_ = false;  // Clear change_ flag.
  return change;
}

uint16_t GlobalValueNumbering::GetFieldId(const MirFieldInfo& field_info, uint16_t type) {
  FieldReference key = { field_info.DeclaringDexFile(), field_info.DeclaringFieldIndex(), type };
  auto lb = field_index_map_.lower_bound(key);
  if (lb != field_index_map_.end() && !FieldReferenceComparator()(key, lb->first)) {
    return lb->second;
  }
  DCHECK_LT(field_index_map_.size(), kNoValue);
  uint16_t id = field_index_map_.size();
  field_index_map_.PutHint(lb, key, id);
  // The new value was inserted before lb; lb remains valid across the PutHint().
  DCHECK(lb != field_index_map_.begin());
  --lb;
  DCHECK(lb->first.dex_file == key.dex_file && lb->first.field_idx == key.field_idx);
  field_index_reverse_map_.push_back(&*lb);
  return id;
}

uint16_t GlobalValueNumbering::GetFieldType(uint16_t field_id) {
  DCHECK_LT(field_id, field_index_reverse_map_.size());
  return field_index_reverse_map_[field_id]->first.type;
}

bool GlobalValueNumbering::HasNullCheckLastInsn(const BasicBlock* pred_bb,
                                                BasicBlockId succ_id) const {
  if (pred_bb->block_type != kDalvikByteCode || pred_bb->last_mir_insn == nullptr) {
    return false;
  }
  Instruction::Code last_opcode = pred_bb->last_mir_insn->dalvikInsn.opcode;
  return ((last_opcode == Instruction::IF_EQZ && pred_bb->fall_through == succ_id) ||
      (last_opcode == Instruction::IF_NEZ && pred_bb->taken == succ_id));
}

bool GlobalValueNumbering::NullCheckedInAllPredecessors(
    const ScopedArenaVector<uint16_t>& merge_names) const {
  // Implicit parameters:
  //   - *work_lvn: the LVN for which we're checking predecessors.
  //   - merge_lvns_: the predecessor LVNs.
  DCHECK_EQ(merge_lvns_.size(), merge_names.size());
  for (size_t i = 0, size = merge_lvns_.size(); i != size; ++i) {
    const LocalValueNumbering* pred_lvn = merge_lvns_[i];
    uint16_t value_name = merge_names[i];
    if (!pred_lvn->IsValueNullChecked(value_name)) {
      // Check if the predecessor has an IF_EQZ/IF_NEZ as the last insn.
      const BasicBlock* pred_bb = cu_->mir_graph->GetBasicBlock(pred_lvn->Id());
      if (!HasNullCheckLastInsn(pred_bb, work_lvn_->Id())) {
        return false;
      }
      // IF_EQZ/IF_NEZ checks some sreg, see if that sreg contains the value_name.
      int s_reg = pred_bb->last_mir_insn->ssa_rep->uses[0];
      auto it = sreg_value_map_.find(s_reg);
      if (it == sreg_value_map_.end() || it->second != value_name) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace art
