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

#ifndef ART_COMPILER_DEX_SELECTIVITY_H_
#define ART_COMPILER_DEX_SELECTIVITY_H_
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
 */

class Selectivity {
 public:
  /**
   * @brief Sets the function pointer to execute during the PreCompileSummary
   *  function within the PreCompile stage.
   */
  static void SetPreCompileSummaryLogic(bool (*function)(CompilerDriver* driver,
                                                         VerificationResults* verification_results));

  /**
   * @brief Calls the function pointer to execute during the PreCompileSummary
   *  function within the PreCompile stage.
   */
  static bool PreCompileSummaryLogic(CompilerDriver* driver,
                                     VerificationResults* verification_results);

  /**
   * @brief Sets the function pointer to execute during the CompileClass
   *  function within the Compile stage.
   */
  static void SetSkipClassCompilation(bool (*function)(const DexFile& dex_file,
                                                       const DexFile::ClassDef& class_def));

  /**
   * @brief Calls the function pointer to execute during the CompileClass function
   *  within the Compile stage.
   */
  static bool SkipClassCompilation(const DexFile& dex_file,
                                   const DexFile::ClassDef& class_def);

  /**
   * @brief Sets the function pointer to execute during the CompileMethod function
   *  within the Compile stage.
   */
  static void SetSkipMethodCompilation(bool (*function)(const DexFile::CodeItem* code_item,
                                                        uint32_t method_idx,
                                                        uint32_t* access_flags,
                                                        uint16_t* class_def_idx,
                                                        const DexFile& dex_file,
                                                        DexToDexCompilationLevel* dex_to_dex_compilation_level));

  /**
   * @brief Calls the function pointer to execute during the CompileMethod function
   * within the Compile stage.
   */
  static bool SkipMethodCompilation(const DexFile::CodeItem* code_item, uint32_t method_idx,
                                    uint32_t* access_flags, uint16_t* class_def_idx,
                                    const DexFile& dex_file,
                                    DexToDexCompilationLevel* dex_to_dex_compilation_level);

 private:
  static bool (*precompile_summary_logic)(CompilerDriver* driver, VerificationResults* verifier);
  static bool (*skip_class_compilation)(const DexFile& dex_file, const DexFile::ClassDef&);
  static bool (*skip_method_compilation)(const DexFile::CodeItem* code_item, uint32_t method_idx,
                                         uint32_t* access_flags, uint16_t* class_def_idx,
                                         const DexFile& dex_file,
                                         DexToDexCompilationLevel* dex_to_dex_compilation_level);
};
}  // namespace art
#endif  // ART_COMPILER_DEX_SELECTIVITY_H_
