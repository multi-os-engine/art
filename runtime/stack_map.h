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
static constexpr ssize_t kFrameSlotSize = 4;

// Word alignment required on ARM, in bytes.
static constexpr size_t kWordAlignment = 4;

// Size of Dex virtual registers.
static constexpr size_t kVRegSize = 4;

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
    return region_.LoadUnaligned<uint8_t>(kDepthOffset);
  }

  void SetDepth(uint8_t depth) {
    region_.StoreUnaligned<uint8_t>(kDepthOffset, depth);
  }

  uint32_t GetMethodReferenceIndexAtDepth(uint8_t depth) const {
    return region_.LoadUnaligned<uint32_t>(kFixedSize + depth * SingleEntrySize());
  }

  void SetMethodReferenceIndexAtDepth(uint8_t depth, uint32_t index) {
    region_.StoreUnaligned<uint32_t>(kFixedSize + depth * SingleEntrySize(), index);
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

// Dex register location container used by DexRegisterMap and StackMapStream.
class DexRegisterLocation {
 public:
  /*
   * The location kind used to populate the Dex register information in a
   * StackMapStream can either be:
   * - kNone: the register has no location yet, meaning it has not been set;
   * - kConstant: value holds the constant;
   * - kStack: value holds the stack offset;
   * - kRegister: value holds the physical register number;
   * - kFpuRegister: value holds the physical register number.
   *
   * In addition, DexRegisterMap also uses these values:
   * - kInStackLargeOffset: value holds a "large" stack offset (greater than
   *   or equal to 128 bytes);
   * - kConstantLargeValue: value holds a "large" constant (lower than 0, or
   *   or greater than or equal to 32).
   */
  enum class Kind : uint8_t {
    // Short location kinds, for entries fitting on one byte (3 bits
    // for the kind, 5 bits for the value) in a DexRegisterMap.
    kNone = 0,                // 0b000
    kInStack = 1,             // 0b001
    kInRegister = 2,          // 0b010
    kInFpuRegister = 3,       // 0b011
    kConstant = 4,            // 0b100

    // Large location kinds, requiring a 5-byte encoding (1 byte for the
    // kind, 4 bytes for the value).

    // Stack location at a large offset, meaning that the offset value
    // divided by the stack frame slot size (4 bytes) cannot fit on a
    // 5-bit unsigned integer (i.e., this offset value is greater than
    // or equal to 2^5 * 4 = 128 bytes).
    kInStackLargeOffset = 5,  // 0b101

    // Large constant, that cannot fit on a 5-bit signed integer (i.e.,
    // lower than 0, or greater than or equal to 2^5 = 32).
    kConstantLargeValue = 6,  // 0b110

    kLastLocationKind = kConstantLargeValue
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
      case Kind::kConstantLargeValue:
        return "as constant (large value)";
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
      case Kind::kConstantLargeValue:
        return false;

      default:
        UNREACHABLE();
    }
  }

  // Convert `kind` to a "surface" kind, i.e. one that doesn't include
  // any value with a "large" qualifier.
  // TODO: Introduce another enum type for the surface kind?
  static Kind ConvertToSurfaceKind(Kind kind) {
    switch (kind) {
      case Kind::kNone:
      case Kind::kInStack:
      case Kind::kInRegister:
      case Kind::kInFpuRegister:
      case Kind::kConstant:
        return kind;

      case Kind::kInStackLargeOffset:
        return Kind::kInStack;

      case Kind::kConstantLargeValue:
        return Kind::kConstant;

      default:
        UNREACHABLE();
    }
  }

  DexRegisterLocation(Kind kind, int32_t value)
      : kind_(kind), value_(value) {}

  static DexRegisterLocation None() {
    return DexRegisterLocation(Kind::kNone, 0);
  }

  // Get the "surface" kind of the location, i.e., the one that doesn't
  // include any value with a "large" qualifier.
  Kind GetKind() const {
    return ConvertToSurfaceKind(kind_);
  }

  // Get the value of the location.
  int32_t GetValue() const { return value_; }

  // Get the actual kind of the location.
  Kind GetInternalKind() const { return kind_; }

  bool operator==(const DexRegisterLocation& other_location) {
    return kind_ == other_location.kind_ && value_ == other_location.value_;
  }

 private:
  Kind kind_;
  int32_t value_;
};

