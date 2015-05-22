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

#include "remembered_set.h"
#include "gc/space/space.h"

#include "common_runtime_test.h"
#include "gc/space/space-inl.h"
#include "mirror/array-inl.h"
#include "space_bitmap-inl.h"
#include "thread-inl.h"

namespace art {
namespace gc {
namespace accounting {

class RememberedSetFactory {
 public:
  static RememberedSet* Create(
      space::ContinuousSpace* space, space::ContinuousSpace* target_space);
};

class RememberedSetTest : public CommonRuntimeTest {
 public:
  RememberedSetTest() : java_lang_object_array_(nullptr) {
  }
  mirror::ObjectArray<mirror::Object>* AllocObjectArray(
      Thread* self, space::ContinuousMemMapAllocSpace* space, size_t component_count)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    auto* klass = GetObjectArrayClass(self, space);
    const size_t size = ComputeArraySize(self, klass, component_count, 2);
    size_t bytes_allocated = 0;
    auto* obj = down_cast<mirror::ObjectArray<mirror::Object>*>(
        space->Alloc(self, size, &bytes_allocated, nullptr));
    if (obj != nullptr) {
      obj->SetClass(klass);
      obj->SetLength(static_cast<int32_t>(component_count));
      space->GetLiveBitmap()->Set(obj);
      EXPECT_GE(bytes_allocated, size);
    }
    return obj;
  }

  void ResetClass() {
    java_lang_object_array_ = nullptr;
  }
  void RunTest();

 private:
  mirror::Class* GetObjectArrayClass(Thread* self, space::ContinuousMemMapAllocSpace* space)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    if (java_lang_object_array_ == nullptr) {
      java_lang_object_array_ =
          Runtime::Current()->GetClassLinker()->GetClassRoot(ClassLinker::kObjectArrayClass);
      // Since the test doesn't have an image, the class of the object array keeps cards live
      // inside the card bitmap remembered set and causes the check
      // ASSERT_FALSE(rem_set->ContainsCardFor(reinterpret_cast<uintptr_t>(obj3)));
      // to fail since the class ends up keeping the card dirty. To get around this, we make a fake
      // copy of the class in the same space that we are allocating in.
      DCHECK(java_lang_object_array_ != nullptr);
      const size_t class_size = java_lang_object_array_->GetClassSize();
      size_t bytes_allocated = 0;
      auto* klass = down_cast<mirror::Class*>(space->Alloc(self, class_size, &bytes_allocated,
                                                           nullptr));
      DCHECK(klass != nullptr);
      memcpy(klass, java_lang_object_array_, class_size);
      Runtime::Current()->GetHeap()->GetCardTable()->MarkCard(klass);
      java_lang_object_array_ = klass;
    }
    return java_lang_object_array_;
  }
  mirror::Class* java_lang_object_array_;
};

// Collect visited objects into container.
static void CollectVisitedCallback(mirror::HeapReference<mirror::Object>* ref, void* arg)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  DCHECK(ref != nullptr);
  DCHECK(arg != nullptr);
  reinterpret_cast<std::set<mirror::Object*>*>(arg)->insert(ref->AsMirrorPtr());
}

RememberedSet* RememberedSetFactory::Create(
    space::ContinuousSpace* space, space::ContinuousSpace* target_space) {
  std::ostringstream name;
  return new RememberedSet(name.str(), Runtime::Current()->GetHeap(), space);
}

TEST_F(RememberedSetTest, TestCardBitmap) {
  RunTest();
}

