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

#include "dataflow_iterator-inl.h"
#include "dex/mir_field_info.h"
#include "global_value_numbering.h"
#include "gvn_dead_code_elimination.h"
#include "local_value_numbering.h"
#include "gtest/gtest.h"

namespace art {

class GvnDeadCodeEliminationTest : public testing::Test {
 protected:
  static constexpr uint16_t kNoValue = GlobalValueNumbering::kNoValue;

  struct IFieldDef {
    uint16_t field_idx;
    uintptr_t declaring_dex_file;
    uint16_t declaring_field_idx;
    bool is_volatile;
    DexMemAccessType type;
  };

  struct SFieldDef {
    uint16_t field_idx;
    uintptr_t declaring_dex_file;
    uint16_t declaring_field_idx;
    bool is_volatile;
    DexMemAccessType type;
  };

  struct BBDef {
    static constexpr size_t kMaxSuccessors = 4;
    static constexpr size_t kMaxPredecessors = 4;

    BBType type;
    size_t num_successors;
    BasicBlockId successors[kMaxPredecessors];
    size_t num_predecessors;
    BasicBlockId predecessors[kMaxPredecessors];
  };

  struct MIRDef {
    static constexpr size_t kMaxSsaDefs = 2;
    static constexpr size_t kMaxSsaUses = 4;

    BasicBlockId bbid;
    Instruction::Code opcode;
    int64_t value;
    uint32_t field_info;
    size_t num_uses;
    int32_t uses[kMaxSsaUses];
    size_t num_defs;
    int32_t defs[kMaxSsaDefs];
  };

#define DEF_SUCC0() \
    0u, { }
#define DEF_SUCC1(s1) \
    1u, { s1 }
#define DEF_SUCC2(s1, s2) \
    2u, { s1, s2 }
#define DEF_SUCC3(s1, s2, s3) \
    3u, { s1, s2, s3 }
#define DEF_SUCC4(s1, s2, s3, s4) \
    4u, { s1, s2, s3, s4 }
#define DEF_PRED0() \
    0u, { }
#define DEF_PRED1(p1) \
    1u, { p1 }
#define DEF_PRED2(p1, p2) \
    2u, { p1, p2 }
#define DEF_PRED3(p1, p2, p3) \
    3u, { p1, p2, p3 }
#define DEF_PRED4(p1, p2, p3, p4) \
    4u, { p1, p2, p3, p4 }
#define DEF_BB(type, succ, pred) \
    { type, succ, pred }

#define DEF_CONST(bb, opcode, reg, value) \
    { bb, opcode, value, 0u, 0, { }, 1, { reg } }
#define DEF_CONST_WIDE(bb, opcode, reg, value) \
    { bb, opcode, value, 0u, 0, { }, 2, { reg, reg + 1 } }
#define DEF_CONST_STRING(bb, opcode, reg, index) \
    { bb, opcode, index, 0u, 0, { }, 1, { reg } }
#define DEF_IGET(bb, opcode, reg, obj, field_info) \
    { bb, opcode, 0u, field_info, 1, { obj }, 1, { reg } }
#define DEF_IGET_WIDE(bb, opcode, reg, obj, field_info) \
    { bb, opcode, 0u, field_info, 1, { obj }, 2, { reg, reg + 1 } }
#define DEF_IPUT(bb, opcode, reg, obj, field_info) \
    { bb, opcode, 0u, field_info, 2, { reg, obj }, 0, { } }
#define DEF_IPUT_WIDE(bb, opcode, reg, obj, field_info) \
    { bb, opcode, 0u, field_info, 3, { reg, reg + 1, obj }, 0, { } }
#define DEF_SGET(bb, opcode, reg, field_info) \
    { bb, opcode, 0u, field_info, 0, { }, 1, { reg } }
#define DEF_SGET_WIDE(bb, opcode, reg, field_info) \
    { bb, opcode, 0u, field_info, 0, { }, 2, { reg, reg + 1 } }
#define DEF_SPUT(bb, opcode, reg, field_info) \
    { bb, opcode, 0u, field_info, 1, { reg }, 0, { } }
#define DEF_SPUT_WIDE(bb, opcode, reg, field_info) \
    { bb, opcode, 0u, field_info, 2, { reg, reg + 1 }, 0, { } }
#define DEF_AGET(bb, opcode, reg, obj, idx) \
    { bb, opcode, 0u, 0u, 2, { obj, idx }, 1, { reg } }
#define DEF_AGET_WIDE(bb, opcode, reg, obj, idx) \
    { bb, opcode, 0u, 0u, 2, { obj, idx }, 2, { reg, reg + 1 } }
#define DEF_APUT(bb, opcode, reg, obj, idx) \
    { bb, opcode, 0u, 0u, 3, { reg, obj, idx }, 0, { } }
#define DEF_APUT_WIDE(bb, opcode, reg, obj, idx) \
    { bb, opcode, 0u, 0u, 4, { reg, reg + 1, obj, idx }, 0, { } }
#define DEF_INVOKE1(bb, opcode, reg) \
    { bb, opcode, 0u, 0u, 1, { reg }, 0, { } }
#define DEF_UNIQUE_REF(bb, opcode, reg) \
    { bb, opcode, 0u, 0u, 0, { }, 1, { reg } }  // CONST_CLASS, CONST_STRING, NEW_ARRAY, ...
#define DEF_IFZ(bb, opcode, reg) \
    { bb, opcode, 0u, 0u, 1, { reg }, 0, { } }
#define DEF_MOVE(bb, opcode, reg, src) \
    { bb, opcode, 0u, 0u, 1, { src }, 1, { reg } }
#define DEF_MOVE_WIDE(bb, opcode, reg, src) \
    { bb, opcode, 0u, 0u, 2, { src, src + 1 }, 2, { reg, reg + 1 } }
#define DEF_PHI2(bb, reg, src1, src2) \
    { bb, static_cast<Instruction::Code>(kMirOpPhi), 0, 0u, 2u, { src1, src2 }, 1, { reg } }
#define DEF_BINOP(bb, opcode, result, src1, src2) \
    { bb, opcode, 0u, 0u, 2, { src1, src2 }, 1, { result } }

