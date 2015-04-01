/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include "instrumentation.h"

#include "common_runtime_test.h"
#include "common_throws.h"
#include "class_linker-inl.h"
#include "dex_file.h"
#include "handle_scope-inl.h"
#include "jvalue.h"
#include "runtime.h"
#include "scoped_thread_state_change.h"
#include "thread_list.h"
#include "thread-inl.h"

namespace art {

class TestInstrumentationListener FINAL : public instrumentation::InstrumentationListener {
 public:
  TestInstrumentationListener()
    : received_method_enter_event(false), received_method_exit_event(false),
      received_method_unwind_event(false), received_dex_pc_moved_event(false),
      received_field_read_event(false), received_field_written_event(false),
      received_exception_caught_event(false), received_backward_branch_event(false) {}

  virtual ~TestInstrumentationListener() {}

  void MethodEntered(Thread* thread ATTRIBUTE_UNUSED,
                     mirror::Object* this_object ATTRIBUTE_UNUSED,
                     mirror::ArtMethod* method ATTRIBUTE_UNUSED,
                     uint32_t dex_pc ATTRIBUTE_UNUSED)
      OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    received_method_enter_event = true;
  }

  void MethodExited(Thread* thread ATTRIBUTE_UNUSED,
                    mirror::Object* this_object ATTRIBUTE_UNUSED,
                    mirror::ArtMethod* method ATTRIBUTE_UNUSED,
                    uint32_t dex_pc ATTRIBUTE_UNUSED,
                    const JValue& return_value ATTRIBUTE_UNUSED)
      OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    received_method_exit_event = true;
  }

  void MethodUnwind(Thread* thread ATTRIBUTE_UNUSED,
                    mirror::Object* this_object ATTRIBUTE_UNUSED,
                    mirror::ArtMethod* method ATTRIBUTE_UNUSED,
                    uint32_t dex_pc ATTRIBUTE_UNUSED)
      OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    received_method_unwind_event = true;
  }

  void DexPcMoved(Thread* thread ATTRIBUTE_UNUSED,
                  mirror::Object* this_object ATTRIBUTE_UNUSED,
                  mirror::ArtMethod* method ATTRIBUTE_UNUSED,
                  uint32_t new_dex_pc ATTRIBUTE_UNUSED)
      OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    received_dex_pc_moved_event = true;
  }

  void FieldRead(Thread* thread ATTRIBUTE_UNUSED,
                 mirror::Object* this_object ATTRIBUTE_UNUSED,
                 mirror::ArtMethod* method ATTRIBUTE_UNUSED,
                 uint32_t dex_pc ATTRIBUTE_UNUSED,
                 mirror::ArtField* field ATTRIBUTE_UNUSED)
      OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    received_field_read_event = true;
  }

  void FieldWritten(Thread* thread ATTRIBUTE_UNUSED,
                    mirror::Object* this_object ATTRIBUTE_UNUSED,
                    mirror::ArtMethod* method ATTRIBUTE_UNUSED,
                    uint32_t dex_pc ATTRIBUTE_UNUSED,
                    mirror::ArtField* field ATTRIBUTE_UNUSED,
                    const JValue& field_value ATTRIBUTE_UNUSED)
      OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    received_field_written_event = true;
  }

  void ExceptionCaught(Thread* thread ATTRIBUTE_UNUSED,
                       mirror::Throwable* exception_object ATTRIBUTE_UNUSED)
      OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    received_exception_caught_event = true;
  }

  void BackwardBranch(Thread* thread ATTRIBUTE_UNUSED,
                      mirror::ArtMethod* method ATTRIBUTE_UNUSED,
                      int32_t dex_pc_offset ATTRIBUTE_UNUSED)
      OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    received_backward_branch_event = true;
  }

  void Reset() {
    received_method_enter_event = false;
    received_method_exit_event = false;
    received_method_unwind_event = false;
    received_dex_pc_moved_event = false;
    received_field_read_event = false;
    received_field_written_event = false;
    received_exception_caught_event = false;
    received_backward_branch_event = false;
  }

  bool received_method_enter_event;
  bool received_method_exit_event;
  bool received_method_unwind_event;
  bool received_dex_pc_moved_event;
  bool received_field_read_event;
  bool received_field_written_event;
  bool received_exception_caught_event;
  bool received_backward_branch_event;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestInstrumentationListener);
};

