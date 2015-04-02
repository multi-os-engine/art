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

#ifndef ART_COMPILER_ISA_INTERFACE_QUICK_H_
#define ART_COMPILER_ISA_INTERFACE_QUICK_H_

#include "dex/compiler_ir.h"
#include "dex/quick/mir_to_lir.h"
#include "driver/compiler_driver.h"
#include "jni/quick/calling_convention.h"
#include "linker/relative_patcher.h"
#include "utils/assembler.h"

namespace art {

Assembler* CreateAssembler(InstructionSet instruction_set);

JniCallingConvention* CreateJniCallingConvention(bool is_static, bool is_synchronized,
                                                 const char* shorty,
                                                 InstructionSet instruction_set);

ManagedRuntimeCallingConvention* CreateManagedRuntimeCallingConvention(
    bool is_static, bool is_synchronized, const char* shorty, InstructionSet instruction_set);

const std::vector<uint8_t>* CreateTrampolineFor32(InstructionSet isa,
                                                  EntryPointCallingConvention abi,
                                                  ThreadOffset<4> offset);

const std::vector<uint8_t>* CreateTrampolineFor64(InstructionSet isa,
                                                  EntryPointCallingConvention abi,
                                                  ThreadOffset<8> offset);

Mir2Lir* ArmCodeGenerator(CompilationUnit* const cu, MIRGraph* const mir_graph,
                          ArenaAllocator* const arena);

Mir2Lir* Arm64CodeGenerator(CompilationUnit* const cu, MIRGraph* const mir_graph,
                            ArenaAllocator* const arena);

Mir2Lir* MipsCodeGenerator(CompilationUnit* const cu, MIRGraph* const mir_graph,
                           ArenaAllocator* const arena);

Mir2Lir* X86CodeGenerator(CompilationUnit* const cu, MIRGraph* const mir_graph,
                          ArenaAllocator* const arena);


Mir2Lir* GetIsaCodeGenerator(CompilationUnit* cu);

namespace linker {

RelativePatcher* CreateRelativePatcher(InstructionSet instruction_set,
                                       RelativePatcherTargetProvider* provider,
                                       const InstructionSetFeatures* features);

}  // namespace linker

}  // namespace art

#endif  // ART_COMPILER_ISA_INTERFACE_QUICK_H_
