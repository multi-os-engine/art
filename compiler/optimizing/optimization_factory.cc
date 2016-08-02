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

#include "optimization_factory.h"

#include <string>

#include "bounds_check_elimination.h"
#include "constant_folding.h"
#include "dead_code_elimination.h"
#include "dex_cache_array_fixups_arm.h"
#include "dex_cache_array_fixups_mips.h"
#include "driver/dex_compilation_unit.h"
#include "gvn.h"
#include "induction_var_analysis.h"
#include "inliner.h"
#include "instruction_simplifier_arm.h"
#include "instruction_simplifier_arm64.h"
#include "instruction_simplifier.h"
#include "intrinsics.h"
#include "licm.h"
#include "load_store_elimination.h"
#include "pc_relative_fixups_mips.h"
#include "pc_relative_fixups_x86.h"
#include "sharpening.h"
#include "select_generator.h"
#include "side_effects_analysis.h"
#include "x86_memory_gen.h"

namespace art {

  HOptimizationFactory::HOptimizationFactory(
    ArenaAllocator* arena,
    HGraph* graph,
    OptimizingCompilerStats* stats,
    CodeGenerator* codegen,
    CompilerDriver* driver,
    const DexCompilationUnit& dex_compilation_unit,
    StackHandleScopeCollection* handles)
      : arena_(arena),
        graph_(graph),
        stats_(stats),
        codegen_(codegen),
        driver_(driver),
        dex_compilation_unit_(dex_compilation_unit),
        handles_(handles),
        most_recent_side_effects_(nullptr),
        most_recent_induction_(nullptr) { }

  static std::string ExtractOptimizationName(std::string pass_name) {
    size_t pos = pass_name.find("::");
    if (pos == std::string::npos) {
      return pass_name;
    }
    return pass_name.substr(0, pos);
  }

  HOptimization* HOptimizationFactory::BuildOptimization(std::string& pass_name) {
    std::string opt_name = ExtractOptimizationName(pass_name);

    if (opt_name == arm::InstructionSimplifierArm::kInstructionSimplifierArmPassName) {
      return new (arena_) arm::InstructionSimplifierArm(graph_, stats_);
    }
    if (opt_name == arm64::InstructionSimplifierArm64::kInstructionSimplifierArm64PassName) {
      return new (arena_) arm64::InstructionSimplifierArm64(graph_, stats_);
    }
    if (opt_name == BoundsCheckElimination::kBoundsCheckEliminationPassName) {
      return new (arena_) BoundsCheckElimination(
        graph_, *most_recent_side_effects_, most_recent_induction_);
    }
    if (opt_name == GVNOptimization::kGlobalValueNumberingPassName) {
      return new (arena_) GVNOptimization(graph_, *most_recent_side_effects_);
    }
    if (opt_name == HConstantFolding::kConstantFoldingPassName) {
      return new (arena_) HConstantFolding(graph_);
    }
    if (opt_name == HDeadCodeElimination::kDeadCodeEliminationPassName) {
      return new (arena_) HDeadCodeElimination(graph_, stats_);
    }
    if (opt_name == HInliner::kInlinerPassName) {
      size_t number_of_dex_registers = dex_compilation_unit_.GetCodeItem()->registers_size_;
      return new (arena_) HInliner(graph_,
                                   graph_,
                                   codegen_,
                                   dex_compilation_unit_,
                                   dex_compilation_unit_,
                                   driver_,
                                   handles_,
                                   stats_,
                                   number_of_dex_registers,
                                   0);
    }
    if (opt_name == HSharpening::kSharpeningPassName) {
        return new (arena_) HSharpening(graph_, codegen_, dex_compilation_unit_, driver_);
    }
    if (opt_name == HSelectGenerator::kSelectGeneratorPassName) {
      return new (arena_) HSelectGenerator(graph_, stats_);
    }
    if (opt_name == HInductionVarAnalysis::kInductionPassName) {
      return new (arena_) HInductionVarAnalysis(graph_);
    }
    if (opt_name == InstructionSimplifier::kInstructionSimplifierPassName) {
      return new (arena_) InstructionSimplifier(graph_, stats_);
    }
    if (opt_name == IntrinsicsRecognizer::kIntrinsicsRecognizerPassName) {
      return new (arena_) IntrinsicsRecognizer(
        graph_, driver_, stats_);
    }
    if (opt_name == LICM::kLoopInvariantCodeMotionPassName) {
      return new (arena_) LICM(graph_, *most_recent_side_effects_, stats_);
    }
    if (opt_name == LoadStoreElimination::kLoadStoreEliminationPassName) {
      return new (arena_) LoadStoreElimination(graph_, *most_recent_side_effects_);
    }
    if (opt_name == mips::DexCacheArrayFixups::kDexCacheArrayFixupsMipsPassName) {
      return new (arena_) mips::DexCacheArrayFixups(graph_, stats_);
    }
    if (opt_name == mips::PcRelativeFixups::kPcRelativeFixupsMipsPassName) {
      return new (arena_) mips::PcRelativeFixups(graph_, codegen_, stats_);
    }
    if (opt_name == SideEffectsAnalysis::kSideEffectsAnalysisPassName) {
      return new (arena_) SideEffectsAnalysis(graph_);
    }
    if (opt_name == x86::PcRelativeFixups::kPcRelativeFixupsX86PassName) {
      return new (arena_) x86::PcRelativeFixups(graph_, codegen_, stats_);
    }
    if (opt_name == x86::X86MemoryOperandGeneration::kX86MemoryOperandGenerationPassName) {
      return new (arena_) x86::X86MemoryOperandGeneration(graph_, codegen_, stats_);
    }
    return nullptr;
  }

  std::vector<HOptimization*> HOptimizationFactory::BuildOptimizations(
    const std::vector<std::string>& pass_names) {
    std::vector<HOptimization*> ret;
    for (std::string name : pass_names) {
      HOptimization* opt = BuildOptimization(name);
      CHECK(opt != nullptr) << "Couldn't build optimization: \"" << name << "\"";
      ret.push_back(opt);

      // Some HOptimizations require SideEffectsAnalysis or HInductionVarAnalysis instances.
      // We assume that they expect the most recent analysis results available to them.
      if (name == SideEffectsAnalysis::kSideEffectsAnalysisPassName) {
        most_recent_side_effects_ = down_cast<SideEffectsAnalysis*>(opt);
      }
      if (name == HInductionVarAnalysis::kInductionPassName) {
        most_recent_induction_ = down_cast<HInductionVarAnalysis*>(opt);
      }
    }
    return ret;
  }

}  // namespace art
