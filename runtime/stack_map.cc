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

#include "stack_map.h"

#include <stdint.h>

#include "indenter.h"

namespace art {

constexpr size_t DexRegisterLocationCatalog::kNoLocationEntryIndex;
constexpr uint32_t StackMap::kNoDexRegisterMap;
constexpr uint32_t StackMap::kNoInlineInfo;

DexRegisterLocation::Kind DexRegisterMap::GetLocationInternalKind(uint16_t dex_register_number,
                                                                  uint16_t number_of_dex_registers,
                                                                  const CodeInfo& code_info,
                                                                  const StackMapEncoding& enc) const {
  DexRegisterLocationCatalog dex_register_location_catalog =
      code_info.GetDexRegisterLocationCatalog(enc);
  size_t location_catalog_entry_index = GetLocationCatalogEntryIndex(
      dex_register_number,
      number_of_dex_registers,
      code_info.GetNumberOfDexRegisterLocationCatalogEntries());
  return dex_register_location_catalog.GetLocationInternalKind(location_catalog_entry_index);
}

DexRegisterLocation DexRegisterMap::GetDexRegisterLocation(uint16_t dex_register_number,
                                                           uint16_t number_of_dex_registers,
                                                           const CodeInfo& code_info,
                                                           const StackMapEncoding& enc) const {
  DexRegisterLocationCatalog dex_register_location_catalog =
      code_info.GetDexRegisterLocationCatalog(enc);
  size_t location_catalog_entry_index = GetLocationCatalogEntryIndex(
      dex_register_number,
      number_of_dex_registers,
      code_info.GetNumberOfDexRegisterLocationCatalogEntries());
  return dex_register_location_catalog.GetDexRegisterLocation(location_catalog_entry_index);
}

// Loads `number_of_bytes` at the given `offset` and assemble a uint32_t. If `check_max` is true,
// this method converts a maximum value of size `number_of_bytes` into a uint32_t 0xFFFFFFFF.
static uint32_t LoadAt(MemoryRegion region,
                       size_t number_of_bytes,
                       size_t offset,
                       bool check_max = false) {
  if (number_of_bytes == 0u) {
    DCHECK(!check_max);
    return 0;
  } else if (number_of_bytes == 1u) {
    uint8_t value = region.LoadUnaligned<uint8_t>(offset);
    if (check_max && value == 0xFF) {
      return -1;
    } else {
      return value;
    }
  } else if (number_of_bytes == 2u) {
    uint16_t value = region.LoadUnaligned<uint16_t>(offset);
    if (check_max && value == 0xFFFF) {
      return -1;
    } else {
      return value;
    }
  } else if (number_of_bytes == 3u) {
    uint16_t low = region.LoadUnaligned<uint16_t>(offset);
    uint16_t high = region.LoadUnaligned<uint8_t>(offset + sizeof(uint16_t));
    uint32_t value = (high << 16) + low;
    if (check_max && value == 0xFFFFFF) {
      return -1;
    } else {
      return value;
    }
  } else {
    DCHECK_EQ(number_of_bytes, 4u);
    return region.LoadUnaligned<uint32_t>(offset);
  }
}

static void StoreAt(MemoryRegion region, size_t number_of_bytes, size_t offset, uint32_t value) {
  if (number_of_bytes == 0u) {
    DCHECK_EQ(value, 0u);
  } else if (number_of_bytes == 1u) {
    region.StoreUnaligned<uint8_t>(offset, value);
  } else if (number_of_bytes == 2u) {
    region.StoreUnaligned<uint16_t>(offset, value);
  } else if (number_of_bytes == 3u) {
    region.StoreUnaligned<uint16_t>(offset, Low16Bits(value));
    region.StoreUnaligned<uint8_t>(offset + sizeof(uint16_t), High16Bits(value));
  } else {
    region.StoreUnaligned<uint32_t>(offset, value);
    DCHECK_EQ(number_of_bytes, 4u);
  }
}

uint32_t StackMap::GetDexPc(const StackMapEncoding& enc) const {
  return LoadAt(region_, enc.NumberOfBytesForDexPc(), enc.ComputeStackMapDexPcOffset());
}

void StackMap::SetDexPc(const StackMapEncoding& enc, uint32_t dex_pc) {
  StoreAt(region_, enc.NumberOfBytesForDexPc(), enc.ComputeStackMapDexPcOffset(), dex_pc);
}

uint32_t StackMap::GetNativePcOffset(const StackMapEncoding& enc) const {
  return LoadAt(region_, enc.NumberOfBytesForNativePc(), enc.ComputeStackMapNativePcOffset());
}

void StackMap::SetNativePcOffset(const StackMapEncoding& enc, uint32_t native_pc_offset) {
  StoreAt(region_, enc.NumberOfBytesForNativePc(), enc.ComputeStackMapNativePcOffset(), native_pc_offset);
}

uint32_t StackMap::GetDexRegisterMapOffset(const StackMapEncoding& enc) const {
  return LoadAt(region_,
                enc.NumberOfBytesForDexRegisterMap(),
                enc.ComputeStackMapDexRegisterMapOffset(),
                /* check_max */ true);
}

void StackMap::SetDexRegisterMapOffset(const StackMapEncoding& enc, uint32_t offset) {
  StoreAt(region_,
          enc.NumberOfBytesForDexRegisterMap(),
          enc.ComputeStackMapDexRegisterMapOffset(),
          offset);
}

uint32_t StackMap::GetInlineDescriptorOffset(const StackMapEncoding& enc) const {
  if (!enc.HasInlineInfo()) return kNoInlineInfo;
  return LoadAt(region_,
                enc.NumberOfBytesForInlineInfo(),
                enc.ComputeStackMapInlineInfoOffset(),
                /* check_max */ true);
}

void StackMap::SetInlineDescriptorOffset(const StackMapEncoding& enc, uint32_t offset) {
  DCHECK(enc.HasInlineInfo());
  StoreAt(region_,
          enc.NumberOfBytesForInlineInfo(),
          enc.ComputeStackMapInlineInfoOffset(),
          offset);
}

uint32_t StackMap::GetRegisterMask(const StackMapEncoding& enc) const {
  return LoadAt(region_,
                enc.NumberOfBytesForRegisterMask(),
                enc.ComputeStackMapRegisterMaskOffset());
}

void StackMap::SetRegisterMask(const StackMapEncoding& enc, uint32_t mask) {
  StoreAt(region_,
          enc.NumberOfBytesForRegisterMask(),
          enc.ComputeStackMapRegisterMaskOffset(),
          mask);
}

size_t StackMap::ComputeStackMapSizeInternal(size_t stack_mask_size,
                                             size_t number_of_bytes_for_inline_info,
                                             size_t number_of_bytes_for_dex_map,
                                             size_t number_of_bytes_for_dex_pc,
                                             size_t number_of_bytes_for_native_pc,
                                             size_t number_of_bytes_for_register_mask) {
  return stack_mask_size
      + number_of_bytes_for_inline_info
      + number_of_bytes_for_dex_map
      + number_of_bytes_for_dex_pc
      + number_of_bytes_for_native_pc
      + number_of_bytes_for_register_mask;
}

size_t StackMap::ComputeStackMapSize(size_t stack_mask_size,
                                     size_t inline_info_size,
                                     size_t dex_register_map_size,
                                     size_t dex_pc_max,
                                     size_t native_pc_max,
                                     size_t register_mask_max) {
  StackMapEncoding encoding = StackMapEncoding::ComputeFromSizes(stack_mask_size,
                                                         inline_info_size,
                                                         dex_register_map_size + 1,
                                                         dex_pc_max,
                                                         native_pc_max,
                                                         register_mask_max);
  return encoding.StackMapSize();
}

MemoryRegion StackMap::GetStackMask(const StackMapEncoding& enc) const {
  return region_.Subregion(enc.ComputeStackMapStackMaskOffset(), enc.GetStackMaskSize());
}

static void DumpRegisterMapping(std::ostream& os,
                                size_t dex_register_num,
                                DexRegisterLocation location,
                                const std::string& prefix = "v",
                                const std::string& suffix = "") {
  Indenter indent_filter(os.rdbuf(), kIndentChar, kIndentBy1Count);
  std::ostream indented_os(&indent_filter);
  indented_os << prefix << dex_register_num << ": "
              << DexRegisterLocation::PrettyDescriptor(location.GetInternalKind())
              << " (" << location.GetValue() << ")" << suffix << '\n';
}

void CodeInfo::DumpStackMap(std::ostream& os,
                            size_t stack_map_num,
                            uint16_t number_of_dex_registers) const {
  StackMapEncoding encoding = ExtractEncoding();
  StackMap stack_map = GetStackMapAt(stack_map_num, encoding);
  DumpStackMapHeader(os, stack_map_num);
  if (stack_map.HasDexRegisterMap(encoding)) {
    DexRegisterMap dex_register_map =
        GetDexRegisterMapOf(stack_map, number_of_dex_registers, encoding);
    dex_register_map.Dump(os, *this, number_of_dex_registers);
  }
}

void CodeInfo::DumpStackMapHeader(std::ostream& os, size_t stack_map_num) const {
  StackMapEncoding encoding = ExtractEncoding();
  StackMap stack_map = GetStackMapAt(stack_map_num, encoding);
  Indenter indent_filter(os.rdbuf(), kIndentChar, kIndentBy1Count);
  std::ostream indented_os(&indent_filter);
  indented_os << "StackMap " << stack_map_num
              << std::hex
              << " (dex_pc=0x" << stack_map.GetDexPc(encoding)
              << ", native_pc_offset=0x" << stack_map.GetNativePcOffset(encoding)
              << ", dex_register_map_offset=0x" << stack_map.GetDexRegisterMapOffset(encoding)
              << ", inline_info_offset=0x" << stack_map.GetInlineDescriptorOffset(encoding)
              << ", register_mask=0x" << stack_map.GetRegisterMask(encoding)
              << std::dec
              << ", stack_mask=0b";
  MemoryRegion stack_mask = stack_map.GetStackMask(encoding);
  for (size_t i = 0, e = stack_mask.size_in_bits(); i < e; ++i) {
    indented_os << stack_mask.LoadBit(e - i - 1);
  }
  indented_os << ")\n";
};

void CodeInfo::Dump(std::ostream& os,
                    uint16_t number_of_dex_registers,
                    bool dump_stack_maps) const {
  StackMapEncoding enc = ExtractEncoding();
  uint32_t code_info_size = GetOverallSize();
  size_t number_of_stack_maps = GetNumberOfStackMaps();
  Indenter indent_filter(os.rdbuf(), kIndentChar, kIndentBy1Count);
  std::ostream indented_os(&indent_filter);
  indented_os << "Optimized CodeInfo (size=" << code_info_size
              << ", number_of_dex_registers=" << number_of_dex_registers
              << ", number_of_stack_maps=" << number_of_stack_maps
              << ", has_inline_info=" << enc.HasInlineInfo()
              << ", number_of_bytes_for_inline_info=" << enc.NumberOfBytesForInlineInfo()
              << ", number_of_bytes_for_dex_register_map=" << enc.NumberOfBytesForDexRegisterMap()
              << ", number_of_bytes_for_dex_pc=" << enc.NumberOfBytesForDexPc()
              << ", number_of_bytes_for_native_pc=" << enc.NumberOfBytesForNativePc()
              << ", number_of_bytes_for_register_mask=" << enc.NumberOfBytesForRegisterMask()
              << ")\n";
  // Display the Dex register location catalog.
  GetDexRegisterLocationCatalog(enc).Dump(indented_os, *this);
  // Display stack maps along with (live) Dex register maps.
  if (dump_stack_maps) {
    for (size_t i = 0; i < number_of_stack_maps; ++i) {
      DumpStackMap(indented_os, i, number_of_dex_registers);
    }
  }
  // TODO: Dump the stack map's inline information? We need to know more from the caller:
  //       we need to know the number of dex registers for each inlined method.
}

void DexRegisterLocationCatalog::Dump(std::ostream& os, const CodeInfo& code_info) {
  StackMapEncoding encoding = code_info.ExtractEncoding();
  size_t number_of_location_catalog_entries =
      code_info.GetNumberOfDexRegisterLocationCatalogEntries();
  size_t location_catalog_size_in_bytes = code_info.GetDexRegisterLocationCatalogSize(encoding);
  Indenter indent_filter(os.rdbuf(), kIndentChar, kIndentBy1Count);
  std::ostream indented_os(&indent_filter);
  indented_os
      << "DexRegisterLocationCatalog (number_of_entries=" << number_of_location_catalog_entries
      << ", size_in_bytes=" << location_catalog_size_in_bytes << ")\n";
  for (size_t i = 0; i < number_of_location_catalog_entries; ++i) {
    DexRegisterLocation location = GetDexRegisterLocation(i);
    DumpRegisterMapping(indented_os, i, location, "entry ");
  }
}

void DexRegisterMap::Dump(std::ostream& os,
                          const CodeInfo& code_info,
                          uint16_t number_of_dex_registers) const {
  StackMapEncoding encoding = code_info.ExtractEncoding();
  size_t number_of_location_catalog_entries =
      code_info.GetNumberOfDexRegisterLocationCatalogEntries();
  Indenter indent_filter(os.rdbuf(), kIndentChar, kIndentBy1Count);
  std::ostream indented_os(&indent_filter);
  // TODO: Display the bit mask of live Dex registers.
  for (size_t j = 0; j < number_of_dex_registers; ++j) {
    if (IsDexRegisterLive(j)) {
      size_t location_catalog_entry_index = GetLocationCatalogEntryIndex(
          j, number_of_dex_registers, number_of_location_catalog_entries);
      DexRegisterLocation location = GetDexRegisterLocation(j,
                                                            number_of_dex_registers,
                                                            code_info,
                                                            encoding);
      DumpRegisterMapping(
          indented_os, j, location, "v",
          "\t[entry " + std::to_string(static_cast<int>(location_catalog_entry_index)) + "]");
    }
  }
}

void InlineInfo::Dump(std::ostream& os,
                      const CodeInfo& code_info,
                      uint16_t number_of_dex_registers[]) const {
  Indenter indent_filter(os.rdbuf(), kIndentChar, kIndentBy1Count);
  std::ostream indented_os(&indent_filter);
  indented_os << "InlineInfo with depth " << static_cast<uint32_t>(GetDepth()) << "\n";

  for (size_t i = 0; i < GetDepth(); ++i) {
    indented_os << " At depth " << i
                << std::hex
                << " (dex_pc=0x" << GetDexPcAtDepth(i)
                << ", method_index=0x" << GetMethodIndexAtDepth(i)
                << ")\n";
    if (HasDexRegisterMapAtDepth(i)) {
      StackMapEncoding encoding = code_info.ExtractEncoding();
      DexRegisterMap dex_register_map =
          code_info.GetDexRegisterMapAtDepth(i, *this, number_of_dex_registers[i], encoding);
      dex_register_map.Dump(indented_os, code_info, number_of_dex_registers[i]);
    }
  }
}

}  // namespace art
