/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_COMPILER_UTILS_MIPS_ASSEMBLER_MIPS_H_
#define ART_COMPILER_UTILS_MIPS_ASSEMBLER_MIPS_H_

#include <deque>
#include <vector>

#include "base/macros.h"
#include "constants_mips.h"
#include "globals.h"
#include "managed_register_mips.h"
#include "utils/array_ref.h"
#include "utils/assembler.h"
#include "offsets.h"

namespace art {
namespace mips {

enum class Condition {
  kEq,
  kNe,
  kLtz,
  kLez,
  kGtz,
  kGez,
  kNoCondition = -1,
};

std::ostream& operator<<(std::ostream& os, const Condition& rhs);

enum LoadOperandType {
  kLoadSignedByte,
  kLoadUnsignedByte,
  kLoadSignedHalfword,
  kLoadUnsignedHalfword,
  kLoadWord,
  kLoadWordPair,  // TODO: Remove?
  kLoadSWord,
  kLoadDWord
};

std::ostream& operator<<(std::ostream& os, const LoadOperandType& rhs);

enum StoreOperandType {
  kStoreByte,
  kStoreHalfword,
  kStoreWord,
  kStoreWordPair,  // TODO: Remove?
  kStoreSWord,
  kStoreDWord
};

std::ostream& operator<<(std::ostream& os, const StoreOperandType& rhs);

class MipsAssembler FINAL : public Assembler {
 public:
  MipsAssembler()
      : fixups_(),
        fixup_dependents_(),
        last_position_adjustment_(0u),
        last_old_position_(0u),
        last_fixup_id_(0u) {}
  virtual ~MipsAssembler() {}

  void FinalizeCode() OVERRIDE;

  // TODO: Check the order/use of the `rs` and `rt` registers.

  // Emit Machine Instructions.
  void Add(Register rd, Register rs, Register rt);
  void Addu(Register rd, Register rs, Register rt);
  void Addi(Register rt, Register rs, uint16_t imm16);
  void Addiu(Register rt, Register rs, uint16_t imm16);
  void Sub(Register rd, Register rs, Register rt);
  void Subu(Register rd, Register rs, Register rt);
  void Mult(Register rs, Register rt);
  void Multu(Register rs, Register rt);
  void Div(Register rs, Register rt);
  void Divu(Register rs, Register rt);

  void And(Register rd, Register rs, Register rt);
  void Andi(Register rt, Register rs, uint16_t imm16);
  void Or(Register rd, Register rs, Register rt);
  void Ori(Register rt, Register rs, uint16_t imm16);
  void Xor(Register rd, Register rs, Register rt);
  void Xori(Register rt, Register rs, uint16_t imm16);
  void Nor(Register rd, Register rs, Register rt);

  void Seb(Register rd, Register rt);
  void Seh(Register rd, Register rt);

  void Sll(Register rd, Register rs, int shamt);
  void Srl(Register rd, Register rs, int shamt);
  void Sra(Register rd, Register rs, int shamt);
  void Sllv(Register rd, Register rs, Register rt);
  void Srlv(Register rd, Register rs, Register rt);
  void Srav(Register rd, Register rs, Register rt);

  void Lb(Register rt, Register rs, uint16_t imm16);
  void Lh(Register rt, Register rs, uint16_t imm16);
  void Lw(Register rt, Register rs, uint16_t imm16);
  void Lbu(Register rt, Register rs, uint16_t imm16);
  void Lhu(Register rt, Register rs, uint16_t imm16);
  void Lui(Register rt, uint16_t imm16);
  void Sync(uint32_t stype);
  void Mfhi(Register rd);
  void Mflo(Register rd);

  void Sb(Register rt, Register rs, uint16_t imm16);
  void Sh(Register rt, Register rs, uint16_t imm16);
  void Sw(Register rt, Register rs, uint16_t imm16);

  void Slt(Register rd, Register rs, Register rt);
  void Sltu(Register rd, Register rs, Register rt);
  void Slti(Register rt, Register rs, uint16_t imm16);
  void Sltiu(Register rt, Register rs, uint16_t imm16);

  void B(uint16_t offset);
  void Bal(uint16_t offset);
  void Beq(Register rt, Register rs, uint16_t offset);
  void Bne(Register rt, Register rs, uint16_t offset);
  void Bltz(Register rs, uint16_t offset);
  void Blez(Register rs, uint16_t offset);
  void Bgtz(Register rs, uint16_t offset);
  void Bgez(Register rs, uint16_t offset);

  void J(uint32_t address);
  void Jal(uint32_t address);
  void Jr(Register rs);
  void Jalr(Register rd, Register rs);
  void Jalr(Register rs);