/**
 * Store information on unique Dex register locations used in a method.
 * The information is of the form:
 * [DexRegisterLocation+].
 * DexRegisterLocations are either 1- or 5-byte wide (see art::DexRegisterLocation::Kind).
 */
class DexRegisterDictionary {
 public:
  explicit DexRegisterDictionary(MemoryRegion region) : region_(region) {}

  // Short (compressed) location, fitting on one byte.
  typedef uint8_t ShortLocation;

  void SetRegisterInfo(size_t offset, const DexRegisterLocation& dex_register_location) {
    DexRegisterLocation::Kind kind = ComputeCompressedKind(dex_register_location);
    int32_t value = dex_register_location.GetValue();
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
      DCHECK(IsShortValue(value)) << value;
      region_.StoreUnaligned<ShortLocation>(offset, MakeShortLocation(kind, value));
    } else {
      // Large location.  Write the location on one byte and the value
      // on 4 bytes.
      DCHECK(!IsShortValue(value)) << value;
      if (kind == DexRegisterLocation::Kind::kInStackLargeOffset) {
        // Also divide large stack offsets by 4 for the sake of consistency.
        DCHECK_EQ(value % kFrameSlotSize, 0);
        value /= kFrameSlotSize;
      }
      // Data can be unaligned as the written Dex register locations can
      // either be 1-byte or 5-byte wide.  Use
      // art::MemoryRegion::StoreUnaligned instead of
      // art::MemoryRegion::Store to prevent unligned word accesses on ARM.
      region_.StoreUnaligned<DexRegisterLocation::Kind>(offset, kind);
      region_.StoreUnaligned<int32_t>(offset + sizeof(DexRegisterLocation::Kind), value);
    }
  }

  // Find the offset of the dictionary entry number `dictionary_entry_index`.
  size_t FindLocationOffset(size_t dictionary_entry_index) const {
    size_t offset = kFixedSize;
    // Skip the first `dictionary_entry_index - 1` entries.
    for (uint16_t i = 0; i < dictionary_entry_index; ++i) {
      // Read the first next byte and inspect its first 3 bits to decide
      // whether it is a short or a large location.
      DexRegisterLocation::Kind kind = ExtractKindAtOffset(offset);
      if (DexRegisterLocation::IsShortLocationKind(kind)) {
        // Short location.  Skip the current byte.
        offset += SingleShortEntrySize();
      } else {
        // Large location.  Skip the 5 next bytes.
        offset += SingleLargeEntrySize();
      }
    }
    return offset;
  }

  // Get the surface kind.
  DexRegisterLocation::Kind GetLocationKind(size_t dictionary_entry_index) const {
    return DexRegisterLocation::ConvertToSurfaceKind(
        GetLocationInternalKind(dictionary_entry_index));
  }

  // Get the internal kind.
  DexRegisterLocation::Kind GetLocationInternalKind(size_t dictionary_entry_index) const {
    return ExtractKindAtOffset(FindLocationOffset(dictionary_entry_index));
  }

  // TODO: Rename as GetDexRegisterLocation?
  DexRegisterLocation GetLocationKindAndValue(size_t dictionary_entry_index) const {
    if (dictionary_entry_index == NoDexRegisterLocationEntryIndex()) {
      return DexRegisterLocation::None();
    }
    size_t offset = FindLocationOffset(dictionary_entry_index);
    // Read the first byte and inspect its first 3 bits to get the location.
    ShortLocation first_byte = region_.LoadUnaligned<ShortLocation>(offset);
    DexRegisterLocation::Kind kind = ExtractKindFromShortLocation(first_byte);
    if (DexRegisterLocation::IsShortLocationKind(kind)) {
      // Short location.  Extract the value from the remaining 5 bits.
      int32_t value = ExtractValueFromShortLocation(first_byte);
      if (kind == DexRegisterLocation::Kind::kInStack) {
        // Convert the stack slot (short) offset to a byte offset value.
        value *= kFrameSlotSize;
      }
      return DexRegisterLocation(kind, value);
    } else {
      // Large location.  Read the four next bytes to get the value.
      int32_t value = region_.LoadUnaligned<int32_t>(offset + sizeof(DexRegisterLocation::Kind));
      if (kind == DexRegisterLocation::Kind::kInStackLargeOffset) {
        // Convert the stack slot (large) offset to a byte offset value.
        value *= kFrameSlotSize;
      }
      return DexRegisterLocation(kind, value);
    }
  }

  int32_t GetStackOffsetInBytes(size_t dictionary_entry_index) const {
    DexRegisterLocation location = GetLocationKindAndValue(dictionary_entry_index);
    DCHECK(location.GetKind() == DexRegisterLocation::Kind::kInStack);
    // GetLocationKindAndValue returns the offset in bytes.
    return location.GetValue();
  }

  int32_t GetConstant(size_t dictionary_entry_index) const {
    DexRegisterLocation location = GetLocationKindAndValue(dictionary_entry_index);
    DCHECK(location.GetKind() == DexRegisterLocation::Kind::kConstant);
    return location.GetValue();
  }

  int32_t GetMachineRegister(size_t dictionary_entry_index) const {
    DexRegisterLocation location =
        GetLocationKindAndValue(dictionary_entry_index);
    DCHECK(location.GetInternalKind() == DexRegisterLocation::Kind::kInRegister
           || location.GetInternalKind() == DexRegisterLocation::Kind::kInFpuRegister)
        << DexRegisterLocation::PrettyDescriptor(location.GetInternalKind());
    return location.GetValue();
  }

  // Compute the compressed kind of `location`.
  static DexRegisterLocation::Kind ComputeCompressedKind(const DexRegisterLocation& location) {
    switch (location.GetInternalKind()) {
      case DexRegisterLocation::Kind::kNone:
        DCHECK_EQ(location.GetValue(), 0);
        return DexRegisterLocation::Kind::kNone;

      case DexRegisterLocation::Kind::kInRegister:
        DCHECK_GE(location.GetValue(), 0);
        DCHECK_LT(location.GetValue(), 1 << kValueBits);
        return DexRegisterLocation::Kind::kInRegister;

      case DexRegisterLocation::Kind::kInFpuRegister:
        DCHECK_GE(location.GetValue(), 0);
        DCHECK_LT(location.GetValue(), 1 << kValueBits);
        return DexRegisterLocation::Kind::kInFpuRegister;

      case DexRegisterLocation::Kind::kInStack:
        return IsShortStackOffsetValue(location.GetValue())
            ? DexRegisterLocation::Kind::kInStack
            : DexRegisterLocation::Kind::kInStackLargeOffset;

      case DexRegisterLocation::Kind::kConstant:
        return IsShortConstantValue(location.GetValue())
            ? DexRegisterLocation::Kind::kConstant
            : DexRegisterLocation::Kind::kConstantLargeValue;

      default:
        LOG(FATAL) << "Unexpected location kind"
                   << DexRegisterLocation::PrettyDescriptor(location.GetInternalKind());
        UNREACHABLE();
    }
  }

  // Can `location` be turned into a short location?
  static bool CanBeEncodedAsShortLocation(const DexRegisterLocation& location) {
    switch (location.GetInternalKind()) {
      case DexRegisterLocation::Kind::kNone:
      case DexRegisterLocation::Kind::kInRegister:
      case DexRegisterLocation::Kind::kInFpuRegister:
        return true;

      case DexRegisterLocation::Kind::kInStack:
        return IsShortStackOffsetValue(location.GetValue());

      case DexRegisterLocation::Kind::kConstant:
        return IsShortConstantValue(location.GetValue());

      default:
        UNREACHABLE();
    }
  }

  static size_t EntrySize(const DexRegisterLocation& location) {
    return CanBeEncodedAsShortLocation(location) ? SingleShortEntrySize() : SingleLargeEntrySize();
  }

  static size_t SingleShortEntrySize() {
    return sizeof(ShortLocation);
  }

  static size_t SingleLargeEntrySize() {
    return sizeof(DexRegisterLocation::Kind) + sizeof(int32_t);
  }

  size_t Size() const {
    return region_.size();
  }

  // Special (invalid) Dex register dictionary entry index meaning
  // that there is no location for a given Dex register (i.e., it is
  // mapped to a DexRegisterLocation::Kind::kNone location).
  static constexpr size_t NoDexRegisterLocationEntryIndex() {
    return -1;
  }

 private:
  static constexpr int kFixedSize = 0;

  // Width of the kind "field" in a short location, in bits.
  static constexpr size_t kKindBits = 3;
  // Width of the value "field" in a short location, in bits.
  static constexpr size_t kValueBits = 5;

  static constexpr uint8_t kKindMask = (1 << kKindBits) - 1;
  static constexpr int32_t kValueMask = (1 << kValueBits) - 1;
  static constexpr size_t kKindOffset = 0;
  static constexpr size_t kValueOffset = kKindBits;

  static bool IsShortStackOffsetValue(int32_t value) {
    DCHECK_EQ(value % kFrameSlotSize, 0);
    return IsShortValue(value / kFrameSlotSize);
  }

  static bool IsShortConstantValue(int32_t value) {
    return IsShortValue(value);
  }

  static bool IsShortValue(int32_t value) {
    return IsUint<kValueBits>(value);
  }

  static ShortLocation MakeShortLocation(DexRegisterLocation::Kind kind, int32_t value) {
    uint8_t kind_integer_value = static_cast<uint8_t>(kind);
    DCHECK(IsUint<kKindBits>(kind_integer_value)) << kind_integer_value;
    DCHECK(IsShortValue(value)) << value;
    return (kind_integer_value & kKindMask) << kKindOffset
        | (value & kValueMask) << kValueOffset;
  }

  static DexRegisterLocation::Kind ExtractKindFromShortLocation(ShortLocation location) {
    uint8_t kind = (location >> kKindOffset) & kKindMask;
    DCHECK_LE(kind, static_cast<uint8_t>(DexRegisterLocation::Kind::kLastLocationKind));
    // We do not encode kNone locations in the stack map.
    DCHECK_NE(kind, static_cast<uint8_t>(DexRegisterLocation::Kind::kNone));
    return static_cast<DexRegisterLocation::Kind>(kind);
  }

  static int32_t ExtractValueFromShortLocation(ShortLocation location) {
    return (location >> kValueOffset) & kValueMask;
  }

  // Extract a location kind from the byte at position `offset`.
  DexRegisterLocation::Kind ExtractKindAtOffset(size_t offset) const {
    ShortLocation first_byte = region_.LoadUnaligned<ShortLocation>(offset);
    return ExtractKindFromShortLocation(first_byte);
  }

  MemoryRegion region_;

  friend class CodeInfo;
  friend class StackMapStream;
};

