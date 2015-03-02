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

// Size of a frame slot, in bytes.  This constant is a signed value,
// to please the compiler in arithmetic operations involving int32_t
// (signed) values.
static ssize_t constexpr kFrameSlotSize = 4;

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

  // List-based encoding (similar to kDexRegisterLocationList) with
  // compressed location information.
  kDexRegisterCompressedLocationList
};

// The chosen strategy to encode Dex register maps in stack maps.
// TODO: Rename as kDexRegisterMapEncoding, as it is a constant.
static constexpr DexRegisterMapEncoding dex_register_map_encoding =
    kDexRegisterCompressedLocationList;


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

  static constexpr int kFixedSize = 0;

 private:
  MemoryRegion region_;
};

class DexRegisterCompressedMap {
 public:
  explicit DexRegisterCompressedMap(MemoryRegion region) : region_(region) {}

  enum LocationKind : uint8_t {
    // Short location kinds, for entries fitting on one byte (3 bits
    // for the kind, 5 bits for the value).
    kNone = 0,           // 0b000
    kInStack = 1,        // 0b001
    kInRegister = 2,     // 0b010
    kInFpuRegister = 3,  // 0b011
    kConstant = 4,       // 0b100

    // Long location kinds, requiring a 5-byte encoding (1 byte for
    // the kind, 4 bytes for the value).
    kInStackLong = 5,    // 0b101
    kConstantLong = 6,   // 0b110

    kLastLocationKind = kConstantLong
  };
  static_assert(
      sizeof(art::DexRegisterCompressedMap::LocationKind) == 1u,
      "art::DexRegisterCompressedMap::LocationKind has a size different from one byte.");

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
      case kInStackLong:
        return "in stack (long offset)";
      case kConstantLong:
        return "as constant (long value)";

