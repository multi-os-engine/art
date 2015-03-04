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

#include "jit.h"
#include "jit_code_cache.h"
#include "mirror/art_method-inl.h"
#include "scoped_thread_state_change.h"

namespace art {
namespace jit {

static constexpr size_t kNumJitCompilerThreads = 1;

class JitCompileTask : public Task {
 public:
  explicit JitCompileTask(mirror::ArtMethod* method, JitInstrumentationCache* cache)
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
  mirror::ArtMethod* const method_;
  JitInstrumentationCache* const cache_;
};

JitInstrumentationCache::JitInstrumentationCache(size_t hot_method_threshold)
    : lock_("jit instrumentation lock"), increment_counter_(0), increment_frequency_(13),
      hot_method_threshold_(hot_method_threshold),
      adjusted_hot_method_threshold_(hot_method_threshold_ / increment_frequency_) {
  CHECK_LE(adjusted_hot_method_threshold_, 0xFFu);
  Locks::mutator_lock_->AssertExclusiveHeld(Thread::Current());
  if (Jit::kInstrumentationInDexFile) {
    Runtime::Current()->GetClassLinker()->AddJitInstrumentationToDexFiles();
  }
}

void JitInstrumentationCache::CreateThreadPool() {
  thread_pool_.reset(new ThreadPool("Jit thread pool", kNumJitCompilerThreads));
}

void JitInstrumentationCache::DeleteThreadPool() {
  thread_pool_.reset();
}

void JitInstrumentationCache::SignalCompiled(Thread* self, mirror::ArtMethod* method) {
  if (!Jit::kInstrumentationInDexFile) {
    ScopedObjectAccessUnchecked soa(self);
    jmethodID method_id = soa.EncodeMethod(method);
    MutexLock mu(self, lock_);
    auto it = samples_.find(method_id);
    if (it != samples_.end()) {
      samples_.erase(it);
    }
  }
}

void JitInstrumentationCache::AddSample(Thread* self, mirror::ArtMethod* method) {
  ScopedObjectAccessUnchecked soa(self);
  // Since we don't have on-stack replacement, some methods can remain in the interpreter longer
  // than we want resulting in samples even after the method is compiled.
  if (method->IsClassInitializer() ||
      Runtime::Current()->GetJit()->GetCodeCache()->ContainsMethod(method)) {
    return;
  }
  // Only add a jit sample every increment_frequency_ times AddSample is called.
  ++increment_counter_;
  if (increment_counter_ < increment_frequency_) {
    return;
  }
  increment_counter_ = 0;
  jmethodID method_id = soa.EncodeMethod(method);
  size_t sample_count = 0;
  if (Jit::kInstrumentationInDexFile) {
    auto* dex_file = method->GetDexFile();
    auto dex_method_index = method->GetDexMethodIndex();
    auto* samples = dex_file->GetJitSamples();
    sample_count = samples[dex_method_index];
    // Make sure we don't overflow the counter.
    sample_count += sample_count < (1u << (kBitsPerByte * sizeof(samples[0]))) - 1;
    samples[dex_method_index] = static_cast<uint8_t>(sample_count);
  } else {
    MutexLock mu(self, lock_);
    auto it = samples_.find(method_id);
    if (it != samples_.end()) {
      ++it->second;
      sample_count = it->second;;
    } else {
      sample_count = 1u;
      samples_.insert(std::make_pair(method_id, sample_count));
    }
  }
  // If we have enough samples, request or perform compilation.
  if (sample_count == adjusted_hot_method_threshold_) {
    if (thread_pool_.get() != nullptr) {
      thread_pool_->AddTask(self, new JitCompileTask(method->GetInterfaceMethodIfProxy(), this));
      thread_pool_->StartWorkers(self);
    } else {
      VLOG(jit) << "Compiling hot method " << PrettyMethod(method);
      Runtime::Current()->GetJit()->CompileMethod(method->GetInterfaceMethodIfProxy(), self);
    }
  }
}

JitInstrumentationListener::JitInstrumentationListener(JitInstrumentationCache* cache)
    : instrumentation_cache_(cache) {
  CHECK(instrumentation_cache_ != nullptr);
}

}  // namespace jit
}  // namespace art
