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
  class ClassLoader;
  class DexCache;
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

  struct AssignabilityTestTuple : public std::tuple<std::string, std::string, bool> {
    using Base = std::tuple<std::string, std::string, bool>;
    using Base::Base;
    AssignabilityTestTuple(const AssignabilityTestTuple&) = default;

    const std::string& GetDestination() const { return std::get<0>(*this); }
    const std::string& GetSource() const { return std::get<1>(*this); }
    bool IsAssignable() const { return std::get<2>(*this); }
  };

  struct ClassResolutionTuple : public std::tuple<uint32_t, uint32_t> {
    using Base = std::tuple<uint32_t, uint32_t>;
    using Base::Base;
    ClassResolutionTuple(const ClassResolutionTuple&) = default;

    uint32_t GetDexTypeIndex() const { return std::get<0>(*this); }
    uint32_t GetModifiers() const { return std::get<1>(*this); }
    bool IsResolved() const { return GetModifiers() != kUnresolved; }
  };

  struct FieldResolutionTuple : public std::tuple<uint32_t, uint32_t> {
    using Base = std::tuple<uint32_t, uint32_t>;
    using Base::Base;
    FieldResolutionTuple(const FieldResolutionTuple&) = default;

    uint32_t GetDexFieldIndex() const { return std::get<0>(*this); }
    uint32_t GetModifiers() const { return std::get<1>(*this); }
    bool IsResolved() const { return GetModifiers() != kUnresolved; }
  };

  struct MethodResolutionTuple : public std::tuple<uint32_t, MethodResolutionType, uint32_t> {
    using Base = std::tuple<uint32_t, MethodResolutionType, uint32_t>;
    using Base::Base;
    MethodResolutionTuple(const MethodResolutionTuple&) = default;

    uint32_t GetDexMethodIndex() const { return std::get<0>(*this); }
    MethodResolutionType GetMethodResolutionType() const { return std::get<1>(*this); }
    uint32_t GetModifiers() const { return std::get<2>(*this); }
    bool IsResolved() const { return GetModifiers() != kUnresolved; }
  };

  const std::set<ClassResolutionTuple>& GetClasses() const { return classes_; }
  const std::set<FieldResolutionTuple>& GetFields() const { return fields_; }
  const std::set<MethodResolutionTuple>& GetMethods() const { return methods_; }
  const std::set<AssignabilityTestTuple>& GetAssignables() const { return assignables_; }

  void RecordAssignabilityTest(mirror::Class* destination,
                               mirror::Class* source,
                               bool is_strict,
                               bool expected_outcome)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!lock_);

  void RecordClassResolution(uint16_t dex_type_idx, mirror::Class* klass)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!lock_);

  void RecordFieldResolution(uint32_t dex_field_idx, ArtField* field)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!lock_);

  void RecordMethodResolution(uint32_t dex_method_idx,
                              MethodResolutionType resolution_type,
                              ArtMethod* method)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!lock_);

  bool Verify(Handle<mirror::ClassLoader> class_loader,
              Handle<mirror::DexCache> dex_cache,
              bool can_load_classes) const
      SHARED_REQUIRES(Locks::mutator_lock_);

  void Dump(std::ostream& os) const
      SHARED_REQUIRES(Locks::mutator_lock_);

  static constexpr uint32_t kUnresolved = static_cast<uint32_t>(-1);

 private:
  void AddAssignable(mirror::Class* destination, mirror::Class* source, bool is_assignable)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!lock_);

  const DexFile& dex_file_;

  Mutex lock_;
  std::set<ClassResolutionTuple> classes_ GUARDED_BY(lock_);
  std::set<FieldResolutionTuple> fields_ GUARDED_BY(lock_);
  std::set<MethodResolutionTuple> methods_ GUARDED_BY(lock_);
  std::set<AssignabilityTestTuple> assignables_ GUARDED_BY(lock_);
};

}  // namespace verifier
}  // namespace art

#endif  // ART_RUNTIME_VERIFIER_VERIFIER_METADATA_H_
