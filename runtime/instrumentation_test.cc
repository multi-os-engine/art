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
namespace instrumentation {

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

class TestChecker {
 public:
  explicit TestChecker(uint32_t event_type) : event_type_(event_type) {}
  ~TestChecker() {}

  uint32_t GetInstrumentationEvent() const {
    return event_type_;
  }

  bool HasEventListener(const instrumentation::Instrumentation* instr) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    switch (event_type_) {
      case instrumentation::Instrumentation::kMethodEntered:
        return instr->HasMethodEntryListeners();
      case instrumentation::Instrumentation::kMethodExited:
        return instr->HasMethodExitListeners();
      case instrumentation::Instrumentation::kMethodUnwind:
        return instr->HasMethodUnwindListeners();
      case instrumentation::Instrumentation::kDexPcMoved:
        return instr->HasDexPcListeners();
      case instrumentation::Instrumentation::kFieldRead:
        return instr->HasFieldReadListeners();
      case instrumentation::Instrumentation::kFieldWritten:
        return instr->HasFieldWriteListeners();
      case instrumentation::Instrumentation::kExceptionCaught:
        return instr->HasExceptionCaughtListeners();
      case instrumentation::Instrumentation::kBackwardBranch:
        return instr->HasBackwardBranchListeners();
      default:
        LOG(FATAL) << "Unknown instrumentation event " << event_type_;
        UNREACHABLE();
    }
  }

  void ReportEvent(const instrumentation::Instrumentation* instr, Thread* self,
                   mirror::ArtMethod* method, mirror::Object* obj, uint32_t dex_pc)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    switch (event_type_) {
      case instrumentation::Instrumentation::kMethodEntered:
        instr->MethodEnterEvent(self, obj, method, dex_pc);
        break;
      case instrumentation::Instrumentation::kMethodExited: {
        JValue value;
        instr->MethodExitEvent(self, obj, method, dex_pc, value);
        break;
      }
      case instrumentation::Instrumentation::kMethodUnwind:
        instr->MethodUnwindEvent(self, obj, method, dex_pc);
        break;
      case instrumentation::Instrumentation::kDexPcMoved:
        instr->DexPcMovedEvent(self, obj, method, dex_pc);
        break;
      case instrumentation::Instrumentation::kFieldRead:
        instr->FieldReadEvent(self, obj, method, dex_pc, nullptr);
        break;
      case instrumentation::Instrumentation::kFieldWritten: {
        JValue value;
        instr->FieldWriteEvent(self, obj, method, dex_pc, nullptr, value);
        break;
      }
      case instrumentation::Instrumentation::kExceptionCaught: {
        ThrowArithmeticExceptionDivideByZero();
        mirror::Throwable* event_exception = self->GetException();
        instr->ExceptionCaughtEvent(self, event_exception);
        self->ClearException();
        break;
      }
      case instrumentation::Instrumentation::kBackwardBranch:
        instr->BackwardBranch(self, method, dex_pc);
        break;
      default:
        LOG(FATAL) << "Unknown instrumentation event " << event_type_;
        UNREACHABLE();
    }
  }

  bool DidListenerReceiveEvent(const TestInstrumentationListener& listener) {
    switch (event_type_) {
      case instrumentation::Instrumentation::kMethodEntered:
        return listener.received_method_enter_event;
      case instrumentation::Instrumentation::kMethodExited:
        return listener.received_method_exit_event;
      case instrumentation::Instrumentation::kMethodUnwind:
        return listener.received_method_unwind_event;
      case instrumentation::Instrumentation::kDexPcMoved:
        return listener.received_dex_pc_moved_event;
      case instrumentation::Instrumentation::kFieldRead:
        return listener.received_field_read_event;
      case instrumentation::Instrumentation::kFieldWritten:
        return listener.received_field_written_event;
      case instrumentation::Instrumentation::kExceptionCaught:
        return listener.received_exception_caught_event;
      case instrumentation::Instrumentation::kBackwardBranch:
        return listener.received_backward_branch_event;
      default:
        LOG(FATAL) << "Unknown instrumentation event " << event_type_;
        UNREACHABLE();
    }
  }

 private:
  // The instrumentation event we want to test.
  const uint32_t event_type_;
};