  void DoPrepareIFields(const IFieldDef* defs, size_t count) {
    cu_.mir_graph->ifield_lowering_infos_.clear();
    cu_.mir_graph->ifield_lowering_infos_.reserve(count);
    for (size_t i = 0u; i != count; ++i) {
      const IFieldDef* def = &defs[i];
      MirIFieldLoweringInfo field_info(def->field_idx, def->type);
      if (def->declaring_dex_file != 0u) {
        field_info.declaring_dex_file_ = reinterpret_cast<const DexFile*>(def->declaring_dex_file);
        field_info.declaring_field_idx_ = def->declaring_field_idx;
        field_info.flags_ &= ~(def->is_volatile ? 0u : MirSFieldLoweringInfo::kFlagIsVolatile);
      }
      cu_.mir_graph->ifield_lowering_infos_.push_back(field_info);
    }
  }

  template <size_t count>
  void PrepareIFields(const IFieldDef (&defs)[count]) {
    DoPrepareIFields(defs, count);
  }

  void DoPrepareSFields(const SFieldDef* defs, size_t count) {
    cu_.mir_graph->sfield_lowering_infos_.clear();
    cu_.mir_graph->sfield_lowering_infos_.reserve(count);
    for (size_t i = 0u; i != count; ++i) {
      const SFieldDef* def = &defs[i];
      MirSFieldLoweringInfo field_info(def->field_idx, def->type);
      // Mark even unresolved fields as initialized.
      field_info.flags_ |= MirSFieldLoweringInfo::kFlagClassIsInitialized;
      // NOTE: MirSFieldLoweringInfo::kFlagClassIsInDexCache isn't used by GVN.
      if (def->declaring_dex_file != 0u) {
        field_info.declaring_dex_file_ = reinterpret_cast<const DexFile*>(def->declaring_dex_file);
        field_info.declaring_field_idx_ = def->declaring_field_idx;
        field_info.flags_ &= ~(def->is_volatile ? 0u : MirSFieldLoweringInfo::kFlagIsVolatile);
      }
      cu_.mir_graph->sfield_lowering_infos_.push_back(field_info);
    }
  }

  template <size_t count>
  void PrepareSFields(const SFieldDef (&defs)[count]) {
    DoPrepareSFields(defs, count);
  }

