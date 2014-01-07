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

#ifndef ART_COMPILER_DEX_BB_OPTIMIZATIONS_H_
#define ART_COMPILER_DEX_BB_OPTIMIZATIONS_H_

#include "compiler_internals.h"
#include "pass.h"

namespace art {

/**
 * @class CodeLayout
 * @brief Perform the code layout pass.
 */
class CodeLayout : public Pass {
 public:
  /**
   * @brief Constructor.
   */
  CodeLayout();

  /**
   * @brief Walk the basic blocks.
   * @param cUnit the CompilationUnit.
   */
  virtual bool WalkBasicBlocks(CompilationUnit *cUnit, BasicBlock *bb) const;

  /**
   * @brief Start of the pass function.
   * @param cUnit the CompilationUnit.
   */
  virtual void Start(CompilationUnit *cUnit) const;
};

/**
 * @class SSATransformation
 * @brief Perform an SSA representation pass on the CompilationUnit.
 */
class SSATransformation : public Pass {
 public:
  /**
   * @brief Constructor.
   */
  SSATransformation();

  /**
   * @brief Walk the basic blocks.
   * @param cUnit the CompilationUnit.
   */
  virtual bool WalkBasicBlocks(CompilationUnit *cUnit, BasicBlock *bb) const;

  /**
   * @brief Start of the pass function.
   * @param cUnit the CompilationUnit.
   */
  virtual void Start(CompilationUnit *cUnit) const;

  /**
   * @brief End of the pass function.
   * @param cUnit the CompilationUnit.
   */
  virtual void End(CompilationUnit *cUnit) const;
};

/**
 * @class ConstantPropagation
 * @brief Perform a constant propagation pass.
 */
class ConstantPropagation : public Pass {
 public:
  /**
   * @brief Constructor.
   */
  ConstantPropagation();

  /**
   * @brief Walk the basic blocks.
   * @param cUnit the CompilationUnit.
   */
  virtual bool WalkBasicBlocks(CompilationUnit *cUnit, BasicBlock *bb) const;

  /**
   * @brief Start of the pass function.
   * @param cUnit the CompilationUnit.
   */
  virtual void Start(CompilationUnit *cUnit) const;
};

/**
 * @class InitRegLocations
 * @brief Initialize Register Locations.
 */
class InitRegLocations : public Pass {
 public:
  /**
   * @brief Constructor.
   */
  InitRegLocations();

  /**
   * @brief Start of the pass function.
   * @param cUnit the CompilationUnit.
   */
  virtual void Start(CompilationUnit *cUnit) const;
};

/**
 * @class MethodUseCount
 * @brief Count the uses of the method
 */
class MethodUseCount : public Pass {
 public:
  /**
   * @brief Constructor.
   */
  MethodUseCount();

  /**
   * @brief Walk the basic blocks.
   * @param cUnit the CompilationUnit.
   */
  virtual bool WalkBasicBlocks(CompilationUnit *cUnit, BasicBlock *bb) const;

  /**
   * @brief Gate for the pass, taking the CompilationUnit and the pass information.
   * @param cUnit the CompilationUnit.
   */
  virtual bool Gate(const CompilationUnit *cUnit) const;
};

/**
 * @class NullCheckEliminationAndTypeInferenceInit
 * @brief Null check elimination and type inference initialization step.
 */
class NullCheckEliminationAndTypeInferenceInit : public Pass {
 public:
  /**
   * @brief Constructor.
   */
  NullCheckEliminationAndTypeInferenceInit();

  /**
   * @brief Walk the basic blocks.
   * @param cUnit the CompilationUnit.
   */
  virtual bool WalkBasicBlocks(CompilationUnit *cUnit, BasicBlock *bb) const;

  /**
   * @brief Gate for the pass, taking the CompilationUnit and the pass information.
   * @param cUnit the CompilationUnit.
   */
  virtual bool Gate(const CompilationUnit *cUnit) const;
};

/**
 * @class NullCheckEliminationAndTypeInference
 * @brief Null check elimination and type inference.
 */
class NullCheckEliminationAndTypeInference : public Pass {
 public:
  /**
   * @brief Constructor.
   */
  NullCheckEliminationAndTypeInference();

  /**
   * @brief Walk the basic blocks.
   * @param cUnit the CompilationUnit.
   */
  bool WalkBasicBlocks(CompilationUnit *cUnit, BasicBlock *bb) const;
};

/**
 * @class NullCheckEliminationAndTypeInference
 * @brief Null check elimination and type inference.
 */
class BBCombine : public Pass {
 public:
  /**
   * @brief Constructor.
   */
  BBCombine();

  /**
   * @brief Walk the basic blocks.
   * @param cUnit the CompilationUnit.
   */
  bool WalkBasicBlocks(CompilationUnit *cUnit, BasicBlock *bb) const;

  /**
   * @brief Gate for the pass, taking the CompilationUnit and the pass information.
   * @param cUnit the CompilationUnit.
   */
  bool Gate(const CompilationUnit *cUnit) const;
};

/**
 * @class BasicBlock Optimizations
 * @brief Any simple BasicBlock optimization can be put here.
 */
class BBOptimizations : public Pass {
 public:
  /**
   * @brief Constructor.
   */
  BBOptimizations();

  /**
   * @brief Gate for the pass, taking the CompilationUnit and the pass information.
   * @param cUnit the CompilationUnit.
   */
  bool Gate(const CompilationUnit *cUnit) const;

  /**
   * @brief Start of the pass function.
   * @param cUnit the CompilationUnit.
   */
  void Start(CompilationUnit *cUnit) const;
};

}  // namespace art

#endif  // ART_COMPILER_DEX_BB_OPTIMIZATIONS_H_
