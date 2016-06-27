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

// General description of instruction scheduling.
//
// This pass tries to improve the quality of the generated code by reordering
// instructions in the graph to avoid execution delays caused by execution
// dependencies.
// Currently, scheduling is performed at the block level, so no instruction
// ever leaves its block in this pass.
//
// The scheduling process iterates through blocks in the graph. For blocks that
// we can and want to schedule:
// 1) Build a dependency graph for instructions.
//    It includes data dependencies (inputs/uses), but also environment
//    dependencies and side-effect dependencies.
// 2) Schedule the dependency graph.
//    This is a topological sort of the dependency graph, using heuristics to
//    decide what node to scheduler first when there are multiple candidates.
//
// A few factors impacting the quality of the scheduling are:
// - The heuristics used to decide what node to schedule in the topological sort
//   when there are multiple valid candidates. There is a wide range of
//   complexity possible here, going from a simple model only considering
//   latencies, to a super detailed CPU pipeline model.
// - Fewer dependencies in the dependency graph give more freedom for the
//   scheduling heuristics. For example de-aliasing can allow possibilities for
//   reordering of memory accesses.
// - The level of abstraction of the IR. It is easier to evaluate scheduling for
//   IRs that translate to a single assembly instruction than for IRs
//   that generate multiple assembly instructions or generate different code
//   depending on properties of the IR.
// - Scheduling is performed before register allocation, it is not aware of the
//   impact of moving instructions on register allocation.
//
//
// The scheduling code uses the terms predecessors, successors, and dependencies.
// This can be confusing at times, so here are clarifications.
// These terms are used from the point of view of the program dependency graph. So
// the inputs of an instruction are part of its dependencies, and hence part its
// predecessors. So the uses of an instruction are (part of) its successors.
// (Side-effect dependencies can yield predecessors or successors that are not
// inputs or uses.)
//
// Here is a trivial example. For the Java code:
//
//    int a = 1 + 2;
//
// we could have the instructions
//
//    i1 HIntConstant 1
//    i2 HIntConstant 2
//    i3 HAdd [i1,i2]
//
// `i1` and `i2` are predecessors of `i3`.
// `i3` is a successor of `i1` and a successor of `i2`.
// In a scheduling graph for this code we would have three nodes `n1`, `n2`,
// and `n3` (respectively for instructions `i1`, `i1`, and `i3`).
// Conceptually the program dependency graph for this would contain two edges
//
//    n1 -> n3
//    n2 -> n3
//
// Since we schedule backwards (starting from the last instruction in each basic
// block), the implementation of nodes keeps a list of pointers their
// predecessors. So `n3` would keep pointers to its predecessors `n1` and `n2`.
//
// Node dependencies are also referred to from the program dependency graph
// point of view: we say that node `B` immediately depends on `A` if there is an
// edge from `A` to `B` in the program dependency graph. `A` is a predecessor of
// `B`, `B` is a successor of `A`. In the example above `n3` depends on `n1` and
// `n2`.
// Since nodes in the scheduling graph keep a list of their predecessors, node
// `B` will have a pointer to its predecessor `A`.
// As we schedule backwards, `B` will be selected for scheduling before `A` is.
//
// So the scheduling for the example above could happen as follow
//
//    |---------------------------+------------------------|
//    | candidates for scheduling | instructions scheduled |
//    | --------------------------+------------------------|
//
// The only node without successors is `n3`, so it is the only initial
// candidate.
//
//    | n3                        | (none)                 |
//
// We schedule `n3` as the last (and only) instruction. All its predecessors
// that do not have any unscheduled successors become candidate. That is, `n1`
// and `n2` become candidates.
//
//    | n1, n2                    | n3                     |
//
// One of the candidates is selected. In practice this is where scheduling
// heuristics kick in, to decide which of the candidates should be selected.
// In this example, let it be `n2`. It is scheduled before previously scheduled
// nodes (in program order). There are no other nodes to add to the list of
// candidates.
//
//    | n2                        | n1                     |
//    |                           | n3                     |
//
// The only candidate available for scheduling is `n2`. Schedule it before
// (in program order) the previously scheduled nodes.
//
//    | (none)                    | n2                     |
//    |                           | n1                     |
//    |                           | n3                     |
//    |---------------------------+------------------------|
//
// So finally the instructions will be executed in the order `i2`, `i1`, and `i3`.
// In this trivial example, it does not matter which of `i1` and `i2` is
// scheduled first since they are constants. However the same process would
// apply if `i1` and `i2` were actual operations (for example `HMul` and `HDiv`).

