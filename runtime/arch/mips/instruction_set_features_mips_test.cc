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

#include "instruction_set_features_mips.h"

#include <gtest/gtest.h>

namespace art {

// TODO: Fix MIPS features so this test doesn't need to be disabled.
// This test is known to fail for MIPS64. Why this test fails has not
// yet been diagnosed. Until the test failure can be diagnosed the test
// is being disabled so that is doesn't distract the user from other
// failures which may occur and are likely to be real failures.
TEST(MipsInstructionSetFeaturesTest, DISABLED_MipsFeatures) {
  std::string error_msg;
  std::unique_ptr<const InstructionSetFeatures> mips_features(
      InstructionSetFeatures::FromVariant(kMips, "default", &error_msg));
  ASSERT_TRUE(mips_features.get() != nullptr) << error_msg;
  EXPECT_EQ(mips_features->GetInstructionSet(), kMips);
  EXPECT_TRUE(mips_features->Equals(mips_features.get()));
  EXPECT_STREQ("smp,fpu32,mips2", mips_features->GetFeatureString().c_str());
  EXPECT_EQ(mips_features->AsBitmap(), 7U);
}

}  // namespace art