  void DoPrepareBasicBlocks(const BBDef* defs, size_t count) {
    cu_.mir_graph->block_id_map_.clear();
    cu_.mir_graph->block_list_.clear();
    ASSERT_LT(3u, count);  // null, entry, exit and at least one bytecode block.
    ASSERT_EQ(kNullBlock, defs[0].type);
    ASSERT_EQ(kEntryBlock, defs[1].type);
    ASSERT_EQ(kExitBlock, defs[2].type);
    for (size_t i = 0u; i != count; ++i) {
      const BBDef* def = &defs[i];
      BasicBlock* bb = cu_.mir_graph->CreateNewBB(def->type);
      if (def->num_successors <= 2) {
        bb->successor_block_list_type = kNotUsed;
        bb->fall_through = (def->num_successors >= 1) ? def->successors[0] : 0u;
        bb->taken = (def->num_successors >= 2) ? def->successors[1] : 0u;
      } else {
        bb->successor_block_list_type = kPackedSwitch;
        bb->fall_through = 0u;
        bb->taken = 0u;
        bb->successor_blocks.reserve(def->num_successors);
        for (size_t j = 0u; j != def->num_successors; ++j) {
          SuccessorBlockInfo* successor_block_info =
              static_cast<SuccessorBlockInfo*>(cu_.arena.Alloc(sizeof(SuccessorBlockInfo),
                                                               kArenaAllocSuccessor));
          successor_block_info->block = j;
          successor_block_info->key = 0u;  // Not used by class init check elimination.
          bb->successor_blocks.push_back(successor_block_info);
        }
      }
      bb->predecessors.assign(def->predecessors, def->predecessors + def->num_predecessors);
      if (def->type == kDalvikByteCode || def->type == kEntryBlock || def->type == kExitBlock) {
        bb->data_flow_info = static_cast<BasicBlockDataFlow*>(
            cu_.arena.Alloc(sizeof(BasicBlockDataFlow), kArenaAllocDFInfo));
        bb->data_flow_info->live_in_v = live_in_v_;
      }
    }
    ASSERT_EQ(count, cu_.mir_graph->block_list_.size());
    cu_.mir_graph->entry_block_ = cu_.mir_graph->block_list_[1];
    ASSERT_EQ(kEntryBlock, cu_.mir_graph->entry_block_->block_type);
    cu_.mir_graph->exit_block_ = cu_.mir_graph->block_list_[2];
    ASSERT_EQ(kExitBlock, cu_.mir_graph->exit_block_->block_type);
  }

  template <size_t count>
  void PrepareBasicBlocks(const BBDef (&defs)[count]) {
    DoPrepareBasicBlocks(defs, count);
  }

  void DoPrepareMIRs(const MIRDef* defs, size_t count) {
    mir_count_ = count;
    mirs_ = reinterpret_cast<MIR*>(cu_.arena.Alloc(sizeof(MIR) * count, kArenaAllocMIR));
    ssa_reps_.resize(count);
    for (size_t i = 0u; i != count; ++i) {
      const MIRDef* def = &defs[i];
      MIR* mir = &mirs_[i];
      ASSERT_LT(def->bbid, cu_.mir_graph->block_list_.size());
      BasicBlock* bb = cu_.mir_graph->block_list_[def->bbid];
      bb->AppendMIR(mir);
      mir->dalvikInsn.opcode = def->opcode;
      mir->dalvikInsn.vB = static_cast<int32_t>(def->value);
      mir->dalvikInsn.vB_wide = def->value;
      if (IsInstructionIGetOrIPut(def->opcode)) {
        ASSERT_LT(def->field_info, cu_.mir_graph->ifield_lowering_infos_.size());
        mir->meta.ifield_lowering_info = def->field_info;
        ASSERT_EQ(cu_.mir_graph->ifield_lowering_infos_[def->field_info].MemAccessType(),
                  IGetOrIPutMemAccessType(def->opcode));
      } else if (IsInstructionSGetOrSPut(def->opcode)) {
        ASSERT_LT(def->field_info, cu_.mir_graph->sfield_lowering_infos_.size());
        mir->meta.sfield_lowering_info = def->field_info;
        ASSERT_EQ(cu_.mir_graph->sfield_lowering_infos_[def->field_info].MemAccessType(),
                  SGetOrSPutMemAccessType(def->opcode));
      } else if (def->opcode == static_cast<Instruction::Code>(kMirOpPhi)) {
        mir->meta.phi_incoming = static_cast<BasicBlockId*>(
            allocator_->Alloc(def->num_uses * sizeof(BasicBlockId), kArenaAllocDFInfo));
        ASSERT_EQ(def->num_uses, bb->predecessors.size());
        std::copy(bb->predecessors.begin(), bb->predecessors.end(), mir->meta.phi_incoming);
      }
      mir->ssa_rep = &ssa_reps_[i];
      mir->ssa_rep->num_uses = def->num_uses;
      mir->ssa_rep->uses = const_cast<int32_t*>(def->uses);  // Not modified by LVN.
      mir->ssa_rep->fp_use = nullptr;  // Not used by LVN.
      mir->ssa_rep->num_defs = def->num_defs;
      mir->ssa_rep->defs = const_cast<int32_t*>(def->defs);  // Not modified by LVN.
      mir->ssa_rep->fp_def = nullptr;  // Not used by LVN.
      mir->dalvikInsn.opcode = def->opcode;
      mir->offset = i;  // LVN uses offset only for debug output
      mir->optimization_flags = 0u;
    }
    mirs_[count - 1u].next = nullptr;
    DexFile::CodeItem* code_item = static_cast<DexFile::CodeItem*>(
        cu_.arena.Alloc(sizeof(DexFile::CodeItem), kArenaAllocMisc));
    code_item->insns_size_in_code_units_ = 2u * count;
    code_item->registers_size_ = kMaxVRegs;
    cu_.mir_graph->current_code_item_ = code_item;
  }

