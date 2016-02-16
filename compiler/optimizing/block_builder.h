/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ART_COMPILER_OPTIMIZING_BLOCK_BUILDER_H_
#define ART_COMPILER_OPTIMIZING_BLOCK_BUILDER_H_

#include "base/arena_containers.h"
#include "base/arena_object.h"
#include "dex_file.h"
#include "nodes.h"

namespace art {

class HBasicBlockBuilder : public ValueObject {
 public:
  HBasicBlockBuilder(HGraph* graph,
                     const DexFile* const dex_file,
                     const DexFile::CodeItem& code_item)
      : arena_(graph->GetArena()),
        graph_(graph),
        dex_file_(dex_file),
        code_item_(code_item),
        branch_targets_(code_item.insns_size_in_code_units_,
                        nullptr,
                        arena_->Adapter(kArenaAllocGraphBuilder)),
        number_of_branches_(0u) {}

  bool Build();

  size_t GetNumberOfBranches() const { return number_of_branches_; }
  HBasicBlock* GetBlockAt(uint32_t dex_pc) const { return branch_targets_[dex_pc]; }

 private:
  HBasicBlock* MaybeCreateBlockAt(uint32_t dex_pc);
  HBasicBlock* MaybeCreateBlockAt(uint32_t dex_pc, uint32_t semantic_dex_pc);

  bool CreateBranchTargets();
  void ConnectBasicBlocks();
  void InsertTryBoundaryBlocks();

  bool MightHaveLiveNormalPredecessors(HBasicBlock* catch_block);
  bool ContainsThrowingInstructions(HBasicBlock* block);

  ArenaAllocator* const arena_;
  HGraph* const graph_;

  const DexFile* const dex_file_;
  const DexFile::CodeItem& code_item_;

  ArenaVector<HBasicBlock*> branch_targets_;
  size_t number_of_branches_;

  DISALLOW_COPY_AND_ASSIGN(HBasicBlockBuilder);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_BLOCK_BUILDER_H_
