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

  uint32_t number_of_location_set_entries = code_info.GetNumberOfDexRegisterLocationSetEntries();
  ASSERT_EQ(2u, number_of_location_set_entries);
  DexRegisterLocationSet location_set = code_info.GetDexRegisterLocationSet();
  // The Dex register location set contains:
  // - one 1-byte short Dex register location, and
  // - one 5-byte large Dex register location.
  size_t expected_location_set_size = 1u + 5u;
  ASSERT_EQ(expected_location_set_size, location_set.Size());

  StackMap stack_map = code_info.GetStackMapAt(0);
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForDexPc(0)));
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForNativePcOffset(64)));
  ASSERT_EQ(0u, stack_map.GetDexPc(code_info));
  ASSERT_EQ(64u, stack_map.GetNativePcOffset(code_info));
  ASSERT_EQ(0x3u, stack_map.GetRegisterMask(code_info));

  MemoryRegion stack_mask = stack_map.GetStackMask(code_info);
  ASSERT_TRUE(SameBits(stack_mask, sp_mask));

  ASSERT_TRUE(stack_map.HasDexRegisterMap(code_info));
  DexRegisterMap dex_register_map =
      code_info.GetDexRegisterMapOf(stack_map, number_of_dex_registers);
  ASSERT_TRUE(dex_register_map.IsDexRegisterLive(0));
  ASSERT_TRUE(dex_register_map.IsDexRegisterLive(1));
  ASSERT_EQ(2u, dex_register_map.GetNumberOfLiveDexRegisters(number_of_dex_registers));
  // The Dex register map contains:
  // - one 1-byte live bit mask, and
  // - one 1-byte set of location set entry indices composed of two 2-bit values.
  size_t expected_dex_register_map_size = 1u + 1u;
  ASSERT_EQ(expected_dex_register_map_size, dex_register_map.Size());

  ASSERT_EQ(Kind::kInStack,
            dex_register_map.GetLocationKind(0, number_of_dex_registers, code_info));
  ASSERT_EQ(Kind::kConstant,
            dex_register_map.GetLocationKind(1, number_of_dex_registers, code_info));
  ASSERT_EQ(Kind::kInStack,
            dex_register_map.GetLocationInternalKind(0, number_of_dex_registers, code_info));
  ASSERT_EQ(Kind::kConstantLargeValue,
            dex_register_map.GetLocationInternalKind(1, number_of_dex_registers, code_info));
  ASSERT_EQ(0, dex_register_map.GetStackOffsetInBytes(0, number_of_dex_registers, code_info));
  ASSERT_EQ(-2, dex_register_map.GetConstant(1, number_of_dex_registers, code_info));

  size_t index0 = dex_register_map.GetLocationSetEntryIndex(
      0, number_of_dex_registers, number_of_location_set_entries);
  size_t index1 = dex_register_map.GetLocationSetEntryIndex(
      1, number_of_dex_registers, number_of_location_set_entries);
  ASSERT_EQ(0u, index0);
  ASSERT_EQ(1u, index1);
  DexRegisterLocation location0 = location_set.GetDexRegisterLocation(index0);
  DexRegisterLocation location1 = location_set.GetDexRegisterLocation(index1);
  ASSERT_EQ(Kind::kInStack, location0.GetKind());
  ASSERT_EQ(Kind::kConstant, location1.GetKind());
  ASSERT_EQ(Kind::kInStack, location0.GetInternalKind());
  ASSERT_EQ(Kind::kConstantLargeValue, location1.GetInternalKind());
  ASSERT_EQ(0, location0.GetValue());
  ASSERT_EQ(-2, location1.GetValue());

  ASSERT_FALSE(stack_map.HasInlineInfo(code_info));
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

  uint32_t number_of_location_set_entries = code_info.GetNumberOfDexRegisterLocationSetEntries();
  ASSERT_EQ(4u, number_of_location_set_entries);
  DexRegisterLocationSet location_set = code_info.GetDexRegisterLocationSet();
  // The Dex register location set contains:
  // - three 1-byte short Dex register locations, and
  // - one 5-byte large Dex register location.
  size_t expected_location_set_size = 3u * 1u + 5u;
  ASSERT_EQ(expected_location_set_size, location_set.Size());

  // First stack map.
  {
    StackMap stack_map = code_info.GetStackMapAt(0);
    ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForDexPc(0)));
    ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForNativePcOffset(64)));
    ASSERT_EQ(0u, stack_map.GetDexPc(code_info));
    ASSERT_EQ(64u, stack_map.GetNativePcOffset(code_info));
    ASSERT_EQ(0x3u, stack_map.GetRegisterMask(code_info));

    MemoryRegion stack_mask = stack_map.GetStackMask(code_info);
    ASSERT_TRUE(SameBits(stack_mask, sp_mask1));

    ASSERT_TRUE(stack_map.HasDexRegisterMap(code_info));
    DexRegisterMap dex_register_map =
        code_info.GetDexRegisterMapOf(stack_map, number_of_dex_registers);
    ASSERT_TRUE(dex_register_map.IsDexRegisterLive(0));
    ASSERT_TRUE(dex_register_map.IsDexRegisterLive(1));
    ASSERT_EQ(2u, dex_register_map.GetNumberOfLiveDexRegisters(number_of_dex_registers));
    // The Dex register map contains:
    // - one 1-byte live bit mask, and
    // - one 1-byte set of location set entry indices composed of two 2-bit
    //   values.
    size_t expected_dex_register_map_size = 1u + 1u;
    ASSERT_EQ(expected_dex_register_map_size, dex_register_map.Size());

    ASSERT_EQ(Kind::kInStack,
              dex_register_map.GetLocationKind(0, number_of_dex_registers, code_info));
    ASSERT_EQ(Kind::kConstant,
              dex_register_map.GetLocationKind(1, number_of_dex_registers, code_info));
    ASSERT_EQ(Kind::kInStack,
              dex_register_map.GetLocationInternalKind(0, number_of_dex_registers, code_info));
    ASSERT_EQ(Kind::kConstantLargeValue,
              dex_register_map.GetLocationInternalKind(1, number_of_dex_registers, code_info));
    ASSERT_EQ(0, dex_register_map.GetStackOffsetInBytes(0, number_of_dex_registers, code_info));
    ASSERT_EQ(-2, dex_register_map.GetConstant(1, number_of_dex_registers, code_info));

    size_t index0 = dex_register_map.GetLocationSetEntryIndex(
        0, number_of_dex_registers, number_of_location_set_entries);
    size_t index1 = dex_register_map.GetLocationSetEntryIndex(
        1, number_of_dex_registers, number_of_location_set_entries);
    ASSERT_EQ(0u, index0);
    ASSERT_EQ(1u, index1);
    DexRegisterLocation location0 = location_set.GetDexRegisterLocation(index0);
    DexRegisterLocation location1 = location_set.GetDexRegisterLocation(index1);
    ASSERT_EQ(Kind::kInStack, location0.GetKind());
    ASSERT_EQ(Kind::kConstant, location1.GetKind());
    ASSERT_EQ(Kind::kInStack, location0.GetInternalKind());
    ASSERT_EQ(Kind::kConstantLargeValue, location1.GetInternalKind());
    ASSERT_EQ(0, location0.GetValue());
    ASSERT_EQ(-2, location1.GetValue());

    ASSERT_TRUE(stack_map.HasInlineInfo(code_info));
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
    ASSERT_EQ(1u, stack_map.GetDexPc(code_info));
    ASSERT_EQ(128u, stack_map.GetNativePcOffset(code_info));
    ASSERT_EQ(0xFFu, stack_map.GetRegisterMask(code_info));

    MemoryRegion stack_mask = stack_map.GetStackMask(code_info);
    ASSERT_TRUE(SameBits(stack_mask, sp_mask2));

    ASSERT_TRUE(stack_map.HasDexRegisterMap(code_info));
    DexRegisterMap dex_register_map =
        code_info.GetDexRegisterMapOf(stack_map, number_of_dex_registers);
    ASSERT_TRUE(dex_register_map.IsDexRegisterLive(0));
    ASSERT_TRUE(dex_register_map.IsDexRegisterLive(1));
    ASSERT_EQ(2u, dex_register_map.GetNumberOfLiveDexRegisters(number_of_dex_registers));
    // The Dex register map contains:
    // - one 1-byte live bit mask, and
    // - one 1-byte set of location set entry indices composed of two 2-bit
    //   values.
    size_t expected_dex_register_map_size = 1u + 1u;
    ASSERT_EQ(expected_dex_register_map_size, dex_register_map.Size());

    ASSERT_EQ(Kind::kInRegister,
              dex_register_map.GetLocationKind(0, number_of_dex_registers, code_info));
    ASSERT_EQ(Kind::kInFpuRegister,
              dex_register_map.GetLocationKind(1, number_of_dex_registers, code_info));
    ASSERT_EQ(Kind::kInRegister,
              dex_register_map.GetLocationInternalKind(0, number_of_dex_registers, code_info));
    ASSERT_EQ(Kind::kInFpuRegister,
              dex_register_map.GetLocationInternalKind(1, number_of_dex_registers, code_info));
    ASSERT_EQ(18, dex_register_map.GetMachineRegister(0, number_of_dex_registers, code_info));
    ASSERT_EQ(3, dex_register_map.GetMachineRegister(1, number_of_dex_registers, code_info));

    size_t index0 = dex_register_map.GetLocationSetEntryIndex(
        0, number_of_dex_registers, number_of_location_set_entries);
    size_t index1 = dex_register_map.GetLocationSetEntryIndex(
        1, number_of_dex_registers, number_of_location_set_entries);
    ASSERT_EQ(2u, index0);
    ASSERT_EQ(3u, index1);
    DexRegisterLocation location0 = location_set.GetDexRegisterLocation(index0);
    DexRegisterLocation location1 = location_set.GetDexRegisterLocation(index1);
    ASSERT_EQ(Kind::kInRegister, location0.GetKind());
    ASSERT_EQ(Kind::kInFpuRegister, location1.GetKind());
    ASSERT_EQ(Kind::kInRegister, location0.GetInternalKind());
    ASSERT_EQ(Kind::kInFpuRegister, location1.GetInternalKind());
    ASSERT_EQ(18, location0.GetValue());
    ASSERT_EQ(3, location1.GetValue());

    ASSERT_FALSE(stack_map.HasInlineInfo(code_info));
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

  uint32_t number_of_location_set_entries = code_info.GetNumberOfDexRegisterLocationSetEntries();
  ASSERT_EQ(1u, number_of_location_set_entries);
  DexRegisterLocationSet location_set = code_info.GetDexRegisterLocationSet();
  // The Dex register location set contains:
  // - one 5-byte large Dex register location.
  size_t expected_location_set_size = 5u;
  ASSERT_EQ(expected_location_set_size, location_set.Size());

  StackMap stack_map = code_info.GetStackMapAt(0);
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForDexPc(0)));
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForNativePcOffset(64)));
  ASSERT_EQ(0u, stack_map.GetDexPc(code_info));
  ASSERT_EQ(64u, stack_map.GetNativePcOffset(code_info));
  ASSERT_EQ(0x3u, stack_map.GetRegisterMask(code_info));

  ASSERT_TRUE(stack_map.HasDexRegisterMap(code_info));
  DexRegisterMap dex_register_map =
      code_info.GetDexRegisterMapOf(stack_map, number_of_dex_registers);
  ASSERT_FALSE(dex_register_map.IsDexRegisterLive(0));
  ASSERT_TRUE(dex_register_map.IsDexRegisterLive(1));
  ASSERT_EQ(1u, dex_register_map.GetNumberOfLiveDexRegisters(number_of_dex_registers));
  // The Dex register map contains:
  // - one 1-byte live bit mask.
  // No space is allocated for the sole location set entry index, as it
  // is useless.
  size_t expected_dex_register_map_size = 1u + 0u;
  ASSERT_EQ(expected_dex_register_map_size, dex_register_map.Size());

  ASSERT_EQ(Kind::kNone,
            dex_register_map.GetLocationKind(0, number_of_dex_registers, code_info));
  ASSERT_EQ(Kind::kConstant,
            dex_register_map.GetLocationKind(1, number_of_dex_registers, code_info));
  ASSERT_EQ(Kind::kNone,
            dex_register_map.GetLocationInternalKind(0, number_of_dex_registers, code_info));
  ASSERT_EQ(Kind::kConstantLargeValue,
            dex_register_map.GetLocationInternalKind(1, number_of_dex_registers, code_info));
  ASSERT_EQ(-2, dex_register_map.GetConstant(1, number_of_dex_registers, code_info));

  size_t index0 = dex_register_map.GetLocationSetEntryIndex(
      0, number_of_dex_registers, number_of_location_set_entries);
  size_t index1 =  dex_register_map.GetLocationSetEntryIndex(
      1, number_of_dex_registers, number_of_location_set_entries);
  ASSERT_EQ(DexRegisterLocationSet::kNoLocationEntryIndex, index0);
  ASSERT_EQ(0u, index1);
  DexRegisterLocation location0 = location_set.GetDexRegisterLocation(index0);
  DexRegisterLocation location1 = location_set.GetDexRegisterLocation(index1);
  ASSERT_EQ(Kind::kNone, location0.GetKind());
  ASSERT_EQ(Kind::kConstant, location1.GetKind());
  ASSERT_EQ(Kind::kNone, location0.GetInternalKind());
  ASSERT_EQ(Kind::kConstantLargeValue, location1.GetInternalKind());
  ASSERT_EQ(0, location0.GetValue());
  ASSERT_EQ(-2, location1.GetValue());

  ASSERT_FALSE(stack_map.HasInlineInfo(code_info));
}

