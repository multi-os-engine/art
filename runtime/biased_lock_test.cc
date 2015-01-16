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
#include "scoped_thread_state_change.h"
#include "thread_pool.h"
#include "utils.h"

namespace art {

class BiasedLockTest : public CommonRuntimeTest {
 public:
  Handle<mirror::String> object_;
  Handle<mirror::String> watchdog_object_;
  std::unique_ptr<Barrier> setup_barrier_;
  std::unique_ptr<Barrier> complete_barrier_;
  bool completed_;
};

class BiasedAndLockedTask : public Task {
 public:
  explicit BiasedAndLockedTask(BiasedLockTest* biased_lock_test) :
      biased_lock_test_(biased_lock_test) {}

  void Run(Thread* self) {
    {
      ScopedObjectAccess soa(self);

      // Lock the object. This should transition it to thin lock biasable state.
      biased_lock_test_->object_.Get()->MonitorEnter(self);
      LockWord lockword = biased_lock_test_->object_.Get()->GetLockWord(false);
      LockWord::LockState new_state = lockword.GetState();

      // Cannot use ASSERT only, as analysis thinks we'll keep holding the mutex.
      if (LockWord::LockState::kThinLockBiasable != new_state) {
        biased_lock_test_->object_.Get()->MonitorExit(self);         // To appease analysis.
        ASSERT_EQ(LockWord::LockState::kThinLockBiasable, new_state);  // To fail the test.
        return;
      }

      biased_lock_test_->object_.Get()->MonitorExit(self);
      uint32_t kProfilingCount = LockWord::kThinLockBiasableMaxProfCount;
      // Lock the object for kProfilingCount times to fill profiling counter.
      for (uint32_t i = 0; i < kProfilingCount; i++) {
        biased_lock_test_->object_.Get()->MonitorEnter(self);   // Lock the object.
        LockWord lw = biased_lock_test_->object_.Get()->GetLockWord(false);
        LockWord::LockState ls = lw.GetState();
        EXPECT_EQ(LockWord::LockState::kThinLockBiasable, ls);
        EXPECT_EQ(lw.ThinLockBiasableCount(), 1U);
        biased_lock_test_->object_.Get()->MonitorExit(self);
      }

      // Lock the object once more to change the lock state into biased state
      biased_lock_test_->object_.Get()->MonitorEnter(self);
      lockword = biased_lock_test_->object_.Get()->GetLockWord(false);
      new_state = lockword.GetState();
      EXPECT_EQ(LockWord::LockState::kBiasLocked, new_state);
      EXPECT_EQ(lockword.BiasLockCount(), 1U);
    }  // Need to drop the mutator lock to use the barrier.

    biased_lock_test_->setup_barrier_->Wait(self);   // Let the other thread know we're done.

    LockWord::LockState new_state;
    do {
      ScopedObjectAccess soa(self);
      LockWord lockword = biased_lock_test_->object_.Get()->GetLockWord(true);
      new_state = lockword.GetState();
    } while (LockWord::LockState::kThinLockBiasable == new_state);

    {
      ScopedObjectAccess soa(self);
      biased_lock_test_->object_.Get()->MonitorExit(self);  // Release the object.
    }

    biased_lock_test_->complete_barrier_->Wait(self);  // Wait for test completion.
  }

  void Finalize() {
    delete this;
  }

 private:
  BiasedLockTest* biased_lock_test_;
};

class BiasedAndUnlockedTask : public Task {
 public:
  explicit BiasedAndUnlockedTask(BiasedLockTest* biased_lock_test) :
      biased_lock_test_(biased_lock_test) {}

