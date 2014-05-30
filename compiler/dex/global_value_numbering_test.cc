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

#include "compiler_internals.h"
#include "dataflow_iterator.h"
#include "dataflow_iterator-inl.h"
#include "global_value_numbering.h"
#include "local_value_numbering.h"
#include "gtest/gtest.h"

namespace art {

class GlobalValueNumberingTest : public testing::Test {
 protected:
  struct IFieldDef {
    uint16_t field_idx;
    uintptr_t declaring_dex_file;
    uint16_t declaring_field_idx;
    bool is_volatile;
  };

  struct SFieldDef {
    uint16_t field_idx;
    uintptr_t declaring_dex_file;
    uint16_t declaring_field_idx;
    bool is_volatile;
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

  void DoPrepareIFields(const IFieldDef* defs, size_t count) {
    cu_.mir_graph->ifield_lowering_infos_.Reset();
    cu_.mir_graph->ifield_lowering_infos_.Resize(count);
    for (size_t i = 0u; i != count; ++i) {
      const IFieldDef* def = &defs[i];
      MirIFieldLoweringInfo field_info(def->field_idx);
      if (def->declaring_dex_file != 0u) {
        field_info.declaring_dex_file_ = reinterpret_cast<const DexFile*>(def->declaring_dex_file);
        field_info.declaring_field_idx_ = def->declaring_field_idx;
        field_info.flags_ = 0u |  // Without kFlagIsStatic.
            (def->is_volatile ? MirIFieldLoweringInfo::kFlagIsVolatile : 0u);
      }
      cu_.mir_graph->ifield_lowering_infos_.Insert(field_info);
    }
  }

  template <size_t count>
  void PrepareIFields(const IFieldDef (&defs)[count]) {
    DoPrepareIFields(defs, count);
  }

  void DoPrepareSFields(const SFieldDef* defs, size_t count) {
    cu_.mir_graph->sfield_lowering_infos_.Reset();
    cu_.mir_graph->sfield_lowering_infos_.Resize(count);
    for (size_t i = 0u; i != count; ++i) {
      const SFieldDef* def = &defs[i];
      MirSFieldLoweringInfo field_info(def->field_idx);
      if (def->declaring_dex_file != 0u) {
        field_info.declaring_dex_file_ = reinterpret_cast<const DexFile*>(def->declaring_dex_file);
        field_info.declaring_field_idx_ = def->declaring_field_idx;
        field_info.flags_ = MirSFieldLoweringInfo::kFlagIsStatic |
            (def->is_volatile ? MirSFieldLoweringInfo::kFlagIsVolatile : 0u);
      }
      cu_.mir_graph->sfield_lowering_infos_.Insert(field_info);
    }
  }

  template <size_t count>
  void PrepareSFields(const SFieldDef (&defs)[count]) {
    DoPrepareSFields(defs, count);
  }