  void AddS(FRegister fd, FRegister fs, FRegister ft);
  void SubS(FRegister fd, FRegister fs, FRegister ft);
  void MulS(FRegister fd, FRegister fs, FRegister ft);
  void DivS(FRegister fd, FRegister fs, FRegister ft);
  void AddD(DRegister fd, DRegister fs, DRegister ft);
  void SubD(DRegister fd, DRegister fs, DRegister ft);
  void MulD(DRegister fd, DRegister fs, DRegister ft);
  void DivD(DRegister fd, DRegister fs, DRegister ft);
  void MovS(FRegister fd, FRegister fs);
  void MovD(DRegister fd, DRegister fs);
  void NegS(FRegister fd, FRegister fs);
  void NegD(DRegister fd, DRegister fs);

  void Cvtsw(FRegister fd, FRegister fs);
  void Cvtdw(DRegister fd, FRegister fs);
  void Cvtsd(FRegister fd, DRegister fs);
  void Cvtds(DRegister fd, FRegister fs);

  void Mfc1(Register rt, FRegister fs);
  void Mtc1(FRegister ft, Register rs);
  void Lwc1(FRegister ft, Register rs, uint16_t imm16);
  void Ldc1(DRegister ft, Register rs, uint16_t imm16);
  void Swc1(FRegister ft, Register rs, uint16_t imm16);
  void Sdc1(DRegister ft, Register rs, uint16_t imm16);

  void Break();
  void Nop();
  void Move(Register rt, Register rs);
  void Clear(Register rt);
  void Not(Register rt, Register rs);
  void Mul(Register rd, Register rs, Register rt);
  void Div(Register rd, Register rs, Register rt);
  void Rem(Register rd, Register rs, Register rt);

  void AddConstant(Register rt, Register rs, int32_t value);
  void LoadImmediate(Register rt, int32_t value);
  void LoadSImmediate(FRegister rt, float value);
  void LoadDImmediate(DRegister rt, double value);

  void EmitLoad(ManagedRegister m_dst, Register src_register, int32_t src_offset, size_t size);
  void LoadFromOffset(LoadOperandType type, Register reg, Register base, int32_t offset);
  void LoadSFromOffset(FRegister reg, Register base, int32_t offset);
  void LoadDFromOffset(DRegister reg, Register base, int32_t offset);
  void StoreToOffset(StoreOperandType type, Register reg, Register base, int32_t offset);
  void StoreSToOffset(FRegister reg, Register base, int32_t offset);
  void StoreDToOffset(DRegister reg, Register base, int32_t offset);

  // Push `rd` to the stack and adjust the stack frame.
  void Push(Register rd);
  // Pop `rd` from the stack and adjust the stack frame.
  void Pop(Register rd);

  // Label-aware branch instructions.
  void B(Label* label);
  void Bal(Label* label);
  void Beq(Register rt, Register rs, Label* label);
  void Bne(Register rt, Register rs, Label* label);
  void Bltz(Register rs, Label* label);
  void Blez(Register rs, Label* label);
  void Bgtz(Register rs, Label* label);
  void Bgez(Register rs, Label* label);
  void BranchOnLowerThan(Register rt, Register rs, Label* label);
  void BranchOnLowerThanOrEqual(Register rt, Register rs, Label* label);
  void BranchOnGreaterThan(Register rt, Register rs, Label* label);
  void BranchOnGreaterThanOrEqual(Register rt, Register rs, Label* label);
  void BranchOnLowerThanUnsigned(Register rt, Register rs, Label* label);
  void BranchOnLowerThanOrEqualUnsigned(Register rt, Register rs, Label* label);
  void BranchOnGreaterThanUnsigned(Register rt, Register rs, Label* label);
  void BranchOnGreaterThanOrEqualUnsigned(Register rt, Register rs, Label* label);

  void Bind(Label* label, bool is_jump);

  // Emit data (e.g. encoded instruction or immediate) to the instruction stream.
  void Emit(int32_t value);

  // Adjust label position.
  void AdjustLabelPosition(Label* label);

  // Get the final position of a label after local fixup based on the old position
  // recorded before FinalizeCode().
  uint32_t GetAdjustedPosition(uint32_t old_position);

  //
  // Overridden common assembler high-level functionality
  //

  // Emit code that will create an activation on the stack
  void BuildFrame(size_t frame_size, ManagedRegister method_reg,
                  const std::vector<ManagedRegister>& callee_save_regs,
                  const ManagedRegisterEntrySpills& entry_spills) OVERRIDE;

