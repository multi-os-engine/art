/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include "type_inference.h"

#include "base/bit_vector-inl.h"
#include "compiler_ir.h"
#include "dataflow_iterator-inl.h"
#include "dex_flags.h"
#include "dex_file-inl.h"
#include "driver/dex_compilation_unit.h"
#include "mir_field_info.h"
#include "mir_graph.h"
#include "mir_method_info.h"

namespace art {

TypeInference::Type TypeInference::Type::ShortyType(char shorty) {
  switch (shorty) {
    case 'L':
      return Type(kFlagLowWord | kFlagNarrow | kFlagRef);
    case 'D':
      return Type(kFlagLowWord | kFlagWide | kFlagFp);
    case 'J':
      return Type(kFlagLowWord | kFlagWide | kFlagCore);
    case 'F':
      return Type(kFlagLowWord | kFlagNarrow | kFlagFp);
    default:
      DCHECK(shorty == 'I' || shorty == 'S' || shorty == 'C' || shorty == 'B' || shorty == 'Z');
      return Type(kFlagLowWord | kFlagNarrow | kFlagCore);
  }
}

TypeInference::Type TypeInference::Type::DexType(const DexFile* dex_file, uint32_t type_idx) {
  const char* desc = dex_file->GetTypeDescriptor(dex_file->GetTypeId(type_idx));
  if (UNLIKELY(desc[0] == 'V')) {
    return Uninitialized();
  } else if (UNLIKELY(desc[0] == '[')) {
    size_t array_depth = 0u;
    while (*desc == '[') {
      ++array_depth;
      ++desc;
    }
    Type shorty_result = Type::ShortyType(desc[0]);
    return ArrayType(array_depth, shorty_result);
  } else {
    return ShortyType(desc[0]);
  }
}

TypeInference::CheckCastData::CheckCastData(MIRGraph* mir_graph, ScopedArenaAllocator* alloc)
    : mir_graph_(mir_graph),
      alloc_(alloc),
      num_blocks_(mir_graph->GetNumBlocks()),
      num_sregs_(mir_graph->GetNumSSARegs()),
      check_cast_map_(std::less<MIR*>(), alloc->Adapter()),
      split_sreg_data_(std::less<int32_t>(), alloc->Adapter()) {
}

void TypeInference::CheckCastData::AddCheckCast(MIR* check_cast, Type type) {
  DCHECK_EQ(check_cast->dalvikInsn.opcode, Instruction::CHECK_CAST);
  type.CheckPureRef();
  int32_t extra_s_reg = static_cast<int32_t>(num_sregs_);
  num_sregs_ += 1;
  check_cast_map_.Put(check_cast, std::make_pair(extra_s_reg, type));
  int32_t s_reg = check_cast->ssa_rep->uses[0];
  auto lb = split_sreg_data_.lower_bound(s_reg);
  if (lb == split_sreg_data_.end() || split_sreg_data_.key_comp()(s_reg, lb->first)) {
    SplitSRegData split_s_reg_data = {
        0,
        alloc_->AllocArray<int32_t>(num_blocks_, kArenaAllocMisc),
        alloc_->AllocArray<int32_t>(num_blocks_, kArenaAllocMisc),
        new (alloc_) ArenaBitVector(alloc_, num_blocks_, false)
    };
    std::fill_n(split_s_reg_data.starting_mod_s_reg, num_blocks_, INVALID_SREG);
    std::fill_n(split_s_reg_data.ending_mod_s_reg, num_blocks_, INVALID_SREG);
    split_s_reg_data.def_phi_blocks_->ClearAllBits();
    BasicBlock* def_bb = FindDefBlock(check_cast);
    split_s_reg_data.ending_mod_s_reg[def_bb->id] = s_reg;
    split_s_reg_data.def_phi_blocks_->SetBit(def_bb->id);
    lb = split_sreg_data_.PutBefore(lb, s_reg, split_s_reg_data);
  }
  lb->second.ending_mod_s_reg[check_cast->bb] = extra_s_reg;
  lb->second.def_phi_blocks_->SetBit(check_cast->bb);
}

void TypeInference::CheckCastData::AddPseudoPhis() {
  // Look for pseudo-phis where a split SSA reg merges with a differently typed version
  // and initialize all starting_mod_s_reg.
  DCHECK(!split_sreg_data_.empty());
  ArenaBitVector* phi_blocks = new (alloc_) ArenaBitVector(alloc_, num_blocks_, false);

  for (auto& entry : split_sreg_data_) {
    SplitSRegData& data = entry.second;

    // Find pseudo-phi nodes.
    phi_blocks->ClearAllBits();
    ArenaBitVector* input_blocks = data.def_phi_blocks_;
    do {
      for (uint32_t idx : input_blocks->Indexes()) {
        BasicBlock* def_bb = mir_graph_->GetBasicBlock(idx);
        if (def_bb->dom_frontier != nullptr) {
          phi_blocks->Union(def_bb->dom_frontier);
        }
      }
    } while (input_blocks->Union(phi_blocks));

    // Find live pseudo-phis. Make sure they're merging the same SSA reg.
    data.def_phi_blocks_->ClearAllBits();
    int32_t s_reg = entry.first;
    int v_reg = mir_graph_->SRegToVReg(s_reg);
    for (uint32_t phi_bb_id : phi_blocks->Indexes()) {
      BasicBlock* phi_bb = mir_graph_->GetBasicBlock(phi_bb_id);
      DCHECK(phi_bb != nullptr);
      DCHECK(phi_bb->data_flow_info != nullptr);
      DCHECK(phi_bb->data_flow_info->live_in_v != nullptr);
      if (IsSRegLiveAtStart(phi_bb, v_reg, s_reg)) {
        int32_t extra_s_reg = static_cast<int32_t>(num_sregs_);
        num_sregs_ += 1;
        data.starting_mod_s_reg[phi_bb_id] = extra_s_reg;
        data.def_phi_blocks_->SetBit(phi_bb_id);
      }
    }

    // SSA rename for s_reg.
    TopologicalSortIterator iter(mir_graph_);
    for (BasicBlock* bb = iter.Next(); bb != nullptr; bb = iter.Next()) {
      if (bb->data_flow_info == nullptr || bb->block_type == kEntryBlock) {
        continue;
      }
      BasicBlockId bb_id = bb->id;
      if (data.def_phi_blocks_->IsBitSet(bb_id)) {
        DCHECK_NE(data.starting_mod_s_reg[bb_id], INVALID_SREG);
      } else {
        DCHECK_EQ(data.starting_mod_s_reg[bb_id], INVALID_SREG);
        if (IsSRegLiveAtStart(bb, v_reg, s_reg)) {
          // The earliest predecessor must have been processed already.
          BasicBlock* pred_bb = FindTopologicallyEarliestPredecessor(bb);
          int32_t mod_s_reg = data.ending_mod_s_reg[pred_bb->id];
          data.starting_mod_s_reg[bb_id] = (mod_s_reg != INVALID_SREG) ? mod_s_reg : s_reg;
        } else if (data.ending_mod_s_reg[bb_id] != INVALID_SREG) {
          // Start the original defining block with s_reg.
          data.starting_mod_s_reg[bb_id] = s_reg;
        }
      }
      if (data.ending_mod_s_reg[bb_id] == INVALID_SREG) {
        // If the block doesn't define the modified SSA reg, it propagates the starting type.
        data.ending_mod_s_reg[bb_id] = data.starting_mod_s_reg[bb_id];
      }
    }
  }
}

void TypeInference::CheckCastData::InitializeCheckCastSRegs(Type* sregs) const {
  for (const auto& entry : check_cast_map_) {
    DCHECK_LT(static_cast<size_t>(entry.second.first), num_sregs_);
    sregs[entry.second.first] = entry.second.second;
  }
}

void TypeInference::CheckCastData::MergeCheckCastConflicts(Type* sregs) const {
  for (const auto& entry : check_cast_map_) {
    DCHECK_LT(static_cast<size_t>(entry.second.first), num_sregs_);
    sregs[entry.first->ssa_rep->uses[0]].MergeNonArrayFlags(sregs[entry.second.first]);
  }
}

void TypeInference::CheckCastData::Start(BasicBlock* bb) {
  for (auto& entry : split_sreg_data_) {
    entry.second.current_mod_s_reg = entry.second.starting_mod_s_reg[bb->id];
  }
}

bool TypeInference::CheckCastData::ProcessPseudoPhis(BasicBlock* bb, Type* sregs) {
  // If we're processing the initial merge of a loop head, merge only refs from
  // preceding blocks in topological sort order, otherwise merge all incoming refs.
  bool use_all_predecessors = true;
  uint16_t loop_head_idx = 0u;  // Used only if !use_all_predecessors.
  if (mir_graph_->GetTopologicalSortOrderLoopHeadStack()->size() != 0) {
    auto top = mir_graph_->GetTopologicalSortOrderLoopHeadStack()->back();
    loop_head_idx = top.first;
    bool recalculating = top.second;
    use_all_predecessors = recalculating ||
        loop_head_idx != mir_graph_->GetTopologicalSortOrderIndexes()[bb->id];
  }

  bool changed = false;
  for (auto& entry : split_sreg_data_) {
    DCHECK_EQ(entry.second.current_mod_s_reg, entry.second.starting_mod_s_reg[bb->id]);
    if (entry.second.def_phi_blocks_->IsBitSet(bb->id)) {
      int32_t* ending_mod_s_reg = entry.second.ending_mod_s_reg;
      Type merged_type = Type::Uninitialized();
      for (BasicBlockId pred_id : bb->predecessors) {
        if (use_all_predecessors ||
            mir_graph_->GetTopologicalSortOrderIndexes()[pred_id] < loop_head_idx) {
          DCHECK_LT(static_cast<size_t>(ending_mod_s_reg[pred_id]), num_sregs_);
          if (sregs[ending_mod_s_reg[pred_id]].Ref()) {
            merged_type.MergePureRefAndArray(sregs[ending_mod_s_reg[pred_id]]);
          }
        }
      }
      if (merged_type.Ref()) {
        merged_type.CheckPureRef();  // There must have been at least one predecessor to merge.
        changed |= sregs[entry.second.current_mod_s_reg].MergePureRefAndArray(merged_type);
      } else {
        // This can happen during an initial merge of a loop head if the original def is
        // actually an untyped null. (All other definitions are typed using the check-cast.)
        DCHECK(!use_all_predecessors);
      }
    }
  }
  return changed;
}

void TypeInference::CheckCastData::ProcessCheckCast(MIR* mir) {
  auto mir_it = check_cast_map_.find(mir);
  DCHECK(mir_it != check_cast_map_.end());
  auto split_it = split_sreg_data_.find(mir->ssa_rep->uses[0]);
  DCHECK(split_it != split_sreg_data_.end());
  split_it->second.current_mod_s_reg = mir_it->second.first;
}

TypeInference::SplitSRegData* TypeInference::CheckCastData::GetSplitSRegData(int32_t s_reg) {
  auto it = split_sreg_data_.find(s_reg);
  return (it == split_sreg_data_.end()) ? nullptr : &it->second;
}

BasicBlock* TypeInference::CheckCastData::FindDefBlock(MIR* check_cast) {
  // Find the initial definition of the SSA reg used by the check-cast.
  DCHECK_EQ(check_cast->dalvikInsn.opcode, Instruction::CHECK_CAST);
  int32_t s_reg = check_cast->ssa_rep->uses[0];
  if (mir_graph_->IsInVReg(s_reg)) {
    return mir_graph_->GetEntryBlock();
  }
  int v_reg = mir_graph_->SRegToVReg(s_reg);
  BasicBlock* bb = mir_graph_->GetBasicBlock(check_cast->bb);
  DCHECK(bb != nullptr);
  while (true) {
    // Find the earliest predecessor in the topological sort order to ensure we don't
    // go in a loop.
    BasicBlock* pred_bb = FindTopologicallyEarliestPredecessor(bb);
    DCHECK(pred_bb != nullptr);
    DCHECK(pred_bb->data_flow_info != nullptr);
    DCHECK(pred_bb->data_flow_info->vreg_to_ssa_map_exit != nullptr);
    if (pred_bb->data_flow_info->vreg_to_ssa_map_exit[v_reg] != s_reg) {
      // The s_reg was not valid at the end of pred_bb, so it must have been defined in bb.
      return bb;
    }
    bb = pred_bb;
  }
}

BasicBlock* TypeInference::CheckCastData::FindTopologicallyEarliestPredecessor(BasicBlock* bb) {
  DCHECK(!bb->predecessors.empty());
  const auto& indexes = mir_graph_->GetTopologicalSortOrderIndexes();
  DCHECK_LT(bb->id, indexes.size());
  size_t best_idx = indexes[bb->id];
  BasicBlockId best_id = NullBasicBlockId;
  for (BasicBlockId pred_id : bb->predecessors) {
    DCHECK_LT(pred_id, indexes.size());
    if (best_idx > indexes[pred_id]) {
      best_idx = indexes[pred_id];
      best_id = pred_id;
    }
  }
  // There must be at least one predecessor earlier than the bb.
  DCHECK_LT(best_idx, indexes[bb->id]);
  return mir_graph_->GetBasicBlock(best_id);
}

bool TypeInference::CheckCastData::IsSRegLiveAtStart(BasicBlock* bb, int v_reg, int32_t s_reg) {
  DCHECK_EQ(v_reg, mir_graph_->SRegToVReg(s_reg));
  DCHECK(bb != nullptr);
  DCHECK(bb->data_flow_info != nullptr);
  DCHECK(bb->data_flow_info->live_in_v != nullptr);
  if (!bb->data_flow_info->live_in_v->IsBitSet(v_reg)) {
    return false;
  }
  for (BasicBlockId pred_id : bb->predecessors) {
    BasicBlock* pred_bb = mir_graph_->GetBasicBlock(pred_id);
    DCHECK(pred_bb != nullptr);
    DCHECK(pred_bb->data_flow_info != nullptr);
    DCHECK(pred_bb->data_flow_info->vreg_to_ssa_map_exit != nullptr);
    if (pred_bb->data_flow_info->vreg_to_ssa_map_exit[v_reg] != s_reg) {
      return false;
    }
  }
  return true;
}

TypeInference::TypeInference(MIRGraph* mir_graph, ScopedArenaAllocator* alloc)
    : mir_graph_(mir_graph),
      cu_(mir_graph->GetCurrentDexCompilationUnit()->GetCompilationUnit()),
      check_cast_data_(InitializeCheckCastData(mir_graph, alloc)),
      num_sregs_(
          check_cast_data_ != nullptr ? check_cast_data_->NumSRegs() : mir_graph->GetNumSSARegs()),
      ifields_(PrepareIFieldTypes(cu_->dex_file, mir_graph, alloc)),
      sfields_(PrepareSFieldTypes(cu_->dex_file, mir_graph, alloc)),
      signatures_(PrepareSignatures(cu_->dex_file, mir_graph, alloc)),
      current_method_signature_(
          Signature(cu_->dex_file, cu_->method_idx, (cu_->access_flags & kAccStatic) != 0, alloc)),
      sregs_(alloc->AllocArray<Type>(num_sregs_, kArenaAllocMisc)) {
  InitializeSRegs();
  PropagateRefs();
}

bool TypeInference::Apply(BasicBlock* bb) {
  bool changed = false;
  if (UNLIKELY(check_cast_data_ != nullptr)) {
    check_cast_data_->Start(bb);
    // Don't process pseudo-phis, they now have their final values.
  }
  for (MIR* mir = bb->first_mir_insn; mir != nullptr; mir = mir->next) {
    if (InferTypeAndSize(bb, mir)) {
      changed = true;
    }
  }
  return changed;
}

void TypeInference::Finish() {
  if (UNLIKELY(check_cast_data_ != nullptr)) {
    check_cast_data_->MergeCheckCastConflicts(sregs_);
  }

  size_t num_sregs = mir_graph_->GetNumSSARegs();  // Without the extra SSA regs.
  for (size_t s_reg = 0; s_reg != num_sregs; ++s_reg) {
    if (sregs_[s_reg].SizeConflict()) {
      /*
       * The dex bytecode definition does not explicitly outlaw the definition of the same
       * virtual register to be used in both a 32-bit and 64-bit pair context.  However, dx
       * does not generate this pattern (at least recently).  Further, in the next revision of
       * dex, we will forbid this.  To support the few cases in the wild, detect this pattern
       * and punt to the interpreter.
       */
      LOG(WARNING) << PrettyMethod(cu_->method_idx, *cu_->dex_file)
                   << " has size conflict block for sreg " << s_reg
                   << ", punting to interpreter.";
      mir_graph_->PuntToInterpreter();
      return;
    }
  }

  size_t conflict_s_reg = 0;
  bool type_conflict = false;
  for (size_t s_reg = 0; s_reg != num_sregs; ++s_reg) {
    Type type = sregs_[s_reg];
    RegLocation* loc = &mir_graph_->reg_location_[s_reg];
    loc->wide = type.Wide();
    loc->defined = type.IsDefined();
    loc->fp = type.Fp();
    loc->core = type.Core();
    loc->ref = type.Ref();
    loc->high_word = type.HighWord();
    if (UNLIKELY(type.TypeConflict())) {
      type_conflict = true;
      conflict_s_reg = s_reg;
    }
  }

  if (type_conflict) {
    /*
     * We don't normally expect to see a Dalvik register definition used both as a
     * floating point and core value, though technically it could happen with constants.
     * Until we have proper typing, detect this situation and disable register promotion
     * (which relies on the distinction between core a fp usages).
     */
    LOG(WARNING) << PrettyMethod(cu_->method_idx, *cu_->dex_file)
                 << " has type conflict block for sreg " << conflict_s_reg
                 << ", disabling register promotion.";
    cu_->disable_opt |= (1 << kPromoteRegs);
  }
}

TypeInference::Type TypeInference::FieldType(const DexFile* dex_file, uint32_t field_idx) {
  uint32_t type_idx = dex_file->GetFieldId(field_idx).type_idx_;
  Type result = Type::DexType(dex_file, type_idx);
  return result;
}

TypeInference::Type* TypeInference::PrepareIFieldTypes(const DexFile* dex_file,
                                                       MIRGraph* mir_graph,
                                                       ScopedArenaAllocator* alloc) {
  size_t count = mir_graph->GetIFieldLoweringInfoCount();
  Type* ifields = alloc->AllocArray<Type>(count, kArenaAllocDFInfo);
  for (uint32_t i = 0u; i != count; ++i) {
    // NOTE: Quickened field accesses have invalid FieldIndex() but they are always resolved.
    const MirFieldInfo& info = mir_graph->GetIFieldLoweringInfo(i);
    const DexFile* current_dex_file = info.IsResolved() ? info.DeclaringDexFile() : dex_file;
    uint32_t field_idx = info.IsResolved() ? info.DeclaringFieldIndex() : info.FieldIndex();
    ifields[i] = FieldType(current_dex_file, field_idx);
    DCHECK_EQ(info.MemAccessType() == kDexMemAccessWide, ifields[i].Wide());
    DCHECK_EQ(info.MemAccessType() == kDexMemAccessObject, ifields[i].Ref());
  }
  return ifields;
}

TypeInference::Type* TypeInference::PrepareSFieldTypes(const DexFile* dex_file,
                                                       MIRGraph* mir_graph,
                                                       ScopedArenaAllocator* alloc) {
  size_t count = mir_graph->GetSFieldLoweringInfoCount();
  Type* sfields = alloc->AllocArray<Type>(count, kArenaAllocDFInfo);
  for (uint32_t i = 0u; i != count; ++i) {
    // FieldIndex() is always valid for static fields (no quickened instructions).
    sfields[i] = FieldType(dex_file, mir_graph->GetSFieldLoweringInfo(i).FieldIndex());
  }
  return sfields;
}

TypeInference::MethodSignature TypeInference::Signature(const DexFile* dex_file,
                                                        uint32_t method_idx,
                                                        bool is_static,
                                                        ScopedArenaAllocator* alloc) {
  const DexFile::MethodId& method_id = dex_file->GetMethodId(method_idx);
  const DexFile::ProtoId& proto_id = dex_file->GetMethodPrototype(method_id);
  Type return_type = Type::DexType(dex_file, proto_id.return_type_idx_);
  const DexFile::TypeList* type_list = dex_file->GetProtoParameters(proto_id);
  size_t this_size = (is_static ? 0u : 1u);
  size_t param_size = ((type_list != nullptr) ? type_list->Size() : 0u);
  size_t size = this_size + param_size;
  Type* param_types = (size != 0u) ? alloc->AllocArray<Type>(size, kArenaAllocDFInfo) : nullptr;
  if (!is_static) {
    param_types[0] = Type::DexType(dex_file, method_id.class_idx_);
  }
  for (size_t i = 0; i != param_size; ++i)  {
    uint32_t type_idx = type_list->GetTypeItem(i).type_idx_;
    param_types[this_size + i] = Type::DexType(dex_file, type_idx);
  }
  return MethodSignature{ return_type, size, param_types };  // NOLINT
}

TypeInference::MethodSignature* TypeInference::PrepareSignatures(const DexFile* dex_file,
                                                                 MIRGraph* mir_graph,
                                                                 ScopedArenaAllocator* alloc) {
  size_t count = mir_graph->GetMethodLoweringInfoCount();
  MethodSignature* signatures = alloc->AllocArray<MethodSignature>(count, kArenaAllocDFInfo);
  for (uint32_t i = 0u; i != count; ++i) {
    // NOTE: Quickened invokes have invalid MethodIndex() but they are always resolved.
    const MirMethodInfo& info = mir_graph->GetMethodLoweringInfo(i);
    uint32_t method_idx = info.IsResolved() ? info.DeclaringMethodIndex() : info.MethodIndex();
    const DexFile* current_dex_file = info.IsResolved() ? info.DeclaringDexFile() : dex_file;
    signatures[i] = Signature(current_dex_file, method_idx, info.IsStatic(), alloc);
  }
  return signatures;
}

TypeInference::CheckCastData* TypeInference::InitializeCheckCastData(MIRGraph* mir_graph,
                                                                     ScopedArenaAllocator* alloc) {
  if (!mir_graph->HasCheckCast()) {
    return nullptr;
  }

  CheckCastData* data = nullptr;
  const DexFile* dex_file = nullptr;
  PreOrderDfsIterator iter(mir_graph);
  for (BasicBlock* bb = iter.Next(); bb != nullptr; bb = iter.Next()) {
    for (MIR* mir = bb->first_mir_insn; mir != nullptr; mir = mir->next) {
      if (mir->dalvikInsn.opcode == Instruction::CHECK_CAST) {
        if (data == nullptr) {
          data = new (alloc) CheckCastData(mir_graph, alloc);
          dex_file = mir_graph->GetCurrentDexCompilationUnit()->GetCompilationUnit()->dex_file;
        }
        Type type = Type::DexType(dex_file, mir->dalvikInsn.vB);
        data->AddCheckCast(mir, type);
      }
    }
  }
  if (data != nullptr) {
    data->AddPseudoPhis();
  }
  return data;
}

void TypeInference::InitializeSRegs() {
  std::fill_n(sregs_, num_sregs_, Type::Uninitialized());

  // Initialize parameter SSA regs.
  int32_t param_s_reg = mir_graph_->GetFirstInVR();
  for (size_t i = 0, size = current_method_signature_.num_params; i != size; ++i)  {
    Type param_type = current_method_signature_.param_types[i];
    sregs_[param_s_reg] = param_type;
    param_s_reg += param_type.Wide() ? 2 : 1;
  }
  DCHECK_EQ(static_cast<uint32_t>(param_s_reg),
            mir_graph_->GetFirstInVR() + mir_graph_->GetNumOfInVRs());

  // Initialize check-cast types.
  if (UNLIKELY(check_cast_data_ != nullptr)) {
    check_cast_data_->InitializeCheckCastSRegs(sregs_);
  }

  // Initialize well-known SSA register definition types.
  PreOrderDfsIterator iter(mir_graph_);
  for (BasicBlock* bb = iter.Next(); bb != nullptr; bb = iter.Next()) {
    // Ignore pseudo-phis, we're not setting types for SSA regs that depend on them in this pass.
    for (MIR* mir = bb->first_mir_insn; mir != nullptr; mir = mir->next) {
      uint16_t opcode = mir->dalvikInsn.opcode;
      switch (opcode) {
        case Instruction::CONST_4:
        case Instruction::CONST_16:
        case Instruction::CONST:
        case Instruction::CONST_HIGH16:
        case Instruction::CONST_WIDE_16:
        case Instruction::CONST_WIDE_32:
        case Instruction::CONST_WIDE:
        case Instruction::CONST_WIDE_HIGH16:
        case Instruction::MOVE:
        case Instruction::MOVE_FROM16:
        case Instruction::MOVE_16:
        case Instruction::MOVE_WIDE:
        case Instruction::MOVE_WIDE_FROM16:
        case Instruction::MOVE_WIDE_16:
        case Instruction::MOVE_OBJECT:
        case Instruction::MOVE_OBJECT_FROM16:
        case Instruction::MOVE_OBJECT_16:
          if ((mir->optimization_flags & MIR_CALLEE) != 0) {
            // Inlined const/move keeps method_lowering_info for type inference.
            DCHECK_LT(mir->meta.method_lowering_info, mir_graph_->GetMethodLoweringInfoCount());
            Type return_type = signatures_[mir->meta.method_lowering_info].return_type;
            DCHECK(return_type.IsDefined());  // Method return type can't be void.
            sregs_[mir->ssa_rep->defs[0]] = return_type;
            if (return_type.Wide()) {
              DCHECK_EQ(mir->ssa_rep->defs[0] + 1, mir->ssa_rep->defs[1]);
              sregs_[mir->ssa_rep->defs[1]] = return_type.ToHighWord();
            }
            break;
          }
          FALLTHROUGH_INTENDED;
        case kMirOpPhi:
        case Instruction::AGET_OBJECT:
          // These cannot be determined in this simple pass and will be processed later.
          break;

        case Instruction::MOVE_RESULT_OBJECT:
          // Nothing to do, handled with invoke-* or filled-new-array/-range.
          break;
        case Instruction::MOVE_EXCEPTION:
          // NOTE: We can never catch an array.
          sregs_[mir->ssa_rep->defs[0]] = Type::NonArrayRefType();
          break;
        case Instruction::CONST_STRING:
        case Instruction::CONST_STRING_JUMBO:
          sregs_[mir->ssa_rep->defs[0]] = Type::NonArrayRefType();
          break;
        case Instruction::CONST_CLASS:
          sregs_[mir->ssa_rep->defs[0]] = Type::NonArrayRefType();
          break;
        case Instruction::CHECK_CAST:
          DCHECK(check_cast_data_ != nullptr);
          // NOTE: The extra SSA reg type has already been assigned in InitializeCheckCastSRegs().
          break;
        case Instruction::NEW_INSTANCE:
          sregs_[mir->ssa_rep->defs[0]] = Type::DexType(cu_->dex_file, mir->dalvikInsn.vB);
          DCHECK(sregs_[mir->ssa_rep->defs[0]].Ref());
          DCHECK_EQ(sregs_[mir->ssa_rep->defs[0]].ArrayDepth(), 0u);
          break;
        case Instruction::NEW_ARRAY:
          sregs_[mir->ssa_rep->defs[0]] = Type::DexType(cu_->dex_file, mir->dalvikInsn.vC);
          DCHECK(sregs_[mir->ssa_rep->defs[0]].Ref());
          DCHECK_NE(sregs_[mir->ssa_rep->defs[0]].ArrayDepth(), 0u);
          break;
        case Instruction::FILLED_NEW_ARRAY:
        case Instruction::FILLED_NEW_ARRAY_RANGE: {
          MIR* move_result_mir = mir_graph_->FindMoveResult(bb, mir);
          if (move_result_mir != nullptr) {
            DCHECK_EQ(move_result_mir->dalvikInsn.opcode, Instruction::MOVE_RESULT_OBJECT);
            Type array_type = Type::DexType(cu_->dex_file, mir->dalvikInsn.vB);
            array_type.CheckPureRef();  // Previously checked by the method verifier.
            DCHECK(!array_type.ComponentType().Wide());
            sregs_[move_result_mir->ssa_rep->defs[0]] = array_type;
          }
          break;
        }
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
        case Instruction::INVOKE_VIRTUAL_QUICK:
        case Instruction::INVOKE_VIRTUAL_RANGE_QUICK: {
          MIR* move_result_mir = mir_graph_->FindMoveResult(bb, mir);
          if (move_result_mir != nullptr) {
            Type return_type = signatures_[mir->meta.method_lowering_info].return_type;
            sregs_[move_result_mir->ssa_rep->defs[0]] = return_type;
            if (return_type.Wide()) {
              DCHECK_EQ(move_result_mir->ssa_rep->defs[0] + 1, move_result_mir->ssa_rep->defs[1]);
              sregs_[move_result_mir->ssa_rep->defs[1]] = return_type.ToHighWord();
            }
          }
          break;
        }

        case Instruction::IGET_WIDE:
        case Instruction::IGET_WIDE_QUICK:
          DCHECK_EQ(mir->ssa_rep->defs[0] + 1, mir->ssa_rep->defs[1]);
          sregs_[mir->ssa_rep->defs[1]] = ifields_[mir->meta.ifield_lowering_info].ToHighWord();
          FALLTHROUGH_INTENDED;
        case Instruction::IGET:
        case Instruction::IGET_OBJECT:
        case Instruction::IGET_BOOLEAN:
        case Instruction::IGET_BYTE:
        case Instruction::IGET_CHAR:
        case Instruction::IGET_SHORT:
        case Instruction::IGET_QUICK:
        case Instruction::IGET_OBJECT_QUICK:
        case Instruction::IGET_BOOLEAN_QUICK:
        case Instruction::IGET_BYTE_QUICK:
        case Instruction::IGET_CHAR_QUICK:
        case Instruction::IGET_SHORT_QUICK:
          sregs_[mir->ssa_rep->defs[0]] = ifields_[mir->meta.ifield_lowering_info];
          break;
        case Instruction::SGET_WIDE:
          DCHECK_EQ(mir->ssa_rep->defs[0] + 1, mir->ssa_rep->defs[1]);
          sregs_[mir->ssa_rep->defs[1]] = sfields_[mir->meta.sfield_lowering_info].ToHighWord();
          FALLTHROUGH_INTENDED;
        case Instruction::SGET:
        case Instruction::SGET_OBJECT:
        case Instruction::SGET_BOOLEAN:
        case Instruction::SGET_BYTE:
        case Instruction::SGET_CHAR:
        case Instruction::SGET_SHORT:
          sregs_[mir->ssa_rep->defs[0]] = sfields_[mir->meta.sfield_lowering_info];
          break;
        default:
          // No invokes or reference definitions here.
          DCHECK_EQ(MIRGraph::GetDataFlowAttributes(mir) & (DF_FORMAT_35C | DF_FORMAT_3RC), 0u);
          DCHECK_NE(MIRGraph::GetDataFlowAttributes(mir) & (DF_DA | DF_REF_A), (DF_DA | DF_REF_A));
          break;
      }
    }
  }
}

void TypeInference::PropagateRefs() {
  LoopRepeatingTopologicalSortIterator iter(mir_graph_);
  bool changed = false;
  for (BasicBlock* bb = iter.Next(changed); bb != nullptr; bb = iter.Next(changed)) {
    changed = PropagateRefs(bb);
  }
}

bool TypeInference::PropagateRefs(BasicBlock* bb) {
  // Iteratively determine reference types for SSA regs that cannot be determined
  // in a single pass. These are basically just phis, pseudo-phis, moves and aget-object.
  bool changed = false;
  if (UNLIKELY(check_cast_data_ != nullptr)) {
    check_cast_data_->Start(bb);
    changed |= check_cast_data_->ProcessPseudoPhis(bb, sregs_);
  }

  // For Phis, if we're processing the initial merge of a loop head, merge only refs from
  // preceding blocks in topological sort order, otherwise merge all incoming refs.
  bool use_all_predecessors = true;
  uint16_t loop_head_idx = 0u;  // Used only if !use_all_predecessors.
  if (mir_graph_->GetTopologicalSortOrderLoopHeadStack()->size() != 0) {
    auto top = mir_graph_->GetTopologicalSortOrderLoopHeadStack()->back();
    loop_head_idx = top.first;
    bool recalculating = top.second;
    use_all_predecessors = recalculating ||
        loop_head_idx != mir_graph_->GetTopologicalSortOrderIndexes()[bb->id];
  }

  for (MIR* mir = bb->first_mir_insn; mir != nullptr; mir = mir->next) {
    uint16_t opcode = mir->dalvikInsn.opcode;
    switch (opcode) {
      case kMirOpPhi: {
        Type merged_type = Type::Uninitialized();
        int32_t* uses = mir->ssa_rep->uses;
        for (size_t pred_idx = 0u, size = bb->predecessors.size(); pred_idx != size; ++pred_idx) {
          BasicBlockId pred_id = bb->predecessors[pred_idx];
          if (use_all_predecessors ||
              mir_graph_->GetTopologicalSortOrderIndexes()[pred_id] < loop_head_idx) {
            int32_t input_mod_s_reg = PhiInputModifiedSReg(uses[pred_idx], bb, pred_idx);
            if (sregs_[input_mod_s_reg].Ref()) {
              merged_type.MergePureRefAndArray(sregs_[input_mod_s_reg]);
            }
          }
        }
        if (merged_type.Ref()) {
          merged_type.CheckPureRef();
          changed |= sregs_[mir->ssa_rep->defs[0]].MergePureRefAndArray(merged_type);
        } else {
          // Either a non-reference Phi, or merging only untyped nulls. May become
          // a typed reference on subsequent iterations.
        }
        break;
      }

      case Instruction::MOVE_OBJECT:
      case Instruction::MOVE_OBJECT_16:
      case Instruction::MOVE_OBJECT_FROM16:
        if ((mir->optimization_flags & MIR_CALLEE) != 0) {
          // Nothing to do, the type of the inlined move has already been determined.
        } else {
          Type src_type = sregs_[ModifiedSReg(mir->ssa_rep->uses[0])];
          if (src_type.Ref()) {  // Ignore untyped null.
            src_type.CheckPureRef();
            sregs_[mir->ssa_rep->defs[0]].CheckUnassignedOrPureRef();
            changed |= sregs_[mir->ssa_rep->defs[0]].Copy(src_type);
          }
        }
        break;
      case Instruction::CHECK_CAST:
        DCHECK(check_cast_data_ != nullptr);
        check_cast_data_->ProcessCheckCast(mir);  // Update the current modified SSA reg.
        break;

      case Instruction::AGET_OBJECT: {
        Type array_type = sregs_[ModifiedSReg(mir->ssa_rep->uses[0])];
        if (array_type.Ref()) {  // Ignore untyped null.
          if (array_type.ArrayDepth() == 0u) {
            // Method verifier shouldn't let this through.
            LOG(WARNING) << "Found aget-object on non-array reference at 0x" << std::hex
                << mir->offset << " in " << PrettyMethod(cu_->method_idx, *cu_->dex_file);
          } else {
            changed |= sregs_[mir->ssa_rep->defs[0]].Copy(array_type.ComponentType());
          }
        }
        break;
      }

      default:
        break;
    }
  }
  return changed;
}

int32_t TypeInference::ModifiedSReg(int32_t s_reg) {
  if (UNLIKELY(check_cast_data_ != nullptr)) {
    SplitSRegData* split_data = check_cast_data_->GetSplitSRegData(s_reg);
    if (UNLIKELY(split_data != nullptr)) {
      DCHECK_NE(split_data->current_mod_s_reg, INVALID_SREG);
      return split_data->current_mod_s_reg;
    }
  }
  return s_reg;
}

int32_t TypeInference::PhiInputModifiedSReg(int32_t s_reg, BasicBlock* bb, size_t pred_idx) {
  DCHECK_LT(pred_idx, bb->predecessors.size());
  if (UNLIKELY(check_cast_data_ != nullptr)) {
    SplitSRegData* split_data = check_cast_data_->GetSplitSRegData(s_reg);
    if (UNLIKELY(split_data != nullptr)) {
      return split_data->ending_mod_s_reg[bb->predecessors[pred_idx]];
    }
  }
  return s_reg;
}

bool TypeInference::UpdateSRegFromLowWordType(int32_t mod_s_reg, Type low_word_type) {
  DCHECK(low_word_type.LowWord());
  bool changed = sregs_[mod_s_reg].MergeNonArrayFlags(low_word_type);
  if (!sregs_[mod_s_reg].Narrow()) {  // Wide without conflict with narrow.
    DCHECK(low_word_type.Wide());
    DCHECK_LT(mod_s_reg, mir_graph_->GetNumSSARegs());  // Original SSA reg.
    changed |= sregs_[mod_s_reg + 1].MergeHighWord(sregs_[mod_s_reg]);
  }
  // Ignore array type.
  return changed;
}

bool TypeInference::InferTypeAndSize(BasicBlock* bb, MIR* mir) {
  bool changed = false;
  SSARepresentation *ssa_rep = mir->ssa_rep;

  if (ssa_rep != nullptr) {
    uint64_t attrs = MIRGraph::GetDataFlowAttributes(mir);
    const int* uses = ssa_rep->uses;
    const int* defs = ssa_rep->defs;

    // Special-case handling for Phi comes first because we have 2 Phis instead of a wide one.
    if ((attrs & DF_NULL_TRANSFER_N) != 0) {
      // At least one input must have been previously processed. Look for the first
      // occurrence of a high_word or low_word flag to determine the type.
      DCHECK_EQ(bb->predecessors.size(), ssa_rep->num_uses);
      Type merged_type = sregs_[defs[0]];
      for (size_t pred_idx = 0; pred_idx != ssa_rep->num_uses; ++pred_idx) {
        int32_t input_mod_s_reg = PhiInputModifiedSReg(uses[pred_idx], bb, pred_idx);
        if (sregs_[input_mod_s_reg].IsDefined()) {
          merged_type.MergeNonArrayFlags(sregs_[input_mod_s_reg]);
        }
      }
      if (UNLIKELY(!merged_type.IsDefined())) {
        // No change
      } else if (merged_type.HighWord()) {
        // Ignore the high word phi, just remember if there's a size mismatch.
        if (UNLIKELY(merged_type.LowWord())) {
          sregs_[defs[0]].SetLowWord();
          sregs_[defs[0]].SetHighWord();
        }
      } else {
        // Propagate both up and down.
        changed |= UpdateSRegFromLowWordType(defs[0], merged_type);
        for (size_t pred_idx = 0; pred_idx != ssa_rep->num_uses; ++pred_idx) {
          int32_t input_mod_s_reg = PhiInputModifiedSReg(uses[pred_idx], bb, pred_idx);
          changed |= UpdateSRegFromLowWordType(input_mod_s_reg, merged_type);
        }
      }
      return changed;  // Don't process the Phi any further.
    }

    // Special-case handling for check-cast because ModifiedSReg(uses[0]) in not valid yet.
    if ((attrs & DF_CHK_CAST) != 0) {
      DCHECK(check_cast_data_ != nullptr);
      check_cast_data_->ProcessCheckCast(mir);
      return sregs_[uses[0]].SetRef();
    }

    // Handle defs
    if (attrs & DF_DA) {
      int32_t s_reg = defs[0];
      changed |= sregs_[s_reg].SetLowWord();
      if (attrs & DF_FP_A) {
        changed |= sregs_[s_reg].SetFp();
      }
      if (attrs & DF_CORE_A) {
        changed |= sregs_[s_reg].SetCore();
      }
      if (attrs & DF_REF_A) {
        changed |= sregs_[s_reg].SetRef();
      }
      if (attrs & DF_A_WIDE) {
        changed |= sregs_[s_reg].SetWide();
        DCHECK_EQ(s_reg + 1, ModifiedSReg(defs[1]));
        changed |= sregs_[s_reg + 1].MergeHighWord(sregs_[s_reg]);
      } else {
        sregs_[s_reg].SetNarrow();
      }
    }

    // Handles uses
    size_t next = 0;
#define PROCESS(REG)                                                        \
    if (attrs & DF_U##REG) {                                                \
      int32_t mod_s_reg = ModifiedSReg(uses[next]);                         \
      changed |= sregs_[mod_s_reg].SetLowWord();                            \
      if (attrs & DF_FP_##REG) {                                            \
        changed |= sregs_[mod_s_reg].SetFp();                               \
      }                                                                     \
      if (attrs & DF_CORE_##REG) {                                          \
        changed |= sregs_[mod_s_reg].SetCore();                             \
      }                                                                     \
      if (attrs & DF_REF_##REG) {                                           \
        changed |= sregs_[mod_s_reg].SetRef();                              \
      }                                                                     \
      if (attrs & DF_##REG##_WIDE) {                                        \
        changed |= sregs_[mod_s_reg].SetWide();                             \
        DCHECK_EQ(mod_s_reg + 1, ModifiedSReg(uses[next + 1]));             \
        changed |= sregs_[mod_s_reg + 1].SetWide();                         \
        changed |= sregs_[mod_s_reg + 1].MergeHighWord(sregs_[mod_s_reg]);  \
        next += 2;                                                          \
      } else {                                                              \
        changed |= sregs_[mod_s_reg].SetNarrow();                           \
        next++;                                                             \
      }                                                                     \
    }
    PROCESS(A)
    PROCESS(B)
    PROCESS(C)
#undef PROCESS
    DCHECK(next == ssa_rep->num_uses || (attrs & (DF_FORMAT_35C | DF_FORMAT_3RC)) != 0);

    // Special handling for moves. Propagate fp/core/ref both ways.
    if ((attrs & DF_IS_MOVE) != 0) {
      int32_t used_mod_s_reg = ModifiedSReg(uses[0]);
      int32_t defd_mod_s_reg = defs[0];
      changed |= UpdateSRegFromLowWordType(used_mod_s_reg, sregs_[defd_mod_s_reg]);
      changed |= UpdateSRegFromLowWordType(defd_mod_s_reg, sregs_[used_mod_s_reg]);
    }

    if ((attrs & (DF_IFIELD | DF_SFIELD)) != 0) {
      Type field_type = ((attrs & DF_IFIELD) != 0)
          ? ifields_[mir->meta.ifield_lowering_info]
          : sfields_[mir->meta.sfield_lowering_info];
      DCHECK_EQ((attrs & DF_A_WIDE) != 0, field_type.Wide());
      int32_t mod_s_reg = (attrs & DF_DA) != 0 ? defs[0] : ModifiedSReg(uses[0]);
      changed |= UpdateSRegFromLowWordType(mod_s_reg, field_type);
    }

    if ((attrs & DF_HAS_RANGE_CHKS) != 0) {
      int32_t base_mod_s_reg = ModifiedSReg(uses[ssa_rep->num_uses - 2u]);
      int32_t mod_s_reg = (attrs & DF_DA) != 0 ? defs[0] : ModifiedSReg(uses[0]);
      Type array_type = sregs_[base_mod_s_reg];
      if (array_type.ArrayDepth() != 0u) {
        UpdateSRegFromLowWordType(mod_s_reg, array_type.ComponentType());
      } else {
        LOG(WARNING) << "Found " << mir->dalvikInsn.opcode << " on non-array reference at 0x"
            << std::hex << mir->offset << " in " << PrettyMethod(cu_->method_idx, *cu_->dex_file);
      }
    }

    // Special-case return handling
    if ((mir->dalvikInsn.opcode == Instruction::RETURN) ||
        (mir->dalvikInsn.opcode == Instruction::RETURN_WIDE) ||
        (mir->dalvikInsn.opcode == Instruction::RETURN_OBJECT)) {
      int32_t mod_s_reg = ModifiedSReg(uses[0]);
      DCHECK_EQ(current_method_signature_.return_type.Wide(),
                mir->dalvikInsn.opcode == Instruction::RETURN_WIDE);
      DCHECK(!current_method_signature_.return_type.Wide() || mod_s_reg + 1 == uses[1]);
      changed |= UpdateSRegFromLowWordType(mod_s_reg, current_method_signature_.return_type);
    }

    // Special-case handling for format 35c/3rc invokes
    if ((attrs & (DF_FORMAT_35C | DF_FORMAT_3RC)) != 0) {
      DCHECK_EQ(next, 0u);
      // Result type handled in previous phase. Handle arguments.
      if (attrs & DF_NON_NULL_RET) {
        DCHECK(mir->dalvikInsn.opcode == Instruction::FILLED_NEW_ARRAY ||
               mir->dalvikInsn.opcode == Instruction::FILLED_NEW_ARRAY_RANGE);
        Type array_type = Type::DexType(cu_->dex_file, mir->dalvikInsn.vB);
        array_type.CheckPureRef();  // Previously checked by the method verifier.
        DCHECK(!array_type.ComponentType().Wide());
        Type component_type = array_type.ComponentType();
        DCHECK_EQ(ssa_rep->num_uses, mir->dalvikInsn.vA);
        for (; next != ssa_rep->num_uses; ++next) {
          int32_t input_mod_s_reg = ModifiedSReg(uses[next]);
          changed |= sregs_[input_mod_s_reg].MergeNonArrayFlags(component_type);
        }
      } else {
        DCHECK_NE(mir->dalvikInsn.FlagsOf() & Instruction::kInvoke, 0);
        const MethodSignature* signature = &signatures_[mir->meta.method_lowering_info];
        for (size_t i = 0, size = signature->num_params; i != size; ++i)  {
          Type param_type = signature->param_types[i];
          int32_t param_s_reg = ModifiedSReg(uses[next]);
          DCHECK(!param_type.Wide() || uses[next] + 1 == uses[next + 1]);
          changed |= UpdateSRegFromLowWordType(param_s_reg, param_type);
          next += param_type.Wide() ? 2 : 1;
        }
        DCHECK_EQ(next, mir->dalvikInsn.vA);
      }
    }
  }
  return changed;
}

}  // namespace art
