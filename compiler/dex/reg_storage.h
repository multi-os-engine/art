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

#ifndef ART_COMPILER_DEX_REG_STORAGE_H_
#define ART_COMPILER_DEX_REG_STORAGE_H_


namespace art {

/*
 * Representation of the physical register, register pair or vector holding a Dalvik value.
 * The basic configuration of the storage (i.e. solo reg, pair, vector, floating point) is
 * common across all targets, but the encoding of the actual storage element (and how that
 * maps to the def/use mask) is target independent.
 *
 * The two most-significant bits describe the basic shape of the storage, while meaning of the
 * lower 14 bits depends on the shape:
 *
 *  [PW]
 *       P:  0  -> pair, 1 -> solo (or vector)
 *       W:  1  -> 64 bits, 0 -> 32 bits
 *
 * We use the most significant bit of the lower 7 group to describe the type of register.
 *
 *  [F]
 *       0 -> Core
 *       1 -> Floating point
 *
 * Followed by a bit used, when applicable, to designate single or double precision float.
 *
 *  [D]
 *      0 -> Single precison
 *      1 -> Double precison
 *
 * The low 5/6 bits yield the actual resource number.
 *
 *  [00]                [xxxxxxxxxxxxxx]  Invalid
 *  [01] [F] [0] [HHHHH] [F] [0] [LLLLL]  64-bit storage, composed of 2 32-bit registers
 *  [10] [0] [xxxxxx]    [F] [D] [RRRRR]  32-bit solo register
 *  [11] [0] [xxxxxx]    [F] [D] [RRRRR]  64-bit solo register
 *  [10] [1] [xxxxxx]    [F]    [VVVVVV]  32-bit vector storage
 *  [11] [1] [xxxxxx]    [F]    [VVVVVV]  64-bit vector storage
 *
 * x - don't care
 * L - low register number of a pair
 * H - high register number of a pair
 * R - register number of a solo reg
 * V - vector description
 *
 * Note that in all non-invalid cases, we can determine if the storage is floating point
 * by testing bit 6.  This must remain true across targets.  Though it appears to be
 * permitted by the format, the [F][D] values from each half of a pair must match (to
 * allow the high and low regs of a pair to be individually manipulated).
 *
 * Expansion note: As defined, we limit the number of registers to 32 per class.  Should
 * special or coprocessor registers need to be represented, expansion in the solo register
 * form can be done by using an [FD] encoding of [01], taking care not to conflict with
 * kInvalidRegVal.
 *
 */

class RegStorage {
 public:
  enum RegStorageKind {
    kInvalid     = 0x0000,
    k64BitPair   = 0x4000,
    k32BitSolo   = 0x8000,
    k64BitSolo   = 0xc000,
    k32BitVector = 0xa000,
    k64BitVector = 0xe000,
    kVectorMask  = 0xe000,
    kPairMask    = 0x8000,
    kPair        = 0x0000,
    kSizeMask    = 0x4000,
    k64Bit       = 0x4000,
    k32Bit       = 0x0000,
    kVector      = 0xa000,
    kSolo        = 0x8000,
    kShapeMask   = 0xc000,
    kKindMask    = 0xe000,
    kFloat       = 0x0040,
    kDouble      = 0x0020,
    kFloatMask   = 0x0060
  };

  static const uint16_t kRegValMask = 0x007f;
  static const uint16_t kRegNumMask = 0x001f;
  // TODO: deprecate use of kInvalidRegVal and speed up GetReg().
  static const uint16_t kInvalidRegVal = 0x0020;
  static const uint16_t kHighRegShift = 7;
  static const uint16_t kHighRegMask = kRegValMask << kHighRegShift;

  RegStorage(RegStorageKind rs_kind, int reg) {
    DCHECK_NE(rs_kind & kShapeMask, kInvalid);
    DCHECK_NE(rs_kind & kShapeMask, k64BitPair);
    DCHECK_EQ(rs_kind & ~kKindMask, 0);
    DCHECK_EQ(reg & ~kRegValMask, 0);
    reg_ = rs_kind | reg;
  }
  RegStorage(RegStorageKind rs_kind, int low_reg, int high_reg) {
    DCHECK_EQ(rs_kind, k64BitPair);
    DCHECK_EQ(low_reg & ~kRegValMask, 0);
    DCHECK_EQ(high_reg & ~kRegValMask, 0);
    reg_ = rs_kind | (high_reg << kHighRegShift) | low_reg;
  }
  explicit RegStorage(uint16_t val) : reg_(val) {}
  RegStorage() : reg_(kInvalid) {}
  ~RegStorage() {}

