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

#include "jit_instrumentation.h"

#include "art_method-inl.h"
#include "jit.h"
#include "jit_code_cache.h"
#include "scoped_thread_state_change.h"

namespace art {
namespace jit {

class JitCompileTask : public Task {
 public:
  JitCompileTask(ArtMethod* method, JitInstrumentationCache* cache)
      : method_(method), cache_(cache) {
  }

  virtual void Run(Thread* self) OVERRIDE {
    ScopedObjectAccess soa(self);
    VLOG(jit) << "JitCompileTask compiling method " << PrettyMethod(method_);
    if (Runtime::Current()->GetJit()->CompileMethod(method_, self)) {
      cache_->SignalCompiled(self, method_);
    } else {
      VLOG(jit) << "Failed to compile method " << PrettyMethod(method_);
    }
  }

  virtual void Finalize() OVERRIDE {
    delete this;
  }

 private:
  ArtMethod* const method_;
  JitInstrumentationCache* const cache_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(JitCompileTask);
};

JitInstrumentationCache::JitInstrumentationCache(size_t hot_method_threshold,
                                                 size_t warm_method_threshold)
    : hot_method_threshold_(hot_method_threshold),
      warm_method_threshold_(warm_method_threshold) {
}

void JitInstrumentationCache::CreateThreadPool() {
  thread_pool_.reset(new ThreadPool("Jit thread pool", 1));
}

void JitInstrumentationCache::DeleteThreadPool() {
  thread_pool_.reset();
}

void JitInstrumentationCache::SignalCompiled(Thread*, ArtMethod*) {
}

void JitInstrumentationCache::AddSamples(Thread* self, ArtMethod* method, size_t) {
  ScopedObjectAccessUnchecked soa(self);
  // Since we don't have on-stack replacement, some methods can remain in the interpreter longer
  // than we want resulting in samples even after the method is compiled.
  if (method->IsClassInitializer() || method->IsNative() ||
      Runtime::Current()->GetJit()->GetCodeCache()->ContainsMethod(method)) {
    return;
  }
  if (thread_pool_.get() == nullptr) {
    // Runtime is shutting down.
    return;
  }
  uint16_t sample_count = method->IncrementCounter();
  // If we have enough samples, mark as hot and request Jit compilation.
  if (sample_count == warm_method_threshold_) {
    ProfilingInfo* info = method->CreateProfilingInfo();
    VLOG(jit) << "Start profiling " << PrettyMethod(method) << " with " << info << " " << method->GetProfilingInfo();
  }
  if (sample_count == hot_method_threshold_) {
    thread_pool_->AddTask(self, new JitCompileTask(
        method->GetInterfaceMethodIfProxy(sizeof(void*)), this));
    thread_pool_->StartWorkers(self);
  }
}

JitInstrumentationListener::JitInstrumentationListener(JitInstrumentationCache* cache)
    : instrumentation_cache_(cache) {
  CHECK(instrumentation_cache_ != nullptr);
}

void JitInstrumentationListener::InvokeVirtualOrInterface(Thread* thread,
                                                          mirror::Object* this_object,
                                                          ArtMethod* caller,
                                                          uint32_t dex_pc,
                                                          ArtMethod* /* callee */) {
  if (this_object == nullptr) {
    return;
  }

  ProfilingInfo* info = caller->GetProfilingInfo();
  if (info == nullptr) {
    return;
  }

  info->AddInvokeInfo(thread, dex_pc, this_object->GetClass());
}

}  // namespace jit
}  // namespace art
