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
// #include "gc/heap.h"
#include "mirror/class-inl.h"
#include "runtime.h"
#include "utils.h"

namespace art {
namespace verifier {

inline static bool IsInBootClassPath(mirror::Class* klass)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  bool result = klass->IsBootStrapClassLoaded();
  DCHECK_EQ(result, ContainsElement(Runtime::Current()->GetClassLinker()->GetBootClassPath(),
                                    &klass->GetDexFile()));
  return result;
}

inline static mirror::Class* AdjustClassIfArray(mirror::Class* klass)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  DCHECK(klass != nullptr);
  if (klass->IsArrayClass()) {
    DCHECK(klass->GetSuperClass()->IsObjectClass());
    return klass->GetSuperClass();
  } else {
    return klass;
  }
}

inline static bool IsRecordableClass(mirror::Class* klass)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  return klass != nullptr &&
         klass->IsResolved() &&
         !klass->IsArrayClass() &&
         IsInBootClassPath(klass);
}

inline static mirror::Class* FindDirectInterfaceWhichImplements(mirror::Class* implementer,
                                                                mirror::Class* interface)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  DCHECK(interface->IsInterface());
  DCHECK(implementer->Implements(interface));

  Thread* self = Thread::Current();

  StackHandleScope<1> hs(self);
  HandleWrapper<mirror::Class> h_implementer(hs.NewHandleWrapper(&implementer));
  for (uint32_t i = 0; i < h_implementer->NumDirectInterfaces(); ++i) {
    mirror::Class* direct_iface = mirror::Class::GetDirectInterface(self, h_implementer, i);
    if (direct_iface == interface || direct_iface->Implements(interface)) {
      return direct_iface;
    }
  }

  LOG(FATAL) << "Direct interface implementing the interface not found";
  UNREACHABLE();
}

VerifierMetadata::SubtypeRelation::SubtypeRelation(mirror::Class* parent, mirror::Class* child)
    : VerifierMetadata::SubtypeRelationBase(parent, child) {
  if (kIsDebugBuild) {
    ScopedObjectAccess soa(Thread::Current());
    DCHECK(IsRecordableClass(parent));
    DCHECK(IsRecordableClass(child));
    DCHECK_NE(parent, child);
    DCHECK(child->IsSubClass(parent) || child->Implements(parent));
  }
}

VerifierMetadata::UnresolvedMethod::UnresolvedMethod(mirror::Class* klass_ref,
                                                     const std::string& name,
                                                     const std::string& signature)
    : VerifierMetadata::UnresolvedMethodBase(klass_ref, name, signature) {}

void VerifierMetadata::AddResolvedClass(mirror::Class* klass) {
  DCHECK(klass != nullptr);
  klass = AdjustClassIfArray(klass);
  if (IsInBootClassPath(klass)) {
    DCHECK(IsRecordableClass(klass));
    resolved_classes_.insert(klass);
  }
}

void VerifierMetadata::AddSubtypeRelation(mirror::Class* parent, mirror::Class* child) {
  if (IsInBootClassPath(parent)) {
    AddResolvedClass(parent);

    if (child->IsSubClass(parent)) {
      // Referrer extends the declaring class. Keep moving up the hierarchy.
      while (!IsInBootClassPath(child)) {
        child = child->GetSuperClass();
      }
      if (parent != child) {
        AddResolvedClass(child);
        extends_relations_.insert(SubtypeRelation(parent, child));
      }
    } else {
      DCHECK(parent->IsInterface());
      DCHECK(child->Implements(parent));
      while (!IsInBootClassPath(child)) {
        mirror::Class* super = child->GetSuperClass();
        if (super != nullptr && super->Implements(parent)) {
          // Referrer implements the declaring interface but so does its
          // superclass. Keep moving up the hierarchy.
          child = super;
        } else {
          // Referrer implements the declaring interface. Pick that interface
          // or an interface which extends it from the list of implemented
          // interfaces. Note that there may be multiple.
          child = FindDirectInterfaceWhichImplements(child, parent);
        }
      }
      if (parent != child) {
        resolved_classes_.insert(child);
        implements_relations_.insert(SubtypeRelation(parent, child));
      }
    }
  }
}