// Set to true to have instruction scheduling dump scheduling graphs to the file
// `scheduling_graphs.dot`. See `SchedulingGraph::DumpAsDotGraph()`.
static constexpr bool kDumpDotSchedulingGraphs = false;

// Typically used as a default instruction latency.
static constexpr int kGenericInstructionLatency = 1;

class HScheduler;

/**
 * A node representing an `HInstruction` in the `SchedulingGraph`.
 */
class SchedulingNode : public ArenaObject<kArenaAllocScheduling> {
 public:
  SchedulingNode(HInstruction* instr, ArenaAllocator* arena, bool is_scheduling_barrier)
      : latency_(0),
        internal_latency_(0),
        critical_path_(0),
        instruction_(instr),
        is_scheduling_barrier_(is_scheduling_barrier),
        data_predecessors_(arena->Adapter(kArenaAllocMisc)),
        other_predecessors_(arena->Adapter(kArenaAllocMisc)),
        num_unscheduled_successors_(0) {
    data_predecessors_.reserve(kPreallocatedPredecessors);
  }

  void AddDataPredecessor(SchedulingNode* predecessor) {
    data_predecessors_.push_back(predecessor);
    predecessor->num_unscheduled_successors_++;
  }

  void AddOtherPredecessor(SchedulingNode* predecessor) {
    other_predecessors_.push_back(predecessor);
    predecessor->num_unscheduled_successors_++;
  }

  void DecrementNumberOfUnscheduledSuccessors() {
    num_unscheduled_successors_--;
  }

  void MaybeUpdateCriticalPath(int other_critical_path) {
    critical_path_ = std::max(critical_path_, other_critical_path);
  }


  bool HasUnscheduledSuccessors() const {
    return num_unscheduled_successors_ != 0;
  }

  HInstruction* GetInstruction() const { return instruction_; }
  int GetLatency() const { return latency_; }
  void SetLatency(int latency) { latency_ = latency; }
  int GetInternalLatency() const { return internal_latency_; }
  void SetInternalLatency(int internal_latency) { internal_latency_ = internal_latency; }
  int GetCriticalPath() const { return critical_path_; }
  bool IsSchedulingBarrier() const { return is_scheduling_barrier_; }
  ArenaVector<SchedulingNode*>& GetDataPredecessors() { return data_predecessors_; }
  ArenaVector<SchedulingNode*>& GetOtherPredecessors() { return other_predecessors_; }

 private:
  // The latency of this node. It represents the latency between the moment the
  // last instruction for this node has executed to the moment the result
  // produced by this node is available to users.
  int latency_;
  // This represent the time spent *within* the generated code for this node.
  // It should be null for nodes that only generate a single instruction.
  int internal_latency_;

  // The critical path from this instruction to the end of scheduling. It is
  // used by the scheduling heuristics to measure the priority of this instruction.
  // It is defined as
  //     critical_path_ = latency_ + max((use.internal_latency_ + use.critical_path_) for all uses)
  // (Note that here 'uses' is equivalent to 'data successors'. Also see comments in
  // `HScheduler::Schedule(SchedulingNode* scheduling_node)`)
  int critical_path_;

  // The instruction that this node represents.
  HInstruction* instruction_;

  bool is_scheduling_barrier_;

  // The lists of predecessors. They cannot be scheduled before this node. Once
  // this node is scheduled, we check whether any of its predecessors has become a
  // valid candidate for scheduling.
  // Predecessors in `data_predecessors_` are data dependencies. Those in
  // `other_predecessors_` contain side-effect dependencies, environment
  // dependencies, and scheduling barrier dependencies.
  ArenaVector<SchedulingNode*> data_predecessors_;
  ArenaVector<SchedulingNode*> other_predecessors_;

  // The number of unscheduled successors for this node. This number is
  // decremented as successors are scheduled. When it reaches zero this node
  // becomes a valid candidate to schedule.
  int num_unscheduled_successors_;