  void Run(Thread* self) {
    {
      ScopedObjectAccess soa(self);

      // Lock the object. This should transition it to ThinLockBiasable.
      biased_lock_test_->object_.Get()->MonitorEnter(self);
      LockWord lockword = biased_lock_test_->object_.Get()->GetLockWord(false);
      LockWord::LockState new_state = lockword.GetState();

      // Cannot use ASSERT only, as analysis thinks we'll keep holding the mutex.
      if (LockWord::LockState::kThinLockBiasable != new_state) {
        biased_lock_test_->object_.Get()->MonitorExit(self);         // To appease analysis.
        ASSERT_EQ(LockWord::LockState::kThinLockBiasable, new_state);  // To fail the test.
        return;
      }

      biased_lock_test_->object_.Get()->MonitorExit(self);

      uint32_t kProfilingCount = LockWord::kThinLockBiasableMaxProfCount;
      // Lock the object for kProfilingCount times to fill profiling counter.
      for (uint32_t i = 0; i < kProfilingCount; i++) {
        biased_lock_test_->object_.Get()->MonitorEnter(self);   // Lock the object.
        LockWord lw = biased_lock_test_->object_.Get()->GetLockWord(false);
        LockWord::LockState ls = lw.GetState();
        EXPECT_EQ(LockWord::LockState::kThinLockBiasable, ls);
        EXPECT_EQ(lw.ThinLockBiasableCount(), 1U);
        biased_lock_test_->object_.Get()->MonitorExit(self);
      }

      // Lock the object once more to change the lock state into biased state
      biased_lock_test_->object_.Get()->MonitorEnter(self);
      lockword = biased_lock_test_->object_.Get()->GetLockWord(false);
      new_state = lockword.GetState();
      EXPECT_EQ(LockWord::LockState::kBiasLocked, new_state);
      EXPECT_EQ(lockword.BiasLockCount(), 1U);
      biased_lock_test_->object_.Get()->MonitorExit(self);
    }  // Need to drop the mutator lock to use the barrier.

    biased_lock_test_->setup_barrier_->Wait(self);   // Let the other thread know we're done.

    LockWord::LockState new_state;
    do {
      ScopedObjectAccess soa(self);
      LockWord lockword = biased_lock_test_->object_.Get()->GetLockWord(true);
      new_state = lockword.GetState();
    } while (LockWord::LockState::kThinLockBiasable == new_state);

    biased_lock_test_->complete_barrier_->Wait(self);  // Wait for test completion.
  }

  void Finalize() {
    delete this;
  }

 private:
  BiasedLockTest* biased_lock_test_;
};

class ContenderTask : public Task {
 public:
  ContenderTask(BiasedLockTest* biased_lock_test, bool generate_hash_code) :
      biased_lock_test_(biased_lock_test), generate_hash_code_(generate_hash_code) {}

  void Run(Thread* self) {
    // Wait for the other thread to set up the biased lock.
    biased_lock_test_->setup_barrier_->Wait(self);

    {
      ScopedObjectAccess soa(self);
      if (generate_hash_code_) {
        // Generate hash code. This should revoke the biased lock into hash state or fat lock.
        biased_lock_test_->object_.Get()->IdentityHashCode();
      } else {
        // Lock the object. This should revoke the biased lock to thin/fat lock.
        biased_lock_test_->object_.Get()->MonitorEnter(self);
        biased_lock_test_->object_.Get()->MonitorExit(self);  // Release the object.
      }
      LockWord lockword = biased_lock_test_->object_.Get()->GetLockWord(true);
      LockWord::LockState new_state = lockword.GetState();
      // The lock state should be one of the thin not biasable, hash code or fat.
      ASSERT_NE(LockWord::LockState::kThinLockBiasable, new_state);
      ASSERT_NE(LockWord::LockState::kBiasLocked, new_state);
    }

    biased_lock_test_->complete_barrier_->Wait(self);  // Wait for test completion.
  }

  void Finalize() {
    delete this;
  }

 private:
  BiasedLockTest* biased_lock_test_;
  bool generate_hash_code_;
};

class WatchdogTask : public Task {
 public:
  explicit WatchdogTask(BiasedLockTest* biased_lock_test) : biased_lock_test_(biased_lock_test) {}

  void Run(Thread* self) {
    ScopedObjectAccess soa(self);

    biased_lock_test_->watchdog_object_.Get()->MonitorEnter(self);   // Lock the object.

    biased_lock_test_->watchdog_object_.Get()->Wait(self, 15 * 1000, 0);   // Wait for 30s, or being
                                                                           // woken up.

    biased_lock_test_->watchdog_object_.Get()->MonitorExit(self);   // Release the lock.

    if (!biased_lock_test_->completed_) {
      LOG(FATAL) << "Watchdog timeout!";
    }
  }

  void Finalize() {
    delete this;
  }

