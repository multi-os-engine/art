/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ART_RUNTIME_BASE_STATISTICS_H_
#define ART_RUNTIME_BASE_STATISTICS_H_

#include <iostream>

#include "base/stringprintf.h"

namespace art {

// A naive class that compute statistics over a set of values.
class Statistics {
 public:
  explicit Statistics(const char* stat_name) : stat_name_(stat_name) {}

  const char* GetName() const { return stat_name_; }

  // Take into account `value` and update the statistics.
  void Insert(size_t value) {
    ++num_;
    sum_ += value;
    min_ = std::min(min_, value);
    max_ = std::max(max_, value);
  }

  // Return the number of values.
  size_t num() const { return num_; }

  // Return the sum of the values.
  size_t sum() const { return sum_; }

  // Return the minimum value.
  size_t min() const { return num_ ? min_ : 0; }

  // Return the maximum value.
  size_t max() const { return num_ ? max_ : 0; }

  // Return the average value.
  double avg() const {
    return num_ ? (static_cast<double>(sum_) / num_) : 0.0;
  }

  void DumpToCSV(std::ostream& os) const {
    os << GetName() << "," << num() << "," << sum() << ",";
    os << min() << "," << max() << "," << StringPrintf("%.2f\n", avg());
  }

  static void DumpCSVHeader(std::ostream& os) {
    os << "Name,No.,Total Size,Min Size,Max Size,Avg Size\n";
  }

 private:
  static constexpr size_t default_min_value = std::numeric_limits<size_t>::max();
  static constexpr size_t default_max_value = std::numeric_limits<size_t>::min();

  size_t num_ = 0;
  size_t sum_ = 0;
  size_t min_ = default_min_value;
  size_t max_ = default_max_value;

  const char* stat_name_;
};

}  // namespace art

#endif  // ART_RUNTIME_BASE_STATISTICS_H_
