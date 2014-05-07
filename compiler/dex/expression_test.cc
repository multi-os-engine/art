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

#include <vector>

#include "compiler_internals.h"
#include "dataflow_iterator.h"
#include "dataflow_iterator-inl.h"
#include "expression.h"
#include "gtest/gtest.h"
#include "pass_driver.h"

namespace art {

class ExpressionTest : public testing::Test {
 protected:
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
    Instruction::Code opcode;
    BasicBlockId bbid;
    uint32_t vA;
    uint32_t vB;
    uint64_t vB_wide;
    uint32_t vC;
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
#define DEF_MIR(opcode, bb, va, vb, vc) \
    { opcode, bb, va, vb, 0, vc }
#define DEF_WIDE_MIR(opcode, bb, va, vb_wide, vc) \
    { opcode, bb, va, 0, vb_wide, vc }


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

  void DoPrepareSSA(const BBDef* defs, size_t count) {
    ASSERT_LT(3u, count);  // null, entry, exit and at least one bytecode block.
    ASSERT_EQ(kNullBlock, defs[0].type);
    ASSERT_EQ(kEntryBlock, defs[1].type);
    ASSERT_EQ(kExitBlock, defs[2].type);

    // Just put a high number here for our tests.
    cu_.num_dalvik_registers = 500;
    cu_.mir_graph->CompilerInitializeSSAConversion();
    for (size_t i = 0u; i != count; ++i) {
      BasicBlock* bb = cu_.mir_graph->GetBasicBlock(i);
      if (bb != nullptr) {
        for (MIR* mir = bb->first_mir_insn; mir != nullptr; mir = mir->next) {
          cu_.mir_graph->DoSSAConversion(bb);
        }
      }
    }
  }

  template <size_t count>
  void PrepareSSA(const BBDef (&defs)[count]) {
    DoPrepareSSA(defs, count);
  }

  void DoPrepareMIRs(const MIRDef* defs, size_t count) {
    mir_count_ = count;
    mirs_ = reinterpret_cast<MIR*>(cu_.arena.Alloc(sizeof(MIR) * count, kArenaAllocMIR));
    uint64_t merged_df_flags = 0u;
    for (size_t i = 0u; i != count; ++i) {
      const MIRDef* def = &defs[i];
      MIR* mir = &mirs_[i];
      mir->dalvikInsn.opcode = def->opcode;
      mir->dalvikInsn.vA = def->vA;
      mir->dalvikInsn.vB_wide = def->vB_wide;
      mir->dalvikInsn.vB = def->vB;
      mir->dalvikInsn.vC = def->vC;
      ASSERT_LT(def->bbid, cu_.mir_graph->block_list_.Size());
      BasicBlock* bb = cu_.mir_graph->block_list_.Get(def->bbid);
      bb->AppendMIR(mir);
      mir->ssa_rep = nullptr;
      mir->offset = 2 * i;  // All insns need to be at least 2 code units long.
      mir->width = 2u;
      mir->optimization_flags = 0u;
      merged_df_flags |= MIRGraph::GetDataFlowAttributes(def->opcode);
    }
    cu_.mir_graph->merged_df_flags_ = merged_df_flags;

    code_item_ = static_cast<DexFile::CodeItem*>(
        cu_.arena.Alloc(sizeof(DexFile::CodeItem), kArenaAllocMisc));
    memset(code_item_, 0, sizeof(DexFile::CodeItem));
    code_item_->insns_size_in_code_units_ = 2u * count;
    cu_.mir_graph->current_code_item_ = cu_.code_item = code_item_;
  }

  template <size_t count>
  void PrepareMIRs(const MIRDef (&defs)[count]) {
    DoPrepareMIRs(defs, count);
  }

  void PerformExpressionTest() {
    BasicBlock* bb = cu_.mir_graph->GetBasicBlock(3);

    // Get all the MIRs in that BasicBlock
    std::vector<MIR*> list;
    for (MIR* mir = bb->first_mir_insn; mir != nullptr; mir = mir->next) {
      list.push_back(mir);
    }

    // Now create an expression out of it.
    result = Expression::MirsToExpressions(&cu_.arena, list);
  }

  ExpressionTest()
      : pool_(),
        cu_(&pool_),
        mir_count_(0u),
        mirs_(nullptr),
        code_item_(nullptr) {
    cu_.mir_graph.reset(new MIRGraph(&cu_, &cu_.arena));
  }

