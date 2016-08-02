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
  class AbstractFactory;

  class HOptimizationFactory {
   public:
    HOptimizationFactory(ArenaAllocator* arena,
                         HGraph* graph,
                         OptimizingCompilerStats* stats,
                         CodeGenerator* codegen,
                         CompilerDriver* driver,
                         const DexCompilationUnit& dex_compilation_unit,
                         StackHandleScopeCollection* handles);

    std::vector<HOptimization*> BuildOptimizations(const std::vector<std::string>& pass_names);

   private:
    HOptimization* BuildOptimization(std::string& pass_name);

    ArenaAllocator* arena_;
    HGraph* graph_;
    OptimizingCompilerStats* stats_;
    CodeGenerator* codegen_;
    CompilerDriver* driver_;
    const DexCompilationUnit& dex_compilation_unit_;
    StackHandleScopeCollection* handles_;
    SideEffectsAnalysis* most_recent_side_effects_;
    HInductionVarAnalysis* most_recent_induction_;
  };

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_OPTIMIZATION_FACTORY_H_
