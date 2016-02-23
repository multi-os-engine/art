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
#include "base/bit_utils.h"
#include "base/dchecked_vector.h"
#include "memory_region.h"
#include "leb128.h"

namespace art {

class VariableIndentationOutputStream;

// Size of a frame slot, in bytes.  This constant is a signed value,
// to please the compiler in arithmetic operations involving int32_t
// (signed) values.
static constexpr ssize_t kFrameSlotSize = 4;

// Size of Dex virtual registers.
static constexpr size_t kVRegSize = 4;

class CodeInfo;

/**
 * Classes in the following file are wrapper on stack map information backed
 * by a MemoryRegion. As such they read and write to the region, they don't have
 * their own fields.
 */

// Dex register location container used by DexRegisterMap and StackMapStream.
class DexRegisterLocation {
 public:
  // The maximum number of bytes the location takes when encoded.
  static constexpr size_t kMaximumEncodedSize = 5;

  /*
   * The location kind used to populate the Dex register information in a
   * StackMapStream can either be:
   * - kNone: the register has no location, meaning it has not been set.
   * - kStack: vreg stored on the stack, value holds the stack offset;
   * - kConstant: value holds the constant;
   * - kInRegister: vreg stored in low 32 bits of a core physical register,
   *                value holds the register number;
   * - kInRegisterHigh: vreg stored in high 32 bits of a core physical register,
   *                    value holds the register number;
   * - kInFpuRegister: vreg stored in low 32 bits of an FPU register,
   *                   value holds the register number;
   * - kInFpuRegisterHigh: vreg stored in high 32 bits of an FPU register,
   *                       value holds the register number;
   */
  enum class Kind {
    kNone = 0,                // 0b000
    kInStack = 2,             // 0b010
    kConstant = 3,            // 0b011
    kInRegister = 4,          // 0b100
    kInRegisterHigh = 5,      // 0b101
    kInFpuRegister = 6,       // 0b110
    kInFpuRegisterHigh = 7,   // 0b111
  };

  DexRegisterLocation() : kind_(Kind::kNone), value_(0) {}

  DexRegisterLocation(Kind kind, int32_t value) : kind_(kind), value_(value) {
    if (kind_ == Kind::kInStack) {
      // Instead of storing stack offsets expressed in bytes for short stack locations,
      // store slot offsets. This allows us to encode more values using the short form.
      DCHECK_EQ(value_ % kFrameSlotSize, 0);
      value_ /= kFrameSlotSize;
    }
  }

  static DexRegisterLocation None() {
    return DexRegisterLocation(Kind::kNone, 0);
  }

  // Get the kind of the location.
  Kind GetKind() const { return kind_; }

  // TODO: Remove since there are no internal kinds any more.
  Kind GetInternalKind() const { return kind_; }

  // Get the value of the location.
  int32_t GetValue() const {
    return kind_ == Kind::kInStack ? value_ * kFrameSlotSize : value_;
  }

  // Write the DexRegisterLocation to memory. It will consume either one or five bytes.
  // Large stack or constant locations are escaped by storing b11111 in the value field.
  void Encode(const MemoryRegion* region, size_t* offset) const {
    uint32_t value = value_;  // Cast to unsigned.
    if (UNLIKELY(value >= kValueMask) && (kind_ == Kind::kInStack || kind_ == Kind::kConstant)) {
      // The value is too large.  Encode it as five bytes.
      uint32_t encoded = (static_cast<uint32_t>(kind_) << kValueBits) | kValueMask;
      region->StoreUnaligned<uint8_t>((*offset)++, dchecked_integral_cast<uint8_t>(encoded));
      region->StoreUnaligned<uint32_t>(*offset, value);
      *offset += 4;
    } else {
      // Encode as single byte.
      DCHECK(IsUint<kValueBits>(value));
      uint32_t encoded = (static_cast<uint32_t>(kind_) << kValueBits) | value;
      region->StoreUnaligned<uint8_t>((*offset)++, dchecked_integral_cast<uint8_t>(encoded));
    }
  }

