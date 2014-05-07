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

#include "base/macros.h"
#include "clean_up_passes.h"
#include "compiler_internals.h"
#include "pass_driver_me_cleanup.h"

namespace art {

/*
 * Create the pass list. These passes are immutable and are shared across the threads.
 *
 * Advantage is that there will be no race conditions here.
 * Disadvantage is the passes can't change their internal states depending on CompilationUnit:
 *   - This is not yet an issue: no current pass would require it.
 */
// The initial list of passes to be used by the PassDriveME.
template<>
const Pass* const PassDriver<PassDriverMECleanUp>::g_passes[] = {
  GetPassInstance<InitializeData>(),
  GetPassInstance<ClearPhiInstructions>(),
  GetPassInstance<CalculatePredecessors>(),
  GetPassInstance<DFSOrders>(),
  GetPassInstance<BuildDomination>(),
  GetPassInstance<DefBlockMatrix>(),
  GetPassInstance<CreatePhiNodes>(),
  GetPassInstance<ClearVisitedFlag>(),
  GetPassInstance<SSAConversion>(),
  GetPassInstance<PhiNodeOperands>(),
  GetPassInstance<ConstantPropagation>(),
  GetPassInstance<PerformInitRegLocations>(),
  GetPassInstance<MethodUseCount>(),
};

// The number of the passes in the initial list of Passes (g_passes).
template<>
uint16_t const PassDriver<PassDriverMECleanUp>::g_passes_size = arraysize(PassDriver<PassDriverMECleanUp>::g_passes);

// The default pass list is used by the PassDriverME instance of PassDriver to initialize pass_list_.
template<>
std::vector<const Pass*> PassDriver<PassDriverMECleanUp>::g_default_pass_list(PassDriver<PassDriverMECleanUp>::g_passes, PassDriver<PassDriverMECleanUp>::g_passes + PassDriver<PassDriverMECleanUp>::g_passes_size);

}  // namespace art
