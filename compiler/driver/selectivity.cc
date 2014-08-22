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

#include "selectivity.h"
namespace art {
Selectivity* Selectivity::instance_ = DefaultSelectivity::GetInstance();

std::vector<const Pass*> DefaultSelectivity::custom_opt_list = {
  GetPassInstance<CacheFieldLoweringInfo>(),
  GetPassInstance<CacheMethodLoweringInfo>(),
  GetPassInstance<SpecialMethodInliner>(),
  GetPassInstance<CodeLayout>(),
  GetPassInstance<NullCheckEliminationAndTypeInference>(),
  GetPassInstance<ClassInitCheckElimination>(),
  GetPassInstance<BBCombine>(),
  GetPassInstance<BBOptimizations>(),
};

Selectivity* DefaultSelectivity::GetInstance() {
  if (instance_ == nullptr) {
    static DefaultSelectivity default_selectivity;
    instance_ = &default_selectivity;
  }
  return instance_;
}

bool DefaultSelectivity::PreCompileSummaryLogic(CompilerDriver* driver,
                                         VerificationResults* verification_results) {
  UNUSED(driver);
  UNUSED(verification_results);
  return false;
}

bool DefaultSelectivity::SkipClassCompile(const DexFile& dex_file,
                                   const DexFile::ClassDef& class_def) {
  UNUSED(dex_file);
  UNUSED(class_def);
  return false;
}

bool DefaultSelectivity::SkipMethodCompile(const DexFile::CodeItem* code_item,
                                    uint32_t method_idx, uint32_t* access_flags,
                                    uint16_t* class_def_idx, const DexFile& dex_file,
                                    DexToDexCompilationLevel* dex_to_dex_compilation_level) {
  UNUSED(code_item);
  UNUSED(method_idx);
  UNUSED(access_flags);
  UNUSED(class_def_idx);
  UNUSED(dex_file);
  UNUSED(dex_to_dex_compilation_level);
  return false;
}

void DefaultSelectivity::AnalyzeResolvedMethod(mirror::ArtMethod* method, const DexFile& dex_file) {
  UNUSED(method);
  UNUSED(dex_file);
  return;
}

void DefaultSelectivity::AnalyzeVerifiedMethod(verifier::MethodVerifier* verifier) {
  UNUSED(verifier);
  return;
}

void DefaultSelectivity::DumpSelectivityStats() {
  return;
}

void DefaultSelectivity::ToggleAnalysis(bool setting, std::string disable_passes) {
  UNUSED(disable_passes);
  if (setting) {
    PassDriver<PassDriverMEOpts>::SetSpecialDriverSelection(SwitchMethodLists);
  }
  return;
}

void DefaultSelectivity::SwitchMethodLists(PassDriver<PassDriverMEOpts>* driver, CompilationUnit* cu) {
  if (cu->compiler_driver->GetCompilerOptions().IsSmallMethod(cu->code_item->insns_size_in_code_units_)) {
    driver->CopyPasses(custom_opt_list);
  } else {
    driver->SetDefaultPasses();
  }
  return;
}

}  // namespace art
