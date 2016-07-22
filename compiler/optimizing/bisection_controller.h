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

#ifndef ART_COMPILER_OPTIMIZING_BISECTION_CONTROLLER_H_
#define ART_COMPILER_OPTIMIZING_BISECTION_CONTROLLER_H_

#include <stdlib.h>

#include "utils.h"

namespace art {

class BisectionController {
 public:
  BisectionController();
  void Init(uint32_t optimize_up_to_method, uint32_t optimize_up_to_phase);
  void StartOptimizingNextMethod(const char* method_name);
  bool CanRunOptimizationPhase(const char* phase_name);

 private:
  uint32_t optimize_up_to_method_;
  uint32_t optimize_up_to_phase_;
  uint32_t next_method_nr_;
  uint32_t phase_nr_;
  DISALLOW_COPY_AND_ASSIGN(BisectionController);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_BISECTION_CONTROLLER_H_
