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
#include "lambda/box_table.h"

#include "base/mutex.h"
#include "common_throws.h"
#include "mirror/method.h"
#include "mirror/object-inl.h"
#include "thread.h"

#include <vector>

namespace art {
namespace lambda {

mirror::Object* BoxTable::BoxLambda(const ClosureType& closure) {
  Thread* self = Thread::Current();

  {
    ReaderMutexLock mu(self, *Locks::lambda_table_lock_);

    // Attempt to look up this object, it's possible it was already boxed previously.
    // If this is the case we *must* return the same object as before to maintain
    // referential equality.
    //
    // In managed code:
    //   Functional f = () -> 5;  // vF = create-lambda
    //   Object a = f;            // vA = box-lambda vA
    //   Object b = f;            // vB = box-lambda vB
    //   assert(a == f)
    ValueType value = FindBoxedLambda(closure);
    if (value != nullptr) {
      return value;
    }

    // Otherwise we need to box ourselves and insert it into the hash map
  }

  // Release the lambda table lock here, so that thread suspension is allowed.

  // Convert the ArtMethod into a java.lang.reflect.Method which will serve
  // as the temporary 'boxed' version of the lambda. This is good enough
  // to check all the basic object identities that a boxed lambda must retain.

  // TODO: Boxing an innate lambda (i.e. made with create-lambda) should make a proxy class
  // TODO: Boxing a learned lambda (i.e. made with unbox-lambda) should return the original object
  mirror::Method* method_as_object =
      mirror::Method::CreateFromArtMethod(self, closure);

  if (UNLIKELY(method_as_object == nullptr)) {
    // Most likely an OOM has occurred.
    CHECK(self->IsExceptionPending());
    return nullptr;
  }

  // The method has been successfully boxed into an object, now insert it into the hash map.
  {
    WriterMutexLock mu(self, *Locks::lambda_table_lock_);

    // Lookup the object again, it's possible another thread already boxed it while
    // we were allocating the object before.
    ValueType value = FindBoxedLambda(closure);
    if (UNLIKELY(value != nullptr)) {
      // Let the GC clean up method_as_object at a later time.
      return value;
    }

    // Otherwise we should insert it into the hash map in this thread.
    map_.Insert(std::make_pair(closure, method_as_object));
  }

  return method_as_object;
}

bool BoxTable::UnboxLambda(mirror::Object* object, ClosureType* out_closure) {
  DCHECK(object != nullptr);
  *out_closure = nullptr;

  mirror::Object* boxed_closure_object = object;

  // Raise ClassCastException if object is not instanceof java.lang.reflect.Method
  if (UNLIKELY(!boxed_closure_object->InstanceOf(mirror::Method::StaticClass()))) {
    ThrowClassCastException(mirror::Method::StaticClass(), boxed_closure_object->GetClass());
    return false;
  }

  // TODO(iam): We must check that the closure object extends/implements the type
  // specified in [type id]. This is not currently implemented since it's always a Method.

  // If we got this far, the inputs are valid.
  // Write out the java.lang.reflect.Method's embedded ArtMethod* into the vreg target.
  mirror::AbstractMethod* boxed_closure_as_method =
      down_cast<mirror::AbstractMethod*>(boxed_closure_object);

  ArtMethod* unboxed_closure = boxed_closure_as_method->GetArtMethod();
  DCHECK(unboxed_closure != nullptr);

  *out_closure = unboxed_closure;
  return true;
}

BoxTable::ValueType BoxTable::FindBoxedLambda(const ClosureType& closure) const {
  auto map_iterator = map_.Find(closure);
  if (map_iterator != map_.end()) {
    const std::pair<ClosureType, ValueType>& key_value_pair = *map_iterator;
    const ValueType& value = key_value_pair.second;

    DCHECK(value != nullptr);  // Never store null boxes.
    return value;
  }

  return nullptr;
}

bool BoxTable::RegisterBoxedLambda(const ClosureType& closure) {
  Thread* self = Thread::Current();
  WriterMutexLock mu(self, *Locks::lambda_table_lock_);

  UNUSED(closure);

  return true;
}

void BoxTable::SweepWeakBoxedLambdas(IsMarkedCallback* visitor, void* arg) {
  DCHECK(visitor != nullptr);

  Thread* self = Thread::Current();
  WriterMutexLock mu(self, *Locks::lambda_table_lock_);

  /*
   * Visit every weak root in our lambda box table.
   * Remove unmarked objects, update marked objects to new address.
   */
  std::vector<ClosureType> remove_list;
  for (std::pair<ClosureType, ValueType>& key_value_pair : map_) {
    ValueType old_value = key_value_pair.second;

    ValueType new_value = visitor(old_value, arg);

    if (new_value == nullptr) {
      // The object has been swept away.
      // Delete the entry from the map.
      remove_list.push_back(key_value_pair.first);
    } else {
      // The object has been moved.
      // Update the map.
      key_value_pair.second = new_value;
    }
  }

  // Prune all the boxed objects we had marked as deleted.
  for (const ClosureType& closure : remove_list) {
    map_.Erase(map_.Find(closure));
  }

  // Occasionally shrink the map to avoid growing very large.
  if (map_.CalculateLoadFactor() < kMinimumLoadFactor) {
    map_.ShrinkToMaximumLoad();
  }
}

}  // namespace lambda
}  // namespace art