class ITestChecker {
 public:
  virtual ~ITestChecker() {}
  virtual uint32_t GetInstrumentationEvent() = 0;
  virtual bool HasEventListener(const instrumentation::Instrumentation* instr) = 0;
  virtual void ReportEvent(const instrumentation::Instrumentation* instr,
                           Thread* self, mirror::ArtMethod* method, mirror::Object* obj,
                           uint32_t dex_pc) = 0;
  virtual bool DidListenerReceiveEvent(const TestInstrumentationListener& listener) = 0;
};

class InstrumentationTest : public CommonRuntimeTest {
 public:
  void TestEvent(ITestChecker& checker) {
    ScopedObjectAccess soa(Thread::Current());
    instrumentation::Instrumentation* instr = Runtime::Current()->GetInstrumentation();
    const uint32_t instrumentation_event = checker.GetInstrumentationEvent();
    TestInstrumentationListener listener;
    {
      soa.Self()->TransitionFromRunnableToSuspended(kSuspended);
      Runtime* runtime = Runtime::Current();
      runtime->GetThreadList()->SuspendAll("Add instrumentation listener");
      instr->AddListener(&listener, instrumentation_event);
      runtime->GetThreadList()->ResumeAll();
      soa.Self()->TransitionFromSuspendedToRunnable();
    }

    mirror::ArtMethod* const event_method = nullptr;
    mirror::Object* const event_obj = nullptr;
    const uint32_t event_dex_pc = 0;

    // Check the listener is registered and is notified of the event.
    EXPECT_TRUE(checker.HasEventListener(instr));
    EXPECT_FALSE(checker.DidListenerReceiveEvent(listener));
    checker.ReportEvent(instr, soa.Self(), event_method, event_obj, event_dex_pc);
    EXPECT_TRUE(checker.DidListenerReceiveEvent(listener));

    listener.Reset();
    {
      soa.Self()->TransitionFromRunnableToSuspended(kSuspended);
      Runtime* runtime = Runtime::Current();
      runtime->GetThreadList()->SuspendAll("Remove instrumentation listener");
      instr->RemoveListener(&listener, instrumentation_event);
      runtime->GetThreadList()->ResumeAll();
      soa.Self()->TransitionFromSuspendedToRunnable();
    }

    // Check the listener is not registered and is not notified of the event.
    EXPECT_FALSE(checker.HasEventListener(instr));
    EXPECT_FALSE(checker.DidListenerReceiveEvent(listener));
    checker.ReportEvent(instr, soa.Self(), event_method, event_obj, event_dex_pc);
    EXPECT_FALSE(checker.DidListenerReceiveEvent(listener));
  }

