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

#ifndef ART_COMPILER_OPTIMIZING_INSTRUCTION_SIMPLIFIER_H_
#define ART_COMPILER_OPTIMIZING_INSTRUCTION_SIMPLIFIER_H_

#include "nodes.h"
#include "optimization.h"

namespace art {

/**
 * Implements optimizations specific to each instruction.
 */
class HInstructionSimplifier : public HOptimization {
 public:
  explicit HInstructionSimplifier(HGraph* graph,
                                  const HGraphVisualizer& visualizer)
      : HOptimization(graph,
                      true,
                      kInstructionSimplificationPassName,
                      visualizer) {}

  void Run();

 private:
  virtual void VisitSuspendCheck(HSuspendCheck* check) OVERRIDE;
  virtual void VisitEqual(HEqual* equal) OVERRIDE;

  static constexpr const char* kInstructionSimplificationPassName =
    "instruction_simplification";
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_INSTRUCTION_SIMPLIFIER_H_
