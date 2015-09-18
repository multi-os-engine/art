/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ART_COMPILER_OPTIMIZING_GENERATE_SELECTS_H_
#define ART_COMPILER_OPTIMIZING_GENERATE_SELECTS_H_

#include "optimization.h"

namespace art {

class HGenerateSelects : public HOptimization {
 public:
  void Run() OVERRIDE SHARED_REQUIRES(Locks::mutator_lock_);

 protected:
  HGenerateSelects(HGraph* graph,
                   OptimizingCompilerStats* stats,
                   const char *pass_name)
    : HOptimization(graph, pass_name, stats) { }

 private:
  void TryGeneratingSelects(HBasicBlock* block) SHARED_REQUIRES(Locks::mutator_lock_);

  virtual bool SupportsSelect(Primitive::Type cond_type, Primitive::Type value_type) = 0;
};

class HX86GenerateSelects : public HGenerateSelects {
 public:
  HX86GenerateSelects(HGraph* graph,
                      OptimizingCompilerStats* stats,
                      const char *pass_name = kX86GenerateSelectsPassName)
    : HGenerateSelects(graph, stats, pass_name) {}

  static constexpr const char* kX86GenerateSelectsPassName = "generate_selects_x86";

 private:
  bool SupportsSelect(Primitive::Type cond_type, Primitive::Type value_type) OVERRIDE {
    // X86 can't handle FP for either condition or value.
    if (Primitive::IsFloatingPointType(cond_type) || Primitive::IsFloatingPointType(value_type)) {
      return false;
    }

    // X86 can't handle long as the condition type.
    return cond_type != Primitive::kPrimLong;
  }

  DISALLOW_COPY_AND_ASSIGN(HX86GenerateSelects);
};

class HX86_64GenerateSelects : public HGenerateSelects {
 public:
  HX86_64GenerateSelects(HGraph* graph,
                         OptimizingCompilerStats* stats,
                         const char *pass_name = kX86_64GenerateSelectsPassName)
    : HGenerateSelects(graph, stats, pass_name) {}

  static constexpr const char* kX86_64GenerateSelectsPassName = "generate_selects_x86_64";

 private:
  bool SupportsSelect(Primitive::Type cond_type, Primitive::Type value_type) OVERRIDE {
    // X86 can't handle FP for either condition or value.
    if (Primitive::IsFloatingPointType(cond_type) || Primitive::IsFloatingPointType(value_type)) {
      return false;
    }

    return true;
  }

  DISALLOW_COPY_AND_ASSIGN(HX86_64GenerateSelects);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_GENERATE_SELECTS_H_
