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

#ifndef ART_COMPILER_OPTIMIZING_SUSPEND_CHECK_ELIMINATION_H_
#define ART_COMPILER_OPTIMIZING_SUSPEND_CHECK_ELIMINATION_H_

#include "optimization.h"

namespace art {

class CodeGenerator;

/**
 * Remove suspend checks in leaf functions.
 */
class SuspendCheckElimination : public HOptimization {
 public:
  explicit SuspendCheckElimination(const CodeGenerator* codegen);
  void Run() OVERRIDE;
  static constexpr const char* kSuspendCheckEliminationPassName = "suspend_check_elimination";

 private:
  const CodeGenerator* codegen_;
  DISALLOW_COPY_AND_ASSIGN(SuspendCheckElimination);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_SUSPEND_CHECK_ELIMINATION_H_
