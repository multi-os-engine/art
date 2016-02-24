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

#define ELEMENT_BYTE_OFFSET_AFTER(PreviousElement) \
  k ## PreviousElement ## Offset + sizeof(PreviousElement ## Type)

#define ELEMENT_BIT_OFFSET_AFTER(PreviousElement) \
  k ## PreviousElement ## BitOffset + PreviousElement ## BitSize

class VariableIndentationOutputStream;

// Size of a frame slot, in bytes.  This constant is a signed value,
// to please the compiler in arithmetic operations involving int32_t
// (signed) values.
static constexpr ssize_t kFrameSlotSize = 4;

// Size of Dex virtual registers.
static constexpr size_t kVRegSize = 4;

class CodeInfo;
class StackMapEncoding;

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

  DexRegisterLocation(Kind kind, int32_t value) : kind_(kind), value_(value) {}

  static DexRegisterLocation None() {
    return DexRegisterLocation(Kind::kNone, 0);
  }

  // Get the kind of the location.
  Kind GetKind() const { return kind_; }

  // Get the value of the location.
  int32_t GetValue() const { return value_; }

  bool IsLive() const { return kind_ != Kind::kNone; }

  // Write the DexRegisterLocation to memory. It will consume either one or five bytes.
  // Large stack or constant locations are escaped by storing b11111 in the value field.
  void Encode(const MemoryRegion* region, size_t* offset) const;

  // Decode previously encoded value.
  static DexRegisterLocation Decode(const MemoryRegion* region, size_t* offset);

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
class DexRegisterMap {
 public:
  DexRegisterMap() : locations_() {}

  explicit DexRegisterMap(dchecked_vector<DexRegisterLocation>&& locations)
      : locations_(locations) {}

  bool IsValid() const { return !locations_.empty(); }

  size_t GetNumberOfLiveDexRegisters() const {
    size_t number_of_live_dex_registers = 0;
    for (const DexRegisterLocation& location : locations_) {
      if (location.IsLive()) {
        ++number_of_live_dex_registers;
      }
    }
    return number_of_live_dex_registers;
  }

  size_t Size() const { return locations_.size(); }

  void Dump(VariableIndentationOutputStream* vios) const;

  const DexRegisterLocation& operator[](size_t index) const {
    return locations_[index];
  }

 private:
  dchecked_vector<DexRegisterLocation> locations_;
};