  // Decode previously encoded value.
  static DexRegisterLocation Decode(const MemoryRegion* region, size_t* offset) {
    uint8_t encoded = region->LoadUnaligned<uint8_t>((*offset)++);
    Kind kind = static_cast<Kind>(encoded >> kValueBits);
    int32_t value = encoded & kValueMask;
    if (UNLIKELY(value == kValueMask) && (kind == Kind::kInStack || kind == Kind::kConstant)) {
      value = region->LoadUnaligned<uint32_t>(*offset);
      *offset += 4;
    }
    // Set the internal fields directly. The constructor would mangle stack offset.
    DexRegisterLocation result;
    result.kind_ = kind;
    result.value_ = value;
    return result;
  }

  bool operator==(DexRegisterLocation other) const {
    return kind_ == other.kind_ && value_ == other.value_;
  }

  bool operator!=(DexRegisterLocation other) const {
    return !(*this == other);
  }

 private:
  Kind kind_;
  int32_t value_;

  // Width of the value field in short encoded form.
  static constexpr size_t kValueBits = 5;
  // Maximum representable value of the value field in short encoded form.
  static constexpr uint32_t kValueMask = (1 << kValueBits) - 1;
};

std::ostream& operator<<(std::ostream& stream, const DexRegisterLocation::Kind& kind);
std::ostream& operator<<(std::ostream& stream, const DexRegisterLocation& location);

// Decoded Dex register locations for a specific PC.
// TODO: Remove all the unused arguments in this class.
class DexRegisterMap {
 public:
  DexRegisterMap() : locations_() {}

  explicit DexRegisterMap(dchecked_vector<DexRegisterLocation>&& locations)
      : locations_(locations) {}

  bool IsValid() const { return !locations_.empty(); }

  size_t size() const { return locations_.size(); }

  // Get the surface kind of Dex register `dex_register_number`.
  DexRegisterLocation::Kind GetLocationKind(uint16_t dex_register_number,
                                            uint16_t number_of_dex_registers ATTRIBUTE_UNUSED,
                                            const CodeInfo& code_info ATTRIBUTE_UNUSED) const {
    return locations_[dex_register_number].GetKind();
  }

  // Get the internal kind of Dex register `dex_register_number`.
  DexRegisterLocation::Kind GetLocationInternalKind(
      uint16_t dex_register_number,
      uint16_t number_of_dex_registers ATTRIBUTE_UNUSED,
      const CodeInfo& code_info ATTRIBUTE_UNUSED) const {
    return locations_[dex_register_number].GetKind();
  }

  // Get the Dex register location `dex_register_number`.
  DexRegisterLocation GetDexRegisterLocation(uint16_t dex_register_number,
                                             uint16_t number_of_dex_registers ATTRIBUTE_UNUSED,
                                             const CodeInfo& code_info ATTRIBUTE_UNUSED) const {
    return locations_[dex_register_number];
  }

  int32_t GetStackOffsetInBytes(uint16_t dex_register_number,
                                uint16_t number_of_dex_registers ATTRIBUTE_UNUSED,
                                const CodeInfo& code_info ATTRIBUTE_UNUSED) const {
    const DexRegisterLocation& location = locations_[dex_register_number];
    DCHECK(location.GetKind() == DexRegisterLocation::Kind::kInStack);
    // GetDexRegisterLocation returns the offset in bytes.
    return location.GetValue();
  }

  int32_t GetConstant(uint16_t dex_register_number,
                      uint16_t number_of_dex_registers ATTRIBUTE_UNUSED,
                      const CodeInfo& code_info ATTRIBUTE_UNUSED) const {
    const DexRegisterLocation& location = locations_[dex_register_number];
    DCHECK_EQ(location.GetKind(), DexRegisterLocation::Kind::kConstant);
    return location.GetValue();
  }

