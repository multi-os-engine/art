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

#define REGISTER_TEMPLATE_FACTORY(OPTIMIZATION, NAME, FACTORY, FACTORY_ARGS...) \
  {std::string(OPTIMIZATION::NAME), new FACTORY<OPTIMIZATION>(FACTORY_ARGS)}
#define REGISTER_FACTORY(OPTIMIZATION, NAME, FACTORY, FACTORY_ARGS...) \
  {std::string(OPTIMIZATION::NAME), new FACTORY(FACTORY_ARGS)}

namespace art {

  class AbstractFactory {
   public:
    virtual ~AbstractFactory() { }
    virtual HOptimization* Build(ArenaAllocator* arena, HGraph* graph);
  };

  template <typename T>
  class PassGraphFactory : public AbstractFactory {
   public:
    T* Build(ArenaAllocator* arena, HGraph* graph) OVERRIDE {
      return new (arena) T(graph);
    }
  };

  template <typename T>
  class PassGraphStatsFactory : public AbstractFactory {
   public:
    explicit PassGraphStatsFactory(OptimizingCompilerStats* stats)
      : stats_(stats) { }
    T* Build(ArenaAllocator* arena, HGraph* graph) OVERRIDE {
      return new (arena) T(graph, stats_);
    }

   private:
    OptimizingCompilerStats* stats_;
  };

  template <typename T>
  class PassGraphCodegenStatsFactory : public AbstractFactory {
   public:
    PassGraphCodegenStatsFactory(CodeGenerator* codegen, OptimizingCompilerStats* stats)
      : codegen_(codegen), stats_(stats) { }
    T* Build(ArenaAllocator* arena, HGraph* graph) OVERRIDE {
      return new (arena) T(graph, codegen_, stats_);
    }

   private:
    CodeGenerator* codegen_;
    OptimizingCompilerStats* stats_;
  };

  template <typename T>
  class PassGraphSideEffectsFactory : public AbstractFactory {
   public:
    explicit PassGraphSideEffectsFactory(SideEffectsAnalysis*& side_effects)
      : side_effects_(side_effects) { }
    T* Build(ArenaAllocator* arena, HGraph* graph) OVERRIDE {
      return new (arena) T(graph, *side_effects_);
    }

   private:
    SideEffectsAnalysis*& side_effects_;
  };

  class LICMFactory : public AbstractFactory {
   public:
    LICMFactory(SideEffectsAnalysis*& side_effects,
                OptimizingCompilerStats* stats)
      : side_effects_(side_effects), stats_(stats) { }
    LICM* Build(ArenaAllocator* arena, HGraph* graph) OVERRIDE {
      return new (arena) LICM(graph, *side_effects_, stats_);
    }

   private:
    SideEffectsAnalysis*& side_effects_;
    OptimizingCompilerStats* stats_;
  };

  class BoundsCheckEliminationFactory : public AbstractFactory {
   public:
    BoundsCheckEliminationFactory(SideEffectsAnalysis*& side_effects,
                                  HInductionVarAnalysis* induction)
      : side_effects_(side_effects), induction_(induction) { }
    BoundsCheckElimination* Build(ArenaAllocator* arena, HGraph* graph) OVERRIDE {
      return new (arena) BoundsCheckElimination(graph, *side_effects_, induction_);
    }

   private:
    SideEffectsAnalysis*& side_effects_;
    HInductionVarAnalysis* induction_;
  };

  class HSharpeningFactory : public AbstractFactory {
   public:
    HSharpeningFactory(CodeGenerator* codegen,
                       const DexCompilationUnit& dex_compilation_unit,
                       CompilerDriver* driver)
      : codegen_(codegen), dex_compilation_unit_(dex_compilation_unit), driver_(driver) { }
    HSharpening* Build(ArenaAllocator* arena, HGraph* graph) OVERRIDE {
      return new (arena) HSharpening(graph, codegen_, dex_compilation_unit_, driver_);
    }

