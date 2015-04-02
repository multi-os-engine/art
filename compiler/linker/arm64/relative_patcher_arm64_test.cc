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
#include "linker/arm64/relative_patcher_arm64.h"

namespace art {
namespace linker {

class Arm64RelativePatcherTest : public RelativePatcherTest {
 public:
  explicit Arm64RelativePatcherTest(const std::string& variant)
      : RelativePatcherTest(kArm64, variant) { }

 protected:
  static const uint8_t kCallRawCode[];
  static const ArrayRef<const uint8_t> kCallCode;
  static const uint8_t kNopRawCode[];
  static const ArrayRef<const uint8_t> kNopCode;

  // All branches can be created from kBlPlus0 by adding the low 26 bits.
  static constexpr uint32_t kBlPlus0 = 0x94000000u;

  // Special BL values.
  static constexpr uint32_t kBlPlusMax = 0x95ffffffu;
  static constexpr uint32_t kBlMinusMax = 0x96000000u;

  uint32_t Create2MethodsWithGap(const ArrayRef<const uint8_t>& method1_code,
                                 const ArrayRef<LinkerPatch>& method1_patches,
                                 const ArrayRef<const uint8_t>& last_method_code,
                                 const ArrayRef<LinkerPatch>& last_method_patches,
                                 uint32_t distance_without_thunks) {
    CHECK_EQ(distance_without_thunks % kArm64Alignment, 0u);
    const uint32_t method1_offset =
        CompiledCode::AlignCode(kTrampolineSize, kArm64) + sizeof(OatQuickMethodHeader);
    AddCompiledMethod(MethodRef(1u), method1_code, ArrayRef<LinkerPatch>(method1_patches));
    const uint32_t gap_start =
        CompiledCode::AlignCode(method1_offset + method1_code.size(), kArm64);

    // We want to put the method3 at a very precise offset.
    const uint32_t last_method_offset = method1_offset + distance_without_thunks;
    const uint32_t gap_end = last_method_offset - sizeof(OatQuickMethodHeader);
    CHECK(IsAligned<kArm64Alignment>(gap_end));

    // Fill the gap with intermediate methods in chunks of 2MiB and the last in [2MiB, 4MiB).
    // (This allows deduplicating the small chunks to avoid using 256MiB of memory for +-128MiB
    // offsets by this test.)
    uint32_t method_idx = 2u;
    constexpr uint32_t kSmallChunkSize = 2 * MB;
    std::vector<uint8_t> gap_code;
    size_t gap_size = gap_end - gap_start;
    for (; gap_size >= 2u * kSmallChunkSize; gap_size -= kSmallChunkSize) {
      uint32_t chunk_code_size = kSmallChunkSize - sizeof(OatQuickMethodHeader);
      gap_code.resize(chunk_code_size, 0u);
      AddCompiledMethod(MethodRef(method_idx), ArrayRef<const uint8_t>(gap_code),
                        ArrayRef<LinkerPatch>());
      method_idx += 1u;
    }
    uint32_t chunk_code_size = gap_size - sizeof(OatQuickMethodHeader);
    gap_code.resize(chunk_code_size, 0u);
    AddCompiledMethod(MethodRef(method_idx), ArrayRef<const uint8_t>(gap_code),
                      ArrayRef<LinkerPatch>());
    method_idx += 1u;

    // Add the last method and link
    AddCompiledMethod(MethodRef(method_idx), last_method_code, last_method_patches);
    Link();

    // Check assumptions.
    CHECK_EQ(GetMethodOffset(1), method1_offset);
    auto last_result = method_offset_map_.FindMethodOffset(MethodRef(method_idx));
    CHECK(last_result.first);
    // There may be a thunk before method2.
    if (last_result.second != last_method_offset) {
      // Thunk present. Check that there's only one.
      uint32_t aligned_thunk_size = CompiledCode::AlignCode(ThunkSize(), kArm64);
      CHECK_EQ(last_result.second, last_method_offset + aligned_thunk_size);
    }
    return method_idx;
  }

