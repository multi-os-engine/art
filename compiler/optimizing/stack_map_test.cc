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
#include "stack_map_stream.h"
#include "utils/arena_bit_vector.h"

#include "gtest/gtest.h"

namespace art {

static bool SameBits(MemoryRegion region, const BitVector& bit_vector) {
  for (size_t i = 0; i < region.size_in_bits(); ++i) {
    if (region.LoadBit(i) != bit_vector.IsBitSet(i)) {
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
  size_t number_of_dex_registers = 2;
  stream.AddStackMapEntry(0, 64, 0x3, &sp_mask, number_of_dex_registers, 0);
  stream.AddDexRegisterEntry(0, Kind::kInStack, 0);         // Short location.
  stream.AddDexRegisterEntry(1, Kind::kConstant, -2);       // Short location.

  size_t size = stream.ComputeNeededSize();
  void* memory = arena.Alloc(size, kArenaAllocMisc);
  MemoryRegion region(memory, size);
  stream.FillIn(region);

  CodeInfo code_info(region);
  ASSERT_EQ(0u, code_info.GetStackMaskSize());
  ASSERT_EQ(1u, code_info.GetNumberOfStackMaps());

  uint32_t number_of_dictionary_entries = code_info.GetNumberOfDexRegisterDictionaryEntries();
  ASSERT_EQ(2u, number_of_dictionary_entries);
  DexRegisterDictionary dictionary = code_info.GetDexRegisterDictionary();
  // The Dex register dictionary contains:
  // - one 1-byte short Dex register location, and
  // - one 5-byte large Dex register location.
  size_t expected_dictionary_size = 1u + 5u;
  ASSERT_EQ(expected_dictionary_size, dictionary.Size());

  StackMap stack_map = code_info.GetStackMapAt(0);
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForDexPc(0)));
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForNativePcOffset(64)));
  ASSERT_EQ(0u, stack_map.GetDexPc());
  ASSERT_EQ(64u, stack_map.GetNativePcOffset());
  ASSERT_EQ(0x3u, stack_map.GetRegisterMask());

  MemoryRegion stack_mask = stack_map.GetStackMask();
  ASSERT_TRUE(SameBits(stack_mask, sp_mask));

  ASSERT_TRUE(stack_map.HasDexRegisterMap());
  DexRegisterMap dex_register_map =
      code_info.GetDexRegisterMapOf(stack_map, number_of_dex_registers);
  ASSERT_TRUE(dex_register_map.IsDexRegisterLive(0));
  ASSERT_TRUE(dex_register_map.IsDexRegisterLive(1));
  ASSERT_EQ(2u, dex_register_map.GetNumberOfLiveDexRegisters(number_of_dex_registers));
  // The Dex register map contains:
  // - one 1-byte live bit mask, and
  // - one 1-byte set of dictionary entry indices composed of two 2-bit values.
  size_t expected_dex_register_map_size = 1u + 1u;
  ASSERT_EQ(expected_dex_register_map_size, dex_register_map.Size());
  size_t index0 = dex_register_map.GetDictionaryEntryIndex(
      0, number_of_dex_registers, number_of_dictionary_entries);
  size_t index1 = dex_register_map.GetDictionaryEntryIndex(
      1, number_of_dex_registers, number_of_dictionary_entries);
  ASSERT_EQ(0u, index0);
  ASSERT_EQ(1u, index1);
  DexRegisterLocation location0 = dictionary.GetLocationKindAndValue(index0);
  DexRegisterLocation location1 = dictionary.GetLocationKindAndValue(index1);
  ASSERT_EQ(Kind::kInStack, location0.GetKind());
  ASSERT_EQ(Kind::kConstant, location1.GetKind());
  ASSERT_EQ(Kind::kInStack, location0.GetInternalKind());
  ASSERT_EQ(Kind::kConstantLargeValue, location1.GetInternalKind());
  ASSERT_EQ(0, location0.GetValue());
  ASSERT_EQ(-2, location1.GetValue());

  ASSERT_FALSE(stack_map.HasInlineInfo());
}

