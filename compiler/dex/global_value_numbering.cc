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
      repeat_count_(0u),
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
      merge_lvns_(allocator->Adapter()) {
  cu_->mir_graph->ClearAllVisitedFlags();
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
  if (bb->block_type == kEntryBlock) {
    repeat_count_ += 1u;
    if (repeat_count_ > kMaxRepeatCount) {
      last_value_ = kNoValue;  // Make bad.
      return nullptr;
    }
  }
  if (bb->block_type == kExitBlock) {
    DCHECK(bb->first_mir_insn == nullptr);
    return nullptr;
  }
  if (bb->visited) {
    return nullptr;
  }
  DCHECK(work_lvn_.get() == nullptr);
  work_lvn_.reset(new (allocator_) LocalValueNumbering(this, bb->id));
  if (bb->block_type == kEntryBlock) {
    if ((cu_->access_flags & kAccStatic) == 0) {
      // If non-static method, mark "this" as non-null
      int this_reg = cu_->num_dalvik_registers - cu_->num_ins;
      work_lvn_->SetSRegNullChecked(this_reg);
    }
  } else {
    // If non-clobbered catch optimization is disabled, consider every catch entry clobbered,
    // otherwise look at each predecessor's throwing insn.
    bool clobbered_catch = bb->catch_entry &&
        (cu_->disable_opt & (1 << kGlobalValueNumberingNonClobberedCatch)) != 0;
    // Merge all incoming arcs.
    // To avoid repeated allocation on the ArenaStack, reuse a single vector kept as a member.
    DCHECK(merge_lvns_.empty());
    GrowableArray<BasicBlockId>::Iterator iter(bb->predecessors);
    for (BasicBlock* pred_bb = cu_->mir_graph->GetBasicBlock(iter.Next());
         pred_bb != nullptr; pred_bb = cu_->mir_graph->GetBasicBlock(iter.Next())) {
      if (lvns_[pred_bb->id] != nullptr) {
        merge_lvns_.push_back(lvns_[pred_bb->id]);
      }
      if (bb->catch_entry && !clobbered_catch && IsThrowingInsnClobberring(pred_bb)) {
        clobbered_catch = true;
      }
    }
    // Determine merge type.
    LocalValueNumbering::MergeType merge_type = LocalValueNumbering::kNormalMerge;
    if (clobbered_catch) {
      merge_type = LocalValueNumbering::kClobberedCatchMerge;
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
      change = (lvns_[bb->id] == nullptr || !lvns_[bb->id]->Equals(*work_lvn_));
      break;
    }
  }

  std::unique_ptr<const LocalValueNumbering> old_lvn(lvns_[bb->id]);
  lvns_[bb->id] = work_lvn_.release();

  bb->visited = true;
  if (change) {
    ChildBlockIterator iter(bb, cu_->mir_graph.get());
    for (BasicBlock* child = iter.Next(); child != nullptr; child = iter.Next()) {
      child->visited = false;
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
  field_index_map_.PutHint(lb, key, id);
  // The new value was inserted before lb; lb remains valid across the PutHint().
  DCHECK(lb != field_index_map_.begin());
  --lb;
  DCHECK(lb->first.dex_file == key.dex_file && lb->first.field_idx == key.field_idx);
  field_index_reverse_map_.push_back(&*lb);
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
  array_location_map_.PutHint(lb, key, location);
  // The new value was inserted before lb; lb remains valid across the PutHint().
  DCHECK(lb != array_location_map_.begin());
  --lb;
  DCHECK(!cmp(lb->first, key) && !cmp(key, lb->first));
  array_location_reverse_map_.push_back(&*lb);
  return location;
}

bool GlobalValueNumbering::IsThrowingInsnClobberring(const BasicBlock* pred_bb) const {
  DCHECK(pred_bb->fall_through != kNullBlock);
  DCHECK(pred_bb->taken == kNullBlock);
  BasicBlock* succ_bb = cu_->mir_graph->GetBasicBlock(pred_bb->fall_through);
  DCHECK(succ_bb != nullptr);
  MIR* mir = succ_bb->first_mir_insn;
  DCHECK(mir != nullptr);
  // There was initially a throwing insn but it could have been optimized away
  // or replaced with a non-throwing insn by the inliner.
  if (static_cast<int>(mir->dalvikInsn.opcode) == kMirOpNop) {
    return false;
  }
  DCHECK(!MIRGraph::IsPseudoMirOp(mir->dalvikInsn.opcode));
  if ((Instruction::FlagsOf(mir->dalvikInsn.opcode) & Instruction::kThrow) == 0) {
    // This should be an inlined CONST/MOVE.
    return false;
  }
  switch (mir->dalvikInsn.opcode) {
    case Instruction::CONST_STRING:
    case Instruction::CONST_STRING_JUMBO:
    case Instruction::CONST_CLASS:
      // These calls to the runtime cannot modify any location the GVN tracks.
      return true;

    case Instruction::MONITOR_ENTER:
    case Instruction::MONITOR_EXIT:
      // If MONITOR_ENTER/MONITOR_EXIT throws it has no side effect in any location the GVN tracks.
      return false;

    case Instruction::CHECK_CAST:
    case Instruction::INSTANCE_OF:
    case Instruction::ARRAY_LENGTH:
    case Instruction::NEW_INSTANCE:
    case Instruction::NEW_ARRAY:
    case Instruction::FILLED_NEW_ARRAY:
    case Instruction::FILLED_NEW_ARRAY_RANGE:
      // No side-effects on throw.
      return false;

    case Instruction::THROW:
      // The THROW call to the runtime cannot modify any location the GVN tracks.
      // NOTE: We assign a new value to MOVE_EXCEPTION even if it would catch the thrown object.
      return false;

    case Instruction::AGET:
    case Instruction::AGET_WIDE:
    case Instruction::AGET_OBJECT:
    case Instruction::AGET_BOOLEAN:
    case Instruction::AGET_BYTE:
    case Instruction::AGET_CHAR:
    case Instruction::AGET_SHORT:
    case Instruction::APUT:
    case Instruction::APUT_WIDE:
    case Instruction::APUT_OBJECT:
    case Instruction::APUT_BOOLEAN:
    case Instruction::APUT_BYTE:
    case Instruction::APUT_CHAR:
    case Instruction::APUT_SHORT:
    case Instruction::IGET:
    case Instruction::IGET_WIDE:
    case Instruction::IGET_OBJECT:
    case Instruction::IGET_BOOLEAN:
    case Instruction::IGET_BYTE:
    case Instruction::IGET_CHAR:
    case Instruction::IGET_SHORT:
    case Instruction::IPUT:
    case Instruction::IPUT_WIDE:
    case Instruction::IPUT_OBJECT:
    case Instruction::IPUT_BOOLEAN:
    case Instruction::IPUT_BYTE:
    case Instruction::IPUT_CHAR:
    case Instruction::IPUT_SHORT:
      // No side-effects on throw.
      return false;

    case Instruction::SGET:
    case Instruction::SGET_WIDE:
    case Instruction::SGET_OBJECT:
    case Instruction::SGET_BOOLEAN:
    case Instruction::SGET_BYTE:
    case Instruction::SGET_CHAR:
    case Instruction::SGET_SHORT:
    case Instruction::SPUT:
    case Instruction::SPUT_WIDE:
    case Instruction::SPUT_OBJECT:
    case Instruction::SPUT_BOOLEAN:
    case Instruction::SPUT_BYTE:
    case Instruction::SPUT_CHAR:
    case Instruction::SPUT_SHORT:
        // Check if the mir can call class initializer. Otherwise it can't even throw.
        return !cu_->mir_graph->GetSFieldLoweringInfo(mir).IsInitialized() &&
            (mir->optimization_flags & MIR_IGNORE_CLINIT_CHECK) == 0;

    case Instruction::INVOKE_VIRTUAL:
    case Instruction::INVOKE_SUPER:
    case Instruction::INVOKE_DIRECT:
    case Instruction::INVOKE_STATIC:
    case Instruction::INVOKE_INTERFACE:
    case Instruction::INVOKE_VIRTUAL_RANGE:
    case Instruction::INVOKE_SUPER_RANGE:
    case Instruction::INVOKE_DIRECT_RANGE:
    case Instruction::INVOKE_STATIC_RANGE:
    case Instruction::INVOKE_INTERFACE_RANGE:
      return true;

    case Instruction::DIV_INT:
    case Instruction::REM_INT:
    case Instruction::DIV_LONG:
    case Instruction::REM_LONG:
    case Instruction::DIV_INT_2ADDR:
    case Instruction::REM_INT_2ADDR:
    case Instruction::DIV_LONG_2ADDR:
    case Instruction::REM_LONG_2ADDR:
    case Instruction::DIV_INT_LIT16:
    case Instruction::REM_INT_LIT16:
    case Instruction::DIV_INT_LIT8:
    case Instruction::REM_INT_LIT8:
      // No side-effects on throw.
      return false;

    case Instruction::FILL_ARRAY_DATA:
      DCHECK(false) << "Verifier error: FILL_ARRAY_DATA within reachable code should be rejected.";
      // Intentional fall-through.
    case Instruction::IGET_QUICK:
    case Instruction::IGET_WIDE_QUICK:
    case Instruction::IGET_OBJECT_QUICK:
    case Instruction::IPUT_QUICK:
    case Instruction::IPUT_WIDE_QUICK:
    case Instruction::IPUT_OBJECT_QUICK:
    case Instruction::INVOKE_VIRTUAL_QUICK:
    case Instruction::INVOKE_VIRTUAL_RANGE_QUICK:
      DCHECK(false) << "Verifier/compiler error: quickened insns should be rejected for compiler.";
      // Intentional fall-through.
    default:
      LOG(FATAL) << "Unexpected opcode: " << mir->dalvikInsn.opcode;
      return false;
  }
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
