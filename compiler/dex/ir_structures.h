/*
 * Copyright (C) 2013 The Android Open Source Project
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

#ifndef ART_COMPILER_DEX_BASIC_BLOCK_H_
#define ART_COMPILER_DEX_BASIC_BLOCK_H_

#include <stdint.h>

#include "dex_instruction.h"
#include "compiler_ir.h"

namespace art {

class ArenaBitVector;

typedef uint16_t BasicBlockId;
static const BasicBlockId NullBasicBlockId = 0;

// Dataflow attributes of a basic block.
struct BasicBlockDataFlow {
  ArenaBitVector* use_v;
  ArenaBitVector* def_v;
  ArenaBitVector* live_in_v;
  ArenaBitVector* phi_v;
  int32_t* vreg_to_ssa_map;
  ArenaBitVector* ending_null_check_v;
};

/*
 * Normalized use/def for a MIR operation using SSA names rather than vregs.  Note that
 * uses/defs retain the Dalvik convention that long operations operate on a pair of 32-bit
 * vregs.  For example, "ADD_LONG v0, v2, v3" would have 2 defs (v0/v1) and 4 uses (v2/v3, v4/v5).
 * Following SSA renaming, this is the primary struct used by code generators to locate
 * operand and result registers.  This is a somewhat confusing and unhelpful convention that
 * we may want to revisit in the future.
 */
struct SSARepresentation {
  int16_t num_uses;
  int16_t num_defs;
  int32_t* uses;
  bool* fp_use;
  int32_t* defs;
  bool* fp_def;
};

/*
 * The Midlevel Intermediate Representation node, which may be largely considered a
 * wrapper around a Dalvik byte code.
 */
struct MIR {
  /*
   * TODO: remove embedded DecodedInstruction to save space, keeping only opcode.  Recover
   * additional fields on as-needed basis.  Question: how to support MIR Pseudo-ops; probably
   * need to carry aux data pointer.
   */
  DecodedInstruction dalvikInsn;
  uint16_t width;                 // Note: width can include switch table or fill array data.
  NarrowDexOffset offset;         // Offset of the instruction in code units.
  uint16_t optimization_flags;
  int16_t m_unit_index;           // From which method was this MIR included
  BasicBlock *bb;
  // TTODO: handle this to set it right
  MIR *prev;
  MIR* next;
  SSARepresentation* ssa_rep;
  union {
    // Incoming edges for phi node.
    BasicBlockId* phi_incoming;
    // Establish link from check instruction (kMirOpCheck) to the actual throwing instruction.
    MIR* throw_insn;
    // Fused cmp branch condition.
    ConditionCode ccode;
  } meta;

 bool RemoveFromBasicBlock();
 bool IsConditionalBranch() const;
};

struct SuccessorBlockInfo;

struct BasicBlock {
  BasicBlockId id;
  BasicBlockId dfs_id;
  NarrowDexOffset start_offset;     // Offset in code units.
  BasicBlockId fall_through;
  BasicBlockId taken;
  BasicBlockId i_dom;               // Immediate dominator.
  uint16_t nesting_depth;
  BBType block_type:4;
  BlockListType successor_block_list_type:4;
  bool visited:1;
  bool hidden:1;
  bool catch_entry:1;
  bool explicit_throw:1;
  bool conditional_branch:1;
  bool terminated_by_return:1;  // Block ends with a Dalvik return opcode.
  bool dominates_return:1;      // Is a member of return extended basic block.
  bool use_lvn:1;               // Run local value numbering on this block.
  MIR* first_mir_insn;
  MIR* last_mir_insn;
  BasicBlockDataFlow* data_flow_info;
  ArenaBitVector* dominators;
  ArenaBitVector* i_dominated;      // Set nodes being immediately dominated.
  ArenaBitVector* dom_frontier;     // Dominance frontier.
  GrowableArray<BasicBlockId>* predecessors;
  GrowableArray<SuccessorBlockInfo*>* successor_blocks;

  bool RemoveMIR(MIR* mir);
  void ResetOptimizationFlags(uint16_t reset_flags);
  void Hide(CompilationUnit *c_unit);
};

/*
 * The "blocks" field in "successor_block_list" points to an array of elements with the type
 * "SuccessorBlockInfo".  For catch blocks, key is type index for the exception.  For swtich
 * blocks, key is the case value.
 */
struct SuccessorBlockInfo {
  BasicBlockId block;
  int key;
};

/**
 * @class ChildBlockIterator
 * @brief Enable an easy iteration of the children
 */
class ChildBlockIterator {
 public:
  /**
   * @brief Constructs a child iterator.
   * @param bb The basic whose children we need to iterate through.
   * @param mir_graph The MIRGraph used to get the arena for allocation.
   */
  ChildBlockIterator(BasicBlock* bb, MIRGraph* mir_graph);

  /**
   * @brief Used to obtain a pointer to unvisited child.
   * @return Returns pointer to an unvisited child. When all children are visited it returns null.
   */
  BasicBlock* GetNextChildPtr();

 private:
  /**
   * @brief Used to keep track of the basic block whose children we are visiting.
   */
  BasicBlock* basic_block_;

  /**
   * @brief Keep track of the mir_graph the basic blocks belong to.
   */
  MIRGraph* mir_graph_;

  /**
   * @brief Whether we visited fallthrough child.
   */
  bool visited_fallthrough_;

  /**
   * @brief Whether we visited taken child.
   */
  bool visited_taken_;

  /**
   * @brief Whether we have blocks to visit in the successor list.
   */
  bool have_successors_;

  /**
   * @brief Used to iterate through the block's successor list.
   */
  GrowableArray<SuccessorBlockInfo*>::Iterator* successor_iter_;
};

}  // namespace art
#endif
