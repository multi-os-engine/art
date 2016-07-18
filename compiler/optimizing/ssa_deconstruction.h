/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ART_COMPILER_OPTIMIZING_SSA_DECONSTRUCTION_H_
#define ART_COMPILER_OPTIMIZING_SSA_DECONSTRUCTION_H_

#include "primitive.h"

namespace art {

class ArenaAllocator;
class CodeGenerator;
class HBasicBlock;
class HInstruction;
class HParallelMove;
class LiveInterval;
class Location;
class SsaLivenessAnalysis;

/**
 * Deconstructs SSA form after register allocation by resolving nonlinear
 * control flow, connecting live interval siblings, and resolving phi inputs.
 */
class SsaDeconstruction {
 public:
  SsaDeconstruction(ArenaAllocator* allocator,
                    CodeGenerator* codegen,
                    const SsaLivenessAnalysis& liveness,
                    size_t max_safepoint_live_registers);
  void DeconstructSsa();

 private:
  void ResolveNonlinearControlFlow();

  // Insert moves where necessary to resolve phi inputs.
  void ResolvePhiInputs();

  // Connect adjacent siblings within blocks.
  // Uses max_safepoint_live_regs to check that we did not underestimate the
  // number of live registers at safepoints.
  void ConnectSiblings(LiveInterval* interval, size_t max_safepoint_live_regs);

  // Connect siblings between block entries and exits.
  void ConnectSplitSiblings(LiveInterval* interval, HBasicBlock* from, HBasicBlock* to) const;

  // Helper methods for inserting parallel moves in the graph.
  void InsertParallelMoveAtExitOf(HBasicBlock* block,
                                  HInstruction* instruction,
                                  Location source,
                                  Location destination) const;
  void InsertParallelMoveAtEntryOf(HBasicBlock* block,
                                   HInstruction* instruction,
                                   Location source,
                                   Location destination) const;
  void InsertMoveAfter(HInstruction* instruction, Location source, Location destination) const;
  void AddInputMoveFor(HInstruction* input,
                       HInstruction* user,
                       Location source,
                       Location destination) const;
  void InsertParallelMoveAt(size_t position,
                            HInstruction* instruction,
                            Location source,
                            Location destination) const;
  void AddMove(HParallelMove* move,
               Location source,
               Location destination,
               HInstruction* instruction,
               Primitive::Type type) const;

  ArenaAllocator* const allocator_;
  CodeGenerator* const codegen_;
  const SsaLivenessAnalysis& liveness_;
  const size_t max_safepoint_live_registers_;
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_SSA_DECONSTRUCTION_H_