class InstrumentationTest : public CommonRuntimeTest {
 public:
  void CheckConfigureStubs(const std::string& key, Instrumentation::InstrumentationLevel level) {
    ScopedObjectAccess soa(Thread::Current());
    instrumentation::Instrumentation* instr = Runtime::Current()->GetInstrumentation();

    {
      soa.Self()->TransitionFromRunnableToSuspended(kSuspended);
      Runtime* runtime = Runtime::Current();
      runtime->GetThreadList()->SuspendAll("Instrumentation::ConfigureStubs");
      instr->ConfigureStubs(key, level);
      runtime->GetThreadList()->ResumeAll();
      soa.Self()->TransitionFromSuspendedToRunnable();
    }
  }

  Instrumentation::InstrumentationLevel GetCurrentInstrumentationLevel() {
    return Runtime::Current()->GetInstrumentation()->GetCurrentInstrumentationLevel();
  }

  size_t GetInstrumentationUserCount() {
    ScopedObjectAccess soa(Thread::Current());
    return Runtime::Current()->GetInstrumentation()->requested_instrumentation_levels_.size();
  }

  void TestEvent(TestChecker& checker) {
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

// Test instrumentation listeners for each event.
TEST_F(InstrumentationTest, MethodEntryEvent) {
  TestChecker checker(instrumentation::Instrumentation::kMethodEntered);
  TestEvent(checker);
}

TEST_F(InstrumentationTest, MethodExitEvent) {
  TestChecker checker(instrumentation::Instrumentation::kMethodExited);
  TestEvent(checker);
}

TEST_F(InstrumentationTest, MethodUnwindEvent) {
  TestChecker checker(instrumentation::Instrumentation::kMethodUnwind);
  TestEvent(checker);
}

TEST_F(InstrumentationTest, DexPcMovedEvent) {
  TestChecker checker(instrumentation::Instrumentation::kDexPcMoved);
  TestEvent(checker);
}

TEST_F(InstrumentationTest, FieldReadEvent) {
  TestChecker checker(instrumentation::Instrumentation::kFieldRead);
  TestEvent(checker);
}

TEST_F(InstrumentationTest, FieldWriteEvent) {
  TestChecker checker(instrumentation::Instrumentation::kFieldWritten);
  TestEvent(checker);
}

TEST_F(InstrumentationTest, ExceptionCaughtEvent) {
  TestChecker checker(instrumentation::Instrumentation::kExceptionCaught);
  TestEvent(checker);
}

TEST_F(InstrumentationTest, BackwardBranchEvent) {
  TestChecker checker(instrumentation::Instrumentation::kBackwardBranch);
  TestEvent(checker);
}

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

// We use a macro to print the line number where the test is failing.
// TODO format correctly
#define CHECK_INSTRUMENTATION(_level, _user_count)                                            \
  do {                                                                                        \
    Instrumentation* const instr = Runtime::Current()->GetInstrumentation();                  \
    bool interpreter = (_level == Instrumentation::InstrumentationLevel::kInstrumentWithInterpreter); \
    EXPECT_EQ(_level, GetCurrentInstrumentationLevel());                                      \
    EXPECT_EQ(_user_count, GetInstrumentationUserCount());                                    \
    if (instr->IsForcedInterpretOnly()) {                                                     \
      EXPECT_TRUE(instr->InterpretOnly());                                                    \
    } else if (interpreter) {                                                                                  \
      EXPECT_TRUE(instr->InterpretOnly());                                                        \
    } else {                                                                                           \
      EXPECT_FALSE(instr->InterpretOnly()); \
    } \
    if (interpreter) { \
      EXPECT_TRUE(instr->AreAllMethodsDeoptimized()); \
    } else { \
      EXPECT_FALSE(instr->AreAllMethodsDeoptimized()); \
    }\
  } while (false)

TEST_F(InstrumentationTest, ConfigureStubs_Nothing) {
  const std::string kClientKey("TestClient");
  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentNothing, 0U);

  // Check no-op.
  CheckConfigureStubs(kClientKey, Instrumentation::InstrumentationLevel::kInstrumentNothing);
  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentNothing, 0U);
}

TEST_F(InstrumentationTest, ConfigureStubs_InstrumentationStubs) {
  const std::string kClientKey("TestClient");
  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentNothing, 0U);

  // Check we can switch to instrumentation stubs
  CheckConfigureStubs(kClientKey, Instrumentation::InstrumentationLevel::kInstrumentWithInstrumentationStubs);
  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentWithInstrumentationStubs, 1U);

  // Check we can disable instrumentation.
  CheckConfigureStubs(kClientKey, Instrumentation::InstrumentationLevel::kInstrumentNothing);
  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentNothing, 0U);
}

