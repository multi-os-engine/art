
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

#include "verifier_metadata.h"

#include <iostream>

#include "class_linker.h"
#include "mirror/class-inl.h"
#include "reg_type_cache.h"
#include "runtime.h"
#include "utils.h"

namespace art {
namespace verifier {

std::ostream& operator<<(std::ostream& os, const MethodResolutionType& rhs) {
  switch (rhs) {
    case kDirectMethodResolution:
      os << "direct";
      break;
    case kVirtualMethodResolution:
      os << "virtual";
      break;
    case kInterfaceMethodResolution:
      os << "interface";
      break;
  }
  return os;
}

inline static bool IsInBootClassPath(mirror::Class* klass)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  return klass->IsBootStrapClassLoaded();
  // DCHECK_EQ(result, ContainsElement(Runtime::Current()->GetClassLinker()->GetBootClassPath(),
  //                                   &klass->GetDexFile()));
}

inline static void GetBootClassPathFrontier_Impl(
    mirror::Class* klass, /* out */ std::set<mirror::Class*>* frontier)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  for (; klass != nullptr; klass = klass->GetSuperClass()) {
    if (IsInBootClassPath(klass)) {
      frontier->insert(klass);
      return;
    }

    uint32_t num_direct_ifaces = klass->NumDirectInterfaces();
    if (num_direct_ifaces > 0u) {
      Thread* self = Thread::Current();
      StackHandleScope<1> hs(self);
      HandleWrapper<mirror::Class> h_klass(hs.NewHandleWrapper(&klass));

      for (uint32_t i = 0u; i < num_direct_ifaces; ++i) {
        mirror::Class* direct_iface = mirror::Class::GetDirectInterface(self, h_klass, i);
        if (IsInBootClassPath(direct_iface)) {
          // Direct interface in boot classpath. Add it to `frontier` and
          // do not scan further.
          frontier->insert(direct_iface);
        } else {
          // Direct interface not in the boot classpath. Scan it recursively
          // in case it implements an interface which is in the boot classpath.
          GetBootClassPathFrontier_Impl(direct_iface, frontier);
        }
      }
    }
  }

  std::string tmp;
  LOG(FATAL) << "Class " << klass->GetDescriptor(&tmp) << " has unresolved superclass";
  UNREACHABLE();
}

inline static std::set<mirror::Class*> GetBootClassPathFrontier(mirror::Class* klass)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  std::set<mirror::Class*> frontier;
  GetBootClassPathFrontier_Impl(klass, &frontier);
  return frontier;
}

void VerifierMetadata::AddAssignable(mirror::Class* destination,
                                     mirror::Class* source,
                                     bool is_assignable) {
  DCHECK(IsInBootClassPath(destination));
  DCHECK(IsInBootClassPath(source));
  DCHECK_EQ(destination->IsAssignableFrom(source), is_assignable);
  if (destination != source) {
    MutexLock mu(Thread::Current(), lock_);
    std::string temp;
    assignables_.emplace(AssignabilityTestTuple(destination->GetDescriptor(&temp),
                                                source->GetDescriptor(&temp),
                                                is_assignable));
  }
}

