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

#include "builder.h"
#include "dex_file.h"
#include "dex_instruction.h"
#include "dead_code_elimination.h"
#include "graph_checker.h"
#include "optimizing_unit_test.h"

#include "gtest/gtest.h"

namespace art {

// Create a control-flow graph from Dex bytes.
HGraph* CreateCFG(ArenaAllocator* allocator, const uint16_t* data) {
  HGraphBuilder builder(allocator);
  const DexFile::CodeItem* item =
    reinterpret_cast<const DexFile::CodeItem*>(data);
  HGraph* graph = builder.BuildGraph(*item);
  return graph;
}


static void TestCode(const uint16_t* data) {
  ArenaPool pool;
  ArenaAllocator allocator(&pool);
  HGraph* graph = CreateCFG(&allocator, data);
  ASSERT_NE(graph, nullptr);

  graph->BuildDominatorTree();
  graph->TransformToSSA();

  DeadCodeElimination(graph).Run();

  SSAChecker ssa_checker(&allocator, graph);
  ssa_checker.VisitInsertionOrder();
  ASSERT_TRUE(ssa_checker.IsValid());
}


/* Small three-register program.

                                16-bit
                                offset
                                ------
       v1 <- 1                  0.      const/4 v1, #+1
       v0 <- 0                  1.      const/4 v0, #+0
       if v1 >= 0 goto L1       2.      if-gez v1, +3
       v0 <- v1                 4.      move v0, v1
   L1: v2 <- v0 + v1            5.      add-int v2, v0, v1
       return-void              7.      return
*/
TEST(DeadCodeElimination, Program1) {
  const uint16_t data[] = THREE_REGISTERS_CODE_ITEM(
    Instruction::CONST_4 | 1 << 8 | 1 << 12,
    Instruction::CONST_4 | 0 << 8 | 0 << 12,
    Instruction::IF_GEZ | 1 << 8, 3,
    Instruction::MOVE | 0 << 8 | 1 << 12,
    Instruction::ADD_INT | 2 << 8, 0 | 1 << 8,
    Instruction::RETURN_VOID);

  TestCode(data);
}

}  // namespace art
