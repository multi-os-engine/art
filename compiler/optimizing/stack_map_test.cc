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

#include "stack_map.h"

#include "base/arena_bit_vector.h"
#include "stack_map_stream.h"

#include "gtest/gtest.h"

namespace art {

static bool CheckStackMask(const StackMap& stack_map, const BitVector& bit_vector) {
  int number_of_bits = stack_map.GetNumberOfStackMaskBits();
  if (bit_vector.GetHighestBitSet() >= number_of_bits) {
    return false;
  }
  for (int i = 0; i < number_of_bits; ++i) {
    if (stack_map.GetStackMaskBit(i) != bit_vector.IsBitSet(i)) {
      return false;
    }
  }
  return true;
}

using Kind = DexRegisterLocation::Kind;

TEST(StackMapTest, Test1) {
  ArenaPool pool;
  ArenaAllocator arena(&pool);
  StackMapStream stream(&arena);

  ArenaBitVector sp_mask(&arena, 0, false);
  stream.BeginStackMapEntry(0, 64, 0x3, &sp_mask);
  stream.AddDexRegisterEntry(Kind::kInStack, 0);         // Short location.
  stream.AddDexRegisterEntry(Kind::kConstant, -2);       // Short location.
  stream.EndStackMapEntry();

  size_t size = stream.PrepareForFillIn();
  void* memory = arena.Alloc(size, kArenaAllocMisc);
  MemoryRegion region(memory, size);
  stream.FillIn(region);

  CodeInfo code_info(region);
  ASSERT_EQ(1u, code_info.GetNumberOfStackMaps());

  StackMap stack_map = code_info.GetStackMapAt(0);
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForDexPc(0)));
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForNativePcOffset(64)));
  ASSERT_EQ(0u, stack_map.GetDexPc());
  ASSERT_EQ(64u, stack_map.GetNativePcOffset());
  ASSERT_EQ(0x3u, stack_map.GetRegisterMask());

  ASSERT_TRUE(CheckStackMask(stack_map, sp_mask));

  ASSERT_TRUE(stack_map.HasDexRegisterMap());
  DexRegisterMap dex_register_map = code_info.GetDexRegisterMapOf(stack_map);
  ASSERT_TRUE(dex_register_map[0].IsLive());
  ASSERT_TRUE(dex_register_map[1].IsLive());
  ASSERT_EQ(2u, dex_register_map.GetNumberOfLiveDexRegisters());
  // The Dex register map contains:
  // - one 1-byte live bit mask, and
  // - one 1-byte set of location catalog entry indices composed of two 2-bit values.
  size_t expected_dex_register_map_size = 1u + 1u;
  ASSERT_EQ(expected_dex_register_map_size, dex_register_map.Size());

  ASSERT_EQ(Kind::kInStack, dex_register_map[0].GetKind());
  ASSERT_EQ(Kind::kConstant, dex_register_map[1].GetKind());
  ASSERT_EQ(0, dex_register_map[0].GetValue());
  ASSERT_EQ(-2, dex_register_map[1].GetValue());

  ASSERT_FALSE(stack_map.HasInlineInfo());
}

