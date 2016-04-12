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
#include "thread_list.h"

namespace art {
namespace jit {

class JitCompileTask FINAL : public Task {
 public:
  enum TaskKind {
    kAllocateProfile,
    kCompile,
    kCompileOsr
  };

  JitCompileTask(ArtMethod* method, TaskKind kind) : method_(method), kind_(kind) {
    ScopedObjectAccess soa(Thread::Current());
    // Add a global ref to the class to prevent class unloading until compilation is done.
    klass_ = soa.Vm()->AddGlobalRef(soa.Self(), method_->GetDeclaringClass());
    CHECK(klass_ != nullptr);
  }

  ~JitCompileTask() {
    ScopedObjectAccess soa(Thread::Current());
    soa.Vm()->DeleteGlobalRef(soa.Self(), klass_);
  }

  void Run(Thread* self) OVERRIDE {
    ScopedObjectAccess soa(self);
    if (kind_ == kCompile) {
      VLOG(jit) << "JitCompileTask compiling method " << PrettyMethod(method_);
      if (!Runtime::Current()->GetJit()->CompileMethod(method_, self, /* osr */ false)) {
        VLOG(jit) << "Failed to compile method " << PrettyMethod(method_);
      }
    } else if (kind_ == kCompileOsr) {
      VLOG(jit) << "JitCompileTask compiling method osr " << PrettyMethod(method_);
      if (!Runtime::Current()->GetJit()->CompileMethod(method_, self, /* osr */ true)) {
        VLOG(jit) << "Failed to compile method osr " << PrettyMethod(method_);
      }
    } else {
      DCHECK(kind_ == kAllocateProfile);
      if (ProfilingInfo::Create(self, method_, /* retry_allocation */ true)) {
        VLOG(jit) << "Start profiling " << PrettyMethod(method_);
      }
    }
  }

  void Finalize() OVERRIDE {
    delete this;
  }

 private:
  ArtMethod* const method_;
  const TaskKind kind_;
  jobject klass_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(JitCompileTask);
};


void Jit::AddSamples(Thread* self, ArtMethod* method, uint16_t count) {
  if (thread_pool_ == nullptr) {
    // Should only see this when shutting down.
    DCHECK(Runtime::Current()->IsShuttingDown(self));
    return;
  }

  if (method->IsClassInitializer() || method->IsNative()) {
    // We do not want to compile such methods.
    return;
  }
  DCHECK(thread_pool_ != nullptr);
  DCHECK_GT(warm_method_threshold_, 0);
  DCHECK_GT(hot_method_threshold_, warm_method_threshold_);
  DCHECK_GT(osr_method_threshold_, hot_method_threshold_);

  int32_t starting_count = method->GetCounter();
  int32_t new_count = starting_count + count;   // int32 here to avoid wrap-around;
  if (starting_count < warm_method_threshold_) {
    if (new_count >= warm_method_threshold_) {
      bool success = ProfilingInfo::Create(self, method, /* retry_allocation */ false);
      if (success) {
        VLOG(jit) << "Start profiling " << PrettyMethod(method);
      }

      if (thread_pool_ == nullptr) {
        // Calling ProfilingInfo::Create might put us in a suspended state, which could
        // lead to the thread pool being deleted when we are shutting down.
        DCHECK(Runtime::Current()->IsShuttingDown(self));
        return;
      }

      if (!success) {
        // We failed allocating. Instead of doing the collection on the Java thread, we push
        // an allocation to a compiler thread, that will do the collection.
        thread_pool_->AddTask(self, new JitCompileTask(method, JitCompileTask::kAllocateProfile));
      }
    }
    // Avoid jumping more than one state at a time.
    new_count = std::min(new_count, hot_method_threshold_ - 1);
  } else if (starting_count < hot_method_threshold_) {
    if (new_count >= hot_method_threshold_) {
      DCHECK(thread_pool_ != nullptr);
      thread_pool_->AddTask(self, new JitCompileTask(method, JitCompileTask::kCompile));
    }
    // Avoid jumping more than one state at a time.
    new_count = std::min(new_count, osr_method_threshold_ - 1);
  } else if (starting_count < osr_method_threshold_) {
    if (new_count >= osr_method_threshold_) {
      DCHECK(thread_pool_ != nullptr);
      thread_pool_->AddTask(self, new JitCompileTask(method, JitCompileTask::kCompileOsr));
    }
  }
  // Update hotness counter
  method->SetCounter(new_count);
}

void Jit::MethodEntered(Thread* thread, ArtMethod* method) {
  if (UNLIKELY(Runtime::Current()->GetJit()->JitAtFirstUse())) {
    // The compiler requires a ProfilingInfo object.
    ProfilingInfo::Create(thread, method, /* retry_allocation */ true);
    JitCompileTask compile_task(method, JitCompileTask::kCompile);
    compile_task.Run(thread);
    return;
  }

  ProfilingInfo* profiling_info = method->GetProfilingInfo(sizeof(void*));
  // Update the entrypoint if the ProfilingInfo has one. The interpreter will call it
  // instead of interpreting the method.
  // We avoid doing this if exit stubs are installed to not mess with the instrumentation.
  // TODO(ngeoffray): Clean up instrumentation and code cache interactions.
  if ((profiling_info != nullptr) &&
      (profiling_info->GetSavedEntryPoint() != nullptr) &&
      !Runtime::Current()->GetInstrumentation()->AreExitStubsInstalled()) {
    method->SetEntryPointFromQuickCompiledCode(profiling_info->GetSavedEntryPoint());
  } else {
    AddSamples(thread, method, 1);
  }
}

void Jit::InvokeVirtualOrInterface(Thread* thread,
                                   mirror::Object* this_object,
                                   ArtMethod* caller,
                                   uint32_t dex_pc,
                                   ArtMethod* callee ATTRIBUTE_UNUSED) {
  ScopedAssertNoThreadSuspension ants(thread, __FUNCTION__);
  DCHECK(this_object != nullptr);
  ProfilingInfo* info = caller->GetProfilingInfo(sizeof(void*));
  if (info != nullptr) {
    // Since the instrumentation is marked from the declaring class we need to mark the card so
    // that mod-union tables and card rescanning know about the update.
    Runtime::Current()->GetHeap()->WriteBarrierEveryFieldOf(caller->GetDeclaringClass());
    info->AddInvokeInfo(dex_pc, this_object->GetClass());
  }
}

void Jit::WaitForCompilationToFinish(Thread* self) {
  if (thread_pool_ != nullptr) {
    thread_pool_->Wait(self, false, false);
  }
}

}  // namespace jit
}  // namespace art