TEST_F(InstrumentationTest, ConfigureStubs_Interpreter) {
  const std::string kClientKey("TestClient");

  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentNothing, 0U);

  // Check we can switch to interpreter
  CheckConfigureStubs(kClientKey, Instrumentation::InstrumentationLevel::kInstrumentWithInterpreter);
  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentWithInterpreter, 1U);

  // Check we can disable instrumentation.
  CheckConfigureStubs(kClientKey, Instrumentation::InstrumentationLevel::kInstrumentNothing);
  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentNothing, 0U);
}

TEST_F(InstrumentationTest, ConfigureStubs_InstrumentationStubsToInterpreter) {
  const std::string kClientKey("TestClient");

  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentNothing, 0U);

  // Configure stubs with instrumentation stubs.
  CheckConfigureStubs(kClientKey, Instrumentation::InstrumentationLevel::kInstrumentWithInstrumentationStubs);
  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentWithInstrumentationStubs, 1U);

  // Configure stubs with interpreter.
  CheckConfigureStubs(kClientKey, Instrumentation::InstrumentationLevel::kInstrumentWithInterpreter);
  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentWithInterpreter, 1U);

  // Check we can disable instrumentation.
  CheckConfigureStubs(kClientKey, Instrumentation::InstrumentationLevel::kInstrumentNothing);
  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentNothing, 0U);
}

TEST_F(InstrumentationTest, ConfigureStubs_InterpreterToInstrumentationStubs) {
  const std::string kClientKey("TestClient");

  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentNothing, 0U);

  // Configure stubs with interpreter.
  CheckConfigureStubs(kClientKey, Instrumentation::InstrumentationLevel::kInstrumentWithInterpreter);
  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentWithInterpreter, 1U);

  // Configure stubs with instrumentation stubs.
  CheckConfigureStubs(kClientKey, Instrumentation::InstrumentationLevel::kInstrumentWithInstrumentationStubs);
  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentWithInstrumentationStubs, 1U);

  // Check we can disable instrumentation.
  CheckConfigureStubs(kClientKey, Instrumentation::InstrumentationLevel::kInstrumentNothing);
  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentNothing, 0U);
}

TEST_F(InstrumentationTest, ConfigureStubs_InstrumentationStubsToInterpreterToInstrumentationStubs) {
  const std::string kClientKey("TestClient");

  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentNothing, 0U);

  // Configure stubs with instrumentation stubs.
  CheckConfigureStubs(kClientKey, Instrumentation::InstrumentationLevel::kInstrumentWithInstrumentationStubs);
  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentWithInstrumentationStubs, 1U);

  // Configure stubs with interpreter.
  CheckConfigureStubs(kClientKey, Instrumentation::InstrumentationLevel::kInstrumentWithInterpreter);
  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentWithInterpreter, 1U);

  // Configure stubs with instrumentation stubs again.
  CheckConfigureStubs(kClientKey, Instrumentation::InstrumentationLevel::kInstrumentWithInstrumentationStubs);
  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentWithInstrumentationStubs, 1U);

  // Check we can disable instrumentation.
  CheckConfigureStubs(kClientKey, Instrumentation::InstrumentationLevel::kInstrumentNothing);
  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentNothing, 0U);
}

TEST_F(InstrumentationTest, MultiConfigureStubs_Nothing) {
  const std::string kClientOneKey("TestClientOne");
  const std::string kClientTwoKey("TestClientTwo");

  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentNothing, 0U);

  // Check kInstrumentNothing with two clients.
  CheckConfigureStubs(kClientOneKey, Instrumentation::InstrumentationLevel::kInstrumentNothing);
  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentNothing, 0U);

  CheckConfigureStubs(kClientTwoKey, Instrumentation::InstrumentationLevel::kInstrumentNothing);
  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentNothing, 0U);
}