  bool operator==(const RegStorage rhs) const {
    return (reg_ == rhs.GetRawBits());
  }

  bool operator!=(const RegStorage rhs) const {
    return (reg_ != rhs.GetRawBits());
  }

  bool Valid() const {
    return ((reg_ & kShapeMask) != kInvalid);
  }

  bool Is32Bit() const {
    return ((reg_ & kSizeMask) == k32Bit);
  }

  bool Is64Bit() const {
    return ((reg_ & kSizeMask) == k64Bit);
  }

  bool IsPair() const {
    return ((reg_ & kPairMask) == kPair);
  }

  bool IsSolo() const {
    return ((reg_ & kVector) == kSolo);
  }

  bool IsVector() const {
    return ((reg_ & kVector) == kVector);
  }

  bool Is32BitVector() const {
    return ((reg_ & kVectorMask) == k32BitVector);
  }

  bool Is64BitVector() const {
    return ((reg_ & kVectorMask) == k64BitVector);
  }

  bool IsFloat() const {
    DCHECK(Valid());
    return ((reg_ & kFloat) == kFloat);
  }

  bool IsDouble() const {
    DCHECK(Valid());
    return ((reg_ & kFloatMask) == (kFloat | kDouble));
  }

  bool IsSingle() const {
    DCHECK(Valid());
    return ((reg_ & kFloatMask) == kFloat);
  }

  static bool IsFloat(uint16_t reg) {
    return ((reg & kFloat) == kFloat);
  }

  static bool IsDouble(uint16_t reg) {
    return ((reg & kFloatMask) == (kFloat | kDouble));
  }

  static bool IsSingle(uint16_t reg) {
    return ((reg & kFloatMask) == kFloat);
  }

  // Used to retrieve either the low register of a pair, or the only register.
  int GetReg() const {
    DCHECK(!IsPair()) << "reg_ = 0x" << std::hex << reg_;
    return Valid() ? (reg_ & kRegValMask) : kInvalidRegVal;
  }

  void SetReg(int reg) {
    DCHECK(Valid());
    reg_ = (reg_ & ~kRegValMask) | reg;
  }

  void SetLowReg(int reg) {
    DCHECK(IsPair());
    reg_ = (reg_ & ~kRegValMask) | reg;
  }

  // Retrieve the least significant register of a pair.
  int GetLowReg() const {
    DCHECK(IsPair());
    return (reg_ & kRegValMask);
  }

  // Create a stand-alone RegStorage from the low reg of a pair.
  RegStorage GetLow() const {
    DCHECK(IsPair());
    return RegStorage(k32BitSolo, reg_ & kRegValMask);
  }

  // Retrieve the most significant register of a pair.
  int GetHighReg() const {
    DCHECK(IsPair());
    return (reg_ & kHighRegMask) >> kHighRegShift;
  }

  // Create a stand-alone RegStorage from the high reg of a pair.
  RegStorage GetHigh() const {
    DCHECK(IsPair());
    return RegStorage(k32BitSolo, (reg_ & kHighRegMask) >> kHighRegShift);
  }

  void SetHighReg(int reg) {
    DCHECK(IsPair());
    reg_ = (reg_ & ~kHighRegMask) | (reg << kHighRegShift);
    DCHECK_EQ(GetHighReg(), reg);
  }

  // Combine 2 32-bit solo regs into a pair.
  static RegStorage MakeRegPair(RegStorage low, RegStorage high) {
    DCHECK(!low.IsPair());
    DCHECK(low.Is32Bit());
    DCHECK(!high.IsPair());
    DCHECK(high.Is32Bit());
    return RegStorage(k64BitPair, low.GetReg(), high.GetReg());
  }

  // Create a 32-bit solo.
  static RegStorage Solo32(int reg_num) {
    return RegStorage(k32BitSolo, reg_num);
  }

  // Create a 64-bit solo.
  static RegStorage Solo64(int reg_num) {
    return RegStorage(k64BitSolo, reg_num);
  }

  static RegStorage InvalidReg() {
    return RegStorage(kInvalid);
  }

  int GetRawBits() const {
    return reg_;
  }

 private:
  uint16_t reg_;
};

}  // namespace art

#endif  // ART_COMPILER_DEX_REG_STORAGE_H_
