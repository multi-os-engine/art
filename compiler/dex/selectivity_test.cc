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

#include "common_compiler_test.h"
#include "dex/selectivity.h"

namespace art {
class SelectivityTest: public CommonCompilerTest {
 protected:
  void SetupSelectivityTest() {
    SkipClassCalled = false;
    SkipMethodCalled = false;
    SimplePreCompileSummaryCalled = false;
  }

  void CompileAll(jobject class_loader) LOCKS_EXCLUDED(Locks::mutator_lock_) {
    TimingLogger timings("SelectivityTest::CompileAll", false, false);
    TimingLogger::ScopedTiming t(__FUNCTION__, &timings);
    compiler_driver_->CompileAll(class_loader,
                                 Runtime::Current()->GetCompileTimeClassPath(class_loader),
                                 &timings);
  }

  static bool AlwaysSkipClass(const DexFile& dex_file, const DexFile::ClassDef& class_def) {
    SkipClassCalled = true;
    return true;
  }

  static bool AlwaysSkipMethod(const DexFile::CodeItem* code_item,
                               uint32_t method_idx, uint32_t* access_flags,
                               uint16_t* class_def_idx, const DexFile& dex_file,
                               DexToDexCompilationLevel* dex_to_dex_compilation_level) {
    SkipMethodCalled = true;
    return true;
  }

  static bool SimplePreCompileSummary(CompilerDriver* driver,
                                      VerificationResults* verification_results) {
    SimplePreCompileSummaryCalled = true;
    return true;
  }

  void SetAlwaysSkipMethod() {
    SkipMethodCalled = false;
    Selectivity::SetSkipMethodCompilation(AlwaysSkipMethod);
  }

  void SetAlwaysSkipClass() {
    SkipClassCalled = false;
    Selectivity::SetSkipClassCompilation(AlwaysSkipClass);
  }

  void SetPreCompileSummaryLogic() {
    SimplePreCompileSummaryCalled = false;
    Selectivity::SetPreCompileSummaryLogic(SimplePreCompileSummary);
  }

  static bool SkipMethodCalled;
  static bool SkipClassCalled;
  static bool SimplePreCompileSummaryCalled;
};

bool SelectivityTest::SkipMethodCalled = false;
bool SelectivityTest::SkipClassCalled = false;
bool SelectivityTest::SimplePreCompileSummaryCalled = false;

TEST_F(SelectivityTest, CheckBaseSelectivityFunctionSkipClass) {
  TEST_DISABLED_FOR_PORTABLE();
  mirror::Class* klass;
  jobject jclass_loader;
  SetupSelectivityTest();
  Handle<mirror::ClassLoader> class_loader;
  {
    ScopedObjectAccess soa(Thread::Current());
    jclass_loader = LoadDex("Main");
    StackHandleScope<1> hs(soa.Self());
    class_loader = hs.NewHandle(soa.Decode<mirror::ClassLoader*>(jclass_loader));
  }
  ASSERT_TRUE(jclass_loader != NULL);
  SetAlwaysSkipClass();

  // Paranoid. SkipMethod should never be called.
  ASSERT_TRUE(SkipMethodCalled == false);
  ASSERT_TRUE(SkipClassCalled == false);

  // Exercise flow that will call Selectivity::SkipClassCompilation in CompilerDriver.
  CompileAll(jclass_loader);

  // Paranoid. SkipMethod should never be called.
  ASSERT_TRUE(SkipMethodCalled == false);
  ASSERT_TRUE(SkipClassCalled == true);
  {
    ScopedObjectAccess soa(Thread::Current());
    klass = class_linker_->FindClass(soa.Self(), "LMain;", class_loader);
    ASSERT_TRUE(klass != NULL);
    mirror::ArtMethod* method = klass->FindDirectMethod("main", "([Ljava/lang/String;)V");
    ASSERT_TRUE(method != NULL);
    MethodReference method_reference(method->GetDexFile(), method->GetMethodIndex());
    // Despite calling compile, it should not have compiled.
    ASSERT_TRUE(compiler_driver_->GetCompiledMethod(method_reference) == nullptr);
  }
}

TEST_F(SelectivityTest, CheckBaseSelectivityFunctionSkipMethod) {
  TEST_DISABLED_FOR_PORTABLE();
  SetupSelectivityTest();
  ScopedObjectAccess soa(Thread::Current());
  jobject jclass_loader = LoadDex("Main");
  StackHandleScope<1> hs(soa.Self());

  SetAlwaysSkipMethod();

  Handle<mirror::ClassLoader> class_loader(
      hs.NewHandle(soa.Decode<mirror::ClassLoader*>(jclass_loader)));

  // Paranoid. SkipClass should never be called.
  ASSERT_TRUE(SkipClassCalled == false);
  ASSERT_TRUE(SkipMethodCalled == false);

  // Exercise flow that will call Selectivity::SkipMethodCompilation in CompilerDriver.
  CompileDirectMethod(class_loader, "Main", "main", "([Ljava/lang/String;)V");

  // Paranoid. SkipClass should never be called.
  ASSERT_TRUE(SkipClassCalled == false);
  ASSERT_TRUE(SkipMethodCalled == true);

  mirror::Class* klass = class_linker_->FindClass(soa.Self(), "LMain;", class_loader);
  ASSERT_TRUE(klass != NULL);

  mirror::ArtMethod* method = klass->FindDirectMethod("main", "([Ljava/lang/String;)V");
  ASSERT_TRUE(method != NULL);
  MethodReference method_reference(method->GetDexFile(), method->GetMethodIndex());
  // Despite calling compile, it should not have compiled.
  ASSERT_TRUE(compiler_driver_->GetCompiledMethod(method_reference) == nullptr);
}

TEST_F(SelectivityTest, CheckBaseSelectivityFunctionPreCompileSummaryLogic) {
  TEST_DISABLED_FOR_PORTABLE();
  jobject jclass_loader;
  SetupSelectivityTest();
  Handle<mirror::ClassLoader> class_loader;
  {
    ScopedObjectAccess soa(Thread::Current());
    jclass_loader = LoadDex("Main");
    StackHandleScope<1> hs(soa.Self());
    class_loader = hs.NewHandle(soa.Decode<mirror::ClassLoader*>(jclass_loader));
  }
  ASSERT_TRUE(jclass_loader != NULL);
  SetPreCompileSummaryLogic();


  ASSERT_TRUE(SimplePreCompileSummaryCalled == false);
  // Exercise flow that will call Selectivity::PreCompileSummaryLogic in CompilerDriver.
  CompileAll(jclass_loader);
  ASSERT_TRUE(SimplePreCompileSummaryCalled == true);
}

}  // namespace art
