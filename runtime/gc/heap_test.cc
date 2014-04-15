/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "common_runtime_test.h"
#include "gc/accounting/card_table-inl.h"
#include "gc/accounting/space_bitmap-inl.h"
#include "gc/space/space.h"
#include "gc/space/zygote_space.h"
#include "mirror/class-inl.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "sirt_ref.h"

namespace art {
namespace gc {

class HeapTest : public CommonRuntimeTest {};

TEST_F(HeapTest, ClearGrowthLimit) {
  Heap* heap = Runtime::Current()->GetHeap();
  int64_t max_memory_before = heap->GetMaxMemory();
  int64_t total_memory_before = heap->GetTotalMemory();
  heap->ClearGrowthLimit();
  int64_t max_memory_after = heap->GetMaxMemory();
  int64_t total_memory_after = heap->GetTotalMemory();
  EXPECT_GE(max_memory_after, max_memory_before);
  EXPECT_GE(total_memory_after, total_memory_before);
}

TEST_F(HeapTest, GarbageCollectClassLinkerInit) {
  {
    ScopedObjectAccess soa(Thread::Current());
    // garbage is created during ClassLinker::Init

    SirtRef<mirror::Class> c(soa.Self(), class_linker_->FindSystemClass(soa.Self(),
                                                                        "[Ljava/lang/Object;"));
    for (size_t i = 0; i < 1024; ++i) {
      SirtRef<mirror::ObjectArray<mirror::Object> > array(soa.Self(),
          mirror::ObjectArray<mirror::Object>::Alloc(soa.Self(), c.get(), 2048));
      for (size_t j = 0; j < 2048; ++j) {
        mirror::String* string = mirror::String::AllocFromModifiedUtf8(soa.Self(), "hello, world!");
        // SIRT operator -> deferences the SIRT before running the method.
        array->Set<false>(j, string);
      }
    }
  }
  Runtime::Current()->GetHeap()->CollectGarbage(false);
}

TEST_F(HeapTest, HeapBitmapCapacityTest) {
  byte* heap_begin = reinterpret_cast<byte*>(0x1000);
  const size_t heap_capacity = kObjectAlignment * (sizeof(intptr_t) * 8 + 1);
  UniquePtr<accounting::ContinuousSpaceBitmap> bitmap(
      accounting::ContinuousSpaceBitmap::Create("test bitmap", heap_begin, heap_capacity));
  mirror::Object* fake_end_of_heap_object =
      reinterpret_cast<mirror::Object*>(&heap_begin[heap_capacity - kObjectAlignment]);
  bitmap->Set(fake_end_of_heap_object);
}

class RandGen {
 public:
  explicit RandGen(uint32_t seed) : val_(seed) {}

  uint32_t next() {
    val_ = val_ * 48271 % 2147483647 + 13;
    return val_;
  }

  uint32_t val_;
};

class CompactionTest : public HeapTest {
 protected:
  void SetUpRuntimeOptions(Runtime::Options *options) OVERRIDE {
    // Pretend to be zygote, so we can run compaction.
    options->push_back(std::make_pair("-Xzygote", nullptr));
  }

  void Test() NO_THREAD_SAFETY_ANALYSIS;

  struct Context {
    mirror::Object* prev_;      // The end of the previous object.
    mirror::Object* expected_;  // Where we expect the next one
  };

  static void Callback(mirror::Object* obj, void* arg)
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    Context* context = reinterpret_cast<Context*>(arg);

    if (context->prev_ != nullptr) {
      // Check whether we're "close" to expected_.
      if (obj > context->expected_) {
        EXPECT_LE(obj - context->expected_, 10);
      } else if (obj < context->expected_) {
        // TODO: If we have precise ClassClass sizes, we could check for real compaction.
        if (reinterpret_cast<uintptr_t>(context->expected_) != ~static_cast<uintptr_t>(0U)) {
          LOG(WARNING) << "Found a bad expectation.";
        }
      }
    }

    context->prev_ = obj;

