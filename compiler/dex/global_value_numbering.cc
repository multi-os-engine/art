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
      topological_order_(nullptr),
      topological_order_indexes_(allocator->Adapter()),
      last_back_edge_indexes_(allocator->Adapter()),
      loop_repeat_ranges_(allocator->Adapter()),
      current_idx_(0u),
      end_idx_(0u),
      bbs_processed_(0u),
      max_bbs_to_process_(kMaxBbsToProcessMultiplyFactor * cu_->mir_graph->GetNumReachableBlocks()),
      last_value_(0u),
      modifications_allowed_(false),
      global_value_map_(std::less<uint64_t>(), allocator->Adapter()),
      field_index_map_(FieldReferenceComparator(), allocator->Adapter()),
      field_index_reverse_map_(allocator->Adapter()),
      array_location_map_(ArrayLocationComparator(), allocator->Adapter()),
      array_location_reverse_map_(allocator->Adapter()),
      ref_set_map_(std::less<ValueNameSet>(), allocator->Adapter()),
      lvns_(cu_->mir_graph->GetNumBlocks(), nullptr, allocator->Adapter()),
      work_lvn_(nullptr),
      work_lvn_uses_internal_ordering_(false),
      merge_lvns_(allocator->Adapter()) {
  // If we're actually running GVN (rather than LVN), prepare data for correct ordering.
  if ((cu_->disable_opt & (1u << kGlobalValueNumbering)) == 0u) {
    topological_order_ = cu_->mir_graph->GetTopologicalSortOrder();
    const size_t size = topological_order_->Size();
    DCHECK_NE(size, 0u);
    end_idx_ = size;
    topological_order_indexes_.resize(cu_->mir_graph->GetNumBlocks(), static_cast<size_t>(-1));
    for (size_t i = 0u; i != size; ++i) {
      topological_order_indexes_[topological_order_->Get(i)] = i;
    }
    last_back_edge_indexes_.reserve(size);
    for (size_t i = 0u; i != size; ++i) {
      last_back_edge_indexes_.push_back(i);
      BasicBlock* bb = cu_->mir_graph->GetBasicBlock(topological_order_->Get(i));
      ChildBlockIterator iter(bb, cu_->mir_graph.get());
      for (BasicBlock* child_bb = iter.Next(); child_bb != nullptr; child_bb = iter.Next()) {
        size_t pred_idx = topological_order_indexes_[child_bb->id];
        if (pred_idx < i) {
          last_back_edge_indexes_[pred_idx] = i;
        }
      }
    }
    cu_->mir_graph->ClearAllVisitedFlags();
    DCHECK(cu_->mir_graph->GetBasicBlock(topological_order_->Get(0))->data_flow_info != nullptr);
  }
}

GlobalValueNumbering::~GlobalValueNumbering() {
  STLDeleteElements(&lvns_);
}

LocalValueNumbering* GlobalValueNumbering::PrepareNextBasicBlock() {
  DCHECK_EQ((cu_->disable_opt & (1u << kGlobalValueNumbering)), 0u);
  if (current_idx_ != end_idx_) {
    BasicBlock* bb = cu_->mir_graph->GetBasicBlock(topological_order_->Get(current_idx_));
    DCHECK(!bb->visited);
    return DoPrepareBasicBlock(bb, true);
  }
  return nullptr;
}

LocalValueNumbering* GlobalValueNumbering::PrepareBasicBlock(BasicBlock* bb) {
  return DoPrepareBasicBlock(bb, false);
}