/* Information on Dex register locations for a specific PC, mapping a
 * stack map's Dex register to a location entry in a DexRegisterDictionary.
 * The information is of the form:
 * [live_bit_mask, entries*]
 * where entries are concatenated unsigned integer values encoded on a number
 * of bits (fixed per DexRegisterMap instances of a CodeInfo object) depending
 * on the number of entries in the Dex register dictionary
 * (see DexRegisterMap::SingleEntrySizeInBits).  The map is 1-byte aligned.
 */
class DexRegisterMap {
 public:
  explicit DexRegisterMap(MemoryRegion region) : region_(region) {}

  static size_t LiveBitMaskSize(uint16_t number_of_dex_registers) {
    return RoundUp(number_of_dex_registers, kBitsPerByte) / kBitsPerByte;
  }

  void SetLiveBitMask(uint16_t number_of_dex_registers,
                      const BitVector& live_dex_registers_mask) {
    size_t live_bit_mask_offset_in_bits = GetDexRegisterMapLiveBitMaskOffset() * kBitsPerByte;
    for (uint16_t i = 0; i < number_of_dex_registers; ++i) {
      region_.StoreBit(live_bit_mask_offset_in_bits + i, live_dex_registers_mask.IsBitSet(i));
    }
  }

  // Get the index of the entry in the Dex register dictionary
  // corresponding to `dex_register_number`.
  size_t GetDictionaryEntryIndex(uint16_t dex_register_number,
                                 uint16_t number_of_dex_registers,
                                 size_t number_of_dictionary_entries) const {
    if (!IsDexRegisterLive(dex_register_number)) {
      return DexRegisterDictionary::NoDexRegisterLocationEntryIndex();
    }

    if (number_of_dictionary_entries == 1) {
      // We do not allocate space for location maps in the case of a
      // single-entry dictionary, as it is useless.  The only valid
      // entry index is 0;
      return 0;
    }

    // The bit offset of the beginning of the map locations.
    size_t map_locations_offset_in_bits =
        GetDexRegisterMapLocationsOffset(number_of_dex_registers) * kBitsPerByte;
    size_t index_in_dex_register_map = GetIndexInDexRegisterMap(dex_register_number);
    DCHECK_LT(index_in_dex_register_map, GetNumberOfLiveDexRegisters(number_of_dex_registers));
    // The bit size of an entry.
    size_t map_entry_size_in_bits = SingleEntrySizeInBits(number_of_dictionary_entries);
    // The bit offset where `index_in_dex_register_map` is located.
    size_t entry_offset_in_bits =
        map_locations_offset_in_bits + index_in_dex_register_map * map_entry_size_in_bits;
    size_t dictionary_entry_index = region_.LoadBits(entry_offset_in_bits, map_entry_size_in_bits);
    DCHECK_LT(dictionary_entry_index, number_of_dictionary_entries);
    return dictionary_entry_index;
  }