typedef std::vector<DexRegisterMap> DexRegisterMaps;

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
    bit_offset += MinimumBitsToStore(dex_pc_max + 1 /* for kDexNoIndex */);

    flags_bit_offset_ = dchecked_integral_cast<uint8_t>(bit_offset);
    bit_offset += MinimumBitsToStore(flags_max);

    // We also need +1 for kNoInlineInfo, but since the inline_info_size is strictly
    // greater than the offset we might try to encode, we already implicitly have it.
    // If inline_info_size == 0, then kNoInlineInfo is the only encodable value.
    inline_info_bit_offset_ = dchecked_integral_cast<uint8_t>(bit_offset);
    bit_offset += MinimumBitsToStore(inline_info_size);

    register_mask_bit_offset_ = dchecked_integral_cast<uint8_t>(bit_offset);
    bit_offset += MinimumBitsToStore(register_mask_max);

    stack_mask_bit_offset_ = dchecked_integral_cast<uint8_t>(bit_offset);
    bit_offset += stack_mask_bit_size;

    return RoundUp(bit_offset, kBitsPerByte) / kBitsPerByte;
  }

  bool HasInlineInfo() const { return InlineInfoBitSize() > 0; }

  size_t NativePcBitSize() const {
    return dex_pc_bit_offset_ - kNativePcBitOffset;
  }

  size_t DexPcBitSize() const {
    return flags_bit_offset_ - dex_pc_bit_offset_;
  }

  size_t FlagsBitSize() const {
    return inline_info_bit_offset_ - flags_bit_offset_;
  }

  size_t InlineInfoBitSize() const {
    return register_mask_bit_offset_ - inline_info_bit_offset_;
  }

  size_t RegisterMaskBitSize() const {
    return stack_mask_bit_offset_  - register_mask_bit_offset_;
  }

  size_t NativePcBitOffset() const { return kNativePcBitOffset; }
  size_t DexPcBitOffset() const { return dex_pc_bit_offset_; }
  size_t FlagsBitOffset() const { return flags_bit_offset_; }
  size_t InlineInfoBitOffset() const { return inline_info_bit_offset_; }
  size_t RegisterMaskBitOffset() const { return register_mask_bit_offset_; }
  size_t StackMaskBitOffset() const { return stack_mask_bit_offset_; }

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
    // This flag is set if the stack map has any encoded dex register location mappings.
    // It is set if and only if StackMapEntry::num_dex_registers was non-zero.
    // Inlined infos may or may not have own mappings regardless of this flags.
    kHasDexRegisterMap = 1 << 0,
    // All locations (including inlined) are identical to the last state.
    kSameDexRegisterMap = 1 << 1,
  };

  StackMap() : region_(), encoding_() {}

  explicit StackMap(MemoryRegion region, const StackMapEncoding* encoding)
      : region_(region), encoding_(encoding) {}

  bool IsValid() const { return region_.pointer() != nullptr; }

  uint32_t GetDexPc() const {
    return LoadAtPlus1(encoding_->DexPcBitSize(), encoding_->DexPcBitOffset());
  }

  void SetDexPc(uint32_t dex_pc) {
    StoreAtPlus1(encoding_->DexPcBitSize(), encoding_->DexPcBitOffset(), dex_pc);
  }

  uint32_t GetNativePcOffset() const {
    return LoadAt(encoding_->NativePcBitSize(), encoding_->NativePcBitOffset());
  }

  void SetNativePcOffset(uint32_t native_pc_offset) {
    StoreAt(encoding_->NativePcBitSize(), encoding_->NativePcBitOffset(), native_pc_offset);
  }

  uint32_t GetFlags() const {
    return LoadAt(encoding_->FlagsBitSize(), encoding_->FlagsBitOffset());
  }

  void SetFlags(uint32_t offset) {
    StoreAt(encoding_->FlagsBitSize(), encoding_->FlagsBitOffset(), offset);
  }

  uint32_t GetInlineDescriptorOffset() const {
    return LoadAtPlus1(encoding_->InlineInfoBitSize(), encoding_->InlineInfoBitOffset());
  }

  void SetInlineDescriptorOffset(uint32_t offset) {
    StoreAtPlus1(encoding_->InlineInfoBitSize(), encoding_->InlineInfoBitOffset(), offset);
  }

  uint32_t GetRegisterMask() const {
    return LoadAt(encoding_->RegisterMaskBitSize(), encoding_->RegisterMaskBitOffset());
  }

  void SetRegisterMask(uint32_t mask) {
    StoreAt(encoding_->RegisterMaskBitSize(), encoding_->RegisterMaskBitOffset(), mask);
  }

  size_t GetNumberOfStackMaskBits() const {
    return region_.size_in_bits() - encoding_->StackMaskBitOffset();
  }

  bool GetStackMaskBit(size_t index) const {
    return region_.LoadBit(encoding_->StackMaskBitOffset() + index);
  }

  void SetStackMaskBit(size_t index, bool value) {
    region_.StoreBit(encoding_->StackMaskBitOffset() + index, value);
  }

  bool HasDexRegisterMap() const {
    return (GetFlags() & Flags::kHasDexRegisterMap) != 0;
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
            const std::string& header_suffix = "") const;

  // Special (invalid) offset for the InlineDescriptorOffset field meaning
  // that there is no inline info for this stack map.
  static constexpr uint32_t kNoInlineInfo = -1;

 private:
  // Load/store `bit_count` at the given `bit_offset` as uint32_t.
  uint32_t LoadAt(size_t bit_count, size_t bit_offset) const;
  void StoreAt(size_t bit_count, size_t bit_offset, uint32_t value);

  // The following variants store 'value + 1' which allows us to store -1.
  uint32_t LoadAtPlus1(size_t bit_count, size_t bit_offset) const {
    return LoadAt(bit_count, bit_offset) - 1;  // Can return -1.
  }
  void StoreAtPlus1(size_t bit_count, size_t bit_offset, uint32_t value) {
    return StoreAt(bit_count, bit_offset, value + 1);  // Can store -1.
  }

  MemoryRegion region_;
  const StackMapEncoding* encoding_;
};