TEST(StackMapTest, Test2) {
  ArenaPool pool;
  ArenaAllocator arena(&pool);
  StackMapStream stream(&arena);

  ArenaBitVector sp_mask1(&arena, 0, true);
  sp_mask1.SetBit(2);
  sp_mask1.SetBit(4);
  stream.BeginStackMapEntry(0, 64, 0x3, &sp_mask1);
  stream.AddDexRegisterEntry(Kind::kInStack, 0);         // Short location.
  stream.AddDexRegisterEntry(Kind::kConstant, -2);       // Large location.
  stream.BeginInlineInfoEntry(82, 3, kDirect);
  stream.EndInlineInfoEntry();
  stream.BeginInlineInfoEntry(42, 2, kStatic);
  stream.EndInlineInfoEntry();
  stream.EndStackMapEntry();

  ArenaBitVector sp_mask2(&arena, 0, true);
  sp_mask2.SetBit(3);
  sp_mask2.SetBit(8);
  stream.BeginStackMapEntry(1, 128, 0xFF, &sp_mask2);
  stream.AddDexRegisterEntry(Kind::kInRegister, 18);     // Short location.
  stream.AddDexRegisterEntry(Kind::kInFpuRegister, 3);   // Short location.
  stream.EndStackMapEntry();

  ArenaBitVector sp_mask3(&arena, 0, true);
  sp_mask3.SetBit(1);
  sp_mask3.SetBit(5);
  stream.BeginStackMapEntry(2, 192, 0xAB, &sp_mask3);
  stream.AddDexRegisterEntry(Kind::kInRegister, 6);       // Short location.
  stream.AddDexRegisterEntry(Kind::kInRegisterHigh, 8);   // Short location.
  stream.EndStackMapEntry();

  ArenaBitVector sp_mask4(&arena, 0, true);
  sp_mask4.SetBit(6);
  sp_mask4.SetBit(7);
  stream.BeginStackMapEntry(3, 256, 0xCD, &sp_mask4);
  stream.AddDexRegisterEntry(Kind::kInFpuRegister, 3);      // Short location, same in stack map 2.
  stream.AddDexRegisterEntry(Kind::kInFpuRegisterHigh, 1);  // Short location.
  stream.EndStackMapEntry();

  size_t size = stream.PrepareForFillIn();
  void* memory = arena.Alloc(size, kArenaAllocMisc);
  MemoryRegion region(memory, size);
  stream.FillIn(region);

  CodeInfo code_info(region);
  ASSERT_EQ(4u, code_info.GetNumberOfStackMaps());

  // First stack map.
  {
    StackMap stack_map = code_info.GetStackMapAt(0);
    ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForDexPc(0)));
    ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForNativePcOffset(64)));
    ASSERT_EQ(0u, stack_map.GetDexPc());
    ASSERT_EQ(64u, stack_map.GetNativePcOffset());
    ASSERT_EQ(0x3u, stack_map.GetRegisterMask());

    ASSERT_TRUE(CheckStackMask(stack_map, sp_mask1));

    ASSERT_TRUE(stack_map.HasDexRegisterMap());
    DexRegisterMap dex_register_map = code_info.GetDexRegisterMapOf(stack_map);
    ASSERT_TRUE(dex_register_map[0].IsLive());
    ASSERT_TRUE(dex_register_map[1].IsLive());
    ASSERT_EQ(2u, dex_register_map.GetNumberOfLiveDexRegisters());
    // The Dex register map contains:
    // - one 1-byte live bit mask, and
    // - one 1-byte set of location catalog entry indices composed of two 2-bit values.
    size_t expected_dex_register_map_size = 1u + 1u;
    ASSERT_EQ(expected_dex_register_map_size, dex_register_map.Size());

    ASSERT_EQ(Kind::kInStack, dex_register_map[0].GetKind());
    ASSERT_EQ(Kind::kConstant, dex_register_map[1].GetKind());
    ASSERT_EQ(0, dex_register_map[0].GetValue());
    ASSERT_EQ(-2, dex_register_map[1].GetValue());

    ASSERT_TRUE(stack_map.HasInlineInfo());
    InlineInfo inline_info = code_info.GetInlineInfoOf(stack_map);
    ASSERT_EQ(2u, inline_info.GetDepth());
    ASSERT_EQ(82u, inline_info.GetMethodIndexAtDepth(0));
    ASSERT_EQ(42u, inline_info.GetMethodIndexAtDepth(1));
    ASSERT_EQ(3u, inline_info.GetDexPcAtDepth(0));
    ASSERT_EQ(2u, inline_info.GetDexPcAtDepth(1));
    ASSERT_EQ(kDirect, inline_info.GetInvokeTypeAtDepth(0));
    ASSERT_EQ(kStatic, inline_info.GetInvokeTypeAtDepth(1));
  }

  // Second stack map.
  {
    StackMap stack_map = code_info.GetStackMapAt(1);
    ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForDexPc(1u)));
    ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForNativePcOffset(128u)));
    ASSERT_EQ(1u, stack_map.GetDexPc());
    ASSERT_EQ(128u, stack_map.GetNativePcOffset());
    ASSERT_EQ(0xFFu, stack_map.GetRegisterMask());

    ASSERT_TRUE(CheckStackMask(stack_map, sp_mask2));

    ASSERT_TRUE(stack_map.HasDexRegisterMap());
    DexRegisterMap dex_register_map = code_info.GetDexRegisterMapOf(stack_map);
    ASSERT_TRUE(dex_register_map[0].IsLive());
    ASSERT_TRUE(dex_register_map[1].IsLive());
    ASSERT_EQ(2u, dex_register_map.GetNumberOfLiveDexRegisters());
    // The Dex register map contains:
    // - one 1-byte live bit mask, and
    // - one 1-byte set of location catalog entry indices composed of two 2-bit values.
    size_t expected_dex_register_map_size = 1u + 1u;
    ASSERT_EQ(expected_dex_register_map_size, dex_register_map.Size());

    ASSERT_EQ(Kind::kInRegister, dex_register_map[0].GetKind());
    ASSERT_EQ(Kind::kInFpuRegister, dex_register_map[1].GetKind());
    ASSERT_EQ(18, dex_register_map[0].GetValue());
    ASSERT_EQ(3, dex_register_map[1].GetValue());

    ASSERT_FALSE(stack_map.HasInlineInfo());
  }

  // Third stack map.
  {
    StackMap stack_map = code_info.GetStackMapAt(2);
    ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForDexPc(2u)));
    ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForNativePcOffset(192u)));
    ASSERT_EQ(2u, stack_map.GetDexPc());
    ASSERT_EQ(192u, stack_map.GetNativePcOffset());
    ASSERT_EQ(0xABu, stack_map.GetRegisterMask());

    ASSERT_TRUE(CheckStackMask(stack_map, sp_mask3));

    ASSERT_TRUE(stack_map.HasDexRegisterMap());
    DexRegisterMap dex_register_map = code_info.GetDexRegisterMapOf(stack_map);
    ASSERT_TRUE(dex_register_map[0].IsLive());
    ASSERT_TRUE(dex_register_map[1].IsLive());
    ASSERT_EQ(2u, dex_register_map.GetNumberOfLiveDexRegisters());
    // The Dex register map contains:
    // - one 1-byte live bit mask, and
    // - one 1-byte set of location catalog entry indices composed of two 2-bit values.
    size_t expected_dex_register_map_size = 1u + 1u;
    ASSERT_EQ(expected_dex_register_map_size, dex_register_map.Size());

    ASSERT_EQ(Kind::kInRegister, dex_register_map[0].GetKind());
    ASSERT_EQ(Kind::kInRegisterHigh, dex_register_map[1].GetKind());
    ASSERT_EQ(6, dex_register_map[0].GetValue());
    ASSERT_EQ(8, dex_register_map[1].GetValue());

    ASSERT_FALSE(stack_map.HasInlineInfo());
  }

  // Fourth stack map.
  {
    StackMap stack_map = code_info.GetStackMapAt(3);
    ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForDexPc(3u)));
    ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForNativePcOffset(256u)));
    ASSERT_EQ(3u, stack_map.GetDexPc());
    ASSERT_EQ(256u, stack_map.GetNativePcOffset());
    ASSERT_EQ(0xCDu, stack_map.GetRegisterMask());

    ASSERT_TRUE(CheckStackMask(stack_map, sp_mask4));

    ASSERT_TRUE(stack_map.HasDexRegisterMap());
    DexRegisterMap dex_register_map = code_info.GetDexRegisterMapOf(stack_map);
    ASSERT_TRUE(dex_register_map[0].IsLive());
    ASSERT_TRUE(dex_register_map[1].IsLive());
    ASSERT_EQ(2u, dex_register_map.GetNumberOfLiveDexRegisters());
    // The Dex register map contains:
    // - one 1-byte live bit mask, and
    // - one 1-byte set of location catalog entry indices composed of two 2-bit values.
    size_t expected_dex_register_map_size = 1u + 1u;
    ASSERT_EQ(expected_dex_register_map_size, dex_register_map.Size());

    ASSERT_EQ(Kind::kInFpuRegister, dex_register_map[0].GetKind());
    ASSERT_EQ(Kind::kInFpuRegisterHigh, dex_register_map[1].GetKind());
    ASSERT_EQ(3, dex_register_map[0].GetValue());
    ASSERT_EQ(1, dex_register_map[1].GetValue());

    ASSERT_FALSE(stack_map.HasInlineInfo());
  }
}

