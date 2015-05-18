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

#include "byte_swap_utils-inl.h"

#include <stdlib.h>
#include <functional>

#include <gtest/gtest.h>

namespace art {

class ByteSwapUtilsTest : public testing::Test {};

static constexpr size_t kAlignment = 8;

template<typename T, size_t kNumElements, typename InitFunc>
void SwapAlignTest(void (*swap_func)(T*, const T*, size_t), InitFunc init_func) {
  uint8_t* dst = nullptr;
  uint8_t* src = nullptr;
  ASSERT_EQ(0, posix_memalign(reinterpret_cast<void**>(&dst), kAlignment,
                              sizeof(T) * kNumElements + kAlignment));
  ASSERT_EQ(0, posix_memalign(reinterpret_cast<void**>(&src), kAlignment,
                              sizeof(T) * kNumElements + kAlignment));

  T src_buf[kNumElements];
  T dst_buf[kNumElements];
  for (uint64_t i = 0; i < kNumElements; i++) {
    init_func(&src_buf[i], &dst_buf[i], i);
  }

  // Vary a few alignments.
  for (size_t dst_offset = 0; dst_offset < kAlignment; dst_offset++) {
    T* dst_unaligned = reinterpret_cast<T*>(&dst[dst_offset]);
    for (size_t src_offset = 0; src_offset < kAlignment; src_offset++) {
      T* src_unaligned = reinterpret_cast<T*>(&src[src_offset]);
      memset(dst_unaligned, 0, sizeof(T) * kNumElements);
      memcpy(src_unaligned, src_buf, sizeof(T) * kNumElements);
      swap_func(dst_unaligned, src_unaligned, kNumElements);
      ASSERT_EQ(0, memcmp(dst_buf, dst_unaligned, sizeof(T) * kNumElements))
          << "Failed at dst align " << dst_offset << " src align " << src_offset;
    }
  }
  free(dst);
  free(src);
}

TEST_F(ByteSwapUtilsTest, SwapShorts_AlignTest) {
  // Use an odd number to guarantee that the last 16-bit swap code
  // is executed.
  auto init_func = [] (jshort* src, jshort* dst, uint64_t i) {
    *src = ((2*i) << 8) | (2*(i+1));
    *dst = (2*i) | ((2*(i+1)) << 8);
  };

  SwapAlignTest<jshort, 9, decltype(init_func)>(SwapShorts, init_func);
}

TEST_F(ByteSwapUtilsTest, SwapInts_AlignTest) {
  auto init_func = [] (jint* src, jint* dst, uint64_t i) {
    *src = ((4*i) << 24) | ((4*(i+1)) << 16) | ((4*(i+2)) << 8) | (4*(i+3));
    *dst = (4*i) | ((4*(i+1)) << 8) | ((4*(i+2)) << 16) | ((4*(i+3)) << 24);
  };
  SwapAlignTest<jint, 10, decltype(init_func)>(SwapInts, init_func);
}

TEST_F(ByteSwapUtilsTest, SwapLongs_AlignTest) {
  auto init_func = [] (jlong* src, jlong* dst, uint64_t i) {
    *src = ((8*i) << 56) | ((8*(i+1)) << 48) | ((8*(i+2)) << 40) | ((8*(i+3)) << 32) |
        ((8*(i+4)) << 24) | ((8*(i+5)) << 16) | ((8*(i+6)) << 8) | (8*(i+7));
    *dst = (8*i) | ((8*(i+1)) << 8) | ((8*(i+2)) << 16) | ((8*(i+3)) << 24) |
        ((8*(i+4)) << 32) | ((8*(i+5)) << 40) | ((8*(i+6)) << 48) | ((8*(i+7)) << 56);
  };

  SwapAlignTest<jlong, 10, decltype(init_func)>(SwapLongs, init_func);
}

template<typename T>
void MemoryPeekTest(T value) {
  T* src = nullptr;
  ASSERT_EQ(0, posix_memalign(reinterpret_cast<void**>(&src), kAlignment,
                              sizeof(T) + kAlignment));
  for (size_t i = 0; i < kAlignment; i++) {
    T* src_aligned = reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(src) + i);
    memcpy(reinterpret_cast<void*>(src_aligned), &value, sizeof(T));
    T result = GetUnaligned<T>(src_aligned);
    ASSERT_EQ(value, result);
  }
  free(src);
}

TEST_F(ByteSwapUtilsTest, PeekShort_AlignCheck) {
  MemoryPeekTest<jshort>(0x0102);
}

TEST_F(ByteSwapUtilsTest, PeekInt_AlignCheck) {
  MemoryPeekTest<jint>(0x01020304);
}

TEST_F(ByteSwapUtilsTest, PeekLong_AlignCheck) {
  MemoryPeekTest<jlong>(0x01020405060708ULL);
}

template<typename T>
void MemoryPokeTest(T value) {
  T* dst = nullptr;
  ASSERT_EQ(0, posix_memalign(reinterpret_cast<void**>(&dst), kAlignment,
                              sizeof(T) + kAlignment));
  for(size_t i = 0; i < kAlignment; i++) {
    memset(dst, 0, sizeof(T) + kAlignment);
    T* dst_aligned = reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(dst) + i);
    PutUnaligned<T>(dst_aligned, value);
    ASSERT_EQ(0, memcmp(reinterpret_cast<void*>(dst_aligned), &value, sizeof(T)));
  }
  free(dst);
}

TEST_F(ByteSwapUtilsTest, PokeShort_AlignCheck) {
  MemoryPokeTest<jshort>(0x0102);
}

TEST_F(ByteSwapUtilsTest, PokeInt_AlignCheck) {
  MemoryPokeTest<jint>(0x01020304);
}

TEST_F(ByteSwapUtilsTest, PokeLong_AlignCheck) {
  MemoryPokeTest<jlong>(0x01020405060708ULL);
}

}  // namespace art
