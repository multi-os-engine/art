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

#ifndef ART_COMPILER_OPTIMIZING_OPTIMIZATION_FACTORY_H_
#define ART_COMPILER_OPTIMIZING_OPTIMIZATION_FACTORY_H_

#include <map>
#include <string>
#include <vector>

#include "code_generator.h"
#include "compiler.h"
#include "optimization.h"
#include "induction_var_analysis.h"
#include "side_effects_analysis.h"

namespace art {

/**
 * This is a factory class for constructing a list of HOptimization objects from a list
 * of strings - names of the optimizations. It is constructed from objects describing
 * optimizations environment which are then passed to optimizations constructors.
 *
 * Some HOptimizations require SideEffectsAnalysis or HInductionVarAnalysis instances. This
 * class assumes that they expect the closest instance preceeding it in the pass name list.
 */
class HOptimizationFactory {
 public:
  HOptimizationFactory(ArenaAllocator* arena,
                       HGraph* graph,
                       OptimizingCompilerStats* stats,
                       CodeGenerator* codegen,
                       CompilerDriver* driver,
                       const DexCompilationUnit& dex_compilation_unit,
                       StackHandleScopeCollection* handles);

  // Returns vector of constructed HOptimization* corresponding to pass_names
  std::vector<HOptimization*> BuildOptimizations(const std::vector<std::string>& pass_names);

 private:
  // Constructs a single optimization corresponding to pass_name
  HOptimization* BuildOptimization(std::string& pass_name,
                                   SideEffectsAnalysis* most_recent_side_effects,
                                   HInductionVarAnalysis* most_recent_induction);

  ArenaAllocator* arena_;
  HGraph* graph_;
  OptimizingCompilerStats* stats_;
  CodeGenerator* codegen_;
  CompilerDriver* driver_;
  const DexCompilationUnit& dex_compilation_unit_;
  StackHandleScopeCollection* handles_;
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_OPTIMIZATION_FACTORY_H_
