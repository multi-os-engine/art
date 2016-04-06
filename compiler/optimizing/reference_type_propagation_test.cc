/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "base/arena_allocator.h"
#include "builder.h"
#include "nodes.h"
#include "object_lock.h"
#include "optimizing_unit_test.h"
#include "reference_type_propagation.h"

namespace art {

/**
 * Fixture class for unit testing the ReferenceTypePropagation phase. Used to verify the
 * functionality of methods and situations that are hard to set up with checker tests.
 */
class ReferenceTypePropagationTest : public CommonCompilerTest {
 public:
  ReferenceTypePropagationTest() : pool_(), allocator_(&pool_) {
    graph_ = CreateGraph(&allocator_);
  }

  ~ReferenceTypePropagationTest() { }

  void SetupPropagation(StackHandleScopeCollection* handles) {
    propagation_ = new (&allocator_) ReferenceTypePropagation(graph_, handles, true, "test_prop");
  }

  // Relay method to merge type in reference type propagation.
  ReferenceTypeInfo MergeTypes(const ReferenceTypeInfo& a,
                               const ReferenceTypeInfo& b) SHARED_REQUIRES(Locks::mutator_lock_) {
    return propagation_->MergeTypes(a, b);
  }

  // Helper method to construct an invalid type.
  ReferenceTypeInfo InvalidType() {
    return ReferenceTypeInfo::CreateInvalid();
  }

  // Helper method to construct the Object type.
  ReferenceTypeInfo ObjectType(bool is_exact = true) {
    return ReferenceTypeInfo::Create(propagation_->handle_cache_.GetObjectClassHandle(), is_exact);
  }

  // Helper method to construct the String type.
  ReferenceTypeInfo StringType(bool is_exact = true) {
    return ReferenceTypeInfo::Create(propagation_->handle_cache_.GetStringClassHandle(), is_exact);
  }

  // Helper method to construct the Throwable type.
  ReferenceTypeInfo ThrowableType(bool is_exact=true) {
    return ReferenceTypeInfo::Create(propagation_->handle_cache_.GetThrowableClassHandle(), is_exact);
  }

  Handle<mirror::Class> doit() {
    return propagation_->handle_cache_.GetThrowableClassHandle();
  }

  // General building fields.
  ArenaPool pool_;
  ArenaAllocator allocator_;
  HGraph* graph_;

