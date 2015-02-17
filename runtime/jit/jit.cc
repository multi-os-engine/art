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
#include "runtime_options.h"
#include "thread_list.h"
#include "utils.h"

namespace art {
namespace jit {

JitOptions* JitOptions::CreateFromRuntimeArguments(const RuntimeArgumentMap& options) {
  if (!options.GetOrDefault(RuntimeArgumentMap::UseJIT)) {
    return nullptr;
  }
  auto* jit_options = new JitOptions;
  jit_options->code_cache_capacity_ =
      options.GetOrDefault(RuntimeArgumentMap::JITCodeCacheCapacity);
  jit_options->compile_threshold_ =
      options.GetOrDefault(RuntimeArgumentMap::JITCompileThreshold);
  return jit_options;
}

Jit::Jit()
    : jit_library_handle_(nullptr), jit_compiler_handle_(nullptr), jit_load_(nullptr),
      jit_compile_method_(nullptr) {
}

Jit* Jit::Create(JitOptions* options) {
  std::unique_ptr<Jit> jit(new Jit);
  if (!jit->LoadCompiler()) {
    return nullptr;
  }
  jit->code_cache_.reset(JitCodeCache::Create(options->code_cache_capacity_));
  if (jit->GetCodeCache() == nullptr) {
    return nullptr;
  }
  LOG(INFO) << "JIT created with code_cache_capacity=" << options->code_cache_capacity_
      << " compile_threshold=" << options->compile_threshold_;
  return jit.release();
}

bool Jit::LoadCompiler() {
  jit_library_handle_ = dlopen(
      kIsDebugBuild ? "libartd-compiler.so" : "libart-compiler.so", RTLD_NOW);
  if (jit_library_handle_ == nullptr) {
    LOG(ERROR) << "JIT could not load libart-compiler.so: " << dlerror();
    return false;
  }
  jit_load_ = reinterpret_cast<void* (*)(CompilerCallbacks**)>(
      dlsym(jit_library_handle_, "jit_load"));
  if (jit_load_ == nullptr) {
    LOG(ERROR) << "JIT couldn't find jit_load entry point";
    return false;
  }
  jit_compile_method_ = reinterpret_cast<bool (*)(void*, mirror::ArtMethod*, Thread*)>(
      dlsym(jit_library_handle_, "jit_compile_method"));
  if (jit_compile_method_ == nullptr) {
    LOG(ERROR) << "JIT couldn't find jit_compile_method entry point";
    return false;
  }
  CompilerCallbacks* callbacks = nullptr;
  VLOG(jit) << "Calling JitLoad interpreter_only="
      << Runtime::Current()->GetInstrumentation()->InterpretOnly();
  jit_compiler_handle_ = (jit_load_)(&callbacks);
  if (jit_compiler_handle_ == nullptr) {
    LOG(ERROR) << "JIT couldn't load compiler";
    return false;
  }
  if (callbacks == nullptr) {
    LOG(ERROR) << "JIT compiler callbacks were not set";
    jit_compiler_handle_ = nullptr;
    return false;
  }
  compiler_callbacks_ = callbacks;
  return true;
}

bool Jit::CompileMethod(mirror::ArtMethod* method, Thread* self) {
  DCHECK(!method->IsRuntimeMethod());
  const bool result = jit_compile_method_(jit_compiler_handle_, method, self);
  if (result) {
    method->SetEntryPointFromInterpreter(artInterpreterToCompiledCodeBridge);
  }
  return result;
}

void Jit::CreateThreadPool() {
  CHECK(instrumentation_cache_.get() != nullptr);
  instrumentation_cache_->CreateThreadPool();
}

void Jit::DeleteThreadPool() {
  if (instrumentation_cache_.get() != nullptr) {
    instrumentation_cache_->DeleteThreadPool();
  }
}

Jit::~Jit() {
  DeleteThreadPool();
  if (jit_library_handle_ != nullptr) {
    dlclose(jit_library_handle_);
  }
}

void Jit::CreateInstrumentationCache(size_t compile_threshold) {
  CHECK_GT(compile_threshold, 0U);
  Runtime* const runtime = Runtime::Current();
  runtime->GetThreadList()->SuspendAll();
  // Add Jit interpreter instrumentation, tells the interpreter when to notify the jit to compile
  // something.
  instrumentation_cache_.reset(new jit::JitInstrumentationCache(compile_threshold));
  runtime->GetInstrumentation()->AddListener(
      new jit::JitInstrumentationListener(instrumentation_cache_.get()),
      instrumentation::Instrumentation::kMethodEntered |
      instrumentation::Instrumentation::kBackwardBranch);
  runtime->GetThreadList()->ResumeAll();
}

}  // namespace jit
}  // namespace art
