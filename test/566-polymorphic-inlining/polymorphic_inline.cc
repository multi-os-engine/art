/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "art_method.h"
#include "jit/jit.h"
#include "jit/jit_code_cache.h"
#include "oat_quick_method_header.h"
#include "scoped_thread_state_change.h"
#include "stack_map.h"

namespace art {

namespace {

extern "C" JNIEXPORT void JNICALL Java_Main_ensureJittedAndPolymorphicInline(JNIEnv*, jclass cls) {
  jit::Jit* jit = Runtime::Current()->GetJit();
  if (jit == nullptr) {
    return;
  }

  ScopedObjectAccess soa(Thread::Current());
  mirror::Class* klass = soa.Decode<mirror::Class*>(cls);
  ArtMethod* test_invoke_interface =
      klass->FindDeclaredDirectMethodByName("testInvokeInterface", sizeof(void*));
  ArtMethod* test_invoke_virtual =
      klass->FindDeclaredDirectMethodByName("testInvokeVirtual", sizeof(void*));

  jit->CompileMethod(test_invoke_interface, Thread::Current());
  jit->CompileMethod(test_invoke_virtual, Thread::Current());

  jit::JitCodeCache* code_cache = jit->GetCodeCache();

  OatQuickMethodHeader* header = OatQuickMethodHeader::FromEntryPoint(
      test_invoke_virtual->GetEntryPointFromQuickCompiledCode());
  CHECK(code_cache->ContainsPc(header->GetCode()));

  CodeInfo info = header->GetOptimizedCodeInfo();
  CHECK(info.HasInlineInfo());

  header = OatQuickMethodHeader::FromEntryPoint(
      test_invoke_interface->GetEntryPointFromQuickCompiledCode());
  CHECK(code_cache->ContainsPc(header->GetCode()));
  info = header->GetOptimizedCodeInfo();
  CHECK(info.HasInlineInfo());
}

}
}  // namespace art