  // Map entry at `index_in_dex_register_map` to `dictionary_entry_index`.
  // Pass the number of Dex registers through `number_of_dex_registers`
  // and the number of dictionary entries through
  // `number_of_dictionary_entries`.
  void SetDictionaryEntryIndex(size_t index_in_dex_register_map,
                               size_t dictionary_entry_index,
                               uint16_t number_of_dex_registers,
                               size_t number_of_dictionary_entries) {
    DCHECK_LT(index_in_dex_register_map, GetNumberOfLiveDexRegisters(number_of_dex_registers));
    DCHECK_LT(dictionary_entry_index, number_of_dictionary_entries);

    if (number_of_dictionary_entries == 1) {
      // We do not allocate space for location maps in the case of a
      // single-entry dictionary, as it is useless.
      return;
    }

    // The bit offset of the beginning of the map locations.
    size_t map_locations_offset_in_bits =
        GetDexRegisterMapLocationsOffset(number_of_dex_registers) * kBitsPerByte;
    // The bit size of an entry.
    size_t map_entry_size_in_bits = SingleEntrySizeInBits(number_of_dictionary_entries);
    // The bit offset where `index_in_dex_register_map` is located.
    size_t entry_offset_in_bits =
        map_locations_offset_in_bits + index_in_dex_register_map * map_entry_size_in_bits;
    region_.StoreBits(entry_offset_in_bits, dictionary_entry_index, map_entry_size_in_bits);
  }