  void DeoptimizeMethod(Thread* self, Handle<mirror::ArtMethod> method,
                        bool enable_deoptimization)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    Runtime* runtime = Runtime::Current();
    instrumentation::Instrumentation* instrumentation = runtime->GetInstrumentation();
    self->TransitionFromRunnableToSuspended(kSuspended);
    runtime->GetThreadList()->SuspendAll("Single method deoptimization");
    if (enable_deoptimization) {
      instrumentation->EnableDeoptimization();
    }
    // TODO check deoptimization is enabled
    instrumentation->Deoptimize(method.Get());
    runtime->GetThreadList()->ResumeAll();
    self->TransitionFromSuspendedToRunnable();
  }

  void UndeoptimizeMethod(Thread* self, Handle<mirror::ArtMethod> method,
                          bool disable_deoptimization)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    Runtime* runtime = Runtime::Current();
    instrumentation::Instrumentation* instrumentation = runtime->GetInstrumentation();
    self->TransitionFromRunnableToSuspended(kSuspended);
    runtime->GetThreadList()->SuspendAll("Single method undeoptimization");
    instrumentation->Undeoptimize(method.Get());
    if (disable_deoptimization) {
      instrumentation->DisableDeoptimization();
    }
    runtime->GetThreadList()->ResumeAll();
    self->TransitionFromSuspendedToRunnable();
  }

  void DeoptimizeEverything(Thread* self, bool enable_deoptimization)
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    Runtime* runtime = Runtime::Current();
    instrumentation::Instrumentation* instrumentation = runtime->GetInstrumentation();
    self->TransitionFromRunnableToSuspended(kSuspended);
    runtime->GetThreadList()->SuspendAll("Full deoptimization");
    if (enable_deoptimization) {
      instrumentation->EnableDeoptimization();
    }
    // TODO check deoptimization is enabled
    instrumentation->DeoptimizeEverything();
    runtime->GetThreadList()->ResumeAll();
    self->TransitionFromSuspendedToRunnable();
  }

  void UndeoptimizeEverything(Thread* self, bool disable_deoptimization)
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    Runtime* runtime = Runtime::Current();
    instrumentation::Instrumentation* instrumentation = runtime->GetInstrumentation();
    self->TransitionFromRunnableToSuspended(kSuspended);
    runtime->GetThreadList()->SuspendAll("Full undeoptimization");
    instrumentation->UndeoptimizeEverything();
    if (disable_deoptimization) {
      instrumentation->DisableDeoptimization();
    }
    runtime->GetThreadList()->ResumeAll();
    self->TransitionFromSuspendedToRunnable();
  }
};

TEST_F(InstrumentationTest, NoInstrumentation) {
  ScopedObjectAccess soa(Thread::Current());
  instrumentation::Instrumentation* instr = Runtime::Current()->GetInstrumentation();
  ASSERT_NE(instr, nullptr);

  EXPECT_FALSE(instr->AreExitStubsInstalled());
  EXPECT_FALSE(instr->AreAllMethodsDeoptimized());
  EXPECT_FALSE(instr->IsActive());
  EXPECT_FALSE(instr->ShouldNotifyMethodEnterExitEvents());
  // TODO add private methods.

  // Test interpreter table is the default one.
  EXPECT_EQ(instrumentation::kMainHandlerTable, instr->GetInterpreterHandlerTable());

  // Check there is no registered listener.
  EXPECT_FALSE(instr->HasDexPcListeners());
  EXPECT_FALSE(instr->HasExceptionCaughtListeners());
  EXPECT_FALSE(instr->HasFieldReadListeners());
  EXPECT_FALSE(instr->HasFieldWriteListeners());
  EXPECT_FALSE(instr->HasMethodEntryListeners());
  EXPECT_FALSE(instr->HasMethodExitListeners());
}

TEST_F(InstrumentationTest, DexPcMovedEvent) {
  class DexPcMovedEventChecker : public ITestChecker {
   public:
    uint32_t GetInstrumentationEvent() OVERRIDE
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      return instrumentation::Instrumentation::kDexPcMoved;
    }
    bool HasEventListener(const instrumentation::Instrumentation* instr) OVERRIDE
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      return instr->HasDexPcListeners();
    }
    void ReportEvent(const instrumentation::Instrumentation* instr,
                     Thread* self, mirror::ArtMethod* method, mirror::Object* obj,
                     uint32_t dex_pc) OVERRIDE
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      instr->DexPcMovedEvent(self, obj, method, dex_pc);
    }
    bool DidListenerReceiveEvent(const TestInstrumentationListener& listener) OVERRIDE
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      return listener.received_dex_pc_moved_event;
    }
  };
  DexPcMovedEventChecker checker;
  TestEvent(checker);
}

TEST_F(InstrumentationTest, MethodEntryEvent) {
  class MethodEntryEventChecker : public ITestChecker {
   public:
    uint32_t GetInstrumentationEvent() OVERRIDE
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      return instrumentation::Instrumentation::kMethodEntered;
    }
    bool HasEventListener(const instrumentation::Instrumentation* instr) OVERRIDE
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      return instr->HasMethodEntryListeners();
    }
    void ReportEvent(const instrumentation::Instrumentation* instr,
                     Thread* self, mirror::ArtMethod* method, mirror::Object* obj,
                     uint32_t dex_pc) OVERRIDE
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      instr->MethodEnterEvent(self, obj, method, dex_pc);
    }
    bool DidListenerReceiveEvent(const TestInstrumentationListener& listener) OVERRIDE
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      return listener.received_method_enter_event;
    }
  };
  MethodEntryEventChecker checker;
  TestEvent(checker);
}