  uint32_t GetMethodOffset(uint32_t method_idx) {
    auto result = method_offset_map_.FindMethodOffset(MethodRef(method_idx));
    CHECK(result.first);
    CHECK_EQ(result.second & 3u, 0u);
    return result.second;
  }

  uint32_t ThunkSize() {
    return static_cast<Arm64RelativePatcher*>(patcher_.get())->thunk_code_.size();
  }

  bool CheckThunk(uint32_t thunk_offset) {
    Arm64RelativePatcher* patcher = static_cast<Arm64RelativePatcher*>(patcher_.get());
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

  std::vector<uint8_t> GenNopsAndBl(size_t num_nops, uint32_t bl) {
    std::vector<uint8_t> result;
    result.reserve(num_nops * 4u + 4u);
    for (size_t i = 0; i != num_nops; ++i) {
      result.insert(result.end(), kNopCode.begin(), kNopCode.end());
    }
    result.push_back(static_cast<uint8_t>(bl));
    result.push_back(static_cast<uint8_t>(bl >> 8));
    result.push_back(static_cast<uint8_t>(bl >> 16));
    result.push_back(static_cast<uint8_t>(bl >> 24));
    return result;
  }
};

const uint8_t Arm64RelativePatcherTest::kCallRawCode[] = {
    0x00, 0x00, 0x00, 0x94
};

const ArrayRef<const uint8_t> Arm64RelativePatcherTest::kCallCode(kCallRawCode);

const uint8_t Arm64RelativePatcherTest::kNopRawCode[] = {
    0x1f, 0x20, 0x03, 0xd5
};

const ArrayRef<const uint8_t> Arm64RelativePatcherTest::kNopCode(kNopRawCode);

class Arm64RelativePatcherTestDefault : public Arm64RelativePatcherTest {
 public:
  Arm64RelativePatcherTestDefault() : Arm64RelativePatcherTest("default") { }
};

class Arm64RelativePatcherTestDenver64 : public Arm64RelativePatcherTest {
 public:
  Arm64RelativePatcherTestDenver64() : Arm64RelativePatcherTest("denver64") { }
};

TEST_F(Arm64RelativePatcherTestDefault, CallSelf) {
  LinkerPatch patches[] = {
      LinkerPatch::RelativeCodePatch(0u, nullptr, 1u),
  };
  AddCompiledMethod(MethodRef(1u), kCallCode, ArrayRef<LinkerPatch>(patches));
  Link();

  static const uint8_t expected_code[] = {
      0x00, 0x00, 0x00, 0x94
  };
  EXPECT_TRUE(CheckLinkedMethod(MethodRef(1u), ArrayRef<const uint8_t>(expected_code)));
}

TEST_F(Arm64RelativePatcherTestDefault, CallOther) {
  LinkerPatch method1_patches[] = {
      LinkerPatch::RelativeCodePatch(0u, nullptr, 2u),
  };
  AddCompiledMethod(MethodRef(1u), kCallCode, ArrayRef<LinkerPatch>(method1_patches));
  LinkerPatch method2_patches[] = {
      LinkerPatch::RelativeCodePatch(0u, nullptr, 1u),
  };
  AddCompiledMethod(MethodRef(2u), kCallCode, ArrayRef<LinkerPatch>(method2_patches));
  Link();

  uint32_t method1_offset = GetMethodOffset(1u);
  uint32_t method2_offset = GetMethodOffset(2u);
  uint32_t diff_after = method2_offset - method1_offset;
  ASSERT_EQ(diff_after & 3u, 0u);
  ASSERT_LT(diff_after >> 2, 1u << 8);  // Simple encoding, (diff_after >> 2) fits into 8 bits.
  static const uint8_t method1_expected_code[] = {
      static_cast<uint8_t>(diff_after >> 2), 0x00, 0x00, 0x94
  };
  EXPECT_TRUE(CheckLinkedMethod(MethodRef(1u), ArrayRef<const uint8_t>(method1_expected_code)));
  uint32_t diff_before = method1_offset - method2_offset;
  ASSERT_EQ(diff_before & 3u, 0u);
  ASSERT_GE(diff_before, -1u << 27);
  auto method2_expected_code = GenNopsAndBl(0u, kBlPlus0 | ((diff_before >> 2) & 0x03ffffffu));
  EXPECT_TRUE(CheckLinkedMethod(MethodRef(2u), ArrayRef<const uint8_t>(method2_expected_code)));
}

TEST_F(Arm64RelativePatcherTestDefault, CallTrampoline) {
  LinkerPatch patches[] = {
      LinkerPatch::RelativeCodePatch(0u, nullptr, 2u),
  };
  AddCompiledMethod(MethodRef(1u), kCallCode, ArrayRef<LinkerPatch>(patches));
  Link();

  uint32_t method1_offset = GetMethodOffset(1u);
  uint32_t diff = kTrampolineOffset - method1_offset;
  ASSERT_EQ(diff & 1u, 0u);
  ASSERT_GE(diff, -1u << 9);  // Simple encoding, -256 <= (diff >> 1) < 0 (checked as unsigned).
  auto expected_code = GenNopsAndBl(0u, kBlPlus0 | ((diff >> 2) & 0x03ffffffu));
  EXPECT_TRUE(CheckLinkedMethod(MethodRef(1u), ArrayRef<const uint8_t>(expected_code)));
}

TEST_F(Arm64RelativePatcherTestDefault, CallOtherAlmostTooFarAfter) {
  auto method1_raw_code = GenNopsAndBl(1u, kBlPlus0);
  constexpr uint32_t bl_offset_in_method1 = 1u * 4u;  // After NOPs.
  ArrayRef<const uint8_t> method1_code(method1_raw_code);
  ASSERT_EQ(bl_offset_in_method1 + 4u, method1_code.size());
  uint32_t expected_last_method_idx = 65;  // Based on 2MiB chunks in Create2MethodsWithGap().
  LinkerPatch method1_patches[] = {
      LinkerPatch::RelativeCodePatch(bl_offset_in_method1, nullptr, expected_last_method_idx),
  };

  constexpr uint32_t max_positive_disp = 128 * MB - 4u;
  uint32_t last_method_idx = Create2MethodsWithGap(method1_code, method1_patches,
                                                   kNopCode, ArrayRef<LinkerPatch>(),
                                                   bl_offset_in_method1 + max_positive_disp);
  ASSERT_EQ(expected_last_method_idx, last_method_idx);

  uint32_t method1_offset = GetMethodOffset(1u);
  uint32_t last_method_offset = GetMethodOffset(last_method_idx);
  ASSERT_EQ(method1_offset + bl_offset_in_method1 + max_positive_disp, last_method_offset);

  // Check linked code.
  auto expected_code = GenNopsAndBl(1u, kBlPlusMax);
  EXPECT_TRUE(CheckLinkedMethod(MethodRef(1u), ArrayRef<const uint8_t>(expected_code)));
}

TEST_F(Arm64RelativePatcherTestDefault, CallOtherAlmostTooFarBefore) {
  auto last_method_raw_code = GenNopsAndBl(0u, kBlPlus0);
  constexpr uint32_t bl_offset_in_last_method = 0u * 4u;  // After NOPs.
  ArrayRef<const uint8_t> last_method_code(last_method_raw_code);
  ASSERT_EQ(bl_offset_in_last_method + 4u, last_method_code.size());
  LinkerPatch last_method_patches[] = {
      LinkerPatch::RelativeCodePatch(bl_offset_in_last_method, nullptr, 1u),
  };

  constexpr uint32_t max_negative_disp = 128 * MB;
  uint32_t last_method_idx = Create2MethodsWithGap(kNopCode, ArrayRef<LinkerPatch>(),
                                                   last_method_code, last_method_patches,
                                                   max_negative_disp - bl_offset_in_last_method);
  uint32_t method1_offset = GetMethodOffset(1u);
  uint32_t last_method_offset = GetMethodOffset(last_method_idx);
  ASSERT_EQ(method1_offset, last_method_offset + bl_offset_in_last_method - max_negative_disp);

  // Check linked code.
  auto expected_code = GenNopsAndBl(0u, kBlMinusMax);
  EXPECT_TRUE(CheckLinkedMethod(MethodRef(last_method_idx),
                                ArrayRef<const uint8_t>(expected_code)));
}

TEST_F(Arm64RelativePatcherTestDefault, CallOtherJustTooFarAfter) {
  auto method1_raw_code = GenNopsAndBl(0u, kBlPlus0);
  constexpr uint32_t bl_offset_in_method1 = 0u * 4u;  // After NOPs.
  ArrayRef<const uint8_t> method1_code(method1_raw_code);
  ASSERT_EQ(bl_offset_in_method1 + 4u, method1_code.size());
  uint32_t expected_last_method_idx = 65;  // Based on 2MiB chunks in Create2MethodsWithGap().
  LinkerPatch method1_patches[] = {
      LinkerPatch::RelativeCodePatch(bl_offset_in_method1, nullptr, expected_last_method_idx),
  };

  constexpr uint32_t just_over_max_positive_disp = 128 * MB;
  uint32_t last_method_idx = Create2MethodsWithGap(
      method1_code, method1_patches, kNopCode, ArrayRef<LinkerPatch>(),
      bl_offset_in_method1 + just_over_max_positive_disp);
  ASSERT_EQ(expected_last_method_idx, last_method_idx);

  uint32_t method1_offset = GetMethodOffset(1u);
  uint32_t last_method_offset = GetMethodOffset(last_method_idx);
  uint32_t last_method_header_offset = last_method_offset - sizeof(OatQuickMethodHeader);
  ASSERT_TRUE(IsAligned<kArm64Alignment>(last_method_header_offset));
  uint32_t thunk_offset = last_method_header_offset - CompiledCode::AlignCode(ThunkSize(), kArm64);
  ASSERT_TRUE(IsAligned<kArm64Alignment>(thunk_offset));
  uint32_t diff = thunk_offset - (method1_offset + bl_offset_in_method1);
  ASSERT_EQ(diff & 3u, 0u);
  ASSERT_LT(diff, 128 * MB);
  auto expected_code = GenNopsAndBl(0u, kBlPlus0 | (diff >> 2));
  EXPECT_TRUE(CheckLinkedMethod(MethodRef(1u), ArrayRef<const uint8_t>(expected_code)));
  CheckThunk(thunk_offset);
}

TEST_F(Arm64RelativePatcherTestDefault, CallOtherJustTooFarBefore) {
  auto last_method_raw_code = GenNopsAndBl(1u, kBlPlus0);
  constexpr uint32_t bl_offset_in_last_method = 1u * 4u;  // After NOPs.
  ArrayRef<const uint8_t> last_method_code(last_method_raw_code);
  ASSERT_EQ(bl_offset_in_last_method + 4u, last_method_code.size());
  LinkerPatch last_method_patches[] = {
      LinkerPatch::RelativeCodePatch(bl_offset_in_last_method, nullptr, 1u),
  };

  constexpr uint32_t just_over_max_negative_disp = 128 * MB + 4;
  uint32_t last_method_idx = Create2MethodsWithGap(
      kNopCode, ArrayRef<LinkerPatch>(), last_method_code, last_method_patches,
      just_over_max_negative_disp - bl_offset_in_last_method);
  uint32_t method1_offset = GetMethodOffset(1u);
  uint32_t last_method_offset = GetMethodOffset(last_method_idx);
  ASSERT_EQ(method1_offset,
            last_method_offset + bl_offset_in_last_method - just_over_max_negative_disp);

  // Check linked code.
  uint32_t thunk_offset =
      CompiledCode::AlignCode(last_method_offset + last_method_code.size(), kArm64);
  uint32_t diff = thunk_offset - (last_method_offset + bl_offset_in_last_method);
  ASSERT_EQ(diff & 3u, 0u);
  ASSERT_LT(diff, 128 * MB);
  auto expected_code = GenNopsAndBl(1u, kBlPlus0 | (diff >> 2));
  EXPECT_TRUE(CheckLinkedMethod(MethodRef(last_method_idx),
                                ArrayRef<const uint8_t>(expected_code)));
  EXPECT_TRUE(CheckThunk(thunk_offset));
}

}  // namespace linker
}  // namespace art
