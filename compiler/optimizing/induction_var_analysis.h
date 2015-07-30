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

#include <stack>
#include <string>
#include <unordered_map>

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
    NodeInfo() : state(kUnvisited), depth(0) {}
    NodeInfo(VisitState s, int d) : state(s), depth(d) {}
    VisitState state;
    int depth;
  };

  enum InductionClass {
    kNone,
    kInvariant,
    kLinear,
    kWrapAround,
    kPeriodic,
    kMonotonic
  };

  enum InductionOp {
    kNop,  // no-operation: a true induction
    kAdd,
    kSub,
    kNeg,
    kMul,
    kDiv,
    kFetch
  };

  /**
   * Defines a detected induction as:
   *   (1) invariant:
   *         oper: a + b, a - b, -b, a * b, a / b
   *         fetch: fetch instruction
   *   (2) linear:
   *         nop: a * i + b
   *   (3) wrap-around
   *         nop: a, then defined by b
   *   (4) periodic
   *         nop: a, then defined by b (repeated when exhausted)
   *   (5) monotonic
   *         // TODO:
   */
  struct InductionInfo : public ArenaObject<kArenaAllocMisc> {
    InductionInfo(InductionClass ic,
                  InductionOp op,
                  InductionInfo* a,
                  InductionInfo* b,
                  HInstruction* f)
        : inducClass(ic),
          oper(op),
          opA(a),
          opB(b),
          fetch(f) {}
    InductionClass inducClass;
    InductionOp oper;
    InductionInfo* opA;
    InductionInfo* opB;
    HInstruction* fetch;
  };

  inline InductionInfo* NewInductionInfo(
      InductionClass c, InductionOp op,
      InductionInfo* a, InductionInfo* b, HInstruction* i) {
    return new (graph_->GetArena()) InductionInfo(c, op, a, b, i);
  }

  // Methods for analysis.
  void VisitLoop(HLoopInformation* loop);
  void VisitNode(HLoopInformation* loop, HInstruction* instruction);
  int VisitDescendant(HLoopInformation* loop, HInstruction* instruction);
  void ClassifyTrivial(HLoopInformation* loop, HInstruction* instruction);
  void ClassifyNonTrivial(HLoopInformation* loop);

  // Transfer operations.
  InductionInfo* TransferPhi(InductionInfo* a, InductionInfo* b);
  InductionInfo* TransferAddSub(InductionInfo* a,
                                InductionInfo* b, InductionOp op);
  InductionInfo* TransferMul(InductionInfo* a, InductionInfo* b);
  InductionInfo* TransferNeg(InductionInfo* a);
  InductionInfo* TransferCycleOverPhi(HInstruction* phi);
  InductionInfo* TransferCycleOverAddSub(HLoopInformation* loop,
                                         HInstruction* link,
                                         HInstruction* stride,
                                         InductionOp op);

  // Assign and lookup.
  void AssignInfo(HLoopInformation* loop,
                  HInstruction* instruction, InductionInfo* info);
  InductionInfo* LookupInfo(HLoopInformation* loop, HInstruction* instruction);
  bool InductionEqual(InductionInfo* info1, InductionInfo* info2);
  std::string InductionToString(InductionInfo* info);

  // Bookkeeping during and after analysis.
  int globalDepth_;
  std::stack<HInstruction*> stack_;
  std::vector<HInstruction*> scc_;
  std::unordered_map<int, NodeInfo> map_;
  std::unordered_map<int, InductionInfo*> cycle_;
  std::unordered_map<int, std::unordered_map<int, InductionInfo*>> induction_;

  DISALLOW_COPY_AND_ASSIGN(HInductionVarAnalysis);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_INDUCTION_VAR_ANALYSIS_H_
