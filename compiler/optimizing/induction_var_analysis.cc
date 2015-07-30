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

/**
 * Returns true if instruction provides proper mu-operation for the given loop.
 */
static bool IsMu(HLoopInformation* loop, HInstruction* instruction) {
  return instruction->IsPhi() && instruction->InputCount() == 2 &&
      instruction->GetBlock() == loop->GetHeader();
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
  for (size_t i = 0, count = instruction->InputCount(); i < count; ++i) {
    low = std::min(low, VisitDescendant(loop, instruction->InputAt(i)));
  }

  // Lower or found SCC?
  if (low < d1) {
    map_[id].depth = low;
  } else {
    scc_.clear();
    cycle_.clear();
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
    if (scc_.size() == 1 && !IsMu(loop, scc_[0])) {
      ClassifyTrivial(loop, scc_[0]);
    } else {
      ClassifyNonTrivial(loop);
    }
    scc_.clear();
    cycle_.clear();
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

void HInductionVarAnalysis::ClassifyTrivial(HLoopInformation* loop,
                                            HInstruction* instruction) {
  InductionInfo* info = nullptr;
  if (instruction->IsPhi()) {
    for (size_t i = 1, count = instruction->InputCount(); i < count; i++) {
      info = TransferPhi(LookupInfo(loop, instruction->InputAt(0)),
                         LookupInfo(loop, instruction->InputAt(i)));
    }
  } else if (instruction->IsAdd()) {
    info = TransferAddSub(LookupInfo(loop, instruction->InputAt(0)),
                          LookupInfo(loop, instruction->InputAt(1)), kAdd);
  } else if (instruction->IsSub()) {
    info = TransferAddSub(LookupInfo(loop, instruction->InputAt(0)),
                          LookupInfo(loop, instruction->InputAt(1)), kSub);
  } else if (instruction->IsMul()) {
    info = TransferMul(LookupInfo(loop, instruction->InputAt(0)),
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

void HInductionVarAnalysis::ClassifyNonTrivial(HLoopInformation* loop) {
  const size_t size = scc_.size();
  CHECK(size >= 1);
  HInstruction* phi = scc_[size - 1];
  if (!IsMu(loop, phi)) {
    return;
  }
  HInstruction* external = phi->InputAt(0);
  HInstruction* internal = phi->InputAt(1);
  InductionInfo* initial = LookupInfo(loop, external);
  if (initial == nullptr || initial->inducClass != kInvariant) {
    return;
  }

  // Singleton mu-operation may be a wrap-around induction.
  if (size == 1) {
    InductionInfo *update = LookupInfo(loop, internal);
    if (update != nullptr) {
      AssignInfo(loop, phi,
                 NewInductionInfo(kWrapAround, kNop, initial, update, nullptr));
    }
    return;
  }

  // Inspect remainder of the cycle.
  cycle_.clear();
  cycle_[phi->GetId()] = static_cast<InductionInfo*>(nullptr);
  for (size_t i = 0; i < size - 1; i++) {
    HInstruction* oper = scc_[i];
    InductionInfo* update = nullptr;
    if (oper->IsPhi()) {
      update = TransferCycleOverPhi(oper);
    } else if (oper->IsAdd()) {
      update = TransferCycleOverAddSub(loop,
                                       oper->InputAt(0),
                                       oper->InputAt(1), kAdd);
      if (update == nullptr) {  // try other way around
        update = TransferCycleOverAddSub(loop,
                                         oper->InputAt(1),
                                         oper->InputAt(0), kAdd);
      }
    } else if (oper->IsSub()) {
      update = TransferCycleOverAddSub(loop,
                                       oper->InputAt(0),
                                       oper->InputAt(1), kSub);
    }
    if (update == nullptr) {
      return;
    }
    cycle_[oper->GetId()] = update;
  }  // for cycle

  // Success if the internal link received accumulated nonzero update.
  auto it = cycle_.find(internal->GetId());
  if (it != cycle_.end() && it->second != nullptr) {
    // Classify header phi and feed the cycle "on-demand".
    AssignInfo(loop, phi,
        NewInductionInfo(kLinear, kNop, it->second, initial, nullptr));
    for (size_t i = 0; i < size - 1; i++) {
      ClassifyTrivial(loop, scc_[i]);
    }
  }
}

HInductionVarAnalysis::InductionInfo*
HInductionVarAnalysis::TransferPhi(InductionInfo* a, InductionInfo* b) {
  if (InductionEqual(a, b)) {
    return a;
  }
  return nullptr;
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
HInductionVarAnalysis::TransferCycleOverPhi(HInstruction* phi) {
  const size_t count = phi->InputCount();
  CHECK(count > 0);
  auto ita = cycle_.find(phi->InputAt(0)->GetId());
  if (ita != cycle_.end()) {
    InductionInfo* a = ita->second;
    for (size_t i = 1; i < count; i++) {
      auto itb = cycle_.find(phi->InputAt(i)->GetId());
      if (itb == cycle_.end() ||
          !HInductionVarAnalysis::InductionEqual(a, itb->second)) {
        return nullptr;
      }
    }
    return a;
  }
  return nullptr;
}

HInductionVarAnalysis::InductionInfo*
HInductionVarAnalysis::TransferCycleOverAddSub(HLoopInformation* loop,
                                               HInstruction* link,
                                               HInstruction* stride,
                                               InductionOp op) {
  auto it = cycle_.find(link->GetId());
  if (it != cycle_.end()) {
    InductionInfo* a = it->second;
    InductionInfo* b = LookupInfo(loop, stride);
    if (b != nullptr && b->inducClass == kInvariant) {
      if (a == nullptr) {
        if (op == kSub) {  // negation required
          return NewInductionInfo(kInvariant, kNeg, nullptr, b, nullptr);
        }
        return b;
      } else {
        return NewInductionInfo(kInvariant, op, a, b, nullptr);
      }
    }
  }
  return nullptr;
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

bool HInductionVarAnalysis::InductionEqual(InductionInfo* info1,
                                           InductionInfo* info2) {
  // Test structural equality only, without accounting for simplifications.
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
