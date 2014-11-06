/*
 * Copyright (C) 2014 The Android Open Source Project
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

#include "instruction_set.h"

#include <gtest/gtest.h>

#ifdef HAVE_ANDROID_OS
#include "cutils/properties.h"
#endif

#include "base/stringprintf.h"

namespace art {

TEST(InstructionSetFeaturesTest, X86Features) {
  // Build features for a 32-bit x86 atom processor.
  std::string error_msg;
  std::unique_ptr<const InstructionSetFeatures> x86_features(
      InstructionSetFeatures::FromVariant(kX86, "atom", &error_msg));
  ASSERT_TRUE(x86_features.get() != nullptr) << error_msg;
  EXPECT_EQ(x86_features->GetInstructionSet(), kX86);
  EXPECT_TRUE(x86_features->Equals(x86_features.get()));
  EXPECT_STREQ("none", x86_features->GetFeatureString().c_str());
  EXPECT_EQ(x86_features->AsBitmap(), 0U);

  // Build features for a 32-bit x86 default processor.
  std::unique_ptr<const InstructionSetFeatures> x86_default_features(
      x86_features->AddFeaturesFromString("default", &error_msg));
  ASSERT_TRUE(x86_default_features.get() != nullptr) << error_msg;
  EXPECT_EQ(x86_default_features->GetInstructionSet(), kX86);
  EXPECT_TRUE(x86_default_features->Equals(x86_default_features.get()));
  EXPECT_STREQ("none", x86_default_features->GetFeatureString().c_str());
  EXPECT_EQ(x86_default_features->AsBitmap(), 0U);

  // Build features for a 64-bit x86-64 atom processor.
  std::unique_ptr<const InstructionSetFeatures> x86_64_features(
      InstructionSetFeatures::FromVariant(kX86_64, "atom", &error_msg));
  ASSERT_TRUE(x86_64_features.get() != nullptr) << error_msg;
  EXPECT_EQ(x86_64_features->GetInstructionSet(), kX86_64);
  EXPECT_TRUE(x86_64_features->Equals(x86_64_features.get()));
  EXPECT_STREQ("none", x86_64_features->GetFeatureString().c_str());
  EXPECT_EQ(x86_64_features->AsBitmap(), 0U);

  EXPECT_FALSE(x86_64_features->Equals(x86_features.get()));
  EXPECT_FALSE(x86_64_features->Equals(x86_default_features.get()));
  EXPECT_TRUE(x86_features->Equals(x86_default_features.get()));
}

#ifdef HAVE_ANDROID_OS
TEST(InstructionSetFeaturesTest, FeaturesFromSystemPropertyVariant) {
  // Take the default set of instruction features from the build.
  std::unique_ptr<const InstructionSetFeatures> instruction_set_features(
      InstructionSetFeatures::FromCppDefines());

  // Read the features property.
  std::string key = StringPrintf("dalvik.vm.isa.%s.variant", GetInstructionSetString(kRuntimeISA));
  char dex2oat_isa_variant[PROPERTY_VALUE_MAX];
  if (property_get(key.c_str(), dex2oat_isa_variant, nullptr) > 0) {
    // Use features from property to build InstructionSetFeatures and check against build's
    // features.
    std::string error_msg;
    std::unique_ptr<const InstructionSetFeatures> property_features(
        InstructionSetFeatures::FromVariant(kRuntimeISA, dex2oat_isa_variant, &error_msg));
    ASSERT_TRUE(property_features.get() != nullptr) << error_msg;

    EXPECT_TRUE(property_features->Equals(instruction_set_features.get()))
      << "System property features: " << *property_features.get()
      << "\nFeatures from build: " << *instruction_set_features.get();
  }
}

TEST(InstructionSetFeaturesTest, FeaturesFromSystemPropertyString) {
  // Take the default set of instruction features from the build.
  std::unique_ptr<const InstructionSetFeatures> instruction_set_features(
      InstructionSetFeatures::FromCppDefines());

  // Read the features property.
  std::string key = StringPrintf("dalvik.vm.isa.%s.features", GetInstructionSetString(kRuntimeISA));
  char dex2oat_isa_features[PROPERTY_VALUE_MAX];
  if (property_get(key.c_str(), dex2oat_isa_features, nullptr) > 0) {
    // Use features from property to build InstructionSetFeatures and check against build's
    // features.
    std::string error_msg;
    std::unique_ptr<const InstructionSetFeatures> base_features(
        InstructionSetFeatures::FromVariant(kRuntimeISA, "default", &error_msg));
    ASSERT_TRUE(base_features.get() != nullptr) << error_msg;

    std::unique_ptr<const InstructionSetFeatures> property_features(
        base_features->AddFeaturesFromString(dex2oat_isa_features, &error_msg));
    ASSERT_TRUE(property_features.get() != nullptr) << error_msg;

    EXPECT_TRUE(property_features->Equals(instruction_set_features.get()))
      << "System property features: " << *property_features.get()
      << "\nFeatures from build: " << *instruction_set_features.get();
  }
}
#endif

#if defined(__arm__)
TEST(InstructionSetFeaturesTest, DISABLED_FeaturesFromCpuInfo) {
  LOG(WARNING) << "Test disabled due to buggy ARM kernels";
#else
TEST(InstructionSetFeaturesTest, FeaturesFromCpuInfo) {
#endif
  // Take the default set of instruction features from the build.
  std::unique_ptr<const InstructionSetFeatures> instruction_set_features(
      InstructionSetFeatures::FromCppDefines());

  // Check we get the same instruction set features using /proc/cpuinfo.
  std::unique_ptr<const InstructionSetFeatures> cpuinfo_features(
      InstructionSetFeatures::FromCpuInfo());
  EXPECT_TRUE(cpuinfo_features->Equals(instruction_set_features.get()))
      << "CPU Info features: " << *cpuinfo_features.get()
      << "\nFeatures from build: " << *instruction_set_features.get();
}

#if defined(__arm__)
TEST(InstructionSetFeaturesTest, DISABLED_FeaturesFromHwcap) {
  LOG(WARNING) << "Test disabled due to buggy ARM kernels";
#else
TEST(InstructionSetFeaturesTest, FeaturesFromHwcap) {
#endif
  // Take the default set of instruction features from the build.
  std::unique_ptr<const InstructionSetFeatures> instruction_set_features(
      InstructionSetFeatures::FromCppDefines());

  // Check we get the same instruction set features using AT_HWCAP.
  std::unique_ptr<const InstructionSetFeatures> hwcap_features(
      InstructionSetFeatures::FromHwcap());
  EXPECT_TRUE(hwcap_features->Equals(instruction_set_features.get()))
      << "Hwcap features: " << *hwcap_features.get()
      << "\nFeatures from build: " << *instruction_set_features.get();
}


#if defined(__arm__)
TEST(InstructionSetFeaturesTest, DISABLED_FeaturesFromAssembly) {
  LOG(WARNING) << "Test disabled due to buggy ARM kernels";
#else
TEST(InstructionSetFeaturesTest, FeaturesFromAssembly) {
#endif
  // Take the default set of instruction features from the build.
  std::unique_ptr<const InstructionSetFeatures> instruction_set_features(
      InstructionSetFeatures::FromCppDefines());

  // Check we get the same instruction set features using assembly tests.
  std::unique_ptr<const InstructionSetFeatures> assembly_features(
      InstructionSetFeatures::FromAssembly());
  EXPECT_TRUE(assembly_features->Equals(instruction_set_features.get()))
      << "Assembly features: " << *assembly_features.get()
      << "\nFeatures from build: " << *instruction_set_features.get();
}

}  // namespace art
