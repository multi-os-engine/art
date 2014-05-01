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

#ifndef ART_ANALYSIS_STATIC_ANALYSIS_PASS_DRIVER_H_
#define ART_ANALYSIS_STATIC_ANALYSIS_PASS_DRIVER_H_

#include "dex/pass_driver.h"
#include "mirror/art_method.h"
#include "static_analysis_pass.h"

namespace art {

class StaticAnalysisPassDriver : public PassDriver<StaticAnalysisPassDriver> {
 public:
  StaticAnalysisPassDriver(mirror::ArtMethod* method, const DexFile* dex_file, uint32_t* results);
  StaticAnalysisPassDriver(verifier::MethodVerifier*, uint32_t* results);
  ~StaticAnalysisPassDriver();
  bool RunPass(const Pass* pass, bool time_split = true);
  static const std::string DumpAnalysis();
 protected:
  StaticAnalysisPassDataHolder static_analysis_pass_data_holder_;
};

}  // namespace art

#endif  // ART_ANALYSIS_STATIC_ANALYSIS_PASS_DRIVER_H_
