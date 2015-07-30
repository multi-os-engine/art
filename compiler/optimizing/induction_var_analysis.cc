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

#include "induction_var_analysis.h"

namespace art {

/**
 * Returns true if instruction is invariant within the given loop.
 */
static bool IsLoopInvariant(HLoopInformation* loop, HInstruction* instruction) {
  if (!instruction->GetEnvironment()) {
    HLoopInformation* otherLoop = instruction->GetBlock()->GetLoopInformation();
    if (otherLoop != loop) {
      return otherLoop == nullptr || loop->IsIn(*otherLoop);
    }
  }
  return false;
}

//
// Class methods.
//

HInductionVarAnalysis::HInductionVarAnalysis(HGraph* graph)
    : HOptimization(graph, kInductionPassName),
      df_count_(0),
      stack_(),
      scc_(),
      map_() {
}

HInductionVarAnalysis::~HInductionVarAnalysis() {
}

void HInductionVarAnalysis::Run() {
  // Detects sequence variables (generalized induction variables) during an
  // inner-loop-first traversal of all loops using Gerlek's algorithm.
  for (HPostOrderIterator it_graph(*graph_);
       !it_graph.Done(); it_graph.Advance()) {
    HBasicBlock* graph_block = it_graph.Current();
    if (graph_block->IsLoopHeader()) {
      VisitLoop(graph_block->GetLoopInformation());
    }
  }  // for graph blocks
}

void HInductionVarAnalysis::VisitLoop(HLoopInformation* loop) {
  // Find SCCs in the SSA graph of this loop using Tarjan's algorithm.
  // Due to the descendant-first nature, classification happens "on-demand".
  df_count_ = 0;
  CHECK(stack_.empty());
  map_.clear();

  for (HBlocksInLoopIterator it_loop(*loop);
       !it_loop.Done(); it_loop.Advance()) {
    HBasicBlock* loop_block = it_loop.Current();
    CHECK(loop_block->IsInLoop());
    if (loop_block->GetLoopInformation() != loop) {
      continue;  // inner loops already visited
    }
    // Visit phis and instructions.
    for (HInstructionIterator it(loop_block->GetPhis());
         !it.Done(); it.Advance()) {
      HInstruction* instruction = it.Current();
      if (map_[instruction->GetId()].state_ == kUnvisited) {
        VisitNode(loop, instruction);
      }
    }
    for (HInstructionIterator it(loop_block->GetInstructions());
         !it.Done(); it.Advance()) {
      HInstruction* instruction = it.Current();
      if (map_[instruction->GetId()].state_ == kUnvisited) {
        VisitNode(loop, instruction);
      }
    }
  }  // for loop blocks

  CHECK(stack_.empty());
  map_.clear();
}

void HInductionVarAnalysis::VisitNode(HLoopInformation* loop,
                                      HInstruction* instruction) {
  const int id = instruction->GetId();
  const int df = ++df_count_;
  map_[id].state_ = kOnStack;
  map_[id].df_ = df;
  stack_.push(instruction);

  // Visit all descendants.
  int low = df;
  for (size_t i = 0, cnt = instruction->InputCount(); i < cnt; ++i) {
    low = std::min(low, VisitDescendant(loop, instruction->InputAt(i)));
  }

  // Lower or found SCC?
  if (low < df) {
    map_[id].df_ = low;
  } else {
    scc_.clear();
    while (!stack_.empty()) {
      HInstruction* x = stack_.top();
      scc_.push_back(x);
      stack_.pop();
      map_[x->GetId()].state_ = kDone;
      if (x == instruction) {
        break;
      }
    }  // while stack

    // Found potential sequence.
    ClassifySCC(loop);
    scc_.clear();
  }
}

int HInductionVarAnalysis::VisitDescendant(HLoopInformation* loop,
                                           HInstruction* instruction) {
  // If the definition is either outside the loop (loop invariant entry value)
  // or assigned in inner loop (inner exit value), the traversal stops.
  HLoopInformation* otherLoop = instruction->GetBlock()->GetLoopInformation();
  if (otherLoop != loop) {
    return df_count_;
  }

  // Inspect descendant node.
  const int id = instruction->GetId();
  switch (map_[id].state_) {
    case kUnvisited:
      VisitNode(loop, instruction);
      return map_[id].df_;
    case kOnStack:
      return map_[id].df_;
    default:
      return df_count_;
  }  // switch
}

void HInductionVarAnalysis::ClassifySCC(HLoopInformation* loop) {
  const size_t size = scc_.size();
  if (size == 1) {
    // Trivial SCC.
    ClassifyOne(loop, scc_[size - 1]);
  } else if (size > 1) {
    // Non-trivial SCC: chain that starts with 2-way phi only.
    HInstruction* phi = scc_[size - 1];
    if (phi->IsPhi() && phi->InputCount() == 2) {
      HInstruction* x = phi->InputAt(0);
      HInstruction* y = phi->InputAt(1);
      // Pick first matching classification.
      // TODO: add more classifications
      if (ClassifyLinear(loop, phi, x, y) ||
          ClassifyLinear(loop, phi, y, x)) {
        // Found!
      }
    }
  }
}

void HInductionVarAnalysis::ClassifyOne(HLoopInformation* loop,
                                        HInstruction* instruction) {
  InductionInfo* info = nullptr;

  // Perform lattice operations (first class then expression) on operators
  // based on a "on-demand" classification of the operations.
  // TODO: inspect more operators
  if (instruction->IsPhi()) {
  } else if (instruction->IsBinaryOperation()) {
    if (LookupClass(loop, instruction->InputAt(0)) != kNone &&
        LookupClass(loop, instruction->InputAt(1)) != kNone) {
      if (instruction->IsAdd() || instruction->IsSub()) {
        info = TransferAddSub(
            LookupInfo(loop, instruction->InputAt(0)),
            LookupInfo(loop, instruction->InputAt(1)),
                              instruction->IsAdd() ? kAdd : kSub);
      } else if (instruction->IsMul()) {
        info = TransferMul(
            LookupInfo(loop, instruction->InputAt(0)),
            LookupInfo(loop, instruction->InputAt(1)));
      }
    }
  } else if (instruction->IsNeg()) {
    if (LookupClass(loop, instruction->InputAt(0)) != kNone) {
      info = TransferNeg(LookupInfo(loop, instruction->InputAt(0)));
    }
  }

  // Successfully classified?
  if (info != nullptr) {
    AssignInfo(loop, instruction, info);
  }
}

bool HInductionVarAnalysis::ClassifyLinear(HLoopInformation* loop,
                                           HInstruction* previous,
                                           HInstruction* external,
                                           HInstruction* internal) {
  // Proper phi operand.
  if (!IsLoopInvariant(loop, external)) {
    return false;
  }

  // Proper chain of add/sub.
  const size_t size = scc_.size();
  InductionInfo* stride = nullptr;
  for (size_t i = 0; i < size - 1; i++) {
    HInstruction* oper = scc_[i];
    if (!oper->IsAdd() && !oper->IsSub()) {
      return false;
    }
    const InductionOp op = oper->IsAdd() ? kAdd : kSub;
    HInstruction* a = oper->InputAt(0);
    HInstruction* b = oper->InputAt(1);
    HInstruction* s;
    if (a == previous && IsLoopInvariant(loop, b)) {
      previous = oper;
      s = b;
    } else if (IsLoopInvariant(loop, a) && b == previous && op == kAdd) {
      previous = oper;
      s = a;
    } else {
      return false;
    }
    stride = Chain(
        stride, NewInductionInfo(kInvariant, kNop, nullptr, nullptr, s), op);
  }  // for chain

  // On success, classify phi first and then feed the chain "on-demand".
  if (previous == internal) {
    CHECK(stride != nullptr);
    InductionInfo* info = NewInductionInfo(
        kLinear, kNop, stride,
        NewInductionInfo(kInvariant, kNop, nullptr, nullptr, external),
        nullptr);
    AssignInfo(loop, scc_[size - 1], info);
    for (size_t i = 0; i < size - 1; i++) {
      ClassifyOne(loop, scc_[i]);
    }
    return true;
  }
  return false;
}

HInductionVarAnalysis::InductionInfo*
HInductionVarAnalysis::TransferAddSub(InductionInfo* a,
                                      InductionInfo* b, InductionOp op) {
  if (a->class_ == kInvariant && b->class_ == kInvariant) {
    return NewInductionInfo(kInvariant, op, a, b, nullptr);
  } else if (a->class_ == kLinear && b->class_ == kInvariant) {
    return NewInductionInfo(
        kLinear, kNop, a->a_,
        NewInductionInfo(kInvariant, op, a->b_, b, nullptr),
        nullptr);
  } else if (a->class_ == kInvariant && b->class_ == kLinear) {
    InductionInfo* ba = b->a_;
    if (op == kSub) {  // negation required
      ba = NewInductionInfo(kInvariant, kNeg, nullptr, ba, nullptr);
    }
    return NewInductionInfo(
        kLinear, kNop, ba,
        NewInductionInfo(kInvariant, op, a, b->b_, nullptr),
        nullptr);
  } else if (a->class_ == kLinear && b->class_ == kLinear) {
    return NewInductionInfo(
        kLinear, kNop,
        NewInductionInfo(kInvariant, op, a->a_, b->a_, nullptr),
        NewInductionInfo(kInvariant, op, a->b_, b->b_, nullptr),
        nullptr);
  }
  return nullptr;
}

HInductionVarAnalysis::InductionInfo*
HInductionVarAnalysis::TransferMul(InductionInfo* a, InductionInfo* b) {
  if (a->class_ == kInvariant && b->class_ == kInvariant) {
    return NewInductionInfo(kInvariant, kMul, a, b, nullptr);
  } else if (a->class_ == kLinear && b->class_ == kInvariant) {
    return NewInductionInfo(
        kLinear, kNop,
        NewInductionInfo(kInvariant, kMul, a->a_, b, nullptr),
        NewInductionInfo(kInvariant, kMul, a->b_, b, nullptr),
        nullptr);
  } else if (a->class_ == kInvariant && b->class_ == kLinear) {
    return NewInductionInfo(
        kLinear, kNop,
        NewInductionInfo(kInvariant, kMul, a, b->a_, nullptr),
        NewInductionInfo(kInvariant, kMul, a, b->b_, nullptr),
        nullptr);
  }
  return nullptr;
}

HInductionVarAnalysis::InductionInfo*
HInductionVarAnalysis::TransferNeg(InductionInfo* a) {
  if (a->class_ == kInvariant) {
    return NewInductionInfo(kInvariant, kNeg, nullptr, a, nullptr);
  } else if (a->class_ == kLinear) {
    return NewInductionInfo(
        kLinear, kNop,
        NewInductionInfo(kInvariant, kNeg, nullptr, a->a_, nullptr),
        NewInductionInfo(kInvariant, kNeg, nullptr, a->b_, nullptr),
        nullptr);
  }
  return nullptr;
}

HInductionVarAnalysis::InductionInfo*
HInductionVarAnalysis::Chain(InductionInfo* a,
                             InductionInfo* b, InductionOp op) {
  if (a == nullptr) {
    if (op == kSub) {  // negation required
      b = NewInductionInfo(kInvariant, kNeg, nullptr, b, nullptr);
    }
    return b;
  }
  return NewInductionInfo(kInvariant, op, a, b, nullptr);
}

void HInductionVarAnalysis::AssignInfo(HLoopInformation* loop,
                                       HInstruction* instruction, InductionInfo* info) {
  const int loopId = loop->GetHeader()->GetBlockId();
  induction_[loopId][instruction->GetId()] = info;
}

HInductionVarAnalysis::InductionClass
HInductionVarAnalysis::LookupClass(HLoopInformation* loop, HInstruction* instruction) {
  if (IsLoopInvariant(loop, instruction)) {
    return kInvariant;
  }
  const int loopId = loop->GetHeader()->GetBlockId();
  auto it = induction_[loopId].find(instruction->GetId());
  if (it != induction_[loopId].end()) {
    return it->second->class_;
  }
  return kNone;
}

HInductionVarAnalysis::InductionInfo*
HInductionVarAnalysis::LookupInfo(HLoopInformation* loop, HInstruction* instruction) {
  if (IsLoopInvariant(loop, instruction)) {
    return NewInductionInfo(kInvariant, kNop, nullptr, nullptr, instruction);
  }
  const int loopId = loop->GetHeader()->GetBlockId();
  auto it = induction_[loopId].find(instruction->GetId());
  if (it != induction_[loopId].end()) {
    return it->second;
  }
  return nullptr;
}

// For debugging and testing only.
std::string HInductionVarAnalysis::InductionToString(InductionInfo* info) {
  if (info != nullptr) {
    if (info->class_ == kInvariant) {
      std::string inv = "(";
      inv += InductionToString(info->a_);
      switch (info->op_) {
        case kNop: break;
        case kAdd: inv += " + "; break;
        case kSub:
        case kNeg: inv += " - "; break;
        case kMul: inv += " * "; break;
        case kDiv: inv += " / "; break;
      }
      inv += InductionToString(info->b_);
      if (info->i_) {
        inv += std::to_string(info->i_->GetId()) + ":" + info->i_->DebugName();
      }
      return inv + ")";
    } else if (info->class_ == kLinear) {
      CHECK_EQ(info->op_, kNop);
      return "(" + InductionToString(info->a_) + " * i + " +
                   InductionToString(info->b_) + ")";
    }
  }
  return "";
}

}  // namespace art