void VerifierMetadata::RecordAssignabilityTest(mirror::Class* destination,
                                               mirror::Class* source,
                                               bool is_strict,
                                               bool is_assignable) {
  DCHECK(destination != nullptr && destination->IsResolved() && !destination->IsPrimitive());
  DCHECK(source != nullptr && source->IsResolved() && !source->IsPrimitive());

  if (!IsInBootClassPath(destination)) {
    // Assignability to a non-boot classpath class is not a dependency.
    return;
  }

  if (destination == source ||
      destination->IsObjectClass() ||
      (!is_strict && destination->IsInterface())) {
    // Cases when `destination` is trivially assignable from `source`.
    DCHECK(is_assignable);
    return;
  }

  if (destination->IsArrayClass() != source->IsArrayClass()) {
    // One is an array, the other one isn't and `destination` is not Object.
    // Trivially not assignable.
    DCHECK(!is_assignable);
    return;
  }

  if (destination->IsArrayClass()) {
    // Both types are arrays. Solve recursively.
    DCHECK(source->IsArrayClass());
    RecordAssignabilityTest(destination->GetComponentType(),
                            source->GetComponentType(),
                            /* is_strict */ true,
                            is_assignable);
    return;
  }

  // std::cout << "assignable=" << IsInBootClassPath(source) << std::endl;

  if (IsInBootClassPath(source)) {
    // Simple case when both `destination` and `source` are in the boot
    // classpath. Record a single dependency between them.
    AddAssignable(destination, source, is_assignable);
    return;
  }

  std::set<mirror::Class*> frontier = GetBootClassPathFrontier(source);

  if (frontier.size() == 1) {
    // Optimize special case where there are no interfaces in the frontier because
    // we do not need to run 'IsAssignableFrom'.
    AddAssignable(destination, *frontier.begin(), is_assignable);
  } else if (!is_assignable) {
    // Another special case when we know that none of the frontier classes
    // are assignable to `destination`.
    for (mirror::Class* klass_frontier : frontier) {
      AddAssignable(destination, klass_frontier, /* is_assignable */ false);
    }
  } else {
    // Test and record which of the frontier classes are assignable to `destination`.
    // There must be at least one.
    bool current_assignable = false;
    bool found_assignable = false;

    for (mirror::Class* klass_frontier : frontier) {
      current_assignable = destination->IsAssignableFrom(klass_frontier);
      found_assignable |= current_assignable;
      AddAssignable(destination, klass_frontier, current_assignable);
    }

    DCHECK(found_assignable);
  }
}

template <typename T>
inline static uint32_t GetAccessFlags(T* element) SHARED_REQUIRES(Locks::mutator_lock_) {
  if (element == nullptr) {
    return VerifierMetadata::kUnresolved;
  } else {
    uint32_t access_flags = element->GetAccessFlags();
    DCHECK_NE(access_flags, VerifierMetadata::kUnresolved);
    return access_flags;
  }
}

void VerifierMetadata::RecordClassResolution(uint16_t dex_type_idx, mirror::Class* klass) {
  if (klass != nullptr && !IsInBootClassPath(klass)) {
    return;
  }

  MutexLock mu(Thread::Current(), lock_);
  classes_.emplace(ClassResolutionTuple(dex_type_idx, GetAccessFlags(klass)));
}

void VerifierMetadata::RecordFieldResolution(uint32_t dex_field_idx, ArtField* field) {
  if (field != nullptr && !IsInBootClassPath(field->GetDeclaringClass())) {
    // Field is declared in the loaded dex file. No boot classpath dependency to record.
    return;
  }

  MutexLock mu(Thread::Current(), lock_);
  fields_.emplace(FieldResolutionTuple(dex_field_idx, GetAccessFlags(field)));
}

void VerifierMetadata::RecordMethodResolution(uint32_t dex_method_idx,
                                              MethodResolutionType resolution_type,
                                              ArtMethod* method) {
  if (method != nullptr && !IsInBootClassPath(method->GetDeclaringClass())) {
    // Method is declared in the loaded dex file. No boot classpath dependency to record.
    return;
  }

  MutexLock mu(Thread::Current(), lock_);
  methods_.emplace(MethodResolutionTuple(dex_method_idx, resolution_type, GetAccessFlags(method)));
}

