/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include "compiler_options.h"

#include "dex/pass_manager.h"

namespace art {

CompilerOptions::CompilerOptions()
    : compiler_filter_(kDefaultCompilerFilter),
      huge_method_threshold_(kDefaultHugeMethodThreshold),
      large_method_threshold_(kDefaultLargeMethodThreshold),
      small_method_threshold_(kDefaultSmallMethodThreshold),
      tiny_method_threshold_(kDefaultTinyMethodThreshold),
      num_dex_methods_threshold_(kDefaultNumDexMethodsThreshold),
      generate_gdb_information_(false),
      include_patch_information_(kDefaultIncludePatchInformation),
      top_k_profile_threshold_(kDefaultTopKProfileThreshold),
      include_debug_symbols_(kDefaultIncludeDebugSymbols),
      implicit_null_checks_(true),
      implicit_so_checks_(true),
      implicit_suspend_checks_(false),
      compile_pic_(false),
      verbose_methods_(nullptr),
      pass_manager_options_(new PassManagerOptions),
      init_failure_output_(nullptr) {
}

CompilerOptions::~CompilerOptions() {
  delete pass_manager_options_;
}

}  // namespace art
