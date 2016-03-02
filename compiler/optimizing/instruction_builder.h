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

#ifndef ART_COMPILER_OPTIMIZING_INSTRUCTION_BUILDER_H_
#define ART_COMPILER_OPTIMIZING_INSTRUCTION_BUILDER_H_

#include "base/arena_containers.h"
#include "base/arena_object.h"
#include "block_builder.h"
#include "nodes.h"

namespace art {

class HInstructionBuilder : public ArenaObject<kArenaAllocGraphBuilder> {
 public:
  HInstructionBuilder(HGraph* graph,
                      const DexFile::CodeItem& code_item,
                      HBasicBlockBuilder* block_builder)
      : arena_(graph->GetArena()),
        graph_(graph),
        code_item_(code_item),
        block_builder_(block_builder) {}

  bool Build();

  bool ProcessBytecodeInstruction()

 private:
  ArenaAllocator* const arena_;
  HGraph* const graph_;
  const DexFile::CodeItem& code_item_;
  HBasicBlockBuilder* block_builder_;

  DISALLOW_COPY_AND_ASSIGN(HInstructionBuilder);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_INSTRUCTION_BUILDER_H_