TEST(StackMapTest, TestNonLiveDexRegisters) {
  ArenaPool pool;
  ArenaAllocator arena(&pool);
  StackMapStream stream(&arena);

  ArenaBitVector sp_mask(&arena, 0, false);
  stream.BeginStackMapEntry(0, 64, 0x3, &sp_mask);
  stream.AddDexRegisterEntry(Kind::kNone, 0);            // No location.
  stream.AddDexRegisterEntry(Kind::kConstant, -2);       // Large location.
  stream.EndStackMapEntry();

  size_t size = stream.PrepareForFillIn();
  void* memory = arena.Alloc(size, kArenaAllocMisc);
  MemoryRegion region(memory, size);
  stream.FillIn(region);

  CodeInfo code_info(region);
  ASSERT_EQ(1u, code_info.GetNumberOfStackMaps());

  StackMap stack_map = code_info.GetStackMapAt(0);
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForDexPc(0)));
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForNativePcOffset(64)));
  ASSERT_EQ(0u, stack_map.GetDexPc());
  ASSERT_EQ(64u, stack_map.GetNativePcOffset());
  ASSERT_EQ(0x3u, stack_map.GetRegisterMask());

  ASSERT_TRUE(stack_map.HasDexRegisterMap());
  DexRegisterMap dex_register_map = code_info.GetDexRegisterMapOf(stack_map);
  ASSERT_FALSE(dex_register_map[0].IsLive());
  ASSERT_TRUE(dex_register_map[1].IsLive());
  ASSERT_EQ(1u, dex_register_map.GetNumberOfLiveDexRegisters());
  ASSERT_EQ(2u, dex_register_map.Size());

  ASSERT_EQ(Kind::kNone, dex_register_map[0].GetKind());
  ASSERT_EQ(Kind::kConstant, dex_register_map[1].GetKind());
  ASSERT_EQ(-2, dex_register_map[1].GetValue());

  ASSERT_FALSE(stack_map.HasInlineInfo());
}

