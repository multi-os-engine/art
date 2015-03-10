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

// Dex register location container used by DexRegisterCompressedMap
// and StackMapStream.
struct DexRegisterLocation {
  /*
   * The location kind used in DexRegisterCompressedMap for a Dex register
   * can either be:
   * - kNone: the register has no location yet, meaning it has not been set.
   * - kConstant: value holds the constant,
   * - kStack: value holds the stack offset,
   * - kRegister: value holds the physical register number.
   * - kFpuRegister: value holds the physical register number.
   *
   * In addition, DexRegisterCompressedMap also uses these values:
   * - kInStackLargeOffset: value holds a "large" stack offset (in the [0, 128)
   *   range).
   * - kConstantBigValue: value holds a "big" constant (in the [-16, 16) range).
   */
  enum class Kind : uint8_t {
    // Short location kinds, for entries fitting on one byte (3 bits
    // for the kind, 5 bits for the value) in a DexRegisterCompressedMap.
    kNone = 0,                // 0b000
    kInStack = 1,             // 0b001
    kInRegister = 2,          // 0b010
    kInFpuRegister = 3,       // 0b011
    kConstant = 4,            // 0b100

    // Long location kinds, requiring a 5-byte encoding (1 byte for the
    // kind, 4 bytes for the value), only used in DexRegisterCompressedMap.

    // Stack location at a large offset, meaning that the offset value
    // divided by the stack frame slot size (4 bytes) cannot fit on a
    // 5-bit unsigned integer (i.e., this offset value is greater than
    // or equal to 2^5 * 4 = 128 bytes).
    kInStackLargeOffset = 5,  // 0b101

    // Big constant, that cannot fit on a 5-bit signed integer (i.e.,
    // lower than -2^(5-1) = -16, or greater than or equal to
    // 2^(5-1) - 1 = 15).
    kConstantBigValue = 6,    // 0b110

    kLastLocationKind = kConstantBigValue
  };

  static_assert(
      sizeof(Kind) == 1u,
      "art::DexRegisterLocation::Kind has a size different from one byte.");

  static const char* PrettyDescriptor(Kind kind) {
    switch (kind) {
      case Kind::kNone:
        return "none";
      case Kind::kInStack:
        return "in stack";
      case Kind::kInRegister:
        return "in register";
      case Kind::kInFpuRegister:
        return "in fpu register";
      case Kind::kConstant:
        return "as constant";
      case Kind::kInStackLargeOffset:
        return "in stack (large offset)";
      case Kind::kConstantBigValue:
        return "as constant (big value)";
      default:
        UNREACHABLE();
    }
  }

  static bool IsShortLocationKind(Kind kind) {
    switch (kind) {
      case Kind::kNone:
      case Kind::kInStack:
      case Kind::kInRegister:
      case Kind::kInFpuRegister:
      case Kind::kConstant:
        return true;

      case Kind::kInStackLargeOffset:
      case Kind::kConstantBigValue:
        return false;

      default:
        UNREACHABLE();
    }
  }

  DexRegisterLocation(Kind location_kind, int32_t location_value)
      : kind(location_kind),
        value(location_value) {}

  Kind kind;
  int32_t value;
};

std::ostream& operator<<(std::ostream& os, const DexRegisterLocation::Kind& rhs);

/**
 * Information on dex register values for a specific PC. The information is
 * of the form:
 * [location_kind, register_value]+.
 * either on 1 or 5 bytes (see art::DexRegisterLocation::Kind).
 */
class DexRegisterCompressedMap {
 public:
  explicit DexRegisterCompressedMap(MemoryRegion region) : region_(region) {}

  // Compressed short location, fitting on one byte.
  typedef uint8_t CompressedShortLocation;