TEST_F(InstrumentationTest, MethodExitEvent) {
  class MethodExitEventChecker : public ITestChecker {
   public:
    uint32_t GetInstrumentationEvent() OVERRIDE
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      return instrumentation::Instrumentation::kMethodExited;
    }
    bool HasEventListener(const instrumentation::Instrumentation* instr) OVERRIDE
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      return instr->HasMethodExitListeners();
    }
    void ReportEvent(const instrumentation::Instrumentation* instr,
                     Thread* self, mirror::ArtMethod* method, mirror::Object* obj,
                     uint32_t dex_pc) OVERRIDE
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      JValue value;
      instr->MethodExitEvent(self, obj, method, dex_pc, value);
    }
    bool DidListenerReceiveEvent(const TestInstrumentationListener& listener) OVERRIDE
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      return listener.received_method_exit_event;
    }
  };
  MethodExitEventChecker checker;
  TestEvent(checker);
}

TEST_F(InstrumentationTest, ExceptionCaughtEvent) {
  class ExceptionCaughtEventChecker : public ITestChecker {
   public:
    uint32_t GetInstrumentationEvent() OVERRIDE
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      return instrumentation::Instrumentation::kExceptionCaught;
    }
    bool HasEventListener(const instrumentation::Instrumentation* instr) OVERRIDE
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      return instr->HasExceptionCaughtListeners();
    }
    void ReportEvent(const instrumentation::Instrumentation* instr, Thread* self,
                     mirror::ArtMethod* method ATTRIBUTE_UNUSED,
                     mirror::Object* obj ATTRIBUTE_UNUSED,
                     uint32_t dex_pc ATTRIBUTE_UNUSED) OVERRIDE
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      ThrowArithmeticExceptionDivideByZero();
      mirror::Throwable* event_exception = self->GetException();
      instr->ExceptionCaughtEvent(self, event_exception);
      self->ClearException();
    }
    bool DidListenerReceiveEvent(const TestInstrumentationListener& listener) OVERRIDE
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      return listener.received_exception_caught_event;
    }
  };
  ExceptionCaughtEventChecker checker;
  TestEvent(checker);
}

TEST_F(InstrumentationTest, BackwardBranchEvent) {
  class BackwardBranchEventChecker : public ITestChecker {
   public:
    uint32_t GetInstrumentationEvent() OVERRIDE
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      return instrumentation::Instrumentation::kBackwardBranch;
    }
    bool HasEventListener(const instrumentation::Instrumentation* instr) OVERRIDE
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      return instr->HasBackwardBranchListeners();
    }
    void ReportEvent(const instrumentation::Instrumentation* instr,
                     Thread* self, mirror::ArtMethod* method, mirror::Object* obj ATTRIBUTE_UNUSED,
                     uint32_t dex_pc) OVERRIDE
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      instr->BackwardBranch(self, method, dex_pc);
    }
    bool DidListenerReceiveEvent(const TestInstrumentationListener& listener) OVERRIDE
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      return listener.received_backward_branch_event;
    }
  };
  BackwardBranchEventChecker checker;
  TestEvent(checker);
}

TEST_F(InstrumentationTest, FieldReadEvent) {
  class FieldReadEventChecker : public ITestChecker {
   public:
    uint32_t GetInstrumentationEvent() OVERRIDE
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      return instrumentation::Instrumentation::kFieldRead;
    }
    bool HasEventListener(const instrumentation::Instrumentation* instr) OVERRIDE
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      return instr->HasFieldReadListeners();
    }
    void ReportEvent(const instrumentation::Instrumentation* instr,
                     Thread* self, mirror::ArtMethod* method, mirror::Object* obj,
                     uint32_t dex_pc) OVERRIDE
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      instr->FieldReadEvent(self, obj, method, dex_pc, nullptr);
    }
    bool DidListenerReceiveEvent(const TestInstrumentationListener& listener) OVERRIDE
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      return listener.received_field_read_event;
    }
  };
  FieldReadEventChecker checker;
  TestEvent(checker);
}