TEST(StackMapTest, TestNoDexRegisterMap) {
  ArenaPool pool;
  ArenaAllocator arena(&pool);
  StackMapStream stream(&arena);

  ArenaBitVector sp_mask(&arena, 0, false);
  uint32_t number_of_dex_registers = 0;
  stream.BeginStackMapEntry(0, 64, 0x3, &sp_mask);
  stream.EndStackMapEntry();

  number_of_dex_registers = 1;
  stream.BeginStackMapEntry(1, 67, 0x4, &sp_mask);
  stream.EndStackMapEntry();

  size_t size = stream.PrepareForFillIn();
  void* memory = arena.Alloc(size, kArenaAllocMisc);
  MemoryRegion region(memory, size);
  stream.FillIn(region);

  CodeInfo code_info(region);
  ASSERT_EQ(2u, code_info.GetNumberOfStackMaps());

  StackMap stack_map = code_info.GetStackMapAt(0);
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForDexPc(0)));
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForNativePcOffset(64)));
  ASSERT_EQ(0u, stack_map.GetDexPc());
  ASSERT_EQ(64u, stack_map.GetNativePcOffset());
  ASSERT_EQ(0x3u, stack_map.GetRegisterMask());

  ASSERT_FALSE(stack_map.HasDexRegisterMap());
  ASSERT_FALSE(stack_map.HasInlineInfo());

  stack_map = code_info.GetStackMapAt(1);
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForDexPc(1)));
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForNativePcOffset(67)));
  ASSERT_EQ(1u, stack_map.GetDexPc());
  ASSERT_EQ(67u, stack_map.GetNativePcOffset());
  ASSERT_EQ(0x4u, stack_map.GetRegisterMask());

  ASSERT_FALSE(stack_map.HasDexRegisterMap());
  ASSERT_FALSE(stack_map.HasInlineInfo());
}

