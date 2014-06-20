/*
 * Copyright (C) 2014 The Android Open Source Project
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

#include "barrier.h"
#include "monitor.h"

#include <string>

#include "atomic.h"
#include "common_runtime_test.h"
#include "handle_scope-inl.h"
#include "mirror/class-inl.h"
#include "mirror/string-inl.h"  // Strings are easiest to allocate
#include "thread_pool.h"
#include "utils.h"

namespace art {

class MonitorTest : public CommonRuntimeTest {
 protected:
  void SetUpRuntimeOptions(Runtime::Options *options) OVERRIDE {
    // Use a smaller heap
    for (std::pair<std::string, const void*>& pair : *options) {
      if (pair.first.find("-Xmx") == 0) {
        pair.first = "-Xmx4M";  // Smallest we can go.
      }
    }
    options->push_back(std::make_pair("-Xint", nullptr));
  }
 public:
  std::unique_ptr<Monitor> monitor_;
  Handle<mirror::String> object_;
  Handle<mirror::String> second_object_;
  Handle<mirror::String> watchdog_object_;
  // One exception test is for waiting on another Thread's lock. This is used to race-free &
  // loop-free pass
  Thread* thread_;
  std::unique_ptr<Barrier> barrier_;
  std::unique_ptr<Barrier> complete_barrier_;
  bool completed_;
};

// Fill the heap.
static const size_t kMaxHandles = 1000000;  // Use arbitrary large amount for now.
static void FillHeap(Thread* self, ClassLinker* class_linker,
                     std::unique_ptr<StackHandleScope<kMaxHandles>>* hsp,
                     std::vector<Handle<mirror::Object>>* handles)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  Runtime::Current()->GetHeap()->SetIdealFootprint(1 * GB);

  hsp->reset(new StackHandleScope<kMaxHandles>(self));
  // Class java.lang.Object.
  Handle<mirror::Class> c((*hsp)->NewHandle(class_linker->FindSystemClass(self,
                                                                       "Ljava/lang/Object;")));
  // Array helps to fill memory faster.
  Handle<mirror::Class> ca((*hsp)->NewHandle(class_linker->FindSystemClass(self,
                                                                        "[Ljava/lang/Object;")));

  // Start allocating with 128K
  size_t length = 128 * KB / 4;
  while (length > 10) {
    Handle<mirror::Object> h((*hsp)->NewHandle<mirror::Object>(
        mirror::ObjectArray<mirror::Object>::Alloc(self, ca.Get(), length / 4)));
    if (self->IsExceptionPending() || h.Get() == nullptr) {
      self->ClearException();

      // Try a smaller length
      length = length / 8;
      // Use at most half the reported free space.
      size_t mem = Runtime::Current()->GetHeap()->GetFreeMemory();
      if (length * 8 > mem) {
        length = mem / 8;
      }
    } else {
      handles->push_back(h);
    }
  }
  LOG(INFO) << "Used " << handles->size() << " arrays to fill space.";

  // Allocate simple objects till it fails.
  while (!self->IsExceptionPending()) {
    Handle<mirror::Object> h = (*hsp)->NewHandle<mirror::Object>(c->AllocObject(self));
    if (!self->IsExceptionPending() && h.Get() != nullptr) {
      handles->push_back(h);
    }
  }
  self->ClearException();
}

// Check that an exception can be thrown correctly.
// This test is potentially racy, but the timeout is long enough that it should work.

class CreateTask : public Task {
 public:
  explicit CreateTask(MonitorTest* monitor_test) : monitor_test_(monitor_test) {}

  void Run(Thread* self) {
    LOG(INFO) << "CreateTask running with thread " << self << " " << Thread::Current();

    {
      ScopedObjectAccess soa(self);

      monitor_test_->thread_ = self;        // Pass the Thread.
      monitor_test_->object_.Get()->MonitorEnter(self);     // Lock the object. This should transition
      LockWord lock_after = monitor_test_->object_.Get()->GetLockWord(false);     // it to thinLocked.
      LockWord::LockState new_state = lock_after.GetState();

      // Cannot use ASSERT only, as analysis thinks we'll keep holding the mutex.
      if (LockWord::LockState::kThinLocked != new_state) {
        monitor_test_->object_.Get()->MonitorExit(self);         // To appease analysis.
        ASSERT_EQ(LockWord::LockState::kThinLocked, new_state);  // To fail the test.
        return;
      }

      // Force a fat lock by running identity hashcode to fill up lock word.
      monitor_test_->object_.Get()->IdentityHashCode();
      LockWord lock_after2 = monitor_test_->object_.Get()->GetLockWord(false);
      LockWord::LockState new_state2 = lock_after2.GetState();

      // Cannot use ASSERT only, as analysis thinks we'll keep holding the mutex.
      if (LockWord::LockState::kFatLocked != new_state2) {
        monitor_test_->object_.Get()->MonitorExit(self);         // To appease analysis.
        ASSERT_EQ(LockWord::LockState::kFatLocked, new_state2);  // To fail the test.
        return;
      }
    }  // Need to drop the mutator lock to use the barrier.

    monitor_test_->barrier_->Wait(self);           // Let the other thread know we're done.

    {
      ScopedObjectAccess soa(self);

      LOG(INFO) << "CreateTask: Giving time.";

      // Give the other thread a wee bit of time to start. 10ms seems fine, as it's large enough
      // for a thread change and some work.
      NanoSleep(10U * 1000 * 1000);

      LOG(INFO) << "CreateTask: Gonna wait now.";

      // Now try to Wait on the Monitor.
      Monitor::Wait(self, monitor_test_->object_.Get(), 250, 0, true, ThreadState::kTimedWaiting);
      // We should not get an exception.
      EXPECT_FALSE(self->IsExceptionPending());
    }

    monitor_test_->complete_barrier_->Wait(self);  // Wait for test completion.

    {
      ScopedObjectAccess soa(self);
      monitor_test_->object_.Get()->MonitorExit(self);  // Release the object. Appeases analysis.
    }
  }

  void Finalize() {
    delete this;
  }

 private:
  MonitorTest* monitor_test_;
};


class UseTask : public Task {
 public:
  UseTask(MonitorTest* monitor_test, bool second_object, int64_t millis) :
      monitor_test_(monitor_test), second_object_(second_object), millis_(millis) {}

  void Run(Thread* self) {
    LOG(INFO) << "UseTask running with thread " << self << " " << Thread::Current();

    monitor_test_->barrier_->Wait(self);  // Wait for the other thread to set up the monitor.

    {
      ScopedObjectAccess soa(self);

      LOG(INFO) << "UseTask: Giving time.";

      // Give the other test a chance to get the mutator lock. 2ms seems fine.
      NanoSleep(2U * 1000 * 1000);

      LOG(INFO) << "UseTask: Ready.";

      // Now try to Wait on the Monitor.
      if (second_object_) {
        monitor_test_->second_object_.Get()->MonitorEnter(self);

        // Need to duplicate this as safety analysis can't track things precisely.
        Monitor::Wait(self, monitor_test_->second_object_.Get(), millis_, 0, true,
                      ThreadState::kTimedWaiting);

        monitor_test_->second_object_.Get()->MonitorExit(self);
      } else {
        Monitor::Wait(self, monitor_test_->object_.Get(), millis_, 0, true,
                      ThreadState::kTimedWaiting);
      }

      // We should get an exception.
      EXPECT_TRUE(self->IsExceptionPending());
      LOG(INFO) << PrettyTypeOf(self->GetException(nullptr));
      self->ClearException();
    }

    monitor_test_->complete_barrier_->Wait(self);  // Wait for test completion.
  }

  void Finalize() {
    delete this;
  }

 private:
  MonitorTest* monitor_test_;
  bool second_object_;  // Whether to lock the first object or the second.
                        // First one is necessary to force owner != self.
                        // Second one is to let us test the invalid waiting range.
  int64_t millis_;
};

class WatchdogTask : public Task {
 public:
  explicit WatchdogTask(MonitorTest* monitor_test) : monitor_test_(monitor_test) {}

  void Run(Thread* self) {
    LOG(INFO) << "Watchdog running with thread " << self;

    ScopedObjectAccess soa(self);

    monitor_test_->watchdog_object_.Get()->MonitorEnter(self);        // Lock the object.

    monitor_test_->watchdog_object_.Get()->Wait(self, 30 * 1000, 0);  // Wait for 30s, or being
                                                                      // woken up.

    monitor_test_->watchdog_object_.Get()->MonitorExit(self);         // Release the lock.

    if (!monitor_test_->completed_) {
      LOG(FATAL) << "Watchdog timeout!";
    }
  }

  void Finalize() {
    delete this;
  }

 private:
  MonitorTest* monitor_test_;
};

static void CommonWaitSetup(MonitorTest* test, ClassLinker* class_linker, bool second_object,
                            int64_t millis, const char* pool_name) {
  // First create the object we lock. String is easiest.
  StackHandleScope<3> hs(Thread::Current());
  {
    ScopedObjectAccess soa(Thread::Current());
    test->object_ = hs.NewHandle(mirror::String::AllocFromModifiedUtf8(Thread::Current(),
                                                                       "hello, world!"));
    if (second_object) {
    test->second_object_ = hs.NewHandle(mirror::String::AllocFromModifiedUtf8(Thread::Current(),
                                                                              "hello, world!"));
    }
    test->watchdog_object_ = hs.NewHandle(mirror::String::AllocFromModifiedUtf8(Thread::Current(),
                                                                                "hello, world!"));
  }

  // Create the barrier used to synchronize.
  test->barrier_ = std::unique_ptr<Barrier>(new Barrier(2));
  test->complete_barrier_ = std::unique_ptr<Barrier>(new Barrier(3));
  test->completed_ = false;

  // Fill the heap.
  std::unique_ptr<StackHandleScope<kMaxHandles>> hsp;
  std::vector<Handle<mirror::Object>> handles;
  {
    Thread* self = Thread::Current();
    ScopedObjectAccess soa(self);

    // Our job: Fill the heap, then try Wait.
    FillHeap(self, class_linker, &hsp, &handles);

    LOG(INFO) << "Filled the heap, releasing again.";

    // Now release everything.
    auto it = handles.begin();
    auto end = handles.end();

    for ( ; it != end; ++it) {
      it->Assign(nullptr);
    }
  }  // Need to drop the mutator lock to allow barriers.

  LOG(INFO) << "Ready to do test.";

  Thread* self = Thread::Current();
  ThreadPool thread_pool(pool_name, 10);
  thread_pool.AddTask(self, new CreateTask(test));
  thread_pool.AddTask(self, new UseTask(test, second_object, millis));
  thread_pool.AddTask(self, new WatchdogTask(test));
  thread_pool.StartWorkers(self);

  // Wait on completion barrier.
  test->complete_barrier_->Wait(Thread::Current());
  test->completed_ = true;

  // Wake the watchdog.
  {
    Thread* self = Thread::Current();
    ScopedObjectAccess soa(self);

    test->watchdog_object_.Get()->MonitorEnter(self);     // Lock the object.
    test->watchdog_object_.Get()->NotifyAll(self);        // Wake up waiting parties.
    test->watchdog_object_.Get()->MonitorExit(self);      // Release the lock.
  }

  thread_pool.StopWorkers(self);
}


// First test: throwing an exception when trying to wait in Monitor with another thread.
TEST_F(MonitorTest, CheckExceptionsWait1) {
  CommonWaitSetup(this, class_linker_, false, 250, "Monitor test thread pool");
}

// First test: throwing an exception when trying to wait in Monitor with another thread.
TEST_F(MonitorTest, CheckExceptionsWait2) {
  CommonWaitSetup(this, class_linker_, true, -1, "Monitor test thread pool 2");
}

}  // namespace art
