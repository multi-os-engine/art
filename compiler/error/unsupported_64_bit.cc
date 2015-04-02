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

#include "unsupported_64_bit.h"

#include <stdint.h>

namespace art {

const std::vector<uint8_t>* CreateTrampolineFor64(
    InstructionSet isa,
    EntryPointCallingConvention abi,
    ThreadOffset<8> offset) {
  UNUSED(isa);
  UNUSED(abi);
  UNUSED(offset);
  LOG(FATAL) << "64 bit unsupported";
  return nullptr;
}

Assembler* CreateAssembler64(InstructionSet instruction_set) {
  UNUSED(instruction_set);
  LOG(FATAL) << "64 bit unsupported";
  return nullptr;
}

ManagedRuntimeCallingConvention* CreateManagedRuntimeCallingConvention64(
      bool is_static,
      bool is_synchronized,
      const char* shorty,
      InstructionSet instruction_set) {
  UNUSED(is_static);
  UNUSED(is_synchronized);
  UNUSED(shorty);
  UNUSED(instruction_set);
  LOG(FATAL) << "64 bit unsupported";
  return nullptr;
}

JniCallingConvention* CreateJniCallingConvention64(bool is_static,
                                                   bool is_synchronized,
                                                   const char* shorty,
                                                   InstructionSet instruction_set)  {
  UNUSED(is_static);
  UNUSED(is_synchronized);
  UNUSED(shorty);
  UNUSED(instruction_set);
  LOG(FATAL) << "64 bit unsupported";
  return nullptr;
}

CodeGenerator* GetCodeGenerator64(HGraph* graph,
                                  InstructionSet instruction_set,
                                  const InstructionSetFeatures& isa_features,
                                  const CompilerOptions& compiler_options) {
  UNUSED(graph);
  UNUSED(instruction_set);
  UNUSED(isa_features);
  UNUSED(compiler_options);
  LOG(FATAL) << "64 bit unsupported";
  return nullptr;
}

Mir2Lir* CreateCodeGenerator64(CompilationUnit* cu) {
  UNUSED(cu);
  LOG(FATAL) << "64 bit unsupported";
  return nullptr;
}

namespace linker {

  RelativePatcher* CreateRelativePatcher64(InstructionSet instruction_set,
                                           RelativePatcherTargetProvider* provider,
                                           const InstructionSetFeatures* features) {
    UNUSED(instruction_set);
    UNUSED(provider);
    UNUSED(features);
    LOG(FATAL) << "64 bit unsupported";
    return nullptr;
  }
}  // namespace linker

}  // namespace art
