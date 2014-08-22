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

#ifndef ART_COMPILER_DRIVER_SELECTIVITY_H_
#define ART_COMPILER_DRIVER_SELECTIVITY_H_
#include "dex/pass_driver_me.h"
#include "driver/compiler_driver.h"
#include "dex_file.h"
namespace art {

/**
 * The purpose of this class is to provide a common set of APIs that allow one to call custom functions.
 * Phase - Purpose of associated actions:
 * 1) PreCompileSummary - Post Resolution and Verification action; Action to affect whole APK and
 *                        modify existing static variables.
 * 2) Skip Class Compilation - Set logic to decide compilation per-class.
 * 3) Skip Method Compilation - Set logic to decide compilation per-method.
 * 4) Analyze Resolved Methods - Set logic to analyze resolved methods.
 * 5) Analyze Verified Methods - Set logic to analyze verified methods.
 * (Note: 4 & 5 are added because the set of data used in 4 != the set of data used in 5)
 * 6) Dump Selectivity Analysis - Logic to provide verbosity in terms of results gained through our analysis.
 * 7) Toggle Analysis - Determines if we should perform any analysis and adjust any passes.
 * 8) Original Compiler Filter Level - Logic to store compiler filter to use for initializing.
 * 9) Used Filter Level - Logic to retrieve compiler filter that was used after analysis.
 */

class Selectivity {
 public:
  /**
   * @brief Get Instance of Selectivity.
   */
  static Selectivity* GetInstance() {
    CHECK(instance_ != nullptr);
    return instance_;
  }

  /**
   * @brief Set Instance of Selectivity.
   */
  static void SetInstance(Selectivity* instance) {
    instance_ = instance;
  }

  /**
   * @brief Calls the function pointer to execute during the PreCompileSummary
   *  function within the PreCompile stage.
   */
  virtual bool PreCompileSummaryLogic(CompilerDriver* driver,
                                     VerificationResults* verification_results) = 0;

  /**
   * @brief Calls the function pointer to execute during the CompileClass function
   *  within the Compile stage.
   */
  virtual bool SkipClassCompile(const DexFile& dex_file,
                                const DexFile::ClassDef& class_def) = 0;

  /**
   * @brief Calls the function pointer to execute during the CompileMethod function
   * within the Compile stage.
   */
  virtual bool SkipMethodCompile(const DexFile::CodeItem* code_item, uint32_t method_idx,
                                 uint32_t* access_flags, uint16_t* class_def_idx,
                                 const DexFile& dex_file,
                                 DexToDexCompilationLevel* dex_to_dex_compilation_level) = 0;

  /**
   * @brief Calls the function pointer to perform analysis on a method after it is resolved.
   */
  virtual void AnalyzeResolvedMethod(mirror::ArtMethod* method, const DexFile& dex_file) = 0;

  /**
   * @brief Calls the function pointer to perform analysis on a method after it is verified.
   */
  virtual void AnalyzeVerifiedMethod(verifier::MethodVerifier* verifier) = 0;

  /**
   * @brief Calls the function pointer to dump any statistics gatthered at the end
   *  of the CompileAll stage if and only if compiler driver is set to dump stats.
   */
  virtual void DumpSelectivityStats() = 0;

  /**
   * @brief Calls the function pointer that performs the logic of whether to
   *  do analysis or not.
   */
  virtual void ToggleAnalysis(bool setting, std::string disable_passes) = 0;

  /*
   * @brief Getter & Setter for the original compiler filter passed into
   * dex2oat before any adjustments.
   */
  CompilerOptions::CompilerFilter GetOriginalCompilerFilter() {
    return original_compiler_filter_;
  }

  void SetOriginalCompilerFilter(CompilerOptions::CompilerFilter filter) {
    original_compiler_filter_ = filter;
  }

  /*
   * @brief Getter & Setter for the used compiler filter chosen by selectivty system.
   */
  CompilerOptions::CompilerFilter GetUsedCompilerFilter() {
    return used_compiler_filter_;
  }

  void SetUsedCompilerFilter(CompilerOptions::CompilerFilter filter) {
    used_compiler_filter_ = filter;
  }
 protected:
  Selectivity() {}
  ~Selectivity() {}
  static Selectivity* instance_;
  CompilerOptions::CompilerFilter used_compiler_filter_;
  CompilerOptions::CompilerFilter original_compiler_filter_;
};

class DefaultSelectivity : public Selectivity {
 public:
  static Selectivity* GetInstance();
  bool PreCompileSummaryLogic(CompilerDriver* driver,
                              VerificationResults* verification_results);
  bool SkipClassCompile(const DexFile& dex_file,
                        const DexFile::ClassDef& class_def);
  bool SkipMethodCompile(const DexFile::CodeItem* code_item, uint32_t method_idx,
                         uint32_t* access_flags, uint16_t* class_def_idx,
                         const DexFile& dex_file,
                         DexToDexCompilationLevel* dex_to_dex_compilation_level);
  void AnalyzeResolvedMethod(mirror::ArtMethod* method, const DexFile& dex_file);
  void AnalyzeVerifiedMethod(verifier::MethodVerifier* verifier);
  void DumpSelectivityStats();
  void ToggleAnalysis(bool setting, std::string disable_passes);
 protected:
  DefaultSelectivity() {}
  ~DefaultSelectivity() {}
};
}  // namespace art
#endif  // ART_COMPILER_DRIVER_SELECTIVITY_H_