TEST(StackMapTest, Test2) {
  ArenaPool pool;
  ArenaAllocator arena(&pool);
  StackMapStream stream(&arena);

  ArenaBitVector sp_mask1(&arena, 0, true);
  sp_mask1.SetBit(2);
  sp_mask1.SetBit(4);
  size_t number_of_dex_registers = 2;
  stream.AddStackMapEntry(0, 64, 0x3, &sp_mask1, number_of_dex_registers, 2);
  stream.AddDexRegisterEntry(0, Kind::kInStack, 0);         // Short location.
  stream.AddDexRegisterEntry(1, Kind::kConstant, -2);       // Large location.
  stream.AddInlineInfoEntry(42);
  stream.AddInlineInfoEntry(82);

  ArenaBitVector sp_mask2(&arena, 0, true);
  sp_mask2.SetBit(3);
  sp_mask1.SetBit(8);
  stream.AddStackMapEntry(1, 128, 0xFF, &sp_mask2, number_of_dex_registers, 0);
  stream.AddDexRegisterEntry(0, Kind::kInRegister, 18);     // Short location.
  stream.AddDexRegisterEntry(1, Kind::kInFpuRegister, 3);   // Short location.

  size_t size = stream.ComputeNeededSize();
  void* memory = arena.Alloc(size, kArenaAllocMisc);
  MemoryRegion region(memory, size);
  stream.FillIn(region);

  CodeInfo code_info(region);
  ASSERT_EQ(1u, code_info.GetStackMaskSize());
  ASSERT_EQ(2u, code_info.GetNumberOfStackMaps());

  uint32_t number_of_dictionary_entries = code_info.GetNumberOfDexRegisterDictionaryEntries();
  ASSERT_EQ(4u, number_of_dictionary_entries);
  DexRegisterDictionary dictionary = code_info.GetDexRegisterDictionary();
  // The Dex register dictionary contains:
  // - three 1-byte short Dex register locations, and
  // - one 5-byte large Dex register location.
  size_t expected_dictionary_size = 3u * 1u + 5u;
  ASSERT_EQ(expected_dictionary_size, dictionary.Size());

  // First stack map.
  {
    StackMap stack_map = code_info.GetStackMapAt(0);
    ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForDexPc(0)));
    ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForNativePcOffset(64)));
    ASSERT_EQ(0u, stack_map.GetDexPc());
    ASSERT_EQ(64u, stack_map.GetNativePcOffset());
    ASSERT_EQ(0x3u, stack_map.GetRegisterMask());

    MemoryRegion stack_mask = stack_map.GetStackMask();
    ASSERT_TRUE(SameBits(stack_mask, sp_mask1));

    ASSERT_TRUE(stack_map.HasDexRegisterMap());
    DexRegisterMap dex_register_map =
        code_info.GetDexRegisterMapOf(stack_map, number_of_dex_registers);
    ASSERT_TRUE(dex_register_map.IsDexRegisterLive(0));
    ASSERT_TRUE(dex_register_map.IsDexRegisterLive(1));
    ASSERT_EQ(2u, dex_register_map.GetNumberOfLiveDexRegisters(number_of_dex_registers));
    // The Dex register map contains:
    // - one 1-byte live bit mask, and
    // - one 1-byte set of dictionary entry indices composed of two 2-bit
    //   values.
    size_t expected_dex_register_map_size = 1u + 1u;
    ASSERT_EQ(expected_dex_register_map_size, dex_register_map.Size());
    size_t index0 = dex_register_map.GetDictionaryEntryIndex(
        0, number_of_dex_registers, number_of_dictionary_entries);
    size_t index1 = dex_register_map.GetDictionaryEntryIndex(
        1, number_of_dex_registers, number_of_dictionary_entries);
    ASSERT_EQ(0u, index0);
    ASSERT_EQ(1u, index1);
    DexRegisterLocation location0 = dictionary.GetLocationKindAndValue(index0);
    DexRegisterLocation location1 = dictionary.GetLocationKindAndValue(index1);
    ASSERT_EQ(Kind::kInStack, location0.GetKind());
    ASSERT_EQ(Kind::kConstant, location1.GetKind());
    ASSERT_EQ(Kind::kInStack, location0.GetInternalKind());
    ASSERT_EQ(Kind::kConstantLargeValue, location1.GetInternalKind());
    ASSERT_EQ(0, location0.GetValue());
    ASSERT_EQ(-2, location1.GetValue());

    ASSERT_TRUE(stack_map.HasInlineInfo());
    InlineInfo inline_info = code_info.GetInlineInfoOf(stack_map);
    ASSERT_EQ(2u, inline_info.GetDepth());
    ASSERT_EQ(42u, inline_info.GetMethodReferenceIndexAtDepth(0));
    ASSERT_EQ(82u, inline_info.GetMethodReferenceIndexAtDepth(1));
  }

  // Second stack map.
  {
    StackMap stack_map = code_info.GetStackMapAt(1);
    ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForDexPc(1u)));
    ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForNativePcOffset(128u)));
    ASSERT_EQ(1u, stack_map.GetDexPc());
    ASSERT_EQ(128u, stack_map.GetNativePcOffset());
    ASSERT_EQ(0xFFu, stack_map.GetRegisterMask());

    MemoryRegion stack_mask = stack_map.GetStackMask();
    ASSERT_TRUE(SameBits(stack_mask, sp_mask2));

    ASSERT_TRUE(stack_map.HasDexRegisterMap());
    DexRegisterMap dex_register_map =
        code_info.GetDexRegisterMapOf(stack_map, number_of_dex_registers);
    ASSERT_TRUE(dex_register_map.IsDexRegisterLive(0));
    ASSERT_TRUE(dex_register_map.IsDexRegisterLive(1));
    ASSERT_EQ(2u, dex_register_map.GetNumberOfLiveDexRegisters(number_of_dex_registers));
    // The Dex register map contains:
    // - one 1-byte live bit mask, and
    // - one 1-byte set of dictionary entry indices composed of two 2-bit
    //   values.
    size_t expected_dex_register_map_size = 1u + 1u;
    ASSERT_EQ(expected_dex_register_map_size, dex_register_map.Size());
    size_t index0 = dex_register_map.GetDictionaryEntryIndex(
        0, number_of_dex_registers, number_of_dictionary_entries);
    size_t index1 = dex_register_map.GetDictionaryEntryIndex(
        1, number_of_dex_registers, number_of_dictionary_entries);
    ASSERT_EQ(2u, index0);
    ASSERT_EQ(3u, index1);
    DexRegisterLocation location0 = dictionary.GetLocationKindAndValue(index0);
    DexRegisterLocation location1 = dictionary.GetLocationKindAndValue(index1);
    ASSERT_EQ(Kind::kInRegister, location0.GetKind());
    ASSERT_EQ(Kind::kInFpuRegister, location1.GetKind());
    ASSERT_EQ(Kind::kInRegister, location0.GetInternalKind());
    ASSERT_EQ(Kind::kInFpuRegister, location1.GetInternalKind());
    ASSERT_EQ(18, location0.GetValue());
    ASSERT_EQ(3, location1.GetValue());

    ASSERT_FALSE(stack_map.HasInlineInfo());
  }
}