  bool IsDexRegisterLive(uint16_t dex_register_number) const {
    size_t live_bit_mask_offset_in_bits = GetDexRegisterMapLiveBitMaskOffset() * kBitsPerByte;
    return region_.LoadBit(live_bit_mask_offset_in_bits + dex_register_number);
  }

  size_t GetNumberOfLiveDexRegisters(uint16_t number_of_dex_registers) const {
    size_t number_of_live_dex_registers = 0;
    for (size_t i = 0; i < number_of_dex_registers; ++i) {
      if (IsDexRegisterLive(i)) {
        ++number_of_live_dex_registers;
      }
    }
    return number_of_live_dex_registers;
  }

  static size_t GetDexRegisterMapLiveBitMaskOffset() {
    return kFixedSize;
  }

  static size_t GetDexRegisterMapLocationsOffset(uint16_t number_of_dex_registers) {
    return GetDexRegisterMapLiveBitMaskOffset() + LiveBitMaskSize(number_of_dex_registers);
  }

  // Return the size of a map entry in bits.  Note that if
  // `number_of_dictionary_entries` equals 1, this function returns 0,
  // which is fine, as there is no need to allocate a map for a
  // single-entry dictionary; the only valid dictionary entry index
  // for a live register in this case is 0 and there is no need to
  // store it.
  static size_t SingleEntrySizeInBits(size_t number_of_dictionary_entries) {
    // Handle the case of 0, as we cannot pass 0 to art::WhichPowerOf2.
    return number_of_dictionary_entries == 0
        ? 0u
        : WhichPowerOf2(RoundUpToPowerOfTwo(number_of_dictionary_entries));
  }

  // Return the size of the DexRegisterMap object, in bytes.
  size_t Size() const {
    return region_.size();
  }

 private:
  // Return the index in the Dex register map corresponding to the Dex
  // register number `dex_register_number`.
  size_t GetIndexInDexRegisterMap(uint16_t dex_register_number) const {
    if (!IsDexRegisterLive(dex_register_number)) {
      return kInvalidIndexInDexRegisterMap;
    }
    return GetNumberOfLiveDexRegisters(dex_register_number);
  }

