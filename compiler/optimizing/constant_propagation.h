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

#ifndef ART_COMPILER_OPTIMIZING_CONSTANT_PROPAGATION_H_
#define ART_COMPILER_OPTIMIZING_CONSTANT_PROPAGATION_H_

#include "nodes.h"

namespace art {

#define FOR_EACH_CONCRETE_BINARY_OPERATION(M)   \
  M(Equal)                                      \
  M(NotEqual)                                   \
  M(LessThan)                                   \
  M(LessThanOrEqual)                            \
  M(GreaterThan)                                \
  M(GreaterThanOrEqual)                         \
  M(Compare)                                    \
  M(Add)                                        \
  M(Sub)

/**
 * A visitor statically evaluating binary operations.
 */
class StaticEvaluator : public HGraphVisitor {
 public:
  StaticEvaluator(ArenaAllocator* allocator, HGraph* graph)
    : HGraphVisitor(graph),
      allocator_(allocator) {}

  // Visit functions for binary operation classes.
#define DECLARE_VISIT_INSTRUCTION(name)                         \
  virtual void Visit##name(H##name* instruction) OVERRIDE {     \
    result_ = TryStaticEvaluationDispatch(instruction);         \
  }

  FOR_EACH_CONCRETE_BINARY_OPERATION(DECLARE_VISIT_INSTRUCTION)

#undef DECLARE_VISIT_INSTRUCTION

  HConstant* GetResult() const { return result_; }

 private:
  template <typename O>
  HConstant* TryStaticEvaluationDispatch(O* operation) const {
    HInstruction* left = operation->GetLeft();
    HInstruction* right = operation->GetRight();
    if (left->IsIntConstant() && right->IsIntConstant()) {
      return StaticEvaluation(operation,
                              left->AsIntConstant(),
                              right->AsIntConstant());
    } else if (left->IsLongConstant() && right->IsLongConstant()) {
      return StaticEvaluation(operation,
                              left->AsLongConstant(),
                              right->AsLongConstant());
    }
    return nullptr;
  }

  template <typename O, typename T>
    T* StaticEvaluation(O* operation, T* left, T* right) const {
      return new(allocator_) T(operation->Evaluate(left->GetValue(),
                                                   right->GetValue()));
  }

  // The result of the last evaluation.
  HConstant* result_;

 private:
  ArenaAllocator* const allocator_;

  DISALLOW_COPY_AND_ASSIGN(StaticEvaluator);
};

#undef FOR_EACH_CONCRETE_BINARY_OPERATION


/**
 * Optimization pass performing a simple constant propagation on the
 * SSA form.
 */
class ConstantPropagation : public ValueObject {
 public:
  explicit ConstantPropagation(HGraph* graph)
    : graph_(graph),
      static_evaluator_(graph->GetArena(), graph) {}

  void Run();

 private:
  HGraph* const graph_;
  StaticEvaluator static_evaluator_;

  DISALLOW_COPY_AND_ASSIGN(ConstantPropagation);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_CONSTANT_PROPAGATION_H_
