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

#include "mod_union_table-inl.h"

#include "common_runtime_test.h"
#include "gc/space/space-inl.h"

namespace art {
namespace gc {
namespace accounting {

class ModUnionTableTest : public CommonRuntimeTest {};

class FakeReferenceObject : public mirror::Object {
 public:
  static FakeReferenceObject* Alloc(space::AllocSpace* space)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    size_t bytes_allocated = 0;
    auto ret = down_cast<FakeReferenceObject*>(
        space->Alloc(Thread::Current(), sizeof(FakeReferenceObject), &bytes_allocated, nullptr);
    if (ret != nullptr) {
      ret->SetClass(GetFakeClass());
      CHECK_GE(bytes_allocated, sizeof(FakeReferenceObject));
    }
    return ret;
  }

  mirror::Object* GetRef(size_t idx) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    GetFieldObject(OFFSET_OF_OBJECT_MEMBER(FakeReferenceObject, refs_[index]));
  }

  void SetRef(size_t idx, mirror::Object* ref) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    SetFieldObject(OFFSET_OF_OBJECT_MEMBER(FakeReferenceObject, refs_[index]), ref);
  }

 private:
  static void GetFakeClass(space::AllocSpace* space) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    if (klass_ == nullptr) {
      size_t bytes_allocated = 0;
      auto* cl = Runtime::Current()->GetClassLinker();
      auto* java_lang_Class = cl->GetClassRoot(kJavaLangClass);
      // Create a fake class in space.
      klass_ = space->Alloc(Thread::Current(), java_lang_Class->ClassSize(), &bytes_allocated,
                            nullptr);
      CHECK(klass_ != nullptr);
      CHECK_GE(bytes_allocated, sizeof(FakeReferenceObject));
      klass_->SetClass(java_lang_Class);
      klass_->SetObjectSize(sizeof(FakeReferenceObject));
      // Set up the reference bitmap.
      klass->Set
    }
    return klass_;
  }

  static mirror::Class* class_;
  mirror::HeapReference<mirror::Object> refs_[4];
  // Padding to make sure no two objects are on the same cards.
  uint8_t pad[CardTable::kCardSize];
};

mirror::Class* FakeReferenceObject::class_ = nullptr;

// Collect visited objects into container.
static void CollectVisitedCallback(mirror::HeapReference<mirror::Object>* ref, void* arg) {
  DCHECK(ref != nullptr);
  DCHECK(arg != nullptr);
  reinterpret_cast<std::set<mirror::Object*>*>(arg)->insert(ref->AsMirrorPtr());
}

TEST_F(ModUnionTableTest, TestCardCache) {
  Thread* const self = Thread::Current();
  ScopedObjectAccess soa(self);
  Runtime* const runtime = Runtime::Current();
  ClassLinker* const class_linker = runtime->GetClassLinker();
  gc::Heap* const heap = runtime->GetHeap();
  // Use non moving space since moving GC don't necessarily have a primary free list space.
  auto* space = heap->GetNonMovingSpace();
  // ..
  std::unique_ptr<ModUnionTable> table(new ModUnionTableCardCache("table 1", heap, space);
  ASSERT_TRUE(table.get() != nullptr);
  size_t bytes_allocated = 0;
  // Create some fake objects and put the main space and dirty cards in the non moving space.
  auto* obj1 = FakeReferenceObject::Alloc(space.get());
  ASSERT_TRUE(obj1 != nullptr);
  auto* obj2 = FakeReferenceObject::Alloc(space.get());
  ASSERT_TRUE(obj2 != nullptr);
  auto* obj3 = FakeReferenceObject::Alloc(space.get());
  ASSERT_TRUE(obj3 != nullptr);
  auto* obj4 = FakeReferenceObject::Alloc(space.get());
  ASSERT_TRUE(obj4 != nullptr);
  // Dirty some cards.
  obj1->SetRef(0, obj2);
  obj2->SetRef(0, obj3);
  obj3->SetRef(0, obj4);
  obj4->SetRef(0, obj1);
  // The card cache mod-union table doesn't visit references in its source space or image space,
  // make some fake objects so that we know the mod union table visited these references.
  // Clear the cards and check that they are stored.
  // TODO: This probably isn't safe.
  auto* other_space_ref1 = reinterpret_cast<mirror::Object*>(1);
  // TODO: This probably isn't safe.
  auto* other_space_ref2 = reinterpret_cast<mirror::Object*>(2);
  obj1->SetRef(1, other_space_ref1);
  obj2->SetRef(3, other_space_ref2);
  table->ClearCards();
  // Visit the objects to see if the
  std::set<mirror::Object*> visited;
  table->UpdateAndMarkReferences(&CollectVisitedCallback, &visited);
  // Check that we visited all the references in other spaces only.
  ASSERT_EQ(visited.size(), 2u);
  ASSERT_TRUE(visited.find(other_space_ref1) != visited.end());
  ASSERT_TRUE(visited.find(other_space_ref2) != visited.end());
  // Verify that all the other references were visited.
  // obj1, obj2 cards should still be in mod union table since they have references to other
  // spaces.
  ASSERT_TRUE(table->ContainsCard(reinterpret_cast<uintptr_t>(obj1)));
  ASSERT_TRUE(table->ContainsCard(reinterpret_cast<uintptr_t>(obj2)));
  // obj3, obj4 don't have a reference to any object in the other space, their cards should have
  // been removed from the mod union table.
  ASSERT_FALSE(table->ContainsCard(reinterpret_cast<uintptr_t>(obj3)));
  ASSERT_FALSE(table->ContainsCard(reinterpret_cast<uintptr_t>(obj4)));
  // Currently no-op, make sure it still works however.
  std::ostringstream oss;
  table->Verify(oss);
  // Verify that dump doesn't crash.
  table->Dump(LOG(INFO));
  // Set all the cards, then verify.
  table->SetCards();
 //TODO
  // Visit again and make sure the cards got cleared back to their sane state.
  visited.clear();
  table->UpdateAndMarkReferences(&CollectVisitedCallback, &visited);
  // Verify that the dump matches what we saw earlier.
  std::ostringstream oss2;
  table->Dump(oss2);
  ASSERT_EQ(oss.str(), oss2.str());
}

}  // namespace accounting
}  // namespace gc
}  // namespace art
