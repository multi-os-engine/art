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

#ifndef ART_COMPILER_ERROR_UNSUPPORTED_64_BIT_H_
#define ART_COMPILER_ERROR_UNSUPPORTED_64_BIT_H_

#include "driver/compiler_driver.h"

namespace art {

class Assembler;
class CodeGenerator;
class CompilerOptions;
class HGraph;
class InstructionSetFeatures;
class JniCallingConvention;
class ManagedRuntimeCallingConvention;
class Mir2Lir;

const std::vector<uint8_t>* CreateTrampolineFor64(
    InstructionSet isa,
    EntryPointCallingConvention abi,
    ThreadOffset<8> offset);

Assembler* CreateAssembler64(InstructionSet instruction_set);

ManagedRuntimeCallingConvention* CreateManagedRuntimeCallingConvention64(
    bool is_static,
    bool is_synchronized,
    const char* shorty,
    InstructionSet instruction_set);

JniCallingConvention* CreateJniCallingConvention64(bool is_static,
                                                   bool is_synchronized,
                                                   const char* shorty,
                                                   InstructionSet instruction_set);

CodeGenerator* GetCodeGenerator64(HGraph* graph,
                                  InstructionSet instruction_set,
                                  const InstructionSetFeatures& isa_features,
                                  const CompilerOptions& compiler_options);

Mir2Lir* CreateCodeGenerator64(CompilationUnit* cu);

namespace linker {
  class RelativePatcher;
  class RelativePatcherTargetProvider;

  RelativePatcher* CreateRelativePatcher64(
      InstructionSet instruction_set,
      RelativePatcherTargetProvider* provider,
      const InstructionSetFeatures* features);

}  // namespace linker
}  // namespace art

#endif  // ART_COMPILER_ERROR_UNSUPPORTED_64_BIT_H_