TEST(StackMapTest, TestNonLiveDexRegisters) {
  ArenaPool pool;
  ArenaAllocator arena(&pool);
  StackMapStream stream(&arena);

  ArenaBitVector sp_mask(&arena, 0, false);
  uint32_t number_of_dex_registers = 2;
  stream.AddStackMapEntry(0, 64, 0x3, &sp_mask, number_of_dex_registers, 0);
  stream.AddDexRegisterEntry(0, Kind::kNone, 0);            // No location.
  stream.AddDexRegisterEntry(1, Kind::kConstant, -2);       // Large location.

  size_t size = stream.ComputeNeededSize();
  void* memory = arena.Alloc(size, kArenaAllocMisc);
  MemoryRegion region(memory, size);
  stream.FillIn(region);

  CodeInfo code_info(region);
  ASSERT_EQ(0u, code_info.GetStackMaskSize());
  ASSERT_EQ(1u, code_info.GetNumberOfStackMaps());

  uint32_t number_of_dictionary_entries = code_info.GetNumberOfDexRegisterDictionaryEntries();
  ASSERT_EQ(1u, number_of_dictionary_entries);
  DexRegisterDictionary dictionary = code_info.GetDexRegisterDictionary();
  // The Dex register dictionary contains:
  // - one 5-byte large Dex register location.
  size_t expected_dictionary_size = 5u;
  ASSERT_EQ(expected_dictionary_size, dictionary.Size());

  StackMap stack_map = code_info.GetStackMapAt(0);
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForDexPc(0)));
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForNativePcOffset(64)));
  ASSERT_EQ(0u, stack_map.GetDexPc());
  ASSERT_EQ(64u, stack_map.GetNativePcOffset());
  ASSERT_EQ(0x3u, stack_map.GetRegisterMask());

  ASSERT_TRUE(stack_map.HasDexRegisterMap());
  DexRegisterMap dex_register_map =
      code_info.GetDexRegisterMapOf(stack_map, number_of_dex_registers);
  ASSERT_FALSE(dex_register_map.IsDexRegisterLive(0));
  ASSERT_TRUE(dex_register_map.IsDexRegisterLive(1));
  ASSERT_EQ(1u, dex_register_map.GetNumberOfLiveDexRegisters(number_of_dex_registers));
  // The Dex register map contains:
  // - one 1-byte live bit mask.
  // No space is allocated for the sole dictionary entry index, as it
  // is useless.
  size_t expected_dex_register_map_size = 1u + 0u;
  ASSERT_EQ(expected_dex_register_map_size, dex_register_map.Size());
  size_t index0 = dex_register_map.GetDictionaryEntryIndex(
      0, number_of_dex_registers, number_of_dictionary_entries);
  size_t index1 =  dex_register_map.GetDictionaryEntryIndex(
      1, number_of_dex_registers, number_of_dictionary_entries);
  ASSERT_EQ(DexRegisterDictionary::NoDexRegisterLocationEntryIndex(), index0);
  ASSERT_EQ(0u, index1);
  DexRegisterLocation location0 = dictionary.GetLocationKindAndValue(index0);
  DexRegisterLocation location1 = dictionary.GetLocationKindAndValue(index1);
  ASSERT_EQ(Kind::kNone, location0.GetKind());
  ASSERT_EQ(Kind::kConstant, location1.GetKind());
  ASSERT_EQ(Kind::kNone, location0.GetInternalKind());
  ASSERT_EQ(Kind::kConstantLargeValue, location1.GetInternalKind());
  ASSERT_EQ(0, location0.GetValue());
  ASSERT_EQ(-2, location1.GetValue());

  ASSERT_FALSE(stack_map.HasInlineInfo());
}