  template <size_t count>
  void PrepareMIRs(const MIRDef (&defs)[count]) {
    DoPrepareMIRs(defs, count);
  }

  template <size_t count>
  void PrepareSRegToVRegMap(const int (&map)[count]) {
    cu_.mir_graph->ssa_base_vregs_.assign(map, map + count);
  }

  void DoPrepareVregToSsaMapExit(BasicBlockId bb_id, const int32_t* map, size_t count) {
    BasicBlock* bb = cu_.mir_graph->GetBasicBlock(bb_id);
    ASSERT_TRUE(bb != nullptr);
    ASSERT_TRUE(bb->data_flow_info != nullptr);
    bb->data_flow_info->vreg_to_ssa_map_exit = reinterpret_cast<int32_t*>(
        cu_.arena.Alloc(count * sizeof(int32_t), kArenaAllocDFInfo));
    std::copy_n(map, count, bb->data_flow_info->vreg_to_ssa_map_exit);
  }

  template <size_t count>
  void PrepareVregToSsaMapExit(BasicBlockId bb_id, const int32_t (&map)[count]) {
    DoPrepareVregToSsaMapExit(bb_id, map, count);
  }

  void PerformGVN() {
    cu_.mir_graph->SSATransformationStart();
    cu_.mir_graph->ComputeDFSOrders();
    cu_.mir_graph->ComputeDominators();
    cu_.mir_graph->ComputeTopologicalSortOrder();
    cu_.mir_graph->SSATransformationEnd();
    cu_.mir_graph->temp_.gvn.ifield_ids =  GlobalValueNumbering::PrepareGvnFieldIds(
        allocator_.get(), cu_.mir_graph->ifield_lowering_infos_);
    cu_.mir_graph->temp_.gvn.sfield_ids =  GlobalValueNumbering::PrepareGvnFieldIds(
        allocator_.get(), cu_.mir_graph->sfield_lowering_infos_);
    ASSERT_TRUE(gvn_ == nullptr);
    gvn_.reset(new (allocator_.get()) GlobalValueNumbering(&cu_, allocator_.get(),
                                                           GlobalValueNumbering::kModeGvn));
    value_names_.resize(mir_count_, 0xffffu);
    LoopRepeatingTopologicalSortIterator iterator(cu_.mir_graph.get());
    bool change = false;
    for (BasicBlock* bb = iterator.Next(change); bb != nullptr; bb = iterator.Next(change)) {
      LocalValueNumbering* lvn = gvn_->PrepareBasicBlock(bb);
      if (lvn != nullptr) {
        for (MIR* mir = bb->first_mir_insn; mir != nullptr; mir = mir->next) {
          value_names_[mir - mirs_] = lvn->GetValueNumber(mir);
        }
      }
      change = (lvn != nullptr) && gvn_->FinishBasicBlock(bb);
      ASSERT_TRUE(gvn_->Good());
    }
    gvn_->StartPostProcessing();
  }

  void PerformDCE() {
    cu_.mir_graph->GetNumOfCodeAndTempVRs();
    dce_.reset(new (allocator_.get()) GvnDeadCodeElimination(gvn_.get(), allocator_.get()));
    PreOrderDfsIterator iterator(cu_.mir_graph.get());
    for (BasicBlock* bb = iterator.Next(); bb != nullptr; bb = iterator.Next()) {
      if (bb->block_type == kDalvikByteCode) {
        dce_->Apply(bb);
      }
    }
  }

  void PerformGVN_DCE() {
    PerformGVN();
    PerformDCE();
  }