      default:
        LOG(FATAL) << "Unexpected location kind " << static_cast<unsigned>(kind);
        return nullptr;
    }
  }

  // Width of the kind "field" in a compressed short location, in bits.
  static const size_t kKindBits = 3;
  // Width of the value "field" in a compressed short location, in bits.
  static const size_t kValueBits = 5;

  // Compressed short location, fitting on a byte.
  // TODO: Replace this bitfield with an uint8_t for portability reasons.
  struct CompressedShortLocation {
    unsigned kind : kKindBits;
    unsigned value : kValueBits;
  } __attribute__ ((__packed__));;

  static_assert(
      sizeof(art::DexRegisterCompressedMap::CompressedShortLocation) == 1,
      "art::DexRegisterCompressedMap::CompressedShortLocation"
      " has a size different from one byte");

  static CompressedShortLocation CompressShortLocation(LocationKind kind,
                                                       int32_t value) {
    DCHECK(IsUint<kKindBits>(static_cast<unsigned>(kind))) << static_cast<unsigned>(kind);
    DCHECK(IsUint<kValueBits>(value)) << value;
    return { kind, static_cast<unsigned>(value) };
  }

  static bool IsShortLocationKind(LocationKind kind) {
    switch (kind) {
      case kNone:
      case kInStack:
      case kInRegister:
      case kInFpuRegister:
      case kConstant:
        return true;

      case kInStackLong:
      case kConstantLong:
        return false;

      default:
        LOG(FATAL) << "Unexpected location kind " << static_cast<unsigned>(kind);
        return false;
    }
  }

  // TODO: Rename variable names `entry` to something else (maybe
  // `position` or `offset`) as it does not have the exact same
  // meaning as in DexRegisterMap? Rename FindEntry() as well.

  void SetRegisterInfo(size_t entry, LocationKind kind, int32_t value) {
    if (IsShortLocationKind(kind)) {
      // Short location.  Compress the kind and the value as a single byte.
      if (kind == kInStack) {
        // Instead of storing stack offsets expressed in bytes for
        // short stack locations, store slot offsets.  A stack offset
        // is a multiple of 4 (kFrameSlotSize).  This means that by
        // dividing it by 4, we can fit values from the [0, 128[
        // interval in a short stack location, and not just values
        // from the [0, 32[ interval.
        DCHECK_EQ(value % kFrameSlotSize, 0);
        value /= kFrameSlotSize;
      }
      DCHECK(IsUint<kValueBits>(value)) << value;
      region_.Store<CompressedShortLocation>(entry,
                                             CompressShortLocation(kind, value));
    } else {
      // Long location.  Write the location on one byte and the value
      // on 4 bytes.
      DCHECK(!IsUint<kValueBits>(value)) << value;
      region_.Store<LocationKind>(entry, kind);
      region_.Store<int32_t>(entry + sizeof(LocationKind), value);
    }
  }

  // Find the entry number `register_index`.
  size_t FindEntry(uint16_t register_index) const {
    size_t entry = kFixedSize;
    // Skip the first `register_index - 1` entries.
    for (uint16_t i = 0; i < register_index; ++i) {
      // Read the first next byte and inspect its first 3 bits to decide
      // whether it is a short or a long location.
      LocationKind kind = ExtractKind(entry);
      if (IsShortLocationKind(kind)) {
        // Short location.  Skip the current byte.
        entry += SingleShortEntrySize();
      } else {
        // Long location.  Skip the 5 next bytes.
        entry += SingleLongEntrySize();
      }
    }
    return entry;
  }

  LocationKind GetLocationKind(uint16_t register_index) const {
    size_t entry = FindEntry(register_index);
    // Read the first byte and inspect its first 3 bits to get the location.
    return ExtractKind(entry);
  }

  int32_t GetValue(uint16_t register_index) const {
    size_t entry = FindEntry(register_index);
    // Read the first byte and inspect the first 3 bits to decide
    // whether it is a short or a long location.
    CompressedShortLocation first_byte =
        region_.Load<CompressedShortLocation>(entry);
    DCHECK(first_byte.kind <= kLastLocationKind);
    LocationKind kind = static_cast<LocationKind>(first_byte.kind);
    if (IsShortLocationKind(kind)) {
      // Short location.  Extract the value from the remaining 5 bits.
      int32_t value = first_byte.value;
      if (kind == kInStack) {
        // Convert the stack slot offset to a byte offset value.
        value *= kFrameSlotSize;
      }
      return value;
    } else {
      // Long location.   Read the four next bytes to get the value.
      return region_.Load<int32_t>(entry + sizeof(LocationKind));
    }
  }

  // Note that using this method is faster than calling GetLocationKind
  // and then GetValue, as we only scan the Dex register map once.
  std::pair<LocationKind, int32_t> GetLocationKindAndValue(uint16_t register_index) const {
    size_t entry = FindEntry(register_index);
    // Read the first byte and inspect its first 3 bits to get the location.
    CompressedShortLocation first_byte =
        region_.Load<CompressedShortLocation>(entry);
    DCHECK(first_byte.kind <= kLastLocationKind);
    LocationKind kind = static_cast<LocationKind>(first_byte.kind);
    if (IsShortLocationKind(kind)) {
      // Short location.  Extract the value from the remaining 5 bits.
      int32_t value = first_byte.value;
      if (kind == kInStack) {
        // Convert the stack slot offset to a byte offset value.
        value *= kFrameSlotSize;
      }
      return { kind, value };
    } else {
      // Long location.   Read the four next bytes to get the value.
      return { kind, region_.Load<int32_t>(entry + sizeof(LocationKind)) };
    }
  }

  static size_t SingleShortEntrySize() {
    return sizeof(CompressedShortLocation);
  }

  static size_t SingleLongEntrySize() {
    return sizeof(LocationKind) + sizeof(int32_t);
  }

  size_t Size() const {
    return region_.size();
  }

  static constexpr int kFixedSize = 0;

 private:
  // Extract a location kind from the byte at position `entry`.
  LocationKind ExtractKind(size_t entry) const {
    CompressedShortLocation first_byte =
        region_.Load<CompressedShortLocation>(entry);
    DCHECK(first_byte.kind <= kLastLocationKind);
    LocationKind kind = static_cast<LocationKind>(first_byte.kind);
    return kind;
  }

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
 * The information is of the form:
 * [overall_size, number_of_stack_maps, stack_mask_size, StackMap+, DexRegisterInfo+, InlineInfo*].
 */