  static constexpr size_t kPreallocatedPredecessors = 2;
};

/*
 * Directed acyclic graph for scheduling.
 */
class SchedulingGraph {
 public:
  explicit SchedulingGraph(const HScheduler* scheduler, ArenaAllocator* arena)
      : scheduler_(scheduler),
        arena_(arena),
        contains_scheduling_barrier_(false),
        nodes_map_(arena_->Adapter()) {}
  virtual ~SchedulingGraph() {}

  SchedulingNode* AddNode(HInstruction* instr, bool is_scheduling_barrier = false) {
    SchedulingNode* node = new (arena_) SchedulingNode(instr, arena_, is_scheduling_barrier);
    nodes_map_.Insert(std::make_pair(instr, node));
    contains_scheduling_barrier_ |= is_scheduling_barrier;
    AddDependencies(instr, is_scheduling_barrier);
    return node;
  }

  void Clear() {
    nodes_map_.Clear();
    contains_scheduling_barrier_ = false;
  }

  SchedulingNode* GetNode(const HInstruction* instr) const {
    auto it = nodes_map_.Find(instr);
    if (it == nodes_map_.end()) {
      return nullptr;
    } else {
      return it->second;
    }
  }

  bool IsSchedulingBarrier(const HInstruction* instruction);

  bool HasImmediateDataDependency(SchedulingNode* node, SchedulingNode* other) const;
  bool HasImmediateDataDependency(const HInstruction* node, const HInstruction* other) const;
  bool HasImmediateOtherDependency(SchedulingNode* node, SchedulingNode* other) const;
  bool HasImmediateOtherDependency(const HInstruction* node, const HInstruction* other) const;

  size_t Size() const {
    return nodes_map_.Size();
  }

  ArenaHashMap<const HInstruction*, SchedulingNode*>* GetSchedulingNodes() {
    return &nodes_map_;
  }

  // Dump the scheduling graph, in dot file format, appending it to the file
  // `scheduling_graphs.dot`.
  void DumpAsDotGraph(const std::string& description,
                      ArenaVector<SchedulingNode*>* initial_candidates = nullptr);

 protected:
  void AddDependency(SchedulingNode* node, SchedulingNode* dependency, bool is_data_dependency);
  void AddDataDependency(SchedulingNode* node, SchedulingNode* dependency) {
    AddDependency(node, dependency, /*is_data_dependency*/true);
  }
  void AddOtherDependency(SchedulingNode* node, SchedulingNode* dependency) {
    AddDependency(node, dependency, /*is_data_dependency*/false);
  }

  // Add dependencies nodes for the given `HInstruction`: inputs, environments, and side-effects.
  void AddDependencies(HInstruction* instruction, bool is_scheduling_barrier = false);

  const HScheduler* scheduler_;

  ArenaAllocator* arena_;

  bool contains_scheduling_barrier_;

  ArenaHashMap<const HInstruction*, SchedulingNode*> nodes_map_;
};

/*
 * The visitors derived from this base class are used by schedulers to evaluate
 * the latencies of `HInstruction`s.
 */
class SchedulingLatencyVisitor : public HGraphDelegateVisitor {
 public:
  // This class and its sub-classes will never be used to drive a visit of an
  // `HGraph` but only to visit `HInstructions` one at a time, so we do not need
  // to pass a valid graph to `HGraphDelegateVisitor()`.
  SchedulingLatencyVisitor() : HGraphDelegateVisitor(nullptr) {}

  void VisitInstruction(HInstruction* instruction) {
    LOG(FATAL) << "Error visiting " << instruction->DebugName() << ". "
        "Architecture-specific scheduling latency visitors must handle all instructions"
        " (potentially by overriding the generic `VisitInstruction()`.";
    UNREACHABLE();
  }

  void Visit(HInstruction* instruction) {
    instruction->Accept(this);
  }

  void CalculateLatency(SchedulingNode* node) {
    // By default nodes have no internal latency.
    last_visited_internal_latency_ = 0;
    Visit(node->GetInstruction());
  }

  int GetLastVisitedLatency() const { return last_visited_latency_; }
  int GetLastVisitedInternalLatency() const { return last_visited_internal_latency_; }

 protected:
  int last_visited_latency_;
  int last_visited_internal_latency_;
};

