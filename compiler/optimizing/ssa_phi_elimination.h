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

#ifndef ART_COMPILER_OPTIMIZING_SSA_PHI_ELIMINATION_H_
#define ART_COMPILER_OPTIMIZING_SSA_PHI_ELIMINATION_H_

#include "nodes.h"
#include "optimization.h"

namespace art {

/**
 * Optimization phase that removes dead phis from the graph. Dead phis are unused
 * phis, or phis only used by other phis.
 */
class HSsaDeadPhiElimination : public HOptimization {
 public:
  explicit HSsaDeadPhiElimination(HGraph* graph,
                                  const HGraphVisualizer& visualizer)
      : HOptimization(graph, true, kSsaDeadPhiEliminationPassName, visualizer),
        worklist_(graph->GetArena(), kDefaultWorklistSize) {}

  void Run();

 private:
  GrowableArray<HPhi*> worklist_;

  static constexpr const char* kSsaDeadPhiEliminationPassName =
    "ssa_dead_phi_elimination";
  static constexpr size_t kDefaultWorklistSize = 8;

  DISALLOW_COPY_AND_ASSIGN(HSsaDeadPhiElimination);
};

/**
 * Removes redundant phis that may have been introduced when doing SSA conversion.
 * For example, when entering a loop, we create phis for all live registers. These
 * registers might be updated with the same value, or not updated at all. We can just
 * replace the phi with the value when entering the loop.
 */
class HSsaRedundantPhiElimination : public HOptimization {
 public:
  explicit HSsaRedundantPhiElimination(HGraph* graph,
                                       const HGraphVisualizer& visualizer)
      : HOptimization(graph,
                      true,
                      kSsaRedundantPhiEliminationPassName,
                      visualizer),
        worklist_(graph->GetArena(), kDefaultWorklistSize) {}

  void Run();

 private:
  GrowableArray<HPhi*> worklist_;

  static constexpr const char* kSsaRedundantPhiEliminationPassName =
    "ssa_redundant_phi_elimination";
  static constexpr size_t kDefaultWorklistSize = 8;

  DISALLOW_COPY_AND_ASSIGN(HSsaRedundantPhiElimination);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_SSA_PHI_ELIMINATION_H_
