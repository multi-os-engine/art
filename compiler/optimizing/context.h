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
 * HContextualizedPass keeps track of instructions' properties during a HGraph traversal.
 * The template parameter T is a class describing the property you want to track. T has to
 * implement two static methods.
 *
 * - T T::Merge(const T& a, const T& b);
 *   This method describes what should be the result when we're merging two properties from two
 *   predecessors. It has to be pure, stateless, associative, commutative and monotonic.
 *
 * - T Default();
 *   Returns a default, safe value.
 *
 * - It should also implement an equality operator, operator==().
 *
 * Inherit it, override the Visit* functions, and call Run.
 *
 * You can do two operations on the context: SetProperty() and MergeProperty().
 * - SetProperty(x) simply replaces what is currently held by the context:
 *     property = x
 * - MergeProperty(x) Merges x with what was previously held.
 */
template <class T, class Visitor>
class HContextualizedPass : public Visitor
{
 public:
  typedef int BlockId;
  typedef int InstructionId;
  typedef SafeMap<InstructionId, T> BlockProperties;
  typedef SafeMap<BlockId, BlockProperties*> GraphProperties;

  HContextualizedPass(HGraph* graph)
    : Visitor(graph), graph_(graph) {}

  void Run() {
    for (HReversePostOrderIterator it(*graph_); !it.Done(); it.Advance()) {
      cur_block_ = it.Current()->GetBlockId();
      MergePredecessors();
      HBasicBlock* block = graph_->GetBlock(cur_block_);
      BeforeBlock(block);
      VisitBasicBlock(it.Current());

      ReplaceOutWithIn(cur_block_);
    }
  }

  ~HContextualizedPass() {
    for (typename GraphProperties::iterator i = in_.begin(); i != in_.end(); ++i) {
      delete i->second;
    }

    for (typename GraphProperties::iterator i = out_.begin(); i != out_.end(); ++i) {
      delete i->second;
    }
  }

  HGraph* GetGraph() const { return graph_; }

 protected:
  // Override those methods if needed.
  virtual void BeforeBlock(HBasicBlock*) {}
  virtual void VisitPhi(HPhi*) {}

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

    return T::Default();
  }

  void VisitBasicBlock(HBasicBlock* block) {
    for (HInstructionIterator it(block->GetPhis()); !it.Done(); it.Advance()) {
      HandlePhi(it.Current()->AsPhi());
    }

    for (HInstructionIterator it(block->GetInstructions()); !it.Done(); it.Advance()) {
      HInstruction* instr = it.Current();
      instr->Accept(this);
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

  void HandlePhi(HPhi* phi) {
    T value = GetProperty(phi->InputAt(0));
    for (size_t i = 1; i < phi->InputCount(); i++) {
      value = T::Merge(value, GetProperty(phi->InputAt(i)));
    }
    SetProperty(phi, value);
    VisitPhi(phi);
  }

  SafeMap<int, T>* BlockSafeGet(GraphProperties& set, size_t idx) {
    typename GraphProperties::const_iterator block = set.find(idx);
    if (block == set.end() || block->second == nullptr) {
      BlockProperties* block_map = new BlockProperties;
      set.Overwrite(idx, block_map);
      return block_map;
    }
    return block->second;
  }

  HGraph* graph_;
  size_t cur_block_;
  GraphProperties in_;
  GraphProperties out_;
};

}
#endif  // ART_COMPILER_OPTIMIZING_CONTEXT_H_
