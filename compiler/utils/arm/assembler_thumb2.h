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

#ifndef ART_COMPILER_UTILS_ARM_ASSEMBLER_THUMB2_H_
#define ART_COMPILER_UTILS_ARM_ASSEMBLER_THUMB2_H_

#include <vector>

#include "base/logging.h"
#include "constants_arm.h"
#include "utils/arm/managed_register_arm.h"
#include "utils/arm/assembler_arm.h"
#include "offsets.h"
#include "utils.h"

namespace art {
namespace arm {

// IfThen state for IT instructions.
enum ItState {
  kItOmitted,
  kItThen,
  kItT = kItThen,
  kItElse,
  kItE = kItElse
};

class Thumb2Assembler FINAL : public ArmAssembler {
 public:
  Thumb2Assembler() : force_32bit_(false), it_cond_index_(3), next_condition_(AL) {}
  virtual ~Thumb2Assembler() {}

  bool IsThumb() const {
    return true;
  }

  // Data-processing instructions.
  void and_(Register rd, Register rn, const ShifterOperand& so, Condition cond = AL) OVERRIDE;

  void eor(Register rd, Register rn, const ShifterOperand& so, Condition cond = AL) OVERRIDE;

  void sub(Register rd, Register rn, const ShifterOperand& so, Condition cond = AL) OVERRIDE;
  void subs(Register rd, Register rn, const ShifterOperand& so, Condition cond = AL) OVERRIDE;

  void rsb(Register rd, Register rn, const ShifterOperand& so, Condition cond = AL) OVERRIDE;
  void rsbs(Register rd, Register rn, const ShifterOperand& so, Condition cond = AL) OVERRIDE;

  void add(Register rd, Register rn, const ShifterOperand& so, Condition cond = AL) OVERRIDE;

  void adds(Register rd, Register rn, const ShifterOperand& so, Condition cond = AL) OVERRIDE;

  void adc(Register rd, Register rn, const ShifterOperand& so, Condition cond = AL) OVERRIDE;

  void sbc(Register rd, Register rn, const ShifterOperand& so, Condition cond = AL) OVERRIDE;

  void rsc(Register rd, Register rn, const ShifterOperand& so, Condition cond = AL) OVERRIDE;

  void tst(Register rn, const ShifterOperand& so, Condition cond = AL) OVERRIDE;

  void teq(Register rn, const ShifterOperand& so, Condition cond = AL) OVERRIDE;

  void cmp(Register rn, const ShifterOperand& so, Condition cond = AL) OVERRIDE;

  void cmn(Register rn, const ShifterOperand& so, Condition cond = AL) OVERRIDE;

  void orr(Register rd, Register rn, const ShifterOperand& so, Condition cond = AL) OVERRIDE;
  void orrs(Register rd, Register rn, const ShifterOperand& so, Condition cond = AL) OVERRIDE;

  void mov(Register rd, const ShifterOperand& so, Condition cond = AL) OVERRIDE;
  void movs(Register rd, const ShifterOperand& so, Condition cond = AL) OVERRIDE;

  void bic(Register rd, Register rn, const ShifterOperand& so, Condition cond = AL) OVERRIDE;

  void mvn(Register rd, const ShifterOperand& so, Condition cond = AL) OVERRIDE;
  void mvns(Register rd, const ShifterOperand& so, Condition cond = AL) OVERRIDE;

  // Miscellaneous data-processing instructions.
  void clz(Register rd, Register rm, Condition cond = AL) OVERRIDE;
  void movw(Register rd, uint16_t imm16, Condition cond = AL) OVERRIDE;
  void movt(Register rd, uint16_t imm16, Condition cond = AL) OVERRIDE;