   private:
    CodeGenerator* codegen_;
    const DexCompilationUnit& dex_compilation_unit_;
    CompilerDriver* driver_;
  };

  class HInlinerFactory : public AbstractFactory {
   public:
    HInlinerFactory(CodeGenerator* codegen,
                    const DexCompilationUnit& dex_compilation_unit,
                    CompilerDriver* driver,
                    StackHandleScopeCollection* handles,
                    OptimizingCompilerStats* stats)
      : codegen_(codegen),
        dex_compilation_unit_(dex_compilation_unit),
        driver_(driver),
        handles_(handles),
        stats_(stats) { }
    HInliner* Build(ArenaAllocator* arena, HGraph* graph) OVERRIDE {
      return new (arena) HInliner(graph,
                                  graph,
                                  codegen_,
                                  dex_compilation_unit_,
                                  dex_compilation_unit_,
                                  driver_,
                                  handles_,
                                  stats_,
                                  dex_compilation_unit_.GetCodeItem()->registers_size_,
                                  0);
      }

   private:
    CodeGenerator* codegen_;
    const DexCompilationUnit& dex_compilation_unit_;
    CompilerDriver* driver_;
    StackHandleScopeCollection* handles_;
    OptimizingCompilerStats* stats_;
  };

  class IntrinsicsRecognizerFactory : public AbstractFactory {
   public:
    IntrinsicsRecognizerFactory(CompilerDriver* driver,
                                OptimizingCompilerStats* stats)
      : driver_(driver), stats_(stats) { }
    IntrinsicsRecognizer* Build(ArenaAllocator* arena, HGraph* graph) OVERRIDE {
      return new (arena) IntrinsicsRecognizer(graph, driver_, stats_);
    }