  void SetRegisterInfo(size_t offset, DexRegisterLocation::Kind kind, int32_t value) {
    if (DexRegisterLocation::IsShortLocationKind(kind)) {
      // Short location.  Compress the kind and the value as a single byte.
      if (kind == DexRegisterLocation::Kind::kInStack) {
        // Instead of storing stack offsets expressed in bytes for
        // short stack locations, store slot offsets.  A stack offset
        // is a multiple of 4 (kFrameSlotSize).  This means that by
        // dividing it by 4, we can fit values from the [0, 128)
        // interval in a short stack location, and not just values
        // from the [0, 32) interval.
        DCHECK_EQ(value % kFrameSlotSize, 0);
        value /= kFrameSlotSize;
      }
      DCHECK(IsUint<kValueBits>(value)) << value;
      region_.Store<CompressedShortLocation>(offset, CompressShortLocation(kind, value));
    } else {
      // Long location.  Write the location on one byte and the value
      // on 4 bytes.
      DCHECK(!IsUint<kValueBits>(value)) << value;
      region_.Store<DexRegisterLocation::Kind>(offset, kind);
      region_.Store<int32_t>(offset + sizeof(DexRegisterLocation::Kind), value);
    }
  }

  // Find the offset of the Dex register location number `register_index`.
  size_t FindLocationOffset(uint16_t register_index) const {
    size_t offset = kFixedSize;
    // Skip the first `register_index - 1` entries.
    for (uint16_t i = 0; i < register_index; ++i) {
      // Read the first next byte and inspect its first 3 bits to decide
      // whether it is a short or a long location.
      DexRegisterLocation::Kind kind = ExtractKind(offset);
      if (DexRegisterLocation::IsShortLocationKind(kind)) {
        // Short location.  Skip the current byte.
        offset += SingleShortEntrySize();
      } else {
        // Long location.  Skip the 5 next bytes.
        offset += SingleLongEntrySize();
      }
    }
    return offset;
  }

  DexRegisterLocation::Kind GetLocationKind(uint16_t register_index) const {
    size_t offset = FindLocationOffset(register_index);
    return ExtractKind(offset);
  }

  DexRegisterLocation GetLocationKindAndValue(uint16_t register_index) const {
    size_t offset = FindLocationOffset(register_index);
    // Read the first byte and inspect its first 3 bits to get the location.
    CompressedShortLocation first_byte =
        region_.Load<CompressedShortLocation>(offset);
    DexRegisterLocation::Kind kind = ExtractKindFromCompressedShortLocation(first_byte);
    if (DexRegisterLocation::IsShortLocationKind(kind)) {
      // Short location.  Extract the value from the remaining 5 bits.
      int32_t value = ExtractValueFromCompressedShortLocation(first_byte);
      if (kind == DexRegisterLocation::Kind::kInStack) {
        // Convert the stack slot offset to a byte offset value.
        value *= kFrameSlotSize;
      }
      return DexRegisterLocation(kind, value);
    } else {
      // Long location.  Read the four next bytes to get the value.
      return DexRegisterLocation(kind,
                                 region_.Load<int32_t>(offset + sizeof(DexRegisterLocation::Kind)));
    }
  }

  int32_t GetStackOffsetInBytes(uint16_t register_index) const {
    DexRegisterLocation location = GetLocationKindAndValue(register_index);
    DCHECK(location.kind == DexRegisterLocation::Kind::kInStack
           || location.kind == DexRegisterLocation::Kind::kInStackLargeOffset);
    // We currently encode the offset in bytes.
    return location.value;
  }

  int32_t GetConstant(uint16_t register_index) const {
    DexRegisterLocation location = GetLocationKindAndValue(register_index);
    DCHECK(location.kind == DexRegisterLocation::Kind::kConstant
           || location.kind == DexRegisterLocation::Kind::kConstantBigValue);
    return location.value;
  }

  int32_t GetMachineRegister(uint16_t register_index) const {
    DexRegisterLocation location = GetLocationKindAndValue(register_index);
    DCHECK(location.kind == DexRegisterLocation::Kind::kInRegister
           || location.kind == DexRegisterLocation::Kind::kInFpuRegister)
        << DexRegisterLocation::PrettyDescriptor(location.kind);
    return location.value;
  }

  // Can the `location` be turned into a compressed short location?
  static bool IsCompressibleLocation(const DexRegisterLocation& location) {
    switch (location.kind) {
      case DexRegisterLocation::Kind::kNone:
      case DexRegisterLocation::Kind::kInRegister:
      case DexRegisterLocation::Kind::kInFpuRegister:
        return true;

      case DexRegisterLocation::Kind::kInStack:
        DCHECK_EQ(location.value % kFrameSlotSize, 0);
        return IsUint<kValueBits>(location.value / kFrameSlotSize);

      case DexRegisterLocation::Kind::kConstant:
        return IsUint<kValueBits>(location.value);

      default:
        UNREACHABLE();
    }
  }