  // Multiply instructions.
  void mul(Register rd, Register rn, Register rm, Condition cond = AL) OVERRIDE;
  void mla(Register rd, Register rn, Register rm, Register ra,
           Condition cond = AL) OVERRIDE;
  void mls(Register rd, Register rn, Register rm, Register ra,
           Condition cond = AL) OVERRIDE;
  void umull(Register rd_lo, Register rd_hi, Register rn, Register rm,
             Condition cond = AL) OVERRIDE;

  void sdiv(Register rd, Register rn, Register rm, Condition cond = AL) OVERRIDE;
  void udiv(Register rd, Register rn, Register rm, Condition cond = AL) OVERRIDE;

  // Load/store instructions.
  void ldr(Register rd, const Address& ad, Condition cond = AL) OVERRIDE;
  void str(Register rd, const Address& ad, Condition cond = AL) OVERRIDE;

  void ldrb(Register rd, const Address& ad, Condition cond = AL) OVERRIDE;
  void strb(Register rd, const Address& ad, Condition cond = AL) OVERRIDE;

  void ldrh(Register rd, const Address& ad, Condition cond = AL) OVERRIDE;
  void strh(Register rd, const Address& ad, Condition cond = AL) OVERRIDE;

  void ldrsb(Register rd, const Address& ad, Condition cond = AL) OVERRIDE;
  void ldrsh(Register rd, const Address& ad, Condition cond = AL) OVERRIDE;

  void ldrd(Register rd, const Address& ad, Condition cond = AL) OVERRIDE;
  void strd(Register rd, const Address& ad, Condition cond = AL) OVERRIDE;

  void ldm(BlockAddressMode am, Register base,
           RegList regs, Condition cond = AL) OVERRIDE;
  void stm(BlockAddressMode am, Register base,
           RegList regs, Condition cond = AL) OVERRIDE;

  void ldrex(Register rd, Register rn, Condition cond = AL) OVERRIDE;
  void strex(Register rd, Register rt, Register rn, Condition cond = AL) OVERRIDE;

  void ldrex(Register rd, Register rn, uint16_t imm, Condition cond = AL);
  void strex(Register rd, Register rt, Register rn, uint16_t imm, Condition cond = AL);


  // Miscellaneous instructions.
  void clrex(Condition cond = AL) OVERRIDE;
  void nop(Condition cond = AL) OVERRIDE;

  // Note that gdb sets breakpoints using the undefined instruction 0xe7f001f0.
  void bkpt(uint16_t imm16) OVERRIDE;
  void svc(uint32_t imm24) OVERRIDE;

  // If-then
  void it(Condition firstcond, ItState i1 = kItOmitted,
        ItState i2 = kItOmitted, ItState i3 = kItOmitted);

  void cbz(Register rn, Label* target);
  void cbnz(Register rn, Label* target);

  // Floating point instructions (VFPv3-D16 and VFPv3-D32 profiles).
  void vmovsr(SRegister sn, Register rt, Condition cond = AL) OVERRIDE;
  void vmovrs(Register rt, SRegister sn, Condition cond = AL) OVERRIDE;
  void vmovsrr(SRegister sm, Register rt, Register rt2, Condition cond = AL) OVERRIDE;
  void vmovrrs(Register rt, Register rt2, SRegister sm, Condition cond = AL) OVERRIDE;
  void vmovdrr(DRegister dm, Register rt, Register rt2, Condition cond = AL) OVERRIDE;
  void vmovrrd(Register rt, Register rt2, DRegister dm, Condition cond = AL) OVERRIDE;
  void vmovs(SRegister sd, SRegister sm, Condition cond = AL) OVERRIDE;
  void vmovd(DRegister dd, DRegister dm, Condition cond = AL) OVERRIDE;

  // Returns false if the immediate cannot be encoded.
  bool vmovs(SRegister sd, float s_imm, Condition cond = AL) OVERRIDE;
  bool vmovd(DRegister dd, double d_imm, Condition cond = AL) OVERRIDE;

