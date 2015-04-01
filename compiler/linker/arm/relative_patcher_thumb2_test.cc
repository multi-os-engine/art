/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include "linker/relative_patcher_test.h"
#include "linker/arm/relative_patcher_thumb2.h"

namespace art {
namespace linker {

class Thumb2RelativePatcherTest : public RelativePatcherTest {
 public:
  Thumb2RelativePatcherTest() : RelativePatcherTest(kThumb2, "default") { }

 protected:
  static const uint8_t kCallRawCode[];
  static const ArrayRef<const uint8_t> kCallCode;

  bool Create2MethodsWithGap(const ArrayRef<const uint8_t>& method0_code,
                             const ArrayRef<LinkerPatch>& method0_patches,
                             const ArrayRef<const uint8_t>& method2_code,
                             const ArrayRef<LinkerPatch>& method2_patches,
                             uint32_t distance_without_thunks) {
    CHECK_EQ(distance_without_thunks % kArmAlignment, 0u);
    const uint32_t method0_offset =
        CompiledCode::AlignCode(kTrampolineSize, kThumb2) + sizeof(OatQuickMethodHeader);
    AddCompiledMethod(MethodRef(0u), method0_code, ArrayRef<LinkerPatch>(method0_patches));

    // We want to put the method2 at a very precise offset.
    const uint32_t method2_offset = method0_offset + distance_without_thunks;
    CHECK(IsAligned<kArmAlignment>(method2_offset - sizeof(OatQuickMethodHeader)));

    // Calculate size of method1 so that we put method2 at the correct place.
    const uint32_t method1_offset =
        CompiledCode::AlignCode(method0_offset + method0_code.size(), kThumb2) +
        sizeof(OatQuickMethodHeader);
    const uint32_t method1_size = (method2_offset - sizeof(OatQuickMethodHeader) - method1_offset);
    std::vector<uint8_t> method1_raw_code(method1_size);
    ArrayRef<const uint8_t> method1_code(method1_raw_code);
    AddCompiledMethod(MethodRef(1u), method1_code, ArrayRef<LinkerPatch>());

    AddCompiledMethod(MethodRef(2u), method2_code, method2_patches);

    Link();

    // Check assumptions.
    auto result0 = method_offset_map_.FindMethodOffset(MethodRef(0));
    CHECK(result0.first);
    CHECK_EQ(result0.second, method0_offset + 1 /* thumb mode */);
    auto result1 = method_offset_map_.FindMethodOffset(MethodRef(0));
    CHECK(result1.first);
    CHECK_EQ(result1.second, method0_offset + 1 /* thumb mode */);
    auto result2 = method_offset_map_.FindMethodOffset(MethodRef(2));
    CHECK(result2.first);
    // There may be a thunk before method2.
    if (result2.second == method2_offset + 1 /* thumb mode */) {
      return false;  // No thunk.
    } else {
      uint32_t aligned_thunk_size = CompiledCode::AlignCode(ThunkSize(), kThumb2);
      CHECK_EQ(result2.second, method2_offset + aligned_thunk_size + 1 /* thumb mode */);
      return true;   // Thunk present.
    }
  }

  uint32_t ThunkSize() {
    return static_cast<Thumb2RelativePatcher*>(patcher_.get())->thunk_code_.size();
  }

