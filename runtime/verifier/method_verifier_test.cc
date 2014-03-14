/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "method_verifier.h"

#include <stdio.h>

#include "UniquePtr.h"
#include "class_linker.h"
#include "common_runtime_test.h"
#include "dex_file.h"
#include "mirror/art_method-inl.h"

namespace art {
namespace verifier {

class MethodVerifierTest : public CommonRuntimeTest {
 protected:
  void VerifyClass(const std::string& descriptor)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    ASSERT_TRUE(descriptor != NULL);
    mirror::Class* klass = class_linker_->FindSystemClass(Thread::Current(), descriptor.c_str());

    // Verify the class
    std::string error_msg;
    ASSERT_TRUE(MethodVerifier::VerifyClass(klass, true, &error_msg) == MethodVerifier::kNoFailure)
        << error_msg;
  }

  void VerifyDexFile(const DexFile* dex)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    ASSERT_TRUE(dex != NULL);

    // Verify all the classes defined in this file
    for (size_t i = 0; i < dex->NumClassDefs(); i++) {
      const DexFile::ClassDef& class_def = dex->GetClassDef(i);
      const char* descriptor = dex->GetClassDescriptor(class_def);
      VerifyClass(descriptor);
    }
  }
};

TEST_F(MethodVerifierTest, LibCore) {
  ScopedObjectAccess soa(Thread::Current());
  VerifyDexFile(java_lang_dex_file_);
}

static VRegKind GetVregKind(int32_t vreg, const std::vector<int32_t>& kinds) {
  const size_t index = vreg * 2;
  CHECK(kinds.size() > index);
  return static_cast<VRegKind>(kinds.at(index));
}

// Check vregs types are correct.
// const/4 v0, #3
// if-lez v1, #0
// add-2addr v0, v1
// return v0
TEST_F(MethodVerifierTest, IntConstantTypePropagation) {
  ScopedObjectAccess soa(Thread::Current());
  SirtRef<mirror::ClassLoader> class_loader(soa.Self(),
                                            soa.Decode<mirror::ClassLoader*>(LoadDex("MethodVerifier")));

  // Find test method "int verifyIntTypes(int)".
  mirror::Class* testClass = class_linker_->FindClass(soa.Self(), "LMethodVerifier;", class_loader);
  ASSERT_TRUE(testClass != nullptr);
  mirror::ArtMethod* m = testClass->FindDeclaredDirectMethod("verifyIntTypes", "(I)I");
  ASSERT_TRUE(m != nullptr);

  MethodHelper mh(m);
  const DexFile::CodeItem* code_item = mh.GetCodeItem();
  SirtRef<mirror::DexCache> dex_cache(soa.Self(), mh.GetDexCache());
  verifier::MethodVerifier verifier(&mh.GetDexFile(), &dex_cache, &class_loader,
                                    &mh.GetClassDef(), code_item, m->GetDexMethodIndex(), m,
                                    m->GetAccessFlags(), false, true);
  bool verified = verifier.Verify();
  ASSERT_TRUE(verified);
  // verifier.Dump(LOG(INFO));

  // Ensure we test the DEX code we expect.
  const int32_t kVreg0 = 0;
  const int32_t kVreg1 = 1;
  ASSERT_TRUE(code_item != nullptr);
  ASSERT_EQ(2U, code_item->registers_size_);  // 2 int registers.
  ASSERT_EQ(1U, code_item->ins_size_);        // 1 int argument register.

  const Instruction* instr = Instruction::At(code_item->insns_);
  ASSERT_EQ(instr->Opcode(), Instruction::CONST_4);
  ASSERT_EQ(instr->VRegA(), kVreg0);
  const uint32_t kConst4DexPc = instr->GetDexPc(code_item->insns_);
  instr = instr->Next();
  ASSERT_EQ(instr->Opcode(), Instruction::IF_LEZ);
  ASSERT_EQ(instr->VRegA(), kVreg1);
  const uint32_t kIfLezDexPc = instr->GetDexPc(code_item->insns_);
  instr = instr->Next();
  ASSERT_EQ(instr->Opcode(), Instruction::ADD_INT_2ADDR);
  ASSERT_EQ(instr->VRegA(), kVreg0);
  ASSERT_EQ(instr->VRegB(), kVreg1);
  const uint32_t kAddInt2AddrDexPc = instr->GetDexPc(code_item->insns_);
  instr = instr->Next();
  ASSERT_EQ(instr->Opcode(), Instruction::RETURN);
  ASSERT_EQ(instr->VRegA(), kVreg0);
  const uint32_t kReturnDexPc = instr->GetDexPc(code_item->insns_);
  instr = instr->Next();
  ASSERT_EQ(reinterpret_cast<const uint16_t*>(instr),
            code_item->insns_ + code_item->insns_size_in_code_units_);

  {
    const std::vector<int32_t> kinds(verifier.DescribeVRegs(kConst4DexPc));
    EXPECT_EQ(kUndefined, GetVregKind(kVreg0, kinds));
    EXPECT_EQ(kIntVReg, GetVregKind(kVreg1, kinds));
  }
  {
    const std::vector<int32_t> kinds(verifier.DescribeVRegs(kIfLezDexPc));
    EXPECT_EQ(kImpreciseConstant, GetVregKind(kVreg0, kinds));
    EXPECT_EQ(kIntVReg, GetVregKind(kVreg1, kinds));
  }
  // TODO we disable this test because there is no register line at DEX pc 0x3. It seems
  // register line is absent because it's the same than previous instruction (at DEX pc 0x1).
  if (false) {
    const std::vector<int32_t> kinds(verifier.DescribeVRegs(kAddInt2AddrDexPc));
    EXPECT_EQ(kImpreciseConstant, GetVregKind(kVreg0, kinds));
    EXPECT_EQ(kIntVReg, GetVregKind(kVreg1, kinds));
  }
  {
    const std::vector<int32_t> kinds(verifier.DescribeVRegs(kReturnDexPc));
    EXPECT_EQ(kIntVReg, GetVregKind(kVreg0, kinds));
    EXPECT_EQ(kUndefined, GetVregKind(kVreg1, kinds));
  }
}