void RememberedSetTest::RunTest() {
  Thread* const self = Thread::Current();
  ScopedObjectAccess soa(self);
  Runtime* const runtime = Runtime::Current();
  gc::Heap* const heap = runtime->GetHeap();
  // Use non moving space since moving GC don't necessarily have a primary free list space.
  auto* space = heap->GetNonMovingSpace();
  ResetClass();
  // Create another space that we can put references in.
  std::unique_ptr<space::BumpPointerSpace> other_space(space::BumpPointerSpace::Create(
      "other space", 4 * MB, nullptr));
  ASSERT_TRUE(other_space.get() != nullptr);
  heap->AddSpace(other_space.get());
  std::unique_ptr<RememberedSet> rem_set(RememberedSetFactory::Create(
      space, other_space.get()));
  ASSERT_TRUE(rem_set.get() != nullptr);
  // Create some fake objects and put the main space and dirty cards in the non moving space.
  auto* obj1 = AllocObjectArray(self, space, CardTable::kCardSize);
  ASSERT_TRUE(obj1 != nullptr);
  auto* obj2 = AllocObjectArray(self, space, CardTable::kCardSize);
  ASSERT_TRUE(obj2 != nullptr);
  auto* obj3 = AllocObjectArray(self, space, CardTable::kCardSize);
  ASSERT_TRUE(obj3 != nullptr);
  auto* obj4 = AllocObjectArray(self, space, CardTable::kCardSize);
  ASSERT_TRUE(obj4 != nullptr);
  // Dirty some cards.
  obj1->Set(0, obj2);
  obj2->Set(0, obj3);
  obj3->Set(0, obj4);
  obj4->Set(0, obj1);
  // Dirty some more cards to objects in another space.
  auto* other_space_ref1 = AllocObjectArray(self, other_space.get(), CardTable::kCardSize);
  ASSERT_TRUE(other_space_ref1 != nullptr);
  auto* other_space_ref2 = AllocObjectArray(self, other_space.get(), CardTable::kCardSize);
  ASSERT_TRUE(other_space_ref2 != nullptr);
  obj1->Set(1, other_space_ref1);
  obj2->Set(3, other_space_ref2);
  rem_set->ClearCards();
  std::set<mirror::Object*> visited;
  rem_set->UpdateAndMarkReferences(&CollectVisitedCallback, nullptr, nullptr, &visited);
  // Check that we visited all the references in other spaces only.
  ASSERT_GE(visited.size(), 2u);
  ASSERT_TRUE(visited.find(other_space_ref1) != visited.end());
  ASSERT_TRUE(visited.find(other_space_ref2) != visited.end());
  // Verify that all the other references were visited.
  // obj1, obj2 cards should still be in remembered set since they have references to other
  // spaces.
  ASSERT_TRUE(rem_set->ContainsCardFor(reinterpret_cast<uintptr_t>(obj1)));
  ASSERT_TRUE(rem_set->ContainsCardFor(reinterpret_cast<uintptr_t>(obj2)));
  // obj3, obj4 don't have a reference to any object in the other space, their cards should have
  // been removed from the rememebered set during UpdateAndMarkReferences.
  ASSERT_FALSE(rem_set->ContainsCardFor(reinterpret_cast<uintptr_t>(obj3)));
  ASSERT_FALSE(rem_set->ContainsCardFor(reinterpret_cast<uintptr_t>(obj4)));
  // Verify that dump doesn't crash.
  std::ostringstream oss;
  rem_set->Dump(oss);
  // Set all the cards, then verify.
  rem_set->SetCards();
  // TODO: Check that the cards are actually set.
  for (auto* ptr = space->Begin(); ptr < AlignUp(space->End(), CardTable::kCardSize);
      ptr += CardTable::kCardSize) {
    ASSERT_TRUE(rem_set->ContainsCardFor(reinterpret_cast<uintptr_t>(ptr)));
  }
  // Visit again and make sure the cards got cleared back to their sane state.
  visited.clear();
  rem_set->UpdateAndMarkReferences(&CollectVisitedCallback, nullptr, nullptr, &visited);
  // Verify that the dump matches what we saw earlier.
  std::ostringstream oss2;
  rem_set->Dump(oss2);
  ASSERT_EQ(oss.str(), oss2.str());
  // Remove the space we added so it doesn't persist to the next test.
  heap->RemoveSpace(other_space.get());
}

}  // namespace accounting
}  // namespace gc
}  // namespace art

