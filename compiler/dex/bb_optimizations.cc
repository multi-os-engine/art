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

#include "bb_optimizations.h"
#include "dataflow_iterator.h"
#include "dataflow_iterator-inl.h"

namespace art {

  /*
   * Code Layout pass implementation start.
   */
  CodeLayout::CodeLayout() {
    passName = "CodeLayout";
    dumpCFGFolder = "2_post_layout_cfg";
  }

  void CodeLayout::start(CompilationUnit *cUnit) const {
    // Verify the layout.
    cUnit->mir_graph->VerifyDataflow();
  }

  bool CodeLayout::walkBasicBlocks(CompilationUnit *cUnit, BasicBlock *bb) const {
    cUnit->mir_graph->LayoutBlocks(bb);
    // No need of repeating, so just return false.
    return false;
  }

  /*
   * SSATransformation pass implementation start.
   */
  SSATransformation::SSATransformation() {
    passName = "SSATransformation";
    traversalType = kPreOrderDFSTraversal;
    dumpCFGFolder = "3_post_ssa_cfg";
  }

  void SSATransformation::start(CompilationUnit *cUnit) const {
    cUnit->mir_graph->InitializeSSATransformation();
  }

  bool SSATransformation::walkBasicBlocks(CompilationUnit *cUnit, BasicBlock *bb) const {
    cUnit->mir_graph->InsertPhiNodeOperands(bb);
    // No need of repeating, so just return false.
    return false;
  }

  void SSATransformation::end(CompilationUnit *cUnit) const {
    // Verify the dataflow information after the pass.
    if (cUnit->enable_debug & (1 << kDebugVerifyDataflow)) {
      cUnit->mir_graph->VerifyDataflow();
    }
  }

  /*
   * ConstantPropagation pass implementation start
   */
  ConstantPropagation::ConstantPropagation() {
    passName = "ConstantPropagation";
  }

  void ConstantPropagation::start(CompilationUnit *cUnit) const {
    cUnit->mir_graph->InitializeConstantPropagation();
  }

  bool ConstantPropagation::walkBasicBlocks(CompilationUnit *cUnit, BasicBlock *bb) const {
    cUnit->mir_graph->DoConstantPropagation(bb);
    // No need of repeating, so just return false.
    return false;
  }

  /*
   * InitRegLocation pass implementation start.
   */
  InitRegLocations::InitRegLocations() {
    passName = "InitRegLocation";
  }

  void InitRegLocations::start(CompilationUnit *cUnit) const {
    cUnit->mir_graph->InitRegLocations();
  }

  /*
   * MethodUseCount pass implementation start.
   */
  MethodUseCount::MethodUseCount() {
    passName = "UseCount";
  }

  bool MethodUseCount::gate(const CompilationUnit *cUnit) const {
    // First initialize the data.
    cUnit->mir_graph->InitializeMethodUses();

    // Check if the pass is to be ignored.
    bool res = ((cUnit->disable_opt & (1 << kPromoteRegs)) == 0);

    return res;
  }

  bool MethodUseCount::walkBasicBlocks(CompilationUnit *cUnit, BasicBlock *bb) const {
    cUnit->mir_graph->CountUses(bb);
    // No need of repeating, so just return false.
    return false;
  }

  /*
   * Null Check Elimination and Type Inference Initialization pass implementation start.
   */
  NullCheckEliminationAndTypeInferenceInit::NullCheckEliminationAndTypeInferenceInit(void) {
    passName = "NCE_TypeInferenceInit";
  }

  bool NullCheckEliminationAndTypeInferenceInit::gate(const CompilationUnit *cUnit) const {
    // First check the ssa register vector
    cUnit->mir_graph->CheckSSARegisterVector();

    // Did we disable the pass?
    bool performInit = ((cUnit->disable_opt & (1 << kNullCheckElimination)) == 0);

    return performInit;
  }

  bool NullCheckEliminationAndTypeInferenceInit::walkBasicBlocks(CompilationUnit *cUnit, BasicBlock *bb) const {
    cUnit->mir_graph->NullCheckEliminationInit(bb);
    // No need of repeating, so just return false.
    return false;
  }

  /*
   * Null Check Elimination and Type Inference pass implementation start.
   */
  NullCheckEliminationAndTypeInference::NullCheckEliminationAndTypeInference(void) {
    passName = "NCE_TypeInference";
    traversalType = kRepeatingPreOrderDFSTraversal;
    dumpCFGFolder = "4_post_nce_cfg";
  }

  bool NullCheckEliminationAndTypeInference::walkBasicBlocks(CompilationUnit *cUnit, BasicBlock *bb) const {
    return cUnit->mir_graph->EliminateNullChecksAndInferTypes(bb);
  }

  /*
   * BasicBlock Combine pass implementation start.
   */
  BBCombine::BBCombine() {
    passName = "BBCombine";
    traversalType = kPreOrderDFSTraversal;
    dumpCFGFolder = "5_post_bbcombine_cfg";
  }

  bool BBCombine::gate(const CompilationUnit *cUnit) const {
    bool performPass = ((cUnit->disable_opt & (1 << kSuppressExceptionEdges)) != 0);
    return performPass;
  }

  bool BBCombine::walkBasicBlocks(CompilationUnit *cUnit, BasicBlock *bb) const {
    cUnit->mir_graph->CombineBlocks(bb);

    // No need of repeating, so just return false.
    return false;
  }

  /*
   * BasicBlock Optimization pass implementation start.
   */
  BBOptimizations::BBOptimizations() {
    passName = "BBOptimizations";
    dumpCFGFolder = "5_post_bbo_cfg";
  }

  bool BBOptimizations::gate(const CompilationUnit *cUnit) const {
    bool performPass = ((cUnit->disable_opt & (1 << kBBOpt)) == 0);
    return performPass;
  }

  void BBOptimizations::start(CompilationUnit *cUnit) const {
    DCHECK_EQ(cUnit->num_compiler_temps, 0);

    /*
     * This pass has a different ordering depending on the suppress exception,
     * so do the pass here for now:
     *   - Later, the start should just change the ordering and we can move the extended
     *     creation into the pass driver's main job with a new iterator
     */
    cUnit->mir_graph->BasicBlockOptimization();
  }
}  // namespace art