  void PerformGVNCodeModifications() {
    ASSERT_TRUE(gvn_ != nullptr);
    ASSERT_TRUE(gvn_->Good());
    gvn_->StartPostProcessing();
    TopologicalSortIterator iterator(cu_.mir_graph.get());
    for (BasicBlock* bb = iterator.Next(); bb != nullptr; bb = iterator.Next()) {
      LocalValueNumbering* lvn = gvn_->PrepareBasicBlock(bb);
      if (lvn != nullptr) {
        for (MIR* mir = bb->first_mir_insn; mir != nullptr; mir = mir->next) {
          uint16_t value_name = lvn->GetValueNumber(mir);
          ASSERT_EQ(value_name, value_names_[mir - mirs_]);
        }
      }
      bool change = (lvn != nullptr) && gvn_->FinishBasicBlock(bb);
      ASSERT_FALSE(change);
      ASSERT_TRUE(gvn_->Good());
    }
  }

  GvnDeadCodeEliminationTest()
      : pool_(),
        cu_(&pool_),
        mir_count_(0u),
        mirs_(nullptr),
        ssa_reps_(),
        allocator_(),
        gvn_(),
        dce_(),
        value_names_(),
        live_in_v_(new (&cu_.arena) ArenaBitVector(&cu_.arena, kMaxSsaRegs, false, kBitMapMisc)) {
    cu_.mir_graph.reset(new MIRGraph(&cu_, &cu_.arena));
    cu_.access_flags = kAccStatic;  // Don't let "this" interfere with this test.
    allocator_.reset(ScopedArenaAllocator::Create(&cu_.arena_stack));
    // By default, the zero-initialized reg_location_[.] with ref == false tells LVN that
    // 0 constants are integral, not references. Nothing else is used by LVN/GVN.
    cu_.mir_graph->reg_location_ = static_cast<RegLocation*>(cu_.arena.Alloc(
        kMaxSsaRegs * sizeof(cu_.mir_graph->reg_location_[0]), kArenaAllocRegAlloc));
    // Bind all possible sregs to live vregs for test purposes.
    live_in_v_->SetInitialBits(kMaxSsaRegs);
    cu_.mir_graph->ssa_base_vregs_.reserve(kMaxSsaRegs);
    cu_.mir_graph->ssa_subscripts_.reserve(kMaxSsaRegs);
    for (unsigned int i = 0; i < kMaxSsaRegs; i++) {
      cu_.mir_graph->ssa_base_vregs_.push_back(i);
      cu_.mir_graph->ssa_subscripts_.push_back(0);
    }
    // Set shorty for a void-returning method without arguments.
    cu_.shorty = "V";
  }

  static constexpr size_t kMaxSsaRegs = 16384u;
  static constexpr size_t kMaxVRegs = 256u;

  ArenaPool pool_;
  CompilationUnit cu_;
  size_t mir_count_;
  MIR* mirs_;
  std::vector<SSARepresentation> ssa_reps_;
  std::unique_ptr<ScopedArenaAllocator> allocator_;
  std::unique_ptr<GlobalValueNumbering> gvn_;
  std::unique_ptr<GvnDeadCodeElimination> dce_;
  std::vector<uint16_t> value_names_;
  ArenaBitVector* live_in_v_;
};

constexpr uint16_t GvnDeadCodeEliminationTest::kNoValue;

class GvnDeadCodeEliminationTestSimple : public GvnDeadCodeEliminationTest {
 public:
  GvnDeadCodeEliminationTestSimple();

 private:
  static const BBDef kSimpleBbs[];
};

const GvnDeadCodeEliminationTest::BBDef GvnDeadCodeEliminationTestSimple::kSimpleBbs[] = {
    DEF_BB(kNullBlock, DEF_SUCC0(), DEF_PRED0()),
    DEF_BB(kEntryBlock, DEF_SUCC1(3), DEF_PRED0()),
    DEF_BB(kExitBlock, DEF_SUCC0(), DEF_PRED1(3)),
    DEF_BB(kDalvikByteCode, DEF_SUCC1(2), DEF_PRED1(1)),
};

GvnDeadCodeEliminationTestSimple::GvnDeadCodeEliminationTestSimple()
    : GvnDeadCodeEliminationTest() {
  PrepareBasicBlocks(kSimpleBbs);
}

class GvnDeadCodeEliminationTestDiamond : public GvnDeadCodeEliminationTest {
 public:
  GvnDeadCodeEliminationTestDiamond();

