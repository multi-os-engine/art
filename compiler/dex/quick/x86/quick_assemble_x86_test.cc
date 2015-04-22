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
#include "utils/assembler_test_base.h"

namespace art {

class QuickAssembleX86Test : public testing::Test {
 protected:
  X86Mir2Lir* Prepare(InstructionSet target) {
    isa_ = target;
    pool_.reset(new ArenaPool());
    compiler_options_.reset(new CompilerOptions(
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
        false));
    verification_results_.reset(new VerificationResults(compiler_options_.get()));
    method_inliner_map_.reset(new DexFileToMethodInlinerMap());
    compiler_driver_.reset(new CompilerDriver(
        compiler_options_.get(),
        verification_results_.get(),
        method_inliner_map_.get(),
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
        ""));
    cu_.reset(new CompilationUnit(pool_.get(), isa_, compiler_driver_.get(), nullptr));
    DexFile::CodeItem* code_item = static_cast<DexFile::CodeItem*>(
        cu_->arena.Alloc(sizeof(DexFile::CodeItem), kArenaAllocMisc));
    memset(code_item, 0, sizeof(DexFile::CodeItem));
    cu_->mir_graph.reset(new MIRGraph(cu_.get(), &cu_->arena));
    cu_->mir_graph->current_code_item_ = code_item;
    cu_->cg.reset(QuickCompiler::GetCodeGenerator(cu_.get(), nullptr));

    test_helper_.reset(new AssemblerTestInfrastructure(
        isa_ == kX86 ? "x86" : "x86_64",
        "as",
        isa_ == kX86 ? " --32" : "",
        "objdump",
        " -h",
        "objdump",
        isa_ == kX86 ?
            " -D -bbinary -mi386 --no-show-raw-insn" :
            " -D -bbinary -mi386:x86-64 -Mx86-64,addr64,data32 --no-show-raw-insn",
        nullptr));

    X86Mir2Lir* m2l = static_cast<X86Mir2Lir*>(cu_->cg.get());
    m2l->CompilerInitializeRegAlloc();
    return m2l;
  }

  void Release() {
    cu_.reset();
    compiler_driver_.reset();
    method_inliner_map_.reset();
    verification_results_.reset();
    compiler_options_.reset();
    pool_.reset();

    test_helper_.reset();
  }

  typedef void (X86Mir2Lir::*AsmFn)(MIR*);

  void TestVectorFn(InstructionSet target,
                    Instruction::Code opcode,
                    AsmFn f,
                    std::string inst_string) {
    X86Mir2Lir *m2l = Prepare(target);

    // Create a vector MIR.
    MIR* mir = cu_->mir_graph->NewMIR();
    mir->dalvikInsn.opcode = opcode;
    mir->dalvikInsn.vA = 0;  // Destination and source.
    mir->dalvikInsn.vB = 1;  // Source.
    int vector_size = 128;
    int vector_type = kDouble;
    mir->dalvikInsn.vC = (vector_type << 16) | vector_size;  // Type size.
    (m2l->*f)(mir);
    m2l->AssembleLIR();

    std::string gcc_asm = inst_string + " %xmm1, %xmm0\n";
    // Need a "base" std::vector.
    std::vector<uint8_t> buffer(m2l->code_buffer_.begin(), m2l->code_buffer_.end());
    test_helper_->Driver(buffer, gcc_asm, inst_string);

    Release();
  }

  void TearDown() OVERRIDE {
    Release();
  }

  bool CheckTools(InstructionSet target) {
    Prepare(target);
    bool result = test_helper_->CheckTools();
    Release();
    return result;
  }

  // Tests are member functions as many of the assembler functions are protected or private,
  // and it would be inelegant to define ART_FRIEND_TEST for all the tests.

  void TestAddpd() {
    TestVectorFn(kX86,
                 static_cast<Instruction::Code>(kMirOpPackedAddition),
                 &X86Mir2Lir::GenAddVector,
                 "addpd");
    TestVectorFn(kX86_64,
                 static_cast<Instruction::Code>(kMirOpPackedAddition),
                 &X86Mir2Lir::GenAddVector,
                 "addpd");
  }

  void TestSubpd() {
    TestVectorFn(kX86,
                 static_cast<Instruction::Code>(kMirOpPackedSubtract),
                 &X86Mir2Lir::GenSubtractVector,
                 "subpd");
    TestVectorFn(kX86_64,
                 static_cast<Instruction::Code>(kMirOpPackedSubtract),
                 &X86Mir2Lir::GenSubtractVector,
                 "subpd");
  }

  void TestMulpd() {
    TestVectorFn(kX86,
                 static_cast<Instruction::Code>(kMirOpPackedMultiply),
                 &X86Mir2Lir::GenMultiplyVector,
                 "mulpd");
    TestVectorFn(kX86_64,
                 static_cast<Instruction::Code>(kMirOpPackedMultiply),
                 &X86Mir2Lir::GenMultiplyVector,
                 "mulpd");
  }

 private:
  InstructionSet isa_;
  std::unique_ptr<ArenaPool> pool_;
  std::unique_ptr<CompilerOptions> compiler_options_;
  std::unique_ptr<VerificationResults> verification_results_;
  std::unique_ptr<DexFileToMethodInlinerMap> method_inliner_map_;
  std::unique_ptr<CompilerDriver> compiler_driver_;
  std::unique_ptr<CompilationUnit> cu_;
  std::unique_ptr<AssemblerTestInfrastructure> test_helper_;
};

TEST_F(QuickAssembleX86Test, CheckTools) {
  ASSERT_TRUE(CheckTools(kX86)) << "x86 tools not found.";
  ASSERT_TRUE(CheckTools(kX86_64)) << "x86_64 tools not found.";
}

#define DECLARE_TEST(name)             \
  TEST_F(QuickAssembleX86Test, name) { \
    Test ## name();                    \
  }

DECLARE_TEST(Addpd)
DECLARE_TEST(Subpd)
DECLARE_TEST(Mulpd)

}  // namespace art
