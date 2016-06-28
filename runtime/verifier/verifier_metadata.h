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

#ifndef ART_RUNTIME_VERIFIER_VERIFIER_METADATA_H_
#define ART_RUNTIME_VERIFIER_VERIFIER_METADATA_H_

#include <set>

#include "art_field.h"
#include "art_method.h"
#include "base/mutex.h"

namespace art {

namespace mirror {
  class Class;
}  // namespace mirror

namespace verifier {

class VerifierMetadata {
 public:
  typedef std::pair<mirror::Class*, mirror::Class*> SubtypeRelationBase;

  struct SubtypeRelation : public SubtypeRelationBase {
    SubtypeRelation(mirror::Class* parent, mirror::Class* child)
        : SubtypeRelationBase(parent, child) {}
    SubtypeRelation(const SubtypeRelation&) = default;

    mirror::Class* GetParent() const { return first; }
    mirror::Class* GetChild() const { return second; }
  };

  const std::set<mirror::Class*>& GetResolvedClasses() const { return resolved_classes_; }
  const std::set<ArtMethod*>& GetResolvedMethods() const { return resolved_methods_; }
  const std::set<ArtField*>& GetResolvedFields() const { return resolved_fields_; }

  const std::set<SubtypeRelation> GetExtendsRelations() const { return extends_relations_; }
  const std::set<SubtypeRelation> GetImplementsRelations() const { return implements_relations_; }

  const std::set<std::string>& GetUnresolvedClasses() const { return unresolved_classes_; }

  void AddResolvedClass(mirror::Class* klass)
      SHARED_REQUIRES(Locks::mutator_lock_);

  void AddResolvedMethod(mirror::Class* referrer, ArtMethod* method)
      SHARED_REQUIRES(Locks::mutator_lock_);

  void AddResolvedField(ArtField* field, mirror::Class* referrer)
      SHARED_REQUIRES(Locks::mutator_lock_);

  void AddUnresolvedClass(const std::string& descriptor)
      SHARED_REQUIRES(Locks::mutator_lock_);

  void AddSubtypeRelation(mirror::Class* parent, mirror::Class* child)
      SHARED_REQUIRES(Locks::mutator_lock_);

  std::string Dump() const
      SHARED_REQUIRES(Locks::mutator_lock_);

 private:
  std::set<mirror::Class*> resolved_classes_;
  std::set<ArtMethod*> resolved_methods_;
  std::set<ArtField*> resolved_fields_;
  std::set<SubtypeRelation> extends_relations_;
  std::set<SubtypeRelation> implements_relations_;

  std::set<std::string> unresolved_classes_;
};

}  // namespace verifier
}  // namespace art

#endif  // ART_RUNTIME_VERIFIER_VERIFIER_METADATA_H_
