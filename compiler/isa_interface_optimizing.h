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

#ifndef ART_COMPILER_ISA_INTERFACE_OPTIMIZING_H_
#define ART_COMPILER_ISA_INTERFACE_OPTIMIZING_H_

#include "driver/compiler_driver.h"
#include "optimizing/code_generator.h"

namespace art {

CodeGenerator* GetCodeGenerator(HGraph* graph,
                                InstructionSet instruction_set,
                                const InstructionSetFeatures& isa_features,
                                const CompilerOptions& compiler_options);

}  // namespace art

#endif  // ART_COMPILER_ISA_INTERFACE_OPTIMIZING_H_