  int32_t GetMachineRegister(uint16_t dex_register_number,
                             uint16_t number_of_dex_registers ATTRIBUTE_UNUSED,
                             const CodeInfo& code_info ATTRIBUTE_UNUSED) const {
    const DexRegisterLocation& location = locations_[dex_register_number];
    DCHECK(location.GetInternalKind() == DexRegisterLocation::Kind::kInRegister ||
           location.GetInternalKind() == DexRegisterLocation::Kind::kInRegisterHigh ||
           location.GetInternalKind() == DexRegisterLocation::Kind::kInFpuRegister ||
           location.GetInternalKind() == DexRegisterLocation::Kind::kInFpuRegisterHigh)
        << location.GetInternalKind();
    return location.GetValue();
  }

  bool IsDexRegisterLive(uint16_t dex_register_number) const {
    return locations_[dex_register_number].GetKind() != DexRegisterLocation::Kind::kNone;
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

  void Dump(VariableIndentationOutputStream* vios) const;

  const DexRegisterLocation& operator[](size_t index) const {
    return locations_[index];
  }

 private:
  dchecked_vector<DexRegisterLocation> locations_;
};

typedef std::vector<DexRegisterMap> DexRegisterMaps;

// Represents bit range of bit-packed integer field.
// We reuse the idea from ULEB128p1 to support encoding of -1 (aka 0xFFFFFFFF).
// If min_value is set to -1, we implicitly subtract one from any loaded value,
// and add one to any stored value. This is generalized to any negative values.
// In other words, min_value acts as a base and the stored value is added to it.
struct FieldEncoding {
  FieldEncoding(size_t start_offset, size_t end_offset, int32_t min_value = 0)
      : start_offset_(start_offset), end_offset_(end_offset), min_value_(min_value) {
    DCHECK_LE(start_offset_, end_offset_);
    DCHECK_LE(BitSize(), 32u);
  }

  size_t BitSize() const { return end_offset_ - start_offset_; }

  int32_t Load(const MemoryRegion& region) const {
    DCHECK_LE(end_offset_, region.size_in_bits());
    const size_t bit_count = BitSize();
    if (bit_count == 0) {
      // Do not touch any memory if the range is empty.
      return min_value_;
    }
    uint8_t* address = region.start() + start_offset_ / kBitsPerByte;
    const uint32_t shift = start_offset_ & (kBitsPerByte - 1);
    // Load the value (reading only the strictly needed bytes).
    uint32_t value = *address++ >> shift;
    for (size_t loaded_bits = 8; loaded_bits < shift + bit_count; loaded_bits += 8) {
      value |= *address++ << (loaded_bits - shift);
    }
    // Clear unwanted most significant bits.
    value &= MaxInt<uint32_t>(bit_count);
    // Compare with the trivial bit-by-bit implementation.
    DCHECK_EQ(value, region.LoadBits(start_offset_, bit_count));
    return value + min_value_;
  }

  void Store(MemoryRegion region, int32_t value) const {
    region.StoreBits(start_offset_, value - min_value_, BitSize());
    DCHECK_EQ(Load(region), value);
  }

 private:
  size_t start_offset_;
  size_t end_offset_;
  int32_t min_value_;
};

class StackMapEncoding {
 public:
  StackMapEncoding() {}

  // Create stack map bit layout based on given sizes.
  // Returns the size of stack map in bytes.
  size_t SetFromSizes(size_t native_pc_max,
                      size_t dex_pc_max,
                      size_t flags_max,
                      size_t inline_info_size,
                      size_t register_mask_max,
                      size_t stack_mask_bit_size) {
    size_t bit_offset = 0;
    DCHECK_EQ(kNativePcBitOffset, bit_offset);
    bit_offset += MinimumBitsToStore(native_pc_max);

    dex_pc_bit_offset_ = dchecked_integral_cast<uint8_t>(bit_offset);
    bit_offset += MinimumBitsToStore(1 /* kNoDexPc */ + dex_pc_max);

    flags_bit_offset_ = dchecked_integral_cast<uint8_t>(bit_offset);
    bit_offset += MinimumBitsToStore(flags_max);

    // We also need +1 for kNoInlineInfo, but since the inline_info_size is strictly
    // greater than the offset we might try to encode, we already implicitly have it.
    // If inline_info_size is zero, we can encode only kNoInlineInfo (in zero bits).
    inline_info_bit_offset_ = dchecked_integral_cast<uint8_t>(bit_offset);
    bit_offset += MinimumBitsToStore(inline_info_size);

    register_mask_bit_offset_ = dchecked_integral_cast<uint8_t>(bit_offset);
    bit_offset += MinimumBitsToStore(register_mask_max);

    stack_mask_bit_offset_ = dchecked_integral_cast<uint8_t>(bit_offset);
    bit_offset += stack_mask_bit_size;

    return RoundUp(bit_offset, kBitsPerByte) / kBitsPerByte;
  }