  // Emit code that will remove an activation from the stack
  void RemoveFrame(size_t frame_size, const std::vector<ManagedRegister>& callee_save_regs)
      OVERRIDE;

  void IncreaseFrameSize(size_t adjust) OVERRIDE;
  void DecreaseFrameSize(size_t adjust) OVERRIDE;

  // Store routines
  void Store(FrameOffset offs, ManagedRegister msrc, size_t size) OVERRIDE;
  void StoreRef(FrameOffset dest, ManagedRegister msrc) OVERRIDE;
  void StoreRawPtr(FrameOffset dest, ManagedRegister msrc) OVERRIDE;

  void StoreImmediateToFrame(FrameOffset dest, uint32_t imm, ManagedRegister mscratch) OVERRIDE;

  void StoreImmediateToThread32(ThreadOffset<4> dest, uint32_t imm, ManagedRegister mscratch)
      OVERRIDE;

  void StoreStackOffsetToThread32(ThreadOffset<4> thr_offs, FrameOffset fr_offs,
                                  ManagedRegister mscratch) OVERRIDE;

  void StoreStackPointerToThread32(ThreadOffset<4> thr_offs) OVERRIDE;

  void StoreSpanning(FrameOffset dest, ManagedRegister msrc, FrameOffset in_off,
                     ManagedRegister mscratch) OVERRIDE;

  // Load routines
  void Load(ManagedRegister mdest, FrameOffset src, size_t size) OVERRIDE;

  void LoadFromThread32(ManagedRegister mdest, ThreadOffset<4> src, size_t size) OVERRIDE;

  void LoadRef(ManagedRegister dest, FrameOffset src) OVERRIDE;

  void LoadRef(ManagedRegister mdest, ManagedRegister base, MemberOffset offs,
               bool unpoison_reference) OVERRIDE;

  void LoadRawPtr(ManagedRegister mdest, ManagedRegister base, Offset offs) OVERRIDE;

  void LoadRawPtrFromThread32(ManagedRegister mdest, ThreadOffset<4> offs) OVERRIDE;

  // Copying routines
  void Move(ManagedRegister mdest, ManagedRegister msrc, size_t size) OVERRIDE;

  void CopyRawPtrFromThread32(FrameOffset fr_offs, ThreadOffset<4> thr_offs,
                              ManagedRegister mscratch) OVERRIDE;

  void CopyRawPtrToThread32(ThreadOffset<4> thr_offs, FrameOffset fr_offs,
                            ManagedRegister mscratch) OVERRIDE;

  void CopyRef(FrameOffset dest, FrameOffset src, ManagedRegister mscratch) OVERRIDE;

  void Copy(FrameOffset dest, FrameOffset src, ManagedRegister mscratch, size_t size) OVERRIDE;

  void Copy(FrameOffset dest, ManagedRegister src_base, Offset src_offset, ManagedRegister mscratch,
            size_t size) OVERRIDE;

  void Copy(ManagedRegister dest_base, Offset dest_offset, FrameOffset src,
            ManagedRegister mscratch, size_t size) OVERRIDE;

  void Copy(FrameOffset dest, FrameOffset src_base, Offset src_offset, ManagedRegister mscratch,
            size_t size) OVERRIDE;

  void Copy(ManagedRegister dest, Offset dest_offset, ManagedRegister src, Offset src_offset,
            ManagedRegister mscratch, size_t size) OVERRIDE;

  void Copy(FrameOffset dest, Offset dest_offset, FrameOffset src, Offset src_offset,
            ManagedRegister mscratch, size_t size) OVERRIDE;

  void MemoryBarrier(ManagedRegister) OVERRIDE;

  // Sign extension
  void SignExtend(ManagedRegister mreg, size_t size) OVERRIDE;

  // Zero extension
  void ZeroExtend(ManagedRegister mreg, size_t size) OVERRIDE;

  // Exploit fast access in managed code to Thread::Current()
  void GetCurrentThread(ManagedRegister tr) OVERRIDE;
  void GetCurrentThread(FrameOffset dest_offset, ManagedRegister mscratch) OVERRIDE;

  // Set up out_reg to hold a Object** into the handle scope, or to be null if the
  // value is null and null_allowed. in_reg holds a possibly stale reference
  // that can be used to avoid loading the handle scope entry to see if the value is
  // null.
  void CreateHandleScopeEntry(ManagedRegister out_reg, FrameOffset handlescope_offset,
                              ManagedRegister in_reg, bool null_allowed) OVERRIDE;