 private:
  static const BBDef kDiamondBbs[];
};

const GvnDeadCodeEliminationTest::BBDef GvnDeadCodeEliminationTestDiamond::kDiamondBbs[] = {
    DEF_BB(kNullBlock, DEF_SUCC0(), DEF_PRED0()),
    DEF_BB(kEntryBlock, DEF_SUCC1(3), DEF_PRED0()),
    DEF_BB(kExitBlock, DEF_SUCC0(), DEF_PRED1(6)),
    DEF_BB(kDalvikByteCode, DEF_SUCC2(4, 5), DEF_PRED1(1)),  // Block #3, top of the diamond.
    DEF_BB(kDalvikByteCode, DEF_SUCC1(6), DEF_PRED1(3)),     // Block #4, left side.
    DEF_BB(kDalvikByteCode, DEF_SUCC1(6), DEF_PRED1(3)),     // Block #5, right side.
    DEF_BB(kDalvikByteCode, DEF_SUCC1(2), DEF_PRED2(4, 5)),  // Block #6, bottom.
};

GvnDeadCodeEliminationTestDiamond::GvnDeadCodeEliminationTestDiamond()
    : GvnDeadCodeEliminationTest() {
  PrepareBasicBlocks(kDiamondBbs);
}

class GvnDeadCodeEliminationTestLoop : public GvnDeadCodeEliminationTest {
 public:
  GvnDeadCodeEliminationTestLoop();

 private:
  static const BBDef kLoopBbs[];
};

const GvnDeadCodeEliminationTest::BBDef GvnDeadCodeEliminationTestLoop::kLoopBbs[] = {
    DEF_BB(kNullBlock, DEF_SUCC0(), DEF_PRED0()),
    DEF_BB(kEntryBlock, DEF_SUCC1(3), DEF_PRED0()),
    DEF_BB(kExitBlock, DEF_SUCC0(), DEF_PRED1(5)),
    DEF_BB(kDalvikByteCode, DEF_SUCC1(4), DEF_PRED1(1)),
    DEF_BB(kDalvikByteCode, DEF_SUCC2(5, 4), DEF_PRED2(3, 4)),  // "taken" loops to self.
    DEF_BB(kDalvikByteCode, DEF_SUCC1(2), DEF_PRED1(4)),
};

GvnDeadCodeEliminationTestLoop::GvnDeadCodeEliminationTestLoop()
    : GvnDeadCodeEliminationTest() {
  PrepareBasicBlocks(kLoopBbs);
}

class GvnDeadCodeEliminationTestCatch : public GvnDeadCodeEliminationTest {
 public:
  GvnDeadCodeEliminationTestCatch();