  FieldEncoding GetNativePcEncoding() const {
    return FieldEncoding(kNativePcBitOffset, dex_pc_bit_offset_);
  }
  FieldEncoding GetDexPcEncoding() const {
    return FieldEncoding(dex_pc_bit_offset_, flags_bit_offset_, -1 /* min_value */);
  }
  FieldEncoding GetFlagsEncoding() const {
    return FieldEncoding(flags_bit_offset_, inline_info_bit_offset_);
  }
  FieldEncoding GetInlineInfoEncoding() const {
    return FieldEncoding(inline_info_bit_offset_, register_mask_bit_offset_, -1 /* min_value */);
  }
  FieldEncoding GetRegisterMaskEncoding() const {
    return FieldEncoding(register_mask_bit_offset_, stack_mask_bit_offset_);
  }
  size_t GetStackMaskBitOffset() const {
    // The end offset is not encoded. It is implicitly the end of stack map entry.
    return stack_mask_bit_offset_;
  }

  void Dump(VariableIndentationOutputStream* vios) const;

 private:
  static constexpr size_t kNativePcBitOffset = 0;
  uint8_t dex_pc_bit_offset_;
  uint8_t flags_bit_offset_;
  uint8_t inline_info_bit_offset_;
  uint8_t register_mask_bit_offset_;
  uint8_t stack_mask_bit_offset_;
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
 *
 *   [native_pc_offset, dex_pc, flags, inlining_info_offset, register_mask, stack_mask].
 */
class StackMap {
 public:
  enum Flags {
    // This flag is set if and only if StackMapEntry::num_dex_registers was non-zero.
    // Inlined infos may or may not have register locations regardless of this flags.
    kHasAnyDexRegisters = 1 << 0,
    // All locations (including inlined) are identical to the last state.
    kSameDexRegisterMap = 1 << 1,
  };

  StackMap() {}
  StackMap(const StackMapEncoding* encoding, MemoryRegion region)
      : encoding_(encoding), region_(region) {}

  bool IsValid() const { return region_.pointer() != nullptr; }

  uint32_t GetDexPc() const {
    return encoding_->GetDexPcEncoding().Load(region_);
  }

  void SetDexPc(uint32_t dex_pc) {
    encoding_->GetDexPcEncoding().Store(region_, dex_pc);
  }

  uint32_t GetNativePcOffset() const {
    return encoding_->GetNativePcEncoding().Load(region_);
  }

  void SetNativePcOffset(uint32_t native_pc_offset) {
    encoding_->GetNativePcEncoding().Store(region_, native_pc_offset);
  }

  uint32_t GetFlags() const {
    return encoding_->GetFlagsEncoding().Load(region_);
  }

  void SetFlags(uint32_t offset) {
    encoding_->GetFlagsEncoding().Store(region_, offset);
  }

  uint32_t GetInlineDescriptorOffset() const {
    return encoding_->GetInlineInfoEncoding().Load(region_);
  }

  void SetInlineDescriptorOffset(uint32_t offset) {
    encoding_->GetInlineInfoEncoding().Store(region_, offset);
  }

  uint32_t GetRegisterMask() const {
    return encoding_->GetRegisterMaskEncoding().Load(region_);
  }

  void SetRegisterMask(uint32_t mask) {
    encoding_->GetRegisterMaskEncoding().Store(region_, mask);
  }