  // Set up out_off to hold a Object** into the handle scope, or to be null if the
  // value is null and null_allowed.
  void CreateHandleScopeEntry(FrameOffset out_off, FrameOffset handlescope_offset,
                              ManagedRegister mscratch, bool null_allowed) OVERRIDE;

  // src holds a handle scope entry (Object**) load this into dst
  void LoadReferenceFromHandleScope(ManagedRegister dst, ManagedRegister src) OVERRIDE;

  // Heap::VerifyObject on src. In some cases (such as a reference to this) we
  // know that src may not be null.
  void VerifyObject(ManagedRegister src, bool could_be_null) OVERRIDE;
  void VerifyObject(FrameOffset src, bool could_be_null) OVERRIDE;

  // Call to address held at [base+offset]
  void Call(ManagedRegister base, Offset offset, ManagedRegister mscratch) OVERRIDE;
  void Call(FrameOffset base, Offset offset, ManagedRegister mscratch) OVERRIDE;
  void CallFromThread32(ThreadOffset<4> offset, ManagedRegister mscratch) OVERRIDE;

  // Generate code to check if Thread::Current()->exception_ is non-null
  // and branch to a ExceptionSlowPath if it is.
  void ExceptionPoll(ManagedRegister mscratch, size_t stack_adjust) OVERRIDE;

 private:
  void EmitR(int opcode, Register rs, Register rt, Register rd, int shamt, int funct);
  void EmitI(int opcode, Register rs, Register rt, uint16_t imm);
  void EmitJ(int opcode, int address);
  void EmitFR(int opcode, int fmt, FRegister ft, FRegister fs, FRegister fd, int funct);
  void EmitFI(int opcode, int fmt, FRegister rt, uint16_t imm);

  int32_t EncodeBranchOffset(int offset, int32_t inst, bool is_jump);
  int DecodeBranchOffset(int32_t inst, bool is_jump);


  typedef uint32_t FixupId;

  class Fixup {
   public:
    // Branch type.
    enum class Type : uint8_t {
      kUnconditionalBranch,             // B.
      kConditionalBranch,               // Beq, Bne.
      kConditionalBranchCompareToZero,  // Bgtz, Bgez, Bltz, Blez.
    };

    // Calculated size of branch instruction based on type and offset.
    enum class Size : uint8_t {
      // Branch encodings supporting 18-bit branch offsets.
      kShortUnconditionalBranch,
      kShortConditionalBranch,
      kShortConditionalBranchCompareToZero,

      // Branch encodings supporting 32-bit branch offsets.
      kLargeUnconditionalBranch,
      kLargeConditionalBranch,
      kLargeConditionalBranchCompareToZero,
    };

    // Unresolved unconditional branch.
    static Fixup UnconditionalBranch(uint32_t location) {
      return Fixup(kNoRegister,
                   kNoRegister,
                   Condition::kNoCondition,
                   Type::kUnconditionalBranch,
                   Size::kShortUnconditionalBranch,
                   location);
    }

    // Unresolved conditional branch.
    static Fixup ConditionalBranch(uint32_t location, Register rt, Register rs, Condition cond) {
      return Fixup(rt,
                   rs,
                   cond,
                   Type::kConditionalBranch,
                   Size::kShortConditionalBranch,
                   location);
    }

    // Unresolved conditional branch compare to zero.
    static Fixup ConditionalBranchCompareToZero(uint32_t location, Register rs, Condition cond) {
      return Fixup(kNoRegister,
                   rs,
                   cond,
                   Type::kConditionalBranchCompareToZero,
                   Size::kShortConditionalBranchCompareToZero,
                   location);
    }

    Type GetType() const {
      return type_;
    }

    Size GetOriginalSize() const {
      return original_size_;
    }

    Size GetSize() const {
      return size_;
    }

    uint32_t GetOriginalSizeInBytes() const {
      return SizeInBytes(original_size_);
    }

    uint32_t GetSizeInBytes() const {
      return SizeInBytes(size_);
    }

    uint32_t GetLocation() const {
      return location_;
    }

    uint32_t GetAdjustment() const {
      return adjustment_;
    }

    // Prepare the assembler->fixup_dependents_ and each Fixup's dependents_start_/count_.
    static void PrepareDependents(MipsAssembler* assembler);

    ArrayRef<FixupId> Dependents(const MipsAssembler& assembler) const {
      return ArrayRef<FixupId>(assembler.fixup_dependents_.get() + dependents_start_,
                               dependents_count_);
    }

    // Resolve a branch when the target is known.
    void Resolve(uint32_t target) {
      CHECK_EQ(target_, kUnresolved);
      CHECK_NE(target, kUnresolved);
      target_ = target;
    }

