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

#include <set>
#include <string>

#include "utils.h"

namespace art {

/**
 * Implements pass control for purposes of builtin bisection bug search
 *
 * Config's format is 'method:<value>,pass:<value>' where <value> can be equal to 'all' or
 * an integer. Value for method denotes number of methods to be optimized. Value for
 * pass specifies how many optimization passes are to be ran for the last optimized method.
 * If a value is equal to 'all' all methods are optimized or all optimizations ran
 * respectively.
 */
class BisectionController {
 public:
  BisectionController();
  ~BisectionController();
  void Init(const std::string* config);
  /**
   * True if should optimize method according to configured rules
   */
  bool CanOptimizeMethod(const char* method_name);
  /**
   * True if should run optimization pass according to configured rules
   */
  bool CanOptimizePass(const char* pass_name);
  /**
   * True if should run optimization step according to configured rules
   */
  bool CanOptimizeStep();

 private:
  bool IsMandatoryPass(const char* pass_name);

  int optimize_up_to_method_;
  int optimize_up_to_pass_;
  int method_nr_;
  int pass_nr_;
  const std::set<const std::string> mandatory_;

  DISALLOW_COPY_AND_ASSIGN(BisectionController);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_BISECTION_CONTROLLER_H_
