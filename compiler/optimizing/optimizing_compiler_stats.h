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

namespace art {

enum MethodCompilationStat {
  kAttemptCompilation = 0,
  kCompiledBaseline,
  kCompileOptimized,
  kNotCompiledUnsupportedIsa,
  kNotCompiledPathologicalCase,
  kNotCompiledHugeMethod,
  kNotCompiledLargeMethodNoBranches,
  kNotCompiledCannotBuildSSA,
  kNotCompiledNoCode,
  kNotCompiledUnresolvedMethod,
  kNotCompiledUnresolvedField,
  kNotCompiledNonSequentialRegPair,
  kNotCompiledVolatile,
  kNotCompiledDisabled,
  kNotOptimizedTryCatch,
  kNotOptimizedDisabled,
  kNotCompiledCantAccesType,
  kNotOptimizedRegisterAllocator,
  kNotCompiledUnhandledInstruction,
  kLastStat
};

class OptimizingCompilerStats {
 public:
  OptimizingCompilerStats() {}

  void RecordStat(int stat) {
    compile_stats_[stat]++;
  }

  void Log() const {
    if (compile_stats_[kAttemptCompilation] == 0) {
      LOG(INFO) << "Did not compile any method.";
    } else {
      size_t unoptimized_percent =
          compile_stats_[kCompiledBaseline] * 100 / compile_stats_[kAttemptCompilation];
      size_t optimized_percent =
          compile_stats_[kCompileOptimized] * 100 / compile_stats_[kAttemptCompilation];
      std::ostringstream oss;
      oss << "Attempt compilation of " << compile_stats_[kAttemptCompilation] << " methods: "
          << unoptimized_percent << "% (" << compile_stats_[kCompiledBaseline] << ") unoptimized, "
          << optimized_percent << "% (" << compile_stats_[kCompileOptimized] << ") optimized.\n";
      for (int i = 0; i < kLastStat; i++) {
        if (compile_stats_[i] != 0) {
          oss << PrintMethodCompilationStat(i) << ": " << compile_stats_[i] << "\n";
        }
      }
      LOG(INFO) << oss.str();
    }
  }

 private:
  std::string PrintMethodCompilationStat(int stat) const {
    switch (stat) {
      case kAttemptCompilation : return "kAttemptCompilation";
      case kCompiledBaseline : return "kCompiledBaseline";
      case kCompileOptimized : return "kCompileOptimized";
      case kNotCompiledUnsupportedIsa : return "kNotCompiledUnsupportedIsa";
      case kNotCompiledPathologicalCase : return "kNotCompiledPathologicalCase";
      case kNotCompiledHugeMethod : return "kNotCompiledHugeMethod";
      case kNotCompiledLargeMethodNoBranches : return "kNotCompiledLargeMethodNoBranches";
      case kNotCompiledCannotBuildSSA : return "kNotCompiledCannotBuildSSA";
      case kNotCompiledNoCode : return "kNotCompiledNoCode";
      case kNotCompiledUnresolvedMethod : return "kNotCompiledUnresolvedMethod";
      case kNotCompiledUnresolvedField : return "kNotCompiledUnresolvedField";
      case kNotCompiledNonSequentialRegPair : return "kNotCompiledNonSequentialRegPair";
      case kNotCompiledVolatile : return "kNotCompiledVolatile";
      case kNotCompiledDisabled : return "kNotCompiledDisabled";
      case kNotOptimizedDisabled : return "kNotOptimizedDisabled";
      case kNotOptimizedTryCatch : return "kNotOptimizedTryCatch";
      case kNotCompiledCantAccesType : return "kNotCompiledCantAccesType";
      case kNotOptimizedRegisterAllocator : return "kNotOptimizedRegisterAllocator";
      case kNotCompiledUnhandledInstruction : return "kNotCompiledUnhandledInstruction";
      default: LOG(FATAL) << "invalid stat";
    }
    return "";
  }

  AtomicInteger compile_stats_[kLastStat];

  DISALLOW_COPY_AND_ASSIGN(OptimizingCompilerStats);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_OPTIMIZING_COMPILER_STATS_H_