  size_t GetNumberOfStackMaskBits() const {
    return region_.size_in_bits() - encoding_->GetStackMaskBitOffset();
  }

  bool GetStackMaskBit(size_t index) const {
    return region_.LoadBit(encoding_->GetStackMaskBitOffset() + index);
  }

  void SetStackMaskBit(size_t index, bool value) {
    region_.StoreBit(encoding_->GetStackMaskBitOffset() + index, value);
  }

  bool HasDexRegisterMap() const {
    return (GetFlags() & Flags::kHasAnyDexRegisters) != 0;
  }

  bool HasInlineInfo() const {
    return GetInlineDescriptorOffset() != kNoInlineInfo;
  }

  bool Equals(const StackMap& other) const {
    return region_.pointer() == other.region_.pointer() && region_.size() == other.region_.size();
  }

  void Dump(VariableIndentationOutputStream* vios,
            const CodeInfo& code_info,
            uint32_t code_offset,
            uint16_t number_of_dex_registers,
            const std::string& header_suffix = "") const;

  // Special (invalid) offset for the InlineDescriptorOffset field meaning
  // that there is no inline info for this stack map.
  static constexpr uint32_t kNoInlineInfo = -1;

 private:
  const StackMapEncoding* encoding_;
  MemoryRegion region_;
};

class InlineInfoEncoding {
 public:
  void SetFromSizes(size_t method_index_max,
                    size_t dex_pc_max,
                    size_t invoke_type_max,
                    size_t number_of_dex_registers_max) {
    total_bit_size_ = kMethodIndexBitOffset;
    total_bit_size_ += MinimumBitsToStore(method_index_max);

    dex_pc_bit_offset_ = dchecked_integral_cast<uint8_t>(total_bit_size_);
    total_bit_size_ += MinimumBitsToStore(1 /* kNoDexPc */ + dex_pc_max);

    invoke_type_bit_offset_ = dchecked_integral_cast<uint8_t>(total_bit_size_);
    total_bit_size_ += MinimumBitsToStore(invoke_type_max);

    number_of_dex_registers_bit_offset_ = dchecked_integral_cast<uint8_t>(total_bit_size_);
    total_bit_size_ += MinimumBitsToStore(number_of_dex_registers_max);
  }

  FieldEncoding GetMethodIndexEncoding() const {
    return FieldEncoding(kMethodIndexBitOffset, dex_pc_bit_offset_);
  }
  FieldEncoding GetDexPcEncoding() const {
    return FieldEncoding(dex_pc_bit_offset_, invoke_type_bit_offset_, -1 /* min_value */);
  }
  FieldEncoding GetInvokeTypeEncoding() const {
    return FieldEncoding(invoke_type_bit_offset_, number_of_dex_registers_bit_offset_);
  }
  FieldEncoding GetNumberOfDexRegistersEncoding() const {
    return FieldEncoding(number_of_dex_registers_bit_offset_, total_bit_size_);
  }
  size_t GetEntrySize() const {
    return RoundUp(total_bit_size_, kBitsPerByte) / kBitsPerByte;
  }

  void Dump(VariableIndentationOutputStream* vios) const;

 private:
  static constexpr uint8_t kIsLastBitOffset = 0;
  static constexpr uint8_t kMethodIndexBitOffset = 1;
  uint8_t dex_pc_bit_offset_;
  uint8_t invoke_type_bit_offset_;
  uint8_t number_of_dex_registers_bit_offset_;
  uint8_t total_bit_size_;
};

/**
 * Inline information for a specific PC. The information is of the form:
 *
 *   [is_last, method_index, dex_pc, invoke_type, dex_register_map_offset]+.
 */
class InlineInfo {
 public:
  InlineInfo(const InlineInfoEncoding* encoding, MemoryRegion region, size_t offset = 0)
      : encoding_(encoding), region_(region), offset_(offset) {
  }

  uint32_t GetDepth() const {
    size_t depth = 0;
    while (!GetRegionAtDepth(depth++).LoadBit(0)) { }  // Check is_last bit.
    return depth;
  }

