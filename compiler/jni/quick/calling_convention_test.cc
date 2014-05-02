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

#include "arch/arm/method_frame_info_arm.h"
#include "arch/arm64/method_frame_info_arm64.h"
#include "arch/mips/method_frame_info_mips.h"
#include "arch/x86/method_frame_info_x86.h"
#include "arch/x86_64/method_frame_info_x86_64.h"
#include "gtest/gtest.h"
#include "jni/quick/calling_convention.h"
#include "UniquePtr.h"

namespace art {

class CallingConventionTest : public testing::Test {
 public:
  typedef MethodFrameInfo (*InfoFn)(bool is_static, const char* shorty);

  void CheckMethodFrameInfo(InstructionSet instruction_set, InfoFn info_fn) {
    static const char kReturnTypes[] = {
        'V', 'B', 'S', 'C', 'I', 'J', 'D', 'F', 'Z', 'L'
    };
    static const char* kParamsDefs[] = {
        "",
        "B", "BB", "BBB", "BBBB", "BBBBB", "BBBBBB",
        "S", "SS", "SSS", "SSSS", "SSSSS", "SSSSSS",
        "C", "CC", "CCC", "CCCC", "CCCCC", "CCCCCC",
        "I", "II", "III", "IIII", "IIIII", "IIIIII",
        "J", "JJ", "JJJ", "JJJJ", "JJJJJ", "JJJJJJ",
        "D", "DD", "DDD", "DDDD", "DDDDD", "DDDDDD",
        "F", "FF", "FFF", "FFFF", "FFFFF", "FFFFFF",
        "Z", "ZZ", "ZZZ", "ZZZZ", "ZZZZZ", "ZZZZZZ",
        "L", "LL", "LLL", "LLLL", "LLLLL", "LLLLLL",
        "IJ", "IJIJ", "IJIJIJ", "JI", "JIJI", "JIJIJI",
        "IL", "ILIL", "ILILIL", "LI", "LILI", "LILILI",
        "LJ", "LJLJ", "LJLJLJ", "JL", "JLJL", "JLJLJL",
        "IJL", "IJLIJL", "JLI", "JLIJLI", "LIJ", "LIJLIJ",
        "BSCIJDFZL", "LZFDJICSB", "JDFZLBSCI", "JICSBLZFD",
    };
    static bool kBools[] = { false, true };
    for (char return_type : kReturnTypes) {
      for (const char* params : kParamsDefs) {
        std::string shorty = std::string() + return_type + params;
        for (bool is_static : kBools) {
          MethodFrameInfo frame_info = info_fn(is_static, shorty.c_str());
          for (bool is_synchronized : kBools) {
            UniquePtr<JniCallingConvention> calling_convention(JniCallingConvention::Create(
                is_static, is_synchronized, shorty.c_str(), instruction_set));
            EXPECT_EQ(frame_info.FrameSizeInBytes(), calling_convention->FrameSize());
            EXPECT_EQ(frame_info.CoreSpillMask(), calling_convention->CoreSpillMask());
            EXPECT_EQ(frame_info.FpSpillMask(), calling_convention->FpSpillMask());
          }
        }
      }
    }
  }
};

TEST_F(CallingConventionTest, ARM) {
  CheckMethodFrameInfo(kArm, &arm::ArmJniMethodFrameInfo);
}

TEST_F(CallingConventionTest, ARM64) {
  CheckMethodFrameInfo(kArm64, &arm64::Arm64JniMethodFrameInfo);
}

TEST_F(CallingConventionTest, MIPS) {
  CheckMethodFrameInfo(kMips, &mips::MipsJniMethodFrameInfo);
}

TEST_F(CallingConventionTest, X86) {
  CheckMethodFrameInfo(kX86, &x86::X86JniMethodFrameInfo);
}

TEST_F(CallingConventionTest, X86_64) {
  CheckMethodFrameInfo(kX86_64, &x86_64::X86_64JniMethodFrameInfo);
}

}  // namespace art
