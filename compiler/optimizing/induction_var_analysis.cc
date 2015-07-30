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
  HLoopInformation* otherLoop = instruction->GetBlock()->GetLoopInformation();
  if (otherLoop != loop) {
    return otherLoop == nullptr || loop->IsIn(*otherLoop);
  }
  return false;
}

//
// Class methods.
//

HInductionVarAnalysis::HInductionVarAnalysis(HGraph* graph)
    : HOptimization(graph, kInductionPassName),
      globalDepth_(0),
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

  // TODO: cleanup of unused information?
}

void HInductionVarAnalysis::VisitLoop(HLoopInformation* loop) {
  // Find SCCs in the SSA graph of this loop using Tarjan's algorithm.
  // Due to the descendant-first nature, classification happens "on-demand".
  globalDepth_ = 0;
  CHECK(stack_.empty());
  map_.clear();

  for (HBlocksInLoopIterator it_loop(*loop);
       !it_loop.Done(); it_loop.Advance()) {
    HBasicBlock* loop_block = it_loop.Current();
    CHECK(loop_block->IsInLoop());
    if (loop_block->GetLoopInformation() != loop) {
      continue;  // inner loops already visited
    }
    // Visit phi-operations and instructions.
    for (HInstructionIterator it(loop_block->GetPhis());
         !it.Done(); it.Advance()) {
      HInstruction* instruction = it.Current();
      if (map_[instruction->GetId()].state == kUnvisited) {
        VisitNode(loop, instruction);
      }
    }
    for (HInstructionIterator it(loop_block->GetInstructions());
         !it.Done(); it.Advance()) {
      HInstruction* instruction = it.Current();
      if (map_[instruction->GetId()].state == kUnvisited) {
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
  const int d1 = ++globalDepth_;
  map_[id] = NodeInfo(kOnStack, d1);
  stack_.push(instruction);

  // Visit all descendants.
  int low = d1;
  for (size_t i = 0, cnt = instruction->InputCount(); i < cnt; ++i) {
    low = std::min(low, VisitDescendant(loop, instruction->InputAt(i)));
  }

  // Lower or found SCC?
  if (low < d1) {
    map_[id].depth = low;
  } else {
    scc_.clear();
    while (!stack_.empty()) {
      HInstruction* x = stack_.top();
      scc_.push_back(x);
      stack_.pop();
      map_[x->GetId()].state = kDone;
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
    return globalDepth_;
  }

  // Inspect descendant node.
  const int id = instruction->GetId();
  switch (map_[id].state) {
    case kUnvisited:
      VisitNode(loop, instruction);
      return map_[id].depth;
    case kOnStack:
      return map_[id].depth;
    default:  // kDone:
      return globalDepth_;
  }  // switch
}

void HInductionVarAnalysis::ClassifySCC(HLoopInformation* loop) {
  const size_t size = scc_.size();
  if (size == 1) {
    // Trivial SCC.
    ClassifyOne(loop, scc_[0]);
  } else if (size > 1) {
    // Non-trivial SCC.
    HInstruction* phi = scc_[size - 1];
    if (phi->IsPhi() && phi->InputCount() == 2) {
      HInstruction* x = phi->InputAt(0);
      HInstruction* y = phi->InputAt(1);
      // Pick first matching classification for phi(external, internal).
      // TODO: add more classifications
      if (ClassifyLinear(loop, phi, x, y)) {
        // Found!
      }
    }
  }
}

void HInductionVarAnalysis::ClassifyOne(HLoopInformation* loop,
                                        HInstruction* instruction) {
  const size_t count = instruction->InputCount();
  InductionInfo* info = nullptr;
  if (instruction->IsPhi()) {
    CHECK(count > 0);
    InductionInfo *a = LookupInfo(loop, instruction->InputAt(0));
    if (a != nullptr) {
      // Header phi (wrap-around) or same-classification merge?
      if (count == 2 && instruction->GetBlock() == loop->GetHeader()) {
        InductionInfo *b = LookupInfo(loop, instruction->InputAt(1));
        if (a->inducClass == kInvariant && b != nullptr) {
          info = NewInductionInfo(kWrapAround, kNop, a, b, nullptr);
        }
      } else {
        for (size_t i = 1; i < count; i++) {
          if (!InductionEqual(a, LookupInfo(loop, instruction->InputAt(i))))
            return;
        }
        info = a;
      }
    }
  } else if (instruction->IsAdd() || instruction->IsSub()) {
    info = TransferAddSub(
        LookupInfo(loop, instruction->InputAt(0)),
        LookupInfo(loop, instruction->InputAt(1)),
                         instruction->IsAdd() ? kAdd : kSub);
  } else if (instruction->IsMul()) {
    info = TransferMul(
        LookupInfo(loop, instruction->InputAt(0)),
        LookupInfo(loop, instruction->InputAt(1)));
  } else if (instruction->IsNeg()) {
    info = TransferNeg(LookupInfo(loop, instruction->InputAt(0)));
  }
  // TODO: inspect more operators if needed

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
    stride = Chain(stride, LookupInfo(loop, s), op);
  }  // for chain

  // On success, classify phi first and then feed the chain "on-demand".
  if (previous == internal) {
    CHECK(stride != nullptr);
    InductionInfo* info = NewInductionInfo(
        kLinear, kNop, stride, LookupInfo(loop, external), nullptr);
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
  if (a != nullptr && b != nullptr) {
    if (a->inducClass == kInvariant && b->inducClass == kInvariant) {
      return NewInductionInfo(kInvariant, op, a, b, nullptr);
    } else if (a->inducClass == kLinear && b->inducClass == kInvariant) {
      return NewInductionInfo(
          kLinear, kNop, a->opA,
          NewInductionInfo(kInvariant, op, a->opB, b, nullptr),
          nullptr);
    } else if (a->inducClass == kInvariant && b->inducClass == kLinear) {
      InductionInfo* ba = b->opA;
      if (op == kSub) {  // negation required
        ba = NewInductionInfo(kInvariant, kNeg, nullptr, ba, nullptr);
      }
      return NewInductionInfo(
          kLinear, kNop, ba,
          NewInductionInfo(kInvariant, op, a, b->opB, nullptr),
          nullptr);
    } else if (a->inducClass == kLinear && b->inducClass == kLinear) {
      return NewInductionInfo(
          kLinear, kNop,
          NewInductionInfo(kInvariant, op, a->opA, b->opA, nullptr),
          NewInductionInfo(kInvariant, op, a->opB, b->opB, nullptr),
          nullptr);
    }
  }
  return nullptr;
}

HInductionVarAnalysis::InductionInfo*
HInductionVarAnalysis::TransferMul(InductionInfo* a, InductionInfo* b) {
  if (a != nullptr && b != nullptr) {
    if (a->inducClass == kInvariant && b->inducClass == kInvariant) {
      return NewInductionInfo(kInvariant, kMul, a, b, nullptr);
    } else if (a->inducClass == kLinear && b->inducClass == kInvariant) {
      return NewInductionInfo(
          kLinear, kNop,
          NewInductionInfo(kInvariant, kMul, a->opA, b, nullptr),
          NewInductionInfo(kInvariant, kMul, a->opB, b, nullptr),
          nullptr);
    } else if (a->inducClass == kInvariant && b->inducClass == kLinear) {
      return NewInductionInfo(
          kLinear, kNop,
          NewInductionInfo(kInvariant, kMul, a, b->opA, nullptr),
          NewInductionInfo(kInvariant, kMul, a, b->opB, nullptr),
          nullptr);
    }
  }
  return nullptr;
}

HInductionVarAnalysis::InductionInfo*
HInductionVarAnalysis::TransferNeg(InductionInfo* a) {
  if (a != nullptr) {
    if (a->inducClass == kInvariant) {
      return NewInductionInfo(kInvariant, kNeg, nullptr, a, nullptr);
    } else if (a->inducClass == kLinear) {
      return NewInductionInfo(
          kLinear, kNop,
          NewInductionInfo(kInvariant, kNeg, nullptr, a->opA, nullptr),
          NewInductionInfo(kInvariant, kNeg, nullptr, a->opB, nullptr),
          nullptr);
    }
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
                                       HInstruction* instruction,
                                       InductionInfo* info) {
  const int loopId = loop->GetHeader()->GetBlockId();
  const int id = instruction->GetId();
  induction_[loopId][id] = info;
}

HInductionVarAnalysis::InductionInfo*
HInductionVarAnalysis::LookupInfo(HLoopInformation* loop,
                                  HInstruction* instruction) {
  const int loopId = loop->GetHeader()->GetBlockId();
  const int id = instruction->GetId();
  auto it = induction_[loopId].find(id);
  if (it != induction_[loopId].end()) {
    return it->second;
  }
  if (IsLoopInvariant(loop, instruction)) {
    InductionInfo* info = NewInductionInfo(
        kInvariant, kFetch, nullptr, nullptr, instruction);
    induction_[loopId][id] = info;
    return info;
  }
  return nullptr;
}

// Test structural equality only, without accounting for simplifications.
bool HInductionVarAnalysis::InductionEqual(InductionInfo* info1,
                                           InductionInfo* info2) {
  if (info1 != nullptr && info2 != nullptr) {
    return
        info1->inducClass == info2->inducClass &&
        info1->oper       == info2->oper       &&
        info1->fetch      == info2->fetch      &&
        InductionEqual(info1->opA, info2->opA) &&
        InductionEqual(info1->opB, info2->opB);
  }
  return info1 == info2;
}

// For debugging and testing only.
std::string HInductionVarAnalysis::InductionToString(InductionInfo* info) {
  if (info != nullptr) {
    if (info->inducClass == kInvariant) {
      std::string inv = "(";
      inv += InductionToString(info->opA);
      switch (info->oper) {
        case kNop: inv += " ? "; break;
        case kAdd: inv += " + "; break;
        case kSub:
        case kNeg: inv += " - "; break;
        case kMul: inv += " * "; break;
        case kDiv: inv += " / "; break;
        case kFetch:
          CHECK(info->fetch != nullptr);
          inv += std::to_string(info->fetch->GetId())
              + ":" + info->fetch->DebugName();
          break;
      }
      inv += InductionToString(info->opB);
      return inv + ")";
    } else {
      CHECK(info->oper == kNop);
      if (info->inducClass == kLinear) {
        return "(" + InductionToString(info->opA) + " * i + " +
                     InductionToString(info->opB) + ")";
      } else if (info->inducClass == kWrapAround) {
        return "wrap(" + InductionToString(info->opA) + ", " +
                         InductionToString(info->opB) + ")";
      } else if (info->inducClass == kPeriodic) {
        return "periodic(" + InductionToString(info->opA) + ", " +
                             InductionToString(info->opB) + ")";
      }
    }
  }
  return "";
}

}  // namespace art
