/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ART_COMPILER_OPTIMIZING_ITERATORS_H_
#define ART_COMPILER_OPTIMIZING_ITERATORS_H_

#include "locations.h"
#include "nodes.h"
#include "offsets.h"
#include "primitive.h"
#include "utils/arena_object.h"
#include "utils/arena_bit_vector.h"
#include "utils/growable_array.h"

namespace art {

template<typename T>
class HUseIterator : public ValueObject {
 public:
  explicit HUseIterator(HUseListNode<T>* uses) : current_(uses) {}

  bool Done() const { return current_ == nullptr; }

  void Advance() {
    DCHECK(!Done());
    current_ = current_->GetTail();
  }

  HUseListNode<T>* Current() const {
    DCHECK(!Done());
    return current_;
  }

 private:
  HUseListNode<T>* current_;

  friend class HValue;
};

class HInputIterator : public ValueObject {
 public:
  explicit HInputIterator(HInstruction* instruction) : instruction_(instruction), index_(0) {}

  bool Done() const { return index_ == instruction_->InputCount(); }
  HInstruction* Current() const { return instruction_->InputAt(index_); }
  void Advance() { index_++; }

 private:
  HInstruction* instruction_;
  size_t index_;

  DISALLOW_COPY_AND_ASSIGN(HInputIterator);
};

class HInstructionIterator : public ValueObject {
 public:
  explicit HInstructionIterator(const HInstructionList& instructions)
      : instruction_(instructions.first_instruction_) {
    next_ = Done() ? nullptr : instruction_->GetNext();
  }

  bool Done() const { return instruction_ == nullptr; }
  HInstruction* Current() const { return instruction_; }
  void Advance() {
    instruction_ = next_;
    next_ = Done() ? nullptr : instruction_->GetNext();
  }

 private:
  HInstruction* instruction_;
  HInstruction* next_;

  DISALLOW_COPY_AND_ASSIGN(HInstructionIterator);
};

class HBackwardInstructionIterator : public ValueObject {
 public:
  explicit HBackwardInstructionIterator(const HInstructionList& instructions)
      : instruction_(instructions.last_instruction_) {
    next_ = Done() ? nullptr : instruction_->GetPrevious();
  }

  bool Done() const { return instruction_ == nullptr; }
  HInstruction* Current() const { return instruction_; }
  void Advance() {
    instruction_ = next_;
    next_ = Done() ? nullptr : instruction_->GetPrevious();
  }

 private:
  HInstruction* instruction_;
  HInstruction* next_;

  DISALLOW_COPY_AND_ASSIGN(HBackwardInstructionIterator);
};

class HInsertionOrderIterator : public ValueObject {
 public:
  explicit HInsertionOrderIterator(const HGraph& graph) : graph_(graph), index_(0) {}

  bool Done() const { return index_ == graph_.GetBlocks().Size(); }
  HBasicBlock* Current() const { return graph_.GetBlocks().Get(index_); }
  void Advance() { ++index_; }

 private:
  const HGraph& graph_;
  size_t index_;

  DISALLOW_COPY_AND_ASSIGN(HInsertionOrderIterator);
};

class HReversePostOrderIterator : public ValueObject {
 public:
  explicit HReversePostOrderIterator(const HGraph& graph) : graph_(graph), index_(0) {}

  bool Done() const { return index_ == graph_.GetReversePostOrder().Size(); }
  HBasicBlock* Current() const { return graph_.GetReversePostOrder().Get(index_); }
  void Advance() { ++index_; }

 private:
  const HGraph& graph_;
  size_t index_;

  DISALLOW_COPY_AND_ASSIGN(HReversePostOrderIterator);
};

class HPostOrderIterator : public ValueObject {
 public:
  explicit HPostOrderIterator(const HGraph& graph)
      : graph_(graph), index_(graph_.GetReversePostOrder().Size()) {}

  bool Done() const { return index_ == 0; }
  HBasicBlock* Current() const { return graph_.GetReversePostOrder().Get(index_ - 1); }
  void Advance() { --index_; }

 private:
  const HGraph& graph_;
  size_t index_;

  DISALLOW_COPY_AND_ASSIGN(HPostOrderIterator);
};
}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_ITERATORS_H_