TEST_F(InstrumentationTest, MultiConfigureStubs_InstrumentationStubs) {
  const std::string kClientOneKey("TestClientOne");
  const std::string kClientTwoKey("TestClientTwo");

  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentNothing, 0U);

  // Configure stubs with instrumentation stubs for 1st client.
  CheckConfigureStubs(kClientOneKey, Instrumentation::InstrumentationLevel::kInstrumentWithInstrumentationStubs);
  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentWithInstrumentationStubs, 1U);

  // Configure stubs with instrumentation stubs for 2nd client.
  CheckConfigureStubs(kClientTwoKey, Instrumentation::InstrumentationLevel::kInstrumentWithInstrumentationStubs);
  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentWithInstrumentationStubs, 2U);

  // 1st client requests instrumentation deactivation but 2nd client still needs
  // instrumentation stubs.
  CheckConfigureStubs(kClientOneKey, Instrumentation::InstrumentationLevel::kInstrumentNothing);
  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentWithInstrumentationStubs, 1U);

  // 2nd client requests instrumentation deactivation
  CheckConfigureStubs(kClientTwoKey, Instrumentation::InstrumentationLevel::kInstrumentNothing);
  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentNothing, 0U);
}

TEST_F(InstrumentationTest, MultiConfigureStubs_Interpreter) {
  const std::string kClientOneKey("TestClientOne");
  const std::string kClientTwoKey("TestClientTwo");

  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentNothing, 0U);

  // Configure stubs with interpreter for 1st client.
  CheckConfigureStubs(kClientOneKey, Instrumentation::InstrumentationLevel::kInstrumentWithInterpreter);
  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentWithInterpreter, 1U);

  // Configure stubs with interpreter for 2nd client.
  CheckConfigureStubs(kClientTwoKey, Instrumentation::InstrumentationLevel::kInstrumentWithInterpreter);
  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentWithInterpreter, 2U);

  // 1st client requests instrumentation deactivation but 2nd client still needs interpreter.
  CheckConfigureStubs(kClientOneKey, Instrumentation::InstrumentationLevel::kInstrumentNothing);
  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentWithInterpreter, 1U);

  // 2nd client requests instrumentation deactivation
  CheckConfigureStubs(kClientTwoKey, Instrumentation::InstrumentationLevel::kInstrumentNothing);
  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentNothing, 0U);
}

TEST_F(InstrumentationTest, MultiConfigureStubs_InstrumentationStubsThenInterpreter) {
  const std::string kClientOneKey("TestClientOne");
  const std::string kClientTwoKey("TestClientTwo");

  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentNothing, 0U);

  // Configure stubs with instrumentation stubs for 1st client.
  CheckConfigureStubs(kClientOneKey, Instrumentation::InstrumentationLevel::kInstrumentWithInstrumentationStubs);
  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentWithInstrumentationStubs, 1U);

  // Configure stubs with interpreter for 2nd client.
  CheckConfigureStubs(kClientTwoKey, Instrumentation::InstrumentationLevel::kInstrumentWithInterpreter);
  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentWithInterpreter, 2U);

  // 1st client requests instrumentation deactivation but 2nd client still needs interpreter.
  CheckConfigureStubs(kClientOneKey, Instrumentation::InstrumentationLevel::kInstrumentNothing);
  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentWithInterpreter, 1U);

  // 2nd client requests instrumentation deactivation
  CheckConfigureStubs(kClientTwoKey, Instrumentation::InstrumentationLevel::kInstrumentNothing);
  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentNothing, 0U);
}

TEST_F(InstrumentationTest, MultiConfigureStubs_InterpreterThenInstrumentationStubs) {
  const std::string kClientOneKey("TestClientOne");
  const std::string kClientTwoKey("TestClientTwo");

  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentNothing, 0U);

  // Configure stubs with interpreter for 1st client.
  CheckConfigureStubs(kClientOneKey, Instrumentation::InstrumentationLevel::kInstrumentWithInterpreter);
  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentWithInterpreter, 1U);

  // Configure stubs with instrumentation stubs for 2nd client.
  CheckConfigureStubs(kClientTwoKey, Instrumentation::InstrumentationLevel::kInstrumentWithInstrumentationStubs);
  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentWithInterpreter, 2U);

  // 1st client requests instrumentation deactivation but 2nd client still needs
  // instrumentation stubs.
  CheckConfigureStubs(kClientOneKey, Instrumentation::InstrumentationLevel::kInstrumentNothing);
  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentWithInstrumentationStubs, 1U);

  // 2nd client requests instrumentation deactivation
  CheckConfigureStubs(kClientTwoKey, Instrumentation::InstrumentationLevel::kInstrumentNothing);
  CHECK_INSTRUMENTATION(Instrumentation::InstrumentationLevel::kInstrumentNothing, 0U);
}

}  // namespace instrumentation
}  // namespace art
