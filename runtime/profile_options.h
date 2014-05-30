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

#ifndef ART_RUNTIME_PROFILE_OPTIONS_H_
#define ART_RUNTIME_PROFILE_OPTIONS_H_

#include <string>
#include <ostream>

namespace art {

class ProfileOptions {
 public:
  static constexpr bool kDefaultEnabled = false;
  static constexpr uint32_t kDefaultPeriodS = 10;
  static constexpr uint32_t kDefaultDurationS = 20;
  static constexpr uint32_t kDefaultIntervalUs = 500;
  static constexpr double kDefaultBackoffCoefficient = 2.0;
  static constexpr bool kDefaultStartImmediately = false;
  static constexpr bool kDefaultOptimizationEnabled = false;
  static constexpr double kDefaultTopKThreshold = 90.0;
  static constexpr double kDefaultChangeInTopKThreshold = 10.0;

  ProfileOptions() :
    enabled_(kDefaultEnabled),
    output_filename_(""),
    period_s_(kDefaultPeriodS),
    duration_s_(kDefaultDurationS),
    interval_us_(kDefaultIntervalUs),
    backoff_coefficient_(kDefaultBackoffCoefficient),
    start_immediately_(kDefaultStartImmediately),
    optimization_enabled_(kDefaultOptimizationEnabled),
    top_k_threshold_(kDefaultTopKThreshold),
    change_in_top_k_threshold_(kDefaultChangeInTopKThreshold) {}

  ProfileOptions(bool enabled,
                 std::string output_filename,
                 uint32_t period_s,
                 uint32_t duration_s,
                 uint32_t interval_us,
                 double backoff_coefficient,
                 bool start_immediately,
                 bool optimization_enabled,
                 double top_k_threshold,
                 double change_in_top_k_threshold):
    enabled_(enabled),
    output_filename_(output_filename),
    period_s_(period_s),
    duration_s_(duration_s),
    interval_us_(interval_us),
    backoff_coefficient_(backoff_coefficient),
    start_immediately_(start_immediately),
    optimization_enabled_(kDefaultOptimizationEnabled),
    top_k_threshold_(top_k_threshold),
    change_in_top_k_threshold_(change_in_top_k_threshold) {}

  bool IsEnabled() const {
    return enabled_;
  }

  std::string GetOutputFilename() const {
    return output_filename_;
  }

  uint32_t GetPeriodS() const {
    return period_s_;
  }

  uint32_t GetDurationS() const {
    return duration_s_;
  }

  uint32_t GetIntervalUs() const {
    return interval_us_;
  }

  double GetBackoffCoefficient() const {
    return backoff_coefficient_;
  }

  bool GetStartImmediately() const {
    return start_immediately_;
  }

  bool IsOptimizationEnabled() const {
    return optimization_enabled_;
  }

  double GetTopKThreshold() const {
    return top_k_threshold_;
  }

  double GetChangeInTopKThreshold() const {
    return change_in_top_k_threshold_;
  }

  void SetOutputFilename(const std::string& output_filename) {
    output_filename_ = output_filename;
  }

  friend std::ostream & operator<<(std::ostream &os, const ProfileOptions& po) {
    os << "enabled=" << po.enabled_
       << ", output_filename=" << po.output_filename_
       << ", period_s=" << po.period_s_
       << ", duration_s=" << po.duration_s_
       << ", interval_us=" << po.interval_us_
       << ", backoff_coefficient=" << po.backoff_coefficient_
       << ", start_immediately=" << po.start_immediately_
       << ", optimization_enabled" << po.optimization_enabled_
       << ", top_k_threshold=" << po.top_k_threshold_
       << ", change_in_top_k_threshold=" << po.change_in_top_k_threshold_;
    return os;
  }

  friend class ParsedOptions;

 private:
  // Whether or not the applications should be profiled.
  bool enabled_;
  // The name of the file where profile data should be stored.
  std::string output_filename_;
  // Generate profile every n seconds.
  uint32_t period_s_;
  // Run profile for n seconds.
  uint32_t duration_s_;
  // Microseconds between samples.
  uint32_t interval_us_;
  // Coefficient to exponential backoff.
  double backoff_coefficient_;
  // Whether the profile should start upon app startup or be delayed by some random offset.
  bool start_immediately_;
  // Indicates if we should optimize / recompile based on profiles.
  bool optimization_enabled_;
  // Top K% of samples that are considered relevant when deciding what to compile.
  double top_k_threshold_;
  // How much the top K% samples needs to change in order for the app to be recompiled.
  double change_in_top_k_threshold_;
};

}  // namespace art

#endif  // ART_RUNTIME_PROFILE_OPTIONS_H_
