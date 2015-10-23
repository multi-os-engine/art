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
#include "lambda/box_class_table.h"

#include "base/mutex.h"
#include "common_throws.h"
#include "gc_root-inl.h"
#include "lambda/closure.h"
#include "lambda/leaking_allocator.h"
#include "mirror/method.h"
#include "mirror/object-inl.h"
#include "thread.h"

#include <string>
#include <vector>

// XX: Maybe we don't need this class, the class loader itself can look up by name?
// But if we ever want to store the captured variables *inline* within the proxy,
// then we would want to have a new proxy for each distinct ArtLambdaMethod,
// and then we do want to store this separately?

namespace art {
namespace lambda {

static void DeleteClass(mirror::Class* klass) SHARED_REQUIRES(Locks::mutator_lock_) {
  // TODO: How to delete the class?
  UNUSED(klass);
}

// Create the lambda proxy class given the name of the lambda interface (e.g. Ljava/lang/Runnable;)
// Also needs a proper class loader (or null for bootclasspath) where the proxy will be created into.
// The class must **not** have already been created.
// Returns a non-null ptr on success, otherwise returns null and has an exception set.
static mirror::Class* CreateClass(Thread* self,
                                  const std::string& class_name,
                                  const Handle<mirror::ClassLoader>& class_loader)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  ScopedObjectAccessUnchecked soa(self);
  StackHandleScope<3> hs(self);

  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();

  // Find the java.lang.Class for our class name (from the class loader).
  // const size_t hash = ComputeModifiedUtf8Hash(class_name.c_str());
  Handle<mirror::Class> lambda_interface =
      hs.NewHandle(class_linker->FindClass(self, class_name.c_str(), class_loader));
  // TODO: use LookupClass in a loop
  // TODO: DCHECK That this doesn't actually cause the class to be loaded,
  //       since the create-lambda should've loaded it already
  /*
      hs.NewHandle(class_linker->LookupClass(self,
                                             class_name.c_str(),
                                             hash,
                                             class_loader.Get()));
                                             */
  DCHECK(lambda_interface.Get() != nullptr) << "CreateClass with class_name=" << class_name;
  DCHECK(lambda_interface->IsInterface()) << "CreateClass with class_name=" << class_name;

  DCHECK_EQ(class_name.substr(0, 1), "L");
  // "Lfoo;" -> "Lfoo$LambdaProxy"   // replace ';' with "$LambdaProxy;"
  // XX: Why doesn't it like the ";" at the end?!
  std::string proxy_class_name = class_name.substr(0, class_name.size() - 1) + "$LambdaProxy;";
  // FIXME: add comment to class linker saying that the name must be dotted
  proxy_class_name = DescriptorToDot(proxy_class_name.c_str());
  // "Lfoo/bar;" -> "foo.bar"
  LOG(INFO) << "CreateClass (Lambda Box Table) for " << proxy_class_name;

  jclass java_lang_class = soa.AddLocalReference<jclass>(mirror::Class::GetJavaLangClass());

  // Builds the interfaces array.
  // -- Class[] proxy_class_interfaces = new Class[] { lambda_interface };
  jobjectArray proxy_class_interfaces = soa.Env()->NewObjectArray(1,  // 1 element
                                                                  java_lang_class,
                                                                  lambda_interface.ToJObject());

  if (UNLIKELY(proxy_class_interfaces == nullptr)) {
    self->AssertPendingOOMException();
    return nullptr;
  }