TEST(StackMapTest, TestNoDexRegisterMap) {
  ArenaPool pool;
  ArenaAllocator arena(&pool);
  StackMapStream stream(&arena);

  ArenaBitVector sp_mask(&arena, 0, false);
  uint32_t number_of_dex_registers = 0;
  stream.AddStackMapEntry(0, 64, 0x3, &sp_mask, number_of_dex_registers, 0);

  size_t size = stream.ComputeNeededSize();
  void* memory = arena.Alloc(size, kArenaAllocMisc);
  MemoryRegion region(memory, size);
  stream.FillIn(region);

  CodeInfo code_info(region);
  ASSERT_EQ(0u, code_info.GetStackMaskSize());
  ASSERT_EQ(1u, code_info.GetNumberOfStackMaps());

  uint32_t number_of_dictionary_entries = code_info.GetNumberOfDexRegisterDictionaryEntries();
  ASSERT_EQ(0u, number_of_dictionary_entries);
  DexRegisterDictionary dictionary = code_info.GetDexRegisterDictionary();
  ASSERT_EQ(0u, dictionary.Size());

  StackMap stack_map = code_info.GetStackMapAt(0);
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForDexPc(0)));
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForNativePcOffset(64)));
  ASSERT_EQ(0u, stack_map.GetDexPc());
  ASSERT_EQ(64u, stack_map.GetNativePcOffset());
  ASSERT_EQ(0x3u, stack_map.GetRegisterMask());

  ASSERT_FALSE(stack_map.HasDexRegisterMap());
  ASSERT_FALSE(stack_map.HasInlineInfo());
}

}  // namespace art
