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

#ifndef ART_COMPILER_OPTIMIZING_INDUCTION_VAR_ANALYSIS_H_
#define ART_COMPILER_OPTIMIZING_INDUCTION_VAR_ANALYSIS_H_

#include <map>
#include <stack>
#include <string>

#include "nodes.h"
#include "optimization.h"

namespace art {

/**
 * Induction variable analysis.
 */
class HInductionVarAnalysis : public HOptimization {
 public:
  explicit HInductionVarAnalysis(HGraph* graph);
  ~HInductionVarAnalysis();

  // TODO: design public API useful in later phases

  /**
   * Returns string representation of induction found for the instruction
   * in the given loop (for testing and debugging only).
   */
  std::string InductionToString(HLoopInformation* loop,
                                HInstruction* instruction) {
    return InductionToString(LookupInfo(loop, instruction));
  }

  void Run() OVERRIDE;

 private:
  static constexpr const char* kInductionPassName = "induction_var_analysis";

  enum VisitState {
    kUnvisited,
    kOnStack,
    kDone
  };

  struct NodeInfo {
    NodeInfo() : state_(kUnvisited), df_(0) {}
    VisitState state_;
    int df_;
  };

  enum InductionClass {
    kNone,
    kInvariant,
    kLinear
    // TODO: add more induction classes as needed
  };

  enum InductionOp {
    kNop,  // no-operation: induction class
    kAdd,
    kSub,
    kNeg,
    kMul,
    kDiv
  };

  /**
   * Defines a detected induction as:
   *   (1) invariant:
   *         nop: fetch i
   *         oper: a + b, a - b, -b, a * b, a / b
   *   (2) linear:
   *         nop: a * i + b
   */
  struct InductionInfo : public ArenaObject<kArenaAllocMisc> {
    InductionInfo(InductionClass c,
                  InductionOp o,
                  InductionInfo* a,
                  InductionInfo* b,
                  HInstruction* i) : class_(c), op_(o), a_(a), b_(b), i_(i) {}
    InductionClass class_;
    InductionOp op_;
    InductionInfo* a_;
    InductionInfo* b_;
    HInstruction* i_;
  };

  inline InductionInfo* NewInductionInfo(
      InductionClass c, InductionOp o,
      InductionInfo* a, InductionInfo* b, HInstruction* i) {
    return new (graph_->GetArena()) InductionInfo(c, o, a, b, i);
  }

  // Methods for analysis.
  void VisitLoop(HLoopInformation* loop);
  void VisitNode(HLoopInformation* loop, HInstruction* instruction);
  int VisitDescendant(HLoopInformation* loop, HInstruction* instruction);
  void ClassifySCC(HLoopInformation* loop);
  void ClassifyOne(HLoopInformation* loop, HInstruction* instruction);
  bool ClassifyLinear(HLoopInformation* loop, HInstruction* external,
                      HInstruction* previous, HInstruction* internal);

  // Transfer operations.
  InductionInfo* TransferAddSub(InductionInfo* a, InductionInfo* b, InductionOp op);
  InductionInfo* TransferMul(InductionInfo* a, InductionInfo* b);
  InductionInfo* TransferNeg(InductionInfo* a);
  InductionInfo* Chain(InductionInfo* a, InductionInfo* b, InductionOp op);

  // Assign and lookup.
  void AssignInfo(HLoopInformation* loop,
                  HInstruction* instruction, InductionInfo* info);
  InductionClass LookupClass(HLoopInformation* loop, HInstruction* instruction);
  InductionInfo* LookupInfo(HLoopInformation* loop, HInstruction* instruction);
  std::string InductionToString(InductionInfo* info);

  // Bookkeeping during and after analysis.
  int df_count_;
  std::stack<HInstruction*> stack_;
  std::vector<HInstruction*> scc_;
  std::map<int, NodeInfo> map_;
  std::map<int, std::map<int, InductionInfo*>> induction_;

  DISALLOW_COPY_AND_ASSIGN(HInductionVarAnalysis);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_INDUCTION_VAR_ANALYSIS_H_
