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

#ifndef ART_RUNTIME_STACK_MAP_H_
#define ART_RUNTIME_STACK_MAP_H_

#include "base/bit_vector.h"
#include "memory_region.h"
#include "utils.h"

namespace art {

/**
 * Classes in the following file are wrapper on stack map information backed
 * by a MemoryRegion. As such they read and write to the region, they don't have
 * their own fields.
 */

/**
 * Inline information for a specific PC. The information is of the form:
 * [inlining_depth, [method_dex reference]+]
 */
class InlineInfo {
 public:
  explicit InlineInfo(MemoryRegion region) : region_(region) {}

  uint8_t GetDepth() const {
    return region_.Load<uint8_t>(kDepthOffset);
  }

  void SetDepth(uint8_t depth) {
    region_.Store<uint8_t>(kDepthOffset, depth);
  }

  uint32_t GetMethodReferenceIndexAtDepth(uint8_t depth) const {
    return region_.Load<uint32_t>(kFixedSize + depth * SingleEntrySize());
  }

  void SetMethodReferenceIndexAtDepth(uint8_t depth, uint32_t index) {
    region_.Store<uint32_t>(kFixedSize + depth * SingleEntrySize(), index);
  }

  static size_t SingleEntrySize() {
    return sizeof(uint32_t);
  }

 private:
  // TODO: Instead of plain types such as "uint8_t", introduce
  // typedefs (and document the memory layout of InlineInfo).
  static constexpr int kDepthOffset = 0;
  static constexpr int kFixedSize = kDepthOffset + sizeof(uint8_t);

  MemoryRegion region_;

  friend class CodeInfo;
  friend class StackMap;
  friend class StackMapStream;
};


// Strategies used to encode Dex register maps in stack maps.
enum DexRegisterMapEncoding {
  // The initial encoding, where a Dex register map is encoded as a
  // list of [location_kind, register_value] pairs.
  kDexRegisterLocationList,

  // Encoding based on a dictionary of unique [location_kind, register_value]
  // pairs per method.
  kDexRegisterLocationDictionary
};

// The chosen strategy to encode Dex register maps in stack maps.
// TODO: Rename as kDexRegisterMapEncoding, as it is a constant.
static constexpr DexRegisterMapEncoding dex_register_map_encoding =
    kDexRegisterLocationDictionary;


/**
 * Information on dex register values for a specific PC. The information is
 * of the form:
 * [location_kind, register_value]+.
 *
 * The location_kind for a Dex register can either be:
 * - kConstant: register_value holds the constant,
 * - kStack: register_value holds the stack offset,
 * - kRegister: register_value holds the physical register number.
 * - kFpuRegister: register_value holds the physical register number.
 * - kNone: the register has no location yet, meaning it has not been set.
 */
class DexRegisterMap {
 public:
  explicit DexRegisterMap(MemoryRegion region) : region_(region) {}

  enum LocationKind : uint8_t {
    kNone,
    kInStack,
    kInRegister,
    kInFpuRegister,
    kConstant
  };
  static_assert(
      sizeof(art::DexRegisterMap::LocationKind) == 1u,
      "art::DexRegisterMap::LocationKind has a size different from one byte.");

  static const char* PrettyDescriptor(LocationKind kind) {
    switch (kind) {
      case kNone:
        return "none";
      case kInStack:
        return "in stack";
      case kInRegister:
        return "in register";
      case kInFpuRegister:
        return "in fpu register";
      case kConstant:
        return "as constant";
      default:
        LOG(FATAL) << "Invalid location kind " << static_cast<int>(kind);
        return nullptr;
    }
  }

  LocationKind GetLocationKind(uint16_t register_index) const {
    return region_.Load<LocationKind>(
        kFixedSize + register_index * SingleEntrySize());
  }

  void SetRegisterInfo(uint16_t register_index, LocationKind kind, int32_t value) {
    size_t entry = kFixedSize + register_index * SingleEntrySize();
    region_.Store<LocationKind>(entry, kind);
    region_.Store<int32_t>(entry + sizeof(LocationKind), value);
  }

  int32_t GetValue(uint16_t register_index) const {
    return region_.Load<int32_t>(
        kFixedSize + sizeof(LocationKind) + register_index * SingleEntrySize());
  }

  static size_t SingleEntrySize() {
    return sizeof(LocationKind) + sizeof(int32_t);
  }

  size_t Size() const {
    return region_.size();
  }

  // For testing purpose only.
  size_t ComputeSize(size_t number_of_dex_registers) const {
    size_t size = kFixedSize + number_of_dex_registers * SingleEntrySize();
    DCHECK_EQ(size, Size());
    return size;
  }

  static constexpr int kFixedSize = 0;