  void SetDepth(uint32_t depth) {
    DCHECK_GT(depth, 0u);
    for (size_t d = 0; d < depth; ++d) {
      GetRegionAtDepth(d).StoreBit(0, d == depth - 1);  // Set is_last bit.
    }
  }

  uint32_t GetMethodIndexAtDepth(uint32_t depth) const {
    return encoding_->GetMethodIndexEncoding().Load(GetRegionAtDepth(depth));
  }

  void SetMethodIndexAtDepth(uint32_t depth, uint32_t index) {
    encoding_->GetMethodIndexEncoding().Store(GetRegionAtDepth(depth), index);
  }

  uint32_t GetDexPcAtDepth(uint32_t depth) const {
    return encoding_->GetDexPcEncoding().Load(GetRegionAtDepth(depth));
  }

  void SetDexPcAtDepth(uint32_t depth, uint32_t dex_pc) {
    encoding_->GetDexPcEncoding().Store(GetRegionAtDepth(depth), dex_pc);
  }

  uint32_t GetInvokeTypeAtDepth(uint32_t depth) const {
    return encoding_->GetInvokeTypeEncoding().Load(GetRegionAtDepth(depth));
  }

  void SetInvokeTypeAtDepth(uint32_t depth, uint32_t invoke_type) {
    encoding_->GetInvokeTypeEncoding().Store(GetRegionAtDepth(depth), invoke_type);
  }

  uint32_t GetNumberOfDexRegistersAtDepth(uint32_t depth) const {
    return encoding_->GetNumberOfDexRegistersEncoding().Load(GetRegionAtDepth(depth));
  }

  void SetNumberOfDexRegistersAtDepth(uint32_t depth, uint32_t offset) {
    encoding_->GetNumberOfDexRegistersEncoding().Store(GetRegionAtDepth(depth), offset);
  }

  void Dump(VariableIndentationOutputStream* vios, const CodeInfo& info) const;

  bool Equals(const InlineInfo& other) {
    return region_.start() + offset_ == other.region_.start() + other.offset_;
  }

 private:
  MemoryRegion GetRegionAtDepth(uint32_t depth) const {
    size_t entry_size = encoding_->GetEntrySize();
    DCHECK_GT(entry_size, 0u);
    return region_.Subregion(offset_ + depth * entry_size, entry_size);
  }

  const InlineInfoEncoding* encoding_;
  MemoryRegion region_;
  size_t offset_;  // Offset in region_ where this InlineInfo starts.
};

// Most of the fields are encoded as ULEB128 to save space.
struct CodeInfoHeader {
  uint32_t number_of_stack_maps;  // Number of stack map entries.
  uint32_t stack_map_size;  // Byte size of single stack map entry.
  uint32_t number_of_dex_registers;  // Excludes inlined registers.
  uint32_t dex_register_maps_size;
  uint32_t inline_infos_size;
  const StackMapEncoding* stack_map_encoding;
  const InlineInfoEncoding* inline_info_encoding;

  CodeInfoHeader() { }

  void Decode(const uint8_t** ptr) {
    number_of_stack_maps = DecodeUnsignedLeb128(ptr);
    stack_map_size = DecodeUnsignedLeb128(ptr);
    number_of_dex_registers = DecodeUnsignedLeb128(ptr);
    dex_register_maps_size = DecodeUnsignedLeb128(ptr);
    inline_infos_size = DecodeUnsignedLeb128(ptr);
    if (number_of_stack_maps > 0) {
      static_assert(alignof(StackMapEncoding) == 1, "StackMapEncoding must be unaligned");
      stack_map_encoding = reinterpret_cast<const StackMapEncoding*>(*ptr);
      *ptr += sizeof(StackMapEncoding);
    } else {
      stack_map_encoding = nullptr;
    }
    if (inline_infos_size > 0) {
      static_assert(alignof(InlineInfoEncoding) == 1, "InlineInfoEncoding must be unaligned");
      inline_info_encoding = reinterpret_cast<const InlineInfoEncoding*>(*ptr);
      *ptr += sizeof(InlineInfoEncoding);
    } else {
      inline_info_encoding = nullptr;
    }
  }