void VerifierMetadata::AddResolvedMethod(ArtMethod* method, mirror::Class* klass_ref) {
  DCHECK(method != nullptr);
  DCHECK(klass_ref != nullptr && klass_ref->IsResolved());

  mirror::Class* klass_decl = method->GetDeclaringClass();
  if (IsInBootClassPath(klass_decl)) {
    AddSubtypeRelation(/* parent */ klass_decl, /* child */ klass_ref);
    resolved_methods_.insert(method);
  }
}

void VerifierMetadata::AddResolvedField(ArtField* field, mirror::Class* klass_ref) {
  DCHECK(field != nullptr);
  DCHECK(klass_ref != nullptr && klass_ref->IsResolved());

  mirror::Class* klass_decl = field->GetDeclaringClass();
  DCHECK(field->IsStatic() || !klass_decl->IsInterface());

  if (IsInBootClassPath(klass_decl)) {
    AddSubtypeRelation(/* parent */ klass_decl, /* child */ klass_ref);
    resolved_fields_.insert(field);
  }
}

void VerifierMetadata::AddUnresolvedClass(const std::string& descriptor) {
  unresolved_classes_.insert(descriptor);
}

void VerifierMetadata::AddUnresolvedMethod(mirror::Class* klass,
                                           const std::string& name,
                                           const std::string& signature) {
  DCHECK(klass != nullptr && klass->IsResolved());

  // TODO: unresolved superclass?
  while (!IsInBootClassPath(klass)) {
    klass = klass->GetSuperClass();
  }

  AddResolvedClass(klass);
  unresolved_methods_.insert(UnresolvedMethod(klass, name, signature));
}

std::string VerifierMetadata::Dump() const {
  std::string tmp;
  std::stringstream result;

  for (mirror::Class* klass : resolved_classes_) {
    result << "resolved class " << klass->GetDescriptor(&tmp) << " {\n";
    for (const SubtypeRelation& extends : extends_relations_) {
      if (extends.GetChild() == klass) {
        result << "  extends " << extends.GetParent()->GetDescriptor(&tmp) << "\n";
      }
    }
    for (const SubtypeRelation& implements : implements_relations_) {
      if (implements.GetChild() == klass) {
        result << "  implements " << implements.GetParent()->GetDescriptor(&tmp) << "\n";
      }
    }
    result << "  access_flags = " << PrettyJavaAccessFlags(klass->GetAccessFlags()) << "\n";
    for (ArtMethod* method : resolved_methods_) {
      if (method->GetDeclaringClass() == klass) {
        result << "  resolved method " << method->GetName() << " {\n";
        result << "    signature = " << method->GetSignature().ToString() << "\n";
        result << "    access_flags = " << PrettyJavaAccessFlags(method->GetAccessFlags()) << "\n";
        result << "  }\n";
      }
    }
    for (auto&& method : unresolved_methods_) {
      if (method.GetDeclaringClass() == klass) {
        result << "  unresolved method " << method.GetName() << " {\n";
        result << "    signature = " << method.GetSignature() << "\n";
        result << "  }\n";
      }
    }
    for (ArtField* field : resolved_fields_) {
      if (field->GetDeclaringClass() == klass) {
        result << "  field " << field->GetName() << " {\n";
        result << "    type = " << field->GetTypeDescriptor() << "\n";
        result << "    access_flags = " << PrettyJavaAccessFlags(field->GetAccessFlags()) << "\n";
        result << "  }\n";
      }
    }
    result << "}\n";
  }

  for (auto&& klass : unresolved_classes_) {
    result << "unresolved class " << klass << "\n";
  }

  return result.str();
}

}  // namespace verifier
}  // namespace art