bool VerifierMetadata::Verify(Handle<mirror::ClassLoader> class_loader,
                              Handle<mirror::DexCache> dex_cache,
                              bool can_load_classes) const {
  for (auto&& entry : GetClasses()) {
    const char* descriptor = dex_file_.StringByTypeIdx(entry.GetDexTypeIndex());
    mirror::Class* klass = RegTypeCache::ResolveClass(
        descriptor,
        class_loader.Get(),
        can_load_classes);
    DCHECK(klass == nullptr || klass->IsResolved());
    if (entry.GetModifiers() != GetAccessFlags(klass)) {
      return false;
    }
  }

  for (auto&& entry : GetAssignables()) {
    mirror::Class* destination = RegTypeCache::ResolveClass(
        entry.GetDestination().c_str(),
        class_loader.Get(),
        can_load_classes);
    mirror::Class* source = RegTypeCache::ResolveClass(
        entry.GetSource().c_str(),
        class_loader.Get(),
        can_load_classes);
    if (destination == nullptr || source == nullptr) {
      return false;
    }
    DCHECK(destination->IsResolved() && source->IsResolved());
    if (destination->IsAssignableFrom(source) != entry.IsAssignable()) {
      return false;
    }
  }

  for (auto&& entry : GetFields()) {
    ArtField* field = Runtime::Current()->GetClassLinker()->ResolveFieldJLS(
        dex_file_,
        entry.GetDexFieldIndex(),
        dex_cache,
        class_loader);
    if (field == nullptr) {
      DCHECK(Thread::Current()->IsExceptionPending());
      Thread::Current()->ClearException();
    }
    if (entry.GetModifiers() != GetAccessFlags(field)) {
      return false;
    }
  }

  for (auto&& entry : GetMethods()) {
    const DexFile::MethodId& method_id = dex_file_.GetMethodId(entry.GetDexMethodIndex());

    mirror::Class* klass = RegTypeCache::ResolveClass(
        dex_file_.GetMethodDeclaringClassDescriptor(method_id),
        class_loader.Get(),
        can_load_classes);
    if (klass == nullptr) {
      return false;
    }
    DCHECK(klass->IsResolved());

    const char* name = dex_file_.GetMethodName(method_id);
    const Signature signature = dex_file_.GetMethodSignature(method_id);
    auto pointer_size = Runtime::Current()->GetClassLinker()->GetImagePointerSize();

    ArtMethod* method = nullptr;
    switch (entry.GetMethodResolutionType()) {
      case kDirectMethodResolution:
        method = klass->FindDirectMethod(name, signature, pointer_size);
        break;
      case kVirtualMethodResolution:
        method = klass->FindVirtualMethod(name, signature, pointer_size);
        break;
      case kInterfaceMethodResolution:
        method = klass->FindInterfaceMethod(name, signature, pointer_size);
        break;
      default:
        LOG(FATAL) << "Unknown resolution kind";
        UNREACHABLE();
    }

    if (entry.GetModifiers() != GetAccessFlags(method)) {
      return false;
    }
  }

  return true;
}

void VerifierMetadata::Dump(std::ostream& os) const {
  std::string tmp;

  for (auto& entry : GetClasses()) {
    os << "class " << dex_file_.StringByTypeIdx(entry.GetDexTypeIndex()) << " ";
    if (entry.IsResolved()) {
      os << PrettyJavaAccessFlags(entry.GetModifiers());
    } else {
      os << "unresolved";
    }
    os << std::endl;
  }

  for (auto& entry : GetAssignables()) {
    os << "type "
       << entry.GetDestination()
       << (entry.IsAssignable() ? "" : " not") << " assignable from "
       << entry.GetSource();
    os << std::endl;
  }

  for (auto&& entry : GetFields()) {
    const DexFile::FieldId& field_id = dex_file_.GetFieldId(entry.GetDexFieldIndex());
    os << "field "
       << dex_file_.GetFieldDeclaringClassDescriptor(field_id) << "->"
       << dex_file_.GetFieldName(field_id) << ":"
       << dex_file_.GetFieldTypeDescriptor(field_id) << " ";
    if (entry.IsResolved()) {
      os << PrettyJavaAccessFlags(entry.GetModifiers());
    } else {
      os << "unresolved";
    }
    os << std::endl;
  }

  for (auto&& entry : GetMethods()) {
    const DexFile::MethodId& method_id = dex_file_.GetMethodId(entry.GetDexMethodIndex());
    os << entry.GetMethodResolutionType()
       << " method "
       << dex_file_.GetMethodDeclaringClassDescriptor(method_id) << "->"
       << dex_file_.GetMethodName(method_id)
       << dex_file_.GetMethodSignature(method_id).ToString() << " ";
    if (entry.IsResolved()) {
      os << PrettyJavaAccessFlags(entry.GetModifiers());
    } else {
      os << "unresolved";
    }
    os << std::endl;
  }
}

}  // namespace verifier
}  // namespace art