  template<typename Vector>
  void Encode(Vector* dest) const {
    EncodeUnsignedLeb128(dest, number_of_stack_maps);
    EncodeUnsignedLeb128(dest, stack_map_size);
    EncodeUnsignedLeb128(dest, number_of_dex_registers);
    EncodeUnsignedLeb128(dest, dex_register_maps_size);
    EncodeUnsignedLeb128(dest, inline_infos_size);
    if (number_of_stack_maps > 0) {
      const uint8_t* ptr = reinterpret_cast<const uint8_t*>(stack_map_encoding);
      dest->insert(dest->end(), ptr, ptr + sizeof(StackMapEncoding));
    }
    if (inline_infos_size > 0) {
      const uint8_t* ptr = reinterpret_cast<const uint8_t*>(inline_info_encoding);
      dest->insert(dest->end(), ptr, ptr + sizeof(InlineInfoEncoding));
    }
  }

  void Dump(VariableIndentationOutputStream* vios) const;
};

/**
 * Wrapper around all compiler information collected for a method.
 * The information is of the form:
 *
 *   [Header, StackMapEncoding, StackMap*, InlineInfo*, DexRegisterMap*]
 */
class CodeInfo {
 public:
  explicit CodeInfo(const void* data) {
    uint8_t* ptr = reinterpret_cast<uint8_t*>(const_cast<void*>(data));
    header_.Decode(const_cast<const uint8_t**>(&ptr));
    stack_maps_region_ = MemoryRegion(ptr, header_.number_of_stack_maps * header_.stack_map_size);
    ptr += stack_maps_region_.size();
    dex_register_maps_region_ = MemoryRegion(ptr, header_.dex_register_maps_size);
    ptr += dex_register_maps_region_.size();
    inline_infos_region_ = MemoryRegion(ptr, header_.inline_infos_size);
    ptr += inline_infos_region_.size();
  }

  explicit CodeInfo(MemoryRegion region) : CodeInfo(region.start()) {
    DCHECK_EQ(inline_infos_region_.end(), region.end());
  }

  bool HasInlineInfo() const {
    return header_.stack_map_encoding->GetInlineInfoEncoding().BitSize() > 0;
  }

  StackMap GetStackMapAt(size_t i) const {
    return StackMap(header_.stack_map_encoding,
                    stack_maps_region_.Subregion(header_.stack_map_size * i,
                                                 header_.stack_map_size));
  }

  uint32_t GetNumberOfStackMaps() const {
    return header_.number_of_stack_maps;
  }

  // Get the number dex register locations encoded for given stack map.
  // It may also optionally include inlined dex register locations.
  size_t GetNumberOfDexRegistersOf(const StackMap& stack_map, uint32_t max_depth) const {
    size_t count = 0;
    if ((stack_map.GetFlags() & StackMap::Flags::kHasAnyDexRegisters) != 0) {
      count += header_.number_of_dex_registers;
    }
    if (stack_map.HasInlineInfo()) {
      InlineInfo inline_info = GetInlineInfoOf(stack_map);
      uint32_t depth = inline_info.GetDepth();
      for (uint32_t d = 0; d < depth && d < max_depth; ++d) {
        count += inline_info.GetNumberOfDexRegistersAtDepth(d);
      }
    }
    return count;
  }

  // Return all DexRegisterMaps - one for each stack map. Inlined info is ignored.
  std::vector<DexRegisterMap> GetDexRegisterMaps() const;

  DexRegisterMap GetDexRegisterMapOf(StackMap stack_map,
                                     uint32_t number_of_dex_registers) const;

  // Return the `DexRegisterMap` pointed by `inline_info` at depth `depth`.
  DexRegisterMap GetDexRegisterMapAtDepth(size_t depth,
                                          InlineInfo for_inline_info,
                                          uint32_t number_of_dex_registers) const;

