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

#ifndef ART_ANALYSIS_STATIC_ANALYSIS_PASS_H_
#define ART_ANALYSIS_STATIC_ANALYSIS_PASS_H_
#include "base/macros.h"
#include "base/mutex.h"
#include "dex/pass.h"
#include "static_analysis_info.h"
#include "verifier/method_verifier.h"

namespace art {

// Data holder class.
class StaticAnalysisPassDataHolder: public PassDataHolder {
  public:
    mirror::ArtMethod* method;
    const DexFile* dex_file;
    verifier::MethodVerifier* verifier;
    uint32_t* static_analysis_method_info;
};

class StaticAnalysisMethodCumulativeStats {
};

class StaticAnalysisPass : public Pass {
 public:
  explicit StaticAnalysisPass(const char* name)
    : Pass(name) {
  }

  virtual ~StaticAnalysisPass() {
  }

  /**
   * @fn PerformAnalysis(mirror::ArtMethod* method, const DexFile& dex_file)
   * @brief Performs the particular analysis of the method.
   * @param method an ArtMethod pointer.
   * @param dex_file a dex file reference.
   * @return A bit mask representing the static analysis information for that particular pass.
   */
  virtual uint32_t PerformAnalysis(StaticAnalysisMethodCumulativeStats* stats, mirror::ArtMethod* method, const DexFile* dex_file, verifier::MethodVerifier* verifier) const {
    UNUSED(stats);
    UNUSED(method);
    UNUSED(dex_file);
    UNUSED(verifier);
    return kMethodNone;
  }

  /**
   * @fn DumpPassAnalysis(std::stringstream& string_stream)
   * @brief Dumps the stats that are analyzed for the pass for debugging and data collection purposes.
   * @param string_stream a stringstream to be used to append the pass's stats data to it.
   * Concatenates to a stringstream that can later be used in a LOG statement.
   * Intent is to pass the stringstream to consecutive passes and then finally log it.
   * e.x. LOG(INFO) << my_string_stream_that_was_passed_to_many_DumpPassAnalsis.str().
   */
  virtual void DumpPassAnalysis(std::stringstream& string_stream) const = 0;

  /**
   * @fn Worker(const PassDataHolder* data) const
   * @brief Gets the Pass Name.
   * @return
   */
  bool Worker(const PassDataHolder* data) const {
    const StaticAnalysisPassDataHolder* static_analysis_pass_data_holder = down_cast<const StaticAnalysisPassDataHolder*>(data);
    *(static_analysis_pass_data_holder->static_analysis_method_info) |= PerformAnalysis(GetStats(), static_analysis_pass_data_holder->method, static_analysis_pass_data_holder->dex_file, static_analysis_pass_data_holder->verifier);
    return true;
  }

 protected:
  /**
   * @fn DetermineInfoSize(uint32_t category_instructions, uint32_t total_num_instructions)
   * @brief Calculates the percent of instructions representing a particular category
   *  as part of the total number of instructions in a method.
   *  Puts the percentage into a category that can later be referenced.
   *  LARGE: info presence % >66%
   *  MEDIUM: 33%<info presence %<=66%
   *  SMALL: 0<info presence %<=33%
   *  NONE: info presence % <=0%
   * @param  category_instructions The number of dex opcode instructions representing
   *  particular information / category within a method.
   * @param  total_num_instructions The total number of dex opcode instructions within a method.
   */
  StaticAnalysisInfoSize DetermineInfoSize(uint32_t category_instructions, uint32_t total_num_instructions) const {
    float category_info_rate = static_cast<float>(category_instructions) / static_cast<float>(total_num_instructions);
    if (category_info_rate > kLargeStaticAnalysisInfoMin) {
      return StaticAnalysisInfoSize::LARGE;
    } else if (category_info_rate > kMediumStaticAnalysisInfoMin) {
      return StaticAnalysisInfoSize::MEDIUM;
    } else if (category_info_rate > kSmallStaticAnalysisInfoMin) {
      return StaticAnalysisInfoSize::SMALL;
    }
    return StaticAnalysisInfoSize::NONE;
  }

  /**
   * @fn GetInfoBitValue (uint32_t category_instructions, uint32_t total_num_instructions,
   *  uint32_t none_mask, uint32_t small_mask, uint32_t medium_mask, uint32_t large_mask)
   * @brief Evaluates the number of instructions for a particular category and returns the bitmask.
   *  More information on the non overlapping information bitmasks can be found in static_analysis_info.h.
   * @param  category_instructions The number of dex opcode instructions representing
   *  particular information / category within a method.
   * @param  total_num_instructions The total number of dex opcode instructions within a method
   * @param  none_mask The bitmask to return if the number of opcode instructions
   *  is 0 for a particular category of information.
   * @param  small_mask The bitmask to return if the number of opcode instructions for a
   *  particular category of information is determined to be SMALL by DetermineInfoSize().
   * @param  medium_mask The bitmask to return if the number of opcode instructions for a
   *  particular category of information is determined to be MEDIUM by DetermineInfoSize().
   * @param  large_mask The bitmask to return if the number of opcode instructions for a
   *  particular category of information is determined to be LARGE by DetermineInfoSize().
   * @return The correct bitmask to return among the ones that were passed in.
   */
  uint32_t GetInfoBitValue(uint32_t category_instructions, uint32_t total_num_instructions,
      uint32_t none_mask, uint32_t small_mask, uint32_t medium_mask, uint32_t large_mask) const {
    switch (DetermineInfoSize(category_instructions, total_num_instructions)) {
      case StaticAnalysisInfoSize::NONE:
        return none_mask;
      case StaticAnalysisInfoSize::SMALL:
        return small_mask;
      case StaticAnalysisInfoSize::MEDIUM:
        return medium_mask;
      case StaticAnalysisInfoSize::LARGE:
        return large_mask;
    }
    return none_mask;
  }

  /**
   * @fn CumulativePassStatsType* GetPassStatsInstance() const
   * @brief Gets the static instance of a particular pass stats holder.
   * @return Return the static instance of the pass stats holder.
   */
  template <typename CumulativePassStatsType>
  CumulativePassStatsType* GetPassStatsInstance() const {
    static CumulativePassStatsType stats;
    return &stats;
  }

  /**
   * @fn StaticAnalysisMethodCumulativeStats* GetStats() const
   * @brief The getter function that returns the pass stats holder.
   *  Used by Worker to pass a mutuable version of the stats to PerformAnalysis.
   */
  virtual StaticAnalysisMethodCumulativeStats* GetStats() const = 0;
};
}  // namespace art
#endif  // ART_ANALYSIS_STATIC_ANALYSIS_PASS_H_
