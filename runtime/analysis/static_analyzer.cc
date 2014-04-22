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

#include "analysis/method_static_analysis.h"
#include "analysis/static_analysis_pass.h"
#include "analysis/static_analyzer.h"
#include "mirror/art_method-inl.h"
#include "mirror/dex_cache-inl.h"
namespace art {
namespace {  // anonymous namespace

/**
 * @brief Helper function to create a single instance of a given Pass and can be shared across
 * the threads.
 */
template <typename StaticAnalysisPassType>
StaticAnalysisPass* GetPassInstance() {
  static StaticAnalysisPassType pass;
  return &pass;
}

}  // anonymous namespace

StaticAnalyzer::StaticAnalyzer()
  : static_analysis_methods_info_lock_("static analysis methods info lock") {
  CreatePasses();
}
StaticAnalyzer::~StaticAnalyzer() {
  Thread* self = Thread::Current();
  {
    MutexLock mu(self, static_analysis_methods_info_lock_);
    STLDeleteValues(&static_analysis_methods_info_);
  }
}
bool StaticAnalyzer::IsMethodSizeIn(mirror::ArtMethod* method, uint32_t method_size_bitmap) const {
  if (method != nullptr) {
    uint32_t* static_analysis_method_info = GetStaticAnalysisMethodInfo(method);
    if (static_analysis_method_info != nullptr) {
      return (*static_analysis_method_info & kMethodSizeMask) == method_size_bitmap;
    }
  }
  return false;
}
void StaticAnalyzer::InsertPass(StaticAnalysisPass* new_pass) {
  DCHECK(new_pass != nullptr);
  DCHECK(new_pass->GetName() != nullptr && new_pass->GetName()[0] != 0);
  // It is an error to override an existing pass.

  DCHECK(GetPass(new_pass->GetName()) == nullptr)
      << "Pass name " << new_pass->GetName() << " already used.";

  // Now add to the list.
  pass_list_.push_back(new_pass);
}

void StaticAnalyzer::CreatePasses() {
  /*
   * Create the pass list. These passes are mutable and are shared across the threads.
   *
   * Advantage you can change their internal states.
   * Disadvantage the states have to be of type art::Atomic and only atomic operations can be used
   *  or states must be protected by locks.
   */
  static StaticAnalysisPass* passes[] = {
      GetPassInstance<MethodLogisticsAnalysis>(),
      GetPassInstance<MethodMiscLogisticsAnalysis>(),
      GetPassInstance<MethodSizeAnalysis>(),
      GetPassInstance<MethodOpcodeAnalysis>(),
  };

  // Insert each pass into the list via the InsertPass method.
  pass_list_.reserve(arraysize(passes));
  for (StaticAnalysisPass* pass : passes) {
    InsertPass(pass);
  }
}

void StaticAnalyzer::AnalyzeMethod(mirror::ArtMethod* method,
    const DexFile& dex_file) {
  // We should not be analyzing the same method twice.
  if (GetStaticAnalysisMethodInfo(method) != nullptr) {
    return;
  }
  uint32_t static_analysis_method_info = 0x0;
  // Loop through different passes and bitwise OR the returned analysis info with with the static_analysis_method_info variable.
  for (StaticAnalysisPass* cur_pass : pass_list_) {
    static_analysis_method_info |= cur_pass->PerformAnalysis(method, dex_file);
  }
  // If after performing the different analysis, no important information was found, do not store anything.
  if (static_analysis_method_info == kMethodNone) {
    return;
  }
  {
    MutexLock mu(Thread::Current(), static_analysis_methods_info_lock_);
    static_analysis_methods_info_.Put(method, new uint32_t(static_analysis_method_info));
  }
}

StaticAnalysisPass* StaticAnalyzer::GetPass(const char* name) {
  for (StaticAnalysisPass* cur_pass : pass_list_) {
    if (strcmp(name, cur_pass->GetName()) == 0) {
      return cur_pass;
    }
  }
  return nullptr;
}

const std::string StaticAnalyzer::DumpTimedAnalysis(uint32_t first_time, uint32_t second_time) {
  std::stringstream ss;
  ss << first_time << " first timing. " << second_time << " second timing. " << DumpAnalysis();
  return ss.str();
}

const std::string StaticAnalyzer::DumpAnalysis() {
  std::stringstream ss;
  for (StaticAnalysisPass* cur_pass : pass_list_) {
    cur_pass->DumpPassAnalysis(ss);
  }
  return ss.str();
}

void StaticAnalyzer::LogTimedAnalysis(uint32_t first_time, uint32_t second_time) {
  LOG(INFO) << "Static Analyzer STATS: " << DumpTimedAnalysis(first_time, second_time);
}

void StaticAnalyzer::LogAnalysis() {
  LOG(INFO) << "Static Analyzer STATS: " << DumpAnalysis();
}

uint32_t* StaticAnalyzer::GetStaticAnalysisMethodInfo(mirror::ArtMethod* method) const {
  MutexLock mu(Thread::Current(), static_analysis_methods_info_lock_);
  StaticAnalysisMethodInfoTable::const_iterator it = static_analysis_methods_info_.find(method);
  if (it == static_analysis_methods_info_.end()) {
    return nullptr;
  }
  CHECK(it->second != nullptr);
  return it->second;
}
}  // namespace art
