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

#ifndef ART_COMPILER_OPTIMIZING_OPTIMIZING_COMPILER_STATS_H_
#define ART_COMPILER_OPTIMIZING_OPTIMIZING_COMPILER_STATS_H_

#include <iomanip>
#include <string>
#include <type_traits>

#include "atomic.h"

namespace art {

enum MethodCompilationStat {
  kAttemptCompilation = 0,
  kCompiled,
  kInlinedInvoke,
  kInstructionSimplifications,
  kInstructionSimplificationsArch,
  kUnresolvedMethod,
  kUnresolvedField,
  kUnresolvedFieldNotAFastAccess,
  kRemovedCheckedCast,
  kRemovedDeadInstruction,
  kRemovedNullCheck,
  kNotCompiledBranchOutsideMethodCode,
  kNotCompiledCannotBuildSSA,
  kNotCompiledHugeMethod,
  kNotCompiledLargeMethodNoBranches,
  kNotCompiledMalformedOpcode,
  kNotCompiledNoCodegen,
  kNotCompiledPathological,
  kNotCompiledSpaceFilter,
  kNotCompiledUnhandledInstruction,
  kNotCompiledUnsupportedIsa,
  kNotCompiledVerificationError,
  kNotCompiledVerifyAtRuntime,
  kLastStat
};

class OptimizingCompilerStats {
 public:
  OptimizingCompilerStats() {}

  void RecordStat(MethodCompilationStat stat, size_t count = 1) {
    compile_stats_[stat] += count;
  }

  void Log() const {
    if (!kIsDebugBuild && !VLOG_IS_ON(compiler)) {
      // Don't log anything if release builds or if the compiler is not verbose.
      return;
    }

    if (compile_stats_[kAttemptCompilation] == 0) {
      LOG(INFO) << "Did not compile any method.";
    } else {
      float compiled_percent =
          compile_stats_[kCompiled] * 100.0 / compile_stats_[kAttemptCompilation];
      LOG(INFO) << "Attempted compilation of " << compile_stats_[kAttemptCompilation] << " methods: "
          << std::fixed << std::setprecision(2)
          << compiled_percent << "% (" << compile_stats_[kCompiled] << ") compiled.";

      for (int i = 0; i < kLastStat; i++) {
        if (compile_stats_[i] != 0) {
          LOG(INFO) << PrintMethodCompilationStat(static_cast<MethodCompilationStat>(i)) << ": "
              << compile_stats_[i];
        }
      }
    }
  }

 private:
  std::string PrintMethodCompilationStat(MethodCompilationStat stat) const {
    switch (stat) {
      // `mcs` stands for Method Compilation Stat.
      case kAttemptCompilation : return "mcs#AttemptCompilation";
      case kCompiled : return "mcs#Compiled";
      case kInlinedInvoke : return "mcs#InlinedInvoke";
      case kInstructionSimplifications: return "mcs#InstructionSimplifications";
      case kInstructionSimplificationsArch: return "mcs#InstructionSimplificationsArch";
      case kUnresolvedMethod : return "mcs#UnresolvedMethod";
      case kUnresolvedField : return "mcs#UnresolvedField";
      case kUnresolvedFieldNotAFastAccess : return "mcs#UnresolvedFieldNotAFastAccess";
      case kRemovedCheckedCast: return "mcs#RemovedCheckedCast";
      case kRemovedDeadInstruction: return "mcs#RemovedDeadInstruction";
      case kRemovedNullCheck: return "mcs#RemovedNullCheck";
      case kNotCompiledBranchOutsideMethodCode: return "mcs#NotCompiledBranchOutsideMethodCode";
      case kNotCompiledCannotBuildSSA : return "mcs#NotCompiledCannotBuildSSA";
      case kNotCompiledHugeMethod : return "mcs#NotCompiledHugeMethod";
      case kNotCompiledLargeMethodNoBranches : return "mcs#NotCompiledLargeMethodNoBranches";
      case kNotCompiledMalformedOpcode : return "mcs#NotCompiledMalformedOpcode";
      case kNotCompiledNoCodegen : return "mcs#NotCompiledNoCodegen";
      case kNotCompiledPathological : return "mcs#NotCompiledPathological";
      case kNotCompiledSpaceFilter : return "mcs#NotCompiledSpaceFilter";
      case kNotCompiledUnhandledInstruction : return "mcs#NotCompiledUnhandledInstruction";
      case kNotCompiledUnsupportedIsa : return "mcs#NotCompiledUnsupportedIsa";
      case kNotCompiledVerificationError : return "mcs#NotCompiledVerificationError";
      case kNotCompiledVerifyAtRuntime : return "mcs#NotCompiledVerifyAtRuntime";

      case kLastStat: break;  // Invalid to print out.
    }
    LOG(FATAL) << "invalid stat "
        << static_cast<std::underlying_type<MethodCompilationStat>::type>(stat);
    UNREACHABLE();
  }

  AtomicInteger compile_stats_[kLastStat];

  DISALLOW_COPY_AND_ASSIGN(OptimizingCompilerStats);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_OPTIMIZING_COMPILER_STATS_H_