  Handle<mirror::Class> java_lang_object =
      hs.NewHandle(class_linker->FindSystemClass(soa.Self(), "Ljava/lang/Object;"));
  jobjectArray proxy_class_methods;
  jobjectArray proxy_class_throws;
  {
    // Builds the method array.
    jsize methods_count = 3;  // Object.equals, Object.hashCode and Object.toString.
    methods_count += lambda_interface->NumVirtualMethods();

    proxy_class_methods =
        soa.Env()->NewObjectArray(methods_count,
                                  soa.AddLocalReference<jclass>(mirror::Method::StaticClass()),
                                  nullptr);  // No initial element.

    if (UNLIKELY(proxy_class_methods == nullptr)) {
      self->AssertPendingOOMException();
      return nullptr;
    }

    jsize array_index = 0;

    //
    // Fill the method array with the Object and all the interface's virtual methods.
    //

    // Add a method to 'proxy_class_methods'
    auto add_method_to_array = [&](ArtMethod* method) SHARED_REQUIRES(Locks::mutator_lock_) {
      CHECK(method != nullptr);
      soa.Env()->SetObjectArrayElement(proxy_class_methods,
                                       array_index++,
                                       soa.AddLocalReference<jobject>(
                                           mirror::Method::CreateFromArtMethod(soa.Self(),
                                                                               method))
                                      );  // NOLINT: [whitespace/parens] [2], [whitespace/braces] [5]
    };
    // Add a method to 'proxy_class_methods' by looking it up from java.lang.Object
    auto add_method_to_array_by_lookup = [&](const char* name, const char* method_descriptor)
        SHARED_REQUIRES(Locks::mutator_lock_) {
      ArtMethod* method = java_lang_object->FindDeclaredVirtualMethod(name,
                                                                      method_descriptor,
                                                                      sizeof(void*));
      add_method_to_array(method);
    };  // NOLINT: [readability/braces] [4]

    // Add all methods from Object.
    add_method_to_array_by_lookup("equals",   "(Ljava/lang/Object;)Z");
    add_method_to_array_by_lookup("hashCode", "()I");
    add_method_to_array_by_lookup("toString", "()Ljava/lang/String;");

    // Now adds all interfaces virtual methods.
    // FIXME: this actually needs to properly recursively traverse all super-interfaces.
    // XX: Maybe we can just re-use some of the proxy JNI creation mechanism for this?
    {
      MutableHandle<mirror::Class> next_class = hs.NewHandle(lambda_interface.Get());
      do {
        for (ArtMethod& method : next_class->GetVirtualMethods(sizeof(void*))) {
          add_method_to_array(&method);
        }
        next_class.Assign(next_class->GetSuperClass());
      } while (!next_class->IsObjectClass());
      // Skip adding any methods from "Object".
    }
    CHECK_EQ(array_index, methods_count);

    // FIXME: needs to actually populate throws.

    // Builds an empty exception array.
    proxy_class_throws = soa.Env()->NewObjectArray(0 /* length */,
                                                   java_lang_class,
                                                   nullptr /* initial element*/);  // NOLINT: [whitespace/braces] [5]
    if (UNLIKELY(proxy_class_throws == nullptr)) {
      self->AssertPendingOOMException();
      return nullptr;
    }
  }

  mirror::Class* lambda_proxy_class =
      class_linker->CreateLambdaProxyClass(soa,
                                           soa.Env()->NewStringUTF(proxy_class_name.c_str()),
                                           proxy_class_interfaces,
                                           class_loader.ToJObject(),
                                           proxy_class_methods,
                                           proxy_class_throws);

  // No suspension points between here and the return, just return the raw pointer.

