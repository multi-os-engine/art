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

#ifndef ART_COMPILER_OPTIMIZING_SCHEDULER_H_
#define ART_COMPILER_OPTIMIZING_SCHEDULER_H_

#include <fstream>

#include "driver/compiler_driver.h"
#include "nodes.h"
#include "optimization.h"

namespace art {

// Typically used as a default instruction cost.
static constexpr int32_t kGenericInstructionCost = 1;

class SuccessorEdge;

/**
 * DAG node for HInstruction scheduling.
 */
class SchedulingNode : public ArenaObject<kArenaAllocMisc> {
 public:
  SchedulingNode(HInstruction* instr, ArenaAllocator* arena)
      : cost_(0),
        instruction_(instr),
        delay_(-1),
        start_time_(0),
        num_unscheduled_predecessors_(0),
        successors_(arena->Adapter(kArenaAllocMisc)) {
    successors_.reserve(kReservedNumOfSuccessorEdges);
  }

  void AddSuccessorEdge(SuccessorEdge* successor) {
    successors_.push_back(successor);
  }

  void DecrementNumUnscheduledPredecessors() {
    num_unscheduled_predecessors_--;
  }

  int32_t GetCost() const {
    return cost_;
  }

  int32_t GetDelay() const {
    return delay_;
  }

  HInstruction* GetInstruction() const {
    return instruction_;
  }

  int32_t GetNumUnscheduledPredecessors() const {
    return num_unscheduled_predecessors_;
  }

  int32_t GetStartTime() const {
    return start_time_;
  }

  ArenaVector<SuccessorEdge*>& GetSuccessorEdges() {
    return successors_;
  }

  void SetCost(int32_t cost) {
    cost_ = cost;
  }

  void SetDelay(int32_t delay) {
    delay_ = delay;
  }

  void SetStartTime(int32_t start_time) {
    start_time_ = start_time;
  }

 private:
  // A cost of an instruction is used by the scheduling heuristics to measure this
  // instruction's priority. It is typically defined by the instruction's execution latency.
  int32_t cost_;

  HInstruction* instruction_;

  // The max delay of an instruction is used by the scheduling heuristics to measure this
  // instructions' priority.
  // We define it as the critical path of the graph,
  //     node.delay = node.cost_ + max{delay of node's successors}
  int32_t delay_;

  // Estimated start time when this instruction can be issued.
  // It can be updated and used by scheduler.
  int32_t start_time_;

  int32_t num_unscheduled_predecessors_;
  ArenaVector<SuccessorEdge*> successors_;

  static constexpr int32_t kReservedNumOfSuccessorEdges = 2;
  friend class SchedulingGraph;
};

/**
 * DAG outgoing edge for HInstruction scheduling. If A depends on B, there is an edge from B to A.
 */
class SuccessorEdge : public ArenaObject<kArenaAllocMisc> {
 public:
  explicit SuccessorEdge(SchedulingNode* successor) : successor_(successor) {}

  SchedulingNode* GetSuccessorNode() const {
    return successor_;
  }

 private:
  // Other information can be added to the edge, e.g. pipeline bypass.
  SchedulingNode* successor_;
};

/*
 * DAG with SchedulingNode for scheduling.
 */
class SchedulingGraph {
 public:
  explicit SchedulingGraph(ArenaAllocator* arena)
      : arena_(arena),
        first_instruction_(nullptr),
        nodes_map_(arena_->Adapter()) {}
  virtual ~SchedulingGraph() {}

  void AddNode(HInstruction* instr) {
    if (first_instruction_ == nullptr) {
      first_instruction_ = instr;
    }
    SchedulingNode* node = new (arena_) SchedulingNode(instr, arena_);
    nodes_map_.Insert(std::make_pair(instr, node));
    AddDependencies(instr);
  }

  void Clear() {
    first_instruction_ = nullptr;
    nodes_map_.Clear();
  }

  // Dump DAG in dot file format.
  void DumpGraph(std::string output_file);

  HInstruction* GetFirstInstruction() const {
    return first_instruction_;
  }

  // Get the edge directed from p to s. Returns nullptr if there is no such edge.
  SuccessorEdge* GetSuccessorEdge(SchedulingNode* p, SchedulingNode* s) const;

  SchedulingNode* GetNode(const HInstruction* instr) const {
    auto it = nodes_map_.Find(instr);
    if (it == nodes_map_.end()) {
      return nullptr;
    } else {
      return it->second;
    }
  }

  // Check whether s is the immediate successor of p in the SchedulingGraph,
  // i.e. there is an edge directed from p to s;
  bool HasDependency(SchedulingNode* p, SchedulingNode* s) const;

  bool HasDependency(const HInstruction* p, const HInstruction* s) const;

  size_t Size() const {
    return nodes_map_.Size();
  }

  ArenaHashMap<const HInstruction*, SchedulingNode*>* GetSchedulingNodes() {
    return &nodes_map_;
  }

 protected:
  // Add a SuccessorEdge into SchedulingGraph, where output depends on intput.
  void AddDependency(const HInstruction* output, const HInstruction* input);

  // Add dependencies nodes for HInstruction: inputs, environments, side-effects.
  void AddDependencies(HInstruction* instruction);

  ArenaAllocator* arena_;
  HInstruction* first_instruction_;
  // HInstruction to SchedulingNode map.
  ArenaHashMap<const HInstruction*, SchedulingNode*> nodes_map_;
};

/*
 * TODO: description
 */
class SchedulingCostVisitor : public HGraphDelegateVisitor {
 public:
  // This class and its sub-classes will never be used to drive a visit of an
  // `HGraph` but only to visit `HInstructions` one at a time, so we do not need
  // to pass a valid graph to `HGraphDelegateVisitor()`.
  SchedulingCostVisitor() : HGraphDelegateVisitor(nullptr) {}

