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

// This optimization recognizes a common pattern where a boolean value is
// negated by an If statement selecting from zero/one integer constants.
// The condition is negated and used to replace the entire pattern.
// Note: in order to recognize empty blocks, this optimization must be run
// after the instruction simplifier which removes redundant suspend checks.

#ifndef ART_COMPILER_OPTIMIZING_BOOLEAN_NOT_SIMPLIFIER_H_
#define ART_COMPILER_OPTIMIZING_BOOLEAN_NOT_SIMPLIFIER_H_

#include "optimization.h"

namespace art {

class HBooleanNotSimplifier : public HOptimization {
 public:
  explicit HBooleanNotSimplifier(HGraph* graph)
    : HOptimization(graph, true, kBooleanNotSimplifierPassName) {}

  void Run() OVERRIDE;

  static constexpr const char* kBooleanNotSimplifierPassName = "boolean_not_simplifier";

 private:
  DISALLOW_COPY_AND_ASSIGN(HBooleanNotSimplifier);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_BOOLEAN_NOT_SIMPLIFIER_H_
