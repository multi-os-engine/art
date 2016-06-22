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

#ifndef ART_COMPILER_OPTIMIZING_REGISTER_ALLOCATOR_COMMON_H_
#define ART_COMPILER_OPTIMIZING_REGISTER_ALLOCATOR_COMMON_H_

#include "base/arena_containers.h"
#include "base/macros.h"
#include "primitive.h"

namespace art {

class CodeGenerator;
class LiveInterval;
class SsaLivenessAnalysis;
class HGraph;
class HInstruction;

/**
 * Base class for any register allocator.
 */
class RegisterAllocatorCommon {
 public:
  RegisterAllocatorCommon(ArenaAllocator* allocator,
                          CodeGenerator* codegen,
                          const SsaLivenessAnalysis& analysis);

  virtual ~RegisterAllocatorCommon() = default;

  // Main entry point for the register allocator. Given the liveness analysis,
  // allocates registers to live intervals.
  virtual void AllocateRegisters() = 0;

  // Validate that the register allocator did not allocate the same register to
  // intervals that intersect each other. Returns false if it did not.
  virtual bool Validate(bool log_fatal_on_failure) = 0;

  static bool CanAllocateRegistersFor(const HGraph& graph,
                                      InstructionSet instruction_set);

  // Verifies that live intervals do not conflict. Used by unit testing.
  static bool ValidateIntervals(const ArenaVector<LiveInterval*>& intervals,
                                size_t number_of_spill_slots,
                                size_t number_of_out_slots,
                                const CodeGenerator& codegen,
                                ArenaAllocator* allocator,
                                bool processing_core_registers,
                                bool log_fatal_on_failure);

  static constexpr const char* kRegisterAllocatorPassName = "register";

 protected:
  ArenaAllocator* const allocator_;
  CodeGenerator* const codegen_;
  const SsaLivenessAnalysis& liveness_;
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_REGISTER_ALLOCATOR_COMMON_H_
