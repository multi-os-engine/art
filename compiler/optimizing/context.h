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

#include "nodes.h"
#include "utils/growable_array.h"

namespace art {

/*
 * HContextualizedPass keeps track of instructions' properties during an HGraph
 * traversal. The template parameter T is a class describing the property you
 * want to track. T has to implement two static methods.
 *
 * - T T::Merge(const T& a, const T& b);
 *   This method describes what should be the result when we're merging two
 *   properties from two predecessors. It has to be pure, stateless,
 *   associative, commutative and monotonic.
 *
 * - T Default();
 *   Returns a default, safe value.
 *
 * - It should also implement an equality operator, operator==().
 *
 * This class is intended to be used as a base class for concrete context-based
 * optimizations: inherit it, override the Visit* functions, and call Run.
 *
 * You can do two operations on the context: SetProperty() and MergeProperty().
 * - SetProperty(x) simply replaces what is currently held by the context:
 *     property = x
 * - MergeProperty(x) merges x with what was previously held.
 */
template <class T, class Visitor>
class HContextualizedPass : public Visitor {
 public:
  typedef int BlockId;
  typedef int InstructionId;
  typedef ArenaSafeMap<InstructionId, T> BlockProperties;
  typedef ArenaSafeMap<BlockId, BlockProperties*> GraphProperties;

  HContextualizedPass(HGraph* graph)
    : Visitor(graph),
      in_(std::less<int>(), graph->GetArena()->Adapter()),
      out_(std::less<int>(), graph->GetArena()->Adapter()) {}

  void Run() {
    for (HReversePostOrderIterator it(*Visitor::GetGraph()); !it.Done(); it.Advance()) {
      current_block_id_ = it.Current()->GetBlockId();
      MergePredecessors();
      HBasicBlock* block = Visitor::GetGraph()->GetBlock(current_block_id_);
      BeforeBlock(block);
      VisitBasicBlock(it.Current());

      ReplaceOutWithIn(current_block_id_);
    }
  }

  virtual ~HContextualizedPass() OVERRIDE {
    for (typename GraphProperties::iterator i = in_.begin(); i != in_.end(); ++i) {
      delete i->second;
    }

    for (typename GraphProperties::iterator i = out_.begin(); i != out_.end(); ++i) {
      delete i->second;
    }
  }

 protected:
  // Override these methods if needed.
  virtual void BeforeBlock(HBasicBlock*) {}

  /*
   * Set property for `instr` in current block. Overwrite previous value.
   */
  void SetProperty(HInstruction* instr, const T& property) {
    BlockProperties* block_props = BlockSafeGet(&in_, current_block_id_);
    std::pair<typename BlockProperties::iterator, bool> old_value
        = block_props->Insert(instr->GetId(), property);

    if (!old_value.second) {
      old_value.first->second = property;
    }
  }

  /*
   * Merge property with the current property for the given `instr`.
   */
  void MergeProperty(HInstruction* instr, const T& property) {
    MergeProperty(instr->GetId(), property);
  }

  /*
   * Merge property with the current property for the given `instr`.
   */
  void MergeProperty(int instr_id, const T& property) {
    BlockProperties* block_props = BlockSafeGet(&in_, current_block_id_);
    // Try to insert the new property value.
    std::pair<typename BlockProperties::iterator, bool> old_value =
        block_props->Insert(instr_id, property);

    // Insertion failed: a property was already existing for this instruction, merge them.
    if (!old_value.second) {
      old_value.first->second = T::Merge(old_value.first->second, property);
    }
  }

  T GetProperty(HInstruction* instr) {
    BlockProperties* block_props = BlockSafeGet(&in_, current_block_id_);
    typename BlockProperties::const_iterator property = block_props->find(instr->GetId());

    return (property != block_props->end()) ? property->second : T::Default();
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
    const GrowableArray<HBasicBlock*>& preds =
        Visitor::GetGraph()->GetBlock(current_block_id_)->GetPredecessors();

    if (preds.IsEmpty()) {
      return;
    }

    BlockSafeGet(&in_, current_block_id_)->clear();

    for (size_t i = 0, preds_size = preds.Size(); i < preds_size; ++i) {
      const BlockProperties* cur_pred = BlockSafeGet(&out_, preds.Get(i)->GetBlockId());
      for (typename BlockProperties::const_iterator iter = cur_pred->begin();
          iter != cur_pred->end();
          ++iter) {
        MergeProperty(iter->first, iter->second);
      }
    }
  }

  void ReplaceOutWithIn(size_t block_id) {
    BlockProperties* in = BlockSafeGet(&in_, block_id);
    delete out_.OptionGet(block_id, nullptr);
    out_.Overwrite(block_id, in);
    in_.Overwrite(block_id, nullptr);
  }

  void HandlePhi(HPhi* phi) {
    T value = GetProperty(phi->InputAt(0));
    for (size_t i = 1, inputs_size = phi->InputCount(); i < inputs_size; i++) {
      value = T::Merge(value, GetProperty(phi->InputAt(i)));
    }
    SetProperty(phi, value);
    Visitor::VisitPhi(phi);
  }

  BlockProperties* BlockSafeGet(GraphProperties* set, size_t idx) {
    typename GraphProperties::const_iterator block = set->find(idx);
    if (block == set->end() || block->second == nullptr) {
      BlockProperties* block_map =
          new BlockProperties(std::less<int>(), Visitor::GetGraph()->GetArena()->Adapter());
      set->Overwrite(idx, block_map);
      return block_map;
    }
    return block->second;
  }

  size_t current_block_id_;
  GraphProperties in_;
  GraphProperties out_;
};

}

#endif  // ART_COMPILER_OPTIMIZING_CONTEXT_H_