class SchedulingNodeSelector : public ArenaObject<kArenaAllocMisc> {
 public:
  virtual SchedulingNode* PopHighestPriorityNode(ArenaVector<SchedulingNode*>& nodes) const = 0;
  virtual ~SchedulingNodeSelector() {}
  void DeleteNodeAtIndex(ArenaVector<SchedulingNode*>& nodes, size_t index) const {
    nodes[index] = nodes.back();
    nodes.pop_back();
  }
};

/*
 * Select a `SchedulingNode` at random within the candidates.
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
 * Select a `SchedulingNode` according to critical path information.
 */
class CriticalPathSchedulingNodeSelector : public SchedulingNodeSelector {
 public:
  SchedulingNode* PopHighestPriorityNode(ArenaVector<SchedulingNode*>& nodes) const OVERRIDE {
    DCHECK(!nodes.empty());
    SchedulingNode* select_node = nodes[0];
    size_t select = 0;
    for (size_t i = 1, e = nodes.size(); i < e; i++) {
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
    int candidate_path = candidate->GetCriticalPath();
    int check_path = check->GetCriticalPath();
    // First look at the critical_path.
    if (check_path != candidate_path) {
      return check_path < candidate_path ? check : candidate;
    }
    // If both critical paths are equal, schedule instructions with a higher latency
    // first in program order.
    return check->GetLatency() < candidate->GetLatency() ? check : candidate;
  }
};

class HScheduler {
 public:
  HScheduler(ArenaAllocator* arena, SchedulingLatencyVisitor* latency_visitor)
      : optimize_loop_only_(true),
        arena_(arena),
        latency_visitor_(latency_visitor),
        selector_(new (arena_) CriticalPathSchedulingNodeSelector()),
        scheduling_graph_(this, arena),
        candidates_(arena_->Adapter(kArenaAllocScheduling)) {}
  virtual ~HScheduler() {}

  void Schedule(HGraph* graph);

  void SetOptimizeLoopOnly(bool loop_only) { optimize_loop_only_ = loop_only; }
  void SetSelector(SchedulingNodeSelector* selector) { selector_ = selector; }

  // Instructions can not be rescheduled across a scheduling barrier.
  virtual bool IsSchedulingBarrier(const HInstruction* instruction) const;

 protected:
  void Schedule(HBasicBlock* block);
  void Schedule(SchedulingNode* scheduling_node);
  void Schedule(HInstruction* instruction);

  // Any instruction returning `false` via this method will prevent its
  // containing basic block from being scheduled.
  // This method is used to restrict scheduling to instructions that we know are
  // safe to handle.
  virtual bool IsSchedulable(const HInstruction* instruction) const;
  bool IsSchedulable(const HBasicBlock* block) const;

  void CalculateLatency(SchedulingNode* node) {
    latency_visitor_->CalculateLatency(node);
    node->SetLatency(latency_visitor_->GetLastVisitedLatency());
    node->SetInternalLatency(latency_visitor_->GetLastVisitedInternalLatency());
  }

  bool optimize_loop_only_;  // If `true`, only schedule instructions in loop blocks.
  ArenaAllocator* arena_;
  SchedulingLatencyVisitor* latency_visitor_;
  SchedulingNodeSelector* selector_;

  // We instantiate the members below as part of this class to avoid
  // instantiating them locally for every chunk scheduled.
  SchedulingGraph scheduling_graph_;
  // A pointer indicating where the next instruction to be scheduled will be inserted.
  HInstruction* cursor_;
  // The list of candidates for scheduling. A node becomes a candidate when all
  // its predecessors have been scheduled.
  ArenaVector<SchedulingNode*> candidates_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HScheduler);
};


inline bool SchedulingGraph::IsSchedulingBarrier(const HInstruction* instruction) {
  return scheduler_->IsSchedulingBarrier(instruction);
}


class HInstructionScheduling : public HOptimization {
 public:
  HInstructionScheduling(HGraph* graph, InstructionSet instruction_set)
      : HOptimization(graph, kInstructionScheduling),
        instruction_set_(instruction_set) {}

  void Run();

  static constexpr const char* kInstructionScheduling = "scheduler";

  InstructionSet instruction_set_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HInstructionScheduling);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_SCHEDULER_H_