TEST_F(InstrumentationTest, FieldWriteEvent) {
  class FieldWriteEventChecker : public ITestChecker {
   public:
    uint32_t GetInstrumentationEvent() OVERRIDE
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      return instrumentation::Instrumentation::kFieldWritten;
    }
    bool HasEventListener(const instrumentation::Instrumentation* instr) OVERRIDE
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      return instr->HasFieldWriteListeners();
    }
    void ReportEvent(const instrumentation::Instrumentation* instr,
                     Thread* self, mirror::ArtMethod* method, mirror::Object* obj,
                     uint32_t dex_pc) OVERRIDE
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      JValue value;
      instr->FieldWriteEvent(self, obj, method, dex_pc, nullptr, value);
    }
    bool DidListenerReceiveEvent(const TestInstrumentationListener& listener) OVERRIDE
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      return listener.received_field_written_event;
    }
  };
  FieldWriteEventChecker checker;
  TestEvent(checker);
}

#if 0
// TODO add Instrumentation::HasMethodUnwindListeners.
TEST_F(InstrumentationTest, MethodUnwindEvent) {
  class MethodUnwindEventChecker : public ITestChecker {
   public:
    uint32_t GetInstrumentationEvent() OVERRIDE
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      return instrumentation::Instrumentation::kMethodUnwind;
    }
    bool HasEventListener(const instrumentation::Instrumentation* instr) OVERRIDE
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      return instr->HasMethodUnwindListeners();
    }
    void ReportEvent(const instrumentation::Instrumentation* instr) OVERRIDE
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      JValue value;
      instr->MethodUnwindEvent(nullptr, nullptr, nullptr, DexFile::kDexNoIndex);
    }
    bool DidListenerReceiveEvent(const TestInstrumentationListener& listener) OVERRIDE
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      return listener.received_method_unwind_event;
  };
  MethodUnwindEventChecker checker;
  TestEvent(checker);
}
#endif

TEST_F(InstrumentationTest, DeoptimizeDirectMethod) {
  ScopedObjectAccess soa(Thread::Current());
  jobject class_loader = LoadDex("Instrumentation");
  Runtime* const runtime = Runtime::Current();
  instrumentation::Instrumentation* instr = runtime->GetInstrumentation();
  ClassLinker* class_linker = runtime->GetClassLinker();
  StackHandleScope<2> hs(soa.Self());
  Handle<mirror::ClassLoader> loader(hs.NewHandle(soa.Decode<mirror::ClassLoader*>(class_loader)));
  mirror::Class* klass = class_linker->FindClass(soa.Self(), "LInstrumentation;", loader);
  ASSERT_TRUE(klass != nullptr);
  Handle<mirror::ArtMethod> method_to_deoptimize(
      hs.NewHandle(klass->FindDeclaredDirectMethod("instanceMethod", "()V")));
  ASSERT_TRUE(method_to_deoptimize.Get() != nullptr);
//  const void* const quick_entrypoint = method_to_deoptimize->GetEntryPointFromQuickCompiledCode();
//  ASSERT_TRUE(quick_entrypoint != nullptr);

  EXPECT_FALSE(instr->AreAllMethodsDeoptimized());
  EXPECT_FALSE(instr->IsDeoptimized(method_to_deoptimize.Get()));

  DeoptimizeMethod(soa.Self(), method_to_deoptimize, true);

  EXPECT_FALSE(instr->AreAllMethodsDeoptimized());
  EXPECT_TRUE(instr->AreExitStubsInstalled());
  EXPECT_TRUE(instr->IsDeoptimized(method_to_deoptimize.Get()));
//  const void* deoptimized_quick_entrypoint = method_to_deoptimize->GetEntryPointFromQuickCompiledCode();
//  EXPECT_TRUE(class_linker->IsQuickToInterpreterBridge(deoptimized_quick_entrypoint));

  UndeoptimizeMethod(soa.Self(), method_to_deoptimize, true);

  EXPECT_FALSE(instr->AreAllMethodsDeoptimized());
  EXPECT_FALSE(instr->IsDeoptimized(method_to_deoptimize.Get()));
//  EXPECT_EQ(quick_entrypoint, method_to_deoptimize->GetEntryPointFromQuickCompiledCode());
}