  void vldrs(SRegister sd, const Address& ad, Condition cond = AL) OVERRIDE;
  void vstrs(SRegister sd, const Address& ad, Condition cond = AL) OVERRIDE;
  void vldrd(DRegister dd, const Address& ad, Condition cond = AL) OVERRIDE;
  void vstrd(DRegister dd, const Address& ad, Condition cond = AL) OVERRIDE;

  void vadds(SRegister sd, SRegister sn, SRegister sm, Condition cond = AL) OVERRIDE;
  void vaddd(DRegister dd, DRegister dn, DRegister dm, Condition cond = AL) OVERRIDE;
  void vsubs(SRegister sd, SRegister sn, SRegister sm, Condition cond = AL) OVERRIDE;
  void vsubd(DRegister dd, DRegister dn, DRegister dm, Condition cond = AL) OVERRIDE;
  void vmuls(SRegister sd, SRegister sn, SRegister sm, Condition cond = AL) OVERRIDE;
  void vmuld(DRegister dd, DRegister dn, DRegister dm, Condition cond = AL) OVERRIDE;
  void vmlas(SRegister sd, SRegister sn, SRegister sm, Condition cond = AL) OVERRIDE;
  void vmlad(DRegister dd, DRegister dn, DRegister dm, Condition cond = AL) OVERRIDE;
  void vmlss(SRegister sd, SRegister sn, SRegister sm, Condition cond = AL) OVERRIDE;
  void vmlsd(DRegister dd, DRegister dn, DRegister dm, Condition cond = AL) OVERRIDE;
  void vdivs(SRegister sd, SRegister sn, SRegister sm, Condition cond = AL) OVERRIDE;
  void vdivd(DRegister dd, DRegister dn, DRegister dm, Condition cond = AL) OVERRIDE;

  void vabss(SRegister sd, SRegister sm, Condition cond = AL) OVERRIDE;
  void vabsd(DRegister dd, DRegister dm, Condition cond = AL) OVERRIDE;
  void vnegs(SRegister sd, SRegister sm, Condition cond = AL) OVERRIDE;
  void vnegd(DRegister dd, DRegister dm, Condition cond = AL) OVERRIDE;
  void vsqrts(SRegister sd, SRegister sm, Condition cond = AL) OVERRIDE;
  void vsqrtd(DRegister dd, DRegister dm, Condition cond = AL) OVERRIDE;

  void vcvtsd(SRegister sd, DRegister dm, Condition cond = AL) OVERRIDE;
  void vcvtds(DRegister dd, SRegister sm, Condition cond = AL) OVERRIDE;
  void vcvtis(SRegister sd, SRegister sm, Condition cond = AL) OVERRIDE;
  void vcvtid(SRegister sd, DRegister dm, Condition cond = AL) OVERRIDE;
  void vcvtsi(SRegister sd, SRegister sm, Condition cond = AL) OVERRIDE;
  void vcvtdi(DRegister dd, SRegister sm, Condition cond = AL) OVERRIDE;
  void vcvtus(SRegister sd, SRegister sm, Condition cond = AL) OVERRIDE;
  void vcvtud(SRegister sd, DRegister dm, Condition cond = AL) OVERRIDE;
  void vcvtsu(SRegister sd, SRegister sm, Condition cond = AL) OVERRIDE;
  void vcvtdu(DRegister dd, SRegister sm, Condition cond = AL) OVERRIDE;

  void vcmps(SRegister sd, SRegister sm, Condition cond = AL) OVERRIDE;
  void vcmpd(DRegister dd, DRegister dm, Condition cond = AL) OVERRIDE;
  void vcmpsz(SRegister sd, Condition cond = AL) OVERRIDE;
  void vcmpdz(DRegister dd, Condition cond = AL) OVERRIDE;
  void vmstat(Condition cond = AL) OVERRIDE;  // VMRS APSR_nzcv, FPSCR

  // Branch instructions.
  void b(Label* label, Condition cond = AL);
  void bl(Label* label, Condition cond = AL);
  void blx(Label* label);
  void blx(Register rm, Condition cond = AL) OVERRIDE;
  void bx(Register rm, Condition cond = AL) OVERRIDE;

