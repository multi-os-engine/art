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

#include "mirror/art_method-inl.h"
#include "scoped_thread_state_change.h"

namespace art {
namespace jit {

class JitCompileTask : public Task {
 public:
  explicit JitCompileTask(mirror::ArtMethod* method, JitInstrumentationCache* cache)
      : method_(method), cache_(cache) {
  }

  virtual void Run(Thread* self) OVERRIDE {
    ScopedObjectAccess soa(self);
    // LOG(INFO) << "JitCompileTask compiling method " << PrettyMethod(method_);
    if (Runtime::Current()->JitCompileMethod(method_, self)) {
      cache_->SignalCompiled(self, method_);
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
    : lock_("jit instrumentation lock"), hot_method_threshold_(hot_method_threshold) {
}

void JitInstrumentationCache::CreateThreadPool() {
  thread_pool_.reset(new ThreadPool("Jit thread pool", 1));
}

void JitInstrumentationCache::DeleteThreadPool() {
  thread_pool_.reset(nullptr);
}

void JitInstrumentationCache::SignalCompiled(Thread* self, mirror::ArtMethod* method) {
  ScopedObjectAccessUnchecked soa(self);
  jmethodID method_id = soa.EncodeMethod(method);
  MutexLock mu(self, lock_);
  auto it = samples_.find(method_id);
  if (it != samples_.end()) {
    samples_.erase(it);
  }
}

bool JitInstrumentationCache::IsCompiled(mirror::ArtMethod* method) {
  // method->GetEntryPointFromQuickCompiledCode() != GetQuickToInterpreterBridge();
  return method->IsClassInitializer();
}

void JitInstrumentationCache::AddSamples(Thread* self, mirror::ArtMethod* method, size_t count) {
  ScopedObjectAccessUnchecked soa(self);
  if (IsCompiled(method)) {
    return;
  }
  jmethodID method_id = soa.EncodeMethod(method);
  bool is_hot = false;
  {
    MutexLock mu(self, lock_);
    auto it = samples_.find(method_id);
    if (it != samples_.end()) {
      it->second += count;
      // If we have enough samples, mark as hot and request Jit compilation.
      if (it->second >= hot_method_threshold_ && it->second - count < hot_method_threshold_) {
        is_hot = true;
      }
    } else {
      samples_.insert(std::make_pair(method_id, count));
    }
  }
  if (is_hot) {
    if (thread_pool_.get() != nullptr) {
      LOG(INFO) << "Requesting compile on hot method " << PrettyMethod(method);
      thread_pool_->AddTask(self, new JitCompileTask(method, this));
      thread_pool_->StartWorkers(self);
    } else {
      LOG(INFO) << "Compiling hot method " << PrettyMethod(method);
      Runtime::Current()->JitCompileMethod(method, self);
    }
  }
}

JitInstrumentationListener::JitInstrumentationListener(JitInstrumentationCache* cache)
    : instrumentation_cache_(cache) {
  CHECK(instrumentation_cache_ != nullptr);
}

}  // namespace jit
}  // namespace art