    // Get size.
    // mirror::Class* klass = obj->GetClass();
    uint32_t size = obj->SizeOf();
    /*
    if (klass->IsVariableSize()) {
      size = obj->SizeOf();
      if (klass->IsClassClass()) {
        // TODO: Better estimate.
        // LOG(WARNING) << "No good estimate for " << PrettyClass(klass);
        context->expected_ = reinterpret_cast<mirror::Object*>(~0);
      } else if (klass->IsArrayClass()) {
        int32_t header_size = mirror::Array::DataOffset(klass->GetComponentSize()).Int32Value();
        mirror::Array* array = obj->AsArray();
        int32_t l = array->GetLength();
        size = static_cast<uint32_t>(header_size + (l * klass->GetComponentSize()));
      } else {
        EXPECT_TRUE(false) << "Unhandled class " << PrettyClass(klass);
      }
    } else {
      size = klass->GetObjectSize();
    }
    */
    if (size != 0) {
      uintptr_t next = RoundUp(reinterpret_cast<uintptr_t>(obj) + size, kObjectAlignment);
      context->expected_ = reinterpret_cast<mirror::Object*>(next);
    }
  }
};

void CompactionTest::Test() {
  // Seed with 0x1234 for reproducability.
  RandGen r(0x1234);

  Thread* t;

  for (int loop = 0; loop < 10; ++loop) {
    LOG(INFO) << "Iteration " << (loop+1);
    std::vector<SirtRef<mirror::ObjectArray<mirror::Object> >*> arrays;

    SirtRef<mirror::Class>* c;

    t = Thread::Current();
    {
      ScopedObjectAccess soa(t);
      // garbage is created during ClassLinker::Init

      c = new SirtRef<mirror::Class>(soa.Self(), class_linker_->FindSystemClass(soa.Self(),
                                                                                "[Ljava/lang/Object;"));

      for (size_t i = 0; i < 128; ++i) {
        int32_t length = r.next() % 2048;  // Variable-length array.
        SirtRef<mirror::ObjectArray<mirror::Object> >* array =
            new SirtRef<mirror::ObjectArray<mirror::Object> >(soa.Self(),
                                                              mirror::ObjectArray<mirror::Object>::Alloc(
                                                                  soa.Self(), c->get(), length));
        for (size_t j = 0; j < static_cast<size_t>(length); ++j) {
          mirror::String* string = mirror::String::AllocFromModifiedUtf8(soa.Self(), "hello, world!");
          // Decide whether we want to keep it.
          if (r.next() % 2 == 0) {
            array->get()->Set<false>(j, string);
          }
        }
        // Decide whether we want the array
        if (r.next() % 2 == 0) {
          arrays.push_back(array);
        } else {
          delete array;
        }
      }
    }

    // Now call zygote prefork to compact.
    Heap* heap = Runtime::Current()->GetHeap();
    heap->PreZygoteFork();

    {
      ScopedObjectAccess soa(t);

      const std::vector<space::ContinuousSpace*> spaces = heap->GetContinuousSpaces();

      // Now check all arrays for "sane" values.
      for (auto array : arrays) {
        // First check: array should still be alive
        EXPECT_NE(nullptr, array->get());

        // Second check: run through it, make sure things seem sane
        // Get the array size.
        int32_t length = array->get()->GetLength();
        EXPECT_GE(length, 0);

        for (size_t j = 0; j < static_cast<size_t>(length); ++j) {
          mirror::Object* obj = array->get()->Get(j);
          if (obj != nullptr) {
            byte* obj_ptr = reinterpret_cast<byte*>(obj);

            // Ensure the pointer points into the space.
            bool found = false;
            for (auto space : spaces) {
              byte* space_begin = space->Begin();
              byte* space_end = space->End();

              if (space_begin <= obj_ptr && obj_ptr <= space_end) {
                found = true;
                break;
              }
            }
            EXPECT_EQ(true, found);
          }
        }
      }

      // Now check that we're halfway compact.
      // Get the zygote space
      space::ZygoteSpace* zygote = nullptr;
      for (auto space : spaces) {
        if (space->GetType() == space::kSpaceTypeZygoteSpace) {
          zygote = space->AsZygoteSpace();
        }
      }
      EXPECT_NE(nullptr, zygote);

      accounting::ContinuousSpaceBitmap* live_map = zygote->GetLiveBitmap();
      Context context;
      context.prev_ = nullptr;
      live_map->Walk(Callback, &context);

      // Last, do a general VerifyHeap
      heap->VerifyHeap();

      // Cleanly delete Sirt references in reverse order, or we will get errors
      for (auto i = arrays.rbegin(); i != arrays.rend(); ++i) {
        delete *i;
      }
      delete c;
    }

    // Shut down the runtime and start it again for the next run.
    // This is inefficient for the last iteration, but the easiest.
    TearDown();

    delete Runtime::Current();

    SetUp();
  }
}

TEST_F(CompactionTest, Compaction) {
  Test();
}

}  // namespace gc
}  // namespace art