  void VisitInstruction(HInstruction* instruction) {
    UNREACHABLE();
    LOG(FATAL) << "Error visiting " << instruction->DebugName() << ". "
        "Architecture-specific scheduling cost visitors must handle all instructions"
        " (potentially by overriding the generic `VisitInstruction()`.";
  }

  void Visit(HInstruction* instruction) {
    instruction->Accept(this);
  }

  int CalculateCost(SchedulingNode* node) {
    Visit(node->GetInstruction());
    return last_visited_cost_;
  }

 protected:
  int last_visited_cost_;
};

class SchedulingNodeSelector : public ArenaObject<kArenaAllocMisc> {
 public:
  virtual SchedulingNode* PopHighestPriorityNode(ArenaVector<SchedulingNode*>& nodes) const = 0;
  virtual ~SchedulingNodeSelector() {}
  int32_t GetCurrentTime() const {
    return current_time_;
  }
  void UpdateCurrentTime(int32_t ctime) {
    current_time_ = ctime;
  }
  void DeleteNodeAtIndex(ArenaVector<SchedulingNode*>& nodes, size_t index) const {
    std::iter_swap(nodes.begin() + index, nodes.end() - 1);
    nodes.pop_back();
  }
 protected:
  int32_t current_time_;
};

/*
 * Select SchedulingNode in random order.
 */
class RandomSchedulingNodeSelector : public SchedulingNodeSelector {
 public:
  explicit RandomSchedulingNodeSelector(int seed) {
    srand(seed);
  }

  SchedulingNode* PopHighestPriorityNode(ArenaVector<SchedulingNode*>& nodes) const OVERRIDE {
    DCHECK(!nodes.empty());
    size_t select = rand() % nodes.size();
    SchedulingNode* select_node = nodes[select];
    DeleteNodeAtIndex(nodes, select);
    return select_node;
  }
};

/*
 * Select SchedulingNode by critical path information.
 */
class CriticalPathSchedulingNodeSelector : public SchedulingNodeSelector {
 public:
  SchedulingNode* PopHighestPriorityNode(ArenaVector<SchedulingNode*>& nodes) const OVERRIDE {
    DCHECK_GT(nodes.size(), 0u);
    SchedulingNode* select_node = nodes[0];
    size_t select = 0;
    for (size_t i = 1; i < nodes.size(); i++) {
      SchedulingNode* check = nodes[i];
      select_node = GetHigherPrioritySchedulingNode(nodes[select], check);
      if (select_node == check) {
        select = i;
      }
    }
    DeleteNodeAtIndex(nodes, select);
    return select_node;
  }

  SchedulingNode* GetHigherPrioritySchedulingNode(SchedulingNode* candidate,
                                                  SchedulingNode* check) const {
    // Critical path (delay of the node) has the highest priority.
    if (check->GetDelay() > candidate->GetDelay()) {
      return check;
    } else if (check->GetDelay() == candidate->GetDelay()) {
      if (candidate->GetStartTime() > current_time_ && check->GetStartTime() <= current_time_) {
        return check;
      } else if (candidate->GetStartTime() <= current_time_ &&
                 check->GetStartTime() <= current_time_) {
        return (check->GetCost() > candidate->GetCost()) ? check : candidate;
      } else if (candidate->GetStartTime() > current_time_ &&
                 check->GetStartTime() > current_time_) {
        return (check->GetStartTime() < candidate->GetStartTime()) ? check : candidate;
      }
    }
    return candidate;
  }
};

class HScheduler {
 public:
  HScheduler(ArenaAllocator* arena, SchedulingCostVisitor* cost_visitor)
      : optimize_loop_only_(true),
        arena_(arena),
        cost_visitor_(cost_visitor),
        selector_(new (arena_) CriticalPathSchedulingNodeSelector()) {}
  virtual ~HScheduler() {}

  void Schedule(HGraph* graph);

  void CalculateDelayForAllNodes(SchedulingGraph* scheduling_graph);

  void SetOptimizeLoopOnly(bool loop_only) {
    optimize_loop_only_ = loop_only;
  }

  void SetSelector(SchedulingNodeSelector* selector) {
    selector_ = selector;
  }

 protected:
  void Schedule(SchedulingGraph* sched_graph);
  void CalculateDelay(SchedulingNode* node);

  // Any instruction returning `false` via this method will prevent its
  // containing basic block from being scheduled.
  // This method is used to restrict scheduling to instructions that we know are
  // safe to handle.
  virtual bool IsSchedulable(const HInstruction* instruction) const;
  bool IsSchedulable(const HBasicBlock* block) const;

  // Instructions can not be rescheduled across a scheduling barrier.
  virtual bool IsSchedulingBarrier(const HInstruction* instruction) const;

  void CalculateCost(SchedulingNode* node) {
    node->SetCost(cost_visitor_->CalculateCost(node));
  }

  bool optimize_loop_only_;  // Only schedule instructions in loop blocks.
  ArenaAllocator* arena_;
  SchedulingCostVisitor* cost_visitor_;
  SchedulingNodeSelector* selector_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HScheduler);
};

class HInstructionScheduling : public HOptimization {
 public:
  HInstructionScheduling(HGraph* graph, const InstructionSetFeatures* isa_features)
      : HOptimization(graph, kInstructionScheduling),
        isa_features_(isa_features) {}

  void Run();

  // TODO: s/scheduler/instruction_scheduling/
  static constexpr const char* kInstructionScheduling = "scheduler";

  const InstructionSetFeatures* isa_features_;
 private:
  DISALLOW_COPY_AND_ASSIGN(HInstructionScheduling);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_SCHEDULER_H_