/**
 * Inline information for a specific PC. The information is of the form:
 *
 *   [inlining_depth, entry+]
 *
 * where `entry` is of the form:
 *
 *   [dex_pc, method_index, num_dex_registers].
 */
class InlineInfo {
 public:
  // Memory layout: fixed contents.
  typedef uint8_t DepthType;
  // Memory layout: single entry contents.
  typedef uint32_t MethodIndexType;
  typedef uint32_t DexPcType;
  typedef uint8_t InvokeTypeType;
  typedef uint16_t NumDexRegistersType;

  explicit InlineInfo(MemoryRegion region) : region_(region) {}

  DepthType GetDepth() const {
    return region_.LoadUnaligned<DepthType>(kDepthOffset);
  }

  void SetDepth(DepthType depth) {
    region_.StoreUnaligned<DepthType>(kDepthOffset, depth);
  }

  MethodIndexType GetMethodIndexAtDepth(DepthType depth) const {
    return region_.LoadUnaligned<MethodIndexType>(
        kFixedSize + depth * SingleEntrySize() + kMethodIndexOffset);
  }

  void SetMethodIndexAtDepth(DepthType depth, MethodIndexType index) {
    region_.StoreUnaligned<MethodIndexType>(
        kFixedSize + depth * SingleEntrySize() + kMethodIndexOffset, index);
  }

  DexPcType GetDexPcAtDepth(DepthType depth) const {
    return region_.LoadUnaligned<DexPcType>(
        kFixedSize + depth * SingleEntrySize() + kDexPcOffset);
  }

  void SetDexPcAtDepth(DepthType depth, DexPcType dex_pc) {
    region_.StoreUnaligned<DexPcType>(
        kFixedSize + depth * SingleEntrySize() + kDexPcOffset, dex_pc);
  }

  InvokeTypeType GetInvokeTypeAtDepth(DepthType depth) const {
    return region_.LoadUnaligned<InvokeTypeType>(
        kFixedSize + depth * SingleEntrySize() + kInvokeTypeOffset);
  }

  void SetInvokeTypeAtDepth(DepthType depth, InvokeTypeType invoke_type) {
    region_.StoreUnaligned<InvokeTypeType>(
        kFixedSize + depth * SingleEntrySize() + kInvokeTypeOffset, invoke_type);
  }

  NumDexRegistersType GetNumDexRegistersAtDepth(DepthType depth) const {
    return region_.LoadUnaligned<NumDexRegistersType>(
        kFixedSize + depth * SingleEntrySize() + kNumDexRegistersOffset);
  }

  void SetNumDexRegistersAtDepth(DepthType depth, NumDexRegistersType offset) {
    region_.StoreUnaligned<NumDexRegistersType>(
        kFixedSize + depth * SingleEntrySize() + kNumDexRegistersOffset, offset);
  }

  bool HasDexRegisterMapAtDepth(DepthType depth) const {
    return GetNumDexRegistersAtDepth(depth) != 0;
  }

  static size_t SingleEntrySize() {
    return kFixedEntrySize;
  }

  void Dump(VariableIndentationOutputStream* vios, const CodeInfo& info) const;

  bool Equals(const InlineInfo& other) {
    return region_.start() == other.region_.start();
  }

 private:
  static constexpr int kDepthOffset = 0;
  static constexpr int kFixedSize = ELEMENT_BYTE_OFFSET_AFTER(Depth);

  static constexpr int kMethodIndexOffset = 0;
  static constexpr int kDexPcOffset = ELEMENT_BYTE_OFFSET_AFTER(MethodIndex);
  static constexpr int kInvokeTypeOffset = ELEMENT_BYTE_OFFSET_AFTER(DexPc);
  static constexpr int kNumDexRegistersOffset = ELEMENT_BYTE_OFFSET_AFTER(InvokeType);
  static constexpr int kFixedEntrySize = ELEMENT_BYTE_OFFSET_AFTER(NumDexRegisters);

  MemoryRegion region_;

