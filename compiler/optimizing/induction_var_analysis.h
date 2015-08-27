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

#include <string>

#include "nodes.h"
#include "optimization.h"

namespace art {

/**
 * Induction variable analysis.
 *
 * Based on the paper by M. Gerlek et al.
 * "Beyond Induction Variables: Detecting and Classifying Sequences Using a Demand-Driven SSA Form"
 * (ACM Transactions on Programming Languages and Systems, Volume 17 Issue 1, Jan. 1995).
 */
class HInductionVarAnalysis : public HOptimization {
 public:
  explicit HInductionVarAnalysis(HGraph* graph);

  // TODO: design public API useful in later phases

  /**
   * Returns string representation of induction found for the instruction
   * in the given loop (for testing and debugging only).
   */
  std::string InductionToString(HLoopInformation* loop, HInstruction* instruction) {
    return InductionToString(LookupInfo(loop, instruction));
  }

  void Run() OVERRIDE;

 private:
  static constexpr const char* kInductionPassName = "induction_var_analysis";

  struct NodeInfo {
    explicit NodeInfo(uint32_t d) : depth(d), done(false) {}
    uint32_t depth;
    bool done;
  };

  enum InductionClass {
    kInvariant,
    kLinear,
    kWrapAround,
    kPeriodic
  };

  enum InductionOp {
    kNop,  // no-operation: a true induction
    kAdd,
    kSub,
    kNeg,
    kMul,
    kFetch
  };

  /**
   * Defines a detected induction as:
   *   (1) invariant:
   *         operation: a + b, a - b, -b, a * b, a / b
   *       or:
   *         fetch: fetch from HIR
   *   (2) linear:
   *         nop: a * i + b
   *   (3) wrap-around
   *         nop: a, then defined by b
   *   (4) periodic
   *         nop: a, then defined by b (repeated when exhausted)
   */
  struct InductionInfo : public ArenaObject<kArenaAllocMisc> {
    InductionInfo(InductionClass ic,
                  InductionOp op,
                  InductionInfo* a,
                  InductionInfo* b,
                  HInstruction* f)
        : induction_class(ic),
          operation(op),
          op_a(a),
          op_b(b),
          fetch(f) {}
    InductionClass induction_class;
    InductionOp operation;
    InductionInfo* op_a;
    InductionInfo* op_b;
    HInstruction* fetch;
  };

  inline bool IsVisitedNode(HInstruction* instruction) const {
    return map_.find(instruction) != map_.end();
  }

  inline InductionInfo* NewInvariantOp(InductionOp op, InductionInfo* a, InductionInfo* b) {
    return new (graph_->GetArena()) InductionInfo(kInvariant, op, a, b, nullptr);
  }

  inline InductionInfo* NewInvariantFetch(HInstruction* f) {
    return new (graph_->GetArena()) InductionInfo(kInvariant, kFetch, nullptr, nullptr, f);
  }

  inline InductionInfo* NewInduction(InductionClass ic, InductionInfo* a, InductionInfo* b) {
    return new (graph_->GetArena()) InductionInfo(ic, kNop, a, b, nullptr);
  }

  // Methods for analysis.
  void VisitLoop(HLoopInformation* loop);
  void VisitNode(HLoopInformation* loop, HInstruction* instruction);
  uint32_t VisitDescendant(HLoopInformation* loop, HInstruction* instruction);
  void ClassifyTrivial(HLoopInformation* loop, HInstruction* instruction);
  void ClassifyNonTrivial(HLoopInformation* loop);

  // Transfer operations.
  InductionInfo* TransferPhi(InductionInfo* a, InductionInfo* b);
  InductionInfo* TransferAddSub(InductionInfo* a, InductionInfo* b, InductionOp op);
  InductionInfo* TransferMul(InductionInfo* a, InductionInfo* b);
  InductionInfo* TransferShl(InductionInfo* a, InductionInfo* b);
  InductionInfo* TransferNeg(InductionInfo* a);

  // Solvers.
  InductionInfo* SolvePhi(HLoopInformation* loop,
                          HInstruction* phi,
                          HInstruction* instruction);
  InductionInfo* SolveAddSub(HLoopInformation* loop,
                             HInstruction* phi,
                             HInstruction* instruction,
                             HInstruction* x,
                             HInstruction* y,
                             InductionOp op,
                             bool first);
  InductionInfo* PeriodToEnd(InductionInfo* seq, InductionInfo* last);

  // Assign and lookup.
  void AssignInfo(HLoopInformation* loop, HInstruction* instruction, InductionInfo* info);
  InductionInfo* LookupInfo(HLoopInformation* loop, HInstruction* instruction);
  bool InductionEqual(InductionInfo* info1, InductionInfo* info2);
  std::string InductionToString(InductionInfo* info);

  // Bookkeeping during and after analysis.
  // TODO: fine tune data structures, only keep relevant data
  uint32_t global_depth_;
  ArenaVector<HInstruction*> stack_;
  ArenaVector<HInstruction*> scc_;
  ArenaSafeMap<HInstruction*, NodeInfo> map_;
  ArenaSafeMap<HInstruction*, InductionInfo*> cycle_;
  ArenaSafeMap<HLoopInformation*, ArenaSafeMap<HInstruction*, InductionInfo*>> induction_;

  DISALLOW_COPY_AND_ASSIGN(HInductionVarAnalysis);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_INDUCTION_VAR_ANALYSIS_H_
