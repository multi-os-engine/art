#ifndef ART_COMPILER_OPTIMIZING_CONTEXT_H_
#define ART_COMPILER_OPTIMIZING_CONTEXT_H_

#include "utils/growable_array.h"
#include "nodes.h"

namespace art {

/*
** HContext keeps track of instructions' properties while traversing the HGraph. The template
** parameter T is a class describing the property you want to track. T has to implement two static
** methods that should behave as a Monoid.
** - T T::Merge(const T& a, const T& b);
**   This method describes what should be the result when we're merging two properties from two
**   predecessors. It has to be pure, stateless, associative and commutative.
**
** - T Zero();
**   Returns a T that could be used as a default value. It needs to have the following property:
**   T::Merge(T::Zero(), x) ==  T::Merge(x, T::Zero()) == x.
**   It should behave as zero behaves with addition, hence the name.
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
      properties_.Insert(new SafeMap<int, T>());
    }
  }

  /*
  ** Set property for instr in current block. Overwrite previous value.
  */
  void SetProperty(HInstruction* instr, const T& property) {
    BlockProperties* block = BlockSafeGet(cur_block_);
    block->Overwrite(instr->GetId(), property);
  }

  /*
  ** Merge ni with the current property for the givin instr.
  ** Same as SetProperty(instr, T::Merge(GetProperty(instr), ni));
  */
  void MergeProperty(HInstruction* instr, const T& property) {
    MergeProperty(instr->GetId(), property);
  }

  /*
  ** Merge ni with the current property for the givin instr.
  ** Same as SetProperty(instr, T::Merge(GetProperty(instr), ni));
  */
  void MergeProperty(int instr_id, const T& property) {
    BlockProperties* block = BlockSafeGet(cur_block_);
    typename BlockProperties::iterator res = block->Insert(instr_id, property);
    if (res != block->end()) {
      res->second = T::Merge(res->second, property);
    }
  }

  /*
  ** You *MUST* call this function on the beginning of every block when traversing the graph.
  ** It creates the current block of properties by merging its predecessors.
  */
  void StartBlock(HBasicBlock* b) {
    cur_block_ = b->GetBlockId();
    const GrowableArray<HBasicBlock*>& preds = b->GetPredecessors();

    if (preds.Size() == 0) {
      return;
    }

    for (size_t i = 0; i < preds.Size(); ++i) {
      const BlockProperties* cur_pred = BlockSafeGet(preds.Get(i)->GetBlockId());
      for (typename BlockProperties::const_iterator iter = cur_pred->begin();
          iter != cur_pred->end();
          ++iter) {
        MergeProperty(iter->first, iter->second);
      }
    }
  }

  T GetProperty(HInstruction* instr) {
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
  SafeMap<int, T>* BlockSafeGet(size_t idx) {
    if (properties_.Size() <= idx) {
      size_t old_size = properties_.Size();
      properties_.Resize(idx + 1);

      for (size_t i = old_size; i < idx + 1; ++i) {
        properties_.Insert(new SafeMap<int, T>());
      }
    }
    return properties_.Get(idx);
  }

  int cur_block_;
  GrowableArray<SafeMap<int, T>*> properties_;
  ArenaAllocator* arena_;
};

}
#endif  // ART_COMPILER_OPTIMIZING_CONTEXT_H_