  // Special (invalid) Dex register map entry index meaning that there
  // is no index in the map for a given Dex register (i.e., it must
  // have been mapped to a DexRegisterLocation::Kind::kNone location).
  static constexpr size_t kInvalidIndexInDexRegisterMap = -1;

  static constexpr int kFixedSize = 0;

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
    return region_.LoadUnaligned<uint32_t>(kDexPcOffset);
  }

  void SetDexPc(uint32_t dex_pc) {
    region_.StoreUnaligned<uint32_t>(kDexPcOffset, dex_pc);
  }

  uint32_t GetNativePcOffset() const {
    return region_.LoadUnaligned<uint32_t>(kNativePcOffsetOffset);
  }

  void SetNativePcOffset(uint32_t native_pc_offset) {
    region_.StoreUnaligned<uint32_t>(kNativePcOffsetOffset, native_pc_offset);
  }

  uint32_t GetDexRegisterMapOffset() const {
    return region_.LoadUnaligned<uint32_t>(kDexRegisterMapOffsetOffset);
  }

  void SetDexRegisterMapOffset(uint32_t offset) {
    region_.StoreUnaligned<uint32_t>(kDexRegisterMapOffsetOffset, offset);
  }

  uint32_t GetInlineDescriptorOffset() const {
    return region_.LoadUnaligned<uint32_t>(kInlineDescriptorOffsetOffset);
  }

  void SetInlineDescriptorOffset(uint32_t offset) {
    region_.StoreUnaligned<uint32_t>(kInlineDescriptorOffsetOffset, offset);
  }

  uint32_t GetRegisterMask() const {
    return region_.LoadUnaligned<uint32_t>(kRegisterMaskOffset);
  }

  void SetRegisterMask(uint32_t mask) {
    region_.StoreUnaligned<uint32_t>(kRegisterMaskOffset, mask);
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

  static size_t ComputeStackMapSize(size_t stack_mask_size) {
    return StackMap::kFixedSize + stack_mask_size;
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
    return kFixedSize;
  }

  DexRegisterDictionary GetDexRegisterDictionary() const {
    return DexRegisterDictionary(region_.Subregion(
        GetDexRegisterDictionaryOffset(),
        GetDexRegisterDictionarySize()));
  }

  StackMap GetStackMapAt(size_t i) const {
    size_t size = StackMapSize();
    return StackMap(GetStackMaps().Subregion(i * size, size));
  }

  uint32_t GetOverallSize() const {
    return region_.LoadUnaligned<uint32_t>(kOverallSizeOffset);
  }

  void SetOverallSize(uint32_t size) {
    region_.StoreUnaligned<uint32_t>(kOverallSizeOffset, size);
  }

  uint32_t GetNumberOfDexRegisterDictionaryEntries() const {
    return region_.LoadUnaligned<uint32_t>(kNumberOfDexRegisterDictionaryEntriesOffset);
  }

  void SetNumberOfDexRegisterDictionaryEntries(uint32_t num_entries) {
    region_.StoreUnaligned<uint32_t>(kNumberOfDexRegisterDictionaryEntriesOffset, num_entries);
  }

  uint32_t GetDexRegisterDictionarySize() const {
    return ComputeDexRegisterDictionarySize(GetDexRegisterDictionaryOffset(),
                                            GetNumberOfDexRegisterDictionaryEntries());
  }

  uint32_t GetStackMaskSize() const {
    return region_.LoadUnaligned<uint32_t>(kStackMaskSizeOffset);
  }

  void SetStackMaskSize(uint32_t size) {
    region_.StoreUnaligned<uint32_t>(kStackMaskSizeOffset, size);
  }

  size_t GetNumberOfStackMaps() const {
    return region_.LoadUnaligned<uint32_t>(kNumberOfStackMapsOffset);
  }

  void SetNumberOfStackMaps(uint32_t number_of_stack_maps) {
    region_.StoreUnaligned<uint32_t>(kNumberOfStackMapsOffset, number_of_stack_maps);
  }

  // Get the size of one stack map of this CodeInfo object, in bytes.
  // All stack maps of a CodeInfo have the same size.
  size_t StackMapSize() const {
    return StackMap::ComputeStackMapSize(GetStackMaskSize());
  }

  // Get the size all the stack maps of this CodeInfo object, in bytes.
  size_t StackMapsSize() const {
    return StackMapSize() * GetNumberOfStackMaps();
  }

  uint32_t GetStackMapsOffset() const {
    return kFixedSize + GetDexRegisterDictionarySize();
  }

  DexRegisterMap GetDexRegisterMapOf(StackMap stack_map, uint32_t number_of_dex_registers) const {
    DCHECK(stack_map.HasDexRegisterMap());
    uint32_t offset = stack_map.GetDexRegisterMapOffset();
    size_t size = ComputeDexRegisterMapSizeOf(stack_map, number_of_dex_registers);
    return DexRegisterMap(region_.Subregion(offset, size));
  }

  InlineInfo GetInlineInfoOf(StackMap stack_map) const {
    DCHECK(stack_map.HasInlineInfo());
    uint32_t offset = stack_map.GetInlineDescriptorOffset();
    uint8_t depth = region_.LoadUnaligned<uint8_t>(offset);
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
  static constexpr int kNumberOfDexRegisterDictionaryEntriesOffset =
      kOverallSizeOffset + sizeof(uint32_t);
  static constexpr int kNumberOfStackMapsOffset = kNumberOfDexRegisterDictionaryEntriesOffset + sizeof(uint32_t);
  static constexpr int kStackMaskSizeOffset = kNumberOfStackMapsOffset + sizeof(uint32_t);
  static constexpr int kFixedSize = kStackMaskSizeOffset + sizeof(uint32_t);

  MemoryRegion GetStackMaps() const {
    return region_.size() == 0
        ? MemoryRegion()
        : region_.Subregion(GetStackMapsOffset(), StackMapsSize());
  }

  // Compute the size of the Dex register map associated to `stack_map`.
  size_t ComputeDexRegisterMapSizeOf(StackMap stack_map, uint16_t number_of_dex_registers) const {
    DCHECK(stack_map.HasDexRegisterMap());
    uint32_t dex_register_map_offset_in_code_info = stack_map.GetDexRegisterMapOffset();
    size_t locations_offset_in_dex_register_map =
        DexRegisterMap::GetDexRegisterMapLocationsOffset(number_of_dex_registers);
    // Create a temporary art::DexRegisterMap to be able to call
    // art::DexRegisterMap::GetNumberOfLiveDexRegisters and
    DexRegisterMap dex_register_map_without_locations(
        MemoryRegion(region_.Subregion(dex_register_map_offset_in_code_info,
                                       locations_offset_in_dex_register_map)));
    size_t number_of_live_dex_registers =
        dex_register_map_without_locations.GetNumberOfLiveDexRegisters(number_of_dex_registers);
    size_t map_locations_size_in_bits =
        DexRegisterMap::SingleEntrySizeInBits(GetNumberOfDexRegisterDictionaryEntries())
        * number_of_live_dex_registers;
    size_t map_locations_size_in_bytes =
        RoundUp(map_locations_size_in_bits, kBitsPerByte) / kBitsPerByte;
    size_t dex_register_map_size =
        locations_offset_in_dex_register_map + map_locations_size_in_bytes;
    return dex_register_map_size;
  }

  // Compute the size of a Dex register dictionary starting at offset `origin`
  // in `region_` and containing `number_of_dex_locations` entries.
  size_t ComputeDexRegisterDictionarySize(uint32_t origin, uint32_t number_of_dex_locations) const {
    // TODO: Ideally, we would like to use art::DexRegisterDictionary::Size or
    // art::DexRegisterDictionary::FindLocationOffset, but the
    // DexRegisterDictionary is not yet built.  Try to factor common code.
    size_t offset = origin + DexRegisterDictionary::kFixedSize;

    // Skip the first `number_of_dex_locations - 1` entries.
    for (uint16_t i = 0; i < number_of_dex_locations; ++i) {
      // Read the first next byte and inspect its first 3 bits to decide
      // whether it is a short or a large location.
      DexRegisterDictionary::ShortLocation first_byte =
          region_.LoadUnaligned<DexRegisterDictionary::ShortLocation>(offset);
      DexRegisterLocation::Kind kind =
          DexRegisterDictionary::ExtractKindFromShortLocation(first_byte);
      if (DexRegisterLocation::IsShortLocationKind(kind)) {
        // Short location.  Skip the current byte.
        offset += DexRegisterDictionary::SingleShortEntrySize();
      } else {
        // Large location.  Skip the 5 next bytes.
        offset += DexRegisterDictionary::SingleLargeEntrySize();
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