TEST(StackMapTest, InlineTest) {
  ArenaPool pool;
  ArenaAllocator arena(&pool);
  StackMapStream stream(&arena);

  ArenaBitVector sp_mask1(&arena, 0, true);
  sp_mask1.SetBit(2);
  sp_mask1.SetBit(4);

  // First stack map.
  stream.BeginStackMapEntry(0, 64, 0x3, &sp_mask1);
  stream.AddDexRegisterEntry(Kind::kInStack, 0);
  stream.AddDexRegisterEntry(Kind::kConstant, 4);

  stream.BeginInlineInfoEntry(42, 2, kStatic);
  stream.AddDexRegisterEntry(Kind::kInStack, 8);
  stream.EndInlineInfoEntry();
  stream.BeginInlineInfoEntry(82, 3, kStatic);
  stream.AddDexRegisterEntry(Kind::kInStack, 16);
  stream.AddDexRegisterEntry(Kind::kConstant, 20);
  stream.AddDexRegisterEntry(Kind::kInRegister, 15);
  stream.EndInlineInfoEntry();

  stream.EndStackMapEntry();

  // Second stack map.
  stream.BeginStackMapEntry(2, 22, 0x3, &sp_mask1);
  stream.AddDexRegisterEntry(Kind::kInStack, 56);
  stream.AddDexRegisterEntry(Kind::kConstant, 0);

  stream.BeginInlineInfoEntry(42, 2, kDirect);
  stream.AddDexRegisterEntry(Kind::kInStack, 12);
  stream.EndInlineInfoEntry();
  stream.BeginInlineInfoEntry(82, 3, kStatic);
  stream.AddDexRegisterEntry(Kind::kInStack, 80);
  stream.AddDexRegisterEntry(Kind::kConstant, 10);
  stream.AddDexRegisterEntry(Kind::kInRegister, 5);
  stream.EndInlineInfoEntry();
  stream.BeginInlineInfoEntry(52, 5, kVirtual);
  stream.EndInlineInfoEntry();

  stream.EndStackMapEntry();

  // Third stack map.
  stream.BeginStackMapEntry(4, 56, 0x3, &sp_mask1);
  stream.AddDexRegisterEntry(Kind::kNone, 0);
  stream.AddDexRegisterEntry(Kind::kConstant, 4);
  stream.EndStackMapEntry();

  // Fourth stack map.
  stream.BeginStackMapEntry(6, 78, 0x3, &sp_mask1);
  stream.AddDexRegisterEntry(Kind::kInStack, 56);
  stream.AddDexRegisterEntry(Kind::kConstant, 0);

  stream.BeginInlineInfoEntry(42, 2, kVirtual);
  stream.EndInlineInfoEntry();
  stream.BeginInlineInfoEntry(52, 5, kInterface);
  stream.AddDexRegisterEntry(Kind::kInRegister, 2);
  stream.EndInlineInfoEntry();
  stream.BeginInlineInfoEntry(52, 10, kStatic);
  stream.AddDexRegisterEntry(Kind::kNone, 0);
  stream.AddDexRegisterEntry(Kind::kInRegister, 3);
  stream.EndInlineInfoEntry();

  stream.EndStackMapEntry();

  size_t size = stream.PrepareForFillIn();
  void* memory = arena.Alloc(size, kArenaAllocMisc);
  MemoryRegion region(memory, size);
  stream.FillIn(region);

  CodeInfo ci(region);
  {
    // Verify first stack map.
    StackMap sm0 = ci.GetStackMapAt(0);

    DexRegisterMap dex_registers0 = ci.GetDexRegisterMapOf(sm0);
    ASSERT_EQ(0, dex_registers0[0].GetValue());
    ASSERT_EQ(4, dex_registers0[1].GetValue());

    InlineInfo if0 = ci.GetInlineInfoOf(sm0);
    ASSERT_EQ(2u, if0.GetDepth());
    ASSERT_EQ(2u, if0.GetDexPcAtDepth(0));
    ASSERT_EQ(42u, if0.GetMethodIndexAtDepth(0));
    ASSERT_EQ(kStatic, if0.GetInvokeTypeAtDepth(0));
    ASSERT_EQ(3u, if0.GetDexPcAtDepth(1));
    ASSERT_EQ(82u, if0.GetMethodIndexAtDepth(1));
    ASSERT_EQ(kStatic, if0.GetInvokeTypeAtDepth(1));

    DexRegisterMap dex_registers1 = ci.GetDexRegisterMapAtDepth(0, if0);
    ASSERT_EQ(8, dex_registers1[0].GetValue());

    DexRegisterMap dex_registers2 = ci.GetDexRegisterMapAtDepth(1, if0);
    ASSERT_EQ(16, dex_registers2[0].GetValue());
    ASSERT_EQ(20, dex_registers2[1].GetValue());
    ASSERT_EQ(15, dex_registers2[2].GetValue());
  }

  {
    // Verify second stack map.
    StackMap sm1 = ci.GetStackMapAt(1);

    DexRegisterMap dex_registers0 = ci.GetDexRegisterMapOf(sm1);
    ASSERT_EQ(56, dex_registers0[0].GetValue());
    ASSERT_EQ(0, dex_registers0[1].GetValue());

    InlineInfo if1 = ci.GetInlineInfoOf(sm1);
    ASSERT_EQ(3u, if1.GetDepth());
    ASSERT_EQ(2u, if1.GetDexPcAtDepth(0));
    ASSERT_EQ(42u, if1.GetMethodIndexAtDepth(0));
    ASSERT_EQ(kDirect, if1.GetInvokeTypeAtDepth(0));
    ASSERT_EQ(3u, if1.GetDexPcAtDepth(1));
    ASSERT_EQ(82u, if1.GetMethodIndexAtDepth(1));
    ASSERT_EQ(kStatic, if1.GetInvokeTypeAtDepth(1));
    ASSERT_EQ(5u, if1.GetDexPcAtDepth(2));
    ASSERT_EQ(52u, if1.GetMethodIndexAtDepth(2));
    ASSERT_EQ(kVirtual, if1.GetInvokeTypeAtDepth(2));

    DexRegisterMap dex_registers1 = ci.GetDexRegisterMapAtDepth(0, if1);
    ASSERT_EQ(12, dex_registers1[0].GetValue());

    DexRegisterMap dex_registers2 = ci.GetDexRegisterMapAtDepth(1, if1);
    ASSERT_EQ(80, dex_registers2[0].GetValue());
    ASSERT_EQ(10, dex_registers2[1].GetValue());
    ASSERT_EQ(5, dex_registers2[2].GetValue());

    ASSERT_FALSE(if1.HasDexRegisterMapAtDepth(2));
  }

  {
    // Verify third stack map.
    StackMap sm2 = ci.GetStackMapAt(2);

    DexRegisterMap dex_registers0 = ci.GetDexRegisterMapOf(sm2);
    ASSERT_FALSE(dex_registers0[0].IsLive());
    ASSERT_EQ(4, dex_registers0[1].GetValue());
    ASSERT_FALSE(sm2.HasInlineInfo());
  }

  {
    // Verify fourth stack map.
    StackMap sm3 = ci.GetStackMapAt(3);

    DexRegisterMap dex_registers0 = ci.GetDexRegisterMapOf(sm3);
    ASSERT_EQ(56, dex_registers0[0].GetValue());
    ASSERT_EQ(0, dex_registers0[1].GetValue());

    InlineInfo if2 = ci.GetInlineInfoOf(sm3);
    ASSERT_EQ(3u, if2.GetDepth());
    ASSERT_EQ(2u, if2.GetDexPcAtDepth(0));
    ASSERT_EQ(42u, if2.GetMethodIndexAtDepth(0));
    ASSERT_EQ(kVirtual, if2.GetInvokeTypeAtDepth(0));
    ASSERT_EQ(5u, if2.GetDexPcAtDepth(1));
    ASSERT_EQ(52u, if2.GetMethodIndexAtDepth(1));
    ASSERT_EQ(kInterface, if2.GetInvokeTypeAtDepth(1));
    ASSERT_EQ(10u, if2.GetDexPcAtDepth(2));
    ASSERT_EQ(52u, if2.GetMethodIndexAtDepth(2));
    ASSERT_EQ(kStatic, if2.GetInvokeTypeAtDepth(2));

    ASSERT_FALSE(if2.HasDexRegisterMapAtDepth(0));

    DexRegisterMap dex_registers1 = ci.GetDexRegisterMapAtDepth(1, if2);
    ASSERT_EQ(2, dex_registers1[0].GetValue());

    DexRegisterMap dex_registers2 = ci.GetDexRegisterMapAtDepth(2, if2);
    ASSERT_FALSE(dex_registers2[0].IsLive());
    ASSERT_EQ(3, dex_registers2[1].GetValue());
  }
}

}  // namespace art