// Generate a stack map whose dex register offset is
// StackMap::kNoDexRegisterMapSmallEncoding, and ensure we do
// not treat it as kNoDexRegisterMap.
TEST(StackMapTest, DexRegisterMapOffsetOverflow) {
  ArenaPool pool;
  ArenaAllocator arena(&pool);
  StackMapStream stream(&arena);

  ArenaBitVector sp_mask(&arena, 0, false);
  uint32_t number_of_dex_registers = 1024;
  // Create the first stack map (and its Dex register map).
  stream.AddStackMapEntry(0, 64, 0x3, &sp_mask, number_of_dex_registers, 0);
  uint32_t number_of_dex_live_registers_in_dex_register_map_0 = number_of_dex_registers - 8;
  for (uint32_t i = 0; i < number_of_dex_live_registers_in_dex_register_map_0; ++i) {
    // Use two different Dex register locations to populate this map,
    // as using a single value (in the whole CodeInfo object) would
    // make this Dex register mapping data empty (see
    // art::DexRegisterMap::SingleEntrySizeInBits).
    if (i % 2 == 0) {
      stream.AddDexRegisterEntry(i, DexRegisterLocation::Kind::kConstant, 0);  // Short location.
    } else {
      stream.AddDexRegisterEntry(i, DexRegisterLocation::Kind::kConstant, 1);  // Short location.
    }
  }
  // Create the second stack map (and its Dex register map).
  stream.AddStackMapEntry(0, 64, 0x3, &sp_mask, number_of_dex_registers, 0);
  for (uint32_t i = 0; i < number_of_dex_registers; ++i) {
    stream.AddDexRegisterEntry(i, DexRegisterLocation::Kind::kConstant, 0);  // Short location.
  }

  size_t size = stream.ComputeNeededSize();
  void* memory = arena.Alloc(size, kArenaAllocMisc);
  MemoryRegion region(memory, size);
  stream.FillIn(region);

  CodeInfo code_info(region);
  // The location set contains two entries (DexRegisterLocation(kConstant, 0)
  // and DexRegisterLocation(kConstant, 1)), therefore the location set index
  // has a size of 1 bit.
  uint32_t number_of_location_set_entries = code_info.GetNumberOfDexRegisterLocationSetEntries();
  ASSERT_EQ(2u, number_of_location_set_entries);
  ASSERT_EQ(1u, DexRegisterMap::SingleEntrySizeInBits(number_of_location_set_entries));

  // The first Dex register map contains:
  // - a live register bit mask for 1024 registers (that is, 128 bytes of
  //   data); and
  // - Dex register mapping information for 1016 1-bit Dex (live) register
  //   locations (that is, 127 bytes of data).
  // Hence it has a size of 255 bytes, and therefore...
  ASSERT_EQ(128u, DexRegisterMap::GetLiveBitMaskSize(number_of_dex_registers));
  StackMap stack_map0 = code_info.GetStackMapAt(0);
  DexRegisterMap dex_register_map0 =
      code_info.GetDexRegisterMapOf(stack_map0, number_of_dex_registers);
  ASSERT_EQ(127u, dex_register_map0.GetLocationMappingDataSize(number_of_dex_registers,
                                                               number_of_location_set_entries));
  ASSERT_EQ(255u, dex_register_map0.Size());

  StackMap stack_map1 = code_info.GetStackMapAt(1);
  ASSERT_TRUE(stack_map1.HasDexRegisterMap(code_info));
  // ...the offset of the second Dex register map (relative to the
  // beginning of the Dex register maps region) is 255 (i.e.,
  // kNoDexRegisterMapSmallEncoding).
  ASSERT_NE(StackMap::kNoDexRegisterMap, stack_map1.GetDexRegisterMapOffset(code_info));
  ASSERT_EQ(StackMap::kNoDexRegisterMapSmallEncoding,
            stack_map1.GetDexRegisterMapOffset(code_info));
}