LocalValueNumbering* GlobalValueNumbering::DoPrepareBasicBlock(BasicBlock* bb,
                                                               bool internal_ordering) {
  DCHECK(bb->data_flow_info != nullptr);
  if (UNLIKELY(!Good())) {
    return nullptr;
  }
  if (UNLIKELY(bbs_processed_ == max_bbs_to_process_)) {
    last_value_ = kNoValue;  // Make bad.
    return nullptr;
  }
  DCHECK(work_lvn_.get() == nullptr);
  work_lvn_.reset(new (allocator_) LocalValueNumbering(this, bb->id));
  work_lvn_uses_internal_ordering_ = internal_ordering;
  if (bb->block_type == kExitBlock) {
    // No instructions in the exit block. Don't merge anything.
    DCHECK(bb->first_mir_insn == nullptr);
  } else if (bb->block_type == kEntryBlock) {
    if ((cu_->access_flags & kAccStatic) == 0) {
      // If non-static method, mark "this" as non-null
      int this_reg = cu_->num_dalvik_registers - cu_->num_ins;
      work_lvn_->SetSRegNullChecked(this_reg);
    }
  } else {
    // Merge all incoming arcs.
    // To avoid repeated allocation on the ArenaStack, reuse a single vector kept as a member.
    DCHECK(merge_lvns_.empty());
    // When we encounter the head of an inner loop for the first time during recalculation
    // of an outer loop, we must not take the inner loop's body into account. Therefore use all
    // predecessors only if we're at the head of the current loop or in the odd situations when
    // the last predecessor is at the end (==) or beyond (>) the current loop. This takes into
    // account SSA graphs that have a last node of a loop with two back-egdes (==) or two
    // outright overlapping loops (>).
    size_t idx = topological_order_indexes_[bb->id];
    bool use_all_predecessors = (!internal_ordering) ||
        (!loop_repeat_ranges_.empty() &&
         (loop_repeat_ranges_.back().first == idx ||
          loop_repeat_ranges_.back().second <= last_back_edge_indexes_[idx]));
    GrowableArray<BasicBlockId>::Iterator iter(bb->predecessors);
    for (BasicBlock* pred_bb = cu_->mir_graph->GetBasicBlock(iter.Next());
         pred_bb != nullptr; pred_bb = cu_->mir_graph->GetBasicBlock(iter.Next())) {
      if (lvns_[pred_bb->id] != nullptr &&
          (use_all_predecessors || topological_order_indexes_[pred_bb->id] < idx)) {
        merge_lvns_.push_back(lvns_[pred_bb->id]);
      }
    }
    // Determine merge type.
    LocalValueNumbering::MergeType merge_type = LocalValueNumbering::kNormalMerge;
    if (bb->catch_entry) {
      merge_type = LocalValueNumbering::kCatchMerge;
    } else if (bb->last_mir_insn != nullptr &&
        (bb->last_mir_insn->dalvikInsn.opcode == Instruction::RETURN ||
         bb->last_mir_insn->dalvikInsn.opcode == Instruction::RETURN_OBJECT ||
         bb->last_mir_insn->dalvikInsn.opcode == Instruction::RETURN_WIDE) &&
        (bb->first_mir_insn == bb->last_mir_insn ||
         (bb->first_mir_insn->next == bb->last_mir_insn &&
          static_cast<int>(bb->first_mir_insn->dalvikInsn.opcode) == kMirOpPhi))) {
      merge_type = LocalValueNumbering::kReturnMerge;
    }
    // At least one predecessor must have been processed before this bb.
    CHECK(!merge_lvns_.empty());
    if (merge_lvns_.size() == 1u) {
      work_lvn_->MergeOne(*merge_lvns_[0], merge_type);
      BasicBlock* pred_bb = cu_->mir_graph->GetBasicBlock(merge_lvns_[0]->Id());
      if (HasNullCheckLastInsn(pred_bb, bb->id)) {
        work_lvn_->SetSRegNullChecked(pred_bb->last_mir_insn->ssa_rep->uses[0]);
      }
    } else {
      work_lvn_->Merge(merge_type);
    }
  }
  return work_lvn_.get();
}

