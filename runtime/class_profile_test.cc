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

#include "common_runtime_test.h"

#include "class_linker-inl.h"
#include "class_profile.h"
#include "scoped_thread_state_change.h"

namespace art {

class ClassProfileTest : public CommonRuntimeTest {};

TEST_F(ClassProfileTest, DexCacheProfileDataConstructor) {
  static constexpr uint32_t kChecksum = 4321;
  static constexpr uint32_t kCount = 1234;
  DexCacheProfileData profile1;
  DexCacheProfileData profile2(kChecksum, 0);
  DexCacheProfileData profile3(kChecksum, kCount);
  EXPECT_EQ(profile3.NumClassDefs(), kCount);
  DexCacheProfileData profile4(std::move(profile3));
  EXPECT_EQ(profile4.NumClassDefs(), kCount);
}

TEST_F(ClassProfileTest, DexCacheProfileDataUpdate) {
  // Get some dex cache.
  ScopedObjectAccess soa(Thread::Current());
  StackHandleScope<32> hs(soa.Self());
  ClassLinker* const class_linker = Runtime::Current()->GetClassLinker();
  Handle<mirror::Class> klass(
      hs.NewHandle(class_linker->FindSystemClass(soa.Self(), "Ljava/lang/Class;")));
  ASSERT_TRUE(klass.Get() != nullptr);
  Handle<mirror::DexCache> dex_cache(hs.NewHandle(klass->GetDexCache()));
  ASSERT_TRUE(dex_cache.Get() != nullptr);
  const size_t num_class_defs = dex_cache->GetDexFile()->NumClassDefs();
  DexCacheProfileData profile(*dex_cache->GetDexFile());
  profile.Update(dex_cache.Get());
  size_t old_resolved_count = 0;
  for (size_t i = 0; i < num_class_defs; ++i) {
    old_resolved_count += profile.IsResolved(i);
  }
  const size_t class_def_idx = klass->GetDexClassDefIndex();
  ASSERT_LT(class_def_idx, num_class_defs);
  EXPECT_TRUE(profile.IsResolved(class_def_idx));
  for (size_t i = 0; i < dex_cache->NumResolvedTypes(); ++i) {
    mirror::Class* type = dex_cache->GetResolvedType(i);
    if (type != nullptr && type->GetDexCache() == dex_cache.Get()) {
      EXPECT_TRUE(type->IsResolved());
      EXPECT_TRUE(profile.IsResolved(type->GetDexClassDefIndex()));
    }
  }
  // Force resolve all of the types, update the profile, then verify.
  for (size_t i = 0; i < dex_cache->NumResolvedTypes(); ++i) {
    ScopedNullHandle<mirror::ClassLoader> class_loader;
    const DexFile& dex_file = *dex_cache->GetDexFile();
    mirror::Class* type = class_linker->ResolveType(dex_file, i, dex_cache, class_loader);
    soa.Self()->AssertNoPendingException();
    EXPECT_TRUE(type != nullptr);
    EXPECT_EQ(dex_cache->GetResolvedType(i), type);
  }
  // Update profile now that all the types should be resolved.
  profile.Update(dex_cache.Get());
  size_t new_resolved_count = 0;
  for (size_t i = 0; i < num_class_defs; ++i) {
    new_resolved_count += profile.IsResolved(i);
  }
  // Everything should not have already been resolved until we resolved each type.
  EXPECT_GT(new_resolved_count, old_resolved_count);
  // Every class def should be resolved after we resolved each type.
  EXPECT_EQ(new_resolved_count, num_class_defs);
}

TEST_F(ClassProfileTest, DexCacheProfileReadWrite) {
  ScopedObjectAccess soa(Thread::Current());
  StackHandleScope<32> hs(soa.Self());
  ClassLinker* const class_linker = Runtime::Current()->GetClassLinker();
  Handle<mirror::Class> klass(
      hs.NewHandle(class_linker->FindSystemClass(soa.Self(), "Ljava/lang/Class;")));
  ASSERT_TRUE(klass.Get() != nullptr);
  Handle<mirror::DexCache> dex_cache(hs.NewHandle(klass->GetDexCache()));
  ASSERT_TRUE(dex_cache.Get() != nullptr);
  const size_t num_class_defs = dex_cache->GetDexFile()->NumClassDefs();
  DexCacheProfileData profile(*dex_cache->GetDexFile());
  profile.Update(dex_cache.Get());
  std::vector<uint8_t> data;
  size_t count = profile.WriteToVector(&data);
  EXPECT_EQ(count, data.size());
  std::unique_ptr<DexCacheProfileData> read_profile;
  // Test reading invalid sizes;
  for (size_t i = 0; i < count; ++i) {
    const uint8_t* in = &data[0];
    size_t in_size = i;
    std::string error_msg;
    read_profile.reset(DexCacheProfileData::ReadFromMemory(&in, &in_size, &error_msg));
    EXPECT_TRUE(read_profile.get() == nullptr);
    EXPECT_NE(error_msg.length(), 0u);
  }
  // Test a valid read.
  const uint8_t* in = &data[0];
  size_t in_size = data.size();
  std::string error_msg;
  read_profile.reset(DexCacheProfileData::ReadFromMemory(&in, &in_size, &error_msg));
  EXPECT_STREQ(error_msg.c_str(), "");
  ASSERT_TRUE(read_profile.get() != nullptr);
  EXPECT_EQ(read_profile->NumClassDefs(), profile.NumClassDefs());
  for (size_t i = 0; i < num_class_defs; ++i) {
    EXPECT_EQ(read_profile->IsResolved(i), profile.IsResolved(i));
  }
}

TEST_F(ClassProfileTest, DexCacheGetClassDescriptors) {
  ScopedObjectAccess soa(Thread::Current());
  StackHandleScope<32> hs(soa.Self());
  ClassLinker* const class_linker = Runtime::Current()->GetClassLinker();
  constexpr const char* kLookupDescriptor = "Ljava/lang/Class;";
  Handle<mirror::Class> klass(
      hs.NewHandle(class_linker->FindSystemClass(soa.Self(), kLookupDescriptor)));
  ASSERT_TRUE(klass.Get() != nullptr);
  Handle<mirror::DexCache> dex_cache(hs.NewHandle(klass->GetDexCache()));
  DexCacheProfileData profile(*dex_cache->GetDexFile());
  profile.Update(dex_cache.Get());
  std::unordered_set<std::string> descriptors =
      profile.GetClassDescriptors(*dex_cache->GetDexFile());
  ASSERT_TRUE(!descriptors.empty());
  EXPECT_TRUE(descriptors.find(kLookupDescriptor) != descriptors.end());
}


TEST_F(ClassProfileTest, CollectGetClassDescriptors) {
  ScopedObjectAccess soa(Thread::Current());
  StackHandleScope<32> hs(soa.Self());
  constexpr const char* kLookupDescriptor = "Ljava/lang/Class;";
  ClassLinker* const class_linker = Runtime::Current()->GetClassLinker();
  Handle<mirror::Class> klass(
      hs.NewHandle(class_linker->FindSystemClass(soa.Self(), kLookupDescriptor)));
  ClassProfile profile;
  profile.Collect();
  std::unordered_set<std::string> descriptors = profile.GetClassDescriptors();
  ASSERT_TRUE(!descriptors.empty());
  EXPECT_TRUE(descriptors.find(kLookupDescriptor) != descriptors.end());
}

TEST_F(ClassProfileTest, Dump) {
  // Only check that there is something dumped. Do not check contents.
  ClassProfile profile;
  profile.Collect();
  profile.Dump(LOG(INFO));
}

}  // namespace art