  ArenaPool pool_;
  CompilationUnit cu_;
  size_t mir_count_;
  MIR* mirs_;
  DexFile::CodeItem* code_item_;
  std::vector<SSARepresentation> ssa_reps_;
  std::map<MIR*, Expression*> result;
};

TEST_F(ExpressionTest, SimpleALU) {
  static const BBDef bbs[] = {
      DEF_BB(kNullBlock, DEF_SUCC0(), DEF_PRED0()),
      DEF_BB(kEntryBlock, DEF_SUCC1(3), DEF_PRED0()),
      DEF_BB(kExitBlock, DEF_SUCC0(), DEF_PRED1(3)),
      DEF_BB(kDalvikByteCode, DEF_SUCC1(2), DEF_PRED1(1)),
  };
  static const MIRDef mirs[] = {
      DEF_MIR(Instruction::ADD_INT, 3u, 0u, 5u, 3u),
      DEF_MIR(Instruction::MUL_INT, 3u, 0u, 0u, 3u),
      DEF_MIR(Instruction::ADD_INT, 3u, 0u, 0u, 4u),
  };

  struct Result {
    uint8_t index;
    const char* result;
  };

  static const Result results[] = {
        {2, "(v0 = ((v0 = ((v0 = (v5 + v3)) * v3)) + v4))"}
  };

  PrepareBasicBlocks(bbs);
  PrepareMIRs(mirs);
  PrepareSSA(bbs);
  PerformExpressionTest();

  PrepareBasicBlocks(bbs);
  PrepareMIRs(mirs);
  PrepareSSA(bbs);
  PerformExpressionTest();

  for (size_t i = 0u; i != arraysize(results); ++i) {
    MIR& mir = mirs_[results[i].index];
    Expression* root = result[&mir];
    ASSERT_NE(root, nullptr);
    const char* expected = results[i].result;
    EXPECT_EQ(strcmp(expected, root->ToString(&cu_).c_str()), 0);
  }
}

TEST_F(ExpressionTest, DoubleALU) {
  static const BBDef bbs[] = {
      DEF_BB(kNullBlock, DEF_SUCC0(), DEF_PRED0()),
      DEF_BB(kEntryBlock, DEF_SUCC1(3), DEF_PRED0()),
      DEF_BB(kExitBlock, DEF_SUCC0(), DEF_PRED1(3)),
      DEF_BB(kDalvikByteCode, DEF_SUCC1(2), DEF_PRED1(1)),
  };
  static const MIRDef mirs[] = {
      DEF_MIR(Instruction::CONST, 3u, 6u, 21u, 0u),
      DEF_MIR(Instruction::MUL_INT, 3u, 7u, 6u, 6u),
      DEF_MIR(Instruction::ADD_INT, 3u, 7u, 9u, 7u),
      DEF_MIR(Instruction::CONST, 3u, 3u, 42u, 0u),
      DEF_MIR(Instruction::ADD_INT, 3u, 1u, 1u, 3u),
  };

  struct Result {
    uint8_t index;
    const char* result;
  };

  static const Result results[] = {
        {2, "(v7 = (v9 + (v7 = ((v6 = 21) * (v6 = 21)))))"},
        {4, "(v1 = (v1 + (v3 = 42)))"}
  };

  PrepareBasicBlocks(bbs);
  PrepareMIRs(mirs);
  PrepareSSA(bbs);
  PerformExpressionTest();

  for (size_t i = 0u; i != arraysize(results); ++i) {
    MIR& mir = mirs_[results[i].index];
    Expression* root = result[&mir];
    ASSERT_NE(root, nullptr);
    const char* expected = results[i].result;
    EXPECT_EQ(strcmp(expected, root->ToString(&cu_).c_str()), 0);
  }
}

TEST_F(ExpressionTest, InterleavedALU) {
  static const BBDef bbs[] = {
      DEF_BB(kNullBlock, DEF_SUCC0(), DEF_PRED0()),
      DEF_BB(kEntryBlock, DEF_SUCC1(3), DEF_PRED0()),
      DEF_BB(kExitBlock, DEF_SUCC0(), DEF_PRED1(3)),
      DEF_BB(kDalvikByteCode, DEF_SUCC1(2), DEF_PRED1(1)),
  };
  static const MIRDef mirs[] = {
      DEF_MIR(Instruction::CONST, 3u, 6u, 21u, 0u),
      DEF_MIR(Instruction::CONST, 3u, 3u, 42u, 0u),
      DEF_MIR(Instruction::MUL_INT, 3u, 7u, 6u, 6u),
      DEF_MIR(Instruction::ADD_INT, 3u, 1u, 1u, 3u),
      DEF_MIR(Instruction::ADD_INT, 3u, 7u, 9u, 7u),
  };

  struct Result {
    uint8_t index;
    const char* result;
  };

  static const Result results[] = {
        {3, "(v1 = (v1 + (v3 = 42)))"},
        {4, "(v7 = (v9 + (v7 = ((v6 = 21) * (v6 = 21)))))"}
  };

  PrepareBasicBlocks(bbs);
  PrepareMIRs(mirs);
  PrepareSSA(bbs);
  PerformExpressionTest();

  for (size_t i = 0u; i != arraysize(results); ++i) {
    MIR& mir = mirs_[results[i].index];
    Expression* root = result[&mir];
    ASSERT_NE(root, nullptr);
    const char* expected = results[i].result;
    EXPECT_EQ(strcmp(expected, root->ToString(&cu_).c_str()), 0);
  }
}

TEST_F(ExpressionTest, WithCastAndLong) {
  static const BBDef bbs[] = {
      DEF_BB(kNullBlock, DEF_SUCC0(), DEF_PRED0()),
      DEF_BB(kEntryBlock, DEF_SUCC1(3), DEF_PRED0()),
      DEF_BB(kExitBlock, DEF_SUCC0(), DEF_PRED1(3)),
      DEF_BB(kDalvikByteCode, DEF_SUCC1(2), DEF_PRED1(1)),
  };
  static const MIRDef mirs[] = {
      DEF_WIDE_MIR(Instruction::CONST_WIDE, 3u, 6u, 21u, 0u),
      DEF_MIR(Instruction::CONST, 3u, 3u, 42u, 0u),
      DEF_MIR(Instruction::ADD_INT, 3u, 1u, 1u, 3u),
      DEF_MIR(Instruction::MUL_LONG, 3u, 8u, 6u, 6u),
      DEF_MIR(Instruction::INT_TO_FLOAT, 3u, 12u, 1u, 0u),
      DEF_MIR(Instruction::DIV_FLOAT, 3u, 10u, 12u, 14u),
      DEF_MIR(Instruction::ADD_LONG, 3u, 16u, 8u, 18u),
  };

  struct Result {
    uint8_t index;
    const char* result;
  };

  static const Result results[] = {
        {5, "(v10 = ((v12 = (cast)(v1 = (v1 + (v3 = 42)))) / v14))"},
        {6, "(v16, v17 = ((v8, v9 = ((v6, v7 = 21) * (v6, v7 = 21))) + v18, v19))"}
  };

  PrepareBasicBlocks(bbs);
  PrepareMIRs(mirs);
  PrepareSSA(bbs);
  PerformExpressionTest();

  for (size_t i = 0u; i != arraysize(results); ++i) {
    MIR& mir = mirs_[results[i].index];
    Expression* root = result[&mir];
    ASSERT_NE(root, nullptr);
    const char* expected = results[i].result;
    EXPECT_EQ(strcmp(expected, root->ToString(&cu_).c_str()), 0);
  }
}

TEST_F(ExpressionTest, Redefines) {
  static const BBDef bbs[] = {
      DEF_BB(kNullBlock, DEF_SUCC0(), DEF_PRED0()),
      DEF_BB(kEntryBlock, DEF_SUCC1(3), DEF_PRED0()),
      DEF_BB(kExitBlock, DEF_SUCC0(), DEF_PRED1(3)),
      DEF_BB(kDalvikByteCode, DEF_SUCC1(2), DEF_PRED1(1)),
  };
  static const MIRDef mirs[] = {
      DEF_WIDE_MIR(Instruction::CONST_WIDE, 3u, 6u, 21u, 0u),
      DEF_MIR(Instruction::CONST, 3u, 3u, 42u, 0u),
      DEF_MIR(Instruction::ADD_INT, 3u, 1u, 1u, 3u),
      DEF_MIR(Instruction::MUL_LONG, 3u, 8u, 6u, 6u),
      DEF_MIR(Instruction::INT_TO_FLOAT, 3u, 12u, 1u, 0u),
      DEF_MIR(Instruction::DIV_FLOAT, 3u, 10u, 12u, 14u),
      DEF_MIR(Instruction::ADD_LONG, 3u, 16u, 8u, 18u),
      DEF_MIR(Instruction::MUL_FLOAT, 3u, 10u, 20u, 21u),
      DEF_MIR(Instruction::ADD_INT, 3u, 16u, 22u, 22u),
  };

  struct Result {
    uint8_t index;
    const char* result;
  };

  static const Result results[] = {
        {5, "(v10 = ((v12 = (cast)(v1 = (v1 + (v3 = 42)))) / v14))"},
        {6, "(v16, v17 = ((v8, v9 = ((v6, v7 = 21) * (v6, v7 = 21))) + v18, v19))"},
        {7, "(v10 = (v20 * v21))"},
        {8, "(v16 = (v22 + v22))"}
  };

  PrepareBasicBlocks(bbs);
  PrepareMIRs(mirs);
  PrepareSSA(bbs);
  PerformExpressionTest();

  for (size_t i = 0u; i != arraysize(results); ++i) {
    MIR& mir = mirs_[results[i].index];
    Expression* root = result[&mir];
    ASSERT_NE(root, nullptr);
    const char* expected = results[i].result;
    EXPECT_EQ(strcmp(expected, root->ToString(&cu_).c_str()), 0);
  }
}

}  // namespace art