  ReferenceTypePropagation* propagation_;
};

//
// The actual ReferenceTypePropgation unit tests.
//

TEST_F(ReferenceTypePropagationTest, ProperSetup) {
  ScopedObjectAccess soa(Thread::Current());
  StackHandleScopeCollection handles(soa.Self());
  SetupPropagation(&handles);

  EXPECT_TRUE(propagation_ != nullptr);
}

TEST_F(ReferenceTypePropagationTest, MergeInvalidTypes) {
  ScopedObjectAccess soa(Thread::Current());
  StackHandleScopeCollection handles(soa.Self());
  SetupPropagation(&handles);

  // Two invalid types.
  ReferenceTypeInfo t1(MergeTypes(InvalidType(), InvalidType()));
  EXPECT_FALSE(t1.IsValid());
  EXPECT_FALSE(t1.IsExact());
  EXPECT_TRUE(t1.IsEqual(InvalidType()));

  // Valid type on right.
  ReferenceTypeInfo t2(MergeTypes(InvalidType(), ObjectType()));
  EXPECT_TRUE(t2.IsValid());
  EXPECT_TRUE(t2.IsExact());
  EXPECT_TRUE(t2.IsEqual(ObjectType()));
  ReferenceTypeInfo t3(MergeTypes(InvalidType(), StringType()));
  EXPECT_TRUE(t3.IsValid());
  EXPECT_TRUE(t3.IsExact());
  EXPECT_TRUE(t3.IsEqual(StringType()));

  // Valid type on left.
  ReferenceTypeInfo t4(MergeTypes(ObjectType(), InvalidType()));
  EXPECT_TRUE(t4.IsValid());
  EXPECT_TRUE(t4.IsExact());
  EXPECT_TRUE(t4.IsEqual(ObjectType()));
  ReferenceTypeInfo t5(MergeTypes(StringType(), InvalidType()));
  EXPECT_TRUE(t5.IsValid());
  EXPECT_TRUE(t5.IsExact());
  EXPECT_TRUE(t5.IsEqual(StringType()));
}

TEST_F(ReferenceTypePropagationTest, MergeThrowableTypes) {
  ScopedObjectAccess soa(Thread::Current());
  StackHandleScopeCollection handles(soa.Self());
  SetupPropagation(&handles);

  // Something trickery to make the runtime thing something
  // went wrong with loading the throwable class, making this
  // an erroneous type from here on.
  soa.Self()->SetException(Thread::GetDeoptimizationException());
  Handle<mirror::Class> klass = doit();
  ObjectLock<mirror::Class> lock(soa.Self(), klass);
  mirror::Class::SetStatus(klass, mirror::Class::kStatusError, soa.Self());
  soa.Self()->ClearException();

  // Two erroneous types.
  ReferenceTypeInfo t1(MergeTypes(ThrowableType(), ThrowableType()));
  EXPECT_TRUE(t1.IsValid());
  EXPECT_FALSE(t1.IsExact());
  EXPECT_TRUE(t1.IsEqual(ObjectType(false)));

  // Valid type on right.
  ReferenceTypeInfo t2(MergeTypes(ThrowableType(), StringType()));
  EXPECT_TRUE(t2.IsValid());
  EXPECT_FALSE(t2.IsExact());
  EXPECT_TRUE(t2.IsEqual(ObjectType(false)));

  // Valid type on left.
  ReferenceTypeInfo t3(MergeTypes(StringType(), ThrowableType()));
  EXPECT_TRUE(t3.IsValid());
  EXPECT_FALSE(t3.IsExact());
  EXPECT_TRUE(t3.IsEqual(ObjectType(false)));

  // Invalid and erroneous types yields the erroneous type.
  //
  // TODO: question for reviewers
  //
  // This does what the invalid code bail current requires.
  // But is this right?
  //
  ReferenceTypeInfo t4(MergeTypes(InvalidType(), ThrowableType()));
  EXPECT_TRUE(t4.IsValid());
  EXPECT_TRUE(t4.IsExact());
  EXPECT_TRUE(t4.IsEqual(ThrowableType()));
  ReferenceTypeInfo t5(MergeTypes(ThrowableType(), InvalidType()));
  EXPECT_TRUE(t5.IsValid());
  EXPECT_TRUE(t5.IsExact());
  EXPECT_TRUE(t5.IsEqual(ThrowableType()));
}

TEST_F(ReferenceTypePropagationTest, MergeValidTypes) {
  ScopedObjectAccess soa(Thread::Current());
  StackHandleScopeCollection handles(soa.Self());
  SetupPropagation(&handles);

  // Same types.
  ReferenceTypeInfo t1(MergeTypes(ObjectType(), ObjectType()));
  EXPECT_TRUE(t1.IsValid());
  EXPECT_TRUE(t1.IsExact());
  EXPECT_TRUE(t1.IsEqual(ObjectType()));
  ReferenceTypeInfo t2(MergeTypes(StringType(), StringType()));
  EXPECT_TRUE(t2.IsValid());
  EXPECT_TRUE(t2.IsExact());
  EXPECT_TRUE(t2.IsEqual(StringType()));

  // Left is super class of right.
  ReferenceTypeInfo t3(MergeTypes(ObjectType(), StringType()));
  EXPECT_TRUE(t3.IsValid());
  EXPECT_FALSE(t3.IsExact());
  EXPECT_TRUE(t3.IsEqual(ObjectType(false)));

  // Right is super class of left.
  ReferenceTypeInfo t4(MergeTypes(StringType(), ObjectType()));
  EXPECT_TRUE(t4.IsValid());
  EXPECT_FALSE(t4.IsExact());
  EXPECT_TRUE(t4.IsEqual(ObjectType(false)));

#if 0
  // TODO: question for reviewers

  // These unit tests (merging inexact, but the same types) fails
  // a CHECK, but it seems a possible input to this method. Is there
  // an implicit assumption this situation will never happen, and if
  // so, should we guard against that?

  // Same types, but one or both are inexact.
  ReferenceTypeInfo t5(MergeTypes(StringType(false), StringType()));
  EXPECT_TRUE(t5.IsValid());
  EXPECT_FALSE(t5.IsExact());
  EXPECT_TRUE(t5.IsEqual(StringType(false)));
  ReferenceTypeInfo t6(MergeTypes(StringType(), StringType(false)));
  EXPECT_TRUE(t6.IsValid());
  EXPECT_FALSE(t6.IsExact());
  EXPECT_TRUE(t6.IsEqual(StringType(false)));
  ReferenceTypeInfo t7(MergeTypes(StringType(false), StringType(false)));
  EXPECT_TRUE(t7.IsValid());
  EXPECT_FALSE(t7.IsExact());
  EXPECT_TRUE(t7.IsEqual(StringType(false)));
#endif
}

}  // namespace art

