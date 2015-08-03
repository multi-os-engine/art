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

static constexpr size_t kNumJitCompilerThreads = 1;

class JitCompileTask : public Task {
 public:
  explicit JitCompileTask(ArtMethod* method, JitInstrumentationCache* cache)
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

JitInstrumentationCache::JitInstrumentationCache(size_t hot_method_threshold)
    : lock_("jit instrumentation lock"), samples_array_(),
      hot_method_threshold_(hot_method_threshold) {
}

void JitInstrumentationCache::CreateThreadPool() {
  thread_pool_.reset(new ThreadPool("Jit thread pool", kNumJitCompilerThreads));
}

void JitInstrumentationCache::DeleteThreadPool() {
  thread_pool_.reset();
}

void JitInstrumentationCache::SignalCompiled(Thread* self, ArtMethod* method) {
  uint16_t id = method->GetOrSetMethodID();
  if (UNLIKELY(id == ArtMethod::kMaxMethodID)) {
    MutexLock mu(self, lock_);
    auto it = samples_map_.find(method);
    if (it != samples_map_.end()) {
      samples_map_.erase(it);
    }
  }
}

void JitInstrumentationCache::AddSamples(Thread* self, ArtMethod* method) {
  // Since we don't have on-stack replacement, some methods can remain in the interpreter longer
  // than we want resulting in samples even after the method is compiled.
  if (method->IsClassInitializer() || method->IsNative() ||
      Runtime::Current()->GetJit()->GetCodeCache()->ContainsMethod(method)) {
    return;
  }
  size_t sample_count;
  uint16_t id = method->GetOrSetMethodID();
  if (UNLIKELY(id == ArtMethod::kMaxMethodID)) {
    MutexLock mu(self, lock_);
    auto it = samples_map_.find(method);
    if (it != samples_map_.end()) {
      ++it->second;
      sample_count = it->second;
    } else {
      sample_count = 1;
      samples_map_.insert(std::make_pair(method, 1));
    }
  } else {
    sample_count = samples_array_[id].fetch_add(1, std::memory_order_relaxed) + 1;
  }
  // If we have enough samples, request or perform Jit compilation.
  if (sample_count >= hot_method_threshold_) {
    if (thread_pool_.get() != nullptr) {
      thread_pool_->AddTask(self, new JitCompileTask(
          method->GetInterfaceMethodIfProxy(sizeof(void*)), this));
      thread_pool_->StartWorkers(self);
    } else {
      VLOG(jit) << "Compiling hot method " << PrettyMethod(method);
      Runtime::Current()->GetJit()->CompileMethod(
          method->GetInterfaceMethodIfProxy(sizeof(void*)), self);
    }
  }
}

JitInstrumentationListener::JitInstrumentationListener(JitInstrumentationCache* cache)
    : instrumentation_cache_(cache) {
  CHECK(instrumentation_cache_ != nullptr);
}

}  // namespace jit
}  // namespace art
