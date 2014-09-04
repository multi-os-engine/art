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
#include "pretty_printer.h"
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

// Naive string diff data type.
typedef std::list<std::pair<std::string, std::string>> diff_t;

// Naive patch command: apply a diff to a string.
static std::string patch(const std::string& original, const diff_t& diff) {
  std::string result = original;
  for (const auto& p : diff) {
    std::string::size_type pos = result.find(p.first);
    EXPECT_NE(pos, std::string::npos);
    result.replace(pos, p.first.size(), p.second);
  }
  return result;
}


static void TestCode(const uint16_t* data,
                     const std::string& expected_before,
                     const std::string& expected_after) {
  ArenaPool pool;
  ArenaAllocator allocator(&pool);
  HGraph* graph = CreateCFG(&allocator, data);
  ASSERT_NE(graph, nullptr);

  graph->BuildDominatorTree();
  graph->TransformToSSA();

  StringPrettyPrinter printer_before(graph);
  printer_before.VisitInsertionOrder();
  std::string actual_before = printer_before.str();
  ASSERT_EQ(actual_before, expected_before);

  DeadCodeElimination(graph).Run();

  StringPrettyPrinter printer_after(graph);
  printer_after.VisitInsertionOrder();
  std::string actual_after = printer_after.str();
  ASSERT_EQ(actual_after, expected_after);

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

  std::string expected_before =
    "BasicBlock 0, succ: 1\n"
    "  3: IntConstant [15, 21, 8]\n"
    "  5: IntConstant [21, 8]\n"
    "  19: Goto 1\n"
    "BasicBlock 1, pred: 0, succ: 5, 2\n"
    "  8: GreaterThanOrEqual(3, 5) [9]\n"
    "  9: If(8)\n"
    "BasicBlock 2, pred: 1, succ: 3\n"
    "  12: Goto 3\n"
    "BasicBlock 3, pred: 2, 5, succ: 4\n"
    "  21: Phi(3, 5) [15]\n"
    "  15: Add(21, 3)\n"
    "  17: ReturnVoid\n"
    "BasicBlock 4, pred: 3\n"
    "  18: Exit\n"
    "BasicBlock 5, pred: 1, succ: 3\n"
    "  20: Goto 3\n";

  diff_t expected_diff = {
    { "  3: IntConstant [15, 21, 8]\n", "  3: IntConstant [21, 8]\n" },
    { "  21: Phi(3, 5) [15]\n",         "  21: Phi(3, 5)\n" },
    { "  15: Add(21, 3)\n",             "" }
  };
  std::string expected_after = patch(expected_before, expected_diff);

  TestCode(data, expected_before, expected_after);
}

}  // namespace art
