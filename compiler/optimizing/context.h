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
 * HContext keeps track of instructions' properties during a HGraph traversal in topological order.
 * The template parameter T is a class describing the property you want to track. T has to
 * implement two static methods that should behave as a (commutative) Monoid.
 *
 * - T T::Merge(const T& a, const T& b);
 *   This method describes what should be the result when we're merging two properties from two
 *   predecessors. It has to be pure, stateless, associative and commutative.
 *
 * - T Zero();
 *   Returns a T that could be used as a default value. It needs to have the following property:
 *   T::Merge(T::Zero(), x) ==  T::Merge(x, T::Zero()) == x.
 *   It should behave as zero behaves with addition, hence the name.
 *
 * This is meant to be used *during* a graph traversal. This is *not* a storage of properties that
 * you can query afterwards: the context is consistent for a given instruction only at the point
 * it is traversed.
 *
 * You can do two operations on the context: SetProperty() and MergeProperty().
 * - SetProperty(x) simply replaces what is currently held by the context:
 *     property = x
 * - MergeProperty(x) Merges x with what
 */
template <class T>
class HContext
{
 public:
  typedef SafeMap<int, T> BlockProperties;

  HContext(HGraph* graph)
      : cur_block_(0),
        properties_(graph->GetArena(), graph->GetBlocks().Size()),
        arena_(graph->GetArena()) {
    for (size_t i = 0; i < graph->GetBlocks().Size(); ++i) {
      properties_.Insert(nullptr);
    }
  }

  /*
   * Set property for instr in current block. Overwrite previous value.
   */
  void SetProperty(HInstruction* instr, const T& property) {
    BlockProperties* block = BlockSafeGet(cur_block_);
    block->Overwrite(instr->GetId(), property);
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
    BlockProperties* block = BlockSafeGet(cur_block_);
    typename BlockProperties::iterator old_value = block->Insert(instr_id, property);
    if (old_value != block->end()) {
      old_value->second = T::Merge(old_value->second, property);
    }
  }

  /*
   * You *MUST* call this function on the beginning of every block when traversing the graph.
   * It creates the current block of properties by merging its predecessors.
   */
  void StartBlock(HBasicBlock* b) {
    cur_block_ = b->GetBlockId();
    const GrowableArray<HBasicBlock*>& preds = b->GetPredecessors();

    if (preds.Size() == 0) {
      return;
    }

    for (size_t i = 0; i < preds.Size(); ++i) {
      if (IsBlockInstantiated(preds.Get(i)->GetBlockId())) {
        const BlockProperties* cur_pred = BlockSafeGet(preds.Get(i)->GetBlockId());
        for (typename BlockProperties::const_iterator iter = cur_pred->begin();
            iter != cur_pred->end();
            ++iter) {
          MergeProperty(iter->first, iter->second);
        }
      }
    }
  }

  T GetProperty(HInstruction* instr) {
    if (!IsBlockInstantiated(cur_block_)) {
      return T::Zero();
    }

    const SafeMap<int, T>* block = BlockSafeGet(cur_block_);

    typename BlockProperties::const_iterator property = block->find(instr->GetId());

    if (property != block->end()) {
      return property->second;
    }

    return T::Zero();
  }

  ~HContext() {
    for (size_t i = 0; i < properties_.Size(); ++i) {
      delete properties_.Get(i);
    }
  }

private:
  bool IsBlockInstantiated(size_t block_id) {
    return block_id < properties_.Size() && properties_.Get(block_id) != nullptr;
  }

  SafeMap<int, T>* BlockSafeGet(size_t idx) {
    if (properties_.Size() <= idx) {
      size_t old_size = properties_.Size();
      properties_.Resize(idx + 1);

      for (size_t i = old_size; i < idx + 1; ++i) {
        properties_.Insert(nullptr);
      }
    }

    if (properties_.Get(idx) == nullptr) {
      properties_.Put(idx, new SafeMap<int, T>());
    }
    return properties_.Get(idx);
  }

  int cur_block_;
  GrowableArray<SafeMap<int, T>*> properties_;
  ArenaAllocator* arena_;
};

}
#endif  // ART_COMPILER_OPTIMIZING_CONTEXT_H_
