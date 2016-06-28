
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

inline static bool AreEqualOrParentAndChild(mirror::Class* parent, mirror::Class* child)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  if (parent == child) {
    return true;
  } else if (parent->IsObjectClass()) {
    return true;
  } else if (child->IsArrayClass()) {
    // Array classes do not implement any interfaces and their superclass is
    // Object. Since we know that `parent` is not Object, return false.
    return false;
  } else if (child->IsInterface()) {
    // Interfaces inherit Object, which `parent` is not, but can implement
    // other interfaces.
    return parent->IsInterface() && child->Implements(parent);
  } else {
    return child->IsSubClass(parent) || (parent->IsInterface() && child->Implements(parent));
  }
}

inline static mirror::Class* GetBootClassPathSuperclass(mirror::Class* klass)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  for (; klass != nullptr; klass = klass->GetSuperClass()) {
    if (IsInBootClassPath(klass)) {
      return klass;
    }
  }

  std::string tmp;
  LOG(FATAL) << "Class " << klass->GetDescriptor(&tmp) << " has unresolved superclass";
  UNREACHABLE();
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
                                                source->GetDescriptor(&temp)), is_assignable);
  }
}

inline static ArtField* FindFieldInClass(mirror::Class* klass_ref,
                                         const DexFile& dex_file,
                                         uint32_t dex_field_idx)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  Thread* self = Thread::Current();
  StackHandleScope<1> hs(self);
  Handle<mirror::Class> h_klass_ref(hs.NewHandle(klass_ref));
  const DexFile::FieldId& field_id = dex_file.GetFieldId(dex_field_idx);
  return mirror::Class::FindField(self,
                                  h_klass_ref,
                                  StringPiece(dex_file.StringDataByIdx(field_id.name_idx_)),
                                  StringPiece(dex_file.StringDataByIdx(
                                      dex_file.GetTypeId(field_id.type_idx_).descriptor_idx_)));
}

inline static ArtMethod* FindMethodInClass(mirror::Class* klass_ref,
                                           const DexFile& dex_file,
                                           uint32_t dex_method_idx,
                                           MethodResolutionType resolution_type)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  const DexFile::MethodId& method_id = dex_file.GetMethodId(dex_method_idx);
  const char* name = dex_file.GetMethodName(method_id);
  const Signature signature = dex_file.GetMethodSignature(method_id);
  auto pointer_size = Runtime::Current()->GetClassLinker()->GetImagePointerSize();

  switch (resolution_type) {
    case kDirectMethodResolution:
      return klass_ref->FindDirectMethod(name, signature, pointer_size);
    case kVirtualMethodResolution:
      return klass_ref->FindVirtualMethod(name, signature, pointer_size);
    case kInterfaceMethodResolution:
      return klass_ref->FindInterfaceMethod(name, signature, pointer_size);
  }
}

void VerifierMetadata::AddField(mirror::Class* klass_ref,
                                uint32_t dex_field_idx,
                                bool is_static,
                                ArtField* field) {
  MutexLock mu(Thread::Current(), lock_);
  std::string tmp;

  DCHECK(IsInBootClassPath(klass_ref));
  DCHECK(field == nullptr || IsInBootClassPath(field->GetDeclaringClass()));
  DCHECK_EQ(FindFieldInClass(klass_ref, dex_file_, dex_field_idx), field);

  fields_.emplace(FieldResolutionTuple(klass_ref->GetDescriptor(&tmp),
                                       dex_field_idx,
                                       is_static), field);
}

