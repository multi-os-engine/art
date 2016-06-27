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

#include <string>

#include "arch/arm64/instruction_set_features_arm64.h"
#include "prepare_for_register_allocation.h"
#include "scheduler_arm64.h"
#include "scheduler.h"

namespace art {

void SchedulingGraph::AddDependency(SchedulingNode* node,
                                    SchedulingNode* dependency,
                                    bool is_data_dependency) {
  if (node == nullptr || dependency == nullptr) {
    // A `nullptr` node indicates an instruction out of scheduling range (eg. in
    // an other block), so we do not need to add a dependency edge to the graph.
    return;
  }

  if (is_data_dependency) {
    if (!HasImmediateDataDependency(node, dependency)) {
      node->AddDataPredecessor(dependency);
    }
  } else if (!HasImmediateOtherDependency(node, dependency)) {
    node->AddOtherPredecessor(dependency);
  }
}

// Check whether `node` depends on `other` , taking into
// account `SideEffect` information and `CanThrow` information.
static bool HasSideEffectDependency(const HInstruction* node,
                                    const HInstruction* other) {
  // TODO: Current side effect dependency for heap memory aliasing can be improved by
  // detecting memory location information.
  if (node->GetSideEffects().MayHaveReorderingDependency(other->GetSideEffects())) {
    return true;
  } else if ((other->CanThrow() && node->GetSideEffects().DoesAnyWrite()) ||
             (other->GetSideEffects().DoesAnyWrite() && node->CanThrow()) ||
             (other->CanThrow() && node->CanThrow())) {
    return true;
  }
  return false;
}

void SchedulingGraph::AddDependencies(HInstruction* instruction, bool is_scheduling_barrier) {
  SchedulingNode* instruction_node = GetNode(instruction);

  // Define-use dependencies.
  for (const HUseListNode<HInstruction*>& use : instruction->GetUses()) {
    AddDataDependency(GetNode(use.GetUser()), instruction_node);
  }

  // Scheduling barrier dependencies.
  DCHECK(!is_scheduling_barrier || contains_scheduling_barrier_);
  if (contains_scheduling_barrier_) {
    // A barrier depends on instructions after it. And instructions before the
    // barrier depend on it.
    for (HInstruction* other = instruction->GetNext(); other != nullptr; other = other->GetNext()) {
      SchedulingNode* other_node = GetNode(other);
      bool other_is_barrier = other_node->IsSchedulingBarrier();
      if (is_scheduling_barrier || other_is_barrier) {
        AddOtherDependency(other_node, instruction_node);
      }
      if (other_is_barrier) {
        // This other scheduling barrier guarantees ordering of instructions after
        // it, so avoid creating additional useless dependencies in the graph.
        // For example if we have
        //     instr_1
        //     barrier_2
        //     instr_3
        //     barrier_4
        //     instr_5
        // we only create the following non-data dependencies
        //     1 -> 2
        //     2 -> 3
        //     2 -> 4
        //     3 -> 4
        //     4 -> 5
        // and do not create
        //     1 -> 4
        //     2 -> 5
        // Note that in this example we could also avoid creating the dependency
        // `2 -> 4`.  But if we remove `instr_3` that dependency is required to
        // order the barriers. So we generate it to avoid a special case.
        break;
      }
    }
  }

  // Side effect dependencies.
  if (!instruction->GetSideEffects().DoesNothing() || instruction->CanThrow()) {
    for (HInstruction* other = instruction->GetNext(); other != nullptr; other = other->GetNext()) {
      SchedulingNode* other_node = GetNode(other);
      if (other_node->IsSchedulingBarrier()) {
        // We have reached a scheduling barrier so we can stop further
        // processing.
        DCHECK(HasImmediateOtherDependency(other_node, instruction_node));
        break;
      }
      if (HasSideEffectDependency(other, instruction)) {
        AddOtherDependency(other_node, instruction_node);
      }
    }
  }

  // Environment dependencies.
  // We do not need to process those if the instruction is a scheduling barrier,
  // since the barrier already has non-data dependencies on all following
  // instructions.
  if (!is_scheduling_barrier) {
    for (const HUseListNode<HEnvironment*>& use : instruction->GetEnvUses()) {
      // Note that here we could stop processing if the environment holder is
      // across a scheduling barrier. But checking this would likely require
      // more work than simply iterating through environment uses.
      AddOtherDependency(GetNode(use.GetUser()->GetHolder()), instruction_node);
    }
  }
}

bool SchedulingGraph::HasImmediateDataDependency(SchedulingNode* node,
                                                 SchedulingNode* other) const {
  for (SchedulingNode* predecessor : node->GetDataPredecessors()) {
    if (predecessor == other) return true;
  }
  return false;
}

bool SchedulingGraph::HasImmediateDataDependency(const HInstruction* instruction,
                                                 const HInstruction* other_instruction) const {
  SchedulingNode* node = GetNode(instruction);
  SchedulingNode* other = GetNode(other_instruction);
  if (node == nullptr || other == nullptr) {
    return false;
  }
  return HasImmediateDataDependency(node, other);
}

bool SchedulingGraph::HasImmediateOtherDependency(SchedulingNode* node,
                                                  SchedulingNode* other) const {
  for (SchedulingNode* predecessor : node->GetOtherPredecessors()) {
    if (predecessor == other) return true;
  }
  return false;
}

bool SchedulingGraph::HasImmediateOtherDependency(
    const HInstruction* instruction, const HInstruction* other_instruction) const {
  SchedulingNode* node = GetNode(instruction);
  SchedulingNode* other = GetNode(other_instruction);
  if (node == nullptr || other == nullptr) {
    return false;
  }
  return HasImmediateOtherDependency(node, other);
}

static const std::string InstructionTypeId(const HInstruction* instruction) {
  std::string id;
  Primitive::Type type = instruction->GetType();
  if (type == Primitive::kPrimNot) {
    id.append("l");
  } else {
    id.append(Primitive::Descriptor(instruction->GetType()));
  }
  // Use lower-case to be closer to the `HGraphVisualizer` output.
  id[0] = std::tolower(id[0]);
  id.append(std::to_string(instruction->GetId()));
  return id;
}

// Ideally we would reuse the graph visualizer code, but it is not available
// from here and it is not worth moving all that code only for our use.
static void DumpAsDotNode(std::ostream& output, SchedulingNode* node) {
  HInstruction* instruction = node->GetInstruction();
  // Use the instruction typed id as the node identifier.
  std::string instruction_id = InstructionTypeId(instruction);
  output << instruction_id << "[shape=record, label=\""
      << instruction_id << ' ' << instruction->DebugName() << " [";
  // List the instruction's inputs in its description. When visualizing the
  // graph this helps differentiating data inputs from other dependencies.
  for (HInstruction* input : instruction->GetInputs()) {
    output << InstructionTypeId(input) << ',';
  }
  output << "]";
  // Other properties of the node.
  output << "\\ninternal_latency: " << node->GetInternalLatency();
  output << "\\ncritical_path: " << node->GetCriticalPath();
  if (node->IsSchedulingBarrier()) {
    output << "\\n(barrier)";
  }
  output << "\"];\n";
  // We want program order to go from top to bottom in the graph output, so we
  // reverse the edges and specify `dir=back`.
  for (const SchedulingNode* predecessor : node->GetDataPredecessors()) {
    HInstruction* predecessor_instruction = predecessor->GetInstruction();
    output << InstructionTypeId(predecessor_instruction) << ":s -> " << instruction_id << ":n "
        << "[label=\"" << predecessor->GetLatency() << "\",dir=back]\n";
  }
  for (const SchedulingNode* predecessor : node->GetOtherPredecessors()) {
    HInstruction* predecessor_instruction = predecessor->GetInstruction();
    output << InstructionTypeId(predecessor_instruction) << ":s -> " << instruction_id << ":n "
        << "[dir=back,color=blue]\n";
  }
}

void SchedulingGraph::DumpAsDotGraph(const std::string& description,
                                     ArenaVector<SchedulingNode*>* initial_candidates) {
  std::ofstream output("scheduling_graphs.dot", std::ofstream::out | std::ofstream::app);
  // Description of this graph, as a comment.
  output << "// " << description << "\n";
  // Start the dot graph. Use an increasing index for easier differentiation.
  static unsigned int graph_index = 0;
  output << "digraph G" << graph_index++ << " {\n";
  for (const auto& entry : nodes_map_) {
    DumpAsDotNode(output, entry.second);
  }
  if (initial_candidates != nullptr) {
    // Create a fake 'end_of_scheduling' node to help visualization of critical_paths.
    for (auto node : *initial_candidates) {
      const HInstruction* instruction = node->GetInstruction();
      output << InstructionTypeId(instruction) << ":s -> end_of_scheduling:n "
          << "[label=\"" << node->GetLatency() << "\",dir=back]\n";
    }
  }
  // End of the dot graph.
  output << "}\n";
  output.close();
}

void HScheduler::Schedule(HGraph* graph) {
  for (HReversePostOrderIterator block_it(*graph); !block_it.Done(); block_it.Advance()) {
    HBasicBlock* block = block_it.Current();
    if (IsSchedulable(block)) {
      Schedule(block);
    }
  }
}

void HScheduler::Schedule(HBasicBlock* block) {
  // Build the scheduling graph.
  scheduling_graph_.Clear();
  for (HBackwardInstructionIterator it(block->GetInstructions()); !it.Done(); it.Advance()) {
    HInstruction* instruction = it.Current();
    SchedulingNode* node = scheduling_graph_.AddNode(instruction, IsSchedulingBarrier(instruction));
    CalculateLatency(node);
  }

  if (scheduling_graph_.Size() <= 1) {
    scheduling_graph_.Clear();
    return;
  }

  cursor_ = block->GetLastInstruction();

  // Find the initial candidates for scheduling.
  candidates_.clear();
  for (const auto& it : *scheduling_graph_.GetSchedulingNodes()) {
    SchedulingNode* node = it.second;
    if (!node->HasUnscheduledSuccessors()) {
      node->MaybeUpdateCriticalPath(node->GetLatency());
      candidates_.push_back(node);
    }
  }

  ArenaVector<SchedulingNode*>* initial_candidates;
  if (kDumpDotSchedulingGraphs) {
    // Remember the list of initial candidates for debug output purposes.
    initial_candidates = new ArenaVector<SchedulingNode*>(candidates_,
                                                          arena_->Adapter(kArenaAllocScheduling));
  }

  // Schedule all nodes.
  while (!candidates_.empty()) {
    Schedule(selector_->PopHighestPriorityNode(candidates_));
  }

  if (kDumpDotSchedulingGraphs) {
    // Dump the graph in `dot` format.
    HGraph* graph = block->GetGraph();
    std::stringstream description;
    description << PrettyMethod(graph->GetMethodIdx(), graph->GetDexFile())
        << " B" << block->GetBlockId();
    scheduling_graph_.DumpAsDotGraph(description.str(), initial_candidates);
    delete initial_candidates;
  }
}

void HScheduler::Schedule(SchedulingNode* scheduling_node) {
  // Check whether any of the node's predecessors will be valid candidates after
  // this node is scheduled.
  int path_to_node = scheduling_node->GetCriticalPath();
  for (SchedulingNode* predecessor : scheduling_node->GetDataPredecessors()) {
    predecessor->MaybeUpdateCriticalPath(
        path_to_node + predecessor->GetInternalLatency() + predecessor->GetLatency());
    predecessor->DecrementNumberOfUnscheduledSuccessors();
    if (!predecessor->HasUnscheduledSuccessors()) {
      candidates_.push_back(predecessor);
    }
  }
  for (SchedulingNode* predecessor : scheduling_node->GetOtherPredecessors()) {
    // Do not update the critical path.
    // The 'other' (so 'non-data') dependencies (usually) do not represent a
    // 'material' dependency of nodes on others. They exist for program
    // correctness. So we do not use them to compute the critical path.
    predecessor->DecrementNumberOfUnscheduledSuccessors();
    if (!predecessor->HasUnscheduledSuccessors()) {
      candidates_.push_back(predecessor);
    }
  }
  Schedule(scheduling_node->GetInstruction());
}

void HScheduler::Schedule(HInstruction* instruction) {
  if (instruction == cursor_) {
    cursor_ = cursor_->GetPrevious();
  } else {
    instruction->MoveAfter(cursor_);
  }

  // Schedule condition inputs that can be materialized immediately before their use.
  const HCondition* condition = nullptr;
  if (instruction->IsIf()) {
    condition = instruction->AsIf()->InputAt(0)->AsCondition();
  } else if (instruction->IsSelect()) {
    condition = instruction->AsSelect()->GetCondition()->AsCondition();
  }
  SchedulingNode* condition_node =
      (condition != nullptr) ? scheduling_graph_.GetNode(condition) : nullptr;
  if ((condition_node != nullptr) &&
      condition->HasOnlyOneNonEnvironmentUse() &&
      PrepareForRegisterAllocation::CouldEmitConditionBefore(condition, instruction)) {
    DCHECK(!condition_node->HasUnscheduledSuccessors());
    // Remove the condition from the list of candidates and schedule it.
    ArenaVector<SchedulingNode*>::iterator it =
        std::find(candidates_.begin(), candidates_.end(), condition_node);
    DCHECK(it != candidates_.end());
    *it = candidates_.back();
    candidates_.pop_back();
    Schedule(condition_node);
  }
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
           instruction->IsNeg()) << "unexpected instruction " << instruction->DebugName();
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
           instruction->IsXor()) << "unexpected instruction " << instruction->DebugName();
    return true;
  }
  // The scheduler should not see any of these.
  DCHECK(!instruction->IsParallelMove()) << "unexpected instruction " << instruction->DebugName();
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
      instruction->IsArraySet() ||
      instruction->IsArrayLength() ||
      instruction->IsBoundType() ||
      instruction->IsBoundsCheck() ||
      instruction->IsCheckCast() ||
      instruction->IsClassTableGet() ||
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
      instruction->IsSelect() ||
      instruction->IsStaticFieldGet() ||
      instruction->IsStaticFieldSet() ||
      instruction->IsSuspendCheck() ||
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
      // Code generation of goto relies on SuspendCheck's position.
      instr->IsSuspendCheck();
}

void HInstructionScheduling::Run() {
  switch (instruction_set_) {
    case kArm64: {
      arm64::HArm64Scheduler scheduler(graph_->GetArena());
      scheduler.Schedule(graph_);
      break;
    }
    default:
      break;
  }
}

}  // namespace art