 private:
  MemoryRegion region_;
};

// Store unique Dex register locations ([location_kind, register_value]
// pairs) used in a method.
//
// We reuse the DexRegisterMap container to implement DexRegisterDictionary.
typedef DexRegisterMap DexRegisterDictionary;

// Map a stack maps's Dex register to a location entry in a
// DexRegisterDictionary.
class DexRegisterTable {
 public:
  explicit DexRegisterTable(MemoryRegion region) : region_(region) {}

  // The type used to refer to indices of a DexRegisterDictionary and
  // stored in a DexRegisterMap.  We currently use 8-bit indices for
  // space effiency, but might need to use larger (16- or 32-bit)
  // values for methods containing more than 256 unique Dex locations.
  typedef uint8_t EntryIndex;

  static size_t SingleEntrySize() {
    return sizeof(EntryIndex);
  }

  size_t Size() const {
    return region_.size();
  }

  // For testing purpose only.
  size_t ComputeSize(size_t number_of_dex_registers) const {
    size_t size = kFixedSize + number_of_dex_registers * SingleEntrySize();
    DCHECK_EQ(size, Size());
    return size;
  }

  // Get the index of the entry in the Dex register dictionary
  // corresponding to `register_index`.
  EntryIndex GetEntryIndex(uint16_t register_index) const {
    return region_.Load<EntryIndex>(kFixedSize + register_index * SingleEntrySize());
  }

  // Set the index of the entry for the Dex register dictionary
  // corresponding to `register_index` to `entry_index`.
  void SetEntryIndex(uint16_t register_index, EntryIndex entry_index) const {
    region_.Store<EntryIndex>(kFixedSize + register_index * SingleEntrySize(),
                              entry_index);
  }

  static constexpr int kFixedSize = 0;

 private:
  MemoryRegion region_;
};

/**
 * A Stack Map holds compilation information for a specific PC necessary for:
 * - Mapping it to a dex PC,
 * - Knowing which stack entries are objects,
 * - Knowing which registers hold objects,
 * - Knowing the inlining information,
 * - Knowing the values of dex registers.
 *
 * The information is of the form:
 * [dex_pc, native_pc_offset, dex_register_map_offset, inlining_info_offset, register_mask, stack_mask].
 *
 * Note that register_mask is fixed size, but stack_mask is variable size, depending on the
 * stack size of a method.
 */
class StackMap {
 public:
  explicit StackMap(MemoryRegion region) : region_(region) {}

  uint32_t GetDexPc() const {
    return region_.Load<uint32_t>(kDexPcOffset);
  }

  void SetDexPc(uint32_t dex_pc) {
    region_.Store<uint32_t>(kDexPcOffset, dex_pc);
  }

  uint32_t GetNativePcOffset() const {
    return region_.Load<uint32_t>(kNativePcOffsetOffset);
  }

  void SetNativePcOffset(uint32_t native_pc_offset) {
    region_.Store<uint32_t>(kNativePcOffsetOffset, native_pc_offset);
  }

  uint32_t GetDexRegisterMapOffset() const {
    return region_.Load<uint32_t>(kDexRegisterMapOffsetOffset);
  }

  void SetDexRegisterMapOffset(uint32_t offset) {
    region_.Store<uint32_t>(kDexRegisterMapOffsetOffset, offset);
  }

  uint32_t GetInlineDescriptorOffset() const {
    return region_.Load<uint32_t>(kInlineDescriptorOffsetOffset);
  }

  void SetInlineDescriptorOffset(uint32_t offset) {
    region_.Store<uint32_t>(kInlineDescriptorOffsetOffset, offset);
  }

  uint32_t GetRegisterMask() const {
    return region_.Load<uint32_t>(kRegisterMaskOffset);
  }

  void SetRegisterMask(uint32_t mask) {
    region_.Store<uint32_t>(kRegisterMaskOffset, mask);
  }

  MemoryRegion GetStackMask() const {
    return region_.Subregion(kStackMaskOffset, StackMaskSize());
  }

  void SetStackMask(const BitVector& sp_map) {
    MemoryRegion region = GetStackMask();
    for (size_t i = 0; i < region.size_in_bits(); i++) {
      region.StoreBit(i, sp_map.IsBitSet(i));
    }
  }

  bool HasDexRegisterMap() const {
    return GetDexRegisterMapOffset() != kNoDexRegisterMap;
  }

  bool HasInlineInfo() const {
    return GetInlineDescriptorOffset() != kNoInlineInfo;
  }

  bool Equals(const StackMap& other) const {
    return region_.pointer() == other.region_.pointer()
       && region_.size() == other.region_.size();
  }

  static size_t ComputeAlignedStackMapSize(size_t stack_map_size) {
    // On ARM, the stack maps must be 4-byte aligned.
    return RoundUp(StackMap::kFixedSize + stack_map_size, 4);
  }