class CodeInfo {
 public:
  explicit CodeInfo(MemoryRegion region) : region_(region) {}

  explicit CodeInfo(const void* data) {
    uint32_t size = reinterpret_cast<const uint32_t*>(data)[0];
    region_ = MemoryRegion(const_cast<void*>(data), size);
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
      case kDexRegisterCompressedLocationList:
        return kFixedSize;
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
        DexRegisterMap::kFixedSize + number_of_dex_registers * DexRegisterMap::SingleEntrySize()));
  }

  // Only meaningful when the encoding based on a compressed list of
  // [location_kind, register_value] pairs is used.
  DexRegisterCompressedMap GetDexRegisterCompressedMapOf(StackMap stack_map,
                                                         uint32_t number_of_dex_registers) const {
    DCHECK(dex_register_map_encoding == kDexRegisterCompressedLocationList);
    DCHECK(stack_map.HasDexRegisterMap());
    uint32_t offset = stack_map.GetDexRegisterMapOffset();
    return DexRegisterCompressedMap(region_.Subregion(
        offset,
        ComputeDexRegisterCompressedMapSize(offset, number_of_dex_registers)));
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
  static constexpr int kNumberOfStackMapsOffset = kOverallSizeOffset + sizeof(uint32_t);
  static constexpr int kStackMaskSizeOffset = kNumberOfStackMapsOffset + sizeof(uint32_t);
  static constexpr int kFixedSize = kStackMaskSizeOffset + sizeof(uint32_t);

  MemoryRegion GetStackMaps() const {
    return region_.size() == 0
        ? MemoryRegion()
        : region_.Subregion(kFixedSize, StackMapSize() * GetNumberOfStackMaps());
  }

  // Compute the size of a Dex register compressed map starting at offset
  // `origin` in `region_` and containing `number_of_dex_registers` locations.
  size_t ComputeDexRegisterCompressedMapSize(uint32_t origin,
                                             uint32_t number_of_dex_registers) const {
    DCHECK(dex_register_map_encoding == kDexRegisterCompressedLocationList);
    // TODO: Ideally, we would like to use art::DexRegisterCompressedMap::Size
    // or art::DexRegisterCompressedMap::FindEntry, but the
    // DexRegisterCompressedMap is not yet built.  Try to factor common code.
    size_t offset = origin + DexRegisterCompressedMap::kFixedSize;
    // Skip the first `number_of_dex_registers - 1` entries.
    for (uint16_t i = 0; i < number_of_dex_registers; ++i) {
      // Read the first next byte and inspect its first 3 bits to decide
      // whether it is a short or a long location.
      DexRegisterCompressedMap::CompressedShortLocation first_byte =
          region_.Load<DexRegisterCompressedMap::CompressedShortLocation>(offset);
      DCHECK(first_byte.kind <= DexRegisterCompressedMap::kLastLocationKind);
      DexRegisterCompressedMap::LocationKind kind =
          static_cast<DexRegisterCompressedMap::LocationKind>(first_byte.kind);
      if (DexRegisterCompressedMap::IsShortLocationKind(kind)) {
        // Short location.  Skip the current byte.
        offset += DexRegisterCompressedMap::SingleShortEntrySize();
      } else {
        // Long location.  Skip the 5 next bytes.
        offset += DexRegisterCompressedMap::SingleLongEntrySize();
      }
    }
    size_t size = offset - origin;
    return size;
  }

  MemoryRegion region_;
  friend class StackMapStream;
};

}  // namespace art

#endif  // ART_RUNTIME_STACK_MAP_H_