  void DoPrepareBasicBlocks(const BBDef* defs, size_t count) {
    cu_.mir_graph->block_id_map_.clear();
    cu_.mir_graph->block_list_.Reset();
    ASSERT_LT(3u, count);  // null, entry, exit and at least one bytecode block.
    ASSERT_EQ(kNullBlock, defs[0].type);
    ASSERT_EQ(kEntryBlock, defs[1].type);
    ASSERT_EQ(kExitBlock, defs[2].type);
    for (size_t i = 0u; i != count; ++i) {
      const BBDef* def = &defs[i];
      BasicBlock* bb = cu_.mir_graph->NewMemBB(def->type, i);
      cu_.mir_graph->block_list_.Insert(bb);
      if (def->num_successors <= 2) {
        bb->successor_block_list_type = kNotUsed;
        bb->successor_blocks = nullptr;
        bb->fall_through = (def->num_successors >= 1) ? def->successors[0] : 0u;
        bb->taken = (def->num_successors >= 2) ? def->successors[1] : 0u;
      } else {
        bb->successor_block_list_type = kPackedSwitch;
        bb->fall_through = 0u;
        bb->taken = 0u;
        bb->successor_blocks = new (&cu_.arena) GrowableArray<SuccessorBlockInfo*>(
            &cu_.arena, def->num_successors, kGrowableArraySuccessorBlocks);
        for (size_t j = 0u; j != def->num_successors; ++j) {
          SuccessorBlockInfo* successor_block_info =
              static_cast<SuccessorBlockInfo*>(cu_.arena.Alloc(sizeof(SuccessorBlockInfo),
                                                               kArenaAllocSuccessor));
          successor_block_info->block = j;
          successor_block_info->key = 0u;  // Not used by class init check elimination.
          bb->successor_blocks->Insert(successor_block_info);
        }
      }
      bb->predecessors = new (&cu_.arena) GrowableArray<BasicBlockId>(
          &cu_.arena, def->num_predecessors, kGrowableArrayPredecessors);
      for (size_t j = 0u; j != def->num_predecessors; ++j) {
        ASSERT_NE(0u, def->predecessors[j]);
        bb->predecessors->Insert(def->predecessors[j]);
      }
      if (def->type == kDalvikByteCode || def->type == kEntryBlock || def->type == kExitBlock) {
        bb->data_flow_info = static_cast<BasicBlockDataFlow*>(
            cu_.arena.Alloc(sizeof(BasicBlockDataFlow), kArenaAllocDFInfo));
      }
    }
    cu_.mir_graph->num_blocks_ = count;
    ASSERT_EQ(count, cu_.mir_graph->block_list_.Size());
    cu_.mir_graph->entry_block_ = cu_.mir_graph->block_list_.Get(1);
    ASSERT_EQ(kEntryBlock, cu_.mir_graph->entry_block_->block_type);
    cu_.mir_graph->exit_block_ = cu_.mir_graph->block_list_.Get(2);
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
      ASSERT_LT(def->bbid, cu_.mir_graph->block_list_.Size());
      BasicBlock* bb = cu_.mir_graph->block_list_.Get(def->bbid);
      bb->AppendMIR(mir);
      mir->dalvikInsn.opcode = def->opcode;
      mir->dalvikInsn.vB = static_cast<int32_t>(def->value);
      mir->dalvikInsn.vB_wide = def->value;
      if (def->opcode >= Instruction::IGET && def->opcode <= Instruction::IPUT_SHORT) {
        ASSERT_LT(def->field_info, cu_.mir_graph->ifield_lowering_infos_.Size());
        mir->meta.ifield_lowering_info = def->field_info;
      } else if (def->opcode >= Instruction::SGET && def->opcode <= Instruction::SPUT_SHORT) {
        ASSERT_LT(def->field_info, cu_.mir_graph->sfield_lowering_infos_.Size());
        mir->meta.sfield_lowering_info = def->field_info;
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
  }

  template <size_t count>
  void PrepareMIRs(const MIRDef (&defs)[count]) {
    DoPrepareMIRs(defs, count);
  }

  void PerformGVN() {
    cu_.mir_graph->SSATransformationStart();
    cu_.mir_graph->ComputeDFSOrders();
    cu_.mir_graph->SSATransformationEnd();
    ASSERT_TRUE(gvn_ == nullptr);
    gvn_.reset(new (allocator_.get()) GlobalValueNumbering(&cu_, allocator_.get()));
    ASSERT_FALSE(gvn_->CanModify());
    // bool gate_result = cu_.mir_graph->GlobalValueNumberingGate();
    // ASSERT_TRUE(gate_result);
    value_names_.resize(mir_count_, 0xffffu);
    RepeatingPreOrderDfsIterator iterator(cu_.mir_graph.get());
    bool change = false;
    for (BasicBlock* bb = iterator.Next(change); bb != nullptr; bb = iterator.Next(change)) {
      LocalValueNumbering* lvn = gvn_->PrepareBasicBlock(bb);
      if (lvn != nullptr) {
        for (MIR* mir = bb->first_mir_insn; mir != nullptr; mir = mir->next) {
          value_names_[mir - mirs_] = lvn->GetValueNumber(mir);
        }
      }
      change = (lvn != nullptr) && gvn_->FinishBasicBlock(bb);
    }
    EXPECT_TRUE(gvn_->Good());
    // cu_.mir_graph->GlobalValueNumberingEnd();
  }

  GlobalValueNumberingTest()
      : pool_(),
        cu_(&pool_),
        mir_count_(0u),
        mirs_(nullptr),
        ssa_reps_(),
        allocator_(),
        gvn_(),
        value_names_() {
    cu_.mir_graph.reset(new MIRGraph(&cu_, &cu_.arena));
    cu_.access_flags = kAccStatic;  // Don't let "this" interfere with this test.
    allocator_.reset(ScopedArenaAllocator::Create(&cu_.arena_stack));
    // gvn_->AllowModifications();
  }

  ArenaPool pool_;
  CompilationUnit cu_;
  size_t mir_count_;
  MIR* mirs_;
  std::vector<SSARepresentation> ssa_reps_;
  std::unique_ptr<ScopedArenaAllocator> allocator_;
  std::unique_ptr<GlobalValueNumbering> gvn_;
  std::vector<uint16_t> value_names_;
};

TEST_F(GlobalValueNumberingTest, DiamondNonAliasingIFields) {
  static const IFieldDef ifields[] = {
      { 0u, 1u, 0u, false },  // Object.
      { 1u, 1u, 1u, false },  // Object.
      { 2u, 1u, 2u, false },  // Object.
      { 3u, 1u, 3u, false },  // Object.
      { 4u, 1u, 4u, false },  // Short.
      { 5u, 1u, 5u, false },  // Char.
      { 6u, 0u, 0u, false },  // Unresolved, Short.
      { 7u, 1u, 7u, false },  // Object.
      { 8u, 0u, 0u, false },  // Unresolved, Object.
      { 9u, 1u, 9u, false },  // Object.
      { 10u, 1u, 10u, false },  // Object.
      { 11u, 1u, 11u, false },  // Object.
      { 12u, 1u, 12u, false },  // Object.
  };
  static const BBDef bbs[] = {
      DEF_BB(kNullBlock, DEF_SUCC0(), DEF_PRED0()),
      DEF_BB(kEntryBlock, DEF_SUCC1(3), DEF_PRED0()),
      DEF_BB(kExitBlock, DEF_SUCC0(), DEF_PRED1(6)),
      DEF_BB(kDalvikByteCode, DEF_SUCC2(4, 5), DEF_PRED1(1)),  // Block #3, top of the diamond.
      DEF_BB(kDalvikByteCode, DEF_SUCC1(6), DEF_PRED1(3)),     // Block #4, left side.
      DEF_BB(kDalvikByteCode, DEF_SUCC1(6), DEF_PRED1(3)),     // Block #5, right side.
      DEF_BB(kDalvikByteCode, DEF_SUCC1(2), DEF_PRED2(4, 5)),  // Block #6, bottom.
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
      DEF_IGET(3, Instruction::IGET, 25u, 600u, 7u),
      DEF_IGET(3, Instruction::IGET, 26u, 601u, 7u),
      DEF_IPUT(4, Instruction::IPUT, 27u, 602u, 8u),  // Doesn't clobber field #7 for other refs.
      DEF_IGET(6, Instruction::IGET, 28u, 600u, 7u),  // Same as the top.
      DEF_IGET(6, Instruction::IGET, 29u, 601u, 7u),  // Same as the top.

      DEF_UNIQUE_REF(3, Instruction::NEW_INSTANCE, 700u),
      DEF_CONST(4, Instruction::CONST, 31u, 3000),
      DEF_IPUT(4, Instruction::IPUT, 31u, 700u, 9u),
      DEF_IPUT(4, Instruction::IPUT, 31u, 700u, 10u),
      DEF_CONST(5, Instruction::CONST, 34u, 3001),
      DEF_IPUT(5, Instruction::IPUT, 34u, 700u, 9u),
      DEF_IPUT(5, Instruction::IPUT, 34u, 700u, 10u),
      DEF_IGET(6, Instruction::IGET, 37u, 700u, 9u),
      DEF_IGET(6, Instruction::IGET, 38u, 700u, 10u),  // Same value as read from field #9.

      DEF_UNIQUE_REF(3, Instruction::NEW_INSTANCE, 800u),
      DEF_UNIQUE_REF(3, Instruction::NEW_INSTANCE, 801u),
      DEF_CONST(4, Instruction::CONST, 41u, 3000),
      DEF_IPUT(4, Instruction::IPUT, 41u, 800u, 11u),
      DEF_IPUT(4, Instruction::IPUT, 41u, 801u, 11u),
      DEF_CONST(5, Instruction::CONST, 44u, 3001),
      DEF_IPUT(5, Instruction::IPUT, 44u, 800u, 11u),
      DEF_IPUT(5, Instruction::IPUT, 44u, 801u, 11u),
      DEF_IGET(6, Instruction::IGET, 47u, 800u, 11u),
      DEF_IGET(6, Instruction::IGET, 48u, 801u, 11u),  // Same value as read from ref 46u.

      // Invoke doesn't interfere with non-aliasing refs. There's one test above where a reference
      // escapes in the left BB and the INVOKE in the right BB shouldn't interfere with that either.
      DEF_INVOKE1(5, Instruction::INVOKE_STATIC, 48u),
  };

  PrepareIFields(ifields);
  PrepareBasicBlocks(bbs);
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

  EXPECT_EQ(value_names_[25], value_names_[28]);
  EXPECT_EQ(value_names_[26], value_names_[29]);

  EXPECT_EQ(value_names_[37], value_names_[38]);

  EXPECT_EQ(value_names_[47], value_names_[48]);
}

TEST_F(GlobalValueNumberingTest, DiamondAliasingIFieldsSingleObject) {
  static const IFieldDef ifields[] = {
      { 0u, 1u, 0u, false },  // Object.
      { 1u, 1u, 1u, false },  // Object.
      { 2u, 1u, 2u, false },  // Object.
      { 3u, 1u, 3u, false },  // Object.
      { 4u, 1u, 4u, false },  // Short.
      { 5u, 1u, 5u, false },  // Char.
      { 6u, 0u, 0u, false },  // Unresolved, Short.
      { 7u, 1u, 7u, false },  // Object.
      { 8u, 1u, 8u, false },  // Object.
  };
  static const BBDef bbs[] = {
      DEF_BB(kNullBlock, DEF_SUCC0(), DEF_PRED0()),
      DEF_BB(kEntryBlock, DEF_SUCC1(3), DEF_PRED0()),
      DEF_BB(kExitBlock, DEF_SUCC0(), DEF_PRED1(6)),
      DEF_BB(kDalvikByteCode, DEF_SUCC2(4, 5), DEF_PRED1(1)),  // Block #3, top of the diamond.
      DEF_BB(kDalvikByteCode, DEF_SUCC1(6), DEF_PRED1(3)),     // Block #4, left side.
      DEF_BB(kDalvikByteCode, DEF_SUCC1(6), DEF_PRED1(3)),     // Block #5, right side.
      DEF_BB(kDalvikByteCode, DEF_SUCC1(2), DEF_PRED2(4, 5)),  // Block #6, bottom.
  };
  static const MIRDef mirs[] = {
      // NOTE: MIRs here are ordered by unique tests. They will be put into appropriate blocks.
      DEF_IGET(3, Instruction::IGET, 0u, 100u, 0u),
      DEF_IGET(6, Instruction::IGET, 1u, 100u, 0u),   // Same as at the top.

      DEF_IGET(4, Instruction::IGET, 2u, 100u, 1u),
      DEF_IGET(6, Instruction::IGET, 3u, 100u, 1u),   // Same as at the left side.

      DEF_IGET(3, Instruction::IGET, 4u, 100u, 2u),
      DEF_CONST(5, Instruction::CONST, 5u, 1000),
      DEF_IPUT(5, Instruction::IPUT, 5u, 100u, 2u),
      DEF_IGET(6, Instruction::IGET, 7u, 100u, 2u),   // Differs from the top and the CONST.

      DEF_IGET(3, Instruction::IGET, 8u, 100u, 3u),
      DEF_CONST(3, Instruction::CONST, 9u, 2000),
      DEF_IPUT(4, Instruction::IPUT, 9u, 100u, 3u),
      DEF_IPUT(5, Instruction::IPUT, 9u, 100u, 3u),
      DEF_IGET(6, Instruction::IGET, 12u, 100u, 3u),  // Differs from the top, equals the CONST.

      DEF_IGET(3, Instruction::IGET_SHORT, 13u, 100u, 4u),
      DEF_IGET(3, Instruction::IGET_CHAR, 14u, 100u, 5u),
      DEF_IPUT(4, Instruction::IPUT_SHORT, 15u, 100u, 6u),  // Clobbers field #4, not #5.
      DEF_IGET(6, Instruction::IGET_SHORT, 16u, 100u, 4u),  // Differs from the top.
      DEF_IGET(6, Instruction::IGET_CHAR, 17u, 100u, 5u),   // Same as the top.

      DEF_CONST(4, Instruction::CONST, 18u, 3000),
      DEF_IPUT(4, Instruction::IPUT, 18u, 100u, 7u),
      DEF_IPUT(4, Instruction::IPUT, 18u, 100u, 8u),
      DEF_CONST(5, Instruction::CONST, 21u, 3001),
      DEF_IPUT(5, Instruction::IPUT, 21u, 100u, 7u),
      DEF_IPUT(5, Instruction::IPUT, 21u, 100u, 8u),
      DEF_IGET(6, Instruction::IGET, 24u, 100u, 7u),
      DEF_IGET(6, Instruction::IGET, 25u, 100u, 8u),  // Same value as read from field #7.
  };

  PrepareIFields(ifields);
  PrepareBasicBlocks(bbs);
  PrepareMIRs(mirs);
  PerformGVN();
  ASSERT_EQ(arraysize(mirs), value_names_.size());
  EXPECT_EQ(value_names_[0], value_names_[1]);

  EXPECT_EQ(value_names_[2], value_names_[3]);

  EXPECT_NE(value_names_[4], value_names_[7]);
  EXPECT_NE(value_names_[5], value_names_[7]);

  EXPECT_NE(value_names_[8], value_names_[12]);
  EXPECT_EQ(value_names_[9], value_names_[12]);

  EXPECT_NE(value_names_[13], value_names_[16]);
  EXPECT_EQ(value_names_[14], value_names_[17]);

  // TODO EXPECT_EQ(value_names_[24], value_names_[25]);
}

TEST_F(GlobalValueNumberingTest, DiamondIFieldsTwoAliasingObjects) {
  static const IFieldDef ifields[] = {
      { 0u, 1u, 0u, false },  // Object.
      { 1u, 1u, 1u, false },  // Object.
      { 2u, 1u, 2u, false },  // Object.
      { 3u, 1u, 3u, false },  // Object.
      { 4u, 1u, 4u, false },  // Short.
      { 5u, 1u, 5u, false },  // Char.
      { 6u, 0u, 0u, false },  // Unresolved, Short.
      { 7u, 1u, 7u, false },  // Object.
      { 8u, 1u, 8u, false },  // Object.
  };
  static const BBDef bbs[] = {
      DEF_BB(kNullBlock, DEF_SUCC0(), DEF_PRED0()),
      DEF_BB(kEntryBlock, DEF_SUCC1(3), DEF_PRED0()),
      DEF_BB(kExitBlock, DEF_SUCC0(), DEF_PRED1(6)),
      DEF_BB(kDalvikByteCode, DEF_SUCC2(4, 5), DEF_PRED1(1)),  // Block #3, top of the diamond.
      DEF_BB(kDalvikByteCode, DEF_SUCC1(6), DEF_PRED1(3)),     // Block #4, left side.
      DEF_BB(kDalvikByteCode, DEF_SUCC1(6), DEF_PRED1(3)),     // Block #5, right side.
      DEF_BB(kDalvikByteCode, DEF_SUCC1(2), DEF_PRED2(4, 5)),  // Block #6, bottom.
  };
  static const MIRDef mirs[] = {
      // NOTE: MIRs here are ordered by unique tests. They will be put into appropriate blocks.
      DEF_IGET(3, Instruction::IGET, 0u, 100u, 0u),
      DEF_IPUT(4, Instruction::IPUT, 1u, 101u, 0u),   // May alias with the IGET at the top.
      DEF_IGET(6, Instruction::IGET, 2u, 100u, 0u),   // Differs from the top.

      DEF_IGET(3, Instruction::IGET, 3u, 100u, 1u),
      DEF_IPUT(5, Instruction::IPUT, 3u, 101u, 1u),   // If aliasing, stores the same value.
      DEF_IGET(6, Instruction::IGET, 5u, 100u, 1u),   // Same as the top.

      DEF_IGET(3, Instruction::IGET, 6u, 100u, 2u),
      DEF_CONST(5, Instruction::CONST, 7u, 1000),
      DEF_IPUT(5, Instruction::IPUT, 7u, 101u, 2u),
      DEF_IGET(6, Instruction::IGET, 9u, 100u, 2u),   // Differs from the top and the CONST.

      DEF_IGET(3, Instruction::IGET, 10u, 100u, 3u),
      DEF_CONST(3, Instruction::CONST, 11u, 2000),
      DEF_IPUT(4, Instruction::IPUT, 11u, 101u, 3u),
      DEF_IPUT(5, Instruction::IPUT, 11u, 101u, 3u),
      DEF_IGET(6, Instruction::IGET, 14u, 100u, 3u),  // Differs from the top and the CONST.

      DEF_IGET(3, Instruction::IGET_SHORT, 15u, 100u, 4u),
      DEF_IGET(3, Instruction::IGET_CHAR, 16u, 100u, 5u),
      DEF_IPUT(4, Instruction::IPUT_SHORT, 17u, 101u, 6u),  // Clobbers field #4, not #5.
      DEF_IGET(6, Instruction::IGET_SHORT, 18u, 100u, 4u),  // Differs from the top.
      DEF_IGET(6, Instruction::IGET_CHAR, 19u, 100u, 5u),   // Same as the top.

      DEF_CONST(4, Instruction::CONST, 20u, 3000),
      DEF_IPUT(4, Instruction::IPUT, 20u, 100u, 7u),
      DEF_IPUT(4, Instruction::IPUT, 20u, 101u, 8u),
      DEF_CONST(5, Instruction::CONST, 23u, 3001),
      DEF_IPUT(5, Instruction::IPUT, 23u, 100u, 7u),
      DEF_IPUT(5, Instruction::IPUT, 23u, 101u, 8u),
      DEF_IGET(6, Instruction::IGET, 26u, 100u, 7u),
      DEF_IGET(6, Instruction::IGET, 27u, 101u, 8u),  // Same value as read from field #7.
  };

  PrepareIFields(ifields);
  PrepareBasicBlocks(bbs);
  PrepareMIRs(mirs);
  PerformGVN();
  ASSERT_EQ(arraysize(mirs), value_names_.size());
  EXPECT_NE(value_names_[0], value_names_[2]);

  // TODO EXPECT_EQ(value_names_[3], value_names_[5]);

  EXPECT_NE(value_names_[6], value_names_[9]);
  EXPECT_NE(value_names_[7], value_names_[9]);

  EXPECT_NE(value_names_[10], value_names_[14]);
  EXPECT_NE(value_names_[10], value_names_[14]);

  EXPECT_NE(value_names_[15], value_names_[18]);
  EXPECT_EQ(value_names_[16], value_names_[19]);

  // TODO EXPECT_EQ(value_names_[26], value_names_[27]);
}

TEST_F(GlobalValueNumberingTest, DiamondSFields) {
  static const SFieldDef sfields[] = {
      { 0u, 1u, 0u, false },  // Object.
      { 1u, 1u, 1u, false },  // Object.
      { 2u, 1u, 2u, false },  // Object.
      { 3u, 1u, 3u, false },  // Object.
      { 4u, 1u, 4u, false },  // Short.
      { 5u, 1u, 5u, false },  // Char.
      { 6u, 0u, 0u, false },  // Unresolved, Short.
      { 7u, 1u, 7u, false },  // Object.
      { 8u, 1u, 8u, false },  // Object.
  };
  static const BBDef bbs[] = {
      DEF_BB(kNullBlock, DEF_SUCC0(), DEF_PRED0()),
      DEF_BB(kEntryBlock, DEF_SUCC1(3), DEF_PRED0()),
      DEF_BB(kExitBlock, DEF_SUCC0(), DEF_PRED1(6)),
      DEF_BB(kDalvikByteCode, DEF_SUCC2(4, 5), DEF_PRED1(1)),  // Block #3, top of the diamond.
      DEF_BB(kDalvikByteCode, DEF_SUCC1(6), DEF_PRED1(3)),     // Block #4, left side.
      DEF_BB(kDalvikByteCode, DEF_SUCC1(6), DEF_PRED1(3)),     // Block #5, right side.
      DEF_BB(kDalvikByteCode, DEF_SUCC1(2), DEF_PRED2(4, 5)),  // Block #6, bottom.
  };
  static const MIRDef mirs[] = {
      // NOTE: MIRs here are ordered by unique tests. They will be put into appropriate blocks.
      DEF_SGET(3, Instruction::SGET, 0u, 0u),
      DEF_SGET(6, Instruction::SGET, 1u, 0u),         // Same as at the top.

      DEF_SGET(4, Instruction::SGET, 2u, 1u),
      DEF_SGET(6, Instruction::SGET, 3u, 1u),         // Same as at the left side.

      DEF_SGET(3, Instruction::SGET, 4u, 2u),
      DEF_CONST(5, Instruction::CONST, 5u, 100),
      DEF_SPUT(5, Instruction::SPUT, 5u, 2u),
      DEF_SGET(6, Instruction::SGET, 7u, 2u),         // Differs from the top and the CONST.

      DEF_SGET(3, Instruction::SGET, 8u, 3u),
      DEF_CONST(3, Instruction::CONST, 9u, 200),
      DEF_SPUT(4, Instruction::SPUT, 9u, 3u),
      DEF_SPUT(5, Instruction::SPUT, 9u, 3u),
      DEF_SGET(6, Instruction::SGET, 12u, 3u),        // Differs from the top, equals the CONST.

      DEF_SGET(3, Instruction::SGET_SHORT, 13u, 4u),
      DEF_SGET(3, Instruction::SGET_CHAR, 14u, 5u),
      DEF_SPUT(4, Instruction::SPUT_SHORT, 15u, 6u),  // Clobbers field #4, not #5.
      DEF_SGET(6, Instruction::SGET_SHORT, 16u, 4u),  // Differs from the top.
      DEF_SGET(6, Instruction::SGET_CHAR, 17u, 5u),   // Same as the top.

      DEF_CONST(4, Instruction::CONST, 18u, 300),
      DEF_SPUT(4, Instruction::SPUT, 18u, 7u),
      DEF_SPUT(4, Instruction::SPUT, 18u, 8u),
      DEF_CONST(5, Instruction::CONST, 21u, 301),
      DEF_SPUT(5, Instruction::SPUT, 21u, 7u),
      DEF_SPUT(5, Instruction::SPUT, 21u, 8u),
      DEF_SGET(6, Instruction::SGET, 24u, 7u),
      DEF_SGET(6, Instruction::SGET, 25u, 8u),        // Same value as read from field #7.
  };

  PrepareSFields(sfields);
  PrepareBasicBlocks(bbs);
  PrepareMIRs(mirs);
  PerformGVN();
  ASSERT_EQ(arraysize(mirs), value_names_.size());
  EXPECT_EQ(value_names_[0], value_names_[1]);

  EXPECT_EQ(value_names_[2], value_names_[3]);

  EXPECT_NE(value_names_[4], value_names_[7]);
  EXPECT_NE(value_names_[5], value_names_[7]);

  EXPECT_NE(value_names_[8], value_names_[12]);
  EXPECT_EQ(value_names_[9], value_names_[12]);

  EXPECT_NE(value_names_[13], value_names_[16]);
  EXPECT_EQ(value_names_[14], value_names_[17]);

  EXPECT_EQ(value_names_[24], value_names_[25]);
}

TEST_F(GlobalValueNumberingTest, DiamondArrays) {
  // TODO
}

TEST_F(GlobalValueNumberingTest, LoopIFields) {
  // TODO
}

TEST_F(GlobalValueNumberingTest, LoopSFields) {
  static const SFieldDef sfields[] = {
      { 0u, 1u, 0u, false },  // Object.
      { 1u, 1u, 1u, false },  // Object.
      { 2u, 1u, 2u, false },  // Object.
  };
  static const BBDef bbs[] = {
      DEF_BB(kNullBlock, DEF_SUCC0(), DEF_PRED0()),
      DEF_BB(kEntryBlock, DEF_SUCC1(3), DEF_PRED0()),
      DEF_BB(kExitBlock, DEF_SUCC0(), DEF_PRED1(5)),
      DEF_BB(kDalvikByteCode, DEF_SUCC1(4), DEF_PRED1(1)),
      DEF_BB(kDalvikByteCode, DEF_SUCC2(5, 4), DEF_PRED2(3, 4)),  // "taken" loops to self.
      DEF_BB(kDalvikByteCode, DEF_SUCC1(2), DEF_PRED1(4)),
  };
  static const MIRDef mirs[] = {
      // NOTE: MIRs here are ordered by unique tests. They will be put into appropriate blocks.
      DEF_SGET(3, Instruction::SGET, 0u, 0u),
      DEF_SGET(4, Instruction::SGET, 1u, 0u),         // Same as at the top.
      DEF_SGET(5, Instruction::SGET, 2u, 0u),         // Same as at the top.

      DEF_SGET(3, Instruction::SGET, 3u, 1u),
      DEF_SGET(4, Instruction::SGET, 4u, 1u),         // Differs from top...
      DEF_SPUT(4, Instruction::SPUT, 5u, 1u),         // Because of this SPUT.
      DEF_SGET(5, Instruction::SGET, 6u, 1u),         // Differs from top and the loop SGET.

      DEF_SGET(3, Instruction::SGET, 7u, 2u),
      DEF_SPUT(4, Instruction::SPUT, 8u, 2u),         // Because of this SPUT...
      DEF_SGET(4, Instruction::SGET, 9u, 2u),         // Differs from top.
      DEF_SGET(5, Instruction::SGET, 10u, 2u),        // Differs from top but same as the loop SGET.
  };

  PrepareSFields(sfields);
  PrepareBasicBlocks(bbs);
  PrepareMIRs(mirs);
  PerformGVN();
  ASSERT_EQ(arraysize(mirs), value_names_.size());
  EXPECT_EQ(value_names_[0], value_names_[1]);
  EXPECT_EQ(value_names_[0], value_names_[2]);

  EXPECT_NE(value_names_[3], value_names_[4]);
  EXPECT_NE(value_names_[3], value_names_[6]);
  EXPECT_NE(value_names_[4], value_names_[5]);

  EXPECT_NE(value_names_[7], value_names_[9]);
  EXPECT_EQ(value_names_[9], value_names_[10]);
}

TEST_F(GlobalValueNumberingTest, LoopArrays) {
  // TODO
}

TEST_F(GlobalValueNumberingTest, CatchIFields) {
  // TODO
}

TEST_F(GlobalValueNumberingTest, CatchSFields) {
  static const SFieldDef sfields[] = {
      { 0u, 1u, 0u, false },
      { 1u, 1u, 1u, false },
  };
  static const BBDef bbs[] = {
      DEF_BB(kNullBlock, DEF_SUCC0(), DEF_PRED0()),
      DEF_BB(kEntryBlock, DEF_SUCC1(3), DEF_PRED0()),
      DEF_BB(kExitBlock, DEF_SUCC0(), DEF_PRED1(5)),
      DEF_BB(kDalvikByteCode, DEF_SUCC2(5, 4), DEF_PRED1(1)),
      DEF_BB(kDalvikByteCode, DEF_SUCC1(5), DEF_PRED1(3)),  // Catch handler.
      DEF_BB(kDalvikByteCode, DEF_SUCC1(2), DEF_PRED2(3, 4)),
  };
  static const MIRDef mirs[] = {
      DEF_SGET(3, Instruction::SGET, 0u, 0u),
      DEF_SPUT(3, Instruction::SPUT, 0u, 1u),
      DEF_SGET(4, Instruction::SGET, 2u, 0u),         // Differs from the top.
      DEF_SPUT(4, Instruction::SPUT, 2u, 1u),
      DEF_SGET(5, Instruction::SGET, 4u, 0u),         // Differs from both SGETs above.
      DEF_SGET(5, Instruction::SGET, 5u, 1u),         // Same as field #1.
  };
  PrepareSFields(sfields);
  PrepareBasicBlocks(bbs);
  BasicBlock* catch_handler = cu_.mir_graph->GetBasicBlock(4u);
  catch_handler->catch_entry = true;
  PrepareMIRs(mirs);
  PerformGVN();
  ASSERT_EQ(arraysize(mirs), value_names_.size());
  EXPECT_NE(value_names_[0], value_names_[2]);
  EXPECT_NE(value_names_[0], value_names_[4]);
  EXPECT_NE(value_names_[2], value_names_[4]);
  EXPECT_EQ(value_names_[4], value_names_[5]);
}

TEST_F(GlobalValueNumberingTest, CatchArrays) {
  // TODO
}

TEST_F(GlobalValueNumberingTest, NullCheckIFields) {
  // TODO
}

TEST_F(GlobalValueNumberingTest, NullCheckSFields) {
  // TODO
}

TEST_F(GlobalValueNumberingTest, NullCheckArrays) {
  // TODO
}

TEST_F(GlobalValueNumberingTest, RangeCheckArrays) {
  // TODO
}

TEST_F(GlobalValueNumberingTest, MergeSameValueInDifferentMemoryLocations) {
  // TODO
}

}  // namespace art