  // Special (invalid) offset for the DexRegisterMapOffset field meaning
  // that there is no Dex register map for this stack map.
  static constexpr uint32_t kNoDexRegisterMap = -1;

  // Special (invalid) offset for the InlineDescriptorOffset field meaning
  // that there is no inline info for this stack map.
  static constexpr uint32_t kNoInlineInfo = -1;

 private:
  // TODO: Instead of plain types such as "uint32_t", introduce
  // typedefs (and document the memory layout of StackMap).
  static constexpr int kDexPcOffset = 0;
  static constexpr int kNativePcOffsetOffset = kDexPcOffset + sizeof(uint32_t);
  static constexpr int kDexRegisterMapOffsetOffset = kNativePcOffsetOffset + sizeof(uint32_t);
  static constexpr int kInlineDescriptorOffsetOffset =
      kDexRegisterMapOffsetOffset + sizeof(uint32_t);
  static constexpr int kRegisterMaskOffset = kInlineDescriptorOffsetOffset + sizeof(uint32_t);
  static constexpr int kFixedSize = kRegisterMaskOffset + sizeof(uint32_t);
  static constexpr int kStackMaskOffset = kFixedSize;

  size_t StackMaskSize() const { return region_.size() - kFixedSize; }

  MemoryRegion region_;

  friend class CodeInfo;
  friend class StackMapStream;
};


/**
 * Wrapper around all compiler information collected for a method.
 * When using the list-based Dex register location encoding, the
 * information is of the form:
 * [overall_size, number_of_stack_maps, stack_mask_size, StackMap+, DexRegisterInfo+, InlineInfo*].
 * When using the dictionary-based Dex register location encoding, the
 * information is of the form:
 * [overall_size, number_of_dictionary_entries, number_of_stack_maps, stack_mask_size, DexRegisterDictionary+, StackMap+, DexRegisterInfo+, InlineInfo*].
 */
class CodeInfo {
 public:
  explicit CodeInfo(MemoryRegion region) : region_(region) {}

  explicit CodeInfo(const void* data) {
    uint32_t size = reinterpret_cast<const uint32_t*>(data)[0];
    region_ = MemoryRegion(const_cast<void*>(data), size);
  }

  uint32_t GetDexRegisterDictionaryOffset() const {
    // The Dex register dictionary starts just after the fixed-size
    // part of the CodeItem object.
    return kFixedSize;
  }

  // Only meaningful when the encoding based on a dictionary of unique
  // [location_kind, register_value] pairs per method is used.
  DexRegisterDictionary GetDexRegisterDictionary() const {
    DCHECK(dex_register_map_encoding == kDexRegisterLocationDictionary);
    return DexRegisterDictionary(region_.Subregion(
        GetDexRegisterDictionaryOffset(),
        GetDexRegisterDictionarySize()));
  }

  StackMap GetStackMapAt(size_t i) const {
    size_t size = StackMapSize();
    return StackMap(GetStackMaps().Subregion(i * size, size));
  }

  uint32_t GetOverallSize() const {
    return region_.Load<uint32_t>(kOverallSizeOffset);
  }

  void SetOverallSize(uint32_t size) {
    region_.Store<uint32_t>(kOverallSizeOffset, size);
  }

  // Only meaningful when the encoding based on a dictionary of unique
  // [location_kind, register_value] pairs per method is used.
  uint32_t GetNumberOfDexRegisterDictionaryEntries() const {
    DCHECK(dex_register_map_encoding == kDexRegisterLocationDictionary);
    return region_.Load<uint32_t>(kNumberOfDexRegisterDictionaryEntriesOffset);
  }

  // Only meaningful when the encoding based on a dictionary of unique
  // [location_kind, register_value] pairs per method is used.
  void SetNumberOfDexRegisterDictionaryEntries(uint32_t num_entries) {
    DCHECK(dex_register_map_encoding == kDexRegisterLocationDictionary);
    region_.Store<uint32_t>(kNumberOfDexRegisterDictionaryEntriesOffset, num_entries);
  }

  // Only meaningful when the encoding based on a dictionary of unique
  // [location_kind, register_value] pairs per method is used.
  uint32_t GetDexRegisterDictionarySize() const {
    DCHECK(dex_register_map_encoding == kDexRegisterLocationDictionary);
    return DexRegisterDictionary::kFixedSize
        + GetNumberOfDexRegisterDictionaryEntries() * DexRegisterDictionary::SingleEntrySize();
  }

  uint32_t GetStackMaskSize() const {
    return region_.Load<uint32_t>(kStackMaskSizeOffset);
  }

  void SetStackMaskSize(uint32_t size) {
    region_.Store<uint32_t>(kStackMaskSizeOffset, size);
  }