  return lambda_proxy_class;
}

BoxClassTable::BoxClassTable()
  : allow_new_weaks_(true),
    new_weaks_condition_("lambda box class table allowed weaks", *Locks::lambda_class_table_lock_) {}

BoxClassTable::~BoxClassTable() {
  // Free all the copies of our closures.
  for (auto map_iterator = map_.begin(); map_iterator != map_.end(); ++map_iterator) {
    std::pair<UnorderedMapKeyType, ValueType>& key_value_pair = *map_iterator;

    mirror::Class* klass = key_value_pair.second.Read();

    // Remove from the map first, so that it doesn't try to access dangling pointer.
    // FIXME: Why does this crash?
    // map_iterator = map_.Erase(map_iterator);

    // Safe to delete, no dangling pointers.
    // XX: Do we want to "unload" the class here instead?
    DeleteClass(klass);
  }
}

mirror::Class* BoxClassTable::GetOrCreateBoxClass(const char* class_name,
                                                  const Handle<mirror::ClassLoader>& class_loader) {
  DCHECK(class_name != nullptr);

  Thread* self = Thread::Current();

  std::string class_name_str = class_name;

  {
    MutexLock mu(self, *Locks::lambda_class_table_lock_);
    BlockUntilWeaksAllowed();

    // Attempt to look up this class, it's possible it was already created previously.
    // If this is the case we *must* return the same object as before to maintain
    // referential equality.
    //
    // In managed code:
    //   Functional f = () -> 5;  // vF = create-lambda
    //   Object a = f;            // vA = box-lambda vA
    //   Object b = f;            // vB = box-lambda vB
    //   assert(a == f)
    ValueType value = FindBoxedClass(class_name_str);
    if (!value.IsNull()) {
      return value.Read();
    }
  }

  // Otherwise we need to box ourselves and insert it into the hash map

  // Release the table lock here, which implicitly allows other threads to suspend
  // (since the GC callbacks will not block on trying to acquire our lock).
  // We also don't want to call into the class linker with the lock held because
  // our lock level is lower.
  self->AllowThreadSuspension();

  // Create a lambda proxy class, within the specified class loader.
  mirror::Class* lambda_proxy_class = CreateClass(self, class_name_str, class_loader);

  // There are no thread suspension points after this, so we don't need to put it into a handle.
  ScopedAssertNoThreadSuspension soants{self, "BoxClassTable::GetOrCreateBoxClass"};  // NOLINT:  [readability/braces] [4]

  if (UNLIKELY(lambda_proxy_class == nullptr)) {
    // Most likely an OOM has occurred.
    CHECK(self->IsExceptionPending());
    return nullptr;
  }

  {
    MutexLock mu(self, *Locks::lambda_class_table_lock_);
    BlockUntilWeaksAllowed();

    // Possible, but unlikely, that someone already came in and made a proxy class
    // on another thread.
    ValueType value = FindBoxedClass(class_name_str);
    if (UNLIKELY(!value.IsNull())) {
      DCHECK_EQ(lambda_proxy_class, value.Read());
      return value.Read();
    }

    // Otherwise we made a brand new proxy class.
    // The class itself is cleaned up by the GC (e.g. class unloading) later.

    // Actually insert into the table.
    map_.Insert({std::move(class_name_str), ValueType(lambda_proxy_class)});
  }

  return lambda_proxy_class;
}

BoxClassTable::ValueType BoxClassTable::FindBoxedClass(const std::string& class_name) const {
  auto map_iterator = map_.Find(class_name);
  if (map_iterator != map_.end()) {
    const std::pair<UnorderedMapKeyType, ValueType>& key_value_pair = *map_iterator;
    const ValueType& value = key_value_pair.second;

    DCHECK(!value.IsNull());  // Never store null boxes.
    return value;
  }

  return ValueType(nullptr);
}

void BoxClassTable::BlockUntilWeaksAllowed() {
  Thread* self = Thread::Current();
  while (UNLIKELY((!kUseReadBarrier && !allow_new_weaks_) ||
                  (kUseReadBarrier && !self->GetWeakRefAccessEnabled()))) {
    new_weaks_condition_.WaitHoldingLocks(self);  // wait while holding mutator lock
  }
}

void BoxClassTable::SweepWeakBoxedLambdas(IsMarkedVisitor* visitor) {
  DCHECK(visitor != nullptr);

  Thread* self = Thread::Current();
  MutexLock mu(self, *Locks::lambda_class_table_lock_);

  /*
   * Visit every weak root in our lambda box class table.
   * Remove unmarked classes, update marked classes to new address.
   */
  std::vector<UnorderedMapKeyType> remove_list;
  for (auto map_iterator = map_.begin(); map_iterator != map_.end(); ) {
    std::pair<UnorderedMapKeyType, ValueType>& key_value_pair = *map_iterator;

    const ValueType& old_value = key_value_pair.second;

    // This does not need a read barrier because this is called by GC.
    mirror::Class* old_value_raw = old_value.Read<kWithoutReadBarrier>();
    mirror::Class* new_value = down_cast<mirror::Class*>(visitor->IsMarked(old_value_raw));

    if (new_value == nullptr) {
      // The class has been swept away (unloaded?).

      // Delete the entry from the map.
      map_iterator = map_.Erase(map_iterator);

      // Clean up the memory by deleting the class.
      // XX: This could happen if class was unloaded? But then it's memory is gone too?
      DeleteClass(old_value_raw);
    } else {
      // The object has been moved.
      // Update the map.
      key_value_pair.second = ValueType(new_value);
      ++map_iterator;
      // XX: Can classes even move, or are they always allocated from a non-movable space?
    }
  }

  // Occasionally shrink the map to avoid growing very large.
  if (map_.CalculateLoadFactor() < kMinimumLoadFactor) {
    map_.ShrinkToMaximumLoad();
  }
}

void BoxClassTable::DisallowNewWeakBoxedLambdas() {
  CHECK(!kUseReadBarrier);
  Thread* self = Thread::Current();
  MutexLock mu(self, *Locks::lambda_class_table_lock_);

  allow_new_weaks_ = false;
}

void BoxClassTable::AllowNewWeakBoxedLambdas() {
  CHECK(!kUseReadBarrier);
  Thread* self = Thread::Current();
  MutexLock mu(self, *Locks::lambda_class_table_lock_);

  allow_new_weaks_ = true;
  new_weaks_condition_.Broadcast(self);
}

void BoxClassTable::BroadcastForNewWeakBoxedLambdas() {
  CHECK(kUseReadBarrier);
  Thread* self = Thread::Current();
  MutexLock mu(self, *Locks::lambda_class_table_lock_);
  new_weaks_condition_.Broadcast(self);
}

void BoxClassTable::EmptyFn::MakeEmpty(std::pair<UnorderedMapKeyType, ValueType>& item) const {
  item.first.clear();

  Locks::mutator_lock_->AssertSharedHeld(Thread::Current());
  item.second = ValueType();  // Also clear the GC root.
}

bool BoxClassTable::EmptyFn::IsEmpty(const std::pair<UnorderedMapKeyType, ValueType>& item) const {
  bool is_empty = item.first.empty();
  DCHECK_EQ(item.second.IsNull(), is_empty);

  return is_empty;
}

bool BoxClassTable::EqualsFn::operator()(const UnorderedMapKeyType& lhs,
                                         const UnorderedMapKeyType& rhs) const {
  // Be damn sure the classes don't just move around from under us.
  Locks::mutator_lock_->AssertSharedHeld(Thread::Current());

  // Being the same class name isn't enough, must also have the same class loader.
  // When we are in the same class loader, classes are equal via the pointer.
  return lhs == rhs;
}

size_t BoxClassTable::HashFn::operator()(const UnorderedMapKeyType& key) const {
  return std::hash<std::string>()(key);
}

}  // namespace lambda
}  // namespace art
