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

#include "mirror/art_method-inl.h"
#include "object_utils.h"
#include "static_analyzer.h"

namespace art {

StaticAnalyzer::StaticAnalyzer()
  : static_analysis_methods_info_lock_("static analysis methods info lock") {
}

StaticAnalyzer::~StaticAnalyzer() {
  Thread* self = Thread::Current();
  {
    MutexLock mu(self, static_analysis_methods_info_lock_);
    STLDeleteValues(&static_analysis_methods_info_);
  }
}

bool StaticAnalyzer::IsMethodSizeIn(mirror::ArtMethod* method, uint32_t method_size_bitmap) const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  if (method != nullptr) {
    // Paranoid check
    if (method->GetDexFile()->NumMethodIds() >= method->GetMethodIndex()) {
      return false;
    }
    uint32_t* static_analysis_method_info = GetStaticAnalysisMethodInfo((method->GetDexFile()->GetMethodId(method->GetMethodIndex())));
    if (static_analysis_method_info != nullptr) {
      return (*static_analysis_method_info & kMethodSizeMask) == method_size_bitmap;
    }
  }
  return false;
}

bool StaticAnalyzer::IsMethodSizeIn(const DexFile::MethodId* method_id, uint32_t method_size_bitmap) const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  if (method_id != nullptr) {
    uint32_t* static_analysis_method_info = GetStaticAnalysisMethodInfo(*method_id);
    if (static_analysis_method_info != nullptr) {
      return (*static_analysis_method_info & kMethodSizeMask) == method_size_bitmap;
    }
  }
  return false;
}

void StaticAnalyzer::AnalyzeMethod(mirror::ArtMethod* method,
    const DexFile& dex_file) {
  if (method->GetMethodIndex() >= dex_file.NumMethodIds()) {
    return;
  }
  const DexFile::MethodId& method_id = dex_file.GetMethodId(method->GetMethodIndex());
  // We should not be analyzing the same method twice.
  uint32_t* static_analysis_method_info = ReserveSpotStaticAnalysisMethodInfo(method_id);
  if (static_analysis_method_info == nullptr) {
    return;
  }
  StaticAnalysisPassDriver static_analysis_pass_driver(method, &dex_file, static_analysis_method_info);
  static_analysis_pass_driver.Launch();
  StaticAnalysisMemoryCleanup(static_analysis_method_info, method_id);
}

void StaticAnalyzer::AnalyzeMethod(verifier::MethodVerifier* verifier) {
  MethodReference ref = verifier->GetMethodReference();
  const DexFile::MethodId& method_id = ref.dex_file->GetMethodId(ref.dex_method_index);
  // We should not be analyzing the same method twice.
  uint32_t* static_analysis_method_info = ReserveSpotStaticAnalysisMethodInfo(method_id);
  if (static_analysis_method_info == nullptr) {
    return;
  }
  StaticAnalysisPassDriver static_analysis_pass_driver(verifier, static_analysis_method_info);
  static_analysis_pass_driver.Launch();
  StaticAnalysisMemoryCleanup(static_analysis_method_info, method_id);
}

void StaticAnalyzer::StaticAnalysisMemoryCleanup(uint32_t* static_analysis_method_info, const DexFile::MethodId& method_id) {
  // If after performing the different analysis, no important information was found, do not store anything.
  if (*static_analysis_method_info == kMethodNone) {
    // Clean up.
    delete static_analysis_method_info;
    {
      MutexLock mu(Thread::Current(), static_analysis_methods_info_lock_);
      static_analysis_methods_info_.erase(&method_id);
    }
    return;
  }
}

const std::string StaticAnalyzer::DumpAnalysis() {
  return StaticAnalysisPassDriver::DumpAnalysis();
}

void StaticAnalyzer::LogAnalysis() {
  LOG(INFO) << "Static Analyzer STATS: " << DumpAnalysis();
}

uint32_t* StaticAnalyzer::GetStaticAnalysisMethodInfo(const DexFile::MethodId& method_id) const LOCKS_EXCLUDED(static_analysis_methods_info_lock_) {
  {
    MutexLock mu(Thread::Current(), static_analysis_methods_info_lock_);
    StaticAnalysisMethodInfoTable::const_iterator it = static_analysis_methods_info_.find(&method_id);
    if (it == static_analysis_methods_info_.end()) {
      return nullptr;
    }
    CHECK(it->second != nullptr);
    return it->second;
  }
}

uint32_t* StaticAnalyzer::ReserveSpotStaticAnalysisMethodInfo(const DexFile::MethodId& method_id) LOCKS_EXCLUDED(static_analysis_methods_info_lock_) {
  {
    MutexLock mu(Thread::Current(), static_analysis_methods_info_lock_);
    StaticAnalysisMethodInfoTable::const_iterator it = static_analysis_methods_info_.find(&method_id);
    if (it != static_analysis_methods_info_.end()) {
      return nullptr;
    }
    uint32_t* static_analysis_method_info = new uint32_t;
    *(static_analysis_method_info) = kMethodNone;
    static_analysis_methods_info_.Put(&method_id, static_analysis_method_info);
    return static_analysis_method_info;
  }
}
}  // namespace art