bool GlobalValueNumbering::FinishBasicBlock(LocalValueNumbering* lvn) {
  DCHECK(lvn != nullptr);
  DCHECK(work_lvn_.get() == lvn);
  ++bbs_processed_;
  merge_lvns_.clear();

  BasicBlock* bb = lvn->GetBasicBlock();
  bool change = false;
  // Look for a branch to self or an already processed child.
  // (No need to repeat the LVN if all children are processed later.)
  ChildBlockIterator iter(bb, cu_->mir_graph.get());
  for (BasicBlock* child = iter.Next(); child != nullptr; child = iter.Next()) {
    if (child == bb || lvns_[child->id] != nullptr) {
      // If we found an already processed child, check if the LVN actually differs.
      change = (lvns_[bb->id] == nullptr || !lvns_[bb->id]->Equals(*work_lvn_));
      break;
    }
  }

  std::unique_ptr<const LocalValueNumbering> old_lvn(lvns_[bb->id]);
  lvns_[bb->id] = work_lvn_.release();

  if (work_lvn_uses_internal_ordering_) {
    // Find the next basic block in the internal ordering.
    bb->visited = true;
    while (!loop_repeat_ranges_.empty() &&
        loop_repeat_ranges_.back().second == current_idx_) {
      loop_repeat_ranges_.pop_back();
    }
    size_t next_idx = current_idx_ + 1u;
    BasicBlock* last_bb = cu_->mir_graph->GetBasicBlock(topological_order_->Get(current_idx_));
    ChildBlockIterator iter(last_bb, cu_->mir_graph.get());
    for (BasicBlock* child_bb = iter.Next(); child_bb != nullptr; child_bb = iter.Next()) {
      if (change) {
        child_bb->visited = false;
      }
      size_t child_idx = topological_order_indexes_[child_bb->id];
      if (!child_bb->visited &&
          child_idx < next_idx && last_back_edge_indexes_[child_idx] == current_idx_) {
        // Rerun the loop in range [child_idx, current_idx_].
        next_idx = child_idx;
      }
    }
    if (next_idx <= current_idx_) {
      loop_repeat_ranges_.push_back(std::make_pair(next_idx, current_idx_));
    }
    for (current_idx_ = next_idx; current_idx_ != end_idx_; ++current_idx_) {
      BasicBlock* next_bb = cu_->mir_graph->GetBasicBlock(topological_order_->Get(current_idx_));
      if (next_bb->data_flow_info != nullptr && !next_bb->visited) {
        break;
      }
    }
  }

  return change;
}

uint16_t GlobalValueNumbering::GetFieldId(const MirFieldInfo& field_info, uint16_t type) {
  FieldReference key = { field_info.DeclaringDexFile(), field_info.DeclaringFieldIndex(), type };
  auto lb = field_index_map_.lower_bound(key);
  if (lb != field_index_map_.end() && !field_index_map_.key_comp()(key, lb->first)) {
    return lb->second;
  }
  DCHECK_LT(field_index_map_.size(), kNoValue);
  uint16_t id = field_index_map_.size();
  auto it = field_index_map_.PutBefore(lb, key, id);
  field_index_reverse_map_.push_back(&*it);
  return id;
}

uint16_t GlobalValueNumbering::GetArrayLocation(uint16_t base, uint16_t index) {
  auto cmp = array_location_map_.key_comp();
  ArrayLocation key = { base, index };
  auto lb = array_location_map_.lower_bound(key);
  if (lb != array_location_map_.end() && !cmp(key, lb->first)) {
    return lb->second;
  }
  uint16_t location = static_cast<uint16_t>(array_location_reverse_map_.size());
  DCHECK_EQ(location, array_location_reverse_map_.size());  // No overflow.
  auto it = array_location_map_.PutBefore(lb, key, location);
  array_location_reverse_map_.push_back(&*it);
  return location;
}

bool GlobalValueNumbering::HasNullCheckLastInsn(const BasicBlock* pred_bb,
                                                BasicBlockId succ_id) {
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
      if (!pred_lvn->IsSregValue(s_reg, value_name)) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace art