TEST_F(MethodVerifierTest, LongConstantTypePropagation) {
  ScopedObjectAccess soa(Thread::Current());
  SirtRef<mirror::ClassLoader> class_loader(soa.Self(),
                                            soa.Decode<mirror::ClassLoader*>(LoadDex("MethodVerifier")));

  // Find test method "int verifyIntTypes(int)".
  mirror::Class* testClass = class_linker_->FindClass(soa.Self(), "LMethodVerifier;", class_loader);
  ASSERT_TRUE(testClass != nullptr);
  mirror::ArtMethod* m = testClass->FindDeclaredDirectMethod("verifyLongTypes", "(J)J");
  ASSERT_TRUE(m != nullptr);

  MethodHelper mh(m);
  const DexFile::CodeItem* code_item = mh.GetCodeItem();
  SirtRef<mirror::DexCache> dex_cache(soa.Self(), mh.GetDexCache());
  verifier::MethodVerifier verifier(&mh.GetDexFile(), &dex_cache, &class_loader,
                                    &mh.GetClassDef(), code_item, m->GetDexMethodIndex(), m,
                                    m->GetAccessFlags(), false, true);
  bool verified = verifier.Verify();
  ASSERT_TRUE(verified);
  // verifier.Dump(LOG(INFO));

  // Ensure we test the DEX code we expect.
  const int32_t kVreg0 = 0;
  const int32_t kVreg1 = 1;
  const int32_t kVreg2 = 2;
  const int32_t kVreg3 = 2;
  const int32_t kVreg4 = 4;
  const int32_t kVreg5 = 4;
  ASSERT_TRUE(code_item != nullptr);
  ASSERT_EQ(6U, code_item->registers_size_);  // 3 long registers.
  ASSERT_EQ(2U, code_item->ins_size_);        // 1 long argument register.

  const Instruction* instr = Instruction::At(code_item->insns_);
  ASSERT_EQ(instr->Opcode(), Instruction::CONST_WIDE_16);
  ASSERT_EQ(instr->VRegA(), kVreg0);
  const uint32_t kConstWide16_1_DexPc = instr->GetDexPc(code_item->insns_);
  instr = instr->Next();
  ASSERT_EQ(instr->Opcode(), Instruction::CONST_WIDE_16);
  ASSERT_EQ(instr->VRegA(), kVreg2);
  const uint32_t kConstWide16_2_DexPc = instr->GetDexPc(code_item->insns_);
  instr = instr->Next();
  ASSERT_EQ(instr->Opcode(), Instruction::CMP_LONG);
  ASSERT_EQ(instr->VRegA(), kVreg2);
  ASSERT_EQ(instr->VRegB(), kVreg4);
  ASSERT_EQ(instr->VRegC(), kVreg2);
  const uint32_t kCmpLongDexPc = instr->GetDexPc(code_item->insns_);
  instr = instr->Next();
  ASSERT_EQ(instr->Opcode(), Instruction::IF_LEZ);
  ASSERT_EQ(instr->VRegA(), kVreg2);
  const uint32_t kIfLezDexPc = instr->GetDexPc(code_item->insns_);
  instr = instr->Next();
  ASSERT_EQ(instr->Opcode(), Instruction::ADD_LONG_2ADDR);
  ASSERT_EQ(instr->VRegA(), kVreg0);
  ASSERT_EQ(instr->VRegB(), kVreg4);
  const uint32_t kAddLong2AddrDexPc = instr->GetDexPc(code_item->insns_);
  instr = instr->Next();
  ASSERT_EQ(instr->Opcode(), Instruction::RETURN_WIDE);
  ASSERT_EQ(instr->VRegA(), kVreg0);
  const uint32_t kReturnWideDexPc = instr->GetDexPc(code_item->insns_);
  instr = instr->Next();
  ASSERT_EQ(reinterpret_cast<const uint16_t*>(instr),
            code_item->insns_ + code_item->insns_size_in_code_units_);

  {
    const std::vector<int32_t> kinds(verifier.DescribeVRegs(kConstWide16_1_DexPc));
    EXPECT_EQ(kUndefined, GetVregKind(kVreg0, kinds));  // v0,v1 pair not yet defined.
    EXPECT_EQ(kUndefined, GetVregKind(kVreg1, kinds));
    EXPECT_EQ(kUndefined, GetVregKind(kVreg2, kinds));  // v2,v3 pair not yet defined.
    EXPECT_EQ(kUndefined, GetVregKind(kVreg3, kinds));
    EXPECT_EQ(kLongLoVReg, GetVregKind(kVreg4, kinds));  // v4,v5 pair holds long argument.
    EXPECT_EQ(kLongHiVReg, GetVregKind(kVreg5, kinds));
  }
  {
      const std::vector<int32_t> kinds(verifier.DescribeVRegs(kConstWide16_2_DexPc));
      EXPECT_EQ(kImpreciseConstant, GetVregKind(kVreg0, kinds));
      EXPECT_EQ(kImpreciseConstant, GetVregKind(kVreg1, kinds));
      EXPECT_EQ(kUndefined, GetVregKind(kVreg2, kinds));
      EXPECT_EQ(kUndefined, GetVregKind(kVreg3, kinds));
      EXPECT_EQ(kLongLoVReg, GetVregKind(kVreg4, kinds));
      EXPECT_EQ(kLongHiVReg, GetVregKind(kVreg5, kinds));
  }
  {
      const std::vector<int32_t> kinds(verifier.DescribeVRegs(kCmpLongDexPc));
      EXPECT_EQ(kImpreciseConstant, GetVregKind(kVreg0, kinds));
      EXPECT_EQ(kImpreciseConstant, GetVregKind(kVreg1, kinds));
      EXPECT_EQ(kImpreciseConstant, GetVregKind(kVreg2, kinds));
      EXPECT_EQ(kImpreciseConstant, GetVregKind(kVreg3, kinds));
      EXPECT_EQ(kLongLoVReg, GetVregKind(kVreg4, kinds));
      EXPECT_EQ(kLongHiVReg, GetVregKind(kVreg5, kinds));
  }
  {
    const std::vector<int32_t> kinds(verifier.DescribeVRegs(kIfLezDexPc));
    EXPECT_EQ(kImpreciseConstant, GetVregKind(kVreg0, kinds));
    EXPECT_EQ(kImpreciseConstant, GetVregKind(kVreg1, kinds));
    EXPECT_EQ(kIntVReg, GetVregKind(kVreg2, kinds));
    EXPECT_EQ(kImpreciseConstant, GetVregKind(kVreg3, kinds));
    EXPECT_EQ(kLongLoVReg, GetVregKind(kVreg4, kinds));
    EXPECT_EQ(kLongHiVReg, GetVregKind(kVreg5, kinds));
  }
  {
    const std::vector<int32_t> kinds(verifier.DescribeVRegs(kAddLong2AddrDexPc));
    EXPECT_EQ(kImpreciseConstant, GetVregKind(kVreg0, kinds));
    EXPECT_EQ(kImpreciseConstant, GetVregKind(kVreg1, kinds));
    EXPECT_EQ(kIntVReg, GetVregKind(kVreg2, kinds));
    EXPECT_EQ(kImpreciseConstant, GetVregKind(kVreg3, kinds));
    EXPECT_EQ(kLongLoVReg, GetVregKind(kVreg4, kinds));
    EXPECT_EQ(kLongHiVReg, GetVregKind(kVreg5, kinds));
  }
  {
    const std::vector<int32_t> kinds(verifier.DescribeVRegs(kReturnWideDexPc));
    EXPECT_EQ(kLongLoVReg, GetVregKind(kVreg0, kinds));
    EXPECT_EQ(kLongHiVReg, GetVregKind(kVreg1, kinds));
    EXPECT_EQ(kUndefined, GetVregKind(kVreg2, kinds));
    EXPECT_EQ(kUndefined, GetVregKind(kVreg3, kinds));
    EXPECT_EQ(kUndefined, GetVregKind(kVreg4, kinds));
    EXPECT_EQ(kUndefined, GetVregKind(kVreg5, kinds));
  }
}

}  // namespace verifier
}  // namespace art
