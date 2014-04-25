/*
 * Copyright(C) 2014 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0(the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ART_COMPILER_DEX_LOOP_INFORMATION_H_
#define ART_COMPILER_DEX_LOOP_INFORMATION_H_

#include "compiler_internals.h"
#include "utils/arena_allocator.h"

namespace art {
// Forward declaration
class Expression;
class InductionVariable;

class LoopInformation {
 protected:
  CompilationUnit* c_unit_;

  LoopInformation* parent_;           /**< @brief Outer loop link. */

  LoopInformation* sibling_next_;     /**< @brief Sibling loops of this level of nesting. */
  LoopInformation* sibling_previous_; /**< @brief Sibling loops of this level of nesting. */

  LoopInformation* nested_;          /**< @brief First inner loop link. */

  unsigned int depth_;               /**< @brief Depth of this loop. */
  ArenaBitVector* basic_blocks_;     /**< @brief BasicBlocks of this loop. */

  BasicBlock* entry_;                /**< @brief Entry block, first block of the loop. */
  BasicBlock* pre_header_;           /**< @brief Preheader block:  all external blocks go through this one before going to entry. */
  ArenaBitVector* exit_loop_;        /**< @brief Added exit blocks of the loop, for sinking code. */
  ArenaBitVector* backward_;         /**< @brief Blocks that return to the entry. */
  ArenaBitVector* post_exit_blocks_; /**< @brief Blocks after the exit blocks. */

  /**
   * @brief Hook for allocation of LoopInformation extension.
   */
  static LoopInformation* (*allocate_loop_information_)(CompilationUnit* c_unit);

  LoopInformation() {}
  void Init(CompilationUnit *c_unit);

  /*
   * @brief Get information about the loop, required by the form loop pass.
   */
  static ArenaBitVector* GetLoopTailBlocks(CompilationUnit *c_unit, BasicBlock *bb);
  static bool GetAllBBInLoop(CompilationUnit *cUnit, BasicBlock *entry_, ArenaBitVector* tailblocks, ArenaBitVector *basic_blocks);
  static void GetOutsFromLoop(CompilationUnit *cUnit, ArenaBitVector* basic_blocks, ArenaBitVector *exit_blocks);

  /**
   * @brief Dumping information about a loop.
   */
  static bool DumpInformationHelper(LoopInformation *info, void *data);

 public:
  explicit LoopInformation(CompilationUnit *c_unit);

  ~LoopInformation();

  static void* operator new(size_t size, ArenaAllocator* arena) {
    return arena->Alloc(sizeof(LoopInformation), kArenaAllocGrowableBitMap);
  }

  void SetLoopInformationAllocator(LoopInformation* (*fct)(CompilationUnit* c_unit)) {
    allocate_loop_information_ = fct;
  }

  static void operator delete(void* p) {}  // Nop.

  static LoopInformation* GetLoopInformation(CompilationUnit *cUnit, LoopInformation* current = 0);

  CompilationUnit* GetCompilationUnit() const {
    return c_unit_;
  }

  LoopInformation* GetParent() const {
    return parent_;
  }

  LoopInformation* GetNextSibling() const {
    return sibling_next_;
  }

  LoopInformation* GetPrevSibling() const {
    return sibling_previous_;
  }

  LoopInformation* GetNested() const {
    return nested_;
  }

  BasicBlock* GetEntryBlock() const {
    return entry_;
  }

  void SetEntryBlock(BasicBlock* bb) {
    entry_ = bb;
  }

  BasicBlock* GetPreHeader() const {
    return pre_header_;
  }

  int GetDepth() const {
    return depth_;
  }

  void SetDepth(int depth_);
  ArenaBitVector* GetExitLoops() const {
    return exit_loop_;
  }

  ArenaBitVector* GetPostExitLoops() const {
    return post_exit_blocks_;
  }

  ArenaBitVector* GetBasicBlocks() const {
    return basic_blocks_;
  }

  ArenaBitVector* GetBackwardBranches() const {
    return backward_;
  }

  BasicBlock* GetPostExitBlock() const;
  BasicBlock* GetBackwardBranchBlock() const;
  BasicBlock* GetExitBlock() const;

  /**
   * @brief Add a loop to the nested loop, it returns the outer loop.
   * @details Hence this can be the inner loop and we add the outer loop.
   *          The hierarchy is created and the function returns the outer loop.
   */
  LoopInformation* Add(LoopInformation* info);

  /*
   * @brief Find a loop functions depending on BasicBlock we want.
   */
  LoopInformation* GetLoopInformationByEntry(const BasicBlock* entry);
  LoopInformation* GetLoopInformationByBasicBlock(const BasicBlock* block);

  /** 
   * @brief Is bb a preheader/exit block?
   */
  bool IsBasicBlockALoopHelper(const BasicBlock* bb);
  bool Contains(const BasicBlock* bb) const;

  void DumpInformation(unsigned int tab = 0);
  void DumpInformationDotFormat(FILE* file);

  bool CanThrow() const;
  void AddInstructionToExits(MIR* mir);
  void AddInstructionsToExits(const std::vector<MIR*> &mirs);

  typedef bool (*fctPtr) (LoopInformation*, void*);
  bool Iterate(fctPtr fct, void* data = nullptr);

  typedef bool (*bbIteratorFctPtr) (CompilationUnit* c_unit, BasicBlock* bb, void* data);
  bool IterateThroughBlocks(bbIteratorFctPtr fct, ArenaBitVector* bv, void *data = nullptr);
  bool IterateThroughLoopBasicBlocks(bbIteratorFctPtr fct, void *data = nullptr);
  bool IterateThroughLoopExitBlocks(bbIteratorFctPtr fct, void *data = nullptr);

  MIR *GetPhiInstruction(const CompilationUnit *c_unit, int vr) const;

  bool CalculateBasicBlockInformation();

  MIR* GetPhiInstruction(int reg) const;
  bool HasInvoke() const;

  bool ExecutedPerIteration(MIR* mir) const;
  bool ExecutedPerIteration(const BasicBlock* bb) const;

  void DumpDot(FILE* file);
  static bool DumpDotHelper(LoopInformation* info, void* data);
};
}  // namespace art

#endif  // ART_COMPILER_DEX_LOOP_INFORMATION_H_