 private:
  static const BBDef kCatchBbs[];
};

const GvnDeadCodeEliminationTest::BBDef GvnDeadCodeEliminationTestCatch::kCatchBbs[] = {
    DEF_BB(kNullBlock, DEF_SUCC0(), DEF_PRED0()),
    DEF_BB(kEntryBlock, DEF_SUCC1(3), DEF_PRED0()),
    DEF_BB(kExitBlock, DEF_SUCC0(), DEF_PRED1(6)),
    DEF_BB(kDalvikByteCode, DEF_SUCC1(4), DEF_PRED1(1)),     // The top.
    DEF_BB(kDalvikByteCode, DEF_SUCC1(6), DEF_PRED1(3)),     // The throwing insn.
    DEF_BB(kDalvikByteCode, DEF_SUCC1(6), DEF_PRED1(3)),     // Catch handler.
    DEF_BB(kDalvikByteCode, DEF_SUCC1(2), DEF_PRED2(4, 5)),  // The merged block.
};

GvnDeadCodeEliminationTestCatch::GvnDeadCodeEliminationTestCatch()
    : GvnDeadCodeEliminationTest() {
  PrepareBasicBlocks(kCatchBbs);
  // Mark catch handler.
  BasicBlock* catch_handler = cu_.mir_graph->GetBasicBlock(5u);
  catch_handler->catch_entry = true;
  // Add successor block info to the check block.
  BasicBlock* check_bb = cu_.mir_graph->GetBasicBlock(3u);
  check_bb->successor_block_list_type = kCatch;
  SuccessorBlockInfo* successor_block_info = reinterpret_cast<SuccessorBlockInfo*>
      (cu_.arena.Alloc(sizeof(SuccessorBlockInfo), kArenaAllocSuccessor));
  successor_block_info->block = catch_handler->id;
  check_bb->successor_blocks.push_back(successor_block_info);
}

TEST_F(GvnDeadCodeEliminationTestSimple, IFields) {
#if 0
  static const IFieldDef ifields[] = {
      { 0u, 1u, 0u, false, kDexMemAccessObject },
      { 1u, 1u, 1u, false, kDexMemAccessObject },
      { 2u, 1u, 2u, false, kDexMemAccessWord },
  };
  static const MIRDef mirs[] = {
      DEF_UNIQUE_REF(3, Instruction::NEW_INSTANCE, 0u),
      DEF_IGET(3, Instruction::IGET_OBJECT, 1u, 0u, 0u),
      DEF_IGET(3, Instruction::IGET_OBJECT, 2u, 1u, 1u),
      DEF_IGET(3, Instruction::IGET, 3u, 2u, 2u),
      DEF_IGET(3, Instruction::IGET_OBJECT, 4u, 0u, 0u),
      DEF_IGET(3, Instruction::IGET_OBJECT, 5u, 4u, 1u),
  };

  static const int32_t sreg_to_vreg_map[] = { 0, 1, 2, 3, 1, 2 };
  PrepareSRegToVRegMap(sreg_to_vreg_map);
  static const int32_t entry_block_vreg_to_ssa_map_exit[] = { 1000, 1001, 1002, 1003 };
  PrepareVregToSsaMapExit(1, entry_block_vreg_to_ssa_map_exit);

  PrepareIFields(ifields);
  PrepareMIRs(mirs);
  PerformGVN_DCE();

  static const bool eliminated[] = {
      false, false, false, false, true, true
  };
  for (size_t i = 0; i != arraysize(eliminated); ++i) {
    bool actually_eliminated = (static_cast<int>(mirs_[i].dalvikInsn.opcode) == kMirOpNop);
    EXPECT_EQ(eliminated[i], actually_eliminated) << i;
  }
#endif
}

#if 0
TEST_F(GvnDeadCodeEliminationTestDiamond, NonAliasingIFields) {
  static const IFieldDef ifields[] = {
      { 0u, 1u, 0u, false, kDexMemAccessWord },
      { 1u, 1u, 1u, false, kDexMemAccessWord },
      { 2u, 1u, 2u, false, kDexMemAccessWord },
      { 3u, 1u, 3u, false, kDexMemAccessWord },
      { 4u, 1u, 4u, false, kDexMemAccessShort },
      { 5u, 1u, 5u, false, kDexMemAccessChar },
      { 6u, 0u, 0u, false, kDexMemAccessShort },   // Unresolved.
      { 7u, 1u, 7u, false, kDexMemAccessWord },
      { 8u, 0u, 0u, false, kDexMemAccessWord },    // Unresolved.
      { 9u, 1u, 9u, false, kDexMemAccessWord },
      { 10u, 1u, 10u, false, kDexMemAccessWord },
      { 11u, 1u, 11u, false, kDexMemAccessWord },
  };
  static const MIRDef mirs[] = {
      // NOTE: MIRs here are ordered by unique tests. They will be put into appropriate blocks.
      DEF_UNIQUE_REF(3, Instruction::NEW_INSTANCE, 100u),
      DEF_IGET(3, Instruction::IGET, 1u, 100u, 0u),
      DEF_IGET(6, Instruction::IGET, 2u, 100u, 0u),   // Same as at the top.

      DEF_UNIQUE_REF(3, Instruction::NEW_INSTANCE, 200u),
      DEF_IGET(4, Instruction::IGET, 4u, 200u, 1u),
      DEF_IGET(6, Instruction::IGET, 5u, 200u, 1u),   // Same as at the left side.

      DEF_UNIQUE_REF(3, Instruction::NEW_INSTANCE, 300u),
      DEF_IGET(3, Instruction::IGET, 7u, 300u, 2u),
      DEF_CONST(5, Instruction::CONST, 8u, 1000),
      DEF_IPUT(5, Instruction::IPUT, 8u, 300u, 2u),
      DEF_IGET(6, Instruction::IGET, 10u, 300u, 2u),  // Differs from the top and the CONST.

      DEF_UNIQUE_REF(3, Instruction::NEW_INSTANCE, 400u),
      DEF_IGET(3, Instruction::IGET, 12u, 400u, 3u),
      DEF_CONST(3, Instruction::CONST, 13u, 2000),
      DEF_IPUT(4, Instruction::IPUT, 13u, 400u, 3u),
      DEF_IPUT(5, Instruction::IPUT, 13u, 400u, 3u),
      DEF_IGET(6, Instruction::IGET, 16u, 400u, 3u),  // Differs from the top, equals the CONST.

      DEF_UNIQUE_REF(3, Instruction::NEW_INSTANCE, 500u),
      DEF_IGET(3, Instruction::IGET_SHORT, 18u, 500u, 4u),
      DEF_IGET(3, Instruction::IGET_CHAR, 19u, 500u, 5u),
      DEF_IPUT(4, Instruction::IPUT_SHORT, 20u, 500u, 6u),  // Clobbers field #4, not #5.
      DEF_IGET(6, Instruction::IGET_SHORT, 21u, 500u, 4u),  // Differs from the top.
      DEF_IGET(6, Instruction::IGET_CHAR, 22u, 500u, 5u),   // Same as the top.

      DEF_UNIQUE_REF(3, Instruction::NEW_INSTANCE, 600u),
      DEF_UNIQUE_REF(3, Instruction::NEW_INSTANCE, 601u),
      DEF_UNIQUE_REF(3, Instruction::NEW_INSTANCE, 602u),
      DEF_IGET(3, Instruction::IGET, 26u, 600u, 7u),
      DEF_IGET(3, Instruction::IGET, 27u, 601u, 7u),
      DEF_IPUT(4, Instruction::IPUT, 28u, 602u, 8u),  // Doesn't clobber field #7 for other refs.
      DEF_IGET(6, Instruction::IGET, 29u, 600u, 7u),  // Same as the top.
      DEF_IGET(6, Instruction::IGET, 30u, 601u, 7u),  // Same as the top.

      DEF_UNIQUE_REF(3, Instruction::NEW_INSTANCE, 700u),
      DEF_CONST(4, Instruction::CONST, 32u, 3000),
      DEF_IPUT(4, Instruction::IPUT, 32u, 700u, 9u),
      DEF_IPUT(4, Instruction::IPUT, 32u, 700u, 10u),
      DEF_CONST(5, Instruction::CONST, 35u, 3001),
      DEF_IPUT(5, Instruction::IPUT, 35u, 700u, 9u),
      DEF_IPUT(5, Instruction::IPUT, 35u, 700u, 10u),
      DEF_IGET(6, Instruction::IGET, 38u, 700u, 9u),
      DEF_IGET(6, Instruction::IGET, 39u, 700u, 10u),  // Same value as read from field #9.

      DEF_UNIQUE_REF(3, Instruction::NEW_INSTANCE, 800u),
      DEF_UNIQUE_REF(3, Instruction::NEW_INSTANCE, 801u),
      DEF_CONST(4, Instruction::CONST, 42u, 3000),
      DEF_IPUT(4, Instruction::IPUT, 42u, 800u, 11u),
      DEF_IPUT(4, Instruction::IPUT, 42u, 801u, 11u),
      DEF_CONST(5, Instruction::CONST, 45u, 3001),
      DEF_IPUT(5, Instruction::IPUT, 45u, 800u, 11u),
      DEF_IPUT(5, Instruction::IPUT, 45u, 801u, 11u),
      DEF_IGET(6, Instruction::IGET, 48u, 800u, 11u),
      DEF_IGET(6, Instruction::IGET, 49u, 801u, 11u),  // Same value as read from ref 46u.

      // Invoke doesn't interfere with non-aliasing refs. There's one test above where a reference
      // escapes in the left BB (we let a reference escape if we use it to store to an unresolved
      // field) and the INVOKE in the right BB shouldn't interfere with that either.
      DEF_INVOKE1(5, Instruction::INVOKE_STATIC, 48u),
  };

  PrepareIFields(ifields);
  PrepareMIRs(mirs);
  PerformGVN();
  ASSERT_EQ(arraysize(mirs), value_names_.size());
  EXPECT_EQ(value_names_[1], value_names_[2]);

  EXPECT_EQ(value_names_[4], value_names_[5]);

  EXPECT_NE(value_names_[7], value_names_[10]);
  EXPECT_NE(value_names_[8], value_names_[10]);

  EXPECT_NE(value_names_[12], value_names_[16]);
  EXPECT_EQ(value_names_[13], value_names_[16]);

  EXPECT_NE(value_names_[18], value_names_[21]);
  EXPECT_EQ(value_names_[19], value_names_[22]);

  EXPECT_EQ(value_names_[26], value_names_[29]);
  EXPECT_EQ(value_names_[27], value_names_[30]);

  EXPECT_EQ(value_names_[38], value_names_[39]);

  EXPECT_EQ(value_names_[48], value_names_[49]);
}
#endif

}  // namespace art