// TODO check entrypoints of different kinds of methods to validate.
TEST_F(InstrumentationTest, FullDeoptimization) {
  ScopedObjectAccess soa(Thread::Current());
  Runtime* const runtime = Runtime::Current();
  instrumentation::Instrumentation* instr = runtime->GetInstrumentation();
  EXPECT_FALSE(instr->AreAllMethodsDeoptimized());

  DeoptimizeEverything(soa.Self(), true);

  EXPECT_TRUE(instr->AreAllMethodsDeoptimized());
  EXPECT_TRUE(instr->AreExitStubsInstalled());

  UndeoptimizeEverything(soa.Self(), true);

  EXPECT_FALSE(instr->AreAllMethodsDeoptimized());
}

TEST_F(InstrumentationTest, MixedDeoptimization) {
  ScopedObjectAccess soa(Thread::Current());
  jobject class_loader = LoadDex("Instrumentation");
  Runtime* const runtime = Runtime::Current();
  instrumentation::Instrumentation* instr = runtime->GetInstrumentation();
  ClassLinker* class_linker = runtime->GetClassLinker();
  StackHandleScope<2> hs(soa.Self());
  Handle<mirror::ClassLoader> loader(hs.NewHandle(soa.Decode<mirror::ClassLoader*>(class_loader)));
  mirror::Class* klass = class_linker->FindClass(soa.Self(), "LInstrumentation;", loader);
  ASSERT_TRUE(klass != nullptr);
  Handle<mirror::ArtMethod> method_to_deoptimize(
      hs.NewHandle(klass->FindDeclaredDirectMethod("instanceMethod", "()V")));
  ASSERT_TRUE(method_to_deoptimize.Get() != nullptr);
//  const void* const quick_entrypoint = method_to_deoptimize->GetEntryPointFromQuickCompiledCode();
//  ASSERT_TRUE(quick_entrypoint != nullptr);

  EXPECT_FALSE(instr->AreAllMethodsDeoptimized());
  EXPECT_FALSE(instr->IsDeoptimized(method_to_deoptimize.Get()));

  DeoptimizeMethod(soa.Self(), method_to_deoptimize, true);

  EXPECT_FALSE(instr->AreAllMethodsDeoptimized());
  EXPECT_TRUE(instr->AreExitStubsInstalled());
  EXPECT_TRUE(instr->IsDeoptimized(method_to_deoptimize.Get()));
//  const void* deoptimized_quick_entrypoint = method_to_deoptimize->GetEntryPointFromQuickCompiledCode();
//  EXPECT_TRUE(class_linker->IsQuickToInterpreterBridge(deoptimized_quick_entrypoint));

  DeoptimizeEverything(soa.Self(), false);

  EXPECT_TRUE(instr->AreAllMethodsDeoptimized());
  EXPECT_TRUE(instr->AreExitStubsInstalled());
  EXPECT_TRUE(instr->IsDeoptimized(method_to_deoptimize.Get()));

  UndeoptimizeEverything(soa.Self(), false);

  EXPECT_FALSE(instr->AreAllMethodsDeoptimized());
  EXPECT_TRUE(instr->AreExitStubsInstalled());
  EXPECT_TRUE(instr->IsDeoptimized(method_to_deoptimize.Get()));

  UndeoptimizeMethod(soa.Self(), method_to_deoptimize, true);

  EXPECT_FALSE(instr->AreAllMethodsDeoptimized());
  EXPECT_FALSE(instr->IsDeoptimized(method_to_deoptimize.Get()));
//  EXPECT_EQ(quick_entrypoint, method_to_deoptimize->GetEntryPointFromQuickCompiledCode());
}
}  // namespace art
