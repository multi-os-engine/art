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

#include "arch/arm64/instruction_set_features_arm64.h"
#include "prepare_for_register_allocation.h"
#include "scheduler_arm64.h"
#include "scheduler.h"

namespace art {

// Output node depends on input node.
void SchedulingGraph::AddDependency(const HInstruction* output, const HInstruction* input) {
  SchedulingNode* output_node = GetNode(output);
  SchedulingNode* input_node = GetNode(input);
  DCHECK(output_node != nullptr);
  // The input instruction can be defined in other blocks, out of scheduling range.
  if (input_node == nullptr) {
    return;
  }

  if (!HasDependency(input_node, output_node)) {
    SuccessorEdge* edge = new (arena_) SuccessorEdge(output_node);
    input_node->AddSuccessorEdge(edge);
    output_node->num_unscheduled_predecessors_++;
  }
}

// Check whether successor HInstruction depends on predecessor HInstruction, taking into
// account SideEffect information and CanThrow information.
static bool HasSideEffectDependency(const HInstruction* predecessor,
                                    const HInstruction* successor) {
  // TODO: Current side effect dependency for heap memory aliasing can be improved by
  // detecting memory location information.
  if (successor->GetSideEffects().MayHaveReorderingDependency(predecessor->GetSideEffects())) {
    return true;
  } else if ((predecessor->CanThrow() && successor->GetSideEffects().DoesAnyWrite()) ||
             (predecessor->GetSideEffects().DoesAnyWrite() && successor->CanThrow()) ||
             (predecessor->CanThrow() && successor->CanThrow())) {
    return true;
  }
  return false;
}

// Add all dependencies for given HInstruction.
void SchedulingGraph::AddDependencies(HInstruction* instruction) {
  // Define-use dependencies.
  for (HInputIterator it(instruction); !it.Done(); it.Advance()) {
    HInstruction* input = it.Current();
    AddDependency(instruction, input);
  }
  // Environment dependencies.
  if (instruction->HasEnvironment()) {
    for (HEnvironment* environment = instruction->GetEnvironment();
         environment != nullptr;
         environment = environment->GetParent()) {
      for (size_t i = 0; i < environment->Size(); ++i) {
        HInstruction* env_instr = environment->GetInstructionAt(i);
        if (env_instr != nullptr) {
          AddDependency(instruction, env_instr);
        }
      }
    }
  }
  // Side effect dependencies. Check side effect dependency from its previous
  // instruction up to the first instruction in the scheduling range.
  if (!instruction->GetSideEffects().DoesNothing() || instruction->CanThrow()) {
    HInstruction* prev_instr = instruction->GetPrevious();
    while (prev_instr) {
      if (HasSideEffectDependency(prev_instr, instruction)) {
        AddDependency(instruction, prev_instr);
      }
      if (prev_instr != first_instruction_) {
        prev_instr = prev_instr->GetPrevious();
      } else {
        break;
      }
    }  // end-while
  }
}

void SchedulingGraph::DumpGraph(std::string output_file) {
  std::ofstream output(output_file, std::ios::out);
  output << "digraph G {\n";
  for (auto& it : nodes_map_) {
    SchedulingNode* node = it.second;
    HInstruction* instr = node->GetInstruction();
    output << "H" << instr->GetId() << "[shape=record, label=\"{" << instr->GetId() << "_"
           << instr->DebugName() << "_" << node->GetDelay() << "}\"];\n";
    ArenaVector<SuccessorEdge*>& successors = node->GetSuccessorEdges();
    for (const SuccessorEdge* edge : successors) {
      HInstruction* successor = edge->GetSuccessorNode()->GetInstruction();
      output << "H" << successor->GetId() << "[shape=record, label=\"{" << successor->GetId()
             << "_" << successor->DebugName()
             << "_" << edge->GetSuccessorNode()->GetDelay() << "}\"];\n";
      output << "H" << instr->GetId() << ":s -> " << "H" << successor->GetId() << ":n"
             << " [label=\"" << node->GetCost() << "\"]\n";
    }
  }
  output << "}";
  output.close();
}

// Get an edge directed from p to s. Returns nullptr if there is no such edge.
SuccessorEdge* SchedulingGraph::GetSuccessorEdge(SchedulingNode* p, SchedulingNode* s) const {
  DCHECK(p != nullptr);
  DCHECK(s != nullptr);
  ArenaVector<SuccessorEdge*>& successors = p->GetSuccessorEdges();
  for (SuccessorEdge* edge : successors) {
    if (edge->GetSuccessorNode() == s) {
      return edge;
    }
  }
  return nullptr;
}

// Check whether there is an edge directed from p to s.
bool SchedulingGraph::HasDependency(SchedulingNode* p, SchedulingNode* s) const {
  return GetSuccessorEdge(p, s) != nullptr;
}

// Check whether there is an edge directed from p to s.
bool SchedulingGraph::HasDependency(const HInstruction* p, const HInstruction* s) const {
  SchedulingNode* prev = GetNode(p);
  SchedulingNode* succ = GetNode(s);
  if (prev == nullptr || succ == nullptr) {
    return false;
  }
  return HasDependency(prev, succ);
}

static bool WillNeedMaterialization(const HCondition* condition) {
  if (condition->HasOnlyOneNonEnvironmentUse()) {
    HInstruction* user = condition->GetUses().GetFirst()->GetUser();
    if (PrepareForRegisterAllocation::CanEmitConditionAt(condition, user)) {
      return false;
    }
  }
  return true;
}

void HScheduler::Schedule(HGraph* graph) {
  SchedulingGraph scheduling_graph(graph->GetArena());
  for (HReversePostOrderIterator block_it(*graph); !block_it.Done(); block_it.Advance()) {
    HBasicBlock* block = block_it.Current();
    if (!IsSchedulable(block)) {
      continue;
    }
    // Add instructions into scheduling_graph and start scheduling.
    for (HInstructionIterator it(block->GetInstructions()); !it.Done(); it.Advance()) {
      HInstruction* instr = it.Current();
      if (IsSchedulingBarrier(instr)) {
        // Schedule instructions up to this barrier and clear the graph to start a new
        // scheduling from next instruction.
        Schedule(&scheduling_graph);
        scheduling_graph.Clear();
        continue;
      }
      scheduling_graph.AddNode(instr);
    }
    Schedule(&scheduling_graph);
    scheduling_graph.Clear();
  }
}

void HScheduler::Schedule(SchedulingGraph* scheduling_graph) {
  if (scheduling_graph->Size() <= 2) {
    return;
  }
  // Initialize: calculate max delay for all nodes.
  CalculateDelayForAllNodes(scheduling_graph);
  // Initial candidates. A valid candidate should have all its predecessors scheduled before.
  ArenaVector<SchedulingNode*> candidates(arena_->Adapter());
  candidates.reserve(scheduling_graph->Size() / 2);
  for (auto& it : *scheduling_graph->GetSchedulingNodes()) {
    SchedulingNode* node = it.second;
    if (node->GetNumUnscheduledPredecessors() == 0) {
      candidates.push_back(node);
    }
  }

  int32_t ctime = 0;  // Simulated current time.
  HInstruction* cursor_instr = scheduling_graph->GetFirstInstruction();
  selector_->UpdateCurrentTime(0);
  while (candidates.size() > 0) {
    SchedulingNode* select_node = selector_->PopHighestPriorityNode(candidates);
    // Move this instruction to the new location.
    if (select_node->GetInstruction() != cursor_instr) {
      select_node->GetInstruction()->MoveBefore(cursor_instr);
    } else {
      cursor_instr = cursor_instr->GetNext();
    }
    // Update current time, if the selected instruction has to be started at a later time.
    ctime = std::max(ctime, select_node->GetStartTime());

    // If a node has been selected before, check whether any of its successors
    // are valid candidates.
    ArenaVector<SuccessorEdge*>& dependencies = select_node->GetSuccessorEdges();
    int32_t latency = select_node->GetCost();
    for (const SuccessorEdge* edge : dependencies) {
      SchedulingNode* successor = edge->GetSuccessorNode();
      successor->SetStartTime(std::max(successor->GetStartTime(), ctime + latency));
      successor->DecrementNumUnscheduledPredecessors();
      if (successor->GetNumUnscheduledPredecessors() == 0) {
        candidates.push_back(successor);
      }
    }
    ctime++;  // Next cycle.
    selector_->UpdateCurrentTime(ctime);
  }
}

void HScheduler::CalculateDelayForAllNodes(SchedulingGraph* scheduling_graph) {
  for (auto& it : *scheduling_graph->GetSchedulingNodes()) {
    SchedulingNode* node = it.second;
    CalculateCost(node);
  }
  for (auto& it : *scheduling_graph->GetSchedulingNodes()) {
    SchedulingNode* node = it.second;
    CalculateDelay(node);
  }
}

void HScheduler::CalculateDelay(SchedulingNode* node) {
  if (node->GetDelay() > -1) {
    // Already calculated.
    return;
  }
  ArenaVector<SuccessorEdge*>& successors = node->GetSuccessorEdges();
  int32_t max_successor_delay = 0;
  for (const SuccessorEdge* edge : successors) {
    SchedulingNode* successor = edge->GetSuccessorNode();
    CalculateDelay(successor);
    max_successor_delay = std::max(max_successor_delay, successor->GetDelay());
  }
  node->SetDelay(node->GetCost() + max_successor_delay);
}

bool HScheduler::IsSchedulable(const HInstruction* instruction) const {
  // We want to avoid exhaustively listing all instructions, so we first check
  // for instruction categories that we know are safe.
  if (instruction->IsControlFlow() ||
      instruction->IsConstant()) {
    return true;
  }
    // Currently all unary and binary operations are safe to schedule, so avoid
    // checking for each of them individually.
    // Since nothing prevents a new scheduling-unsafe HInstruction to subclass
    // HUnaryOperation (or HBinaryOperation), check in debug mode that we have
    // the exhaustive lists here.
  if (instruction->IsUnaryOperation()) {
    DCHECK(instruction->IsBooleanNot() ||
           instruction->IsNot() ||
           instruction->IsNeg());
    return true;
  }
  if (instruction->IsBinaryOperation()) {
    DCHECK(instruction->IsAdd() ||
           instruction->IsAnd() ||
           instruction->IsCompare() ||
           instruction->IsCondition() ||
           instruction->IsDiv() ||
           instruction->IsMul() ||
           instruction->IsOr() ||
           instruction->IsRem() ||
           instruction->IsRor() ||
           instruction->IsShl() ||
           instruction->IsShr() ||
           instruction->IsSub() ||
           instruction->IsUShr() ||
           instruction->IsXor());
    return true;
  }
  // The scheduler should not see any of these.
  DCHECK(!instruction->IsLoadLocal() &&
         !instruction->IsLocal() &&
         !instruction->IsParallelMove() &&
         !instruction->IsStoreLocal());
  // List of instructions explicitly excluded:
  //    HClearException
  //    HClinitCheck
  //    HDeoptimize
  //    HLoadClass
  //    HLoadException
  //    HMemoryBarrier
  //    HMonitorOperation
  //    HNativeDebugInfo
  //    HThrow
  //    HTryBoundary
  // TODO: Some of the instructions above may be safe to schedule (maybe as
  // scheduling barriers).
  return instruction->IsArrayGet() ||
      instruction->IsArrayLength() ||
      instruction->IsBoundType() ||
      instruction->IsBoundsCheck() ||
      instruction->IsCheckCast() ||
      instruction->IsCurrentMethod() ||
      instruction->IsDivZeroCheck() ||
      instruction->IsInstanceFieldGet() ||
      instruction->IsInstanceFieldSet() ||
      instruction->IsInstanceOf() ||
      instruction->IsInvokeInterface() ||
      instruction->IsInvokeStaticOrDirect() ||
      instruction->IsInvokeUnresolved() ||
      instruction->IsInvokeVirtual() ||
      instruction->IsLoadString() ||
      instruction->IsNewArray() ||
      instruction->IsNewInstance() ||
      instruction->IsNullCheck() ||
      instruction->IsPackedSwitch() ||
      instruction->IsParameterValue() ||
      instruction->IsPhi() ||
      instruction->IsReturn() ||
      instruction->IsReturnVoid() ||
      instruction->IsStaticFieldGet() ||
      instruction->IsStaticFieldSet() ||
      instruction->IsSuspendCheck() ||
      instruction->IsTemporary() ||
      instruction->IsTypeConversion() ||
      instruction->IsUnresolvedInstanceFieldGet() ||
      instruction->IsUnresolvedInstanceFieldSet() ||
      instruction->IsUnresolvedStaticFieldGet() ||
      instruction->IsUnresolvedStaticFieldSet();
}

bool HScheduler::IsSchedulable(const HBasicBlock* block) const {
  // We may be only interested in loop blocks.
  if (optimize_loop_only_ && !block->IsInLoop()) {
    return false;
  }
  // Do not schedule blocks that are part of try-catch.
  if (block->GetTryCatchInformation() != nullptr) {
    return false;
  }
  // Check whether all instructions in this block are schedulable.
  for (HInstructionIterator it(block->GetInstructions()); !it.Done(); it.Advance()) {
    if (!IsSchedulable(it.Current())) {
      return false;
    }
  }
  return true;
}

bool HScheduler::IsSchedulingBarrier(const HInstruction* instr) const {
  return instr->IsControlFlow() ||
      // Don't break calling convention.
      instr->IsParameterValue() ||
      // Codegen of goto relies on SuspendCheck's position.
      instr->IsSuspendCheck() ||
      // Keep non-materialized conditions next to their user.
      (instr->IsCondition() && !WillNeedMaterialization(instr->AsCondition()));
}

void HInstructionScheduling::Run() {
  switch (isa_features_->GetInstructionSet()) {
    case kArm64: {
      if (isa_features_->AsArm64InstructionSetFeatures()->IsCortexA53()) {
        arm64::HArm64Scheduler scheduler(graph_->GetArena());
        scheduler.Schedule(graph_);
      }
      break;
    }
    default:
      break;
  }
}

}  // namespace art
