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

#ifndef ART_RUNTIME_JIT_JIT_H_
#define ART_RUNTIME_JIT_JIT_H_

#include <unordered_map>

#include "instrumentation.h"

#include "atomic.h"
#include "base/macros.h"
#include "base/mutex.h"
#include "gc_root.h"
#include "jni.h"
#include "object_callbacks.h"
#include "thread_pool.h"

namespace art {

class CompilerCallbacks;

namespace jit {

class JitCodeCache;
class JitInstrumentationCache;

class Jit {
 public:
  virtual ~Jit();
  static Jit* Create();
  bool CompileMethod(mirror::ArtMethod* method, Thread* self)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void CreateInstrumentationCache();
  void CreateThreadPool();
  CompilerCallbacks* GetCompilerCallbacks() {
    return compiler_callbacks_;
  }
  const JitCodeCache* GetCodeCache() const {
    return code_cache_.get();
  }
  JitCodeCache* GetCodeCache() {
    return code_cache_.get();
  }

 private:
  Jit();
  bool LoadCompiler();

  // JIT compiler
  void* jit_library_handle_;
  void* jit_compiler_handle_;
  void* (*jit_load_)(CompilerCallbacks**);
  bool (*jit_compile_method_)(void*, mirror::ArtMethod*, Thread*);

  std::unique_ptr<jit::JitInstrumentationCache> instrumentation_cache_;
  std::unique_ptr<jit::JitCodeCache> code_cache_;
  CompilerCallbacks* compiler_callbacks_;  // Owned by the jit compiler.
};


}  // namespace jit
}  // namespace art

#endif  // ART_RUNTIME_JIT_JIT_H_
