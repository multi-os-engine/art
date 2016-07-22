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

#include <string>

#include "base/logging.h"
#include "utils.h"

namespace art {

bool BisectionController::IsMandatoryPass(const char* pass_name) {
  return mandatory_.find(pass_name) != mandatory_.end();
}

BisectionController::BisectionController()
  : method_nr_(-1),
    pass_nr_(-1),
    mandatory_({
      "pc_relative_fixups_x86",
      "pc_relative_fixups_mips",
      "dex_cache_array_fixups_mips",
      "dex_cache_array_fixups_arm"
    }) { }

BisectionController::~BisectionController() { }

static int ParseOption(const StringPiece& option_name, const StringPiece& option) {
  if (!option.starts_with(option_name)) {
    LOG(FATAL) << "Failed to parse bisection configuration.";
  }
  const StringPiece& value = option.substr(option_name.size());
  if (value == "all") {
    return std::numeric_limits<int>::max();
  }
  int ret;
  ParseInt(value.data(), &ret);
  return ret;
}

void BisectionController::Init(const std::string* config) {
  DCHECK_NE(config, (std::string*) nullptr) << "Bisection config not present.";

  StringPiece config_piece(*config);
  size_t delim_pos = config_piece.find(',');
  optimize_up_to_method_ = ParseOption("method:", config_piece.substr(0, delim_pos));
  StringPiece pass_config;
  if (delim_pos != std::string::npos) {
    pass_config = config_piece.substr(delim_pos + 1);
  }
  optimize_up_to_pass_ = ParseOption("pass:", pass_config);
}

bool BisectionController::CanOptimizeMethod(const char* method_name) {
  method_nr_++;
  int partially_optimized_method_nr = optimize_up_to_method_ - 1;
  if (method_nr_ > partially_optimized_method_nr) {
    LOG(INFO) << "NOT optimizing method [" << method_nr_ << "] " << method_name;
    return false;
  }
  if (method_nr_ < partially_optimized_method_nr) {
    LOG(INFO) << "optimizing method [" << method_nr_ << "] " << method_name;
  } else {
    LOG(INFO) << "optimizing LAST method [" << method_nr_ << "] " << method_name;
  }
  pass_nr_ = -1;
  return true;
}

bool BisectionController::CanOptimizePass(const char* pass_name) {
  if (method_nr_ >= optimize_up_to_method_) {
    return false;
  }
  pass_nr_++;
  int partially_optimized_method_nr = optimize_up_to_method_ - 1;
  bool optimizing = method_nr_ < partially_optimized_method_nr
    || pass_nr_ < optimize_up_to_pass_
    || IsMandatoryPass(pass_name);
  if (optimizing) {
    LOG(INFO) << "      doing [" << pass_nr_ << "] " << pass_name;
  } else {
    LOG(INFO) << "      NOT doing [" << pass_nr_ << "] " << pass_name;
  }
  return optimizing;
}

bool BisectionController::CanOptimizeStep() {
  return true;
}

}  // namespace art