    // Check if the current size is OK for current location_, target_ and adjustment_.
    // If not, increase the size. Return the size increase, 0 if unchanged.
    // If the target if after this Fixup, also add the difference to adjustment_,
    // so that we don't need to consider forward Fixups as their own dependencies.
    uint32_t AdjustSizeIfNeeded(uint32_t current_code_size);

    // Increase adjustments. This is called for dependents of a Fixup when its size changes.
    void IncreaseAdjustment(uint32_t increase) {
      adjustment_ += increase;
    }

    // Finalize the branch with an adjustment to the location. Both location and target are updated.
    void Finalize(uint32_t location_adjustment) {
      CHECK_NE(target_, kUnresolved);
      location_ += location_adjustment;
      target_ += location_adjustment;
    }

    // Emit instruction(s) into the assembler buffer.
    void Emit(AssemblerBuffer* buffer) const;

   private:
    Fixup(Register rt, Register rs, Condition cond, Type type, Size size, uint32_t location)
        : rt_(rt),
          rs_(rs),
          cond_(cond),
          type_(type),
          original_size_(size),
          size_(size),
          location_(location),
          target_(kUnresolved),
          adjustment_(0u),
          dependents_count_(0u),
          dependents_start_(0u) {}

    static size_t SizeInBytes(Size size);

    // Returns the offset from the PC-using instruction to the target.
    int32_t GetOffset() const;

    size_t IncreaseSize(Size new_size);

    static constexpr uint32_t kUnresolved = 0xffffffff;     // Value for target_ for unresolved.

    const Register rt_;   // For kConditional.
    const Register rs_;   // For kConditional.
    const Condition cond_;
    const Type type_;
    Size original_size_;
    Size size_;
    uint32_t location_;     // Offset into assembler buffer in bytes.
    uint32_t target_;       // Offset into assembler buffer in bytes.
    uint32_t adjustment_;   // The number of extra bytes inserted between location_ and target_.
    // Fixups that require adjustment when current size changes are stored in a single
    // array in the assembler and we store only the start index and count here.
    uint32_t dependents_count_;
    uint32_t dependents_start_;
  };

  FixupId AddFixup(Fixup fixup) {
    FixupId fixup_id = static_cast<FixupId>(fixups_.size());
    fixups_.push_back(fixup);
    // For iterating using FixupId, we need the next id to be representable.
    CHECK_EQ(static_cast<size_t>(static_cast<FixupId>(fixups_.size())), fixups_.size());
    return fixup_id;
  }

  Fixup* GetFixup(FixupId fixup_id) {
    CHECK_LT(fixup_id, fixups_.size());
    return &fixups_[fixup_id];
  }

  void AdjustFixupIfNeeded(Fixup* fixup, uint32_t* current_code_size,
                           std::deque<FixupId>* fixups_to_recalculate);
  uint32_t AdjustFixups();
  void EmitFixups(uint32_t adjusted_code_size);

  void EmitUnconditionalBranchFixup(Label* label);
  void EmitConditionalBranchFixup(Register rt, Register rs, Label* label, Condition condition);
  void EmitConditionalBranchCompareToZeroFixup(Register rs, Label* label, Condition condition);
  void EmitBranchFixupHelper(Fixup fixup, Label* label);

  std::vector<Fixup> fixups_;
  std::unique_ptr<FixupId[]> fixup_dependents_;

  // Data for AdjustedPosition(), see the description there.
  uint32_t last_position_adjustment_;
  uint32_t last_old_position_;
  FixupId last_fixup_id_;

  friend std::ostream& operator<<(std::ostream& os, const Fixup::Type& rhs);
  friend std::ostream& operator<<(std::ostream& os, const Fixup::Size& rhs);

  DISALLOW_COPY_AND_ASSIGN(MipsAssembler);
};

std::ostream& operator<<(std::ostream& os, const MipsAssembler::Fixup::Type& rhs);
std::ostream& operator<<(std::ostream& os, const MipsAssembler::Fixup::Size& rhs);

// Slowpath entered when Thread::Current()->_exception is non-null
class MipsExceptionSlowPath FINAL : public SlowPath {
 public:
  MipsExceptionSlowPath(MipsManagedRegister scratch, size_t stack_adjust)
      : scratch_(scratch), stack_adjust_(stack_adjust) {}
  virtual void Emit(Assembler *sp_asm) OVERRIDE;
 private:
  const MipsManagedRegister scratch_;
  const size_t stack_adjust_;
};

}  // namespace mips
}  // namespace art

#endif  // ART_COMPILER_UTILS_MIPS_ASSEMBLER_MIPS_H_
