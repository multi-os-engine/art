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

static bool IsInBootClassPath(mirror::Class* klass)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  bool result = klass->IsBootStrapClassLoaded();
  DCHECK_EQ(result, ContainsElement(Runtime::Current()->GetClassLinker()->GetBootClassPath(),
                                    &klass->GetDexFile()));
  return result;
}

static mirror::Class* AdjustClass(mirror::Class* klass)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  DCHECK(klass != nullptr);

  if (klass->IsArrayClass()) {
    DCHECK(klass->GetSuperClass()->IsObjectClass());
    return klass->GetSuperClass();
  }

  while (klass != nullptr && !IsInBootClassPath(klass)) {
    klass = klass->GetSuperClass();
  }
  return klass;
}

void VerifierMetadata::AddResolvedClass(mirror::Class* klass) {
  DCHECK(klass != nullptr && klass->IsResolved());
  klass = AdjustClass(klass);
  if (klass != nullptr) {
    resolved_classes_.insert(klass);
  }
}

void VerifierMetadata::AddResolvedMethod(mirror::Class* referrer, ArtMethod* method) {
  UNUSED(referrer);
  UNUSED(method);
  // DCHECK(referrer != nullptr && referrer->IsResolved());
  // DCHECK(method != nullptr);

  // mirror::Class* declaring_klass = method->GetDeclaringClass();
  // bool is_interface_method = declaring_klass->IsInterface();
  // DCHECK(is_interface_method ? referrer->Implements(declaring_klass)
  //                            : referrer->IsSubClass(declaring_klass));

  // if (IsInBootClassPath(declaring_klass)) {
  //   resolved_classes_.insert(declaring_klass);
  //   resolved_methods_.insert(method);

  //   mirror::Class* referrer_klass = AdjustClass(referrer);
  //   if (is_interface_method) {

  //   } else {
  //     DCHECK(referrer_klass->IsSubClass(declaring_klass));
  //     if (referrer_klass != declaring_klass) {
  //       resolved_classes_.insert(referrer_klass);
  //       extends_relations_.insert(SubtypeRelation(/* parent */ declaring_klass,
  //                                                 /* child */ referrer_klass));
  //     }
  //   }
  // }
}

static mirror::Class* GetDirectInterface(mirror::Class* klass,
                                         uint32_t idx,
                                         const DexFile& dex_file,
                                         mirror::IfTable* if_table,
                                         int32_t if_table_count)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  DCHECK_LT(idx, klass->NumDirectInterfaces());
  DCHECK_EQ(&klass->GetDexFile(), &dex_file);
  DCHECK_EQ(klass->GetIfTable(), if_table);
  DCHECK_EQ(klass->GetIfTableCount(), if_table_count);

  uint16_t type_idx = klass->GetDirectInterfaceTypeIdx(idx);
  const char* descriptor1 = dex_file.GetTypeDescriptor(dex_file.GetTypeId(type_idx));

  std::string tmp;
  for (int32_t i = 0; i < if_table_count; ++i) {
    mirror::Class* iface = if_table->GetInterface(i);
    const char* descriptor2 = iface->GetDescriptor(&tmp);
    if (strcmp(descriptor1, descriptor2) == 0) {
      return iface;
    }
  }

  LOG(FATAL) << "Direct interface not found in the interface table";
  UNREACHABLE();
}

static mirror::Class* FindDirectInterfaceWhichImplements(mirror::Class* implementer,
                                                         mirror::Class* interface)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  DCHECK(interface->IsInterface());
  DCHECK(implementer->Implements(interface));

  const DexFile& dex_file = implementer->GetDexFile();
  mirror::IfTable* if_table = implementer->GetIfTable();
  int32_t if_table_count = implementer->GetIfTableCount();
  for (uint32_t i = 0, e = implementer->NumDirectInterfaces(); i < e; ++i) {
    mirror::Class* direct_iface = GetDirectInterface(
        implementer, i, dex_file, if_table, if_table_count);
    if (direct_iface == interface || direct_iface->Implements(interface)) {
      return direct_iface;
    }
  }

  LOG(FATAL) << "Direct interface implementing the interface not found";
  UNREACHABLE();
}

void VerifierMetadata::AddResolvedField(ArtField* field, mirror::Class* referrer) {
  DCHECK(field != nullptr);
  DCHECK(referrer != nullptr);

  mirror::Class* klass = field->GetDeclaringClass();
  if (IsInBootClassPath(klass)) {
    resolved_classes_.insert(klass);
    resolved_fields_.insert(field);

    if (referrer->IsSubClass(klass)) {
      // Referrer extends the declaring class. Keep moving up the hierarchy.
      while (!IsInBootClassPath(referrer)) {
        referrer = referrer->GetSuperClass();
      }
      if (klass != referrer) {
        resolved_classes_.insert(referrer);
        extends_relations_.insert(SubtypeRelation(/* parent */ klass, /* child */ referrer));
      }
    } else {
      DCHECK(klass->IsInterface());
      DCHECK(referrer->Implements(klass));
      DCHECK(field->IsStatic());
      while (!IsInBootClassPath(referrer)) {
        mirror::Class* super = referrer->GetSuperClass();
        if (super != nullptr && super->Implements(klass)) {
          // Referrer implements the declaring interface but so does its
          // superclass. Keep moving up the hierarchy.
          referrer = super;
        } else {
          // Referrer implements the declaring interface. Pick that interface
          // or an interface which extends it from the list of implemented
          // interfaces. Note that there may be multiple.
          referrer = FindDirectInterfaceWhichImplements(referrer, klass);
        }
      }
      if (klass != referrer) {
        resolved_classes_.insert(referrer);
        implements_relations_.insert(SubtypeRelation(/* parent */ klass, /* child */ referrer));
      }
    }
  }
}

void VerifierMetadata::AddUnresolvedClass(const std::string& descriptor) {
  unresolved_classes_.insert(descriptor);
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
        result << "  method " << method->GetName() << " {\n";
        result << "    full name = " << PrettyMethod(method) << "\n";
        result << "    access_flags = " << PrettyJavaAccessFlags(method->GetAccessFlags()) << "\n";
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