  InlineInfo GetInlineInfoOf(StackMap stack_map) const {
    DCHECK(stack_map.HasInlineInfo());
    uint32_t offset = stack_map.GetInlineDescriptorOffset();
    return InlineInfo(header_.inline_info_encoding, inline_infos_region_, offset);
  }

  StackMap GetStackMapForDexPc(uint32_t dex_pc) const {
    for (size_t i = 0, e = GetNumberOfStackMaps(); i < e; ++i) {
      StackMap stack_map = GetStackMapAt(i);
      if (stack_map.GetDexPc() == dex_pc) {
        return stack_map;
      }
    }
    return StackMap();
  }

  // Searches the stack map list backwards because catch stack maps are stored
  // at the end.
  StackMap GetCatchStackMapForDexPc(uint32_t dex_pc) const {
    for (size_t i = GetNumberOfStackMaps(); i > 0; --i) {
      StackMap stack_map = GetStackMapAt(i - 1);
      if (stack_map.GetDexPc() == dex_pc) {
        return stack_map;
      }
    }
    return StackMap();
  }

  StackMap GetOsrStackMapForDexPc(uint32_t dex_pc) const {
    size_t e = GetNumberOfStackMaps();
    if (e == 0) {
      // There cannot be OSR stack map if there is no stack map.
      return StackMap();
    }
    // Walk over all stack maps. If two consecutive stack maps are identical, then we
    // have found a stack map suitable for OSR.
    for (size_t i = 0; i < e - 1; ++i) {
      StackMap stack_map = GetStackMapAt(i);
      if (stack_map.GetDexPc() == dex_pc) {
        StackMap other = GetStackMapAt(i + 1);
        if (other.GetDexPc() == dex_pc &&
            other.GetNativePcOffset() == stack_map.GetNativePcOffset()) {
          DCHECK(!stack_map.HasInlineInfo());
          if (i < e - 2) {
            // Make sure there are not three identical stack maps following each other.
            DCHECK_NE(stack_map.GetNativePcOffset(),
                      GetStackMapAt(i + 2).GetNativePcOffset());
          }
          return stack_map;
        }
      }
    }
    return StackMap();
  }

  StackMap GetStackMapForNativePcOffset(uint32_t native_pc_offset) const {
    // TODO: Safepoint stack maps are sorted by native_pc_offset but catch stack
    //       maps are not. If we knew that the method does not have try/catch,
    //       we could do binary search.
    for (size_t i = 0, e = GetNumberOfStackMaps(); i < e; ++i) {
      StackMap stack_map = GetStackMapAt(i);
      if (stack_map.GetNativePcOffset() == native_pc_offset) {
        return stack_map;
      }
    }
    return StackMap();
  }

  // Dump this CodeInfo object on `os`.  `code_offset` is the (absolute)
  // native PC of the compiled method and `number_of_dex_registers` the
  // number of Dex virtual registers used in this method.  If
  // `dump_stack_maps` is true, also dump the stack maps and the
  // associated Dex register maps.
  void Dump(VariableIndentationOutputStream* vios,
            uint32_t code_offset,
            uint16_t number_of_dex_registers,
            bool dump_stack_maps) const;

 private:
  // Internal method to decode dex register locations for one stack map.
  // This method must be called repeatedly to search thought the encoded data.
  // The arguments keep the state and need to be preserved between calls.
  // Returns the number of valid entries (since the location vector never shrinks).
  size_t DecodeNextDexRegisterMap(const StackMap& stack_map,
                                  size_t* encoded_offset,
                                  dchecked_vector<DexRegisterLocation>* locations) const;

  CodeInfoHeader header_;
  MemoryRegion stack_maps_region_;
  MemoryRegion inline_infos_region_;
  MemoryRegion dex_register_maps_region_;
  friend class StackMapStream;
};

#undef ELEMENT_BYTE_OFFSET_AFTER
#undef ELEMENT_BIT_OFFSET_AFTER

}  // namespace art

#endif  // ART_RUNTIME_STACK_MAP_H_