  size_t GetNumberOfStackMaps() const {
    return region_.Load<uint32_t>(kNumberOfStackMapsOffset);
  }

  void SetNumberOfStackMaps(uint32_t number_of_stack_maps) {
    region_.Store<uint32_t>(kNumberOfStackMapsOffset, number_of_stack_maps);
  }

  size_t StackMapSize() const {
    return StackMap::ComputeAlignedStackMapSize(GetStackMaskSize());
  }

  uint32_t GetStackMapsOffset() const {
    switch (dex_register_map_encoding) {
      case kDexRegisterLocationList:
        return kFixedSize;

      case kDexRegisterLocationDictionary:
        return kFixedSize + GetDexRegisterDictionarySize();
    }
  }

  // Only meaningful when the encoding based on a list of
  // [location_kind, register_value] pairs is used.
  DexRegisterMap GetDexRegisterMapOf(StackMap stack_map,
                                     uint32_t number_of_dex_registers) const {
    DCHECK(dex_register_map_encoding == kDexRegisterLocationList);
    DCHECK(stack_map.HasDexRegisterMap());
    uint32_t offset = stack_map.GetDexRegisterMapOffset();
    return DexRegisterMap(region_.Subregion(offset,
        DexRegisterMap::kFixedSize
        + number_of_dex_registers * DexRegisterMap::SingleEntrySize()));
  }

  // Only meaningful when the encoding based on a dictionary of unique
  // [location_kind, register_value] pairs per method is used.
  DexRegisterTable GetDexRegisterTableOf(StackMap stack_map,
                                         uint32_t number_of_dex_registers) const {
    DCHECK(dex_register_map_encoding == kDexRegisterLocationDictionary);
    DCHECK(stack_map.HasDexRegisterMap());
    // Dex register tables are located at the same position as Dex
    // register maps.
    uint32_t offset = stack_map.GetDexRegisterMapOffset();
    return DexRegisterTable(region_.Subregion(offset,
        DexRegisterTable::kFixedSize
        + number_of_dex_registers * DexRegisterTable::SingleEntrySize()));
  }

  InlineInfo GetInlineInfoOf(StackMap stack_map) const {
    DCHECK(stack_map.HasInlineInfo());
    uint32_t offset = stack_map.GetInlineDescriptorOffset();
    uint8_t depth = region_.Load<uint8_t>(offset);
    return InlineInfo(region_.Subregion(offset,
        InlineInfo::kFixedSize + depth * InlineInfo::SingleEntrySize()));
  }

  StackMap GetStackMapForDexPc(uint32_t dex_pc) const {
    for (size_t i = 0, e = GetNumberOfStackMaps(); i < e; ++i) {
      StackMap stack_map = GetStackMapAt(i);
      if (stack_map.GetDexPc() == dex_pc) {
        return stack_map;
      }
    }
    LOG(FATAL) << "Unreachable";
    UNREACHABLE();
  }

  StackMap GetStackMapForNativePcOffset(uint32_t native_pc_offset) const {
    // TODO: stack maps are sorted by native pc, we can do a binary search.
    for (size_t i = 0, e = GetNumberOfStackMaps(); i < e; ++i) {
      StackMap stack_map = GetStackMapAt(i);
      if (stack_map.GetNativePcOffset() == native_pc_offset) {
        return stack_map;
      }
    }
    LOG(FATAL) << "Unreachable";
    UNREACHABLE();
  }

 private:
  // TODO: Instead of plain types such as "uint32_t", introduce
  // typedefs (and document the memory layout of CodeInfo).
  static constexpr int kOverallSizeOffset = 0;
  // The NumberofDexRegisterDictionaryEntries field is present only when the
  // location dictionary encoding is used.
  static constexpr int kNumberOfDexRegisterDictionaryEntriesOffset =
                  kOverallSizeOffset
                  + (dex_register_map_encoding == kDexRegisterLocationDictionary
                     ? sizeof(uint32_t)
                     : 0u);
  static constexpr int kNumberOfStackMapsOffset = kNumberOfDexRegisterDictionaryEntriesOffset + sizeof(uint32_t);
  static constexpr int kStackMaskSizeOffset = kNumberOfStackMapsOffset + sizeof(uint32_t);
  static constexpr int kFixedSize = kStackMaskSizeOffset + sizeof(uint32_t);

  MemoryRegion GetStackMaps() const {
    return region_.size() == 0
        ? MemoryRegion()
        : region_.Subregion(GetStackMapsOffset(),
                            StackMapSize() * GetNumberOfStackMaps());
  }

  MemoryRegion region_;
  friend class StackMapStream;
};

}  // namespace art

#endif  // ART_RUNTIME_STACK_MAP_H_
