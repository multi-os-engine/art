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

#ifndef ART_COMPILER_OPTIMIZING_OPTIMIZATION_H_
#define ART_COMPILER_OPTIMIZING_OPTIMIZATION_H_

#include "graph_visualizer.h"
#include "nodes.h"

namespace art {

/**
 * Abstraction to implement an optimization pass.
 */
class Optimization : public ValueObject {
 public:
  Optimization(HGraph* graph, bool is_ssa_form, const char* pass_name,
               HGraphVisualizer* visualizer = nullptr)
      : graph_(graph),
        is_ssa_form_(is_ssa_form),
        pass_name_(pass_name),
        visualizer_(visualizer) {}

  virtual ~Optimization() {}

  // Execute the optimization pass.
  void Execute();

  // Peform the analysis itself.
  virtual void Run() = 0;

  // Verify the graph using `checker`; abort if it is not valid.
  template <typename T>
  void Check(T* checker) {
    checker->VisitInsertionOrder();
    if (!checker->IsValid()) {
      LOG(FATAL) << Dumpable<T>(*checker);
    }
  }

 protected:
  HGraph* graph_;

 private:
  // Does the analyzed graph use SSA form?
  bool is_ssa_form_;
  // Optimization pass name.
  const char* pass_name_;
  // A graph visualiser invoked during the execution of the
  // optimization pass if non null.
  HGraphVisualizer* visualizer_;

  DISALLOW_COPY_AND_ASSIGN(Optimization);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_OPTIMIZATION_H_