  bool CheckThunk(uint32_t thunk_offset) {
    Thumb2RelativePatcher* patcher = static_cast<Thumb2RelativePatcher*>(patcher_.get());
    ArrayRef<const uint8_t> expected_code(patcher->thunk_code_);
    if (output_.size() < thunk_offset + expected_code.size()) {
      LOG(ERROR) << "output_.size() == " << output_.size() << " < "
          << "thunk_offset + expected_code.size() == " << (thunk_offset + expected_code.size());
      return false;
    }
    ArrayRef<const uint8_t> linked_code(&output_[thunk_offset], expected_code.size());
    if (linked_code == expected_code) {
      return true;
    }
    // Log failure info.
    DumpDiff(expected_code, linked_code);
    return false;
  }
};

const uint8_t Thumb2RelativePatcherTest::kCallRawCode[] = {
    0x00, 0xf0, 0x00, 0xf8
};

const ArrayRef<const uint8_t> Thumb2RelativePatcherTest::kCallCode(kCallRawCode);

TEST_F(Thumb2RelativePatcherTest, CallSelf) {
  LinkerPatch patches[] = {
      LinkerPatch::RelativeCodePatch(0u, nullptr, 0u),
  };
  AddCompiledMethod(MethodRef(0u), kCallCode, ArrayRef<LinkerPatch>(patches));
  Link();

  static const uint8_t expected_code[] = {
      0xff, 0xf7, 0xfe, 0xff
  };
  EXPECT_TRUE(CheckLinkedMethod(MethodRef(0), ArrayRef<const uint8_t>(expected_code)));
}

TEST_F(Thumb2RelativePatcherTest, CallOther) {
  // TODO: Rewrite by actually providing code for method1 and calling back.
  LinkerPatch method0_patches[] = {
      LinkerPatch::RelativeCodePatch(0u, nullptr, 1u),
  };
  AddCompiledMethod(MethodRef(0u), kCallCode, ArrayRef<LinkerPatch>(method0_patches));
  LinkerPatch method1_patches[] = {
      LinkerPatch::RelativeCodePatch(0u, nullptr, 0u),
  };
  AddCompiledMethod(MethodRef(1u), kCallCode, ArrayRef<LinkerPatch>(method1_patches));
  Link();

  auto result0 = method_offset_map_.FindMethodOffset(MethodRef(0));
  ASSERT_TRUE(result0.first);
  ASSERT_NE(result0.second & 1u, 0u);
  uint32_t method0_offset = result0.second - 1u /* thumb mode */;
  auto result1 = method_offset_map_.FindMethodOffset(MethodRef(1));
  ASSERT_TRUE(result1.first);
  ASSERT_NE(result1.second & 1u, 0u);
  uint32_t method1_offset = result1.second - 1u /* thumb mode */;
  uint32_t diff_after = method1_offset - (method0_offset + 4u /* PC adjustment */);
  ASSERT_EQ(diff_after & 1u, 0u);
  ASSERT_LT(diff_after >> 1, 1u << 8);  // Simple encoding, (diff_after >> 1) fits into 8 bits.
  static const uint8_t method0_expected_code[] = {
      0x00, 0xf0, static_cast<uint8_t>(diff_after >> 1), 0xf8
  };
  EXPECT_TRUE(CheckLinkedMethod(MethodRef(0), ArrayRef<const uint8_t>(method0_expected_code)));
  uint32_t diff_before = method0_offset - (method1_offset + 4u /* PC adjustment */);
  ASSERT_EQ(diff_before & 1u, 0u);
  ASSERT_GE(diff_before, -1u << 9);  // Simple encoding, -256 <= (diff >> 1) < 0.
  const uint8_t method1_expected_code[] = {
      0xff, 0xf7, static_cast<uint8_t>(diff_before >> 1), 0xff
  };
  EXPECT_TRUE(CheckLinkedMethod(MethodRef(1), ArrayRef<const uint8_t>(method1_expected_code)));
}

TEST_F(Thumb2RelativePatcherTest, CallTrampoline) {
  LinkerPatch patches[] = {
      LinkerPatch::RelativeCodePatch(0u, nullptr, 1u),
  };
  AddCompiledMethod(MethodRef(0u), kCallCode, ArrayRef<LinkerPatch>(patches));
  Link();

  auto result = method_offset_map_.FindMethodOffset(MethodRef(0));
  ASSERT_TRUE(result.first);
  ASSERT_NE(result.second & 1u, 0u);
  uint32_t diff = kTrampolineOffset + 1 /* thumb mode */ - (result.second + 4u);
  ASSERT_EQ(diff & 1u, 0u);
  ASSERT_GE(diff, -1u << 9);  // Simple encoding, -256 <= (diff >> 1) < 0 (checked as unsigned).
  const uint8_t expected_code[] = {
      0xff, 0xf7, static_cast<uint8_t>(diff >> 1), 0xff
  };
  EXPECT_TRUE(CheckLinkedMethod(MethodRef(0), ArrayRef<const uint8_t>(expected_code)));
}

TEST_F(Thumb2RelativePatcherTest, CallOtherAlmostTooFarAfter) {
  static const uint8_t method0_raw_code[] = {
      0x00, 0xbf, 0x00, 0xbf, 0x00, 0xbf,  // NOP, NOP, NOP.
      0x00, 0xf0, 0x00, 0xf8
  };
  ArrayRef<const uint8_t> method0_code(method0_raw_code);
  constexpr uint32_t bl_offset_in_method0 = 6u;
  static_assert(bl_offset_in_method0 + 4u == sizeof(method0_raw_code), "BL offset check.");
  LinkerPatch method0_patches[] = {
      LinkerPatch::RelativeCodePatch(bl_offset_in_method0, nullptr, 2u),
  };
  static const uint8_t method2_raw_code[] = {
      0x00, 0xbf  // NOP
  };
  ArrayRef<const uint8_t> method2_code(method2_raw_code);

  constexpr uint32_t max_positive_disp = 16 * MB - 2u + 4u /* PC adjustment */;
  bool thunk_in_gap = Create2MethodsWithGap(method0_code, method0_patches,
                                            method2_code, ArrayRef<LinkerPatch>(),
                                            bl_offset_in_method0 + max_positive_disp);
  ASSERT_FALSE(thunk_in_gap);  // There should be no thunk.

  // Check linked code.
  static const uint8_t expected_code[] = {
      0x00, 0xbf, 0x00, 0xbf, 0x00, 0xbf,  // NOP, NOP, NOP.
      0xff, 0xf3, 0xff, 0xd7
  };
  EXPECT_TRUE(CheckLinkedMethod(MethodRef(0), ArrayRef<const uint8_t>(expected_code)));
}

TEST_F(Thumb2RelativePatcherTest, CallOtherAlmostTooFarBefore) {
  static const uint8_t method0_raw_code[] = {
      0x00, 0xbf  // NOP
  };
  ArrayRef<const uint8_t> method0_code(method0_raw_code);
  static const uint8_t method2_raw_code[] = {
      0x00, 0xbf, 0x00, 0xbf,  // NOP, NOP.
      0x00, 0xf0, 0x00, 0xf8
  };
  constexpr uint32_t bl_offset_in_method2 = 4u;
  static_assert(bl_offset_in_method2 + 4u == sizeof(method2_raw_code), "BL offset check.");
  LinkerPatch method2_patches[] = {
      LinkerPatch::RelativeCodePatch(bl_offset_in_method2, nullptr, 0u),
  };
  ArrayRef<const uint8_t> method2_code(method2_raw_code);

  constexpr uint32_t max_negative_disp = 16 * MB - 4u /* PC adjustment */;
  bool thunk_in_gap = Create2MethodsWithGap(method0_code, ArrayRef<LinkerPatch>(),
                                            method2_code, method2_patches,
                                            max_negative_disp - bl_offset_in_method2);
  ASSERT_FALSE(thunk_in_gap);  // There should be no thunk.

  // Check linked code.
  static const uint8_t expected_code[] = {
      0x00, 0xbf, 0x00, 0xbf,  // NOP, NOP.
      0x00, 0xf4, 0x00, 0xd0
  };
  EXPECT_TRUE(CheckLinkedMethod(MethodRef(2), ArrayRef<const uint8_t>(expected_code)));
}

TEST_F(Thumb2RelativePatcherTest, CallOtherJustTooFarAfter) {
  static const uint8_t method0_raw_code[] = {
      0x00, 0xbf, 0x00, 0xbf,  // NOP, NOP.
      0x00, 0xf0, 0x00, 0xf8
  };
  ArrayRef<const uint8_t> method0_code(method0_raw_code);
  constexpr uint32_t bl_offset_in_method0 = 4u;
  static_assert(bl_offset_in_method0 + 4u == sizeof(method0_raw_code), "BL offset check.");
  LinkerPatch method0_patches[] = {
      LinkerPatch::RelativeCodePatch(bl_offset_in_method0, nullptr, 2u),
  };
  static const uint8_t method2_raw_code[] = {
      0x00, 0xbf  // NOP
  };
  ArrayRef<const uint8_t> method2_code(method2_raw_code);

  constexpr uint32_t max_positive_disp = 16 * MB + 4u /* PC adjustment */;
  bool thunk_in_gap = Create2MethodsWithGap(method0_code, method0_patches,
                                            method2_code, ArrayRef<LinkerPatch>(),
                                            bl_offset_in_method0 + max_positive_disp);
  ASSERT_TRUE(thunk_in_gap);

  auto result0 = method_offset_map_.FindMethodOffset(MethodRef(0));
  uint32_t method0_offset = result0.second - 1 /* thumb mode */;
  auto result2 = method_offset_map_.FindMethodOffset(MethodRef(2));
  uint32_t method2_offset = result2.second - 1 /* thumb mode */;
  uint32_t method2_header_offset = method2_offset - sizeof(OatQuickMethodHeader);
  ASSERT_TRUE(IsAligned<kArmAlignment>(method2_header_offset));
  uint32_t thunk_offset = method2_header_offset - CompiledCode::AlignCode(ThunkSize(), kThumb2);
  ASSERT_TRUE(IsAligned<kArmAlignment>(thunk_offset));
  uint32_t diff = thunk_offset - (method0_offset + bl_offset_in_method0 + 4u /* PC adjustment */);
  ASSERT_EQ(diff & 1u, 0u);
  ASSERT_GE(diff, 16 * MB - (1u << 9));  // Simple encoding, unknown bits fit into the low 8 bits.
  static const uint8_t expected_code[] = {
      0x00, 0xbf, 0x00, 0xbf,  // NOP, NOP.
      0xff, 0xf3, static_cast<uint8_t>(diff >> 1), 0xd7
  };
  EXPECT_TRUE(CheckLinkedMethod(MethodRef(0), ArrayRef<const uint8_t>(expected_code)));
  CheckThunk(thunk_offset);
}

TEST_F(Thumb2RelativePatcherTest, CallOtherJustTooFarBefore) {
  static const uint8_t method0_raw_code[] = {
      0x00, 0xbf  // NOP
  };
  ArrayRef<const uint8_t> method0_code(method0_raw_code);
  static const uint8_t method2_raw_code[] = {
      0x00, 0xbf, 0x00, 0xbf, 0x00, 0xbf,  // NOP, NOP, NOP.
      0x00, 0xf0, 0x00, 0xf8
  };
  constexpr uint32_t bl_offset_in_method2 = 6u;
  static_assert(bl_offset_in_method2 + 4u == sizeof(method2_raw_code), "BL offset check.");
  LinkerPatch method2_patches[] = {
      LinkerPatch::RelativeCodePatch(bl_offset_in_method2, nullptr, 0u),
  };
  ArrayRef<const uint8_t> method2_code(method2_raw_code);

  constexpr uint32_t over_max_negative_disp = 16 * MB + 2 - 4u /* PC adjustment */;
  bool thunk_in_gap = Create2MethodsWithGap(method0_code, ArrayRef<LinkerPatch>(),
                                            method2_code, method2_patches,
                                            over_max_negative_disp - bl_offset_in_method2);
  ASSERT_FALSE(thunk_in_gap);  // There should be a thunk but it should be after the method2.

  // Check linked code.
  auto result2 = method_offset_map_.FindMethodOffset(MethodRef(2));
  CHECK(result2.first);
  uint32_t method2_offset = result2.second - 1 /* thumb mode */;
  uint32_t thunk_offset = CompiledCode::AlignCode(method2_offset + method2_code.size(), kThumb2);
  uint32_t diff = thunk_offset - (method2_offset + bl_offset_in_method2 + 4u /* PC adjustment */);
  ASSERT_EQ(diff & 1u, 0u);
  ASSERT_LT(diff >> 1, 1u << 8);  // Simple encoding, (diff >> 1) fits into 8 bits.
  static const uint8_t expected_code[] = {
      0x00, 0xbf, 0x00, 0xbf, 0x00, 0xbf,  // NOP, NOP, NOP.
      0x00, 0xf0, static_cast<uint8_t>(diff >> 1), 0xf8
  };
  EXPECT_TRUE(CheckLinkedMethod(MethodRef(2), ArrayRef<const uint8_t>(expected_code)));
  EXPECT_TRUE(CheckThunk(thunk_offset));
}

}  // namespace linker
}  // namespace art
