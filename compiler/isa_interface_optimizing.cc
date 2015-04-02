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

#include "isa_interface_optimizing.h"

#include "optimizing/code_generator_arm.h"
#include "optimizing/code_generator_arm64.h"
#include "optimizing/code_generator_x86.h"
#include "optimizing/code_generator_x86_64.h"

namespace art {

CodeGenerator* GetCodeGenerator(HGraph* graph,
                                InstructionSet instruction_set,
                                const InstructionSetFeatures& isa_features,
                                const CompilerOptions& compiler_options) {
  switch (instruction_set) {
    case kArm:
    case kThumb2: {
      return GetCodeGeneratorARM(graph,
          isa_features,
          compiler_options);
    }
    case kArm64: {
      return GetCodeGeneratorARM64(graph,
          isa_features,
          compiler_options);
    }
    case kMips:
      return nullptr;
    case kX86: {
      return GetCodeGeneratorX86(graph,
           isa_features,
           compiler_options);
    }
    case kX86_64: {
      return GetCodeGeneratorX86_64(graph,
          isa_features,
          compiler_options);
    }
    default:
      return nullptr;
  }
}

}  // namespace art