void VerifierMetadata::AddMethod(mirror::Class* klass_ref,
                                 uint32_t dex_method_idx,
                                 MethodResolutionType resolution_type,
                                 ArtMethod* method) {
  MutexLock mu(Thread::Current(), lock_);
  std::string tmp;

  DCHECK(IsInBootClassPath(klass_ref));
  DCHECK(method == nullptr || IsInBootClassPath(method->GetDeclaringClass()));
  DCHECK_EQ(FindMethodInClass(klass_ref, dex_file_, dex_method_idx, resolution_type), method)
      << klass_ref->GetDescriptor(&tmp) << ", " << resolution_type
      << ", " << PrettyMethod(method)
      << ", " << dex_file_.GetMethodDeclaringClassDescriptor(dex_file_.GetMethodId(dex_method_idx));

  methods_.emplace(MethodResolutionTuple(klass_ref->GetDescriptor(&tmp),
                                         dex_method_idx,
                                         resolution_type), method);
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
    // Simple case when both `destination` and `source` are int the boot
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

void VerifierMetadata::RecordClassResolution(const std::string& descriptor, mirror::Class* klass) {
  if (klass == nullptr) {
    MutexLock mu(Thread::Current(), lock_);
    uint32_t access_flags = kUnresolvedClass;
    classes_.emplace(ClassResolutionTuple(descriptor), access_flags);
  } else if (IsInBootClassPath(klass)) {
    MutexLock mu(Thread::Current(), lock_);
    uint32_t access_flags = klass->GetAccessFlags();
    DCHECK_NE(access_flags, kUnresolvedClass);
    classes_.emplace(ClassResolutionTuple(descriptor), access_flags);
  } else {
    // std::cout << "SKIPPED " << descriptor << std::endl;
  }
}

void VerifierMetadata::RecordFieldResolution(uint32_t dex_field_idx,
                                             bool is_static,
                                             mirror::Class* klass_ref,
                                             ArtField* field) {
  DCHECK(klass_ref != nullptr && klass_ref->IsResolved());

  mirror::Class* klass_decl = nullptr;
  if (field != nullptr) {
    klass_decl = field->GetDeclaringClass();
    if (!IsInBootClassPath(klass_decl)) {
      // Field is declared in the loaded dex file. No boot classpath dependency to record.
      return;
    }
  }

  // std::cout << "field=" << IsInBootClassPath(klass_ref) << std::endl;

  if (IsInBootClassPath(klass_ref)) {
    // Both referring and declaring class are in the boot classpath. Record the
    // dependency on these two classes.
    AddField(klass_ref, dex_field_idx, is_static, field);
  } else if (is_static) {
    // If resolved, static field is declared in a superclass or superinterface
    // of `klass_ref`. Look at the boot classpath frontier of `klass_ref` and
    // record dependencies on each individual class.
    // If unresolved, make sure that the field does not resolve for any of this
    // class' boot classpath frontier classes/interfaces.

    // Careful! Multiple frontier classes can implement a field of the same name!

    DCHECK(field == nullptr || AreEqualOrParentAndChild(klass_decl, klass_ref));
    std::set<mirror::Class*> frontier = GetBootClassPathFrontier(klass_ref);

    for (mirror::Class* klass_frontier : frontier) {
      if (field == nullptr) {
        AddField(klass_frontier, dex_field_idx, is_static, nullptr);
      } else {
        AddField(klass_frontier,
                 dex_field_idx,
                 is_static,
                 FindFieldInClass(klass_frontier, dex_file_, dex_field_idx));
      }
    }
  } else {
    // Interfaces cannot declare instance fields. Record dependency only on
    // the first superclass in the boot classpath.
    DCHECK(field == nullptr || klass_ref->IsSubClass(klass_decl));
    AddField(GetBootClassPathSuperclass(klass_ref), dex_field_idx, is_static, field);
  }
}

void VerifierMetadata::RecordMethodResolution(uint32_t dex_method_idx,
                                              MethodResolutionType resolution_type,
                                              mirror::Class* klass_ref,
                                              ArtMethod* method) {
  DCHECK(klass_ref != nullptr && klass_ref->IsResolved());

  mirror::Class* klass_decl = nullptr;
  if (method != nullptr) {
    klass_decl = method->GetDeclaringClass();
    if (!IsInBootClassPath(klass_decl)) {
      // Method is declared in the loaded dex file. No boot classpath dependency
      // to record.
      return;
    }
  }

  if (IsInBootClassPath(klass_ref)) {
    // Both referring and declaring class are in the boot classpath. Record the
    // dependency on these two classes.
    AddMethod(klass_ref, dex_method_idx, resolution_type, method);
    return;
  }

  std::set<mirror::Class*> frontier = GetBootClassPathFrontier(klass_ref);

  if (resolution_type == kInterfaceMethodResolution) {
    // Interface resolution scans the referring class (not in boot classpath) and
    // all implemented interfaces. Note that `klass_ref` may not be an interface.
  } else {
    // Direct and virtual method resolutions only walk up superclasses. Record
    // the dependency on the first superclass of `klass_ref` in boot classpath.
    DCHECK(resolution_type == kDirectMethodResolution ||
           resolution_type == kVirtualMethodResolution);
    DCHECK(method == nullptr || AreEqualOrParentAndChild(klass_decl, klass_ref));

    for (mirror::Class* klass_frontier : frontier) {
      if (method == nullptr) {
        AddMethod(klass_frontier, dex_method_idx, resolution_type, nullptr);
      } else {
        AddMethod(klass_frontier,
                  dex_method_idx,
                  resolution_type,
                  FindMethodInClass(klass_frontier, dex_file_, dex_method_idx, resolution_type));
      }
    }
  }
}

void VerifierMetadata::Dump(std::ostream& os) const {
  std::string tmp;

  for (auto& entry : GetClasses()) {
    os << "class "
       << entry.first.GetDescriptor()
       << (entry.second == kUnresolvedClass ? " unresolved" : " resolved")
       << std::endl;
  }

  for (auto& entry : GetAssignables()) {
    os << "type "
       << entry.first.GetDestination()
       << (entry.second ? "" : " not") << " assignable from "
       << entry.first.GetSource();
    os << std::endl;
  }

  for (auto&& entry : GetFields()) {
    ArtField* field = entry.second;
    const DexFile::FieldId& field_id = dex_file_.GetFieldId(entry.first.GetDexFieldIndex());
    os << (entry.first.IsStatic() ? "static" : "instance")
       << " field "
       << entry.first.GetReferringClass() << "->"
       << dex_file_.GetFieldName(field_id) << ":"
       << dex_file_.GetFieldTypeDescriptor(field_id) << " ";
    if (field == nullptr) {
      os << "unresolved";
    } else {
      os << "resolved to " << field->GetDeclaringClass()->GetDescriptor(&tmp);
    }
    os << std::endl;
  }

  for (auto&& entry : GetMethods()) {
    ArtMethod* method = entry.second;
    const DexFile::MethodId& method_id = dex_file_.GetMethodId(entry.first.GetDexMethodIndex());
    os << entry.first.GetMethodResolutionType()
       << " method "
       << entry.first.GetReferringClass() << "->"
       << dex_file_.GetMethodName(method_id)
       << dex_file_.GetMethodSignature(method_id).ToString() << " ";
    if (method == nullptr) {
      os << "unresolved";
    } else {
      os << "resolved to " << method->GetDeclaringClass()->GetDescriptor(&tmp);
    }
    os << std::endl;
  }
}

}  // namespace verifier
}  // namespace art