  friend class CodeInfo;
  friend class StackMap;
  friend class StackMapStream;
};

// Most of the fields are encoded as ULEB128 to save space.
struct CodeInfoHeader {
  uint32_t number_of_stack_maps;  // Number of stack map entries.
  uint32_t stack_map_size;  // Byte size of single stack map entry.
  uint32_t inline_infos_size;
  uint32_t number_of_dex_registers;  // Excluding inlined.
  uint32_t dex_register_maps_size;

  void Decode(const uint8_t** data) {
    number_of_stack_maps = DecodeUnsignedLeb128(data);
    stack_map_size = DecodeUnsignedLeb128(data);
    inline_infos_size = DecodeUnsignedLeb128(data);
    number_of_dex_registers = DecodeUnsignedLeb128(data);
    dex_register_maps_size = DecodeUnsignedLeb128(data);
  }

  template<typename Vector>
  void Encode(Vector* dest) const {
    EncodeUnsignedLeb128(dest, number_of_stack_maps);
    EncodeUnsignedLeb128(dest, stack_map_size);
    EncodeUnsignedLeb128(dest, inline_infos_size);
    EncodeUnsignedLeb128(dest, number_of_dex_registers);
    EncodeUnsignedLeb128(dest, dex_register_maps_size);
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
    stack_map_encoding_ = reinterpret_cast<StackMapEncoding*>(ptr);
    ptr += sizeof(StackMapEncoding);
    stack_maps_region_ = MemoryRegion(ptr, header_.number_of_stack_maps * header_.stack_map_size);
    ptr += stack_maps_region_.size();
    inline_infos_region_ = MemoryRegion(ptr, header_.inline_infos_size);
    ptr += inline_infos_region_.size();
    dex_register_maps_region_ = MemoryRegion(ptr, header_.dex_register_maps_size);
  }

  explicit CodeInfo(MemoryRegion region) : CodeInfo(region.start()) {
    DCHECK_EQ(dex_register_maps_region_.end(), region.end());
  }

  bool HasInlineInfo() const {
    return stack_map_encoding_->InlineInfoBitSize() > 0;
  }

  void SetStackMapEncoding(StackMapEncoding& encoding) const {
    *stack_map_encoding_ = encoding;
  }

  StackMap GetStackMapAt(size_t i) const {
    return StackMap(stack_maps_region_.Subregion(header_.stack_map_size * i,
                                                 header_.stack_map_size),
                    stack_map_encoding_);
  }

  uint32_t GetNumberOfStackMaps() const {
    return header_.number_of_stack_maps;
  }

  // Get the number dex register for stack map excluding inlined one.
  size_t GetNumberOfDexRegistersOf(const StackMap& stack_map) const {
    return stack_map.HasDexRegisterMap() ? header_.number_of_dex_registers : 0;
  }

  // Return all DexRegisterMaps - one for each stack map. Inlined info is ignored.
  std::vector<DexRegisterMap> GetDexRegisterMaps() const;

  DexRegisterMap GetDexRegisterMapOf(StackMap for_stack_map) const;

  // Return the `DexRegisterMap` pointed by `inline_info` at depth `depth`.
  DexRegisterMap GetDexRegisterMapAtDepth(size_t depth, InlineInfo for_inline_info) const;

  InlineInfo GetInlineInfoOf(StackMap stack_map) const {
    DCHECK(stack_map.HasInlineInfo());
    uint32_t offset = stack_map.GetInlineDescriptorOffset();
    uint8_t depth = inline_infos_region_.LoadUnaligned<uint8_t>(offset);
    return InlineInfo(inline_infos_region_.Subregion(offset,
        InlineInfo::kFixedSize + depth * InlineInfo::SingleEntrySize()));
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
  StackMapEncoding* stack_map_encoding_;
  MemoryRegion stack_maps_region_;
  MemoryRegion inline_infos_region_;
  MemoryRegion dex_register_maps_region_;
  friend class StackMapStream;
};

#undef ELEMENT_BYTE_OFFSET_AFTER
#undef ELEMENT_BIT_OFFSET_AFTER

}  // namespace art

#endif  // ART_RUNTIME_STACK_MAP_H_
