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
// Initialize the function pointers
bool (*Selectivity::precompile_summary_action)(CompilerDriver* driver, VerificationResults* verification_results) = nullptr;
bool (*Selectivity::class_selectivity_action)(const DexFile& dex_file, const DexFile::ClassDef&);
bool (*Selectivity::method_selectivity_action)(const DexFile::CodeItem* code_item, uint32_t method_idx, uint32_t* access_flags, uint16_t* class_def_idx, const DexFile& dex_file, DexToDexCompilationLevel* dex_to_dex_compilation_level);
void Selectivity::SetPreCompileSummaryAction(bool (*function)(CompilerDriver* driver, VerificationResults* verification_results)) {
  if (function != nullptr) {
    precompile_summary_action = function;
  }
}

bool Selectivity::ExecutePreCompileSummaryAction(CompilerDriver* driver, VerificationResults* verification_results) {
  if (precompile_summary_action == nullptr) {
    return false;
  } else {
    return (*precompile_summary_action)(driver, verification_results);
  }
}

void Selectivity::SetClassSelectivityAction(bool (*function)(const DexFile& dex_file, const DexFile::ClassDef& class_def)) {
  if (function != nullptr) {
    class_selectivity_action = function;
  }
}

bool Selectivity::ExecuteClassSelectivityAction(const DexFile& dex_file, const DexFile::ClassDef& class_def) {
  if (class_selectivity_action == nullptr) {
    return false;
  } else {
    return (*class_selectivity_action)(dex_file, class_def);
  }
}

void Selectivity::SetMethodSelectivityAction(bool (*function)(const DexFile::CodeItem* code_item, uint32_t method_idx, uint32_t* access_flags, uint16_t* class_def_idx, const DexFile& dex_file, DexToDexCompilationLevel* dex_to_dex_compilation_level)) {
  if (function != nullptr) {
    method_selectivity_action = function;
  }
}

bool Selectivity::ExecuteMethodSelectivityAction(const DexFile::CodeItem* code_item, uint32_t method_idx, uint32_t* access_flags, uint16_t* class_def_idx, const DexFile& dex_file, DexToDexCompilationLevel* dex_to_dex_compilation_level) {
  if (method_selectivity_action == nullptr) {
    return false;
  } else {
    return (*method_selectivity_action)(code_item, method_idx, access_flags, class_def_idx, dex_file, dex_to_dex_compilation_level);
  }
}
}  // namespace art
