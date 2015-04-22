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

#include "dex/quick/quick_compiler.h"
#include "dex/pass_manager.h"
#include "dex/verification_results.h"
#include "dex/quick/dex_file_to_method_inliner_map.h"
#include "runtime/dex_file.h"
#include "driver/compiler_options.h"
#include "driver/compiler_driver.h"
#include "codegen_x86.h"
#include "gtest/gtest.h"

namespace art {

class QuickAssembleX86Test : public testing::Test {
 protected:
  QuickAssembleX86Test()
      : isa_(kX86),
        pool_(),
        compiler_options_(
            CompilerOptions::kDefaultCompilerFilter,
            CompilerOptions::kDefaultHugeMethodThreshold,
            CompilerOptions::kDefaultLargeMethodThreshold,
            CompilerOptions::kDefaultSmallMethodThreshold,
            CompilerOptions::kDefaultTinyMethodThreshold,
            CompilerOptions::kDefaultNumDexMethodsThreshold,
            false,
            CompilerOptions::kDefaultTopKProfileThreshold,
            false,
            false,
            false,
            false,
            false,
            false,
            false,
            nullptr,
            new PassManagerOptions(),
            nullptr,
            false),
        verification_results_(&compiler_options_),
        method_inliner_map_(),
        compiler_driver_(
            &compiler_options_,
            &verification_results_,
            &method_inliner_map_,
            Compiler::kQuick,
            isa_,
            nullptr,
            false,
            nullptr,
            nullptr,
            nullptr,
            0,
            false,
            false,
            "",
            0,
            -1,
            ""),
        cu_(&pool_, isa_, &compiler_driver_, nullptr),
        allocator_() {
    DexFile::CodeItem* code_item = static_cast<DexFile::CodeItem*>(
      cu_.arena.Alloc(sizeof(DexFile::CodeItem), kArenaAllocMisc));
    memset(code_item, 0, sizeof(DexFile::CodeItem));
    cu_.mir_graph.reset(new MIRGraph(&cu_, &cu_.arena));
    cu_.mir_graph->current_code_item_ = code_item;
    cu_.cg.reset(QuickCompiler::GetCodeGenerator(&cu_, nullptr));
  }

  void TestAddpd() {
    X86Mir2Lir *m2l = static_cast<X86Mir2Lir*>(cu_.cg.get());
    m2l->CompilerInitializeRegAlloc();

    // Create a vector MIR
    MIR* mir = cu_.mir_graph->NewMIR();
    mir->dalvikInsn.opcode = static_cast<Instruction::Code>(kMirOpPackedAddition);
    mir->dalvikInsn.vA = 0;  // destination and source
    mir->dalvikInsn.vB = 1;  // source
    int vector_size = 128;
    int vector_type = kDouble;
    mir->dalvikInsn.vC = (vector_type << 16) | vector_size;  // type size
    m2l->GenAddVector(mir);
    m2l->AssembleLIR();

    // 0: 66 0f 58 c1    addpd  %xmm1,%xmm0
    uint8_t expected_insn[] = {0x66, 0x0f, 0x58, 0xc1};
    uint32_t expected_size = 4;
    EXPECT_EQ(expected_size, m2l->CodeBufferSizeInBytes());
    for (uint32_t i = 0; i < expected_size; i++) {
      EXPECT_EQ(expected_insn[i], m2l->code_buffer_[i]);
    }
  }

  InstructionSet isa_;
  ArenaPool pool_;
  CompilerOptions compiler_options_;
  VerificationResults verification_results_;
  DexFileToMethodInlinerMap method_inliner_map_;
  CompilerDriver compiler_driver_;
  CompilationUnit cu_;
  std::unique_ptr<ScopedArenaAllocator> allocator_;
};

TEST_F(QuickAssembleX86Test, InstructionSet) {
  EXPECT_EQ(kX86, cu_.instruction_set);
}

TEST_F(QuickAssembleX86Test, Addpd) {
  TestAddpd();
}

}  // namespace art