 private:
  BiasedLockTest* biased_lock_test_;
};

static void CommonWaitSetup(BiasedLockTest* test, bool revoke_biased_and_locked,
                            bool generate_hash_code, const char* pool_name) {
  // First create the object we lock. String is easiest.
  StackHandleScope<3> hs(Thread::Current());
  {
    ScopedObjectAccess soa(Thread::Current());
    test->object_ = hs.NewHandle(mirror::String::AllocFromModifiedUtf8(Thread::Current(),
                                                                       "hello, world!"));
    test->watchdog_object_ = hs.NewHandle(mirror::String::AllocFromModifiedUtf8(Thread::Current(),
                                                                                "hello, world!"));
  }

  // Create the barrier used to synchronize.
  test->setup_barrier_ = std::unique_ptr<Barrier>(new Barrier(2));
  test->complete_barrier_ = std::unique_ptr<Barrier>(new Barrier(3));
  test->completed_ = false;

  Thread* self = Thread::Current();
  ThreadPool thread_pool(pool_name, 3);
  if (revoke_biased_and_locked) {
    thread_pool.AddTask(self, new BiasedAndLockedTask(test));
  } else {
    thread_pool.AddTask(self, new BiasedAndUnlockedTask(test));
  }
  thread_pool.AddTask(self, new ContenderTask(test, generate_hash_code));
  thread_pool.AddTask(self, new WatchdogTask(test));
  thread_pool.StartWorkers(self);

  // Wait on completion barrier.
  test->complete_barrier_->Wait(Thread::Current());
  test->completed_ = true;

  // Wake the watchdog.
  {
    ScopedObjectAccess soa(self);

    test->watchdog_object_.Get()->MonitorEnter(self);     // Lock the object.
    test->watchdog_object_.Get()->NotifyAll(self);        // Wake up waiting parties.
    test->watchdog_object_.Get()->MonitorExit(self);      // Release the lock.
  }

  thread_pool.StopWorkers(self);
}

static void FromThinBiasableToBiasTest(BiasedLockTest* test) {
  // First create the object we lock. String is easiest.
  StackHandleScope<3> hs(Thread::Current());
  {
    ScopedObjectAccess soa(Thread::Current());
    test->object_ = hs.NewHandle(mirror::String::AllocFromModifiedUtf8(Thread::Current(),
                                                                       "object for lock test"));
  }

  {
    uint32_t kProfilingCount = LockWord::kThinLockBiasableMaxProfCount;
    Thread* self = Thread::Current();
    ScopedObjectAccess soa(self);
    LockWord lock = test->object_.Get()->GetLockWord(false);
    LockWord::LockState old_state = lock.GetState();
    EXPECT_EQ(LockWord::LockState::kUnlocked, old_state);

    test->object_.Get()->MonitorEnter(self);     // Lock the object.

    LockWord lock_after = test->object_.Get()->GetLockWord(false);
    LockWord::LockState new_state = lock_after.GetState();
    EXPECT_EQ(LockWord::LockState::kThinLockBiasable, new_state);
    EXPECT_EQ(lock_after.ThinLockBiasableCount(), 1U);   // Thin lock starts count at 1.

    test->object_.Get()->MonitorExit(self);      // Release the lock.

    // Lock the object for kProfilingCount times to fill profiling counter.
    for (uint32_t i = 0; i < kProfilingCount; i++) {
      test->object_.Get()->MonitorEnter(self);   // Lock the object.
      LockWord lw = test->object_.Get()->GetLockWord(false);
      LockWord::LockState ls = lw.GetState();
      EXPECT_EQ(LockWord::LockState::kThinLockBiasable, ls);
      EXPECT_EQ(lw.ThinLockBiasableCount(), 1U);
      test->object_.Get()->MonitorExit(self);
    }

    // Lock the object once more would change the lock state into kBiasLocked.
    test->object_.Get()->MonitorEnter(self);
    LockWord lock_after2 = test->object_.Get()->GetLockWord(false);
    LockWord::LockState new_state2 = lock_after2.GetState();
    EXPECT_EQ(LockWord::LockState::kBiasLocked, new_state2);
    EXPECT_EQ(lock_after2.BiasLockCount(), 1U);
    test->object_.Get()->MonitorExit(self);

    // The lock word should change into a hash code.
    test->object_.Get()->IdentityHashCode();

    LockWord lock_after3 = test->object_.Get()->GetLockWord(false);
    LockWord::LockState new_state3 = lock_after3.GetState();
    EXPECT_EQ(LockWord::LockState::kHashCode, new_state3);
  }
}

static void ThinBiasFatTest(BiasedLockTest* test) {
  // First create the object we lock. String is easiest.
  StackHandleScope<3> hs(Thread::Current());
  {
    ScopedObjectAccess soa(Thread::Current());
    test->object_ = hs.NewHandle(mirror::String::AllocFromModifiedUtf8(Thread::Current(),
                                                                       "object for lock test"));
  }

  {
    uint32_t kProfilingCount = LockWord::kThinLockBiasableMaxProfCount;
    Thread* self = Thread::Current();
    ScopedObjectAccess soa(self);
    LockWord lock = test->object_.Get()->GetLockWord(false);
    LockWord::LockState old_state = lock.GetState();
    EXPECT_EQ(LockWord::LockState::kUnlocked, old_state);

    test->object_.Get()->MonitorEnter(self);     // Lock the object.

    LockWord lock_after = test->object_.Get()->GetLockWord(false);
    LockWord::LockState new_state = lock_after.GetState();
    EXPECT_EQ(LockWord::LockState::kThinLockBiasable, new_state);
    EXPECT_EQ(lock_after.ThinLockBiasableCount(), 1U);   // Thin lock starts count at 1.

    test->object_.Get()->MonitorExit(self);      // Release the lock.

    // Lock the object for kProfilingCount times to fill profiling counter.
    for (uint32_t i = 0; i < kProfilingCount; i++) {
      test->object_.Get()->MonitorEnter(self);   // Lock the object.
      LockWord lw = test->object_.Get()->GetLockWord(false);
      LockWord::LockState ls = lw.GetState();
      EXPECT_EQ(LockWord::LockState::kThinLockBiasable, ls);
      EXPECT_EQ(lw.ThinLockBiasableCount(), 1U);
      test->object_.Get()->MonitorExit(self);
    }

    // Lock the object once more would change the lock state into kBiasLocked.
    test->object_.Get()->MonitorEnter(self);
    LockWord lock_after2 = test->object_.Get()->GetLockWord(false);
    LockWord::LockState new_state2 = lock_after2.GetState();
    EXPECT_EQ(LockWord::LockState::kBiasLocked, new_state2);
    EXPECT_EQ(lock_after2.BiasLockCount(), 1U);

    // The lock word state should change into kFatLocked.
    test->object_.Get()->IdentityHashCode();

    LockWord lock_after3 = test->object_.Get()->GetLockWord(false);
    LockWord::LockState new_state3 = lock_after3.GetState();
    EXPECT_EQ(LockWord::LockState::kFatLocked, new_state3);

    // Release the lock
    test->object_.Get()->MonitorExit(self);
  }
}

// First test: The lock state should change from the thin biasable to biased when the profiling
// counter reaches the threshold.
TEST_F(BiasedLockTest, FromThinBiasableToBiasTest) {
  FromThinBiasableToBiasTest(this);
}

// Second test: The lock state should change form the thin biasable to biased then to the fat.
TEST_F(BiasedLockTest, ThinBiasFatTest) {
  ThinBiasFatTest(this);
}

// Third test: revoke a biased lock (lock count > 0) when a contender tries to generate hash code.
TEST_F(BiasedLockTest, RevokeBiasLocked1) {
  CommonWaitSetup(this, true, true, "Biased test thread pool 1");
}

// Fourth test: revoke a biased lock (lock count > 0) when a contender tries to lock the object.
TEST_F(BiasedLockTest, RevokeBiasLocked2) {
  CommonWaitSetup(this, true, false, "Biased test thread pool 2");
}

// Fifth test: revoke a biased lock (lock count == 0) when a contender tries to generate hash code.
TEST_F(BiasedLockTest, RevokeBiasLocked3) {
  CommonWaitSetup(this, false, true, "Biased test thread pool 3");
}

// Sixth test: revoke a biased lock (lock count == 0) when a contender tries to lock at the same time.
TEST_F(BiasedLockTest, RevokeBiasLocked4) {
  CommonWaitSetup(this, false, false, "Biased test thread pool 4");
}

}  // namespace art
