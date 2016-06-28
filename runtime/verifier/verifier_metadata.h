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
#include <map>

#include "art_field.h"
#include "art_method.h"
#include "base/mutex.h"

namespace art {

namespace mirror {
  class Class;
}  // namespace mirror

namespace verifier {

enum MethodResolutionType {
  kDirectMethodResolution,
  kVirtualMethodResolution,
  kInterfaceMethodResolution
};
std::ostream& operator<<(std::ostream& os, const MethodResolutionType& rhs);

class VerifierMetadata {
 public:
  explicit VerifierMetadata(const DexFile& dex_file)
      : dex_file_(dex_file),
        lock_("VerifierMetadata lock", kVerifierMetadataLock) {}

  enum HierarchyRelation {
    ChildExtendsParent,
    ChildImplementsParent,
    NotChildAndParent
  };

  struct AssignabilityTestTuple : public std::pair<std::string, std::string> {
    using Base = std::pair<std::string, std::string>;
    using Base::Base;
    AssignabilityTestTuple(const AssignabilityTestTuple&) = default;

    const std::string& GetDestination() const { return first; }
    const std::string& GetSource() const { return second; }
  };

  struct ClassResolutionTuple : public std::tuple<std::string> {
    using Base = std::tuple<std::string>;
    using Base::Base;
    ClassResolutionTuple(const ClassResolutionTuple&) = default;

    const std::string& GetDescriptor() const { return std::get<0>(*this); }
  };

  struct FieldResolutionTuple : public std::tuple<std::string, uint32_t, bool> {
    using Base = std::tuple<std::string, uint32_t, bool>;
    using Base::Base;
    FieldResolutionTuple(const FieldResolutionTuple&) = default;

    const std::string& GetReferringClass() const { return std::get<0>(*this); }
    uint32_t GetDexFieldIndex() const { return std::get<1>(*this); }
    bool IsStatic() const { return std::get<2>(*this); }
  };

  struct MethodResolutionTuple : public std::tuple<std::string, uint32_t, MethodResolutionType> {
    using Base = std::tuple<std::string, uint32_t, MethodResolutionType>;
    using Base::Base;
    MethodResolutionTuple(const MethodResolutionTuple&) = default;

    const std::string& GetReferringClass() const { return std::get<0>(*this); }
    uint32_t GetDexMethodIndex() const { return std::get<1>(*this); }
    MethodResolutionType GetMethodResolutionType() const { return std::get<2>(*this); }
  };

  const std::map<ClassResolutionTuple, uint32_t>& GetClasses() const { return classes_; }
  const std::map<FieldResolutionTuple, ArtField*>& GetFields() const { return fields_; }
  const std::map<MethodResolutionTuple, ArtMethod*>& GetMethods() const { return methods_; }
  const std::map<AssignabilityTestTuple, bool>& GetAssignables() const { return assignables_; }

  void RecordAssignabilityTest(mirror::Class* destination,
                               mirror::Class* source,
                               bool is_strict,
                               bool expected_outcome)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!lock_);

  void RecordClassResolution(const std::string& descriptor, mirror::Class* klass)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!lock_);

  void RecordFieldResolution(uint32_t dex_field_idx,
                             bool is_static,
                             mirror::Class* klass_ref,
                             ArtField* field)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!lock_);

  void RecordMethodResolution(uint32_t dex_method_idx,
                              MethodResolutionType resolution_type,
                              mirror::Class* klass_ref,
                              ArtMethod* method)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!lock_);

  void Dump(std::ostream& os) const
      SHARED_REQUIRES(Locks::mutator_lock_);

  static constexpr uint32_t kUnresolvedClass = static_cast<uint32_t>(-1);

 private:
  void AddAssignable(mirror::Class* destination, mirror::Class* source, bool is_assignable)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!lock_);

  void AddField(mirror::Class* klass_ref, uint32_t dex_field_idx, bool is_static, ArtField* field)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!lock_);

  void AddMethod(mirror::Class* klass_ref,
                 uint32_t dex_method_idx,
                 MethodResolutionType resolution_type,
                 ArtMethod* method)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!lock_);

  const DexFile& dex_file_;

  Mutex lock_;
  std::map<ClassResolutionTuple, uint32_t> classes_ GUARDED_BY(lock_);
  std::map<FieldResolutionTuple, ArtField*> fields_ GUARDED_BY(lock_);
  std::map<MethodResolutionTuple, ArtMethod*> methods_ GUARDED_BY(lock_);
  std::map<AssignabilityTestTuple, bool> assignables_ GUARDED_BY(lock_);
};

}  // namespace verifier
}  // namespace art

#endif  // ART_RUNTIME_VERIFIER_VERIFIER_METADATA_H_
