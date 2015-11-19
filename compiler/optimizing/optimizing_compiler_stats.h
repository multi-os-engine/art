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
  kCompiledBaseline,
  kCompiledOptimized,
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
      float baseline_percent =
          compile_stats_[kCompiledBaseline] * 100.0 / compile_stats_[kAttemptCompilation];
      float optimized_percent =
          compile_stats_[kCompiledOptimized] * 100.0 / compile_stats_[kAttemptCompilation];
      LOG(INFO) << "Attempted compilation of " << compile_stats_[kAttemptCompilation] << " methods: "
          << std::fixed << std::setprecision(2)
          << baseline_percent << "% (" << compile_stats_[kCompiledBaseline] << ") baseline, "
          << optimized_percent << "% (" << compile_stats_[kCompiledOptimized] << ") optimized, ";

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
      case kAttemptCompilation : return "MCS#AttemptCompilation";
      case kCompiledBaseline : return "MCS#CompiledBaseline";
      case kCompiledOptimized : return "MCS#CompiledOptimized";
      case kInlinedInvoke : return "MCS#InlinedInvoke";
      case kInstructionSimplifications: return "MCS#InstructionSimplifications";
      case kInstructionSimplificationsArch: return "MCS#InstructionSimplificationsArch";
      case kUnresolvedMethod : return "MCS#UnresolvedMethod";
      case kUnresolvedField : return "MCS#UnresolvedField";
      case kUnresolvedFieldNotAFastAccess : return "MCS#UnresolvedFieldNotAFastAccess";
      case kRemovedCheckedCast: return "MCS#RemovedCheckedCast";
      case kRemovedDeadInstruction: return "MCS#RemovedDeadInstruction";
      case kRemovedNullCheck: return "MCS#RemovedNullCheck";
      case kNotCompiledBranchOutsideMethodCode: return "MCS#NotCompiledBranchOutsideMethodCode";
      case kNotCompiledCannotBuildSSA : return "MCS#NotCompiledCannotBuildSSA";
      case kNotCompiledHugeMethod : return "MCS#NotCompiledHugeMethod";
      case kNotCompiledLargeMethodNoBranches : return "MCS#NotCompiledLargeMethodNoBranches";
      case kNotCompiledMalformedOpcode : return "MCS#NotCompiledMalformedOpcode";
      case kNotCompiledNoCodegen : return "MCS#NotCompiledNoCodegen";
      case kNotCompiledPathological : return "MCS#NotCompiledPathological";
      case kNotCompiledSpaceFilter : return "MCS#NotCompiledSpaceFilter";
      case kNotCompiledUnhandledInstruction : return "MCS#NotCompiledUnhandledInstruction";
      case kNotCompiledUnsupportedIsa : return "MCS#NotCompiledUnsupportedIsa";
      case kNotCompiledVerificationError : return "MCS#NotCompiledVerificationError";
      case kNotCompiledVerifyAtRuntime : return "MCS#NotCompiledVerifyAtRuntime";

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
