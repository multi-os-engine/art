/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_SRC_COMPILER_DEX_COMPILER_H_
#define ART_SRC_COMPILER_DEX_COMPILER_H_

#include "dex_file.h"
#include "dex_instruction.h"

namespace llvm {
  class Module;
  class LLVMContext;
}  // namespace llvm

namespace art {
namespace Llvm {
  class DexIRBuilder;
} // namespace Llvm

/*
 * Assembly is an iterative process, and usually terminates within
 * two or three passes.  This should be high enough to handle bizarre
 * cases, but detect an infinite loop bug.
 */
#define MAX_ASSEMBLER_RETRIES 50

// Suppress optimization if corresponding bit set.
enum opt_control_vector {
  kLoadStoreElimination = 0,
  kLoadHoisting,
  kSuppressLoads,
  kNullCheckElimination,
  kPromoteRegs,
  kTrackLiveTemps,
  kSafeOptimizations,
  kBBOpt,
  kMatch,
  kPromoteCompilerTemps,
  kBranchFusing,
};

// Force code generation paths for testing.
enum debugControlVector {
  kDebugVerbose,
  kDebugDumpCFG,
  kDebugSlowFieldPath,
  kDebugSlowInvokePath,
  kDebugSlowStringPath,
  kDebugSlowTypePath,
  kDebugSlowestFieldPath,
  kDebugSlowestStringPath,
  kDebugExerciseResolveMethod,
  kDebugVerifyDataflow,
  kDebugShowMemoryUsage,
  kDebugShowNops,
  kDebugCountOpcodes,
  kDebugDumpCheckStats,
  kDebugDumpBitcodeFile,
  kDebugVerifyBitcode,
  kDebugShowSummaryMemoryUsage,
};

struct CompilationUnit;
struct BasicBlock;

}  // namespace art

#endif // ART_SRC_COMPILER_DEX_COMPILER_H_
