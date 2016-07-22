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

#include "bisection_controller.h"
#include "base/logging.h"

namespace art {

BisectionController::BisectionController()
  : next_method_nr_(0), phase_nr_(0) { }

void BisectionController::Init(uint32_t optimize_up_to_method, uint32_t optimize_up_to_phase) {
  optimize_up_to_method_ = optimize_up_to_method;
  optimize_up_to_phase_ = optimize_up_to_phase;
}

bool BisectionController::CanOptimizeMethod(const char* method_name) {
  if (next_method_nr_ < optimize_up_to_method_) {
    LOG(INFO) << "optimizing method [" << next_method_nr_ << "] " << method_name;
  } else {
    LOG(INFO) << "NOT optimizing method [" << next_method_nr_ << "] " << method_name;
  }
  next_method_nr_++;
  phase_nr_ = 0;
  return true;
}

bool BisectionController::CanOptimizePhase(const char* phase_name) {
  size_t cur_method_nr = next_method_nr_ - 1;
  if (cur_method_nr >= optimize_up_to_method_) {
    return false;
  }
  size_t partially_optimized_method_nr = optimize_up_to_method_ - 1;
  bool optimizing = cur_method_nr < partially_optimized_method_nr || phase_nr_ < optimize_up_to_phase_;
  if (optimizing) {
    LOG(INFO) << "      doing [" << phase_nr_ << "] " << phase_name;
  } else {
    LOG(INFO) << "      NOT doing [" << phase_nr_ << "] " << phase_name;
  }
  phase_nr_++;
  return optimizing;
}

}  // namespace art
