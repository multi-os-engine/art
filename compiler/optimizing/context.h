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

#ifndef ART_COMPILER_OPTIMIZING_CONTEXT_H_
#define ART_COMPILER_OPTIMIZING_CONTEXT_H_

#include "utils/growable_array.h"
#include "nodes.h"

namespace art {

/*
 * HContext keeps track of instructions' properties during a HGraph traversal. It repeats the
 * traversal until everything stabilizes, ie, it does a fixpoint iteration.
 * The template parameter T is a class describing the property you want to track. T has to
 * implement two static methods that should behave as a semilattice.
 *
 * - T T::Merge(const T& a, const T& b);
 *   This method describes what should be the result when we're merging two properties from two
 *   predecessors. It has to be pure, stateless, associative, commutative and monotonic.
 *
 * - T Top();
 *   Returns a T having the following properties:
 *   T::Merge(T::Top(), x) ==  T::Merge(x, T::Top()) ==  T::Merge(x, x) == x, associativity,
 *   commutativity, idempotence.
 *
 * - It should also implement an equality operator, operator==().
 *
 * If you have any doubt, refer to the definitition of a semilattice, but knowing that a block
 * might be traversed several times and that the result should converge should instinctively give
 * you the properties your code should have.
 *
 * Inherit it, override the Visit* functions, and call Run.
 *
 * You can do two operations on the context: SetProperty() and MergeProperty().
 * - SetProperty(x) simply replaces what is currently held by the context:
 *     property = x
 * - MergeProperty(x) Merges x with what was previously held.
 */
template <class T, class Visitor>
class HContext : public Visitor
{
 public:
  typedef SafeMap<int, T> BlockProperties;
  typedef SafeMap<int, BlockProperties*> Properties;

  HContext(HGraph* graph)
    : Visitor(graph), graph_(graph) {}

  virtual void VisitPhi(HPhi*) {}

  /*
   * Dataflow analysis implemented by-the-book from the Dragon's book,
   * Figure 9.23 a: Iterative algorithm for a forward data-flow problem.
   */
  void Run() {
    bool changed = true;
    while (changed) {
      changed = false;
      for (HReversePostOrderIterator it(*graph_); !it.Done(); it.Advance()) {
        cur_block_ = it.Current()->GetBlockId();
        MergePredecessors();
        BlockProperties* in = BlockSafeGet(in_, cur_block_);
        VisitBasicBlock(it.Current());

        changed = changed || *in != *BlockSafeGet(out_, cur_block_);
        ReplaceOutWithIn(cur_block_);
      }
    }
  }

  ~HContext() {
    for (typename Properties::iterator i = in_.begin(); i != in_.end(); ++i) {
      delete i->second;
    }

    for (typename Properties::iterator i = out_.begin(); i != out_.end(); ++i) {
      delete i->second;
    }
  }

 protected:
  /*
   * Set property for instr in current block. Overwrite previous value.
   */
  void SetProperty(HInstruction* instr, const T& property) {
    BlockProperties* block_props = BlockSafeGet(in_, cur_block_);
    std::pair<typename BlockProperties::iterator, bool> old_value
        = block_props->Insert(instr->GetId(), property);

    if (!old_value.second) {
      old_value.first->second = property;
    }
  }

  /*
   * Merge property with the current property for the givin instr.
   * Same as SetProperty(instr, T::Merge(GetProperty(instr), ni));
   */
  void MergeProperty(HInstruction* instr, const T& property) {
    MergeProperty(instr->GetId(), property);
  }

  /*
   * Merge property with the current property for the givin instr.
   * Same as SetProperty(instr, T::Merge(GetProperty(instr), ni));
   */
  void MergeProperty(int instr_id, const T& property) {
    BlockProperties* block_props = BlockSafeGet(in_, cur_block_);
    std::pair<typename BlockProperties::iterator, bool> old_value
        = block_props->Insert(instr_id, property);

    if (!old_value.second) {
      old_value.first->second = T::Merge(old_value.first->second, property);
    }
  }

  T GetProperty(HInstruction* instr) {
    const SafeMap<int, T>* block_props = BlockSafeGet(in_, cur_block_);
    typename BlockProperties::const_iterator property = block_props->find(instr->GetId());

    if (property != block_props->end()) {
      return property->second;
    }

    return T::Top();
  }

  void VisitBasicBlock(HBasicBlock* block) {
    for (HInstructionIterator it(block->GetInstructions()); !it.Done(); it.Advance()) {
      HInstruction* instr = it.Current();
      instr->Accept(this);
    }

    for (HInstructionIterator it(block->GetPhis()); !it.Done(); it.Advance()) {
      VisitPhi(it.Current()->AsPhi());
    }
  }

private:
  /*
   * Used to create an initial state for the block being visited from the last state of the
   * predecessors.
   */
  void MergePredecessors() {
    const GrowableArray<HBasicBlock*>& preds = graph_->GetBlock(cur_block_)->GetPredecessors();

    if (preds.Size() == 0) {
      return;
    }

    BlockSafeGet(in_, cur_block_)->clear();

    for (size_t i = 0; i < preds.Size(); ++i) {
      const BlockProperties* cur_pred = BlockSafeGet(out_, preds.Get(i)->GetBlockId());
      for (typename BlockProperties::const_iterator iter = cur_pred->begin();
          iter != cur_pred->end();
          ++iter) {
        MergeProperty(iter->first, iter->second);
      }
    }
  }

  void ReplaceOutWithIn(size_t block) {
    BlockProperties* in = BlockSafeGet(in_, block);
    delete out_.OptionGet(block, nullptr);
    out_.Overwrite(block, in);
    in_.Overwrite(block, nullptr);
  }

  SafeMap<int, T>* BlockSafeGet(Properties& set, size_t idx) {
    typename Properties::const_iterator block = set.find(idx);
    if (block == set.end() || block->second == nullptr) {
      BlockProperties* block_map = new BlockProperties;
      set.Overwrite(idx, block_map);
      return block_map;
    }
    return block->second;
  }

  HGraph* graph_;
  size_t cur_block_;
  Properties in_;
  Properties out_;
};

}
#endif  // ART_COMPILER_OPTIMIZING_CONTEXT_H_