  static size_t EntrySize(const DexRegisterLocation& location) {
    if (IsCompressibleLocation(location)) {
      return DexRegisterCompressedMap::SingleShortEntrySize();
    } else {
      return DexRegisterCompressedMap::SingleLongEntrySize();
    }
  }

  static size_t SingleShortEntrySize() {
    return sizeof(CompressedShortLocation);
  }

  static size_t SingleLongEntrySize() {
    return sizeof(DexRegisterLocation::Kind) + sizeof(int32_t);
  }

  size_t Size() const {
    return region_.size();
  }

  static constexpr int kFixedSize = 0;

 private:
  // Width of the kind "field" in a compressed short location, in bits.
  static constexpr size_t kKindBits = 3;
  // Width of the value "field" in a compressed short location, in bits.
  static constexpr size_t kValueBits = 5;

  static constexpr unsigned kKindMask = (1 << kKindBits) - 1;
  static constexpr int32_t kValueMask = (1 << kValueBits) - 1;
  static constexpr size_t kKindOffset = 0;
  static constexpr size_t kValueOffset = kKindBits;

  static CompressedShortLocation CompressShortLocation(DexRegisterLocation::Kind kind,
                                                       int32_t value) {
    DCHECK(IsUint<kKindBits>(static_cast<unsigned>(kind))) << static_cast<unsigned>(kind);
    DCHECK(IsUint<kValueBits>(value)) << value;
    return (static_cast<unsigned>(kind) & kKindMask) << kKindOffset
        | (value & kValueMask) << kValueOffset;
  }

  static DexRegisterLocation::Kind ExtractKindFromCompressedShortLocation(CompressedShortLocation location) {
    unsigned kind = (location >> kKindOffset) & kKindMask;
    DCHECK_LE(kind, static_cast<unsigned>(DexRegisterLocation::Kind::kLastLocationKind));
    return static_cast<DexRegisterLocation::Kind>(kind);
  }

  static int32_t ExtractValueFromCompressedShortLocation(CompressedShortLocation location) {
    return (location >> kValueOffset) & kValueMask;
  }

  // Extract a location kind from the byte at position `offset`.
  DexRegisterLocation::Kind ExtractKind(size_t offset) const {
    CompressedShortLocation first_byte = region_.Load<CompressedShortLocation>(offset);
    return ExtractKindFromCompressedShortLocation(first_byte);
  }

  MemoryRegion region_;

  friend class CodeInfo;
  friend class StackMapStream;
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
    return kFixedSize;
  }

  // Only meaningful when the encoding based on a compressed list of
  // [location_kind, register_value] pairs is used.
  DexRegisterCompressedMap GetDexRegisterCompressedMapOf(StackMap stack_map,
                                                         uint32_t number_of_dex_registers) const {
    DCHECK(stack_map.HasDexRegisterMap());
    uint32_t offset = stack_map.GetDexRegisterMapOffset();
    size_t size = ComputeDexRegisterCompressedMapSize(offset, number_of_dex_registers);
    return DexRegisterCompressedMap(region_.Subregion(offset, size));
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
    // TODO: Ideally, we would like to use art::DexRegisterCompressedMap::Size
    // or art::DexRegisterCompressedMap::FindLocationOffset, but the
    // DexRegisterCompressedMap is not yet built.  Try to factor common code.
    size_t offset = origin + DexRegisterCompressedMap::kFixedSize;
    // Skip the first `number_of_dex_registers - 1` entries.
    for (uint16_t i = 0; i < number_of_dex_registers; ++i) {
      // Read the first next byte and inspect its first 3 bits to decide
      // whether it is a short or a long location.
      DexRegisterCompressedMap::CompressedShortLocation first_byte =
          region_.Load<DexRegisterCompressedMap::CompressedShortLocation>(offset);
      DexRegisterLocation::Kind kind =
          DexRegisterCompressedMap::ExtractKindFromCompressedShortLocation(first_byte);
      if (DexRegisterLocation::IsShortLocationKind(kind)) {
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