   private:
    CompilerDriver* driver_;
    OptimizingCompilerStats* stats_;
  };

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
        side_effects_(nullptr),
        induction_(nullptr),
        factory_map_({
          REGISTER_TEMPLATE_FACTORY(arm::InstructionSimplifierArm, \
                                    kInstructionSimplifierArmPassName, \
                                    PassGraphStatsFactory, \
                                    stats),
          REGISTER_TEMPLATE_FACTORY(arm::DexCacheArrayFixups, \
                                    kDexCacheArrayFixupsArmPassName, \
                                    PassGraphStatsFactory, \
                                    stats),
          REGISTER_TEMPLATE_FACTORY(arm64::InstructionSimplifierArm64, \
                                    kInstructionSimplifierArm64PassName, \
                                    PassGraphStatsFactory, \
                                    stats),
          REGISTER_TEMPLATE_FACTORY(HSelectGenerator, \
                                    kSelectGeneratorPassName, \
                                    PassGraphStatsFactory, \
                                    stats),
          REGISTER_TEMPLATE_FACTORY(InstructionSimplifier, \
                                    kInstructionSimplifierPassName, \
                                    PassGraphStatsFactory, \
                                    stats),
          REGISTER_TEMPLATE_FACTORY(InstructionSimplifier, \
                                    kInstructionSimplifierAfterBCEPassName, \
                                    PassGraphStatsFactory, \
                                    stats),
          REGISTER_TEMPLATE_FACTORY(InstructionSimplifier, \
                                    kInstructionSimplifierBeforeCodegenPassName, \
                                    PassGraphStatsFactory, \
                                    stats),
          REGISTER_TEMPLATE_FACTORY(HDeadCodeElimination, \
                                    kInitialDeadCodeEliminationPassName, \
                                    PassGraphStatsFactory, \
                                    stats),
          REGISTER_TEMPLATE_FACTORY(HDeadCodeElimination, \
                                    kFinalDeadCodeEliminationPassName, \
                                    PassGraphStatsFactory, \
                                    stats),
          REGISTER_TEMPLATE_FACTORY(mips::DexCacheArrayFixups, \
                                    kDexCacheArrayFixupsMipsPassName, \
                                    PassGraphStatsFactory, \
                                    stats),

          REGISTER_TEMPLATE_FACTORY(HInductionVarAnalysis, kInductionPassName, PassGraphFactory),
          REGISTER_TEMPLATE_FACTORY(SideEffectsAnalysis, \
                                    kSideEffectsAnalysisPassName, \
                                    PassGraphFactory),
          REGISTER_TEMPLATE_FACTORY(HConstantFolding, \
                                    kConstantFoldingPassName, \
                                    PassGraphFactory),
          REGISTER_TEMPLATE_FACTORY(HConstantFolding, \
                                    kConstantFoldingAfterInliningPassName, \
                                    PassGraphFactory),
          REGISTER_TEMPLATE_FACTORY(HConstantFolding, \
                                    kConstantFoldingAfterBCEPassName, \
                                    PassGraphFactory),

          REGISTER_TEMPLATE_FACTORY(x86::X86MemoryOperandGeneration, \
                                    kX86MemoryOperandGenerationPassName, \
                                    PassGraphCodegenStatsFactory, \
                                    codegen, \
                                    stats),
          REGISTER_TEMPLATE_FACTORY(x86::PcRelativeFixups, \
                                    kPcRelativeFixupsX86PassName, \
                                    PassGraphCodegenStatsFactory, \
                                    codegen, \
                                    stats),
          REGISTER_TEMPLATE_FACTORY(mips::PcRelativeFixups, \
                                    kPcRelativeFixupsMipsPassName, \
                                    PassGraphCodegenStatsFactory, \
                                    codegen, \
                                    stats),

          REGISTER_TEMPLATE_FACTORY(LoadStoreElimination, \
                                    kLoadStoreEliminationPassName, \
                                    PassGraphSideEffectsFactory, \
                                    side_effects_),
          REGISTER_TEMPLATE_FACTORY(GVNOptimization, \
                                    kGlobalValueNumberingPassName, \
                                    PassGraphSideEffectsFactory, \
                                    side_effects_),
          REGISTER_TEMPLATE_FACTORY(GVNOptimization, \
                                    kGlobalValueNumberingAfterArchPassName, \
                                    PassGraphSideEffectsFactory, \
                                    side_effects_),

          REGISTER_FACTORY(IntrinsicsRecognizer, \
                           kIntrinsicsRecognizerPassName, \
                           IntrinsicsRecognizerFactory, \
                           driver, \
                           stats),
          REGISTER_FACTORY(LICM, \
                           kLoopInvariantCodeMotionPassName, \
                           LICMFactory, \
                           side_effects_, \
                           stats),
          REGISTER_FACTORY(BoundsCheckElimination, \
                           kBoundsCheckEliminationPassName, \
                           BoundsCheckEliminationFactory, \
                           side_effects_, \
                           induction_),
          REGISTER_FACTORY(HSharpening, \
                           kSharpeningPassName, \
                           HSharpeningFactory, \
                           codegen, \
                           dex_compilation_unit, \
                           driver),
          REGISTER_FACTORY(HInliner, \
                           kInlinerPassName, \
                           HInlinerFactory, \
                           codegen, \
                           dex_compilation_unit, \
                           driver, \
                           handles, \
                           stats)
        }) { }

  std::vector<HOptimization*> HOptimizationFactory::BuildOptimizations(const std::vector<std::string>& names) {
    std::vector<HOptimization*> ret;
    for (size_t i = 0; i < names.size(); i++) {
      const std::string& name = names[i];
      auto it = factory_map_.find(name);
      CHECK(it != factory_map_.end()) << "Missing factory for optimization: \"" << name << "\"";
      HOptimization* opt = it->second->Build(arena_, graph_);
      ret.push_back(opt);

      if (name == SideEffectsAnalysis::kSideEffectsAnalysisPassName) {
        side_effects_ = down_cast<SideEffectsAnalysis*>(opt);
      }
      if (name == HInductionVarAnalysis::kInductionPassName) {
        induction_ = down_cast<HInductionVarAnalysis*>(opt);
      }
    }
    return ret;
  }

}  // namespace art
