/*
 * Copyright 2014 The Android Open Source Project
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

#include "jit.h"

#include <dlfcn.h>

#include "entrypoints/runtime_asm_entrypoints.h"
#include "interpreter/interpreter.h"
#include "jit_code_cache.h"
#include "jit_instrumentation.h"
#include "mirror/art_method-inl.h"
#include "runtime.h"
#include "thread_list.h"
#include "utils.h"

namespace art {
namespace jit {

Jit::Jit()
    : jit_library_handle_(nullptr), jit_compiler_handle_(nullptr), jit_load_(nullptr),
      jit_compile_method_(nullptr) {
}

Jit* Jit::Create() {
  std::unique_ptr<Jit> jit(new Jit);
  if (!jit->LoadCompiler()) {
    return nullptr;
  }
  jit->code_cache_.reset(JitCodeCache::Create(64 * MB));
  if (jit->GetCodeCache() == nullptr) {
    return nullptr;
  }
  return jit.release();
}

bool Jit::LoadCompiler() {
  jit_library_handle_ = dlopen(
      kIsDebugBuild ? "libartd-compiler.so" : "libart-compiler.so", RTLD_NOW);
  if (jit_library_handle_ == nullptr) {
    LOG(WARNING) << "JIT could now load libart-compiler: " << dlerror();
    return false;
  }
  jit_load_ = reinterpret_cast<void* (*)(CompilerCallbacks**)>(
      dlsym(jit_library_handle_, "jit_load"));
  if (jit_load_ == nullptr) {
    LOG(WARNING) << "JIT couldn't find jit_load entry point";
    return false;
  }
  jit_compile_method_ = reinterpret_cast<bool (*)(void*, mirror::ArtMethod*, Thread*)>(
      dlsym(jit_library_handle_, "jit_compile_method"));
  if (jit_compile_method_ == nullptr) {
    LOG(WARNING) << "JIT couldn't find jit_compile_method entry point";
    return false;
  }
  CompilerCallbacks* callbacks;
  LOG(INFO) << "Calling JitLoad interpreter_only="
      << Runtime::Current()->GetInstrumentation()->InterpretOnly();
  jit_compiler_handle_ = (jit_load_)(&callbacks);
  if (jit_compiler_handle_ == nullptr) {
    LOG(ERROR) << "JIT couldn't load";
    return false;
  }
  if (callbacks == nullptr) {
    LOG(ERROR) << "JIT compiler callbacks were not set";
    jit_compiler_handle_ = nullptr;
    return false;
  }
  compiler_callbacks_ = callbacks;
  LOG(INFO) << "JIT loaded";
  return true;
}

bool Jit::CompileMethod(mirror::ArtMethod* method, Thread* self) {
  CHECK_NE(method, Runtime::Current()->GetImtConflictMethod());
  const bool result = jit_compile_method_(jit_compiler_handle_, method, self);
  if (!result) {
    LOG(INFO) << "JIT failed to compile " << PrettyMethod(method)
        << " falling back to interpretation";
    method->SetEntryPointFromQuickCompiledCode(GetQuickToInterpreterBridge());
    method->SetEntryPointFromInterpreter(artInterpreterToInterpreterBridge);
  } else {
    method->SetEntryPointFromInterpreter(artInterpreterToCompiledCodeBridge);
  }
  VLOG(jit) << "JIT trampoline: new quick_code="
            << reinterpret_cast<const void*>(method->GetEntryPointFromQuickCompiledCode());
  return result;
}

void Jit::CreateThreadPool() {
  CHECK(instrumentation_cache_.get() != nullptr);
  instrumentation_cache_->CreateThreadPool();
}

Jit::~Jit() {
  if (instrumentation_cache_.get() != nullptr) {
    instrumentation_cache_->DeleteThreadPool();
  }
}

void Jit::CreateInstrumentationCache() {
  Runtime* runtime = Runtime::Current();
  runtime->GetThreadList()->SuspendAll();
  // Add Jit interpreter instrumentation, tells the interpreter when to notify the jit to compile
  // something.
  instrumentation_cache_.reset(new jit::JitInstrumentationCache(500));
  runtime->GetInstrumentation()->AddListener(
      new jit::JitInstrumentationListener(instrumentation_cache_.get()),
      instrumentation::Instrumentation::kMethodEntered |
      instrumentation::Instrumentation::kBackwardBranch);
  runtime->GetThreadList()->ResumeAll();
}

}  // namespace jit
}  // namespace art
