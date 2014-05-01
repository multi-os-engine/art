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
#include "method_static_analysis.h"
#include "static_analysis_pass_driver.h"

namespace art {
template<>
const Pass* const PassDriver<StaticAnalysisPassDriver>::g_passes[] = {
  GetPassInstance<MethodLogisticsAnalysis>(),
  GetPassInstance<MethodMiscLogisticsAnalysis>(),
  GetPassInstance<MethodSizeAnalysis>(),
  GetPassInstance<MethodOpcodeAnalysis>(),
};
template<>
uint16_t const PassDriver<StaticAnalysisPassDriver>::g_passes_size = arraysize(PassDriver<StaticAnalysisPassDriver>::g_passes);
// The default pass list is used by CreatePasses to initialize pass_list_.
template<>
std::vector<const Pass*> PassDriver<StaticAnalysisPassDriver>::g_default_pass_list(PassDriver<StaticAnalysisPassDriver>::g_passes,
      PassDriver<StaticAnalysisPassDriver>::g_passes + PassDriver<StaticAnalysisPassDriver>::g_passes_size);

StaticAnalysisPassDriver::StaticAnalysisPassDriver(mirror::ArtMethod* method, const DexFile* dex_file, uint32_t* static_analysis_method_info)
  : PassDriver(), static_analysis_pass_data_holder_() {
  static_analysis_pass_data_holder_.method = method;
  static_analysis_pass_data_holder_.dex_file = dex_file;
  static_analysis_pass_data_holder_.verifier = nullptr;
  static_analysis_pass_data_holder_.static_analysis_method_info = static_analysis_method_info;
}

StaticAnalysisPassDriver::StaticAnalysisPassDriver(verifier::MethodVerifier* verifier, uint32_t* static_analysis_method_info)
  : PassDriver(), static_analysis_pass_data_holder_() {
  static_analysis_pass_data_holder_.method = nullptr;
  static_analysis_pass_data_holder_.dex_file = verifier->GetDexCache()->GetDexFile();
  static_analysis_pass_data_holder_.verifier = verifier;
  static_analysis_pass_data_holder_.static_analysis_method_info = static_analysis_method_info;
}

StaticAnalysisPassDriver::~StaticAnalysisPassDriver() {
}

bool StaticAnalysisPassDriver::RunPass(const Pass* pass, bool time_split) {
  UNUSED(time_split);
  DCHECK(pass != nullptr);
  DCHECK(pass->GetName() != nullptr && pass->GetName()[0] != 0);
  pass->Worker(&static_analysis_pass_data_holder_);
  return true;
}

const std::string StaticAnalysisPassDriver::DumpAnalysis() {
  std::stringstream ss;
  for (const Pass* pass : PassDriver<StaticAnalysisPassDriver>::g_default_pass_list) {
    const StaticAnalysisPass* cur_pass = down_cast<const StaticAnalysisPass*>(pass);
    cur_pass->DumpPassAnalysis(ss);
  }
  return ss.str();
}

}  // namespace art