TEST(StackMapTest, TestShareDexRegisterMap) {
  ArenaPool pool;
  ArenaAllocator arena(&pool);
  StackMapStream stream(&arena);

  ArenaBitVector sp_mask(&arena, 0, false);
  uint32_t number_of_dex_registers = 2;
  // First stack map.
  stream.AddStackMapEntry(0, 64, 0x3, &sp_mask, number_of_dex_registers, 0);
  stream.AddDexRegisterEntry(0, DexRegisterLocation::Kind::kInRegister, 0);  // Short location.
  stream.AddDexRegisterEntry(1, DexRegisterLocation::Kind::kConstant, -2);   // Large location.
  // Second stack map, which should share the same dex register map.
  stream.AddStackMapEntry(0, 64, 0x3, &sp_mask, number_of_dex_registers, 0);
  stream.AddDexRegisterEntry(0, DexRegisterLocation::Kind::kInRegister, 0);  // Short location.
  stream.AddDexRegisterEntry(1, DexRegisterLocation::Kind::kConstant, -2);   // Large location.
  // Third stack map (doesn't share the dex register map).
  stream.AddStackMapEntry(0, 64, 0x3, &sp_mask, number_of_dex_registers, 0);
  stream.AddDexRegisterEntry(0, DexRegisterLocation::Kind::kInRegister, 2);  // Short location.
  stream.AddDexRegisterEntry(1, DexRegisterLocation::Kind::kConstant, -2);   // Large location.

  size_t size = stream.ComputeNeededSize();
  void* memory = arena.Alloc(size, kArenaAllocMisc);
  MemoryRegion region(memory, size);
  stream.FillIn(region);

  CodeInfo ci(region);
  // Verify first stack map.
  StackMap sm0 = ci.GetStackMapAt(0);
  DexRegisterMap dex_registers0 = ci.GetDexRegisterMapOf(sm0, number_of_dex_registers);
  ASSERT_EQ(0, dex_registers0.GetMachineRegister(0, number_of_dex_registers, ci));
  ASSERT_EQ(-2, dex_registers0.GetConstant(1, number_of_dex_registers, ci));

  // Verify second stack map.
  StackMap sm1 = ci.GetStackMapAt(1);
  DexRegisterMap dex_registers1 = ci.GetDexRegisterMapOf(sm1, number_of_dex_registers);
  ASSERT_EQ(0, dex_registers1.GetMachineRegister(0, number_of_dex_registers, ci));
  ASSERT_EQ(-2, dex_registers1.GetConstant(1, number_of_dex_registers, ci));

  // Verify third stack map.
  StackMap sm2 = ci.GetStackMapAt(2);
  DexRegisterMap dex_registers2 = ci.GetDexRegisterMapOf(sm2, number_of_dex_registers);
  ASSERT_EQ(2, dex_registers2.GetMachineRegister(0, number_of_dex_registers, ci));
  ASSERT_EQ(-2, dex_registers2.GetConstant(1, number_of_dex_registers, ci));

  // Verify dex register map offsets.
  ASSERT_EQ(sm0.GetDexRegisterMapOffset(ci), sm1.GetDexRegisterMapOffset(ci));
  ASSERT_NE(sm0.GetDexRegisterMapOffset(ci), sm2.GetDexRegisterMapOffset(ci));
  ASSERT_NE(sm1.GetDexRegisterMapOffset(ci), sm2.GetDexRegisterMapOffset(ci));
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

  uint32_t number_of_location_set_entries = code_info.GetNumberOfDexRegisterLocationSetEntries();
  ASSERT_EQ(0u, number_of_location_set_entries);
  DexRegisterLocationSet location_set = code_info.GetDexRegisterLocationSet();
  ASSERT_EQ(0u, location_set.Size());

  StackMap stack_map = code_info.GetStackMapAt(0);
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForDexPc(0)));
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForNativePcOffset(64)));
  ASSERT_EQ(0u, stack_map.GetDexPc(code_info));
  ASSERT_EQ(64u, stack_map.GetNativePcOffset(code_info));
  ASSERT_EQ(0x3u, stack_map.GetRegisterMask(code_info));

  ASSERT_FALSE(stack_map.HasDexRegisterMap(code_info));
  ASSERT_FALSE(stack_map.HasInlineInfo(code_info));
}

}  // namespace art