  void Lsl(Register rd, Register rm, uint32_t shift_imm, Condition cond = AL);
  void Lsr(Register rd, Register rm, uint32_t shift_imm, Condition cond = AL);
  void Asr(Register rd, Register rm, uint32_t shift_imm, Condition cond = AL);
  void Ror(Register rd, Register rm, uint32_t shift_imm, Condition cond = AL);
  void Rrx(Register rd, Register rm, Condition cond = AL);

  void Push(Register rd, Condition cond = AL) OVERRIDE;
  void Pop(Register rd, Condition cond = AL) OVERRIDE;

  void PushList(RegList regs, Condition cond = AL) OVERRIDE;
  void PopList(RegList regs, Condition cond = AL) OVERRIDE;

  void Mov(Register rd, Register rm, Condition cond = AL) OVERRIDE;

  // Macros.
  // Add signed constant value to rd. May clobber IP.
  void AddConstant(Register rd, int32_t value, Condition cond = AL) OVERRIDE;
  void AddConstant(Register rd, Register rn, int32_t value,
                   Condition cond = AL) OVERRIDE;
  void AddConstantSetFlags(Register rd, Register rn, int32_t value,
                           Condition cond = AL) OVERRIDE;
  void AddConstantWithCarry(Register rd, Register rn, int32_t value,
                            Condition cond = AL) {}

  // Load and Store. May clobber IP.
  void LoadImmediate(Register rd, int32_t value, Condition cond = AL) OVERRIDE;
  void LoadSImmediate(SRegister sd, float value, Condition cond = AL) {}
  void LoadDImmediate(DRegister dd, double value,
                      Register scratch, Condition cond = AL) {}
  void MarkExceptionHandler(Label* label) OVERRIDE;
  void LoadFromOffset(LoadOperandType type,
                      Register reg,
                      Register base,
                      int32_t offset,
                      Condition cond = AL) OVERRIDE;
  void StoreToOffset(StoreOperandType type,
                     Register reg,
                     Register base,
                     int32_t offset,
                     Condition cond = AL) OVERRIDE;
  void LoadSFromOffset(SRegister reg,
                       Register base,
                       int32_t offset,
                       Condition cond = AL) OVERRIDE;
  void StoreSToOffset(SRegister reg,
                      Register base,
                      int32_t offset,
                      Condition cond = AL) OVERRIDE;
  void LoadDFromOffset(DRegister reg,
                       Register base,
                       int32_t offset,
                       Condition cond = AL) OVERRIDE;
  void StoreDToOffset(DRegister reg,
                      Register base,
                      int32_t offset,
                      Condition cond = AL) OVERRIDE;


  // Encode a signed constant in tst instructions, only affecting the flags.
  void EncodeUint32InTstInstructions(uint32_t data) OVERRIDE;
  // ... and decode from a pc pointing to the start of encoding instructions.
  static uint32_t DecodeUint32FromTstInstructions(uword pc);
  static bool IsInstructionForExceptionHandling(uword pc);

  // Emit data (e.g. encoded instruction or immediate) to the
  // instruction stream.
  void Emit(int32_t value) OVERRIDE;
  void Emit16(int16_t value);
  void Bind(Label* label) OVERRIDE;

  void MemoryBarrier(ManagedRegister scratch) OVERRIDE;

  // Force the assembler to generate 32 bit instructions.
  void Force32Bit() {
    force_32bit_ = true;
  }

 private:
  // Emit a single 32 or 16 bit data processing instruction.
  void EmitDataProcessing(Condition cond,
                  Opcode opcode,
                  int set_cc,
                  Register rn,
                  Register rd,
                  const ShifterOperand& so);

  // Must the instruction be 32 bits or can it possibly be encoded
  // in 16 bits?
  bool Is32BitDataProcessing(Condition cond,
                  Opcode opcode,
                  int set_cc,
                  Register rn,
                  Register rd,
                  const ShifterOperand& so);

  // Emit a 32 bit data processing instruction.
  void Emit32BitDataProcessing(Condition cond,
                  Opcode opcode,
                  int set_cc,
                  Register rn,
                  Register rd,
                  const ShifterOperand& so);

  // Emit a 16 bit data processing instruction.
  void Emit16BitDataProcessing(Condition cond,
                  Opcode opcode,
                  int set_cc,
                  Register rn,
                  Register rd,
                  const ShifterOperand& so);

  void Emit16BitAddSub(Condition cond,
                       Opcode opcode,
                       int set_cc,
                       Register rn,
                       Register rd,
                       const ShifterOperand& so);

  void EmitCondBranch(Condition cond, int offset, bool link, bool x);
  void EmitCompareAndBranch(Register rn, int8_t offset, bool n);

  void EmitLoadStore(Condition cond,
                 bool load,
                 bool byte,
                 bool half,
                 bool is_signed,
                 Register rd,
                 const Address& ad);

  void EmitMemOpAddressMode3(Condition cond,
                             int32_t mode,
                             Register rd,
                             const Address& ad);

  void EmitMultiMemOp(Condition cond,
                      BlockAddressMode am,
                      bool load,
                      Register base,
                      RegList regs);

  void EmitShiftImmediate(Condition cond,
                          Shift opcode,
                          Register rd,
                          Register rm,
                          const ShifterOperand& so);

  void EmitShiftRegister(Condition cond,
                         Shift opcode,
                         Register rd,
                         Register rm,
                         const ShifterOperand& so);

  void EmitMulOp(Condition cond,
                 int32_t opcode,
                 Register rd,
                 Register rn,
                 Register rm,
                 Register rs);

  void EmitVFPsss(Condition cond,
                  int32_t opcode,
                  SRegister sd,
                  SRegister sn,
                  SRegister sm);

  void EmitVFPddd(Condition cond,
                  int32_t opcode,
                  DRegister dd,
                  DRegister dn,
                  DRegister dm);

  void EmitVFPsd(Condition cond,
                 int32_t opcode,
                 SRegister sd,
                 DRegister dm);

  void EmitVFPds(Condition cond,
                 int32_t opcode,
                 DRegister dd,
                 SRegister sm);

  void EmitBranch(Condition cond, Label* label, bool link, bool x);
  static int32_t EncodeBranchOffset(int32_t offset, int32_t inst);
  static int DecodeBranchOffset(int32_t inst);
  int32_t EncodeTstOffset(int offset, int32_t inst);
  int DecodeTstOffset(int32_t inst);

  bool IsLowRegister(Register r) {
    return r < R8;
  }

  bool IsHighRegister(Register r) {
     return r >= R8;
  }


  bool force_32bit_;      // Force the assembler to use 32 bit thumb2 instructions.

  // IfThen conditions.
  Condition it_conditions_[4];
  uint8_t it_cond_index_;
  Condition next_condition_;

  void SetItCondition(ItState s, Condition cond, uint8_t index);

  void CheckCondition(Condition cond) {
    CHECK_EQ(cond, next_condition_);

    // Move to the next condition if there is one.
    if (it_cond_index_ < 3) {
      ++it_cond_index_;
      next_condition_ = it_conditions_[it_cond_index_];
    } else {
      next_condition_ = AL;
    }
  }

  void CheckConditionLastIt(Condition cond) {
    if (it_cond_index_ < 3) {
      // Check that the next condition is AL.  This means that the
      // current condition is the last in the IT block.
      CHECK_EQ(it_conditions_[it_cond_index_ + 1], AL);
    }
    CheckCondition(cond);
  }
};

}  // namespace arm
}  // namespace art

#endif  // ART_COMPILER_UTILS_ARM_ASSEMBLER_THUMB2_H_
