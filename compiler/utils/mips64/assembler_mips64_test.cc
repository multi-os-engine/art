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

#include "assembler_mips64.h"

#include <inttypes.h>
#include <map>
#include <random>

#include "base/bit_utils.h"
#include "base/stl_util.h"
#include "utils/assembler_test.h"

#define __ GetAssembler()->

namespace art {

struct MIPS64CpuRegisterCompare {
  bool operator()(const mips64::GpuRegister& a, const mips64::GpuRegister& b) const {
    return a < b;
  }
};

class AssemblerMIPS64Test : public AssemblerTest<mips64::Mips64Assembler,
                                                 mips64::GpuRegister,
                                                 mips64::FpuRegister,
                                                 uint32_t> {
 public:
  typedef AssemblerTest<mips64::Mips64Assembler,
                        mips64::GpuRegister,
                        mips64::FpuRegister,
                        uint32_t> Base;

 protected:
  // Get the typically used name for this architecture, e.g., aarch64, x86-64, ...
  std::string GetArchitectureString() OVERRIDE {
    return "mips64";
  }

  std::string GetAssemblerCmdName() OVERRIDE {
    // We assemble and link for MIPS64R6. See GetAssemblerParameters() for details.
    return "gcc";
  }

  std::string GetAssemblerParameters() OVERRIDE {
    // We assemble and link for MIPS64R6. The reason is that object files produced for MIPS64R6
    // (and MIPS32R6) with the GNU assembler don't have correct final offsets in PC-relative
    // branches in the .text section and so they require a relocation pass (there's a relocation
    // section, .rela.text, that has the needed info to fix up the branches).
    return " -march=mips64r6 -Wa,--no-warn -Wl,-Ttext=0 -Wl,-e0 -nostdlib";
  }

  void Pad(std::vector<uint8_t>& data) OVERRIDE {
    // The GNU linker unconditionally pads the code segment with NOPs to a size that is a multiple
    // of 16 and there doesn't appear to be a way to suppress this padding. Our assembler doesn't
    // pad, so, in order for two assembler outputs to match, we need to match the padding as well.
    // NOP is encoded as four zero bytes on MIPS.
    size_t pad_size = RoundUp(data.size(), 16u) - data.size();
    data.insert(data.end(), pad_size, 0);
  }

  std::string GetDisassembleParameters() OVERRIDE {
    return " -D -bbinary -mmips:isa64r6";
  }

  void SetUpHelpers() OVERRIDE {
    if (registers_.size() == 0) {
      registers_.push_back(new mips64::GpuRegister(mips64::ZERO));
      registers_.push_back(new mips64::GpuRegister(mips64::AT));
      registers_.push_back(new mips64::GpuRegister(mips64::V0));
      registers_.push_back(new mips64::GpuRegister(mips64::V1));
      registers_.push_back(new mips64::GpuRegister(mips64::A0));
      registers_.push_back(new mips64::GpuRegister(mips64::A1));
      registers_.push_back(new mips64::GpuRegister(mips64::A2));
      registers_.push_back(new mips64::GpuRegister(mips64::A3));
      registers_.push_back(new mips64::GpuRegister(mips64::A4));
      registers_.push_back(new mips64::GpuRegister(mips64::A5));
      registers_.push_back(new mips64::GpuRegister(mips64::A6));
      registers_.push_back(new mips64::GpuRegister(mips64::A7));
      registers_.push_back(new mips64::GpuRegister(mips64::T0));
      registers_.push_back(new mips64::GpuRegister(mips64::T1));
      registers_.push_back(new mips64::GpuRegister(mips64::T2));
      registers_.push_back(new mips64::GpuRegister(mips64::T3));
      registers_.push_back(new mips64::GpuRegister(mips64::S0));
      registers_.push_back(new mips64::GpuRegister(mips64::S1));
      registers_.push_back(new mips64::GpuRegister(mips64::S2));
      registers_.push_back(new mips64::GpuRegister(mips64::S3));
      registers_.push_back(new mips64::GpuRegister(mips64::S4));
      registers_.push_back(new mips64::GpuRegister(mips64::S5));
      registers_.push_back(new mips64::GpuRegister(mips64::S6));
      registers_.push_back(new mips64::GpuRegister(mips64::S7));
      registers_.push_back(new mips64::GpuRegister(mips64::T8));
      registers_.push_back(new mips64::GpuRegister(mips64::T9));
      registers_.push_back(new mips64::GpuRegister(mips64::K0));
      registers_.push_back(new mips64::GpuRegister(mips64::K1));
      registers_.push_back(new mips64::GpuRegister(mips64::GP));
      registers_.push_back(new mips64::GpuRegister(mips64::SP));
      registers_.push_back(new mips64::GpuRegister(mips64::S8));
      registers_.push_back(new mips64::GpuRegister(mips64::RA));

      secondary_register_names_.emplace(mips64::GpuRegister(mips64::ZERO), "zero");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::AT), "at");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::V0), "v0");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::V1), "v1");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::A0), "a0");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::A1), "a1");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::A2), "a2");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::A3), "a3");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::A4), "a4");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::A5), "a5");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::A6), "a6");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::A7), "a7");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::T0), "t0");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::T1), "t1");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::T2), "t2");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::T3), "t3");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::S0), "s0");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::S1), "s1");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::S2), "s2");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::S3), "s3");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::S4), "s4");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::S5), "s5");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::S6), "s6");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::S7), "s7");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::T8), "t8");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::T9), "t9");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::K0), "k0");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::K1), "k1");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::GP), "gp");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::SP), "sp");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::S8), "s8");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::RA), "ra");

      fp_registers_.push_back(new mips64::FpuRegister(mips64::F0));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F1));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F2));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F3));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F4));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F5));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F6));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F7));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F8));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F9));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F10));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F11));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F12));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F13));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F14));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F15));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F16));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F17));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F18));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F19));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F20));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F21));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F22));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F23));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F24));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F25));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F26));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F27));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F28));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F29));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F30));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F31));
    }
  }

  void TearDown() OVERRIDE {
    AssemblerTest::TearDown();
    STLDeleteElements(&registers_);
    STLDeleteElements(&fp_registers_);
  }

  std::vector<mips64::GpuRegister*> GetRegisters() OVERRIDE {
    return registers_;
  }

  std::vector<mips64::FpuRegister*> GetFPRegisters() OVERRIDE {
    return fp_registers_;
  }

  uint32_t CreateImmediate(int64_t imm_value) OVERRIDE {
    return imm_value;
  }

  std::string GetSecondaryRegisterName(const mips64::GpuRegister& reg) OVERRIDE {
    CHECK(secondary_register_names_.find(reg) != secondary_register_names_.end());
    return secondary_register_names_[reg];
  }

  std::string RepeatInsn(size_t count, const std::string& insn) {
    std::string result;
    for (; count != 0u; --count) {
      result += insn;
    }
    return result;
  }

  void BranchCondOneRegHelper(void (mips64::Mips64Assembler::*f)(mips64::GpuRegister,
                                                                 mips64::Mips64Label*),
                              std::string instr_name) {
    mips64::Mips64Label label;
    (Base::GetAssembler()->*f)(mips64::A0, &label);
    constexpr size_t kAdduCount1 = 63;
    for (size_t i = 0; i != kAdduCount1; ++i) {
      __ Addu(mips64::ZERO, mips64::ZERO, mips64::ZERO);
    }
    __ Bind(&label);
    constexpr size_t kAdduCount2 = 64;
    for (size_t i = 0; i != kAdduCount2; ++i) {
      __ Addu(mips64::ZERO, mips64::ZERO, mips64::ZERO);
    }
    (Base::GetAssembler()->*f)(mips64::A1, &label);

    std::string expected =
        ".set noreorder\n" +
        instr_name + " $a0, 1f\n"
        "nop\n" +
        RepeatInsn(kAdduCount1, "addu $zero, $zero, $zero\n") +
        "1:\n" +
        RepeatInsn(kAdduCount2, "addu $zero, $zero, $zero\n") +
        instr_name + " $a1, 1b\n"
        "nop\n";
    DriverStr(expected, instr_name);
  }

  void BranchCondTwoRegsHelper(void (mips64::Mips64Assembler::*f)(mips64::GpuRegister,
                                                                  mips64::GpuRegister,
                                                                  mips64::Mips64Label*),
                               std::string instr_name) {
    mips64::Mips64Label label;
    (Base::GetAssembler()->*f)(mips64::A0, mips64::A1, &label);
    constexpr size_t kAdduCount1 = 63;
    for (size_t i = 0; i != kAdduCount1; ++i) {
      __ Addu(mips64::ZERO, mips64::ZERO, mips64::ZERO);
    }
    __ Bind(&label);
    constexpr size_t kAdduCount2 = 64;
    for (size_t i = 0; i != kAdduCount2; ++i) {
      __ Addu(mips64::ZERO, mips64::ZERO, mips64::ZERO);
    }
    (Base::GetAssembler()->*f)(mips64::A2, mips64::A3, &label);

    std::string expected =
        ".set noreorder\n" +
        instr_name + " $a0, $a1, 1f\n"
        "nop\n" +
        RepeatInsn(kAdduCount1, "addu $zero, $zero, $zero\n") +
        "1:\n" +
        RepeatInsn(kAdduCount2, "addu $zero, $zero, $zero\n") +
        instr_name + " $a2, $a3, 1b\n"
        "nop\n";
    DriverStr(expected, instr_name);
  }

 private:
  std::vector<mips64::GpuRegister*> registers_;
  std::map<mips64::GpuRegister, std::string, MIPS64CpuRegisterCompare> secondary_register_names_;

  std::vector<mips64::FpuRegister*> fp_registers_;
};


TEST_F(AssemblerMIPS64Test, Toolchain) {
  EXPECT_TRUE(CheckTools());
}

///////////////////
// FP Operations //
///////////////////

TEST_F(AssemblerMIPS64Test, SqrtS) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::SqrtS, "sqrt.s ${reg1}, ${reg2}"), "sqrt.s");
}

TEST_F(AssemblerMIPS64Test, SqrtD) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::SqrtD, "sqrt.d ${reg1}, ${reg2}"), "sqrt.d");
}

TEST_F(AssemblerMIPS64Test, AbsS) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::AbsS, "abs.s ${reg1}, ${reg2}"), "abs.s");
}

TEST_F(AssemblerMIPS64Test, AbsD) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::AbsD, "abs.d ${reg1}, ${reg2}"), "abs.d");
}

TEST_F(AssemblerMIPS64Test, MovS) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::MovS, "mov.s ${reg1}, ${reg2}"), "mov.s");
}

TEST_F(AssemblerMIPS64Test, MovD) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::MovD, "mov.d ${reg1}, ${reg2}"), "mov.d");
}

TEST_F(AssemblerMIPS64Test, NegS) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::NegS, "neg.s ${reg1}, ${reg2}"), "neg.s");
}

TEST_F(AssemblerMIPS64Test, NegD) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::NegD, "neg.d ${reg1}, ${reg2}"), "neg.d");
}

TEST_F(AssemblerMIPS64Test, RoundLS) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::RoundLS, "round.l.s ${reg1}, ${reg2}"), "round.l.s");
}

TEST_F(AssemblerMIPS64Test, RoundLD) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::RoundLD, "round.l.d ${reg1}, ${reg2}"), "round.l.d");
}

TEST_F(AssemblerMIPS64Test, RoundWS) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::RoundWS, "round.w.s ${reg1}, ${reg2}"), "round.w.s");
}

TEST_F(AssemblerMIPS64Test, RoundWD) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::RoundWD, "round.w.d ${reg1}, ${reg2}"), "round.w.d");
}

TEST_F(AssemblerMIPS64Test, CeilLS) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::CeilLS, "ceil.l.s ${reg1}, ${reg2}"), "ceil.l.s");
}

TEST_F(AssemblerMIPS64Test, CeilLD) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::CeilLD, "ceil.l.d ${reg1}, ${reg2}"), "ceil.l.d");
}

TEST_F(AssemblerMIPS64Test, CeilWS) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::CeilWS, "ceil.w.s ${reg1}, ${reg2}"), "ceil.w.s");
}

TEST_F(AssemblerMIPS64Test, CeilWD) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::CeilWD, "ceil.w.d ${reg1}, ${reg2}"), "ceil.w.d");
}

TEST_F(AssemblerMIPS64Test, FloorLS) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::FloorLS, "floor.l.s ${reg1}, ${reg2}"), "floor.l.s");
}

TEST_F(AssemblerMIPS64Test, FloorLD) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::FloorLD, "floor.l.d ${reg1}, ${reg2}"), "floor.l.d");
}

TEST_F(AssemblerMIPS64Test, FloorWS) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::FloorWS, "floor.w.s ${reg1}, ${reg2}"), "floor.w.s");
}

TEST_F(AssemblerMIPS64Test, FloorWD) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::FloorWD, "floor.w.d ${reg1}, ${reg2}"), "floor.w.d");
}

TEST_F(AssemblerMIPS64Test, SelS) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::SelS, "sel.s ${reg1}, ${reg2}, ${reg3}"), "sel.s");
}

TEST_F(AssemblerMIPS64Test, SelD) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::SelD, "sel.d ${reg1}, ${reg2}, ${reg3}"), "sel.d");
}

TEST_F(AssemblerMIPS64Test, RintS) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::RintS, "rint.s ${reg1}, ${reg2}"), "rint.s");
}

TEST_F(AssemblerMIPS64Test, RintD) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::RintD, "rint.d ${reg1}, ${reg2}"), "rint.d");
}

TEST_F(AssemblerMIPS64Test, ClassS) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::ClassS, "class.s ${reg1}, ${reg2}"), "class.s");
}

TEST_F(AssemblerMIPS64Test, ClassD) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::ClassD, "class.d ${reg1}, ${reg2}"), "class.d");
}

TEST_F(AssemblerMIPS64Test, MinS) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::MinS, "min.s ${reg1}, ${reg2}, ${reg3}"), "min.s");
}

TEST_F(AssemblerMIPS64Test, MinD) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::MinD, "min.d ${reg1}, ${reg2}, ${reg3}"), "min.d");
}

TEST_F(AssemblerMIPS64Test, MaxS) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::MaxS, "max.s ${reg1}, ${reg2}, ${reg3}"), "max.s");
}

TEST_F(AssemblerMIPS64Test, MaxD) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::MaxD, "max.d ${reg1}, ${reg2}, ${reg3}"), "max.d");
}

TEST_F(AssemblerMIPS64Test, CmpUnS) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpUnS, "cmp.un.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.un.s");
}

TEST_F(AssemblerMIPS64Test, CmpEqS) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpEqS, "cmp.eq.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.eq.s");
}

TEST_F(AssemblerMIPS64Test, CmpUeqS) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpUeqS, "cmp.ueq.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.ueq.s");
}

TEST_F(AssemblerMIPS64Test, CmpLtS) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpLtS, "cmp.lt.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.lt.s");
}

TEST_F(AssemblerMIPS64Test, CmpUltS) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpUltS, "cmp.ult.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.ult.s");
}

TEST_F(AssemblerMIPS64Test, CmpLeS) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpLeS, "cmp.le.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.le.s");
}

TEST_F(AssemblerMIPS64Test, CmpUleS) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpUleS, "cmp.ule.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.ule.s");
}

TEST_F(AssemblerMIPS64Test, CmpOrS) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpOrS, "cmp.or.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.or.s");
}

TEST_F(AssemblerMIPS64Test, CmpUneS) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpUneS, "cmp.une.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.une.s");
}

TEST_F(AssemblerMIPS64Test, CmpNeS) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpNeS, "cmp.ne.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.ne.s");
}

TEST_F(AssemblerMIPS64Test, CmpUnD) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpUnD, "cmp.un.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.un.d");
}

TEST_F(AssemblerMIPS64Test, CmpEqD) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpEqD, "cmp.eq.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.eq.d");
}

TEST_F(AssemblerMIPS64Test, CmpUeqD) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpUeqD, "cmp.ueq.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.ueq.d");
}

TEST_F(AssemblerMIPS64Test, CmpLtD) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpLtD, "cmp.lt.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.lt.d");
}

TEST_F(AssemblerMIPS64Test, CmpUltD) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpUltD, "cmp.ult.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.ult.d");
}

TEST_F(AssemblerMIPS64Test, CmpLeD) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpLeD, "cmp.le.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.le.d");
}

TEST_F(AssemblerMIPS64Test, CmpUleD) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpUleD, "cmp.ule.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.ule.d");
}

TEST_F(AssemblerMIPS64Test, CmpOrD) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpOrD, "cmp.or.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.or.d");
}

TEST_F(AssemblerMIPS64Test, CmpUneD) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpUneD, "cmp.une.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.une.d");
}

TEST_F(AssemblerMIPS64Test, CmpNeD) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpNeD, "cmp.ne.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.ne.d");
}

TEST_F(AssemblerMIPS64Test, CvtDL) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::Cvtdl, "cvt.d.l ${reg1}, ${reg2}"), "cvt.d.l");
}

TEST_F(AssemblerMIPS64Test, CvtDS) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::Cvtds, "cvt.d.s ${reg1}, ${reg2}"), "cvt.d.s");
}

TEST_F(AssemblerMIPS64Test, CvtDW) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::Cvtdw, "cvt.d.w ${reg1}, ${reg2}"), "cvt.d.w");
}

TEST_F(AssemblerMIPS64Test, CvtSL) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::Cvtsl, "cvt.s.l ${reg1}, ${reg2}"), "cvt.s.l");
}

TEST_F(AssemblerMIPS64Test, CvtSD) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::Cvtsd, "cvt.s.d ${reg1}, ${reg2}"), "cvt.s.d");
}

TEST_F(AssemblerMIPS64Test, CvtSW) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::Cvtsw, "cvt.s.w ${reg1}, ${reg2}"), "cvt.s.w");
}

TEST_F(AssemblerMIPS64Test, TruncWS) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::TruncWS, "trunc.w.s ${reg1}, ${reg2}"), "trunc.w.s");
}

TEST_F(AssemblerMIPS64Test, TruncWD) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::TruncWD, "trunc.w.d ${reg1}, ${reg2}"), "trunc.w.d");
}

TEST_F(AssemblerMIPS64Test, TruncLS) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::TruncLS, "trunc.l.s ${reg1}, ${reg2}"), "trunc.l.s");
}

TEST_F(AssemblerMIPS64Test, TruncLD) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::TruncLD, "trunc.l.d ${reg1}, ${reg2}"), "trunc.l.d");
}

TEST_F(AssemblerMIPS64Test, Mfc1) {
  DriverStr(RepeatRF(&mips64::Mips64Assembler::Mfc1, "mfc1 ${reg1}, ${reg2}"), "Mfc1");
}

TEST_F(AssemblerMIPS64Test, Mfhc1) {
  DriverStr(RepeatRF(&mips64::Mips64Assembler::Mfhc1, "mfhc1 ${reg1}, ${reg2}"), "Mfhc1");
}

TEST_F(AssemblerMIPS64Test, Mtc1) {
  DriverStr(RepeatRF(&mips64::Mips64Assembler::Mtc1, "mtc1 ${reg1}, ${reg2}"), "Mtc1");
}

TEST_F(AssemblerMIPS64Test, Mthc1) {
  DriverStr(RepeatRF(&mips64::Mips64Assembler::Mthc1, "mthc1 ${reg1}, ${reg2}"), "Mthc1");
}

TEST_F(AssemblerMIPS64Test, Dmfc1) {
  DriverStr(RepeatRF(&mips64::Mips64Assembler::Dmfc1, "dmfc1 ${reg1}, ${reg2}"), "Dmfc1");
}

TEST_F(AssemblerMIPS64Test, Dmtc1) {
  DriverStr(RepeatRF(&mips64::Mips64Assembler::Dmtc1, "dmtc1 ${reg1}, ${reg2}"), "Dmtc1");
}

////////////////
// CALL / JMP //
////////////////

TEST_F(AssemblerMIPS64Test, Jalr) {
  DriverStr(".set noreorder\n" +
            RepeatRRNoDupes(&mips64::Mips64Assembler::Jalr, "jalr ${reg1}, ${reg2}"), "jalr");
}

TEST_F(AssemblerMIPS64Test, Jialc) {
  mips64::Mips64Label label1, label2;
  __ Jialc(&label1, mips64::T9);
  constexpr size_t kAdduCount1 = 63;
  for (size_t i = 0; i != kAdduCount1; ++i) {
    __ Addu(mips64::ZERO, mips64::ZERO, mips64::ZERO);
  }
  __ Bind(&label1);
  __ Jialc(&label2, mips64::T9);
  constexpr size_t kAdduCount2 = 64;
  for (size_t i = 0; i != kAdduCount2; ++i) {
    __ Addu(mips64::ZERO, mips64::ZERO, mips64::ZERO);
  }
  __ Bind(&label2);
  __ Jialc(&label1, mips64::T9);

  std::string expected =
      ".set noreorder\n"
      "lapc $t9, 1f\n"
      "jialc $t9, 0\n" +
      RepeatInsn(kAdduCount1, "addu $zero, $zero, $zero\n") +
      "1:\n"
      "lapc $t9, 2f\n"
      "jialc $t9, 0\n" +
      RepeatInsn(kAdduCount2, "addu $zero, $zero, $zero\n") +
      "2:\n"
      "lapc $t9, 1b\n"
      "jialc $t9, 0\n";
  DriverStr(expected, "Jialc");
}

TEST_F(AssemblerMIPS64Test, LongJialc) {
  mips64::Mips64Label label1, label2;
  __ Jialc(&label1, mips64::T9);
  constexpr uint32_t kAdduCount1 = (1u << 18) + 1;
  for (uint32_t i = 0; i != kAdduCount1; ++i) {
    __ Addu(mips64::ZERO, mips64::ZERO, mips64::ZERO);
  }
  __ Bind(&label1);
  __ Jialc(&label2, mips64::T9);
  constexpr uint32_t kAdduCount2 = (1u << 18) + 1;
  for (uint32_t i = 0; i != kAdduCount2; ++i) {
    __ Addu(mips64::ZERO, mips64::ZERO, mips64::ZERO);
  }
  __ Bind(&label2);
  __ Jialc(&label1, mips64::T9);

  uint32_t offset_forward1 = 3 + kAdduCount1;  // 3: account for auipc, daddiu and jic.
  offset_forward1 <<= 2;
  offset_forward1 += (offset_forward1 & 0x8000) << 1;  // Account for sign extension in daddiu.

  uint32_t offset_forward2 = 3 + kAdduCount2;  // 3: account for auipc, daddiu and jic.
  offset_forward2 <<= 2;
  offset_forward2 += (offset_forward2 & 0x8000) << 1;  // Account for sign extension in daddiu.

  uint32_t offset_back = -(3 + kAdduCount2);  // 3: account for auipc, daddiu and jic.
  offset_back <<= 2;
  offset_back += (offset_back & 0x8000) << 1;  // Account for sign extension in daddiu.

  std::ostringstream oss;
  oss <<
      ".set noreorder\n"
      "auipc $t9, 0x" << std::hex << High16Bits(offset_forward1) << "\n"
      "daddiu $t9, 0x" << std::hex << Low16Bits(offset_forward1) << "\n"
      "jialc $t9, 0\n" <<
      RepeatInsn(kAdduCount1, "addu $zero, $zero, $zero\n") <<
      "1:\n"
      "auipc $t9, 0x" << std::hex << High16Bits(offset_forward2) << "\n"
      "daddiu $t9, 0x" << std::hex << Low16Bits(offset_forward2) << "\n"
      "jialc $t9, 0\n" <<
      RepeatInsn(kAdduCount2, "addu $zero, $zero, $zero\n") <<
      "2:\n"
      "auipc $t9, 0x" << std::hex << High16Bits(offset_back) << "\n"
      "daddiu $t9, 0x" << std::hex << Low16Bits(offset_back) << "\n"
      "jialc $t9, 0\n";
  std::string expected = oss.str();
  DriverStr(expected, "LongJialc");
}

TEST_F(AssemblerMIPS64Test, Bc) {
  mips64::Mips64Label label1, label2;
  __ Bc(&label1);
  constexpr size_t kAdduCount1 = 63;
  for (size_t i = 0; i != kAdduCount1; ++i) {
    __ Addu(mips64::ZERO, mips64::ZERO, mips64::ZERO);
  }
  __ Bind(&label1);
  __ Bc(&label2);
  constexpr size_t kAdduCount2 = 64;
  for (size_t i = 0; i != kAdduCount2; ++i) {
    __ Addu(mips64::ZERO, mips64::ZERO, mips64::ZERO);
  }
  __ Bind(&label2);
  __ Bc(&label1);

  std::string expected =
      ".set noreorder\n"
      "bc 1f\n" +
      RepeatInsn(kAdduCount1, "addu $zero, $zero, $zero\n") +
      "1:\n"
      "bc 2f\n" +
      RepeatInsn(kAdduCount2, "addu $zero, $zero, $zero\n") +
      "2:\n"
      "bc 1b\n";
  DriverStr(expected, "Bc");
}

TEST_F(AssemblerMIPS64Test, Beqzc) {
  BranchCondOneRegHelper(&mips64::Mips64Assembler::Beqzc, "Beqzc");
}

TEST_F(AssemblerMIPS64Test, Bnezc) {
  BranchCondOneRegHelper(&mips64::Mips64Assembler::Bnezc, "Bnezc");
}

TEST_F(AssemblerMIPS64Test, Bltzc) {
  BranchCondOneRegHelper(&mips64::Mips64Assembler::Bltzc, "Bltzc");
}

TEST_F(AssemblerMIPS64Test, Bgezc) {
  BranchCondOneRegHelper(&mips64::Mips64Assembler::Bgezc, "Bgezc");
}

TEST_F(AssemblerMIPS64Test, Blezc) {
  BranchCondOneRegHelper(&mips64::Mips64Assembler::Blezc, "Blezc");
}

TEST_F(AssemblerMIPS64Test, Bgtzc) {
  BranchCondOneRegHelper(&mips64::Mips64Assembler::Bgtzc, "Bgtzc");
}

TEST_F(AssemblerMIPS64Test, Beqc) {
  BranchCondTwoRegsHelper(&mips64::Mips64Assembler::Beqc, "Beqc");
}

TEST_F(AssemblerMIPS64Test, Bnec) {
  BranchCondTwoRegsHelper(&mips64::Mips64Assembler::Bnec, "Bnec");
}

TEST_F(AssemblerMIPS64Test, Bltc) {
  BranchCondTwoRegsHelper(&mips64::Mips64Assembler::Bltc, "Bltc");
}

TEST_F(AssemblerMIPS64Test, Bgec) {
  BranchCondTwoRegsHelper(&mips64::Mips64Assembler::Bgec, "Bgec");
}

TEST_F(AssemblerMIPS64Test, Bltuc) {
  BranchCondTwoRegsHelper(&mips64::Mips64Assembler::Bltuc, "Bltuc");
}

TEST_F(AssemblerMIPS64Test, Bgeuc) {
  BranchCondTwoRegsHelper(&mips64::Mips64Assembler::Bgeuc, "Bgeuc");
}

TEST_F(AssemblerMIPS64Test, Bc1eqz) {
    mips64::Mips64Label label;
    __ Bc1eqz(mips64::F0, &label);
    constexpr size_t kAdduCount1 = 63;
    for (size_t i = 0; i != kAdduCount1; ++i) {
      __ Addu(mips64::ZERO, mips64::ZERO, mips64::ZERO);
    }
    __ Bind(&label);
    constexpr size_t kAdduCount2 = 64;
    for (size_t i = 0; i != kAdduCount2; ++i) {
      __ Addu(mips64::ZERO, mips64::ZERO, mips64::ZERO);
    }
    __ Bc1eqz(mips64::F31, &label);

    std::string expected =
        ".set noreorder\n"
        "bc1eqz $f0, 1f\n"
        "nop\n" +
        RepeatInsn(kAdduCount1, "addu $zero, $zero, $zero\n") +
        "1:\n" +
        RepeatInsn(kAdduCount2, "addu $zero, $zero, $zero\n") +
        "bc1eqz $f31, 1b\n"
        "nop\n";
    DriverStr(expected, "Bc1eqz");
}

TEST_F(AssemblerMIPS64Test, Bc1nez) {
    mips64::Mips64Label label;
    __ Bc1nez(mips64::F0, &label);
    constexpr size_t kAdduCount1 = 63;
    for (size_t i = 0; i != kAdduCount1; ++i) {
      __ Addu(mips64::ZERO, mips64::ZERO, mips64::ZERO);
    }
    __ Bind(&label);
    constexpr size_t kAdduCount2 = 64;
    for (size_t i = 0; i != kAdduCount2; ++i) {
      __ Addu(mips64::ZERO, mips64::ZERO, mips64::ZERO);
    }
    __ Bc1nez(mips64::F31, &label);

    std::string expected =
        ".set noreorder\n"
        "bc1nez $f0, 1f\n"
        "nop\n" +
        RepeatInsn(kAdduCount1, "addu $zero, $zero, $zero\n") +
        "1:\n" +
        RepeatInsn(kAdduCount2, "addu $zero, $zero, $zero\n") +
        "bc1nez $f31, 1b\n"
        "nop\n";
    DriverStr(expected, "Bc1nez");
}

TEST_F(AssemblerMIPS64Test, LongBeqc) {
  mips64::Mips64Label label;
  __ Beqc(mips64::A0, mips64::A1, &label);
  constexpr uint32_t kAdduCount1 = (1u << 15) + 1;
  for (uint32_t i = 0; i != kAdduCount1; ++i) {
    __ Addu(mips64::ZERO, mips64::ZERO, mips64::ZERO);
  }
  __ Bind(&label);
  constexpr uint32_t kAdduCount2 = (1u << 15) + 1;
  for (uint32_t i = 0; i != kAdduCount2; ++i) {
    __ Addu(mips64::ZERO, mips64::ZERO, mips64::ZERO);
  }
  __ Beqc(mips64::A2, mips64::A3, &label);

  uint32_t offset_forward = 2 + kAdduCount1;  // 2: account for auipc and jic.
  offset_forward <<= 2;
  offset_forward += (offset_forward & 0x8000) << 1;  // Account for sign extension in jic.

  uint32_t offset_back = -(kAdduCount2 + 1);  // 1: account for bnec.
  offset_back <<= 2;
  offset_back += (offset_back & 0x8000) << 1;  // Account for sign extension in jic.

  std::ostringstream oss;
  oss <<
      ".set noreorder\n"
      "bnec $a0, $a1, 1f\n"
      "auipc $at, 0x" << std::hex << High16Bits(offset_forward) << "\n"
      "jic $at, 0x" << std::hex << Low16Bits(offset_forward) << "\n"
      "1:\n" <<
      RepeatInsn(kAdduCount1, "addu $zero, $zero, $zero\n") <<
      "2:\n" <<
      RepeatInsn(kAdduCount2, "addu $zero, $zero, $zero\n") <<
      "bnec $a2, $a3, 3f\n"
      "auipc $at, 0x" << std::hex << High16Bits(offset_back) << "\n"
      "jic $at, 0x" << std::hex << Low16Bits(offset_back) << "\n"
      "3:\n";
  std::string expected = oss.str();
  DriverStr(expected, "LongBeqc");
}

//////////
// MISC //
//////////

TEST_F(AssemblerMIPS64Test, Bitswap) {
  DriverStr(RepeatRR(&mips64::Mips64Assembler::Bitswap, "bitswap ${reg1}, ${reg2}"), "bitswap");
}

TEST_F(AssemblerMIPS64Test, Dbitswap) {
  DriverStr(RepeatRR(&mips64::Mips64Assembler::Dbitswap, "dbitswap ${reg1}, ${reg2}"), "dbitswap");
}

TEST_F(AssemblerMIPS64Test, Seb) {
  DriverStr(RepeatRR(&mips64::Mips64Assembler::Seb, "seb ${reg1}, ${reg2}"), "seb");
}

TEST_F(AssemblerMIPS64Test, Seh) {
  DriverStr(RepeatRR(&mips64::Mips64Assembler::Seh, "seh ${reg1}, ${reg2}"), "seh");
}

TEST_F(AssemblerMIPS64Test, Dsbh) {
  DriverStr(RepeatRR(&mips64::Mips64Assembler::Dsbh, "dsbh ${reg1}, ${reg2}"), "dsbh");
}

TEST_F(AssemblerMIPS64Test, Dshd) {
  DriverStr(RepeatRR(&mips64::Mips64Assembler::Dshd, "dshd ${reg1}, ${reg2}"), "dshd");
}

TEST_F(AssemblerMIPS64Test, Dext) {
  std::vector<mips64::GpuRegister*> reg1_registers = GetRegisters();
  std::vector<mips64::GpuRegister*> reg2_registers = GetRegisters();
  WarnOnCombinations(reg1_registers.size() * reg2_registers.size() * 33 * 16);
  std::ostringstream expected;
  for (mips64::GpuRegister* reg1 : reg1_registers) {
    for (mips64::GpuRegister* reg2 : reg2_registers) {
      for (int32_t pos = 0; pos < 32; pos++) {
        for (int32_t size = 1; size <= 32; size++) {
          __ Dext(*reg1, *reg2, pos, size);
          expected << "dext $" << *reg1 << ", $" << *reg2 << ", " << pos << ", " << size << "\n";
        }
      }
    }
  }

  DriverStr(expected.str(), "Dext");
}

TEST_F(AssemblerMIPS64Test, Dinsu) {
  std::vector<mips64::GpuRegister*> reg1_registers = GetRegisters();
  std::vector<mips64::GpuRegister*> reg2_registers = GetRegisters();
  WarnOnCombinations(reg1_registers.size() * reg2_registers.size() * 33 * 16);
  std::ostringstream expected;
  for (mips64::GpuRegister* reg1 : reg1_registers) {
    for (mips64::GpuRegister* reg2 : reg2_registers) {
      for (int32_t pos = 32; pos < 64; pos++) {
        for (int32_t size = 1; pos + size <= 64; size++) {
          __ Dinsu(*reg1, *reg2, pos, size);
          expected << "dinsu $" << *reg1 << ", $" << *reg2 << ", " << pos << ", " << size << "\n";
        }
      }
    }
  }

  DriverStr(expected.str(), "Dinsu");
}

TEST_F(AssemblerMIPS64Test, Wsbh) {
  DriverStr(RepeatRR(&mips64::Mips64Assembler::Wsbh, "wsbh ${reg1}, ${reg2}"), "wsbh");
}

TEST_F(AssemblerMIPS64Test, Sll) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Sll, 5, "sll ${reg1}, ${reg2}, {imm}"), "sll");
}

TEST_F(AssemblerMIPS64Test, Srl) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Srl, 5, "srl ${reg1}, ${reg2}, {imm}"), "srl");
}

TEST_F(AssemblerMIPS64Test, Rotr) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Rotr, 5, "rotr ${reg1}, ${reg2}, {imm}"), "rotr");
}

TEST_F(AssemblerMIPS64Test, Sra) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Sra, 5, "sra ${reg1}, ${reg2}, {imm}"), "sra");
}

TEST_F(AssemblerMIPS64Test, Sllv) {
  DriverStr(RepeatRRR(&mips64::Mips64Assembler::Sllv, "sllv ${reg1}, ${reg2}, ${reg3}"), "sllv");
}

TEST_F(AssemblerMIPS64Test, Srlv) {
  DriverStr(RepeatRRR(&mips64::Mips64Assembler::Srlv, "srlv ${reg1}, ${reg2}, ${reg3}"), "srlv");
}

TEST_F(AssemblerMIPS64Test, Rotrv) {
  DriverStr(RepeatRRR(&mips64::Mips64Assembler::Rotrv, "rotrv ${reg1}, ${reg2}, ${reg3}"), "rotrv");
}

TEST_F(AssemblerMIPS64Test, Srav) {
  DriverStr(RepeatRRR(&mips64::Mips64Assembler::Srav, "srav ${reg1}, ${reg2}, ${reg3}"), "srav");
}

TEST_F(AssemblerMIPS64Test, Dsll) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Dsll, 5, "dsll ${reg1}, ${reg2}, {imm}"), "dsll");
}

TEST_F(AssemblerMIPS64Test, Dsrl) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Dsrl, 5, "dsrl ${reg1}, ${reg2}, {imm}"), "dsrl");
}

TEST_F(AssemblerMIPS64Test, Drotr) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Drotr, 5, "drotr ${reg1}, ${reg2}, {imm}"),
            "drotr");
}

TEST_F(AssemblerMIPS64Test, Dsra) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Dsra, 5, "dsra ${reg1}, ${reg2}, {imm}"), "dsra");
}

TEST_F(AssemblerMIPS64Test, Dsll32) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Dsll32, 5, "dsll32 ${reg1}, ${reg2}, {imm}"),
            "dsll32");
}

TEST_F(AssemblerMIPS64Test, Dsrl32) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Dsrl32, 5, "dsrl32 ${reg1}, ${reg2}, {imm}"),
            "dsrl32");
}

TEST_F(AssemblerMIPS64Test, Drotr32) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Drotr32, 5, "drotr32 ${reg1}, ${reg2}, {imm}"),
            "drotr32");
}

TEST_F(AssemblerMIPS64Test, Dsra32) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Dsra32, 5, "dsra32 ${reg1}, ${reg2}, {imm}"),
            "dsra32");
}

TEST_F(AssemblerMIPS64Test, Sc) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Sc, -9, "sc ${reg1}, {imm}(${reg2})"), "sc");
}

TEST_F(AssemblerMIPS64Test, Scd) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Scd, -9, "scd ${reg1}, {imm}(${reg2})"), "scd");
}

TEST_F(AssemblerMIPS64Test, Ll) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Ll, -9, "ll ${reg1}, {imm}(${reg2})"), "ll");
}

TEST_F(AssemblerMIPS64Test, Lld) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Lld, -9, "lld ${reg1}, {imm}(${reg2})"), "lld");
}

TEST_F(AssemblerMIPS64Test, Seleqz) {
  DriverStr(RepeatRRR(&mips64::Mips64Assembler::Seleqz, "seleqz ${reg1}, ${reg2}, ${reg3}"),
            "seleqz");
}

TEST_F(AssemblerMIPS64Test, Selnez) {
  DriverStr(RepeatRRR(&mips64::Mips64Assembler::Selnez, "selnez ${reg1}, ${reg2}, ${reg3}"),
            "selnez");
}

TEST_F(AssemblerMIPS64Test, Clz) {
  DriverStr(RepeatRR(&mips64::Mips64Assembler::Clz, "clz ${reg1}, ${reg2}"), "clz");
}

TEST_F(AssemblerMIPS64Test, Clo) {
  DriverStr(RepeatRR(&mips64::Mips64Assembler::Clo, "clo ${reg1}, ${reg2}"), "clo");
}

TEST_F(AssemblerMIPS64Test, Dclz) {
  DriverStr(RepeatRR(&mips64::Mips64Assembler::Dclz, "dclz ${reg1}, ${reg2}"), "dclz");
}

TEST_F(AssemblerMIPS64Test, Dclo) {
  DriverStr(RepeatRR(&mips64::Mips64Assembler::Dclo, "dclo ${reg1}, ${reg2}"), "dclo");
}

TEST_F(AssemblerMIPS64Test, LoadFromOffset) {
  __ LoadFromOffset(mips64::kLoadSignedByte, mips64::A0, mips64::A0, 0);
  __ LoadFromOffset(mips64::kLoadSignedByte, mips64::A0, mips64::A1, 0);
  __ LoadFromOffset(mips64::kLoadSignedByte, mips64::A0, mips64::A1, 1);
  __ LoadFromOffset(mips64::kLoadSignedByte, mips64::A0, mips64::A1, 256);
  __ LoadFromOffset(mips64::kLoadSignedByte, mips64::A0, mips64::A1, 1000);
  __ LoadFromOffset(mips64::kLoadSignedByte, mips64::A0, mips64::A1, 0x7FFF);
  __ LoadFromOffset(mips64::kLoadSignedByte, mips64::A0, mips64::A1, 0x8000);
  __ LoadFromOffset(mips64::kLoadSignedByte, mips64::A0, mips64::A1, 0x8001);
  __ LoadFromOffset(mips64::kLoadSignedByte, mips64::A0, mips64::A1, 0x10000);
  __ LoadFromOffset(mips64::kLoadSignedByte, mips64::A0, mips64::A1, 0x12345678);
  __ LoadFromOffset(mips64::kLoadSignedByte, mips64::A0, mips64::A1, -256);
  __ LoadFromOffset(mips64::kLoadSignedByte, mips64::A0, mips64::A1, -32768);
  __ LoadFromOffset(mips64::kLoadSignedByte, mips64::A0, mips64::A1, 0xABCDEF00);

  __ LoadFromOffset(mips64::kLoadUnsignedByte, mips64::A0, mips64::A0, 0);
  __ LoadFromOffset(mips64::kLoadUnsignedByte, mips64::A0, mips64::A1, 0);
  __ LoadFromOffset(mips64::kLoadUnsignedByte, mips64::A0, mips64::A1, 1);
  __ LoadFromOffset(mips64::kLoadUnsignedByte, mips64::A0, mips64::A1, 256);
  __ LoadFromOffset(mips64::kLoadUnsignedByte, mips64::A0, mips64::A1, 1000);
  __ LoadFromOffset(mips64::kLoadUnsignedByte, mips64::A0, mips64::A1, 0x7FFF);
  __ LoadFromOffset(mips64::kLoadUnsignedByte, mips64::A0, mips64::A1, 0x8000);
  __ LoadFromOffset(mips64::kLoadUnsignedByte, mips64::A0, mips64::A1, 0x8001);
  __ LoadFromOffset(mips64::kLoadUnsignedByte, mips64::A0, mips64::A1, 0x10000);
  __ LoadFromOffset(mips64::kLoadUnsignedByte, mips64::A0, mips64::A1, 0x12345678);
  __ LoadFromOffset(mips64::kLoadUnsignedByte, mips64::A0, mips64::A1, -256);
  __ LoadFromOffset(mips64::kLoadUnsignedByte, mips64::A0, mips64::A1, -32768);
  __ LoadFromOffset(mips64::kLoadUnsignedByte, mips64::A0, mips64::A1, 0xABCDEF00);

  __ LoadFromOffset(mips64::kLoadSignedHalfword, mips64::A0, mips64::A0, 0);
  __ LoadFromOffset(mips64::kLoadSignedHalfword, mips64::A0, mips64::A1, 0);
  __ LoadFromOffset(mips64::kLoadSignedHalfword, mips64::A0, mips64::A1, 2);
  __ LoadFromOffset(mips64::kLoadSignedHalfword, mips64::A0, mips64::A1, 256);
  __ LoadFromOffset(mips64::kLoadSignedHalfword, mips64::A0, mips64::A1, 1000);
  __ LoadFromOffset(mips64::kLoadSignedHalfword, mips64::A0, mips64::A1, 0x7FFE);
  __ LoadFromOffset(mips64::kLoadSignedHalfword, mips64::A0, mips64::A1, 0x8000);
  __ LoadFromOffset(mips64::kLoadSignedHalfword, mips64::A0, mips64::A1, 0x8002);
  __ LoadFromOffset(mips64::kLoadSignedHalfword, mips64::A0, mips64::A1, 0x10000);
  __ LoadFromOffset(mips64::kLoadSignedHalfword, mips64::A0, mips64::A1, 0x12345678);
  __ LoadFromOffset(mips64::kLoadSignedHalfword, mips64::A0, mips64::A1, -256);
  __ LoadFromOffset(mips64::kLoadSignedHalfword, mips64::A0, mips64::A1, -32768);
  __ LoadFromOffset(mips64::kLoadSignedHalfword, mips64::A0, mips64::A1, 0xABCDEF00);

  __ LoadFromOffset(mips64::kLoadUnsignedHalfword, mips64::A0, mips64::A0, 0);
  __ LoadFromOffset(mips64::kLoadUnsignedHalfword, mips64::A0, mips64::A1, 0);
  __ LoadFromOffset(mips64::kLoadUnsignedHalfword, mips64::A0, mips64::A1, 2);
  __ LoadFromOffset(mips64::kLoadUnsignedHalfword, mips64::A0, mips64::A1, 256);
  __ LoadFromOffset(mips64::kLoadUnsignedHalfword, mips64::A0, mips64::A1, 1000);
  __ LoadFromOffset(mips64::kLoadUnsignedHalfword, mips64::A0, mips64::A1, 0x7FFE);
  __ LoadFromOffset(mips64::kLoadUnsignedHalfword, mips64::A0, mips64::A1, 0x8000);
  __ LoadFromOffset(mips64::kLoadUnsignedHalfword, mips64::A0, mips64::A1, 0x8002);
  __ LoadFromOffset(mips64::kLoadUnsignedHalfword, mips64::A0, mips64::A1, 0x10000);
  __ LoadFromOffset(mips64::kLoadUnsignedHalfword, mips64::A0, mips64::A1, 0x12345678);
  __ LoadFromOffset(mips64::kLoadUnsignedHalfword, mips64::A0, mips64::A1, -256);
  __ LoadFromOffset(mips64::kLoadUnsignedHalfword, mips64::A0, mips64::A1, -32768);
  __ LoadFromOffset(mips64::kLoadUnsignedHalfword, mips64::A0, mips64::A1, 0xABCDEF00);

  __ LoadFromOffset(mips64::kLoadWord, mips64::A0, mips64::A0, 0);
  __ LoadFromOffset(mips64::kLoadWord, mips64::A0, mips64::A1, 0);
  __ LoadFromOffset(mips64::kLoadWord, mips64::A0, mips64::A1, 4);
  __ LoadFromOffset(mips64::kLoadWord, mips64::A0, mips64::A1, 256);
  __ LoadFromOffset(mips64::kLoadWord, mips64::A0, mips64::A1, 1000);
  __ LoadFromOffset(mips64::kLoadWord, mips64::A0, mips64::A1, 0x7FFC);
  __ LoadFromOffset(mips64::kLoadWord, mips64::A0, mips64::A1, 0x8000);
  __ LoadFromOffset(mips64::kLoadWord, mips64::A0, mips64::A1, 0x8004);
  __ LoadFromOffset(mips64::kLoadWord, mips64::A0, mips64::A1, 0x10000);
  __ LoadFromOffset(mips64::kLoadWord, mips64::A0, mips64::A1, 0x12345678);
  __ LoadFromOffset(mips64::kLoadWord, mips64::A0, mips64::A1, -256);
  __ LoadFromOffset(mips64::kLoadWord, mips64::A0, mips64::A1, -32768);
  __ LoadFromOffset(mips64::kLoadWord, mips64::A0, mips64::A1, 0xABCDEF00);

  __ LoadFromOffset(mips64::kLoadUnsignedWord, mips64::A0, mips64::A0, 0);
  __ LoadFromOffset(mips64::kLoadUnsignedWord, mips64::A0, mips64::A1, 0);
  __ LoadFromOffset(mips64::kLoadUnsignedWord, mips64::A0, mips64::A1, 4);
  __ LoadFromOffset(mips64::kLoadUnsignedWord, mips64::A0, mips64::A1, 256);
  __ LoadFromOffset(mips64::kLoadUnsignedWord, mips64::A0, mips64::A1, 1000);
  __ LoadFromOffset(mips64::kLoadUnsignedWord, mips64::A0, mips64::A1, 0x7FFC);
  __ LoadFromOffset(mips64::kLoadUnsignedWord, mips64::A0, mips64::A1, 0x8000);
  __ LoadFromOffset(mips64::kLoadUnsignedWord, mips64::A0, mips64::A1, 0x8004);
  __ LoadFromOffset(mips64::kLoadUnsignedWord, mips64::A0, mips64::A1, 0x10000);
  __ LoadFromOffset(mips64::kLoadUnsignedWord, mips64::A0, mips64::A1, 0x12345678);
  __ LoadFromOffset(mips64::kLoadUnsignedWord, mips64::A0, mips64::A1, -256);
  __ LoadFromOffset(mips64::kLoadUnsignedWord, mips64::A0, mips64::A1, -32768);
  __ LoadFromOffset(mips64::kLoadUnsignedWord, mips64::A0, mips64::A1, 0xABCDEF00);

  __ LoadFromOffset(mips64::kLoadDoubleword, mips64::A0, mips64::A0, 0);
  __ LoadFromOffset(mips64::kLoadDoubleword, mips64::A0, mips64::A1, 0);
  __ LoadFromOffset(mips64::kLoadDoubleword, mips64::A0, mips64::A1, 4);
  __ LoadFromOffset(mips64::kLoadDoubleword, mips64::A0, mips64::A1, 256);
  __ LoadFromOffset(mips64::kLoadDoubleword, mips64::A0, mips64::A1, 1000);
  __ LoadFromOffset(mips64::kLoadDoubleword, mips64::A0, mips64::A1, 0x7FFC);
  __ LoadFromOffset(mips64::kLoadDoubleword, mips64::A0, mips64::A1, 0x8000);
  __ LoadFromOffset(mips64::kLoadDoubleword, mips64::A0, mips64::A1, 0x8004);
  __ LoadFromOffset(mips64::kLoadDoubleword, mips64::A0, mips64::A1, 0x10000);
  __ LoadFromOffset(mips64::kLoadDoubleword, mips64::A0, mips64::A1, 0x12345678);
  __ LoadFromOffset(mips64::kLoadDoubleword, mips64::A0, mips64::A1, -256);
  __ LoadFromOffset(mips64::kLoadDoubleword, mips64::A0, mips64::A1, -32768);
  __ LoadFromOffset(mips64::kLoadDoubleword, mips64::A0, mips64::A1, 0xABCDEF00);

  const char* expected =
      "lb $a0, 0($a0)\n"
      "lb $a0, 0($a1)\n"
      "lb $a0, 1($a1)\n"
      "lb $a0, 256($a1)\n"
      "lb $a0, 1000($a1)\n"
      "lb $a0, 0x7FFF($a1)\n"
      "ori $at, $zero, 0x8000\n"
      "daddu $at, $at, $a1\n"
      "lb $a0, 0($at)\n"
      "ori $at, $zero, 0x8000\n"
      "daddu $at, $at, $a1\n"
      "lb $a0, 1($at)\n"
      "lui $at, 1\n"
      "daddu $at, $at, $a1\n"
      "lb $a0, 0($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, 0x5678\n"
      "daddu $at, $at, $a1\n"
      "lb $a0, 0($at)\n"
      "lb $a0, -256($a1)\n"
      "lb $a0, -32768($a1)\n"
      "lui $at, 0xABCD\n"
      "ori $at, 0xEF00\n"
      "daddu $at, $at, $a1\n"
      "lb $a0, 0($at)\n"

      "lbu $a0, 0($a0)\n"
      "lbu $a0, 0($a1)\n"
      "lbu $a0, 1($a1)\n"
      "lbu $a0, 256($a1)\n"
      "lbu $a0, 1000($a1)\n"
      "lbu $a0, 0x7FFF($a1)\n"
      "ori $at, $zero, 0x8000\n"
      "daddu $at, $at, $a1\n"
      "lbu $a0, 0($at)\n"
      "ori $at, $zero, 0x8000\n"
      "daddu $at, $at, $a1\n"
      "lbu $a0, 1($at)\n"
      "lui $at, 1\n"
      "daddu $at, $at, $a1\n"
      "lbu $a0, 0($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, 0x5678\n"
      "daddu $at, $at, $a1\n"
      "lbu $a0, 0($at)\n"
      "lbu $a0, -256($a1)\n"
      "lbu $a0, -32768($a1)\n"
      "lui $at, 0xABCD\n"
      "ori $at, 0xEF00\n"
      "daddu $at, $at, $a1\n"
      "lbu $a0, 0($at)\n"

      "lh $a0, 0($a0)\n"
      "lh $a0, 0($a1)\n"
      "lh $a0, 2($a1)\n"
      "lh $a0, 256($a1)\n"
      "lh $a0, 1000($a1)\n"
      "lh $a0, 0x7FFE($a1)\n"
      "ori $at, $zero, 0x8000\n"
      "daddu $at, $at, $a1\n"
      "lh $a0, 0($at)\n"
      "ori $at, $zero, 0x8000\n"
      "daddu $at, $at, $a1\n"
      "lh $a0, 2($at)\n"
      "lui $at, 1\n"
      "daddu $at, $at, $a1\n"
      "lh $a0, 0($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, 0x5678\n"
      "daddu $at, $at, $a1\n"
      "lh $a0, 0($at)\n"
      "lh $a0, -256($a1)\n"
      "lh $a0, -32768($a1)\n"
      "lui $at, 0xABCD\n"
      "ori $at, 0xEF00\n"
      "daddu $at, $at, $a1\n"
      "lh $a0, 0($at)\n"

      "lhu $a0, 0($a0)\n"
      "lhu $a0, 0($a1)\n"
      "lhu $a0, 2($a1)\n"
      "lhu $a0, 256($a1)\n"
      "lhu $a0, 1000($a1)\n"
      "lhu $a0, 0x7FFE($a1)\n"
      "ori $at, $zero, 0x8000\n"
      "daddu $at, $at, $a1\n"
      "lhu $a0, 0($at)\n"
      "ori $at, $zero, 0x8000\n"
      "daddu $at, $at, $a1\n"
      "lhu $a0, 2($at)\n"
      "lui $at, 1\n"
      "daddu $at, $at, $a1\n"
      "lhu $a0, 0($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, 0x5678\n"
      "daddu $at, $at, $a1\n"
      "lhu $a0, 0($at)\n"
      "lhu $a0, -256($a1)\n"
      "lhu $a0, -32768($a1)\n"
      "lui $at, 0xABCD\n"
      "ori $at, 0xEF00\n"
      "daddu $at, $at, $a1\n"
      "lhu $a0, 0($at)\n"

      "lw $a0, 0($a0)\n"
      "lw $a0, 0($a1)\n"
      "lw $a0, 4($a1)\n"
      "lw $a0, 256($a1)\n"
      "lw $a0, 1000($a1)\n"
      "lw $a0, 0x7FFC($a1)\n"
      "ori $at, $zero, 0x8000\n"
      "daddu $at, $at, $a1\n"
      "lw $a0, 0($at)\n"
      "ori $at, $zero, 0x8000\n"
      "daddu $at, $at, $a1\n"
      "lw $a0, 4($at)\n"
      "lui $at, 1\n"
      "daddu $at, $at, $a1\n"
      "lw $a0, 0($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, 0x5678\n"
      "daddu $at, $at, $a1\n"
      "lw $a0, 0($at)\n"
      "lw $a0, -256($a1)\n"
      "lw $a0, -32768($a1)\n"
      "lui $at, 0xABCD\n"
      "ori $at, 0xEF00\n"
      "daddu $at, $at, $a1\n"
      "lw $a0, 0($at)\n"

      "lwu $a0, 0($a0)\n"
      "lwu $a0, 0($a1)\n"
      "lwu $a0, 4($a1)\n"
      "lwu $a0, 256($a1)\n"
      "lwu $a0, 1000($a1)\n"
      "lwu $a0, 0x7FFC($a1)\n"
      "ori $at, $zero, 0x8000\n"
      "daddu $at, $at, $a1\n"
      "lwu $a0, 0($at)\n"
      "ori $at, $zero, 0x8000\n"
      "daddu $at, $at, $a1\n"
      "lwu $a0, 4($at)\n"
      "lui $at, 1\n"
      "daddu $at, $at, $a1\n"
      "lwu $a0, 0($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, 0x5678\n"
      "daddu $at, $at, $a1\n"
      "lwu $a0, 0($at)\n"
      "lwu $a0, -256($a1)\n"
      "lwu $a0, -32768($a1)\n"
      "lui $at, 0xABCD\n"
      "ori $at, 0xEF00\n"
      "daddu $at, $at, $a1\n"
      "lwu $a0, 0($at)\n"

      "ld $a0, 0($a0)\n"
      "ld $a0, 0($a1)\n"
      "lwu $a0, 4($a1)\n"
      "lwu $t3, 8($a1)\n"
      "dins $a0, $t3, 32, 32\n"
      "ld $a0, 256($a1)\n"
      "ld $a0, 1000($a1)\n"
      "ori $at, $zero, 0x7FF8\n"
      "daddu $at, $at, $a1\n"
      "lwu $a0, 4($at)\n"
      "lwu $t3, 8($at)\n"
      "dins $a0, $t3, 32, 32\n"
      "ori $at, $zero, 0x8000\n"
      "daddu $at, $at, $a1\n"
      "ld $a0, 0($at)\n"
      "ori $at, $zero, 0x8000\n"
      "daddu $at, $at, $a1\n"
      "lwu $a0, 4($at)\n"
      "lwu $t3, 8($at)\n"
      "dins $a0, $t3, 32, 32\n"
      "lui $at, 1\n"
      "daddu $at, $at, $a1\n"
      "ld $a0, 0($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, 0x5678\n"
      "daddu $at, $at, $a1\n"
      "ld $a0, 0($at)\n"
      "ld $a0, -256($a1)\n"
      "ld $a0, -32768($a1)\n"
      "lui $at, 0xABCD\n"
      "ori $at, 0xEF00\n"
      "daddu $at, $at, $a1\n"
      "ld $a0, 0($at)\n";
  DriverStr(expected, "LoadFromOffset");
}

TEST_F(AssemblerMIPS64Test, LoadFpuFromOffset) {
  __ LoadFpuFromOffset(mips64::kLoadWord, mips64::F0, mips64::A0, 0);
  __ LoadFpuFromOffset(mips64::kLoadWord, mips64::F0, mips64::A0, 4);
  __ LoadFpuFromOffset(mips64::kLoadWord, mips64::F0, mips64::A0, 256);
  __ LoadFpuFromOffset(mips64::kLoadWord, mips64::F0, mips64::A0, 0x7FFC);
  __ LoadFpuFromOffset(mips64::kLoadWord, mips64::F0, mips64::A0, 0x8000);
  __ LoadFpuFromOffset(mips64::kLoadWord, mips64::F0, mips64::A0, 0x8004);
  __ LoadFpuFromOffset(mips64::kLoadWord, mips64::F0, mips64::A0, 0x10000);
  __ LoadFpuFromOffset(mips64::kLoadWord, mips64::F0, mips64::A0, 0x12345678);
  __ LoadFpuFromOffset(mips64::kLoadWord, mips64::F0, mips64::A0, -256);
  __ LoadFpuFromOffset(mips64::kLoadWord, mips64::F0, mips64::A0, -32768);
  __ LoadFpuFromOffset(mips64::kLoadWord, mips64::F0, mips64::A0, 0xABCDEF00);

  __ LoadFpuFromOffset(mips64::kLoadDoubleword, mips64::F0, mips64::A0, 0);
  __ LoadFpuFromOffset(mips64::kLoadDoubleword, mips64::F0, mips64::A0, 4);
  __ LoadFpuFromOffset(mips64::kLoadDoubleword, mips64::F0, mips64::A0, 256);
  __ LoadFpuFromOffset(mips64::kLoadDoubleword, mips64::F0, mips64::A0, 0x7FFC);
  __ LoadFpuFromOffset(mips64::kLoadDoubleword, mips64::F0, mips64::A0, 0x8000);
  __ LoadFpuFromOffset(mips64::kLoadDoubleword, mips64::F0, mips64::A0, 0x8004);
  __ LoadFpuFromOffset(mips64::kLoadDoubleword, mips64::F0, mips64::A0, 0x10000);
  __ LoadFpuFromOffset(mips64::kLoadDoubleword, mips64::F0, mips64::A0, 0x12345678);
  __ LoadFpuFromOffset(mips64::kLoadDoubleword, mips64::F0, mips64::A0, -256);
  __ LoadFpuFromOffset(mips64::kLoadDoubleword, mips64::F0, mips64::A0, -32768);
  __ LoadFpuFromOffset(mips64::kLoadDoubleword, mips64::F0, mips64::A0, 0xABCDEF00);

  const char* expected =
      "lwc1 $f0, 0($a0)\n"
      "lwc1 $f0, 4($a0)\n"
      "lwc1 $f0, 256($a0)\n"
      "lwc1 $f0, 0x7FFC($a0)\n"
      "ori $at, $zero, 0x8000\n"
      "daddu $at, $at, $a0\n"
      "lwc1 $f0, 0($at)\n"
      "ori $at, $zero, 0x8000\n"
      "daddu $at, $at, $a0\n"
      "lwc1 $f0, 4($at)\n"
      "lui $at, 1\n"
      "daddu $at, $at, $a0\n"
      "lwc1 $f0, 0($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, 0x5678\n"
      "daddu $at, $at, $a0\n"
      "lwc1 $f0, 0($at)\n"
      "lwc1 $f0, -256($a0)\n"
      "lwc1 $f0, -32768($a0)\n"
      "lui $at, 0xABCD\n"
      "ori $at, 0xEF00\n"
      "daddu $at, $at, $a0\n"
      "lwc1 $f0, 0($at)\n"

      "ldc1 $f0, 0($a0)\n"
      "lwc1 $f0, 4($a0)\n"
      "lw $t3, 8($a0)\n"
      "mthc1 $t3, $f0\n"
      "ldc1 $f0, 256($a0)\n"
      "ori $at, $zero, 0x7FF8\n"
      "daddu $at, $at, $a0\n"
      "lwc1 $f0, 4($at)\n"
      "lw $t3, 8($at)\n"
      "mthc1 $t3, $f0\n"
      "ori $at, $zero, 0x8000\n"
      "daddu $at, $at, $a0\n"
      "ldc1 $f0, 0($at)\n"
      "ori $at, $zero, 0x8000\n"
      "daddu $at, $at, $a0\n"
      "lwc1 $f0, 4($at)\n"
      "lw $t3, 8($at)\n"
      "mthc1 $t3, $f0\n"
      "lui $at, 1\n"
      "daddu $at, $at, $a0\n"
      "ldc1 $f0, 0($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, 0x5678\n"
      "daddu $at, $at, $a0\n"
      "ldc1 $f0, 0($at)\n"
      "ldc1 $f0, -256($a0)\n"
      "ldc1 $f0, -32768($a0)\n"
      "lui $at, 0xABCD\n"
      "ori $at, 0xEF00\n"
      "daddu $at, $at, $a0\n"
      "ldc1 $f0, 0($at)\n";
  DriverStr(expected, "LoadFpuFromOffset");
}

TEST_F(AssemblerMIPS64Test, StoreToOffset) {
  __ StoreToOffset(mips64::kStoreByte, mips64::A0, mips64::A0, 0);
  __ StoreToOffset(mips64::kStoreByte, mips64::A0, mips64::A1, 0);
  __ StoreToOffset(mips64::kStoreByte, mips64::A0, mips64::A1, 1);
  __ StoreToOffset(mips64::kStoreByte, mips64::A0, mips64::A1, 256);
  __ StoreToOffset(mips64::kStoreByte, mips64::A0, mips64::A1, 1000);
  __ StoreToOffset(mips64::kStoreByte, mips64::A0, mips64::A1, 0x7FFF);
  __ StoreToOffset(mips64::kStoreByte, mips64::A0, mips64::A1, 0x8000);
  __ StoreToOffset(mips64::kStoreByte, mips64::A0, mips64::A1, 0x8001);
  __ StoreToOffset(mips64::kStoreByte, mips64::A0, mips64::A1, 0x10000);
  __ StoreToOffset(mips64::kStoreByte, mips64::A0, mips64::A1, 0x12345678);
  __ StoreToOffset(mips64::kStoreByte, mips64::A0, mips64::A1, -256);
  __ StoreToOffset(mips64::kStoreByte, mips64::A0, mips64::A1, -32768);
  __ StoreToOffset(mips64::kStoreByte, mips64::A0, mips64::A1, 0xABCDEF00);

  __ StoreToOffset(mips64::kStoreHalfword, mips64::A0, mips64::A0, 0);
  __ StoreToOffset(mips64::kStoreHalfword, mips64::A0, mips64::A1, 0);
  __ StoreToOffset(mips64::kStoreHalfword, mips64::A0, mips64::A1, 2);
  __ StoreToOffset(mips64::kStoreHalfword, mips64::A0, mips64::A1, 256);
  __ StoreToOffset(mips64::kStoreHalfword, mips64::A0, mips64::A1, 1000);
  __ StoreToOffset(mips64::kStoreHalfword, mips64::A0, mips64::A1, 0x7FFE);
  __ StoreToOffset(mips64::kStoreHalfword, mips64::A0, mips64::A1, 0x8000);
  __ StoreToOffset(mips64::kStoreHalfword, mips64::A0, mips64::A1, 0x8002);
  __ StoreToOffset(mips64::kStoreHalfword, mips64::A0, mips64::A1, 0x10000);
  __ StoreToOffset(mips64::kStoreHalfword, mips64::A0, mips64::A1, 0x12345678);
  __ StoreToOffset(mips64::kStoreHalfword, mips64::A0, mips64::A1, -256);
  __ StoreToOffset(mips64::kStoreHalfword, mips64::A0, mips64::A1, -32768);
  __ StoreToOffset(mips64::kStoreHalfword, mips64::A0, mips64::A1, 0xABCDEF00);

  __ StoreToOffset(mips64::kStoreWord, mips64::A0, mips64::A0, 0);
  __ StoreToOffset(mips64::kStoreWord, mips64::A0, mips64::A1, 0);
  __ StoreToOffset(mips64::kStoreWord, mips64::A0, mips64::A1, 4);
  __ StoreToOffset(mips64::kStoreWord, mips64::A0, mips64::A1, 256);
  __ StoreToOffset(mips64::kStoreWord, mips64::A0, mips64::A1, 1000);
  __ StoreToOffset(mips64::kStoreWord, mips64::A0, mips64::A1, 0x7FFC);
  __ StoreToOffset(mips64::kStoreWord, mips64::A0, mips64::A1, 0x8000);
  __ StoreToOffset(mips64::kStoreWord, mips64::A0, mips64::A1, 0x8004);
  __ StoreToOffset(mips64::kStoreWord, mips64::A0, mips64::A1, 0x10000);
  __ StoreToOffset(mips64::kStoreWord, mips64::A0, mips64::A1, 0x12345678);
  __ StoreToOffset(mips64::kStoreWord, mips64::A0, mips64::A1, -256);
  __ StoreToOffset(mips64::kStoreWord, mips64::A0, mips64::A1, -32768);
  __ StoreToOffset(mips64::kStoreWord, mips64::A0, mips64::A1, 0xABCDEF00);

  __ StoreToOffset(mips64::kStoreDoubleword, mips64::A0, mips64::A0, 0);
  __ StoreToOffset(mips64::kStoreDoubleword, mips64::A0, mips64::A1, 0);
  __ StoreToOffset(mips64::kStoreDoubleword, mips64::A0, mips64::A1, 4);
  __ StoreToOffset(mips64::kStoreDoubleword, mips64::A0, mips64::A1, 256);
  __ StoreToOffset(mips64::kStoreDoubleword, mips64::A0, mips64::A1, 1000);
  __ StoreToOffset(mips64::kStoreDoubleword, mips64::A0, mips64::A1, 0x7FFC);
  __ StoreToOffset(mips64::kStoreDoubleword, mips64::A0, mips64::A1, 0x8000);
  __ StoreToOffset(mips64::kStoreDoubleword, mips64::A0, mips64::A1, 0x8004);
  __ StoreToOffset(mips64::kStoreDoubleword, mips64::A0, mips64::A1, 0x10000);
  __ StoreToOffset(mips64::kStoreDoubleword, mips64::A0, mips64::A1, 0x12345678);
  __ StoreToOffset(mips64::kStoreDoubleword, mips64::A0, mips64::A1, -256);
  __ StoreToOffset(mips64::kStoreDoubleword, mips64::A0, mips64::A1, -32768);
  __ StoreToOffset(mips64::kStoreDoubleword, mips64::A0, mips64::A1, 0xABCDEF00);

  const char* expected =
      "sb $a0, 0($a0)\n"
      "sb $a0, 0($a1)\n"
      "sb $a0, 1($a1)\n"
      "sb $a0, 256($a1)\n"
      "sb $a0, 1000($a1)\n"
      "sb $a0, 0x7FFF($a1)\n"
      "ori $at, $zero, 0x8000\n"
      "daddu $at, $at, $a1\n"
      "sb $a0, 0($at)\n"
      "ori $at, $zero, 0x8000\n"
      "daddu $at, $at, $a1\n"
      "sb $a0, 1($at)\n"
      "lui $at, 1\n"
      "daddu $at, $at, $a1\n"
      "sb $a0, 0($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, 0x5678\n"
      "daddu $at, $at, $a1\n"
      "sb $a0, 0($at)\n"
      "sb $a0, -256($a1)\n"
      "sb $a0, -32768($a1)\n"
      "lui $at, 0xABCD\n"
      "ori $at, 0xEF00\n"
      "daddu $at, $at, $a1\n"
      "sb $a0, 0($at)\n"

      "sh $a0, 0($a0)\n"
      "sh $a0, 0($a1)\n"
      "sh $a0, 2($a1)\n"
      "sh $a0, 256($a1)\n"
      "sh $a0, 1000($a1)\n"
      "sh $a0, 0x7FFE($a1)\n"
      "ori $at, $zero, 0x8000\n"
      "daddu $at, $at, $a1\n"
      "sh $a0, 0($at)\n"
      "ori $at, $zero, 0x8000\n"
      "daddu $at, $at, $a1\n"
      "sh $a0, 2($at)\n"
      "lui $at, 1\n"
      "daddu $at, $at, $a1\n"
      "sh $a0, 0($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, 0x5678\n"
      "daddu $at, $at, $a1\n"
      "sh $a0, 0($at)\n"
      "sh $a0, -256($a1)\n"
      "sh $a0, -32768($a1)\n"
      "lui $at, 0xABCD\n"
      "ori $at, 0xEF00\n"
      "daddu $at, $at, $a1\n"
      "sh $a0, 0($at)\n"

      "sw $a0, 0($a0)\n"
      "sw $a0, 0($a1)\n"
      "sw $a0, 4($a1)\n"
      "sw $a0, 256($a1)\n"
      "sw $a0, 1000($a1)\n"
      "sw $a0, 0x7FFC($a1)\n"
      "ori $at, $zero, 0x8000\n"
      "daddu $at, $at, $a1\n"
      "sw $a0, 0($at)\n"
      "ori $at, $zero, 0x8000\n"
      "daddu $at, $at, $a1\n"
      "sw $a0, 4($at)\n"
      "lui $at, 1\n"
      "daddu $at, $at, $a1\n"
      "sw $a0, 0($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, 0x5678\n"
      "daddu $at, $at, $a1\n"
      "sw $a0, 0($at)\n"
      "sw $a0, -256($a1)\n"
      "sw $a0, -32768($a1)\n"
      "lui $at, 0xABCD\n"
      "ori $at, 0xEF00\n"
      "daddu $at, $at, $a1\n"
      "sw $a0, 0($at)\n"

      "sd $a0, 0($a0)\n"
      "sd $a0, 0($a1)\n"
      "sw $a0, 4($a1)\n"
      "dsrl32 $t3, $a0, 0\n"
      "sw $t3, 8($a1)\n"
      "sd $a0, 256($a1)\n"
      "sd $a0, 1000($a1)\n"
      "ori $at, $zero, 0x7FF8\n"
      "daddu $at, $at, $a1\n"
      "sw $a0, 4($at)\n"
      "dsrl32 $t3, $a0, 0\n"
      "sw $t3, 8($at)\n"
      "ori $at, $zero, 0x8000\n"
      "daddu $at, $at, $a1\n"
      "sd $a0, 0($at)\n"
      "ori $at, $zero, 0x8000\n"
      "daddu $at, $at, $a1\n"
      "sw $a0, 4($at)\n"
      "dsrl32 $t3, $a0, 0\n"
      "sw $t3, 8($at)\n"
      "lui $at, 1\n"
      "daddu $at, $at, $a1\n"
      "sd $a0, 0($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, 0x5678\n"
      "daddu $at, $at, $a1\n"
      "sd $a0, 0($at)\n"
      "sd $a0, -256($a1)\n"
      "sd $a0, -32768($a1)\n"
      "lui $at, 0xABCD\n"
      "ori $at, 0xEF00\n"
      "daddu $at, $at, $a1\n"
      "sd $a0, 0($at)\n";
  DriverStr(expected, "StoreToOffset");
}

TEST_F(AssemblerMIPS64Test, StoreFpuToOffset) {
  __ StoreFpuToOffset(mips64::kStoreWord, mips64::F0, mips64::A0, 0);
  __ StoreFpuToOffset(mips64::kStoreWord, mips64::F0, mips64::A0, 4);
  __ StoreFpuToOffset(mips64::kStoreWord, mips64::F0, mips64::A0, 256);
  __ StoreFpuToOffset(mips64::kStoreWord, mips64::F0, mips64::A0, 0x7FFC);
  __ StoreFpuToOffset(mips64::kStoreWord, mips64::F0, mips64::A0, 0x8000);
  __ StoreFpuToOffset(mips64::kStoreWord, mips64::F0, mips64::A0, 0x8004);
  __ StoreFpuToOffset(mips64::kStoreWord, mips64::F0, mips64::A0, 0x10000);
  __ StoreFpuToOffset(mips64::kStoreWord, mips64::F0, mips64::A0, 0x12345678);
  __ StoreFpuToOffset(mips64::kStoreWord, mips64::F0, mips64::A0, -256);
  __ StoreFpuToOffset(mips64::kStoreWord, mips64::F0, mips64::A0, -32768);
  __ StoreFpuToOffset(mips64::kStoreWord, mips64::F0, mips64::A0, 0xABCDEF00);

  __ StoreFpuToOffset(mips64::kStoreDoubleword, mips64::F0, mips64::A0, 0);
  __ StoreFpuToOffset(mips64::kStoreDoubleword, mips64::F0, mips64::A0, 4);
  __ StoreFpuToOffset(mips64::kStoreDoubleword, mips64::F0, mips64::A0, 256);
  __ StoreFpuToOffset(mips64::kStoreDoubleword, mips64::F0, mips64::A0, 0x7FFC);
  __ StoreFpuToOffset(mips64::kStoreDoubleword, mips64::F0, mips64::A0, 0x8000);
  __ StoreFpuToOffset(mips64::kStoreDoubleword, mips64::F0, mips64::A0, 0x8004);
  __ StoreFpuToOffset(mips64::kStoreDoubleword, mips64::F0, mips64::A0, 0x10000);
  __ StoreFpuToOffset(mips64::kStoreDoubleword, mips64::F0, mips64::A0, 0x12345678);
  __ StoreFpuToOffset(mips64::kStoreDoubleword, mips64::F0, mips64::A0, -256);
  __ StoreFpuToOffset(mips64::kStoreDoubleword, mips64::F0, mips64::A0, -32768);
  __ StoreFpuToOffset(mips64::kStoreDoubleword, mips64::F0, mips64::A0, 0xABCDEF00);

  const char* expected =
      "swc1 $f0, 0($a0)\n"
      "swc1 $f0, 4($a0)\n"
      "swc1 $f0, 256($a0)\n"
      "swc1 $f0, 0x7FFC($a0)\n"
      "ori $at, $zero, 0x8000\n"
      "daddu $at, $at, $a0\n"
      "swc1 $f0, 0($at)\n"
      "ori $at, $zero, 0x8000\n"
      "daddu $at, $at, $a0\n"
      "swc1 $f0, 4($at)\n"
      "lui $at, 1\n"
      "daddu $at, $at, $a0\n"
      "swc1 $f0, 0($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, 0x5678\n"
      "daddu $at, $at, $a0\n"
      "swc1 $f0, 0($at)\n"
      "swc1 $f0, -256($a0)\n"
      "swc1 $f0, -32768($a0)\n"
      "lui $at, 0xABCD\n"
      "ori $at, 0xEF00\n"
      "daddu $at, $at, $a0\n"
      "swc1 $f0, 0($at)\n"

      "sdc1 $f0, 0($a0)\n"
      "mfhc1 $t3, $f0\n"
      "swc1 $f0, 4($a0)\n"
      "sw $t3, 8($a0)\n"
      "sdc1 $f0, 256($a0)\n"
      "ori $at, $zero, 0x7FF8\n"
      "daddu $at, $at, $a0\n"
      "mfhc1 $t3, $f0\n"
      "swc1 $f0, 4($at)\n"
      "sw $t3, 8($at)\n"
      "ori $at, $zero, 0x8000\n"
      "daddu $at, $at, $a0\n"
      "sdc1 $f0, 0($at)\n"
      "ori $at, $zero, 0x8000\n"
      "daddu $at, $at, $a0\n"
      "mfhc1 $t3, $f0\n"
      "swc1 $f0, 4($at)\n"
      "sw $t3, 8($at)\n"
      "lui $at, 1\n"
      "daddu $at, $at, $a0\n"
      "sdc1 $f0, 0($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, 0x5678\n"
      "daddu $at, $at, $a0\n"
      "sdc1 $f0, 0($at)\n"
      "sdc1 $f0, -256($a0)\n"
      "sdc1 $f0, -32768($a0)\n"
      "lui $at, 0xABCD\n"
      "ori $at, 0xEF00\n"
      "daddu $at, $at, $a0\n"
      "sdc1 $f0, 0($at)\n";
  DriverStr(expected, "StoreFpuToOffset");
}

///////////////////////
// Loading Constants //
///////////////////////

TEST_F(AssemblerMIPS64Test, LoadConst32) {
  // IsUint<16>(value)
  __ LoadConst32(mips64::V0, 0);
  __ LoadConst32(mips64::V0, 65535);
  // IsInt<16>(value)
  __ LoadConst32(mips64::V0, -1);
  __ LoadConst32(mips64::V0, -32768);
  // Everything else
  __ LoadConst32(mips64::V0, 65536);
  __ LoadConst32(mips64::V0, 65537);
  __ LoadConst32(mips64::V0, 2147483647);
  __ LoadConst32(mips64::V0, -32769);
  __ LoadConst32(mips64::V0, -65536);
  __ LoadConst32(mips64::V0, -65537);
  __ LoadConst32(mips64::V0, -2147483647);
  __ LoadConst32(mips64::V0, -2147483648);

  const char* expected =
      // IsUint<16>(value)
      "ori $v0, $zero, 0\n"         // __ LoadConst32(mips64::V0, 0);
      "ori $v0, $zero, 65535\n"     // __ LoadConst32(mips64::V0, 65535);
      // IsInt<16>(value)
      "addiu $v0, $zero, -1\n"      // __ LoadConst32(mips64::V0, -1);
      "addiu $v0, $zero, -32768\n"  // __ LoadConst32(mips64::V0, -32768);
      // Everything else
      "lui $v0, 1\n"                // __ LoadConst32(mips64::V0, 65536);
      "lui $v0, 1\n"                // __ LoadConst32(mips64::V0, 65537);
      "ori $v0, 1\n"                //                 "
      "lui $v0, 32767\n"            // __ LoadConst32(mips64::V0, 2147483647);
      "ori $v0, 65535\n"            //                 "
      "lui $v0, 65535\n"            // __ LoadConst32(mips64::V0, -32769);
      "ori $v0, 32767\n"            //                 "
      "lui $v0, 65535\n"            // __ LoadConst32(mips64::V0, -65536);
      "lui $v0, 65534\n"            // __ LoadConst32(mips64::V0, -65537);
      "ori $v0, 65535\n"            //                 "
      "lui $v0, 32768\n"            // __ LoadConst32(mips64::V0, -2147483647);
      "ori $v0, 1\n"                //                 "
      "lui $v0, 32768\n";           // __ LoadConst32(mips64::V0, -2147483648);
  DriverStr(expected, "LoadConst32");
}

TEST_F(AssemblerMIPS64Test, LoadConst64) {
  // IsUint<16>(value)
  __ LoadConst64(mips64::V0, 0);
  __ LoadConst64(mips64::V0, 65535);
  // IsInt<16>(value)
  __ LoadConst64(mips64::V0, -1);
  __ LoadConst64(mips64::V0, -32768);
  // (value & 0xFFFF) == 0 && IsInt<16>(value >> 16)
  __ LoadConst64(mips64::V0, 65536);
  __ LoadConst64(mips64::V0, -65536);
  __ LoadConst64(mips64::V0, -2147483648);
  // IsInt<32>(value)
  __ LoadConst64(mips64::V0, 65537);
  __ LoadConst64(mips64::V0, 2147483647);
  __ LoadConst64(mips64::V0, -32769);
  __ LoadConst64(mips64::V0, -65537);
  __ LoadConst64(mips64::V0, -2147483647);
  // ori + dahi
  __ LoadConst64(mips64::V0, 0x0000000100000000ll);
  __ LoadConst64(mips64::V0, 0x00007FFF00000000ll);
  __ LoadConst64(mips64::V0, 0xFFFF800000000000ll);
  __ LoadConst64(mips64::V0, 0xFFFFFFFF00000000ll);
  // ori + dati
  __ LoadConst64(mips64::V0, 0x0001000000000000ll);
  __ LoadConst64(mips64::V0, 0x7FFF000000000000ll);
  __ LoadConst64(mips64::V0, 0x8000000000000000ll);
  __ LoadConst64(mips64::V0, 0xFFFF000000000000ll);
  // lui + dahi
  __ LoadConst64(mips64::V0, 0x0000000100010000ll);
  __ LoadConst64(mips64::V0, 0x000000017FFF0000ll);
  __ LoadConst64(mips64::V0, 0x0000000180000000ll);
  __ LoadConst64(mips64::V0, 0x00000001FFFF0000ll);
  __ LoadConst64(mips64::V0, 0x00007FFF00010000ll);
  __ LoadConst64(mips64::V0, 0x00007FFF7FFF0000ll);
  __ LoadConst64(mips64::V0, 0x00007FFE80000000ll);
  __ LoadConst64(mips64::V0, 0x00007FFEFFFF0000ll);
  __ LoadConst64(mips64::V0, 0xFFFF800000010000ll);
  __ LoadConst64(mips64::V0, 0xFFFF80007FFF0000ll);
  __ LoadConst64(mips64::V0, 0xFFFF800080000000ll);
  __ LoadConst64(mips64::V0, 0xFFFF8000FFFF0000ll);
  __ LoadConst64(mips64::V0, 0xFFFFFFFF00010000ll);
  __ LoadConst64(mips64::V0, 0xFFFFFFFF7FFF0000ll);
  __ LoadConst64(mips64::V0, 0xFFFFFFFE80000000ll);
  __ LoadConst64(mips64::V0, 0xFFFFFFFEFFFF0000ll);
  // lui + dati
  __ LoadConst64(mips64::V0, 0x0001000000010000ll);
  __ LoadConst64(mips64::V0, 0x000100007FFF0000ll);
  __ LoadConst64(mips64::V0, 0x0001FFFF80000000ll);
  __ LoadConst64(mips64::V0, 0x0001FFFFFFFF0000ll);
  __ LoadConst64(mips64::V0, 0x7FFF000000010000ll);
  __ LoadConst64(mips64::V0, 0x7FFF00007FFF0000ll);
  __ LoadConst64(mips64::V0, 0x7FFEFFFF80000000ll);
  __ LoadConst64(mips64::V0, 0x7FFEFFFFFFFF0000ll);
  __ LoadConst64(mips64::V0, 0x8000000000010000ll);
  __ LoadConst64(mips64::V0, 0x800000007FFF0000ll);
  __ LoadConst64(mips64::V0, 0x8000FFFF80000000ll);
  __ LoadConst64(mips64::V0, 0x8000FFFFFFFF0000ll);
  __ LoadConst64(mips64::V0, 0xFFFF000000010000ll);
  __ LoadConst64(mips64::V0, 0xFFFF00007FFF0000ll);
  __ LoadConst64(mips64::V0, 0xFFFEFFFF80000000ll);
  __ LoadConst64(mips64::V0, 0xFFFEFFFFFFFF0000ll);
  // 2**N minus 1
  __ LoadConst64(mips64::V0, 0x00000000FFFFFFFFll);
  __ LoadConst64(mips64::V0, 0x00000001FFFFFFFFll);
  __ LoadConst64(mips64::V0, 0x00000003FFFFFFFFll);
  __ LoadConst64(mips64::V0, 0x00000007FFFFFFFFll);
  __ LoadConst64(mips64::V0, 0x0000000FFFFFFFFFll);
  __ LoadConst64(mips64::V0, 0x0000001FFFFFFFFFll);
  __ LoadConst64(mips64::V0, 0x0000003FFFFFFFFFll);
  __ LoadConst64(mips64::V0, 0x0000007FFFFFFFFFll);
  __ LoadConst64(mips64::V0, 0x000000FFFFFFFFFFll);
  __ LoadConst64(mips64::V0, 0x000001FFFFFFFFFFll);
  __ LoadConst64(mips64::V0, 0x000003FFFFFFFFFFll);
  __ LoadConst64(mips64::V0, 0x000007FFFFFFFFFFll);
  __ LoadConst64(mips64::V0, 0x00000FFFFFFFFFFFll);
  __ LoadConst64(mips64::V0, 0x00001FFFFFFFFFFFll);
  __ LoadConst64(mips64::V0, 0x00003FFFFFFFFFFFll);
  __ LoadConst64(mips64::V0, 0x00007FFFFFFFFFFFll);
  __ LoadConst64(mips64::V0, 0x0000FFFFFFFFFFFFll);
  __ LoadConst64(mips64::V0, 0x0001FFFFFFFFFFFFll);
  __ LoadConst64(mips64::V0, 0x0003FFFFFFFFFFFFll);
  __ LoadConst64(mips64::V0, 0x0007FFFFFFFFFFFFll);
  __ LoadConst64(mips64::V0, 0x000FFFFFFFFFFFFFll);
  __ LoadConst64(mips64::V0, 0x001FFFFFFFFFFFFFll);
  __ LoadConst64(mips64::V0, 0x003FFFFFFFFFFFFFll);
  __ LoadConst64(mips64::V0, 0x007FFFFFFFFFFFFFll);
  __ LoadConst64(mips64::V0, 0x00FFFFFFFFFFFFFFll);
  __ LoadConst64(mips64::V0, 0x01FFFFFFFFFFFFFFll);
  __ LoadConst64(mips64::V0, 0x03FFFFFFFFFFFFFFll);
  __ LoadConst64(mips64::V0, 0x07FFFFFFFFFFFFFFll);
  __ LoadConst64(mips64::V0, 0x0FFFFFFFFFFFFFFFll);
  __ LoadConst64(mips64::V0, 0x1FFFFFFFFFFFFFFFll);
  __ LoadConst64(mips64::V0, 0x3FFFFFFFFFFFFFFFll);
  __ LoadConst64(mips64::V0, 0x7FFFFFFFFFFFFFFFll);

  const char* expected =
      // IsUint<16>(value)
      "ori $v0, $zero, 0\n"          // __ LoadConst64(mips64::V0, 0);
      "ori $v0, $zero, 65535\n"      // __ LoadConst64(mips64::V0, 65535);
      // IsInt<16>(value)
      "daddiu $v0, $zero, -1\n"      // __ LoadConst64(mips64::V0, -1);
      "daddiu $v0, $zero, -32768\n"  // __ LoadConst64(mips64::V0, -32768);
      // (value & 0xFFFF) == 0 && IsInt<16>(value >> 16)
      "lui $v0, 1\n"                 // __ LoadConst64(mips64::V0, 65536);
      "lui $v0, 65535\n"             // __ LoadConst64(mips64::V0, -65536);
      "lui $v0, 32768\n"             // __ LoadConst64(mips64::V0, -2147483648);
      // IsInt<32>(value)
      "lui $v0, 1\n"                 // __ LoadConst64(mips64::V0, 65537);
      "ori $v0, 1\n"                 //                 "
      "lui $v0, 32767\n"             // __ LoadConst64(mips64::V0, 2147483647);
      "ori $v0, 65535\n"             //                 "
      "lui $v0, 65535\n"             // __ LoadConst64(mips64::V0, -32769);
      "ori $v0, 32767\n"             //                 "
      "lui $v0, 65534\n"             // __ LoadConst64(mips64::V0, -65537);
      "ori $v0, 65535\n"             //                 "
      "lui $v0, 32768\n"             // __ LoadConst64(mips64::V0, -2147483647);
      "ori $v0, 1\n"                 //                 "
      // ori + dahi
      "ori $v0, $zero, 0\n"          // __ LoadConst64(mips64::V0, 0x0000000100000000ll);
      "dahi $v0, $v0, 1\n"           //                          "
      "ori $v0, $zero, 0\n"          // __ LoadConst64(mips64::V0, 0x00007FFF00000000ll);
      "dahi $v0, $v0, 32767\n"       //                          "
      "ori $v0, $zero, 0\n"          // __ LoadConst64(mips64::V0, 0xFFFF800000000000ll);
      "dahi $v0, $v0, 32768\n"       //                          "
      "ori $v0, $zero, 0\n"          // __ LoadConst64(mips64::V0, 0xFFFFFFFF00000000ll);
      "dahi $v0, $v0, 65535\n"       //                          "
      // ori + dati
      "ori $v0, $zero, 0\n"          // __ LoadConst64(mips64::V0, 0x0001000000000000ll);
      "dati $v0, $v0, 1\n"           //                          "
      "ori $v0, $zero, 0\n"          // __ LoadConst64(mips64::V0, 0x7FFF000000000000ll);
      "dati $v0, $v0, 32767\n"       //                          "
      "ori $v0, $zero, 0\n"          // __ LoadConst64(mips64::V0, 0x8000000000000000ll);
      "dati $v0, $v0, 32768\n"       //                          "
      "ori $v0, $zero, 0\n"          // __ LoadConst64(mips64::V0, 0xFFFF000000000000ll);
      "dati $v0, $v0, 65535\n"       //                          "
      // lui + dahi
      "lui $v0, 1\n"                 // __ LoadConst64(mips64::V0, 0x0000000100010000ll);
      "dahi $v0, $v0, 1\n"           //                          "
      "lui $v0, 32767\n"             // __ LoadConst64(mips64::V0, 0x000000017FFF0000ll);
      "dahi $v0, $v0, 1\n"           //                          "
      "lui $v0, 32768\n"             // __ LoadConst64(mips64::V0, 0x0000000180000000ll);
      "dahi $v0, $v0, 2\n"           //                          "
      "lui $v0, 65535\n"             // __ LoadConst64(mips64::V0, 0x00000001FFFF0000ll);
      "dahi $v0, $v0, 2\n"           //                          "
      "lui $v0, 1\n"                 // __ LoadConst64(mips64::V0, 0x00007FFF00010000ll);
      "dahi $v0, $v0, 32767\n"       //                          "
      "lui $v0, 32767\n"             // __ LoadConst64(mips64::V0, 0x00007FFF7FFF0000ll);
      "dahi $v0, $v0, 32767\n"       //                          "
      "lui $v0, 32768\n"             // __ LoadConst64(mips64::V0, 0x00007FFE80000000ll);
      "dahi $v0, $v0, 32767\n"       //                          "
      "lui $v0, 65535\n"             // __ LoadConst64(mips64::V0, 0x00007FFEFFFF0000ll);
      "dahi $v0, $v0, 32767\n"       //                          "
      "lui $v0, 1\n"                 // __ LoadConst64(mips64::V0, 0xFFFF800000010000ll);
      "dahi $v0, $v0, 32768\n"       //                          "
      "lui $v0, 32767\n"             // __ LoadConst64(mips64::V0, 0xFFFF80007FFF0000ll);
      "dahi $v0, $v0, 32768\n"       //                          "
      "lui $v0, 32768\n"             // __ LoadConst64(mips64::V0, 0xFFFF800080000000ll);
      "dahi $v0, $v0, 32769\n"       //                          "
      "lui $v0, 65535\n"             // __ LoadConst64(mips64::V0, 0xFFFF8000FFFF0000ll);
      "dahi $v0, $v0, 32769\n"       //                          "
      "lui $v0, 1\n"                 // __ LoadConst64(mips64::V0, 0xFFFFFFFF00010000ll);
      "dahi $v0, $v0, 65535\n"       //                          "
      "lui $v0, 32767\n"             // __ LoadConst64(mips64::V0, 0xFFFFFFFF7FFF0000ll);
      "dahi $v0, $v0, 65535\n"       //                          "
      "lui $v0, 32768\n"             // __ LoadConst64(mips64::V0, 0xFFFFFFFE80000000ll);
      "dahi $v0, $v0, 65535\n"       //                          "
      "lui $v0, 65535\n"             // __ LoadConst64(mips64::V0, 0xFFFFFFFEFFFF0000ll);
      "dahi $v0, $v0, 65535\n"       //                          "
      // lui + dati
      "lui $v0, 1\n"                 // __ LoadConst64(mips64::V0, 0x0001000000010000ll);
      "dati $v0, $v0, 1\n"           //                          "
      "lui $v0, 32767\n"             // __ LoadConst64(mips64::V0, 0x000100007FFF0000ll);
      "dati $v0, $v0, 1\n"           //                          "
      "lui $v0, 32768\n"             // __ LoadConst64(mips64::V0, 0x0001FFFF80000000ll);
      "dati $v0, $v0, 2\n"           //                          "
      "lui $v0, 65535\n"             // __ LoadConst64(mips64::V0, 0x0001FFFFFFFF0000ll);
      "dati $v0, $v0, 2\n"           //                          "
      "lui $v0, 1\n"                 // __ LoadConst64(mips64::V0, 0x7FFF000000010000ll);
      "dati $v0, $v0, 32767\n"       //                          "
      "lui $v0, 32767\n"             // __ LoadConst64(mips64::V0, 0x7FFF00007FFF0000ll);
      "dati $v0, $v0, 32767\n"       //                          "
      "lui $v0, 32768\n"             // __ LoadConst64(mips64::V0, 0x7FFEFFFF80000000ll);
      "dati $v0, $v0, 32767\n"       //                          "
      "lui $v0, 65535\n"             // __ LoadConst64(mips64::V0, 0x7FFEFFFFFFFF0000ll);
      "dati $v0, $v0, 32767\n"       //                          "
      "lui $v0, 1\n"                 // __ LoadConst64(mips64::V0, 0x8000000000010000ll);
      "dati $v0, $v0, 32768\n"       //                          "
      "lui $v0, 32767\n"             // __ LoadConst64(mips64::V0, 0x800000007FFF0000ll);
      "dati $v0, $v0, 32768\n"       //                          "
      "lui $v0, 32768\n"             // __ LoadConst64(mips64::V0, 0x8000FFFF80000000ll);
      "dati $v0, $v0, 32769\n"       //                          "
      "lui $v0, 65535\n"             // __ LoadConst64(mips64::V0, 0x8000FFFFFFFF0000ll);
      "dati $v0, $v0, 32769\n"       //                          "
      "lui $v0, 1\n"                 // __ LoadConst64(mips64::V0, 0xFFFF000000010000ll);
      "dati $v0, $v0, 65535\n"       //                          "
      "lui $v0, 32767\n"             // __ LoadConst64(mips64::V0, 0xFFFF00007FFF0000ll);
      "dati $v0, $v0, 65535\n"       //                          "
      "lui $v0, 32768\n"             // __ LoadConst64(mips64::V0, 0xFFFEFFFF80000000ll);
      "dati $v0, $v0, 65535\n"       //                          "
      "lui $v0, 65535\n"             // __ LoadConst64(mips64::V0, 0xFFFEFFFFFFFF0000ll);
      "dati $v0, $v0, 65535\n"       //                          "
      // 2**N minus 1
      "daddiu $v0, $zero, -1\n"  // __ LoadConst64(mips64::V0, 0x00000000FFFFFFFFll);
      "dsrl32 $v0, $v0, 0\n"
      "daddiu $v0, $zero, -1\n"  // __ LoadConst64(mips64::V0, 0x00000001FFFFFFFFll);
      "dsrl $v0, $v0, 31\n"
      "daddiu $v0, $zero, -1\n"  // __ LoadConst64(mips64::V0, 0x00000003FFFFFFFFll);
      "dsrl $v0, $v0, 30\n"
      "daddiu $v0, $zero, -1\n"  // __ LoadConst64(mips64::V0, 0x00000007FFFFFFFFll);
      "dsrl $v0, $v0, 29\n"
      "daddiu $v0, $zero, -1\n"  // __ LoadConst64(mips64::V0, 0x0000000FFFFFFFFFll);
      "dsrl $v0, $v0, 28\n"
      "daddiu $v0, $zero, -1\n"  // __ LoadConst64(mips64::V0, 0x0000001FFFFFFFFFll);
      "dsrl $v0, $v0, 27\n"
      "daddiu $v0, $zero, -1\n"  // __ LoadConst64(mips64::V0, 0x0000003FFFFFFFFFll);
      "dsrl $v0, $v0, 26\n"
      "daddiu $v0, $zero, -1\n"  // __ LoadConst64(mips64::V0, 0x0000007FFFFFFFFFll);
      "dsrl $v0, $v0, 25\n"
      "daddiu $v0, $zero, -1\n"  // __ LoadConst64(mips64::V0, 0x000000FFFFFFFFFFll);
      "dsrl $v0, $v0, 24\n"
      "daddiu $v0, $zero, -1\n"  // __ LoadConst64(mips64::V0, 0x000001FFFFFFFFFFll);
      "dsrl $v0, $v0, 23\n"
      "daddiu $v0, $zero, -1\n"  // __ LoadConst64(mips64::V0, 0x000003FFFFFFFFFFll);
      "dsrl $v0, $v0, 22\n"
      "daddiu $v0, $zero, -1\n"  // __ LoadConst64(mips64::V0, 0x000007FFFFFFFFFFll);
      "dsrl $v0, $v0, 21\n"
      "daddiu $v0, $zero, -1\n"  // __ LoadConst64(mips64::V0, 0x00000FFFFFFFFFFFll);
      "dsrl $v0, $v0, 20\n"
      "daddiu $v0, $zero, -1\n"  // __ LoadConst64(mips64::V0, 0x00001FFFFFFFFFFFll);
      "dsrl $v0, $v0, 19\n"
      "daddiu $v0, $zero, -1\n"  // __ LoadConst64(mips64::V0, 0x00003FFFFFFFFFFFll);
      "dsrl $v0, $v0, 18\n"
      "daddiu $v0, $zero, -1\n"  // __ LoadConst64(mips64::V0, 0x00007FFFFFFFFFFFll);
      "dsrl $v0, $v0, 17\n"
      "daddiu $v0, $zero, -1\n"  // __ LoadConst64(mips64::V0, 0x0000FFFFFFFFFFFFll);
      "dsrl $v0, $v0, 16\n"
      "daddiu $v0, $zero, -1\n"  // __ LoadConst64(mips64::V0, 0x0001FFFFFFFFFFFFll);
      "dsrl $v0, $v0, 15\n"
      "daddiu $v0, $zero, -1\n"  // __ LoadConst64(mips64::V0, 0x0003FFFFFFFFFFFFll);
      "dsrl $v0, $v0, 14\n"
      "daddiu $v0, $zero, -1\n"  // __ LoadConst64(mips64::V0, 0x0007FFFFFFFFFFFFll);
      "dsrl $v0, $v0, 13\n"
      "daddiu $v0, $zero, -1\n"  // __ LoadConst64(mips64::V0, 0x000FFFFFFFFFFFFFll);
      "dsrl $v0, $v0, 12\n"
      "daddiu $v0, $zero, -1\n"  // __ LoadConst64(mips64::V0, 0x001FFFFFFFFFFFFFll);
      "dsrl $v0, $v0, 11\n"
      "daddiu $v0, $zero, -1\n"  // __ LoadConst64(mips64::V0, 0x003FFFFFFFFFFFFFll);
      "dsrl $v0, $v0, 10\n"
      "daddiu $v0, $zero, -1\n"  // __ LoadConst64(mips64::V0, 0x007FFFFFFFFFFFFFll);
      "dsrl $v0, $v0, 9\n"
      "daddiu $v0, $zero, -1\n"  // __ LoadConst64(mips64::V0, 0x00FFFFFFFFFFFFFFll);
      "dsrl $v0, $v0, 8\n"
      "daddiu $v0, $zero, -1\n"  // __ LoadConst64(mips64::V0, 0x01FFFFFFFFFFFFFFll);
      "dsrl $v0, $v0, 7\n"
      "daddiu $v0, $zero, -1\n"  // __ LoadConst64(mips64::V0, 0x03FFFFFFFFFFFFFFll);
      "dsrl $v0, $v0, 6\n"
      "daddiu $v0, $zero, -1\n"  // __ LoadConst64(mips64::V0, 0x07FFFFFFFFFFFFFFll);
      "dsrl $v0, $v0, 5\n"
      "daddiu $v0, $zero, -1\n"  // __ LoadConst64(mips64::V0, 0x0FFFFFFFFFFFFFFFll);
      "dsrl $v0, $v0, 4\n"
      "daddiu $v0, $zero, -1\n"  // __ LoadConst64(mips64::V0, 0x1FFFFFFFFFFFFFFFll);
      "dsrl $v0, $v0, 3\n"
      "daddiu $v0, $zero, -1\n"  // __ LoadConst64(mips64::V0, 0x3FFFFFFFFFFFFFFFll);
      "dsrl $v0, $v0, 2\n"
      "daddiu $v0, $zero, -1\n"  // __ LoadConst64(mips64::V0, 0x7FFFFFFFFFFFFFFFll);
      "dsrl $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW1S47) {
  __ LoadConst64(mips64::V0, 140737488355328);

  const char* expected =
      "ori $v0, $zero, 1\n"
      "dsll32 $v0, $v0, 15\n";
  DriverStr(expected, "LoadConst64IsUintW1S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW2S46) {
  __ LoadConst64(mips64::V0, 211106232532992);

  const char* expected =
      "ori $v0, $zero, 3\n"
      "dsll32 $v0, $v0, 14\n";
  DriverStr(expected, "LoadConst64IsUintW2S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW2S47) {
  __ LoadConst64(mips64::V0, 422212465065984);

  const char* expected =
      "ori $v0, $zero, 3\n"
      "dsll32 $v0, $v0, 15\n";
  DriverStr(expected, "LoadConst64IsUintW2S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW3S45) {
  __ LoadConst64(mips64::V0, 246290604621824);

  const char* expected =
      "ori $v0, $zero, 7\n"
      "dsll32 $v0, $v0, 13\n";
  DriverStr(expected, "LoadConst64IsUintW3S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW3S46) {
  __ LoadConst64(mips64::V0, 492581209243648);

  const char* expected =
      "ori $v0, $zero, 7\n"
      "dsll32 $v0, $v0, 14\n";
  DriverStr(expected, "LoadConst64IsUintW3S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW3S47) {
  __ LoadConst64(mips64::V0, 985162418487296);

  const char* expected =
      "ori $v0, $zero, 7\n"
      "dsll32 $v0, $v0, 15\n";
  DriverStr(expected, "LoadConst64IsUintW3S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW4S44) {
  __ LoadConst64(mips64::V0, 263882790666240);

  const char* expected =
      "ori $v0, $zero, 15\n"
      "dsll32 $v0, $v0, 12\n";
  DriverStr(expected, "LoadConst64IsUintW4S44");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW4S45) {
  __ LoadConst64(mips64::V0, 527765581332480);

  const char* expected =
      "ori $v0, $zero, 15\n"
      "dsll32 $v0, $v0, 13\n";
  DriverStr(expected, "LoadConst64IsUintW4S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW4S46) {
  __ LoadConst64(mips64::V0, 1055531162664960);

  const char* expected =
      "ori $v0, $zero, 15\n"
      "dsll32 $v0, $v0, 14\n";
  DriverStr(expected, "LoadConst64IsUintW4S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW4S47) {
  __ LoadConst64(mips64::V0, 2111062325329920);

  const char* expected =
      "ori $v0, $zero, 15\n"
      "dsll32 $v0, $v0, 15\n";
  DriverStr(expected, "LoadConst64IsUintW4S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW5S43) {
  __ LoadConst64(mips64::V0, 272678883688448);

  const char* expected =
      "ori $v0, $zero, 31\n"
      "dsll32 $v0, $v0, 11\n";
  DriverStr(expected, "LoadConst64IsUintW5S43");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW5S44) {
  __ LoadConst64(mips64::V0, 545357767376896);

  const char* expected =
      "ori $v0, $zero, 31\n"
      "dsll32 $v0, $v0, 12\n";
  DriverStr(expected, "LoadConst64IsUintW5S44");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW5S45) {
  __ LoadConst64(mips64::V0, 1090715534753792);

  const char* expected =
      "ori $v0, $zero, 31\n"
      "dsll32 $v0, $v0, 13\n";
  DriverStr(expected, "LoadConst64IsUintW5S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW5S46) {
  __ LoadConst64(mips64::V0, 2181431069507584);

  const char* expected =
      "ori $v0, $zero, 31\n"
      "dsll32 $v0, $v0, 14\n";
  DriverStr(expected, "LoadConst64IsUintW5S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW5S47) {
  __ LoadConst64(mips64::V0, 4362862139015168);

  const char* expected =
      "ori $v0, $zero, 31\n"
      "dsll32 $v0, $v0, 15\n";
  DriverStr(expected, "LoadConst64IsUintW5S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW6S42) {
  __ LoadConst64(mips64::V0, 277076930199552);

  const char* expected =
      "ori $v0, $zero, 63\n"
      "dsll32 $v0, $v0, 10\n";
  DriverStr(expected, "LoadConst64IsUintW6S42");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW6S43) {
  __ LoadConst64(mips64::V0, 554153860399104);

  const char* expected =
      "ori $v0, $zero, 63\n"
      "dsll32 $v0, $v0, 11\n";
  DriverStr(expected, "LoadConst64IsUintW6S43");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW6S44) {
  __ LoadConst64(mips64::V0, 1108307720798208);

  const char* expected =
      "ori $v0, $zero, 63\n"
      "dsll32 $v0, $v0, 12\n";
  DriverStr(expected, "LoadConst64IsUintW6S44");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW6S45) {
  __ LoadConst64(mips64::V0, 2216615441596416);

  const char* expected =
      "ori $v0, $zero, 63\n"
      "dsll32 $v0, $v0, 13\n";
  DriverStr(expected, "LoadConst64IsUintW6S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW6S46) {
  __ LoadConst64(mips64::V0, 4433230883192832);

  const char* expected =
      "ori $v0, $zero, 63\n"
      "dsll32 $v0, $v0, 14\n";
  DriverStr(expected, "LoadConst64IsUintW6S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW6S47) {
  __ LoadConst64(mips64::V0, 8866461766385664);

  const char* expected =
      "ori $v0, $zero, 63\n"
      "dsll32 $v0, $v0, 15\n";
  DriverStr(expected, "LoadConst64IsUintW6S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW7S41) {
  __ LoadConst64(mips64::V0, 279275953455104);

  const char* expected =
      "ori $v0, $zero, 127\n"
      "dsll32 $v0, $v0, 9\n";
  DriverStr(expected, "LoadConst64IsUintW7S41");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW7S42) {
  __ LoadConst64(mips64::V0, 558551906910208);

  const char* expected =
      "ori $v0, $zero, 127\n"
      "dsll32 $v0, $v0, 10\n";
  DriverStr(expected, "LoadConst64IsUintW7S42");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW7S43) {
  __ LoadConst64(mips64::V0, 1117103813820416);

  const char* expected =
      "ori $v0, $zero, 127\n"
      "dsll32 $v0, $v0, 11\n";
  DriverStr(expected, "LoadConst64IsUintW7S43");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW7S44) {
  __ LoadConst64(mips64::V0, 2234207627640832);

  const char* expected =
      "ori $v0, $zero, 127\n"
      "dsll32 $v0, $v0, 12\n";
  DriverStr(expected, "LoadConst64IsUintW7S44");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW7S45) {
  __ LoadConst64(mips64::V0, 4468415255281664);

  const char* expected =
      "ori $v0, $zero, 127\n"
      "dsll32 $v0, $v0, 13\n";
  DriverStr(expected, "LoadConst64IsUintW7S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW7S46) {
  __ LoadConst64(mips64::V0, 8936830510563328);

  const char* expected =
      "ori $v0, $zero, 127\n"
      "dsll32 $v0, $v0, 14\n";
  DriverStr(expected, "LoadConst64IsUintW7S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW7S47) {
  __ LoadConst64(mips64::V0, 17873661021126656);

  const char* expected =
      "ori $v0, $zero, 127\n"
      "dsll32 $v0, $v0, 15\n";
  DriverStr(expected, "LoadConst64IsUintW7S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW8S40) {
  __ LoadConst64(mips64::V0, 280375465082880);

  const char* expected =
      "ori $v0, $zero, 255\n"
      "dsll32 $v0, $v0, 8\n";
  DriverStr(expected, "LoadConst64IsUintW8S40");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW8S41) {
  __ LoadConst64(mips64::V0, 560750930165760);

  const char* expected =
      "ori $v0, $zero, 255\n"
      "dsll32 $v0, $v0, 9\n";
  DriverStr(expected, "LoadConst64IsUintW8S41");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW8S42) {
  __ LoadConst64(mips64::V0, 1121501860331520);

  const char* expected =
      "ori $v0, $zero, 255\n"
      "dsll32 $v0, $v0, 10\n";
  DriverStr(expected, "LoadConst64IsUintW8S42");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW8S43) {
  __ LoadConst64(mips64::V0, 2243003720663040);

  const char* expected =
      "ori $v0, $zero, 255\n"
      "dsll32 $v0, $v0, 11\n";
  DriverStr(expected, "LoadConst64IsUintW8S43");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW8S44) {
  __ LoadConst64(mips64::V0, 4486007441326080);

  const char* expected =
      "ori $v0, $zero, 255\n"
      "dsll32 $v0, $v0, 12\n";
  DriverStr(expected, "LoadConst64IsUintW8S44");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW8S45) {
  __ LoadConst64(mips64::V0, 8972014882652160);

  const char* expected =
      "ori $v0, $zero, 255\n"
      "dsll32 $v0, $v0, 13\n";
  DriverStr(expected, "LoadConst64IsUintW8S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW8S46) {
  __ LoadConst64(mips64::V0, 17944029765304320);

  const char* expected =
      "ori $v0, $zero, 255\n"
      "dsll32 $v0, $v0, 14\n";
  DriverStr(expected, "LoadConst64IsUintW8S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW8S47) {
  __ LoadConst64(mips64::V0, 35888059530608640);

  const char* expected =
      "ori $v0, $zero, 255\n"
      "dsll32 $v0, $v0, 15\n";
  DriverStr(expected, "LoadConst64IsUintW8S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW9S39) {
  __ LoadConst64(mips64::V0, 280925220896768);

  const char* expected =
      "ori $v0, $zero, 511\n"
      "dsll32 $v0, $v0, 7\n";
  DriverStr(expected, "LoadConst64IsUintW9S39");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW9S40) {
  __ LoadConst64(mips64::V0, 561850441793536);

  const char* expected =
      "ori $v0, $zero, 511\n"
      "dsll32 $v0, $v0, 8\n";
  DriverStr(expected, "LoadConst64IsUintW9S40");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW9S41) {
  __ LoadConst64(mips64::V0, 1123700883587072);

  const char* expected =
      "ori $v0, $zero, 511\n"
      "dsll32 $v0, $v0, 9\n";
  DriverStr(expected, "LoadConst64IsUintW9S41");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW9S42) {
  __ LoadConst64(mips64::V0, 2247401767174144);

  const char* expected =
      "ori $v0, $zero, 511\n"
      "dsll32 $v0, $v0, 10\n";
  DriverStr(expected, "LoadConst64IsUintW9S42");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW9S43) {
  __ LoadConst64(mips64::V0, 4494803534348288);

  const char* expected =
      "ori $v0, $zero, 511\n"
      "dsll32 $v0, $v0, 11\n";
  DriverStr(expected, "LoadConst64IsUintW9S43");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW9S44) {
  __ LoadConst64(mips64::V0, 8989607068696576);

  const char* expected =
      "ori $v0, $zero, 511\n"
      "dsll32 $v0, $v0, 12\n";
  DriverStr(expected, "LoadConst64IsUintW9S44");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW9S45) {
  __ LoadConst64(mips64::V0, 17979214137393152);

  const char* expected =
      "ori $v0, $zero, 511\n"
      "dsll32 $v0, $v0, 13\n";
  DriverStr(expected, "LoadConst64IsUintW9S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW9S46) {
  __ LoadConst64(mips64::V0, 35958428274786304);

  const char* expected =
      "ori $v0, $zero, 511\n"
      "dsll32 $v0, $v0, 14\n";
  DriverStr(expected, "LoadConst64IsUintW9S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW9S47) {
  __ LoadConst64(mips64::V0, 71916856549572608);

  const char* expected =
      "ori $v0, $zero, 511\n"
      "dsll32 $v0, $v0, 15\n";
  DriverStr(expected, "LoadConst64IsUintW9S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW10S38) {
  __ LoadConst64(mips64::V0, 281200098803712);

  const char* expected =
      "ori $v0, $zero, 1023\n"
      "dsll32 $v0, $v0, 6\n";
  DriverStr(expected, "LoadConst64IsUintW10S38");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW10S39) {
  __ LoadConst64(mips64::V0, 562400197607424);

  const char* expected =
      "ori $v0, $zero, 1023\n"
      "dsll32 $v0, $v0, 7\n";
  DriverStr(expected, "LoadConst64IsUintW10S39");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW10S40) {
  __ LoadConst64(mips64::V0, 1124800395214848);

  const char* expected =
      "ori $v0, $zero, 1023\n"
      "dsll32 $v0, $v0, 8\n";
  DriverStr(expected, "LoadConst64IsUintW10S40");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW10S41) {
  __ LoadConst64(mips64::V0, 2249600790429696);

  const char* expected =
      "ori $v0, $zero, 1023\n"
      "dsll32 $v0, $v0, 9\n";
  DriverStr(expected, "LoadConst64IsUintW10S41");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW10S42) {
  __ LoadConst64(mips64::V0, 4499201580859392);

  const char* expected =
      "ori $v0, $zero, 1023\n"
      "dsll32 $v0, $v0, 10\n";
  DriverStr(expected, "LoadConst64IsUintW10S42");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW10S43) {
  __ LoadConst64(mips64::V0, 8998403161718784);

  const char* expected =
      "ori $v0, $zero, 1023\n"
      "dsll32 $v0, $v0, 11\n";
  DriverStr(expected, "LoadConst64IsUintW10S43");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW10S44) {
  __ LoadConst64(mips64::V0, 17996806323437568);

  const char* expected =
      "ori $v0, $zero, 1023\n"
      "dsll32 $v0, $v0, 12\n";
  DriverStr(expected, "LoadConst64IsUintW10S44");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW10S45) {
  __ LoadConst64(mips64::V0, 35993612646875136);

  const char* expected =
      "ori $v0, $zero, 1023\n"
      "dsll32 $v0, $v0, 13\n";
  DriverStr(expected, "LoadConst64IsUintW10S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW10S46) {
  __ LoadConst64(mips64::V0, 71987225293750272);

  const char* expected =
      "ori $v0, $zero, 1023\n"
      "dsll32 $v0, $v0, 14\n";
  DriverStr(expected, "LoadConst64IsUintW10S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW10S47) {
  __ LoadConst64(mips64::V0, 143974450587500544);

  const char* expected =
      "ori $v0, $zero, 1023\n"
      "dsll32 $v0, $v0, 15\n";
  DriverStr(expected, "LoadConst64IsUintW10S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW11S37) {
  __ LoadConst64(mips64::V0, 281337537757184);

  const char* expected =
      "ori $v0, $zero, 2047\n"
      "dsll32 $v0, $v0, 5\n";
  DriverStr(expected, "LoadConst64IsUintW11S37");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW11S38) {
  __ LoadConst64(mips64::V0, 562675075514368);

  const char* expected =
      "ori $v0, $zero, 2047\n"
      "dsll32 $v0, $v0, 6\n";
  DriverStr(expected, "LoadConst64IsUintW11S38");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW11S39) {
  __ LoadConst64(mips64::V0, 1125350151028736);

  const char* expected =
      "ori $v0, $zero, 2047\n"
      "dsll32 $v0, $v0, 7\n";
  DriverStr(expected, "LoadConst64IsUintW11S39");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW11S40) {
  __ LoadConst64(mips64::V0, 2250700302057472);

  const char* expected =
      "ori $v0, $zero, 2047\n"
      "dsll32 $v0, $v0, 8\n";
  DriverStr(expected, "LoadConst64IsUintW11S40");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW11S41) {
  __ LoadConst64(mips64::V0, 4501400604114944);

  const char* expected =
      "ori $v0, $zero, 2047\n"
      "dsll32 $v0, $v0, 9\n";
  DriverStr(expected, "LoadConst64IsUintW11S41");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW11S42) {
  __ LoadConst64(mips64::V0, 9002801208229888);

  const char* expected =
      "ori $v0, $zero, 2047\n"
      "dsll32 $v0, $v0, 10\n";
  DriverStr(expected, "LoadConst64IsUintW11S42");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW11S43) {
  __ LoadConst64(mips64::V0, 18005602416459776);

  const char* expected =
      "ori $v0, $zero, 2047\n"
      "dsll32 $v0, $v0, 11\n";
  DriverStr(expected, "LoadConst64IsUintW11S43");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW11S44) {
  __ LoadConst64(mips64::V0, 36011204832919552);

  const char* expected =
      "ori $v0, $zero, 2047\n"
      "dsll32 $v0, $v0, 12\n";
  DriverStr(expected, "LoadConst64IsUintW11S44");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW11S45) {
  __ LoadConst64(mips64::V0, 72022409665839104);

  const char* expected =
      "ori $v0, $zero, 2047\n"
      "dsll32 $v0, $v0, 13\n";
  DriverStr(expected, "LoadConst64IsUintW11S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW11S46) {
  __ LoadConst64(mips64::V0, 144044819331678208);

  const char* expected =
      "ori $v0, $zero, 2047\n"
      "dsll32 $v0, $v0, 14\n";
  DriverStr(expected, "LoadConst64IsUintW11S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW11S47) {
  __ LoadConst64(mips64::V0, 288089638663356416);

  const char* expected =
      "ori $v0, $zero, 2047\n"
      "dsll32 $v0, $v0, 15\n";
  DriverStr(expected, "LoadConst64IsUintW11S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW12S36) {
  __ LoadConst64(mips64::V0, 281406257233920);

  const char* expected =
      "ori $v0, $zero, 4095\n"
      "dsll32 $v0, $v0, 4\n";
  DriverStr(expected, "LoadConst64IsUintW12S36");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW12S37) {
  __ LoadConst64(mips64::V0, 562812514467840);

  const char* expected =
      "ori $v0, $zero, 4095\n"
      "dsll32 $v0, $v0, 5\n";
  DriverStr(expected, "LoadConst64IsUintW12S37");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW12S38) {
  __ LoadConst64(mips64::V0, 1125625028935680);

  const char* expected =
      "ori $v0, $zero, 4095\n"
      "dsll32 $v0, $v0, 6\n";
  DriverStr(expected, "LoadConst64IsUintW12S38");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW12S39) {
  __ LoadConst64(mips64::V0, 2251250057871360);

  const char* expected =
      "ori $v0, $zero, 4095\n"
      "dsll32 $v0, $v0, 7\n";
  DriverStr(expected, "LoadConst64IsUintW12S39");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW12S40) {
  __ LoadConst64(mips64::V0, 4502500115742720);

  const char* expected =
      "ori $v0, $zero, 4095\n"
      "dsll32 $v0, $v0, 8\n";
  DriverStr(expected, "LoadConst64IsUintW12S40");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW12S41) {
  __ LoadConst64(mips64::V0, 9005000231485440);

  const char* expected =
      "ori $v0, $zero, 4095\n"
      "dsll32 $v0, $v0, 9\n";
  DriverStr(expected, "LoadConst64IsUintW12S41");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW12S42) {
  __ LoadConst64(mips64::V0, 18010000462970880);

  const char* expected =
      "ori $v0, $zero, 4095\n"
      "dsll32 $v0, $v0, 10\n";
  DriverStr(expected, "LoadConst64IsUintW12S42");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW12S43) {
  __ LoadConst64(mips64::V0, 36020000925941760);

  const char* expected =
      "ori $v0, $zero, 4095\n"
      "dsll32 $v0, $v0, 11\n";
  DriverStr(expected, "LoadConst64IsUintW12S43");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW12S44) {
  __ LoadConst64(mips64::V0, 72040001851883520);

  const char* expected =
      "ori $v0, $zero, 4095\n"
      "dsll32 $v0, $v0, 12\n";
  DriverStr(expected, "LoadConst64IsUintW12S44");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW12S45) {
  __ LoadConst64(mips64::V0, 144080003703767040);

  const char* expected =
      "ori $v0, $zero, 4095\n"
      "dsll32 $v0, $v0, 13\n";
  DriverStr(expected, "LoadConst64IsUintW12S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW12S46) {
  __ LoadConst64(mips64::V0, 288160007407534080);

  const char* expected =
      "ori $v0, $zero, 4095\n"
      "dsll32 $v0, $v0, 14\n";
  DriverStr(expected, "LoadConst64IsUintW12S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW12S47) {
  __ LoadConst64(mips64::V0, 576320014815068160);

  const char* expected =
      "ori $v0, $zero, 4095\n"
      "dsll32 $v0, $v0, 15\n";
  DriverStr(expected, "LoadConst64IsUintW12S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW13S35) {
  __ LoadConst64(mips64::V0, 281440616972288);

  const char* expected =
      "ori $v0, $zero, 8191\n"
      "dsll32 $v0, $v0, 3\n";
  DriverStr(expected, "LoadConst64IsUintW13S35");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW13S36) {
  __ LoadConst64(mips64::V0, 562881233944576);

  const char* expected =
      "ori $v0, $zero, 8191\n"
      "dsll32 $v0, $v0, 4\n";
  DriverStr(expected, "LoadConst64IsUintW13S36");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW13S37) {
  __ LoadConst64(mips64::V0, 1125762467889152);

  const char* expected =
      "ori $v0, $zero, 8191\n"
      "dsll32 $v0, $v0, 5\n";
  DriverStr(expected, "LoadConst64IsUintW13S37");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW13S38) {
  __ LoadConst64(mips64::V0, 2251524935778304);

  const char* expected =
      "ori $v0, $zero, 8191\n"
      "dsll32 $v0, $v0, 6\n";
  DriverStr(expected, "LoadConst64IsUintW13S38");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW13S39) {
  __ LoadConst64(mips64::V0, 4503049871556608);

  const char* expected =
      "ori $v0, $zero, 8191\n"
      "dsll32 $v0, $v0, 7\n";
  DriverStr(expected, "LoadConst64IsUintW13S39");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW13S40) {
  __ LoadConst64(mips64::V0, 9006099743113216);

  const char* expected =
      "ori $v0, $zero, 8191\n"
      "dsll32 $v0, $v0, 8\n";
  DriverStr(expected, "LoadConst64IsUintW13S40");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW13S41) {
  __ LoadConst64(mips64::V0, 18012199486226432);

  const char* expected =
      "ori $v0, $zero, 8191\n"
      "dsll32 $v0, $v0, 9\n";
  DriverStr(expected, "LoadConst64IsUintW13S41");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW13S42) {
  __ LoadConst64(mips64::V0, 36024398972452864);

  const char* expected =
      "ori $v0, $zero, 8191\n"
      "dsll32 $v0, $v0, 10\n";
  DriverStr(expected, "LoadConst64IsUintW13S42");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW13S43) {
  __ LoadConst64(mips64::V0, 72048797944905728);

  const char* expected =
      "ori $v0, $zero, 8191\n"
      "dsll32 $v0, $v0, 11\n";
  DriverStr(expected, "LoadConst64IsUintW13S43");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW13S44) {
  __ LoadConst64(mips64::V0, 144097595889811456);

  const char* expected =
      "ori $v0, $zero, 8191\n"
      "dsll32 $v0, $v0, 12\n";
  DriverStr(expected, "LoadConst64IsUintW13S44");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW13S45) {
  __ LoadConst64(mips64::V0, 288195191779622912);

  const char* expected =
      "ori $v0, $zero, 8191\n"
      "dsll32 $v0, $v0, 13\n";
  DriverStr(expected, "LoadConst64IsUintW13S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW13S46) {
  __ LoadConst64(mips64::V0, 576390383559245824);

  const char* expected =
      "ori $v0, $zero, 8191\n"
      "dsll32 $v0, $v0, 14\n";
  DriverStr(expected, "LoadConst64IsUintW13S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW13S47) {
  __ LoadConst64(mips64::V0, 1152780767118491648);

  const char* expected =
      "ori $v0, $zero, 8191\n"
      "dsll32 $v0, $v0, 15\n";
  DriverStr(expected, "LoadConst64IsUintW13S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW14S34) {
  __ LoadConst64(mips64::V0, 281457796841472);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll32 $v0, $v0, 2\n";
  DriverStr(expected, "LoadConst64IsUintW14S34");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW14S35) {
  __ LoadConst64(mips64::V0, 562915593682944);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll32 $v0, $v0, 3\n";
  DriverStr(expected, "LoadConst64IsUintW14S35");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW14S36) {
  __ LoadConst64(mips64::V0, 1125831187365888);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll32 $v0, $v0, 4\n";
  DriverStr(expected, "LoadConst64IsUintW14S36");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW14S37) {
  __ LoadConst64(mips64::V0, 2251662374731776);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll32 $v0, $v0, 5\n";
  DriverStr(expected, "LoadConst64IsUintW14S37");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW14S38) {
  __ LoadConst64(mips64::V0, 4503324749463552);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll32 $v0, $v0, 6\n";
  DriverStr(expected, "LoadConst64IsUintW14S38");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW14S39) {
  __ LoadConst64(mips64::V0, 9006649498927104);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll32 $v0, $v0, 7\n";
  DriverStr(expected, "LoadConst64IsUintW14S39");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW14S40) {
  __ LoadConst64(mips64::V0, 18013298997854208);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll32 $v0, $v0, 8\n";
  DriverStr(expected, "LoadConst64IsUintW14S40");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW14S41) {
  __ LoadConst64(mips64::V0, 36026597995708416);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll32 $v0, $v0, 9\n";
  DriverStr(expected, "LoadConst64IsUintW14S41");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW14S42) {
  __ LoadConst64(mips64::V0, 72053195991416832);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll32 $v0, $v0, 10\n";
  DriverStr(expected, "LoadConst64IsUintW14S42");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW14S43) {
  __ LoadConst64(mips64::V0, 144106391982833664);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll32 $v0, $v0, 11\n";
  DriverStr(expected, "LoadConst64IsUintW14S43");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW14S44) {
  __ LoadConst64(mips64::V0, 288212783965667328);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll32 $v0, $v0, 12\n";
  DriverStr(expected, "LoadConst64IsUintW14S44");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW14S45) {
  __ LoadConst64(mips64::V0, 576425567931334656);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll32 $v0, $v0, 13\n";
  DriverStr(expected, "LoadConst64IsUintW14S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW14S46) {
  __ LoadConst64(mips64::V0, 1152851135862669312);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll32 $v0, $v0, 14\n";
  DriverStr(expected, "LoadConst64IsUintW14S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW14S47) {
  __ LoadConst64(mips64::V0, 2305702271725338624);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll32 $v0, $v0, 15\n";
  DriverStr(expected, "LoadConst64IsUintW14S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW15S33) {
  __ LoadConst64(mips64::V0, 281466386776064);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll32 $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64IsUintW15S33");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW15S34) {
  __ LoadConst64(mips64::V0, 562932773552128);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll32 $v0, $v0, 2\n";
  DriverStr(expected, "LoadConst64IsUintW15S34");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW15S35) {
  __ LoadConst64(mips64::V0, 1125865547104256);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll32 $v0, $v0, 3\n";
  DriverStr(expected, "LoadConst64IsUintW15S35");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW15S36) {
  __ LoadConst64(mips64::V0, 2251731094208512);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll32 $v0, $v0, 4\n";
  DriverStr(expected, "LoadConst64IsUintW15S36");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW15S37) {
  __ LoadConst64(mips64::V0, 4503462188417024);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll32 $v0, $v0, 5\n";
  DriverStr(expected, "LoadConst64IsUintW15S37");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW15S38) {
  __ LoadConst64(mips64::V0, 9006924376834048);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll32 $v0, $v0, 6\n";
  DriverStr(expected, "LoadConst64IsUintW15S38");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW15S39) {
  __ LoadConst64(mips64::V0, 18013848753668096);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll32 $v0, $v0, 7\n";
  DriverStr(expected, "LoadConst64IsUintW15S39");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW15S40) {
  __ LoadConst64(mips64::V0, 36027697507336192);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll32 $v0, $v0, 8\n";
  DriverStr(expected, "LoadConst64IsUintW15S40");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW15S41) {
  __ LoadConst64(mips64::V0, 72055395014672384);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll32 $v0, $v0, 9\n";
  DriverStr(expected, "LoadConst64IsUintW15S41");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW15S42) {
  __ LoadConst64(mips64::V0, 144110790029344768);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll32 $v0, $v0, 10\n";
  DriverStr(expected, "LoadConst64IsUintW15S42");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW15S43) {
  __ LoadConst64(mips64::V0, 288221580058689536);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll32 $v0, $v0, 11\n";
  DriverStr(expected, "LoadConst64IsUintW15S43");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW15S44) {
  __ LoadConst64(mips64::V0, 576443160117379072);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll32 $v0, $v0, 12\n";
  DriverStr(expected, "LoadConst64IsUintW15S44");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW15S45) {
  __ LoadConst64(mips64::V0, 1152886320234758144);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll32 $v0, $v0, 13\n";
  DriverStr(expected, "LoadConst64IsUintW15S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW15S46) {
  __ LoadConst64(mips64::V0, 2305772640469516288);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll32 $v0, $v0, 14\n";
  DriverStr(expected, "LoadConst64IsUintW15S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW15S47) {
  __ LoadConst64(mips64::V0, 4611545280939032576);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll32 $v0, $v0, 15\n";
  DriverStr(expected, "LoadConst64IsUintW15S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW16S31) {
  __ LoadConst64(mips64::V0, 140735340871680);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll $v0, $v0, 31\n";
  DriverStr(expected, "LoadConst64IsUintW16S31");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW16S32) {
  __ LoadConst64(mips64::V0, 281470681743360);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll32 $v0, $v0, 0\n";
  DriverStr(expected, "LoadConst64IsUintW16S32");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW16S33) {
  __ LoadConst64(mips64::V0, 562941363486720);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll32 $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64IsUintW16S33");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW16S34) {
  __ LoadConst64(mips64::V0, 1125882726973440);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll32 $v0, $v0, 2\n";
  DriverStr(expected, "LoadConst64IsUintW16S34");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW16S35) {
  __ LoadConst64(mips64::V0, 2251765453946880);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll32 $v0, $v0, 3\n";
  DriverStr(expected, "LoadConst64IsUintW16S35");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW16S36) {
  __ LoadConst64(mips64::V0, 4503530907893760);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll32 $v0, $v0, 4\n";
  DriverStr(expected, "LoadConst64IsUintW16S36");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW16S37) {
  __ LoadConst64(mips64::V0, 9007061815787520);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll32 $v0, $v0, 5\n";
  DriverStr(expected, "LoadConst64IsUintW16S37");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW16S38) {
  __ LoadConst64(mips64::V0, 18014123631575040);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll32 $v0, $v0, 6\n";
  DriverStr(expected, "LoadConst64IsUintW16S38");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW16S39) {
  __ LoadConst64(mips64::V0, 36028247263150080);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll32 $v0, $v0, 7\n";
  DriverStr(expected, "LoadConst64IsUintW16S39");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW16S40) {
  __ LoadConst64(mips64::V0, 72056494526300160);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll32 $v0, $v0, 8\n";
  DriverStr(expected, "LoadConst64IsUintW16S40");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW16S41) {
  __ LoadConst64(mips64::V0, 144112989052600320);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll32 $v0, $v0, 9\n";
  DriverStr(expected, "LoadConst64IsUintW16S41");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW16S42) {
  __ LoadConst64(mips64::V0, 288225978105200640);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll32 $v0, $v0, 10\n";
  DriverStr(expected, "LoadConst64IsUintW16S42");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW16S43) {
  __ LoadConst64(mips64::V0, 576451956210401280);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll32 $v0, $v0, 11\n";
  DriverStr(expected, "LoadConst64IsUintW16S43");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW16S44) {
  __ LoadConst64(mips64::V0, 1152903912420802560);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll32 $v0, $v0, 12\n";
  DriverStr(expected, "LoadConst64IsUintW16S44");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW16S45) {
  __ LoadConst64(mips64::V0, 2305807824841605120);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll32 $v0, $v0, 13\n";
  DriverStr(expected, "LoadConst64IsUintW16S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW16S46) {
  __ LoadConst64(mips64::V0, 4611615649683210240);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll32 $v0, $v0, 14\n";
  DriverStr(expected, "LoadConst64IsUintW16S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsUintW16S47) {
  __ LoadConst64(mips64::V0, 9223231299366420480);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll32 $v0, $v0, 15\n";
  DriverStr(expected, "LoadConst64IsUintW16S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW2S46b) {
  __ LoadConst64(mips64::V0, -211106232532992);

  const char* expected =
      "daddiu $v0, $zero, -3\n"
      "dsll32 $v0, $v0, 14\n";
  DriverStr(expected, "LoadConst64IsIntW2S46b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW2S47b) {
  __ LoadConst64(mips64::V0, -422212465065984);

  const char* expected =
      "daddiu $v0, $zero, -3\n"
      "dsll32 $v0, $v0, 15\n";
  DriverStr(expected, "LoadConst64IsIntW2S47b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW3S45b) {
  __ LoadConst64(mips64::V0, -246290604621824);

  const char* expected =
      "daddiu $v0, $zero, -7\n"
      "dsll32 $v0, $v0, 13\n";
  DriverStr(expected, "LoadConst64IsIntW3S45b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW3S46b) {
  __ LoadConst64(mips64::V0, -492581209243648);

  const char* expected =
      "daddiu $v0, $zero, -7\n"
      "dsll32 $v0, $v0, 14\n";
  DriverStr(expected, "LoadConst64IsIntW3S46b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW3S47b) {
  __ LoadConst64(mips64::V0, -985162418487296);

  const char* expected =
      "daddiu $v0, $zero, -7\n"
      "dsll32 $v0, $v0, 15\n";
  DriverStr(expected, "LoadConst64IsIntW3S47b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW4S44b) {
  __ LoadConst64(mips64::V0, -263882790666240);

  const char* expected =
      "daddiu $v0, $zero, -15\n"
      "dsll32 $v0, $v0, 12\n";
  DriverStr(expected, "LoadConst64IsIntW4S44b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW4S45b) {
  __ LoadConst64(mips64::V0, -527765581332480);

  const char* expected =
      "daddiu $v0, $zero, -15\n"
      "dsll32 $v0, $v0, 13\n";
  DriverStr(expected, "LoadConst64IsIntW4S45b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW4S46b) {
  __ LoadConst64(mips64::V0, -1055531162664960);

  const char* expected =
      "daddiu $v0, $zero, -15\n"
      "dsll32 $v0, $v0, 14\n";
  DriverStr(expected, "LoadConst64IsIntW4S46b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW4S47b) {
  __ LoadConst64(mips64::V0, -2111062325329920);

  const char* expected =
      "daddiu $v0, $zero, -15\n"
      "dsll32 $v0, $v0, 15\n";
  DriverStr(expected, "LoadConst64IsIntW4S47b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW5S43b) {
  __ LoadConst64(mips64::V0, -272678883688448);

  const char* expected =
      "daddiu $v0, $zero, -31\n"
      "dsll32 $v0, $v0, 11\n";
  DriverStr(expected, "LoadConst64IsIntW5S43b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW5S44b) {
  __ LoadConst64(mips64::V0, -545357767376896);

  const char* expected =
      "daddiu $v0, $zero, -31\n"
      "dsll32 $v0, $v0, 12\n";
  DriverStr(expected, "LoadConst64IsIntW5S44b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW5S45b) {
  __ LoadConst64(mips64::V0, -1090715534753792);

  const char* expected =
      "daddiu $v0, $zero, -31\n"
      "dsll32 $v0, $v0, 13\n";
  DriverStr(expected, "LoadConst64IsIntW5S45b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW5S46b) {
  __ LoadConst64(mips64::V0, -2181431069507584);

  const char* expected =
      "daddiu $v0, $zero, -31\n"
      "dsll32 $v0, $v0, 14\n";
  DriverStr(expected, "LoadConst64IsIntW5S46b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW5S47b) {
  __ LoadConst64(mips64::V0, -4362862139015168);

  const char* expected =
      "daddiu $v0, $zero, -31\n"
      "dsll32 $v0, $v0, 15\n";
  DriverStr(expected, "LoadConst64IsIntW5S47b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW6S42b) {
  __ LoadConst64(mips64::V0, -277076930199552);

  const char* expected =
      "daddiu $v0, $zero, -63\n"
      "dsll32 $v0, $v0, 10\n";
  DriverStr(expected, "LoadConst64IsIntW6S42b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW6S43b) {
  __ LoadConst64(mips64::V0, -554153860399104);

  const char* expected =
      "daddiu $v0, $zero, -63\n"
      "dsll32 $v0, $v0, 11\n";
  DriverStr(expected, "LoadConst64IsIntW6S43b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW6S44b) {
  __ LoadConst64(mips64::V0, -1108307720798208);

  const char* expected =
      "daddiu $v0, $zero, -63\n"
      "dsll32 $v0, $v0, 12\n";
  DriverStr(expected, "LoadConst64IsIntW6S44b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW6S45b) {
  __ LoadConst64(mips64::V0, -2216615441596416);

  const char* expected =
      "daddiu $v0, $zero, -63\n"
      "dsll32 $v0, $v0, 13\n";
  DriverStr(expected, "LoadConst64IsIntW6S45b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW6S46b) {
  __ LoadConst64(mips64::V0, -4433230883192832);

  const char* expected =
      "daddiu $v0, $zero, -63\n"
      "dsll32 $v0, $v0, 14\n";
  DriverStr(expected, "LoadConst64IsIntW6S46b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW6S47b) {
  __ LoadConst64(mips64::V0, -8866461766385664);

  const char* expected =
      "daddiu $v0, $zero, -63\n"
      "dsll32 $v0, $v0, 15\n";
  DriverStr(expected, "LoadConst64IsIntW6S47b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW7S41b) {
  __ LoadConst64(mips64::V0, -279275953455104);

  const char* expected =
      "daddiu $v0, $zero, -127\n"
      "dsll32 $v0, $v0, 9\n";
  DriverStr(expected, "LoadConst64IsIntW7S41b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW7S42b) {
  __ LoadConst64(mips64::V0, -558551906910208);

  const char* expected =
      "daddiu $v0, $zero, -127\n"
      "dsll32 $v0, $v0, 10\n";
  DriverStr(expected, "LoadConst64IsIntW7S42b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW7S43b) {
  __ LoadConst64(mips64::V0, -1117103813820416);

  const char* expected =
      "daddiu $v0, $zero, -127\n"
      "dsll32 $v0, $v0, 11\n";
  DriverStr(expected, "LoadConst64IsIntW7S43b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW7S44b) {
  __ LoadConst64(mips64::V0, -2234207627640832);

  const char* expected =
      "daddiu $v0, $zero, -127\n"
      "dsll32 $v0, $v0, 12\n";
  DriverStr(expected, "LoadConst64IsIntW7S44b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW7S45b) {
  __ LoadConst64(mips64::V0, -4468415255281664);

  const char* expected =
      "daddiu $v0, $zero, -127\n"
      "dsll32 $v0, $v0, 13\n";
  DriverStr(expected, "LoadConst64IsIntW7S45b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW7S46b) {
  __ LoadConst64(mips64::V0, -8936830510563328);

  const char* expected =
      "daddiu $v0, $zero, -127\n"
      "dsll32 $v0, $v0, 14\n";
  DriverStr(expected, "LoadConst64IsIntW7S46b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW7S47b) {
  __ LoadConst64(mips64::V0, -17873661021126656);

  const char* expected =
      "daddiu $v0, $zero, -127\n"
      "dsll32 $v0, $v0, 15\n";
  DriverStr(expected, "LoadConst64IsIntW7S47b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW8S40b) {
  __ LoadConst64(mips64::V0, -280375465082880);

  const char* expected =
      "daddiu $v0, $zero, -255\n"
      "dsll32 $v0, $v0, 8\n";
  DriverStr(expected, "LoadConst64IsIntW8S40b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW8S41b) {
  __ LoadConst64(mips64::V0, -560750930165760);

  const char* expected =
      "daddiu $v0, $zero, -255\n"
      "dsll32 $v0, $v0, 9\n";
  DriverStr(expected, "LoadConst64IsIntW8S41b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW8S42b) {
  __ LoadConst64(mips64::V0, -1121501860331520);

  const char* expected =
      "daddiu $v0, $zero, -255\n"
      "dsll32 $v0, $v0, 10\n";
  DriverStr(expected, "LoadConst64IsIntW8S42b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW8S43b) {
  __ LoadConst64(mips64::V0, -2243003720663040);

  const char* expected =
      "daddiu $v0, $zero, -255\n"
      "dsll32 $v0, $v0, 11\n";
  DriverStr(expected, "LoadConst64IsIntW8S43b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW8S44b) {
  __ LoadConst64(mips64::V0, -4486007441326080);

  const char* expected =
      "daddiu $v0, $zero, -255\n"
      "dsll32 $v0, $v0, 12\n";
  DriverStr(expected, "LoadConst64IsIntW8S44b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW8S45b) {
  __ LoadConst64(mips64::V0, -8972014882652160);

  const char* expected =
      "daddiu $v0, $zero, -255\n"
      "dsll32 $v0, $v0, 13\n";
  DriverStr(expected, "LoadConst64IsIntW8S45b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW8S46b) {
  __ LoadConst64(mips64::V0, -17944029765304320);

  const char* expected =
      "daddiu $v0, $zero, -255\n"
      "dsll32 $v0, $v0, 14\n";
  DriverStr(expected, "LoadConst64IsIntW8S46b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW8S47b) {
  __ LoadConst64(mips64::V0, -35888059530608640);

  const char* expected =
      "daddiu $v0, $zero, -255\n"
      "dsll32 $v0, $v0, 15\n";
  DriverStr(expected, "LoadConst64IsIntW8S47b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW9S39b) {
  __ LoadConst64(mips64::V0, -280925220896768);

  const char* expected =
      "daddiu $v0, $zero, -511\n"
      "dsll32 $v0, $v0, 7\n";
  DriverStr(expected, "LoadConst64IsIntW9S39b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW9S40b) {
  __ LoadConst64(mips64::V0, -561850441793536);

  const char* expected =
      "daddiu $v0, $zero, -511\n"
      "dsll32 $v0, $v0, 8\n";
  DriverStr(expected, "LoadConst64IsIntW9S40b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW9S41b) {
  __ LoadConst64(mips64::V0, -1123700883587072);

  const char* expected =
      "daddiu $v0, $zero, -511\n"
      "dsll32 $v0, $v0, 9\n";
  DriverStr(expected, "LoadConst64IsIntW9S41b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW9S42b) {
  __ LoadConst64(mips64::V0, -2247401767174144);

  const char* expected =
      "daddiu $v0, $zero, -511\n"
      "dsll32 $v0, $v0, 10\n";
  DriverStr(expected, "LoadConst64IsIntW9S42b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW9S43b) {
  __ LoadConst64(mips64::V0, -4494803534348288);

  const char* expected =
      "daddiu $v0, $zero, -511\n"
      "dsll32 $v0, $v0, 11\n";
  DriverStr(expected, "LoadConst64IsIntW9S43b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW9S44b) {
  __ LoadConst64(mips64::V0, -8989607068696576);

  const char* expected =
      "daddiu $v0, $zero, -511\n"
      "dsll32 $v0, $v0, 12\n";
  DriverStr(expected, "LoadConst64IsIntW9S44b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW9S45b) {
  __ LoadConst64(mips64::V0, -17979214137393152);

  const char* expected =
      "daddiu $v0, $zero, -511\n"
      "dsll32 $v0, $v0, 13\n";
  DriverStr(expected, "LoadConst64IsIntW9S45b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW9S46b) {
  __ LoadConst64(mips64::V0, -35958428274786304);

  const char* expected =
      "daddiu $v0, $zero, -511\n"
      "dsll32 $v0, $v0, 14\n";
  DriverStr(expected, "LoadConst64IsIntW9S46b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW9S47b) {
  __ LoadConst64(mips64::V0, -71916856549572608);

  const char* expected =
      "daddiu $v0, $zero, -511\n"
      "dsll32 $v0, $v0, 15\n";
  DriverStr(expected, "LoadConst64IsIntW9S47b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW10S38b) {
  __ LoadConst64(mips64::V0, -281200098803712);

  const char* expected =
      "daddiu $v0, $zero, -1023\n"
      "dsll32 $v0, $v0, 6\n";
  DriverStr(expected, "LoadConst64IsIntW10S38b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW10S39b) {
  __ LoadConst64(mips64::V0, -562400197607424);

  const char* expected =
      "daddiu $v0, $zero, -1023\n"
      "dsll32 $v0, $v0, 7\n";
  DriverStr(expected, "LoadConst64IsIntW10S39b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW10S40b) {
  __ LoadConst64(mips64::V0, -1124800395214848);

  const char* expected =
      "daddiu $v0, $zero, -1023\n"
      "dsll32 $v0, $v0, 8\n";
  DriverStr(expected, "LoadConst64IsIntW10S40b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW10S41b) {
  __ LoadConst64(mips64::V0, -2249600790429696);

  const char* expected =
      "daddiu $v0, $zero, -1023\n"
      "dsll32 $v0, $v0, 9\n";
  DriverStr(expected, "LoadConst64IsIntW10S41b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW10S42b) {
  __ LoadConst64(mips64::V0, -4499201580859392);

  const char* expected =
      "daddiu $v0, $zero, -1023\n"
      "dsll32 $v0, $v0, 10\n";
  DriverStr(expected, "LoadConst64IsIntW10S42b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW10S43b) {
  __ LoadConst64(mips64::V0, -8998403161718784);

  const char* expected =
      "daddiu $v0, $zero, -1023\n"
      "dsll32 $v0, $v0, 11\n";
  DriverStr(expected, "LoadConst64IsIntW10S43b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW10S44b) {
  __ LoadConst64(mips64::V0, -17996806323437568);

  const char* expected =
      "daddiu $v0, $zero, -1023\n"
      "dsll32 $v0, $v0, 12\n";
  DriverStr(expected, "LoadConst64IsIntW10S44b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW10S45b) {
  __ LoadConst64(mips64::V0, -35993612646875136);

  const char* expected =
      "daddiu $v0, $zero, -1023\n"
      "dsll32 $v0, $v0, 13\n";
  DriverStr(expected, "LoadConst64IsIntW10S45b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW10S46b) {
  __ LoadConst64(mips64::V0, -71987225293750272);

  const char* expected =
      "daddiu $v0, $zero, -1023\n"
      "dsll32 $v0, $v0, 14\n";
  DriverStr(expected, "LoadConst64IsIntW10S46b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW10S47b) {
  __ LoadConst64(mips64::V0, -143974450587500544);

  const char* expected =
      "daddiu $v0, $zero, -1023\n"
      "dsll32 $v0, $v0, 15\n";
  DriverStr(expected, "LoadConst64IsIntW10S47b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW11S37b) {
  __ LoadConst64(mips64::V0, -281337537757184);

  const char* expected =
      "daddiu $v0, $zero, -2047\n"
      "dsll32 $v0, $v0, 5\n";
  DriverStr(expected, "LoadConst64IsIntW11S37b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW11S38b) {
  __ LoadConst64(mips64::V0, -562675075514368);

  const char* expected =
      "daddiu $v0, $zero, -2047\n"
      "dsll32 $v0, $v0, 6\n";
  DriverStr(expected, "LoadConst64IsIntW11S38b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW11S39b) {
  __ LoadConst64(mips64::V0, -1125350151028736);

  const char* expected =
      "daddiu $v0, $zero, -2047\n"
      "dsll32 $v0, $v0, 7\n";
  DriverStr(expected, "LoadConst64IsIntW11S39b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW11S40b) {
  __ LoadConst64(mips64::V0, -2250700302057472);

  const char* expected =
      "daddiu $v0, $zero, -2047\n"
      "dsll32 $v0, $v0, 8\n";
  DriverStr(expected, "LoadConst64IsIntW11S40b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW11S41b) {
  __ LoadConst64(mips64::V0, -4501400604114944);

  const char* expected =
      "daddiu $v0, $zero, -2047\n"
      "dsll32 $v0, $v0, 9\n";
  DriverStr(expected, "LoadConst64IsIntW11S41b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW11S42b) {
  __ LoadConst64(mips64::V0, -9002801208229888);

  const char* expected =
      "daddiu $v0, $zero, -2047\n"
      "dsll32 $v0, $v0, 10\n";
  DriverStr(expected, "LoadConst64IsIntW11S42b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW11S43b) {
  __ LoadConst64(mips64::V0, -18005602416459776);

  const char* expected =
      "daddiu $v0, $zero, -2047\n"
      "dsll32 $v0, $v0, 11\n";
  DriverStr(expected, "LoadConst64IsIntW11S43b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW11S44b) {
  __ LoadConst64(mips64::V0, -36011204832919552);

  const char* expected =
      "daddiu $v0, $zero, -2047\n"
      "dsll32 $v0, $v0, 12\n";
  DriverStr(expected, "LoadConst64IsIntW11S44b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW11S45b) {
  __ LoadConst64(mips64::V0, -72022409665839104);

  const char* expected =
      "daddiu $v0, $zero, -2047\n"
      "dsll32 $v0, $v0, 13\n";
  DriverStr(expected, "LoadConst64IsIntW11S45b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW11S46b) {
  __ LoadConst64(mips64::V0, -144044819331678208);

  const char* expected =
      "daddiu $v0, $zero, -2047\n"
      "dsll32 $v0, $v0, 14\n";
  DriverStr(expected, "LoadConst64IsIntW11S46b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW11S47b) {
  __ LoadConst64(mips64::V0, -288089638663356416);

  const char* expected =
      "daddiu $v0, $zero, -2047\n"
      "dsll32 $v0, $v0, 15\n";
  DriverStr(expected, "LoadConst64IsIntW11S47b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW12S36b) {
  __ LoadConst64(mips64::V0, -281406257233920);

  const char* expected =
      "daddiu $v0, $zero, -4095\n"
      "dsll32 $v0, $v0, 4\n";
  DriverStr(expected, "LoadConst64IsIntW12S36b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW12S37b) {
  __ LoadConst64(mips64::V0, -562812514467840);

  const char* expected =
      "daddiu $v0, $zero, -4095\n"
      "dsll32 $v0, $v0, 5\n";
  DriverStr(expected, "LoadConst64IsIntW12S37b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW12S38b) {
  __ LoadConst64(mips64::V0, -1125625028935680);

  const char* expected =
      "daddiu $v0, $zero, -4095\n"
      "dsll32 $v0, $v0, 6\n";
  DriverStr(expected, "LoadConst64IsIntW12S38b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW12S39b) {
  __ LoadConst64(mips64::V0, -2251250057871360);

  const char* expected =
      "daddiu $v0, $zero, -4095\n"
      "dsll32 $v0, $v0, 7\n";
  DriverStr(expected, "LoadConst64IsIntW12S39b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW12S40b) {
  __ LoadConst64(mips64::V0, -4502500115742720);

  const char* expected =
      "daddiu $v0, $zero, -4095\n"
      "dsll32 $v0, $v0, 8\n";
  DriverStr(expected, "LoadConst64IsIntW12S40b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW12S41b) {
  __ LoadConst64(mips64::V0, -9005000231485440);

  const char* expected =
      "daddiu $v0, $zero, -4095\n"
      "dsll32 $v0, $v0, 9\n";
  DriverStr(expected, "LoadConst64IsIntW12S41b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW12S42b) {
  __ LoadConst64(mips64::V0, -18010000462970880);

  const char* expected =
      "daddiu $v0, $zero, -4095\n"
      "dsll32 $v0, $v0, 10\n";
  DriverStr(expected, "LoadConst64IsIntW12S42b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW12S43b) {
  __ LoadConst64(mips64::V0, -36020000925941760);

  const char* expected =
      "daddiu $v0, $zero, -4095\n"
      "dsll32 $v0, $v0, 11\n";
  DriverStr(expected, "LoadConst64IsIntW12S43b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW12S44b) {
  __ LoadConst64(mips64::V0, -72040001851883520);

  const char* expected =
      "daddiu $v0, $zero, -4095\n"
      "dsll32 $v0, $v0, 12\n";
  DriverStr(expected, "LoadConst64IsIntW12S44b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW12S45b) {
  __ LoadConst64(mips64::V0, -144080003703767040);

  const char* expected =
      "daddiu $v0, $zero, -4095\n"
      "dsll32 $v0, $v0, 13\n";
  DriverStr(expected, "LoadConst64IsIntW12S45b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW12S46b) {
  __ LoadConst64(mips64::V0, -288160007407534080);

  const char* expected =
      "daddiu $v0, $zero, -4095\n"
      "dsll32 $v0, $v0, 14\n";
  DriverStr(expected, "LoadConst64IsIntW12S46b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW12S47b) {
  __ LoadConst64(mips64::V0, -576320014815068160);

  const char* expected =
      "daddiu $v0, $zero, -4095\n"
      "dsll32 $v0, $v0, 15\n";
  DriverStr(expected, "LoadConst64IsIntW12S47b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW13S35b) {
  __ LoadConst64(mips64::V0, -281440616972288);

  const char* expected =
      "daddiu $v0, $zero, -8191\n"
      "dsll32 $v0, $v0, 3\n";
  DriverStr(expected, "LoadConst64IsIntW13S35b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW13S36b) {
  __ LoadConst64(mips64::V0, -562881233944576);

  const char* expected =
      "daddiu $v0, $zero, -8191\n"
      "dsll32 $v0, $v0, 4\n";
  DriverStr(expected, "LoadConst64IsIntW13S36b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW13S37b) {
  __ LoadConst64(mips64::V0, -1125762467889152);

  const char* expected =
      "daddiu $v0, $zero, -8191\n"
      "dsll32 $v0, $v0, 5\n";
  DriverStr(expected, "LoadConst64IsIntW13S37b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW13S38b) {
  __ LoadConst64(mips64::V0, -2251524935778304);

  const char* expected =
      "daddiu $v0, $zero, -8191\n"
      "dsll32 $v0, $v0, 6\n";
  DriverStr(expected, "LoadConst64IsIntW13S38b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW13S39b) {
  __ LoadConst64(mips64::V0, -4503049871556608);

  const char* expected =
      "daddiu $v0, $zero, -8191\n"
      "dsll32 $v0, $v0, 7\n";
  DriverStr(expected, "LoadConst64IsIntW13S39b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW13S40b) {
  __ LoadConst64(mips64::V0, -9006099743113216);

  const char* expected =
      "daddiu $v0, $zero, -8191\n"
      "dsll32 $v0, $v0, 8\n";
  DriverStr(expected, "LoadConst64IsIntW13S40b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW13S41b) {
  __ LoadConst64(mips64::V0, -18012199486226432);

  const char* expected =
      "daddiu $v0, $zero, -8191\n"
      "dsll32 $v0, $v0, 9\n";
  DriverStr(expected, "LoadConst64IsIntW13S41b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW13S42b) {
  __ LoadConst64(mips64::V0, -36024398972452864);

  const char* expected =
      "daddiu $v0, $zero, -8191\n"
      "dsll32 $v0, $v0, 10\n";
  DriverStr(expected, "LoadConst64IsIntW13S42b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW13S43b) {
  __ LoadConst64(mips64::V0, -72048797944905728);

  const char* expected =
      "daddiu $v0, $zero, -8191\n"
      "dsll32 $v0, $v0, 11\n";
  DriverStr(expected, "LoadConst64IsIntW13S43b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW13S44b) {
  __ LoadConst64(mips64::V0, -144097595889811456);

  const char* expected =
      "daddiu $v0, $zero, -8191\n"
      "dsll32 $v0, $v0, 12\n";
  DriverStr(expected, "LoadConst64IsIntW13S44b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW13S45b) {
  __ LoadConst64(mips64::V0, -288195191779622912);

  const char* expected =
      "daddiu $v0, $zero, -8191\n"
      "dsll32 $v0, $v0, 13\n";
  DriverStr(expected, "LoadConst64IsIntW13S45b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW13S46b) {
  __ LoadConst64(mips64::V0, -576390383559245824);

  const char* expected =
      "daddiu $v0, $zero, -8191\n"
      "dsll32 $v0, $v0, 14\n";
  DriverStr(expected, "LoadConst64IsIntW13S46b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW13S47b) {
  __ LoadConst64(mips64::V0, -1152780767118491648);

  const char* expected =
      "daddiu $v0, $zero, -8191\n"
      "dsll32 $v0, $v0, 15\n";
  DriverStr(expected, "LoadConst64IsIntW13S47b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW14S34b) {
  __ LoadConst64(mips64::V0, -281457796841472);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll32 $v0, $v0, 2\n";
  DriverStr(expected, "LoadConst64IsIntW14S34b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW14S35b) {
  __ LoadConst64(mips64::V0, -562915593682944);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll32 $v0, $v0, 3\n";
  DriverStr(expected, "LoadConst64IsIntW14S35b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW14S36b) {
  __ LoadConst64(mips64::V0, -1125831187365888);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll32 $v0, $v0, 4\n";
  DriverStr(expected, "LoadConst64IsIntW14S36b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW14S37b) {
  __ LoadConst64(mips64::V0, -2251662374731776);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll32 $v0, $v0, 5\n";
  DriverStr(expected, "LoadConst64IsIntW14S37b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW14S38b) {
  __ LoadConst64(mips64::V0, -4503324749463552);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll32 $v0, $v0, 6\n";
  DriverStr(expected, "LoadConst64IsIntW14S38b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW14S39b) {
  __ LoadConst64(mips64::V0, -9006649498927104);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll32 $v0, $v0, 7\n";
  DriverStr(expected, "LoadConst64IsIntW14S39b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW14S40b) {
  __ LoadConst64(mips64::V0, -18013298997854208);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll32 $v0, $v0, 8\n";
  DriverStr(expected, "LoadConst64IsIntW14S40b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW14S41b) {
  __ LoadConst64(mips64::V0, -36026597995708416);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll32 $v0, $v0, 9\n";
  DriverStr(expected, "LoadConst64IsIntW14S41b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW14S42b) {
  __ LoadConst64(mips64::V0, -72053195991416832);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll32 $v0, $v0, 10\n";
  DriverStr(expected, "LoadConst64IsIntW14S42b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW14S43b) {
  __ LoadConst64(mips64::V0, -144106391982833664);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll32 $v0, $v0, 11\n";
  DriverStr(expected, "LoadConst64IsIntW14S43b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW14S44b) {
  __ LoadConst64(mips64::V0, -288212783965667328);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll32 $v0, $v0, 12\n";
  DriverStr(expected, "LoadConst64IsIntW14S44b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW14S45b) {
  __ LoadConst64(mips64::V0, -576425567931334656);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll32 $v0, $v0, 13\n";
  DriverStr(expected, "LoadConst64IsIntW14S45b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW14S46b) {
  __ LoadConst64(mips64::V0, -1152851135862669312);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll32 $v0, $v0, 14\n";
  DriverStr(expected, "LoadConst64IsIntW14S46b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW14S47b) {
  __ LoadConst64(mips64::V0, -2305702271725338624);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll32 $v0, $v0, 15\n";
  DriverStr(expected, "LoadConst64IsIntW14S47b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW15S33b) {
  __ LoadConst64(mips64::V0, -281466386776064);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll32 $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64IsIntW15S33b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW15S34b) {
  __ LoadConst64(mips64::V0, -562932773552128);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll32 $v0, $v0, 2\n";
  DriverStr(expected, "LoadConst64IsIntW15S34b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW15S35b) {
  __ LoadConst64(mips64::V0, -1125865547104256);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll32 $v0, $v0, 3\n";
  DriverStr(expected, "LoadConst64IsIntW15S35b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW15S36b) {
  __ LoadConst64(mips64::V0, -2251731094208512);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll32 $v0, $v0, 4\n";
  DriverStr(expected, "LoadConst64IsIntW15S36b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW15S37b) {
  __ LoadConst64(mips64::V0, -4503462188417024);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll32 $v0, $v0, 5\n";
  DriverStr(expected, "LoadConst64IsIntW15S37b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW15S38b) {
  __ LoadConst64(mips64::V0, -9006924376834048);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll32 $v0, $v0, 6\n";
  DriverStr(expected, "LoadConst64IsIntW15S38b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW15S39b) {
  __ LoadConst64(mips64::V0, -18013848753668096);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll32 $v0, $v0, 7\n";
  DriverStr(expected, "LoadConst64IsIntW15S39b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW15S40b) {
  __ LoadConst64(mips64::V0, -36027697507336192);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll32 $v0, $v0, 8\n";
  DriverStr(expected, "LoadConst64IsIntW15S40b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW15S41b) {
  __ LoadConst64(mips64::V0, -72055395014672384);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll32 $v0, $v0, 9\n";
  DriverStr(expected, "LoadConst64IsIntW15S41b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW15S42b) {
  __ LoadConst64(mips64::V0, -144110790029344768);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll32 $v0, $v0, 10\n";
  DriverStr(expected, "LoadConst64IsIntW15S42b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW15S43b) {
  __ LoadConst64(mips64::V0, -288221580058689536);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll32 $v0, $v0, 11\n";
  DriverStr(expected, "LoadConst64IsIntW15S43b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW15S44b) {
  __ LoadConst64(mips64::V0, -576443160117379072);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll32 $v0, $v0, 12\n";
  DriverStr(expected, "LoadConst64IsIntW15S44b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW15S45b) {
  __ LoadConst64(mips64::V0, -1152886320234758144);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll32 $v0, $v0, 13\n";
  DriverStr(expected, "LoadConst64IsIntW15S45b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW15S46b) {
  __ LoadConst64(mips64::V0, -2305772640469516288);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll32 $v0, $v0, 14\n";
  DriverStr(expected, "LoadConst64IsIntW15S46b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsIntW15S47b) {
  __ LoadConst64(mips64::V0, -4611545280939032576);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll32 $v0, $v0, 15\n";
  DriverStr(expected, "LoadConst64IsIntW15S47b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W17S15a) {
  __ LoadConst64(mips64::V0, 2147516416);

  const char* expected =
      "lui $v0, 1\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 15\n";
  DriverStr(expected, "LoadConst64IsInt32W17S15a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W17S31a) {
  __ LoadConst64(mips64::V0, 140739635838976);

  const char* expected =
      "lui $v0, 1\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 31\n";
  DriverStr(expected, "LoadConst64IsInt32W17S31a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W17S32a) {
  __ LoadConst64(mips64::V0, 281479271677952);

  const char* expected =
      "lui $v0, 1\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 0\n";
  DriverStr(expected, "LoadConst64IsInt32W17S32a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W17S33a) {
  __ LoadConst64(mips64::V0, 562958543355904);

  const char* expected =
      "lui $v0, 1\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64IsInt32W17S33a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W17S34a) {
  __ LoadConst64(mips64::V0, 1125917086711808);

  const char* expected =
      "lui $v0, 1\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 2\n";
  DriverStr(expected, "LoadConst64IsInt32W17S34a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W17S35a) {
  __ LoadConst64(mips64::V0, 2251834173423616);

  const char* expected =
      "lui $v0, 1\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 3\n";
  DriverStr(expected, "LoadConst64IsInt32W17S35a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W17S36a) {
  __ LoadConst64(mips64::V0, 4503668346847232);

  const char* expected =
      "lui $v0, 1\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 4\n";
  DriverStr(expected, "LoadConst64IsInt32W17S36a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W17S37a) {
  __ LoadConst64(mips64::V0, 9007336693694464);

  const char* expected =
      "lui $v0, 1\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 5\n";
  DriverStr(expected, "LoadConst64IsInt32W17S37a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W17S38a) {
  __ LoadConst64(mips64::V0, 18014673387388928);

  const char* expected =
      "lui $v0, 1\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 6\n";
  DriverStr(expected, "LoadConst64IsInt32W17S38a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W17S39a) {
  __ LoadConst64(mips64::V0, 36029346774777856);

  const char* expected =
      "lui $v0, 1\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 7\n";
  DriverStr(expected, "LoadConst64IsInt32W17S39a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W17S40a) {
  __ LoadConst64(mips64::V0, 72058693549555712);

  const char* expected =
      "lui $v0, 1\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 8\n";
  DriverStr(expected, "LoadConst64IsInt32W17S40a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W17S41a) {
  __ LoadConst64(mips64::V0, 144117387099111424);

  const char* expected =
      "lui $v0, 1\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 9\n";
  DriverStr(expected, "LoadConst64IsInt32W17S41a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W17S42a) {
  __ LoadConst64(mips64::V0, 288234774198222848);

  const char* expected =
      "lui $v0, 1\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 10\n";
  DriverStr(expected, "LoadConst64IsInt32W17S42a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W17S43a) {
  __ LoadConst64(mips64::V0, 576469548396445696);

  const char* expected =
      "lui $v0, 1\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 11\n";
  DriverStr(expected, "LoadConst64IsInt32W17S43a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W17S44a) {
  __ LoadConst64(mips64::V0, 1152939096792891392);

  const char* expected =
      "lui $v0, 1\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 12\n";
  DriverStr(expected, "LoadConst64IsInt32W17S44a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W17S45a) {
  __ LoadConst64(mips64::V0, 2305878193585782784);

  const char* expected =
      "lui $v0, 1\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 13\n";
  DriverStr(expected, "LoadConst64IsInt32W17S45a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W17S46a) {
  __ LoadConst64(mips64::V0, 4611756387171565568);

  const char* expected =
      "lui $v0, 1\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 14\n";
  DriverStr(expected, "LoadConst64IsInt32W17S46a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W18S14a) {
  __ LoadConst64(mips64::V0, 2147500032);

  const char* expected =
      "lui $v0, 2\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 14\n";
  DriverStr(expected, "LoadConst64IsInt32W18S14a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W18S30a) {
  __ LoadConst64(mips64::V0, 140738562097152);

  const char* expected =
      "lui $v0, 2\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 30\n";
  DriverStr(expected, "LoadConst64IsInt32W18S30a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W18S31a) {
  __ LoadConst64(mips64::V0, 281477124194304);

  const char* expected =
      "lui $v0, 2\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 31\n";
  DriverStr(expected, "LoadConst64IsInt32W18S31a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W18S32a) {
  __ LoadConst64(mips64::V0, 562954248388608);

  const char* expected =
      "lui $v0, 2\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 0\n";
  DriverStr(expected, "LoadConst64IsInt32W18S32a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W18S33a) {
  __ LoadConst64(mips64::V0, 1125908496777216);

  const char* expected =
      "lui $v0, 2\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64IsInt32W18S33a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W18S34a) {
  __ LoadConst64(mips64::V0, 2251816993554432);

  const char* expected =
      "lui $v0, 2\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 2\n";
  DriverStr(expected, "LoadConst64IsInt32W18S34a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W18S35a) {
  __ LoadConst64(mips64::V0, 4503633987108864);

  const char* expected =
      "lui $v0, 2\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 3\n";
  DriverStr(expected, "LoadConst64IsInt32W18S35a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W18S36a) {
  __ LoadConst64(mips64::V0, 9007267974217728);

  const char* expected =
      "lui $v0, 2\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 4\n";
  DriverStr(expected, "LoadConst64IsInt32W18S36a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W18S37a) {
  __ LoadConst64(mips64::V0, 18014535948435456);

  const char* expected =
      "lui $v0, 2\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 5\n";
  DriverStr(expected, "LoadConst64IsInt32W18S37a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W18S38a) {
  __ LoadConst64(mips64::V0, 36029071896870912);

  const char* expected =
      "lui $v0, 2\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 6\n";
  DriverStr(expected, "LoadConst64IsInt32W18S38a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W18S39a) {
  __ LoadConst64(mips64::V0, 72058143793741824);

  const char* expected =
      "lui $v0, 2\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 7\n";
  DriverStr(expected, "LoadConst64IsInt32W18S39a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W18S40a) {
  __ LoadConst64(mips64::V0, 144116287587483648);

  const char* expected =
      "lui $v0, 2\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 8\n";
  DriverStr(expected, "LoadConst64IsInt32W18S40a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W18S41a) {
  __ LoadConst64(mips64::V0, 288232575174967296);

  const char* expected =
      "lui $v0, 2\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 9\n";
  DriverStr(expected, "LoadConst64IsInt32W18S41a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W18S42a) {
  __ LoadConst64(mips64::V0, 576465150349934592);

  const char* expected =
      "lui $v0, 2\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 10\n";
  DriverStr(expected, "LoadConst64IsInt32W18S42a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W18S43a) {
  __ LoadConst64(mips64::V0, 1152930300699869184);

  const char* expected =
      "lui $v0, 2\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 11\n";
  DriverStr(expected, "LoadConst64IsInt32W18S43a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W18S44a) {
  __ LoadConst64(mips64::V0, 2305860601399738368);

  const char* expected =
      "lui $v0, 2\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 12\n";
  DriverStr(expected, "LoadConst64IsInt32W18S44a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W18S45a) {
  __ LoadConst64(mips64::V0, 4611721202799476736);

  const char* expected =
      "lui $v0, 2\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 13\n";
  DriverStr(expected, "LoadConst64IsInt32W18S45a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W19S13a) {
  __ LoadConst64(mips64::V0, 2147491840);

  const char* expected =
      "lui $v0, 4\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 13\n";
  DriverStr(expected, "LoadConst64IsInt32W19S13a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W19S29a) {
  __ LoadConst64(mips64::V0, 140738025226240);

  const char* expected =
      "lui $v0, 4\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 29\n";
  DriverStr(expected, "LoadConst64IsInt32W19S29a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W19S31a) {
  __ LoadConst64(mips64::V0, 562952100904960);

  const char* expected =
      "lui $v0, 4\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 31\n";
  DriverStr(expected, "LoadConst64IsInt32W19S31a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W19S32a) {
  __ LoadConst64(mips64::V0, 1125904201809920);

  const char* expected =
      "lui $v0, 4\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 0\n";
  DriverStr(expected, "LoadConst64IsInt32W19S32a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W19S33a) {
  __ LoadConst64(mips64::V0, 2251808403619840);

  const char* expected =
      "lui $v0, 4\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64IsInt32W19S33a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W19S34a) {
  __ LoadConst64(mips64::V0, 4503616807239680);

  const char* expected =
      "lui $v0, 4\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 2\n";
  DriverStr(expected, "LoadConst64IsInt32W19S34a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W19S35a) {
  __ LoadConst64(mips64::V0, 9007233614479360);

  const char* expected =
      "lui $v0, 4\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 3\n";
  DriverStr(expected, "LoadConst64IsInt32W19S35a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W19S36a) {
  __ LoadConst64(mips64::V0, 18014467228958720);

  const char* expected =
      "lui $v0, 4\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 4\n";
  DriverStr(expected, "LoadConst64IsInt32W19S36a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W19S37a) {
  __ LoadConst64(mips64::V0, 36028934457917440);

  const char* expected =
      "lui $v0, 4\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 5\n";
  DriverStr(expected, "LoadConst64IsInt32W19S37a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W19S38a) {
  __ LoadConst64(mips64::V0, 72057868915834880);

  const char* expected =
      "lui $v0, 4\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 6\n";
  DriverStr(expected, "LoadConst64IsInt32W19S38a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W19S39a) {
  __ LoadConst64(mips64::V0, 144115737831669760);

  const char* expected =
      "lui $v0, 4\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 7\n";
  DriverStr(expected, "LoadConst64IsInt32W19S39a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W19S40a) {
  __ LoadConst64(mips64::V0, 288231475663339520);

  const char* expected =
      "lui $v0, 4\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 8\n";
  DriverStr(expected, "LoadConst64IsInt32W19S40a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W19S41a) {
  __ LoadConst64(mips64::V0, 576462951326679040);

  const char* expected =
      "lui $v0, 4\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 9\n";
  DriverStr(expected, "LoadConst64IsInt32W19S41a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W19S42a) {
  __ LoadConst64(mips64::V0, 1152925902653358080);

  const char* expected =
      "lui $v0, 4\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 10\n";
  DriverStr(expected, "LoadConst64IsInt32W19S42a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W19S43a) {
  __ LoadConst64(mips64::V0, 2305851805306716160);

  const char* expected =
      "lui $v0, 4\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 11\n";
  DriverStr(expected, "LoadConst64IsInt32W19S43a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W19S44a) {
  __ LoadConst64(mips64::V0, 4611703610613432320);

  const char* expected =
      "lui $v0, 4\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 12\n";
  DriverStr(expected, "LoadConst64IsInt32W19S44a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W20S12a) {
  __ LoadConst64(mips64::V0, 2147487744);

  const char* expected =
      "lui $v0, 8\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 12\n";
  DriverStr(expected, "LoadConst64IsInt32W20S12a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W20S28a) {
  __ LoadConst64(mips64::V0, 140737756790784);

  const char* expected =
      "lui $v0, 8\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 28\n";
  DriverStr(expected, "LoadConst64IsInt32W20S28a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W20S31a) {
  __ LoadConst64(mips64::V0, 1125902054326272);

  const char* expected =
      "lui $v0, 8\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 31\n";
  DriverStr(expected, "LoadConst64IsInt32W20S31a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W20S32a) {
  __ LoadConst64(mips64::V0, 2251804108652544);

  const char* expected =
      "lui $v0, 8\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 0\n";
  DriverStr(expected, "LoadConst64IsInt32W20S32a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W20S33a) {
  __ LoadConst64(mips64::V0, 4503608217305088);

  const char* expected =
      "lui $v0, 8\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64IsInt32W20S33a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W20S34a) {
  __ LoadConst64(mips64::V0, 9007216434610176);

  const char* expected =
      "lui $v0, 8\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 2\n";
  DriverStr(expected, "LoadConst64IsInt32W20S34a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W20S35a) {
  __ LoadConst64(mips64::V0, 18014432869220352);

  const char* expected =
      "lui $v0, 8\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 3\n";
  DriverStr(expected, "LoadConst64IsInt32W20S35a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W20S36a) {
  __ LoadConst64(mips64::V0, 36028865738440704);

  const char* expected =
      "lui $v0, 8\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 4\n";
  DriverStr(expected, "LoadConst64IsInt32W20S36a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W20S37a) {
  __ LoadConst64(mips64::V0, 72057731476881408);

  const char* expected =
      "lui $v0, 8\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 5\n";
  DriverStr(expected, "LoadConst64IsInt32W20S37a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W20S38a) {
  __ LoadConst64(mips64::V0, 144115462953762816);

  const char* expected =
      "lui $v0, 8\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 6\n";
  DriverStr(expected, "LoadConst64IsInt32W20S38a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W20S39a) {
  __ LoadConst64(mips64::V0, 288230925907525632);

  const char* expected =
      "lui $v0, 8\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 7\n";
  DriverStr(expected, "LoadConst64IsInt32W20S39a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W20S40a) {
  __ LoadConst64(mips64::V0, 576461851815051264);

  const char* expected =
      "lui $v0, 8\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 8\n";
  DriverStr(expected, "LoadConst64IsInt32W20S40a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W20S41a) {
  __ LoadConst64(mips64::V0, 1152923703630102528);

  const char* expected =
      "lui $v0, 8\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 9\n";
  DriverStr(expected, "LoadConst64IsInt32W20S41a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W20S42a) {
  __ LoadConst64(mips64::V0, 2305847407260205056);

  const char* expected =
      "lui $v0, 8\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 10\n";
  DriverStr(expected, "LoadConst64IsInt32W20S42a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W20S43a) {
  __ LoadConst64(mips64::V0, 4611694814520410112);

  const char* expected =
      "lui $v0, 8\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 11\n";
  DriverStr(expected, "LoadConst64IsInt32W20S43a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W21S11a) {
  __ LoadConst64(mips64::V0, 2147485696);

  const char* expected =
      "lui $v0, 16\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 11\n";
  DriverStr(expected, "LoadConst64IsInt32W21S11a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W21S27a) {
  __ LoadConst64(mips64::V0, 140737622573056);

  const char* expected =
      "lui $v0, 16\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 27\n";
  DriverStr(expected, "LoadConst64IsInt32W21S27a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W21S31a) {
  __ LoadConst64(mips64::V0, 2251801961168896);

  const char* expected =
      "lui $v0, 16\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 31\n";
  DriverStr(expected, "LoadConst64IsInt32W21S31a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W21S32a) {
  __ LoadConst64(mips64::V0, 4503603922337792);

  const char* expected =
      "lui $v0, 16\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 0\n";
  DriverStr(expected, "LoadConst64IsInt32W21S32a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W21S33a) {
  __ LoadConst64(mips64::V0, 9007207844675584);

  const char* expected =
      "lui $v0, 16\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64IsInt32W21S33a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W21S34a) {
  __ LoadConst64(mips64::V0, 18014415689351168);

  const char* expected =
      "lui $v0, 16\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 2\n";
  DriverStr(expected, "LoadConst64IsInt32W21S34a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W21S35a) {
  __ LoadConst64(mips64::V0, 36028831378702336);

  const char* expected =
      "lui $v0, 16\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 3\n";
  DriverStr(expected, "LoadConst64IsInt32W21S35a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W21S36a) {
  __ LoadConst64(mips64::V0, 72057662757404672);

  const char* expected =
      "lui $v0, 16\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 4\n";
  DriverStr(expected, "LoadConst64IsInt32W21S36a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W21S37a) {
  __ LoadConst64(mips64::V0, 144115325514809344);

  const char* expected =
      "lui $v0, 16\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 5\n";
  DriverStr(expected, "LoadConst64IsInt32W21S37a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W21S38a) {
  __ LoadConst64(mips64::V0, 288230651029618688);

  const char* expected =
      "lui $v0, 16\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 6\n";
  DriverStr(expected, "LoadConst64IsInt32W21S38a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W21S39a) {
  __ LoadConst64(mips64::V0, 576461302059237376);

  const char* expected =
      "lui $v0, 16\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 7\n";
  DriverStr(expected, "LoadConst64IsInt32W21S39a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W21S40a) {
  __ LoadConst64(mips64::V0, 1152922604118474752);

  const char* expected =
      "lui $v0, 16\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 8\n";
  DriverStr(expected, "LoadConst64IsInt32W21S40a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W21S41a) {
  __ LoadConst64(mips64::V0, 2305845208236949504);

  const char* expected =
      "lui $v0, 16\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 9\n";
  DriverStr(expected, "LoadConst64IsInt32W21S41a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W21S42a) {
  __ LoadConst64(mips64::V0, 4611690416473899008);

  const char* expected =
      "lui $v0, 16\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 10\n";
  DriverStr(expected, "LoadConst64IsInt32W21S42a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W22S10a) {
  __ LoadConst64(mips64::V0, 2147484672);

  const char* expected =
      "lui $v0, 32\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 10\n";
  DriverStr(expected, "LoadConst64IsInt32W22S10a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W22S26a) {
  __ LoadConst64(mips64::V0, 140737555464192);

  const char* expected =
      "lui $v0, 32\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 26\n";
  DriverStr(expected, "LoadConst64IsInt32W22S26a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W22S31a) {
  __ LoadConst64(mips64::V0, 4503601774854144);

  const char* expected =
      "lui $v0, 32\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 31\n";
  DriverStr(expected, "LoadConst64IsInt32W22S31a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W22S32a) {
  __ LoadConst64(mips64::V0, 9007203549708288);

  const char* expected =
      "lui $v0, 32\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 0\n";
  DriverStr(expected, "LoadConst64IsInt32W22S32a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W22S33a) {
  __ LoadConst64(mips64::V0, 18014407099416576);

  const char* expected =
      "lui $v0, 32\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64IsInt32W22S33a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W22S34a) {
  __ LoadConst64(mips64::V0, 36028814198833152);

  const char* expected =
      "lui $v0, 32\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 2\n";
  DriverStr(expected, "LoadConst64IsInt32W22S34a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W22S35a) {
  __ LoadConst64(mips64::V0, 72057628397666304);

  const char* expected =
      "lui $v0, 32\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 3\n";
  DriverStr(expected, "LoadConst64IsInt32W22S35a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W22S36a) {
  __ LoadConst64(mips64::V0, 144115256795332608);

  const char* expected =
      "lui $v0, 32\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 4\n";
  DriverStr(expected, "LoadConst64IsInt32W22S36a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W22S37a) {
  __ LoadConst64(mips64::V0, 288230513590665216);

  const char* expected =
      "lui $v0, 32\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 5\n";
  DriverStr(expected, "LoadConst64IsInt32W22S37a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W22S38a) {
  __ LoadConst64(mips64::V0, 576461027181330432);

  const char* expected =
      "lui $v0, 32\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 6\n";
  DriverStr(expected, "LoadConst64IsInt32W22S38a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W22S39a) {
  __ LoadConst64(mips64::V0, 1152922054362660864);

  const char* expected =
      "lui $v0, 32\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 7\n";
  DriverStr(expected, "LoadConst64IsInt32W22S39a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W22S40a) {
  __ LoadConst64(mips64::V0, 2305844108725321728);

  const char* expected =
      "lui $v0, 32\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 8\n";
  DriverStr(expected, "LoadConst64IsInt32W22S40a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W22S41a) {
  __ LoadConst64(mips64::V0, 4611688217450643456);

  const char* expected =
      "lui $v0, 32\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 9\n";
  DriverStr(expected, "LoadConst64IsInt32W22S41a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W23S9a) {
  __ LoadConst64(mips64::V0, 2147484160);

  const char* expected =
      "lui $v0, 64\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 9\n";
  DriverStr(expected, "LoadConst64IsInt32W23S9a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W23S25a) {
  __ LoadConst64(mips64::V0, 140737521909760);

  const char* expected =
      "lui $v0, 64\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 25\n";
  DriverStr(expected, "LoadConst64IsInt32W23S25a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W23S31a) {
  __ LoadConst64(mips64::V0, 9007201402224640);

  const char* expected =
      "lui $v0, 64\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 31\n";
  DriverStr(expected, "LoadConst64IsInt32W23S31a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W23S32a) {
  __ LoadConst64(mips64::V0, 18014402804449280);

  const char* expected =
      "lui $v0, 64\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 0\n";
  DriverStr(expected, "LoadConst64IsInt32W23S32a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W23S33a) {
  __ LoadConst64(mips64::V0, 36028805608898560);

  const char* expected =
      "lui $v0, 64\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64IsInt32W23S33a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W23S34a) {
  __ LoadConst64(mips64::V0, 72057611217797120);

  const char* expected =
      "lui $v0, 64\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 2\n";
  DriverStr(expected, "LoadConst64IsInt32W23S34a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W23S35a) {
  __ LoadConst64(mips64::V0, 144115222435594240);

  const char* expected =
      "lui $v0, 64\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 3\n";
  DriverStr(expected, "LoadConst64IsInt32W23S35a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W23S36a) {
  __ LoadConst64(mips64::V0, 288230444871188480);

  const char* expected =
      "lui $v0, 64\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 4\n";
  DriverStr(expected, "LoadConst64IsInt32W23S36a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W23S37a) {
  __ LoadConst64(mips64::V0, 576460889742376960);

  const char* expected =
      "lui $v0, 64\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 5\n";
  DriverStr(expected, "LoadConst64IsInt32W23S37a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W23S38a) {
  __ LoadConst64(mips64::V0, 1152921779484753920);

  const char* expected =
      "lui $v0, 64\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 6\n";
  DriverStr(expected, "LoadConst64IsInt32W23S38a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W23S39a) {
  __ LoadConst64(mips64::V0, 2305843558969507840);

  const char* expected =
      "lui $v0, 64\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 7\n";
  DriverStr(expected, "LoadConst64IsInt32W23S39a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W23S40a) {
  __ LoadConst64(mips64::V0, 4611687117939015680);

  const char* expected =
      "lui $v0, 64\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 8\n";
  DriverStr(expected, "LoadConst64IsInt32W23S40a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W24S8a) {
  __ LoadConst64(mips64::V0, 2147483904);

  const char* expected =
      "lui $v0, 128\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 8\n";
  DriverStr(expected, "LoadConst64IsInt32W24S8a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W24S24a) {
  __ LoadConst64(mips64::V0, 140737505132544);

  const char* expected =
      "lui $v0, 128\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 24\n";
  DriverStr(expected, "LoadConst64IsInt32W24S24a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W24S31a) {
  __ LoadConst64(mips64::V0, 18014400656965632);

  const char* expected =
      "lui $v0, 128\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 31\n";
  DriverStr(expected, "LoadConst64IsInt32W24S31a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W24S32a) {
  __ LoadConst64(mips64::V0, 36028801313931264);

  const char* expected =
      "lui $v0, 128\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 0\n";
  DriverStr(expected, "LoadConst64IsInt32W24S32a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W24S33a) {
  __ LoadConst64(mips64::V0, 72057602627862528);

  const char* expected =
      "lui $v0, 128\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64IsInt32W24S33a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W24S34a) {
  __ LoadConst64(mips64::V0, 144115205255725056);

  const char* expected =
      "lui $v0, 128\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 2\n";
  DriverStr(expected, "LoadConst64IsInt32W24S34a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W24S35a) {
  __ LoadConst64(mips64::V0, 288230410511450112);

  const char* expected =
      "lui $v0, 128\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 3\n";
  DriverStr(expected, "LoadConst64IsInt32W24S35a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W24S36a) {
  __ LoadConst64(mips64::V0, 576460821022900224);

  const char* expected =
      "lui $v0, 128\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 4\n";
  DriverStr(expected, "LoadConst64IsInt32W24S36a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W24S37a) {
  __ LoadConst64(mips64::V0, 1152921642045800448);

  const char* expected =
      "lui $v0, 128\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 5\n";
  DriverStr(expected, "LoadConst64IsInt32W24S37a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W24S38a) {
  __ LoadConst64(mips64::V0, 2305843284091600896);

  const char* expected =
      "lui $v0, 128\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 6\n";
  DriverStr(expected, "LoadConst64IsInt32W24S38a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W24S39a) {
  __ LoadConst64(mips64::V0, 4611686568183201792);

  const char* expected =
      "lui $v0, 128\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 7\n";
  DriverStr(expected, "LoadConst64IsInt32W24S39a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W25S7a) {
  __ LoadConst64(mips64::V0, 2147483776);

  const char* expected =
      "lui $v0, 256\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 7\n";
  DriverStr(expected, "LoadConst64IsInt32W25S7a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W25S23a) {
  __ LoadConst64(mips64::V0, 140737496743936);

  const char* expected =
      "lui $v0, 256\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 23\n";
  DriverStr(expected, "LoadConst64IsInt32W25S23a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W25S31a) {
  __ LoadConst64(mips64::V0, 36028799166447616);

  const char* expected =
      "lui $v0, 256\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 31\n";
  DriverStr(expected, "LoadConst64IsInt32W25S31a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W25S32a) {
  __ LoadConst64(mips64::V0, 72057598332895232);

  const char* expected =
      "lui $v0, 256\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 0\n";
  DriverStr(expected, "LoadConst64IsInt32W25S32a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W25S33a) {
  __ LoadConst64(mips64::V0, 144115196665790464);

  const char* expected =
      "lui $v0, 256\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64IsInt32W25S33a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W25S34a) {
  __ LoadConst64(mips64::V0, 288230393331580928);

  const char* expected =
      "lui $v0, 256\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 2\n";
  DriverStr(expected, "LoadConst64IsInt32W25S34a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W25S35a) {
  __ LoadConst64(mips64::V0, 576460786663161856);

  const char* expected =
      "lui $v0, 256\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 3\n";
  DriverStr(expected, "LoadConst64IsInt32W25S35a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W25S36a) {
  __ LoadConst64(mips64::V0, 1152921573326323712);

  const char* expected =
      "lui $v0, 256\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 4\n";
  DriverStr(expected, "LoadConst64IsInt32W25S36a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W25S37a) {
  __ LoadConst64(mips64::V0, 2305843146652647424);

  const char* expected =
      "lui $v0, 256\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 5\n";
  DriverStr(expected, "LoadConst64IsInt32W25S37a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W25S38a) {
  __ LoadConst64(mips64::V0, 4611686293305294848);

  const char* expected =
      "lui $v0, 256\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 6\n";
  DriverStr(expected, "LoadConst64IsInt32W25S38a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W26S6a) {
  __ LoadConst64(mips64::V0, 2147483712);

  const char* expected =
      "lui $v0, 512\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 6\n";
  DriverStr(expected, "LoadConst64IsInt32W26S6a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W26S22a) {
  __ LoadConst64(mips64::V0, 140737492549632);

  const char* expected =
      "lui $v0, 512\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 22\n";
  DriverStr(expected, "LoadConst64IsInt32W26S22a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W26S31a) {
  __ LoadConst64(mips64::V0, 72057596185411584);

  const char* expected =
      "lui $v0, 512\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 31\n";
  DriverStr(expected, "LoadConst64IsInt32W26S31a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W26S32a) {
  __ LoadConst64(mips64::V0, 144115192370823168);

  const char* expected =
      "lui $v0, 512\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 0\n";
  DriverStr(expected, "LoadConst64IsInt32W26S32a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W26S33a) {
  __ LoadConst64(mips64::V0, 288230384741646336);

  const char* expected =
      "lui $v0, 512\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64IsInt32W26S33a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W26S34a) {
  __ LoadConst64(mips64::V0, 576460769483292672);

  const char* expected =
      "lui $v0, 512\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 2\n";
  DriverStr(expected, "LoadConst64IsInt32W26S34a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W26S35a) {
  __ LoadConst64(mips64::V0, 1152921538966585344);

  const char* expected =
      "lui $v0, 512\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 3\n";
  DriverStr(expected, "LoadConst64IsInt32W26S35a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W26S36a) {
  __ LoadConst64(mips64::V0, 2305843077933170688);

  const char* expected =
      "lui $v0, 512\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 4\n";
  DriverStr(expected, "LoadConst64IsInt32W26S36a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W26S37a) {
  __ LoadConst64(mips64::V0, 4611686155866341376);

  const char* expected =
      "lui $v0, 512\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 5\n";
  DriverStr(expected, "LoadConst64IsInt32W26S37a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W27S5a) {
  __ LoadConst64(mips64::V0, 2147483680);

  const char* expected =
      "lui $v0, 1024\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 5\n";
  DriverStr(expected, "LoadConst64IsInt32W27S5a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W27S21a) {
  __ LoadConst64(mips64::V0, 140737490452480);

  const char* expected =
      "lui $v0, 1024\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 21\n";
  DriverStr(expected, "LoadConst64IsInt32W27S21a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W27S31a) {
  __ LoadConst64(mips64::V0, 144115190223339520);

  const char* expected =
      "lui $v0, 1024\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 31\n";
  DriverStr(expected, "LoadConst64IsInt32W27S31a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W27S32a) {
  __ LoadConst64(mips64::V0, 288230380446679040);

  const char* expected =
      "lui $v0, 1024\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 0\n";
  DriverStr(expected, "LoadConst64IsInt32W27S32a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W27S33a) {
  __ LoadConst64(mips64::V0, 576460760893358080);

  const char* expected =
      "lui $v0, 1024\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64IsInt32W27S33a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W27S34a) {
  __ LoadConst64(mips64::V0, 1152921521786716160);

  const char* expected =
      "lui $v0, 1024\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 2\n";
  DriverStr(expected, "LoadConst64IsInt32W27S34a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W27S35a) {
  __ LoadConst64(mips64::V0, 2305843043573432320);

  const char* expected =
      "lui $v0, 1024\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 3\n";
  DriverStr(expected, "LoadConst64IsInt32W27S35a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W27S36a) {
  __ LoadConst64(mips64::V0, 4611686087146864640);

  const char* expected =
      "lui $v0, 1024\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 4\n";
  DriverStr(expected, "LoadConst64IsInt32W27S36a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W28S4a) {
  __ LoadConst64(mips64::V0, 2147483664);

  const char* expected =
      "lui $v0, 2048\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 4\n";
  DriverStr(expected, "LoadConst64IsInt32W28S4a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W28S20a) {
  __ LoadConst64(mips64::V0, 140737489403904);

  const char* expected =
      "lui $v0, 2048\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 20\n";
  DriverStr(expected, "LoadConst64IsInt32W28S20a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W28S31a) {
  __ LoadConst64(mips64::V0, 288230378299195392);

  const char* expected =
      "lui $v0, 2048\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 31\n";
  DriverStr(expected, "LoadConst64IsInt32W28S31a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W28S32a) {
  __ LoadConst64(mips64::V0, 576460756598390784);

  const char* expected =
      "lui $v0, 2048\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 0\n";
  DriverStr(expected, "LoadConst64IsInt32W28S32a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W28S33a) {
  __ LoadConst64(mips64::V0, 1152921513196781568);

  const char* expected =
      "lui $v0, 2048\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64IsInt32W28S33a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W28S34a) {
  __ LoadConst64(mips64::V0, 2305843026393563136);

  const char* expected =
      "lui $v0, 2048\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 2\n";
  DriverStr(expected, "LoadConst64IsInt32W28S34a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W28S35a) {
  __ LoadConst64(mips64::V0, 4611686052787126272);

  const char* expected =
      "lui $v0, 2048\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 3\n";
  DriverStr(expected, "LoadConst64IsInt32W28S35a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W29S3a) {
  __ LoadConst64(mips64::V0, 2147483656);

  const char* expected =
      "lui $v0, 4096\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 3\n";
  DriverStr(expected, "LoadConst64IsInt32W29S3a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W29S19a) {
  __ LoadConst64(mips64::V0, 140737488879616);

  const char* expected =
      "lui $v0, 4096\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 19\n";
  DriverStr(expected, "LoadConst64IsInt32W29S19a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W29S31a) {
  __ LoadConst64(mips64::V0, 576460754450907136);

  const char* expected =
      "lui $v0, 4096\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 31\n";
  DriverStr(expected, "LoadConst64IsInt32W29S31a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W29S32a) {
  __ LoadConst64(mips64::V0, 1152921508901814272);

  const char* expected =
      "lui $v0, 4096\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 0\n";
  DriverStr(expected, "LoadConst64IsInt32W29S32a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W29S33a) {
  __ LoadConst64(mips64::V0, 2305843017803628544);

  const char* expected =
      "lui $v0, 4096\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64IsInt32W29S33a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W29S34a) {
  __ LoadConst64(mips64::V0, 4611686035607257088);

  const char* expected =
      "lui $v0, 4096\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 2\n";
  DriverStr(expected, "LoadConst64IsInt32W29S34a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W30S2a) {
  __ LoadConst64(mips64::V0, 2147483652);

  const char* expected =
      "lui $v0, 8192\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 2\n";
  DriverStr(expected, "LoadConst64IsInt32W30S2a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W30S18a) {
  __ LoadConst64(mips64::V0, 140737488617472);

  const char* expected =
      "lui $v0, 8192\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 18\n";
  DriverStr(expected, "LoadConst64IsInt32W30S18a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W30S31a) {
  __ LoadConst64(mips64::V0, 1152921506754330624);

  const char* expected =
      "lui $v0, 8192\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 31\n";
  DriverStr(expected, "LoadConst64IsInt32W30S31a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W30S32a) {
  __ LoadConst64(mips64::V0, 2305843013508661248);

  const char* expected =
      "lui $v0, 8192\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 0\n";
  DriverStr(expected, "LoadConst64IsInt32W30S32a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W30S33a) {
  __ LoadConst64(mips64::V0, 4611686027017322496);

  const char* expected =
      "lui $v0, 8192\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64IsInt32W30S33a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W31S1a) {
  __ LoadConst64(mips64::V0, 2147483650);

  const char* expected =
      "lui $v0, 16384\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64IsInt32W31S1a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W31S17a) {
  __ LoadConst64(mips64::V0, 140737488486400);

  const char* expected =
      "lui $v0, 16384\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 17\n";
  DriverStr(expected, "LoadConst64IsInt32W31S17a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W31S31a) {
  __ LoadConst64(mips64::V0, 2305843011361177600);

  const char* expected =
      "lui $v0, 16384\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 31\n";
  DriverStr(expected, "LoadConst64IsInt32W31S31a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W31S32a) {
  __ LoadConst64(mips64::V0, 4611686022722355200);

  const char* expected =
      "lui $v0, 16384\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 0\n";
  DriverStr(expected, "LoadConst64IsInt32W31S32a");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W16S32b) {
  __ LoadConst64(mips64::V0, -281470681743360);

  const char* expected =
      "lui $v0, 65535\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 0\n";
  DriverStr(expected, "LoadConst64IsInt32W16S32b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W16S33b) {
  __ LoadConst64(mips64::V0, -562941363486720);

  const char* expected =
      "lui $v0, 65535\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64IsInt32W16S33b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W16S34b) {
  __ LoadConst64(mips64::V0, -1125882726973440);

  const char* expected =
      "lui $v0, 65535\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 2\n";
  DriverStr(expected, "LoadConst64IsInt32W16S34b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W16S35b) {
  __ LoadConst64(mips64::V0, -2251765453946880);

  const char* expected =
      "lui $v0, 65535\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 3\n";
  DriverStr(expected, "LoadConst64IsInt32W16S35b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W16S36b) {
  __ LoadConst64(mips64::V0, -4503530907893760);

  const char* expected =
      "lui $v0, 65535\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 4\n";
  DriverStr(expected, "LoadConst64IsInt32W16S36b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W16S37b) {
  __ LoadConst64(mips64::V0, -9007061815787520);

  const char* expected =
      "lui $v0, 65535\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 5\n";
  DriverStr(expected, "LoadConst64IsInt32W16S37b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W16S38b) {
  __ LoadConst64(mips64::V0, -18014123631575040);

  const char* expected =
      "lui $v0, 65535\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 6\n";
  DriverStr(expected, "LoadConst64IsInt32W16S38b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W16S39b) {
  __ LoadConst64(mips64::V0, -36028247263150080);

  const char* expected =
      "lui $v0, 65535\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 7\n";
  DriverStr(expected, "LoadConst64IsInt32W16S39b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W16S40b) {
  __ LoadConst64(mips64::V0, -72056494526300160);

  const char* expected =
      "lui $v0, 65535\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 8\n";
  DriverStr(expected, "LoadConst64IsInt32W16S40b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W16S41b) {
  __ LoadConst64(mips64::V0, -144112989052600320);

  const char* expected =
      "lui $v0, 65535\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 9\n";
  DriverStr(expected, "LoadConst64IsInt32W16S41b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W16S42b) {
  __ LoadConst64(mips64::V0, -288225978105200640);

  const char* expected =
      "lui $v0, 65535\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 10\n";
  DriverStr(expected, "LoadConst64IsInt32W16S42b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W16S43b) {
  __ LoadConst64(mips64::V0, -576451956210401280);

  const char* expected =
      "lui $v0, 65535\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 11\n";
  DriverStr(expected, "LoadConst64IsInt32W16S43b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W16S44b) {
  __ LoadConst64(mips64::V0, -1152903912420802560);

  const char* expected =
      "lui $v0, 65535\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 12\n";
  DriverStr(expected, "LoadConst64IsInt32W16S44b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W16S45b) {
  __ LoadConst64(mips64::V0, -2305807824841605120);

  const char* expected =
      "lui $v0, 65535\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 13\n";
  DriverStr(expected, "LoadConst64IsInt32W16S45b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W16S46b) {
  __ LoadConst64(mips64::V0, -4611615649683210240);

  const char* expected =
      "lui $v0, 65535\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 14\n";
  DriverStr(expected, "LoadConst64IsInt32W16S46b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W16S47b) {
  __ LoadConst64(mips64::V0, -9223231299366420480);

  const char* expected =
      "lui $v0, 65535\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 15\n";
  DriverStr(expected, "LoadConst64IsInt32W16S47b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W17S31b) {
  __ LoadConst64(mips64::V0, -281472829227008);

  const char* expected =
      "lui $v0, 65534\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 31\n";
  DriverStr(expected, "LoadConst64IsInt32W17S31b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W17S32b) {
  __ LoadConst64(mips64::V0, -562945658454016);

  const char* expected =
      "lui $v0, 65534\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 0\n";
  DriverStr(expected, "LoadConst64IsInt32W17S32b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W17S33b) {
  __ LoadConst64(mips64::V0, -1125891316908032);

  const char* expected =
      "lui $v0, 65534\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64IsInt32W17S33b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W17S34b) {
  __ LoadConst64(mips64::V0, -2251782633816064);

  const char* expected =
      "lui $v0, 65534\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 2\n";
  DriverStr(expected, "LoadConst64IsInt32W17S34b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W17S35b) {
  __ LoadConst64(mips64::V0, -4503565267632128);

  const char* expected =
      "lui $v0, 65534\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 3\n";
  DriverStr(expected, "LoadConst64IsInt32W17S35b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W17S36b) {
  __ LoadConst64(mips64::V0, -9007130535264256);

  const char* expected =
      "lui $v0, 65534\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 4\n";
  DriverStr(expected, "LoadConst64IsInt32W17S36b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W17S37b) {
  __ LoadConst64(mips64::V0, -18014261070528512);

  const char* expected =
      "lui $v0, 65534\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 5\n";
  DriverStr(expected, "LoadConst64IsInt32W17S37b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W17S38b) {
  __ LoadConst64(mips64::V0, -36028522141057024);

  const char* expected =
      "lui $v0, 65534\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 6\n";
  DriverStr(expected, "LoadConst64IsInt32W17S38b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W17S39b) {
  __ LoadConst64(mips64::V0, -72057044282114048);

  const char* expected =
      "lui $v0, 65534\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 7\n";
  DriverStr(expected, "LoadConst64IsInt32W17S39b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W17S40b) {
  __ LoadConst64(mips64::V0, -144114088564228096);

  const char* expected =
      "lui $v0, 65534\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 8\n";
  DriverStr(expected, "LoadConst64IsInt32W17S40b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W17S41b) {
  __ LoadConst64(mips64::V0, -288228177128456192);

  const char* expected =
      "lui $v0, 65534\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 9\n";
  DriverStr(expected, "LoadConst64IsInt32W17S41b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W17S42b) {
  __ LoadConst64(mips64::V0, -576456354256912384);

  const char* expected =
      "lui $v0, 65534\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 10\n";
  DriverStr(expected, "LoadConst64IsInt32W17S42b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W17S43b) {
  __ LoadConst64(mips64::V0, -1152912708513824768);

  const char* expected =
      "lui $v0, 65534\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 11\n";
  DriverStr(expected, "LoadConst64IsInt32W17S43b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W17S44b) {
  __ LoadConst64(mips64::V0, -2305825417027649536);

  const char* expected =
      "lui $v0, 65534\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 12\n";
  DriverStr(expected, "LoadConst64IsInt32W17S44b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W17S45b) {
  __ LoadConst64(mips64::V0, -4611650834055299072);

  const char* expected =
      "lui $v0, 65534\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 13\n";
  DriverStr(expected, "LoadConst64IsInt32W17S45b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W17S46b) {
  __ LoadConst64(mips64::V0, -9223301668110598144);

  const char* expected =
      "lui $v0, 65534\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 14\n";
  DriverStr(expected, "LoadConst64IsInt32W17S46b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W18S31b) {
  __ LoadConst64(mips64::V0, -562947805937664);

  const char* expected =
      "lui $v0, 65532\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 31\n";
  DriverStr(expected, "LoadConst64IsInt32W18S31b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W18S32b) {
  __ LoadConst64(mips64::V0, -1125895611875328);

  const char* expected =
      "lui $v0, 65532\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 0\n";
  DriverStr(expected, "LoadConst64IsInt32W18S32b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W18S33b) {
  __ LoadConst64(mips64::V0, -2251791223750656);

  const char* expected =
      "lui $v0, 65532\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64IsInt32W18S33b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W18S34b) {
  __ LoadConst64(mips64::V0, -4503582447501312);

  const char* expected =
      "lui $v0, 65532\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 2\n";
  DriverStr(expected, "LoadConst64IsInt32W18S34b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W18S35b) {
  __ LoadConst64(mips64::V0, -9007164895002624);

  const char* expected =
      "lui $v0, 65532\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 3\n";
  DriverStr(expected, "LoadConst64IsInt32W18S35b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W18S36b) {
  __ LoadConst64(mips64::V0, -18014329790005248);

  const char* expected =
      "lui $v0, 65532\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 4\n";
  DriverStr(expected, "LoadConst64IsInt32W18S36b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W18S37b) {
  __ LoadConst64(mips64::V0, -36028659580010496);

  const char* expected =
      "lui $v0, 65532\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 5\n";
  DriverStr(expected, "LoadConst64IsInt32W18S37b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W18S38b) {
  __ LoadConst64(mips64::V0, -72057319160020992);

  const char* expected =
      "lui $v0, 65532\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 6\n";
  DriverStr(expected, "LoadConst64IsInt32W18S38b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W18S39b) {
  __ LoadConst64(mips64::V0, -144114638320041984);

  const char* expected =
      "lui $v0, 65532\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 7\n";
  DriverStr(expected, "LoadConst64IsInt32W18S39b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W18S40b) {
  __ LoadConst64(mips64::V0, -288229276640083968);

  const char* expected =
      "lui $v0, 65532\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 8\n";
  DriverStr(expected, "LoadConst64IsInt32W18S40b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W18S41b) {
  __ LoadConst64(mips64::V0, -576458553280167936);

  const char* expected =
      "lui $v0, 65532\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 9\n";
  DriverStr(expected, "LoadConst64IsInt32W18S41b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W18S42b) {
  __ LoadConst64(mips64::V0, -1152917106560335872);

  const char* expected =
      "lui $v0, 65532\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 10\n";
  DriverStr(expected, "LoadConst64IsInt32W18S42b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W18S43b) {
  __ LoadConst64(mips64::V0, -2305834213120671744);

  const char* expected =
      "lui $v0, 65532\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 11\n";
  DriverStr(expected, "LoadConst64IsInt32W18S43b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W18S44b) {
  __ LoadConst64(mips64::V0, -4611668426241343488);

  const char* expected =
      "lui $v0, 65532\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 12\n";
  DriverStr(expected, "LoadConst64IsInt32W18S44b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W18S45b) {
  __ LoadConst64(mips64::V0, -9223336852482686976);

  const char* expected =
      "lui $v0, 65532\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 13\n";
  DriverStr(expected, "LoadConst64IsInt32W18S45b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W19S31b) {
  __ LoadConst64(mips64::V0, -1125897759358976);

  const char* expected =
      "lui $v0, 65528\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 31\n";
  DriverStr(expected, "LoadConst64IsInt32W19S31b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W19S32b) {
  __ LoadConst64(mips64::V0, -2251795518717952);

  const char* expected =
      "lui $v0, 65528\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 0\n";
  DriverStr(expected, "LoadConst64IsInt32W19S32b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W19S33b) {
  __ LoadConst64(mips64::V0, -4503591037435904);

  const char* expected =
      "lui $v0, 65528\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64IsInt32W19S33b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W19S34b) {
  __ LoadConst64(mips64::V0, -9007182074871808);

  const char* expected =
      "lui $v0, 65528\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 2\n";
  DriverStr(expected, "LoadConst64IsInt32W19S34b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W19S35b) {
  __ LoadConst64(mips64::V0, -18014364149743616);

  const char* expected =
      "lui $v0, 65528\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 3\n";
  DriverStr(expected, "LoadConst64IsInt32W19S35b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W19S36b) {
  __ LoadConst64(mips64::V0, -36028728299487232);

  const char* expected =
      "lui $v0, 65528\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 4\n";
  DriverStr(expected, "LoadConst64IsInt32W19S36b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W19S37b) {
  __ LoadConst64(mips64::V0, -72057456598974464);

  const char* expected =
      "lui $v0, 65528\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 5\n";
  DriverStr(expected, "LoadConst64IsInt32W19S37b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W19S38b) {
  __ LoadConst64(mips64::V0, -144114913197948928);

  const char* expected =
      "lui $v0, 65528\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 6\n";
  DriverStr(expected, "LoadConst64IsInt32W19S38b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W19S39b) {
  __ LoadConst64(mips64::V0, -288229826395897856);

  const char* expected =
      "lui $v0, 65528\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 7\n";
  DriverStr(expected, "LoadConst64IsInt32W19S39b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W19S40b) {
  __ LoadConst64(mips64::V0, -576459652791795712);

  const char* expected =
      "lui $v0, 65528\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 8\n";
  DriverStr(expected, "LoadConst64IsInt32W19S40b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W19S41b) {
  __ LoadConst64(mips64::V0, -1152919305583591424);

  const char* expected =
      "lui $v0, 65528\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 9\n";
  DriverStr(expected, "LoadConst64IsInt32W19S41b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W19S42b) {
  __ LoadConst64(mips64::V0, -2305838611167182848);

  const char* expected =
      "lui $v0, 65528\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 10\n";
  DriverStr(expected, "LoadConst64IsInt32W19S42b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W19S43b) {
  __ LoadConst64(mips64::V0, -4611677222334365696);

  const char* expected =
      "lui $v0, 65528\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 11\n";
  DriverStr(expected, "LoadConst64IsInt32W19S43b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W19S44b) {
  __ LoadConst64(mips64::V0, -9223354444668731392);

  const char* expected =
      "lui $v0, 65528\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 12\n";
  DriverStr(expected, "LoadConst64IsInt32W19S44b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W20S31b) {
  __ LoadConst64(mips64::V0, -2251797666201600);

  const char* expected =
      "lui $v0, 65520\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 31\n";
  DriverStr(expected, "LoadConst64IsInt32W20S31b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W20S32b) {
  __ LoadConst64(mips64::V0, -4503595332403200);

  const char* expected =
      "lui $v0, 65520\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 0\n";
  DriverStr(expected, "LoadConst64IsInt32W20S32b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W20S33b) {
  __ LoadConst64(mips64::V0, -9007190664806400);

  const char* expected =
      "lui $v0, 65520\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64IsInt32W20S33b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W20S34b) {
  __ LoadConst64(mips64::V0, -18014381329612800);

  const char* expected =
      "lui $v0, 65520\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 2\n";
  DriverStr(expected, "LoadConst64IsInt32W20S34b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W20S35b) {
  __ LoadConst64(mips64::V0, -36028762659225600);

  const char* expected =
      "lui $v0, 65520\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 3\n";
  DriverStr(expected, "LoadConst64IsInt32W20S35b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W20S36b) {
  __ LoadConst64(mips64::V0, -72057525318451200);

  const char* expected =
      "lui $v0, 65520\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 4\n";
  DriverStr(expected, "LoadConst64IsInt32W20S36b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W20S37b) {
  __ LoadConst64(mips64::V0, -144115050636902400);

  const char* expected =
      "lui $v0, 65520\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 5\n";
  DriverStr(expected, "LoadConst64IsInt32W20S37b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W20S38b) {
  __ LoadConst64(mips64::V0, -288230101273804800);

  const char* expected =
      "lui $v0, 65520\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 6\n";
  DriverStr(expected, "LoadConst64IsInt32W20S38b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W20S39b) {
  __ LoadConst64(mips64::V0, -576460202547609600);

  const char* expected =
      "lui $v0, 65520\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 7\n";
  DriverStr(expected, "LoadConst64IsInt32W20S39b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W20S40b) {
  __ LoadConst64(mips64::V0, -1152920405095219200);

  const char* expected =
      "lui $v0, 65520\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 8\n";
  DriverStr(expected, "LoadConst64IsInt32W20S40b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W20S41b) {
  __ LoadConst64(mips64::V0, -2305840810190438400);

  const char* expected =
      "lui $v0, 65520\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 9\n";
  DriverStr(expected, "LoadConst64IsInt32W20S41b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W20S42b) {
  __ LoadConst64(mips64::V0, -4611681620380876800);

  const char* expected =
      "lui $v0, 65520\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 10\n";
  DriverStr(expected, "LoadConst64IsInt32W20S42b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W20S43b) {
  __ LoadConst64(mips64::V0, -9223363240761753600);

  const char* expected =
      "lui $v0, 65520\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 11\n";
  DriverStr(expected, "LoadConst64IsInt32W20S43b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W21S31b) {
  __ LoadConst64(mips64::V0, -4503597479886848);

  const char* expected =
      "lui $v0, 65504\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 31\n";
  DriverStr(expected, "LoadConst64IsInt32W21S31b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W21S32b) {
  __ LoadConst64(mips64::V0, -9007194959773696);

  const char* expected =
      "lui $v0, 65504\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 0\n";
  DriverStr(expected, "LoadConst64IsInt32W21S32b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W21S33b) {
  __ LoadConst64(mips64::V0, -18014389919547392);

  const char* expected =
      "lui $v0, 65504\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64IsInt32W21S33b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W21S34b) {
  __ LoadConst64(mips64::V0, -36028779839094784);

  const char* expected =
      "lui $v0, 65504\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 2\n";
  DriverStr(expected, "LoadConst64IsInt32W21S34b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W21S35b) {
  __ LoadConst64(mips64::V0, -72057559678189568);

  const char* expected =
      "lui $v0, 65504\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 3\n";
  DriverStr(expected, "LoadConst64IsInt32W21S35b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W21S36b) {
  __ LoadConst64(mips64::V0, -144115119356379136);

  const char* expected =
      "lui $v0, 65504\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 4\n";
  DriverStr(expected, "LoadConst64IsInt32W21S36b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W21S37b) {
  __ LoadConst64(mips64::V0, -288230238712758272);

  const char* expected =
      "lui $v0, 65504\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 5\n";
  DriverStr(expected, "LoadConst64IsInt32W21S37b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W21S38b) {
  __ LoadConst64(mips64::V0, -576460477425516544);

  const char* expected =
      "lui $v0, 65504\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 6\n";
  DriverStr(expected, "LoadConst64IsInt32W21S38b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W21S39b) {
  __ LoadConst64(mips64::V0, -1152920954851033088);

  const char* expected =
      "lui $v0, 65504\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 7\n";
  DriverStr(expected, "LoadConst64IsInt32W21S39b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W21S40b) {
  __ LoadConst64(mips64::V0, -2305841909702066176);

  const char* expected =
      "lui $v0, 65504\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 8\n";
  DriverStr(expected, "LoadConst64IsInt32W21S40b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W21S41b) {
  __ LoadConst64(mips64::V0, -4611683819404132352);

  const char* expected =
      "lui $v0, 65504\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 9\n";
  DriverStr(expected, "LoadConst64IsInt32W21S41b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W21S42b) {
  __ LoadConst64(mips64::V0, -9223367638808264704);

  const char* expected =
      "lui $v0, 65504\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 10\n";
  DriverStr(expected, "LoadConst64IsInt32W21S42b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W22S31b) {
  __ LoadConst64(mips64::V0, -9007197107257344);

  const char* expected =
      "lui $v0, 65472\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 31\n";
  DriverStr(expected, "LoadConst64IsInt32W22S31b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W22S32b) {
  __ LoadConst64(mips64::V0, -18014394214514688);

  const char* expected =
      "lui $v0, 65472\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 0\n";
  DriverStr(expected, "LoadConst64IsInt32W22S32b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W22S33b) {
  __ LoadConst64(mips64::V0, -36028788429029376);

  const char* expected =
      "lui $v0, 65472\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64IsInt32W22S33b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W22S34b) {
  __ LoadConst64(mips64::V0, -72057576858058752);

  const char* expected =
      "lui $v0, 65472\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 2\n";
  DriverStr(expected, "LoadConst64IsInt32W22S34b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W22S35b) {
  __ LoadConst64(mips64::V0, -144115153716117504);

  const char* expected =
      "lui $v0, 65472\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 3\n";
  DriverStr(expected, "LoadConst64IsInt32W22S35b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W22S36b) {
  __ LoadConst64(mips64::V0, -288230307432235008);

  const char* expected =
      "lui $v0, 65472\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 4\n";
  DriverStr(expected, "LoadConst64IsInt32W22S36b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W22S37b) {
  __ LoadConst64(mips64::V0, -576460614864470016);

  const char* expected =
      "lui $v0, 65472\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 5\n";
  DriverStr(expected, "LoadConst64IsInt32W22S37b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W22S38b) {
  __ LoadConst64(mips64::V0, -1152921229728940032);

  const char* expected =
      "lui $v0, 65472\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 6\n";
  DriverStr(expected, "LoadConst64IsInt32W22S38b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W22S39b) {
  __ LoadConst64(mips64::V0, -2305842459457880064);

  const char* expected =
      "lui $v0, 65472\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 7\n";
  DriverStr(expected, "LoadConst64IsInt32W22S39b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W22S40b) {
  __ LoadConst64(mips64::V0, -4611684918915760128);

  const char* expected =
      "lui $v0, 65472\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 8\n";
  DriverStr(expected, "LoadConst64IsInt32W22S40b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W22S41b) {
  __ LoadConst64(mips64::V0, -9223369837831520256);

  const char* expected =
      "lui $v0, 65472\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 9\n";
  DriverStr(expected, "LoadConst64IsInt32W22S41b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W23S31b) {
  __ LoadConst64(mips64::V0, -18014396361998336);

  const char* expected =
      "lui $v0, 65408\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 31\n";
  DriverStr(expected, "LoadConst64IsInt32W23S31b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W23S32b) {
  __ LoadConst64(mips64::V0, -36028792723996672);

  const char* expected =
      "lui $v0, 65408\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 0\n";
  DriverStr(expected, "LoadConst64IsInt32W23S32b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W23S33b) {
  __ LoadConst64(mips64::V0, -72057585447993344);

  const char* expected =
      "lui $v0, 65408\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64IsInt32W23S33b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W23S34b) {
  __ LoadConst64(mips64::V0, -144115170895986688);

  const char* expected =
      "lui $v0, 65408\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 2\n";
  DriverStr(expected, "LoadConst64IsInt32W23S34b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W23S35b) {
  __ LoadConst64(mips64::V0, -288230341791973376);

  const char* expected =
      "lui $v0, 65408\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 3\n";
  DriverStr(expected, "LoadConst64IsInt32W23S35b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W23S36b) {
  __ LoadConst64(mips64::V0, -576460683583946752);

  const char* expected =
      "lui $v0, 65408\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 4\n";
  DriverStr(expected, "LoadConst64IsInt32W23S36b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W23S37b) {
  __ LoadConst64(mips64::V0, -1152921367167893504);

  const char* expected =
      "lui $v0, 65408\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 5\n";
  DriverStr(expected, "LoadConst64IsInt32W23S37b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W23S38b) {
  __ LoadConst64(mips64::V0, -2305842734335787008);

  const char* expected =
      "lui $v0, 65408\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 6\n";
  DriverStr(expected, "LoadConst64IsInt32W23S38b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W23S39b) {
  __ LoadConst64(mips64::V0, -4611685468671574016);

  const char* expected =
      "lui $v0, 65408\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 7\n";
  DriverStr(expected, "LoadConst64IsInt32W23S39b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W23S40b) {
  __ LoadConst64(mips64::V0, -9223370937343148032);

  const char* expected =
      "lui $v0, 65408\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 8\n";
  DriverStr(expected, "LoadConst64IsInt32W23S40b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W24S31b) {
  __ LoadConst64(mips64::V0, -36028794871480320);

  const char* expected =
      "lui $v0, 65280\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 31\n";
  DriverStr(expected, "LoadConst64IsInt32W24S31b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W24S32b) {
  __ LoadConst64(mips64::V0, -72057589742960640);

  const char* expected =
      "lui $v0, 65280\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 0\n";
  DriverStr(expected, "LoadConst64IsInt32W24S32b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W24S33b) {
  __ LoadConst64(mips64::V0, -144115179485921280);

  const char* expected =
      "lui $v0, 65280\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64IsInt32W24S33b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W24S34b) {
  __ LoadConst64(mips64::V0, -288230358971842560);

  const char* expected =
      "lui $v0, 65280\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 2\n";
  DriverStr(expected, "LoadConst64IsInt32W24S34b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W24S35b) {
  __ LoadConst64(mips64::V0, -576460717943685120);

  const char* expected =
      "lui $v0, 65280\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 3\n";
  DriverStr(expected, "LoadConst64IsInt32W24S35b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W24S36b) {
  __ LoadConst64(mips64::V0, -1152921435887370240);

  const char* expected =
      "lui $v0, 65280\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 4\n";
  DriverStr(expected, "LoadConst64IsInt32W24S36b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W24S37b) {
  __ LoadConst64(mips64::V0, -2305842871774740480);

  const char* expected =
      "lui $v0, 65280\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 5\n";
  DriverStr(expected, "LoadConst64IsInt32W24S37b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W24S38b) {
  __ LoadConst64(mips64::V0, -4611685743549480960);

  const char* expected =
      "lui $v0, 65280\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 6\n";
  DriverStr(expected, "LoadConst64IsInt32W24S38b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W24S39b) {
  __ LoadConst64(mips64::V0, -9223371487098961920);

  const char* expected =
      "lui $v0, 65280\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 7\n";
  DriverStr(expected, "LoadConst64IsInt32W24S39b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W25S31b) {
  __ LoadConst64(mips64::V0, -72057591890444288);

  const char* expected =
      "lui $v0, 65024\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 31\n";
  DriverStr(expected, "LoadConst64IsInt32W25S31b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W25S32b) {
  __ LoadConst64(mips64::V0, -144115183780888576);

  const char* expected =
      "lui $v0, 65024\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 0\n";
  DriverStr(expected, "LoadConst64IsInt32W25S32b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W25S33b) {
  __ LoadConst64(mips64::V0, -288230367561777152);

  const char* expected =
      "lui $v0, 65024\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64IsInt32W25S33b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W25S34b) {
  __ LoadConst64(mips64::V0, -576460735123554304);

  const char* expected =
      "lui $v0, 65024\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 2\n";
  DriverStr(expected, "LoadConst64IsInt32W25S34b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W25S35b) {
  __ LoadConst64(mips64::V0, -1152921470247108608);

  const char* expected =
      "lui $v0, 65024\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 3\n";
  DriverStr(expected, "LoadConst64IsInt32W25S35b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W25S36b) {
  __ LoadConst64(mips64::V0, -2305842940494217216);

  const char* expected =
      "lui $v0, 65024\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 4\n";
  DriverStr(expected, "LoadConst64IsInt32W25S36b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W25S37b) {
  __ LoadConst64(mips64::V0, -4611685880988434432);

  const char* expected =
      "lui $v0, 65024\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 5\n";
  DriverStr(expected, "LoadConst64IsInt32W25S37b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W25S38b) {
  __ LoadConst64(mips64::V0, -9223371761976868864);

  const char* expected =
      "lui $v0, 65024\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 6\n";
  DriverStr(expected, "LoadConst64IsInt32W25S38b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W26S31b) {
  __ LoadConst64(mips64::V0, -144115185928372224);

  const char* expected =
      "lui $v0, 64512\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 31\n";
  DriverStr(expected, "LoadConst64IsInt32W26S31b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W26S32b) {
  __ LoadConst64(mips64::V0, -288230371856744448);

  const char* expected =
      "lui $v0, 64512\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 0\n";
  DriverStr(expected, "LoadConst64IsInt32W26S32b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W26S33b) {
  __ LoadConst64(mips64::V0, -576460743713488896);

  const char* expected =
      "lui $v0, 64512\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64IsInt32W26S33b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W26S34b) {
  __ LoadConst64(mips64::V0, -1152921487426977792);

  const char* expected =
      "lui $v0, 64512\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 2\n";
  DriverStr(expected, "LoadConst64IsInt32W26S34b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W26S35b) {
  __ LoadConst64(mips64::V0, -2305842974853955584);

  const char* expected =
      "lui $v0, 64512\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 3\n";
  DriverStr(expected, "LoadConst64IsInt32W26S35b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W26S36b) {
  __ LoadConst64(mips64::V0, -4611685949707911168);

  const char* expected =
      "lui $v0, 64512\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 4\n";
  DriverStr(expected, "LoadConst64IsInt32W26S36b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W26S37b) {
  __ LoadConst64(mips64::V0, -9223371899415822336);

  const char* expected =
      "lui $v0, 64512\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 5\n";
  DriverStr(expected, "LoadConst64IsInt32W26S37b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W27S31b) {
  __ LoadConst64(mips64::V0, -288230374004228096);

  const char* expected =
      "lui $v0, 63488\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 31\n";
  DriverStr(expected, "LoadConst64IsInt32W27S31b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W27S32b) {
  __ LoadConst64(mips64::V0, -576460748008456192);

  const char* expected =
      "lui $v0, 63488\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 0\n";
  DriverStr(expected, "LoadConst64IsInt32W27S32b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W27S33b) {
  __ LoadConst64(mips64::V0, -1152921496016912384);

  const char* expected =
      "lui $v0, 63488\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64IsInt32W27S33b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W27S34b) {
  __ LoadConst64(mips64::V0, -2305842992033824768);

  const char* expected =
      "lui $v0, 63488\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 2\n";
  DriverStr(expected, "LoadConst64IsInt32W27S34b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W27S35b) {
  __ LoadConst64(mips64::V0, -4611685984067649536);

  const char* expected =
      "lui $v0, 63488\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 3\n";
  DriverStr(expected, "LoadConst64IsInt32W27S35b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W27S36b) {
  __ LoadConst64(mips64::V0, -9223371968135299072);

  const char* expected =
      "lui $v0, 63488\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 4\n";
  DriverStr(expected, "LoadConst64IsInt32W27S36b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W28S31b) {
  __ LoadConst64(mips64::V0, -576460750155939840);

  const char* expected =
      "lui $v0, 61440\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 31\n";
  DriverStr(expected, "LoadConst64IsInt32W28S31b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W28S32b) {
  __ LoadConst64(mips64::V0, -1152921500311879680);

  const char* expected =
      "lui $v0, 61440\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 0\n";
  DriverStr(expected, "LoadConst64IsInt32W28S32b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W28S33b) {
  __ LoadConst64(mips64::V0, -2305843000623759360);

  const char* expected =
      "lui $v0, 61440\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64IsInt32W28S33b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W28S34b) {
  __ LoadConst64(mips64::V0, -4611686001247518720);

  const char* expected =
      "lui $v0, 61440\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 2\n";
  DriverStr(expected, "LoadConst64IsInt32W28S34b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W28S35b) {
  __ LoadConst64(mips64::V0, -9223372002495037440);

  const char* expected =
      "lui $v0, 61440\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 3\n";
  DriverStr(expected, "LoadConst64IsInt32W28S35b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W29S31b) {
  __ LoadConst64(mips64::V0, -1152921502459363328);

  const char* expected =
      "lui $v0, 57344\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 31\n";
  DriverStr(expected, "LoadConst64IsInt32W29S31b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W29S32b) {
  __ LoadConst64(mips64::V0, -2305843004918726656);

  const char* expected =
      "lui $v0, 57344\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 0\n";
  DriverStr(expected, "LoadConst64IsInt32W29S32b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W29S33b) {
  __ LoadConst64(mips64::V0, -4611686009837453312);

  const char* expected =
      "lui $v0, 57344\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64IsInt32W29S33b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W29S34b) {
  __ LoadConst64(mips64::V0, -9223372019674906624);

  const char* expected =
      "lui $v0, 57344\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 2\n";
  DriverStr(expected, "LoadConst64IsInt32W29S34b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W30S31b) {
  __ LoadConst64(mips64::V0, -2305843007066210304);

  const char* expected =
      "lui $v0, 49152\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 31\n";
  DriverStr(expected, "LoadConst64IsInt32W30S31b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W30S32b) {
  __ LoadConst64(mips64::V0, -4611686014132420608);

  const char* expected =
      "lui $v0, 49152\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 0\n";
  DriverStr(expected, "LoadConst64IsInt32W30S32b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W30S33b) {
  __ LoadConst64(mips64::V0, -9223372028264841216);

  const char* expected =
      "lui $v0, 49152\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64IsInt32W30S33b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W31S31b) {
  __ LoadConst64(mips64::V0, -4611686016279904256);

  const char* expected =
      "lui $v0, 32768\n"
      "ori $v0, $v0, 1\n"
      "dsll $v0, $v0, 31\n";
  DriverStr(expected, "LoadConst64IsInt32W31S31b");
}

TEST_F(AssemblerMIPS64Test, LoadConst64IsInt32W31S32b) {
  __ LoadConst64(mips64::V0, -9223372032559808512);

  const char* expected =
      "lui $v0, 32768\n"
      "ori $v0, $v0, 1\n"
      "dsll32 $v0, $v0, 0\n";
  DriverStr(expected, "LoadConst64IsInt32W31S32b");
}

// These next 16 tests will fail when LoadConst64() exploits "dinsu"
// for cases where the upper 32-bit is equal to the lower 32-bits.
// At that point these tests can be updated.
TEST_F(AssemblerMIPS64Test, LoadConst64OriLeftShiftOri16) {
  __ LoadConst64(mips64::V0, 0x0000FFFF0000FFFF);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dinsu $v0, $v0, 32, 32\n";
  DriverStr(expected, "LoadConst64OriLeftShiftOri16");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriLeftShiftOri15) {
  __ LoadConst64(mips64::V0, 0x0000FFFE0000FFFE);

  const char* expected =
      "ori $v0, $zero, 65534\n"
      "dinsu $v0, $v0, 32, 32\n";
  DriverStr(expected, "LoadConst64OriLeftShiftOri15");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriLeftShiftOri14) {
  __ LoadConst64(mips64::V0, 0x0000FFFC0000FFFC);

  const char* expected =
      "ori $v0, $zero, 65532\n"
      "dinsu $v0, $v0, 32, 32\n";
  DriverStr(expected, "LoadConst64OriLeftShiftOri14");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriLeftShiftOri13) {
  __ LoadConst64(mips64::V0, 0x0000FFF80000FFF8);

  const char* expected =
      "ori $v0, $zero, 65528\n"
      "dinsu $v0, $v0, 32, 32\n";
  DriverStr(expected, "LoadConst64OriLeftShiftOri13");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriLeftShiftOri12) {
  __ LoadConst64(mips64::V0, 0x0000FFF00000FFF0);

  const char* expected =
      "ori $v0, $zero, 65520\n"
      "dinsu $v0, $v0, 32, 32\n";
  DriverStr(expected, "LoadConst64OriLeftShiftOri12");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriLeftShiftOri11) {
  __ LoadConst64(mips64::V0, 0x0000FFE00000FFE0);

  const char* expected =
      "ori $v0, $zero, 65504\n"
      "dinsu $v0, $v0, 32, 32\n";
  DriverStr(expected, "LoadConst64OriLeftShiftOri11");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriLeftShiftOri10) {
  __ LoadConst64(mips64::V0, 0x0000FFC00000FFC0);

  const char* expected =
      "ori $v0, $zero, 65472\n"
      "dinsu $v0, $v0, 32, 32\n";
  DriverStr(expected, "LoadConst64OriLeftShiftOri10");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriLeftShiftOri9) {
  __ LoadConst64(mips64::V0, 0x0000FF800000FF80);

  const char* expected =
      "ori $v0, $zero, 65408\n"
      "dinsu $v0, $v0, 32, 32\n";
  DriverStr(expected, "LoadConst64OriLeftShiftOri9");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriLeftShiftOri8) {
  __ LoadConst64(mips64::V0, 0x0000FF000000FF00);

  const char* expected =
      "ori $v0, $zero, 65280\n"
      "dinsu $v0, $v0, 32, 32\n";
  DriverStr(expected, "LoadConst64OriLeftShiftOri8");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriLeftShiftOri7) {
  __ LoadConst64(mips64::V0, 0x0000FE000000FE00);

  const char* expected =
      "ori $v0, $zero, 65024\n"
      "dinsu $v0, $v0, 32, 32\n";
  DriverStr(expected, "LoadConst64OriLeftShiftOri7");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriLeftShiftOri6) {
  __ LoadConst64(mips64::V0, 0x0000FC000000FC00);

  const char* expected =
      "ori $v0, $zero, 64512\n"
      "dinsu $v0, $v0, 32, 32\n";
  DriverStr(expected, "LoadConst64OriLeftShiftOri6");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriLeftShiftOri5) {
  __ LoadConst64(mips64::V0, 0x0000F8000000F800);

  const char* expected =
      "ori $v0, $zero, 63488\n"
      "dinsu $v0, $v0, 32, 32\n";
  DriverStr(expected, "LoadConst64OriLeftShiftOri5");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriLeftShiftOri4) {
  __ LoadConst64(mips64::V0, 0x0000F0000000F000);

  const char* expected =
      "ori $v0, $zero, 61440\n"
      "dinsu $v0, $v0, 32, 32\n";
  DriverStr(expected, "LoadConst64OriLeftShiftOri4");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriLeftShiftOri3) {
  __ LoadConst64(mips64::V0, 0x0000E0000000E000);

  const char* expected =
      "ori $v0, $zero, 57344\n"
      "dinsu $v0, $v0, 32, 32\n";
  DriverStr(expected, "LoadConst64OriLeftShiftOri3");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriLeftShiftOri2) {
  __ LoadConst64(mips64::V0, 0x0000C0000000C000);

  const char* expected =
      "ori $v0, $zero, 49152\n"
      "dinsu $v0, $v0, 32, 32\n";
  DriverStr(expected, "LoadConst64OriLeftShiftOri2");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriLeftShiftOri1) {
  __ LoadConst64(mips64::V0, 0x0000800000008000);

  const char* expected =
      "ori $v0, $zero, 32768\n"
      "dinsu $v0, $v0, 32, 32\n";
  DriverStr(expected, "LoadConst64OriLeftShiftOri1");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW1S31) {
  __ LoadConst64(mips64::V0, 0x0000000080000001);

  const char* expected =
      "ori $v0, $zero, 1\n"
      "dsll $v0, $v0, 31\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW1S31");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW1S47) {
  __ LoadConst64(mips64::V0, 0x0000800000000001);

  const char* expected =
      "ori $v0, $zero, 1\n"
      "dsll32 $v0, $v0, 15\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW1S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW2S30) {
  __ LoadConst64(mips64::V0, 0x00000000C0000001);

  const char* expected =
      "ori $v0, $zero, 3\n"
      "dsll $v0, $v0, 30\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW2S30");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW2S31) {
  __ LoadConst64(mips64::V0, 0x0000000180000001);

  const char* expected =
      "ori $v0, $zero, 3\n"
      "dsll $v0, $v0, 31\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW2S31");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW2S46) {
  __ LoadConst64(mips64::V0, 0x0000C00000000001);

  const char* expected =
      "ori $v0, $zero, 3\n"
      "dsll32 $v0, $v0, 14\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW2S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW2S47) {
  __ LoadConst64(mips64::V0, 0x0001800000000001);

  const char* expected =
      "ori $v0, $zero, 3\n"
      "dsll32 $v0, $v0, 15\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW2S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW3S29) {
  __ LoadConst64(mips64::V0, 0x00000000E0000001);

  const char* expected =
      "ori $v0, $zero, 7\n"
      "dsll $v0, $v0, 29\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW3S29");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW3S30) {
  __ LoadConst64(mips64::V0, 0x00000001C0000001);

  const char* expected =
      "ori $v0, $zero, 7\n"
      "dsll $v0, $v0, 30\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW3S30");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW3S31) {
  __ LoadConst64(mips64::V0, 0x0000000380000001);

  const char* expected =
      "ori $v0, $zero, 7\n"
      "dsll $v0, $v0, 31\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW3S31");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW3S45) {
  __ LoadConst64(mips64::V0, 0x0000E00000000001);

  const char* expected =
      "ori $v0, $zero, 7\n"
      "dsll32 $v0, $v0, 13\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW3S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW3S46) {
  __ LoadConst64(mips64::V0, 0x0001C00000000001);

  const char* expected =
      "ori $v0, $zero, 7\n"
      "dsll32 $v0, $v0, 14\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW3S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW3S47) {
  __ LoadConst64(mips64::V0, 0x0003800000000001);

  const char* expected =
      "ori $v0, $zero, 7\n"
      "dsll32 $v0, $v0, 15\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW3S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW4S28) {
  __ LoadConst64(mips64::V0, 0x00000000F0000001);

  const char* expected =
      "ori $v0, $zero, 15\n"
      "dsll $v0, $v0, 28\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW4S28");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW4S29) {
  __ LoadConst64(mips64::V0, 0x00000001E0000001);

  const char* expected =
      "ori $v0, $zero, 15\n"
      "dsll $v0, $v0, 29\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW4S29");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW4S30) {
  __ LoadConst64(mips64::V0, 0x00000003C0000001);

  const char* expected =
      "ori $v0, $zero, 15\n"
      "dsll $v0, $v0, 30\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW4S30");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW4S31) {
  __ LoadConst64(mips64::V0, 0x0000000780000001);

  const char* expected =
      "ori $v0, $zero, 15\n"
      "dsll $v0, $v0, 31\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW4S31");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW4S44) {
  __ LoadConst64(mips64::V0, 0x0000F00000000001);

  const char* expected =
      "ori $v0, $zero, 15\n"
      "dsll32 $v0, $v0, 12\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW4S44");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW4S45) {
  __ LoadConst64(mips64::V0, 0x0001E00000000001);

  const char* expected =
      "ori $v0, $zero, 15\n"
      "dsll32 $v0, $v0, 13\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW4S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW4S46) {
  __ LoadConst64(mips64::V0, 0x0003C00000000001);

  const char* expected =
      "ori $v0, $zero, 15\n"
      "dsll32 $v0, $v0, 14\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW4S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW4S47) {
  __ LoadConst64(mips64::V0, 0x0007800000000001);

  const char* expected =
      "ori $v0, $zero, 15\n"
      "dsll32 $v0, $v0, 15\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW4S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW5S27) {
  __ LoadConst64(mips64::V0, 0x00000000F8000001);

  const char* expected =
      "ori $v0, $zero, 31\n"
      "dsll $v0, $v0, 27\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW5S27");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW5S28) {
  __ LoadConst64(mips64::V0, 0x00000001F0000001);

  const char* expected =
      "ori $v0, $zero, 31\n"
      "dsll $v0, $v0, 28\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW5S28");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW5S29) {
  __ LoadConst64(mips64::V0, 0x00000003E0000001);

  const char* expected =
      "ori $v0, $zero, 31\n"
      "dsll $v0, $v0, 29\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW5S29");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW5S30) {
  __ LoadConst64(mips64::V0, 0x00000007C0000001);

  const char* expected =
      "ori $v0, $zero, 31\n"
      "dsll $v0, $v0, 30\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW5S30");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW5S31) {
  __ LoadConst64(mips64::V0, 0x0000000F80000001);

  const char* expected =
      "ori $v0, $zero, 31\n"
      "dsll $v0, $v0, 31\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW5S31");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW5S43) {
  __ LoadConst64(mips64::V0, 0x0000F80000000001);

  const char* expected =
      "ori $v0, $zero, 31\n"
      "dsll32 $v0, $v0, 11\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW5S43");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW5S44) {
  __ LoadConst64(mips64::V0, 0x0001F00000000001);

  const char* expected =
      "ori $v0, $zero, 31\n"
      "dsll32 $v0, $v0, 12\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW5S44");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW5S45) {
  __ LoadConst64(mips64::V0, 0x0003E00000000001);

  const char* expected =
      "ori $v0, $zero, 31\n"
      "dsll32 $v0, $v0, 13\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW5S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW5S46) {
  __ LoadConst64(mips64::V0, 0x0007C00000000001);

  const char* expected =
      "ori $v0, $zero, 31\n"
      "dsll32 $v0, $v0, 14\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW5S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW5S47) {
  __ LoadConst64(mips64::V0, 0x000F800000000001);

  const char* expected =
      "ori $v0, $zero, 31\n"
      "dsll32 $v0, $v0, 15\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW5S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW6S26) {
  __ LoadConst64(mips64::V0, 0x00000000FC000001);

  const char* expected =
      "ori $v0, $zero, 63\n"
      "dsll $v0, $v0, 26\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW6S26");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW6S27) {
  __ LoadConst64(mips64::V0, 0x00000001F8000001);

  const char* expected =
      "ori $v0, $zero, 63\n"
      "dsll $v0, $v0, 27\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW6S27");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW6S28) {
  __ LoadConst64(mips64::V0, 0x00000003F0000001);

  const char* expected =
      "ori $v0, $zero, 63\n"
      "dsll $v0, $v0, 28\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW6S28");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW6S29) {
  __ LoadConst64(mips64::V0, 0x00000007E0000001);

  const char* expected =
      "ori $v0, $zero, 63\n"
      "dsll $v0, $v0, 29\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW6S29");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW6S30) {
  __ LoadConst64(mips64::V0, 0x0000000FC0000001);

  const char* expected =
      "ori $v0, $zero, 63\n"
      "dsll $v0, $v0, 30\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW6S30");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW6S31) {
  __ LoadConst64(mips64::V0, 0x0000001F80000001);

  const char* expected =
      "ori $v0, $zero, 63\n"
      "dsll $v0, $v0, 31\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW6S31");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW6S42) {
  __ LoadConst64(mips64::V0, 0x0000FC0000000001);

  const char* expected =
      "ori $v0, $zero, 63\n"
      "dsll32 $v0, $v0, 10\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW6S42");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW6S43) {
  __ LoadConst64(mips64::V0, 0x0001F80000000001);

  const char* expected =
      "ori $v0, $zero, 63\n"
      "dsll32 $v0, $v0, 11\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW6S43");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW6S44) {
  __ LoadConst64(mips64::V0, 0x0003F00000000001);

  const char* expected =
      "ori $v0, $zero, 63\n"
      "dsll32 $v0, $v0, 12\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW6S44");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW6S45) {
  __ LoadConst64(mips64::V0, 0x0007E00000000001);

  const char* expected =
      "ori $v0, $zero, 63\n"
      "dsll32 $v0, $v0, 13\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW6S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW6S46) {
  __ LoadConst64(mips64::V0, 0x000FC00000000001);

  const char* expected =
      "ori $v0, $zero, 63\n"
      "dsll32 $v0, $v0, 14\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW6S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW6S47) {
  __ LoadConst64(mips64::V0, 0x001F800000000001);

  const char* expected =
      "ori $v0, $zero, 63\n"
      "dsll32 $v0, $v0, 15\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW6S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW7S25) {
  __ LoadConst64(mips64::V0, 0x00000000FE000001);

  const char* expected =
      "ori $v0, $zero, 127\n"
      "dsll $v0, $v0, 25\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW7S25");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW7S26) {
  __ LoadConst64(mips64::V0, 0x00000001FC000001);

  const char* expected =
      "ori $v0, $zero, 127\n"
      "dsll $v0, $v0, 26\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW7S26");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW7S27) {
  __ LoadConst64(mips64::V0, 0x00000003F8000001);

  const char* expected =
      "ori $v0, $zero, 127\n"
      "dsll $v0, $v0, 27\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW7S27");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW7S28) {
  __ LoadConst64(mips64::V0, 0x00000007F0000001);

  const char* expected =
      "ori $v0, $zero, 127\n"
      "dsll $v0, $v0, 28\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW7S28");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW7S29) {
  __ LoadConst64(mips64::V0, 0x0000000FE0000001);

  const char* expected =
      "ori $v0, $zero, 127\n"
      "dsll $v0, $v0, 29\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW7S29");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW7S30) {
  __ LoadConst64(mips64::V0, 0x0000001FC0000001);

  const char* expected =
      "ori $v0, $zero, 127\n"
      "dsll $v0, $v0, 30\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW7S30");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW7S31) {
  __ LoadConst64(mips64::V0, 0x0000003F80000001);

  const char* expected =
      "ori $v0, $zero, 127\n"
      "dsll $v0, $v0, 31\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW7S31");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW7S41) {
  __ LoadConst64(mips64::V0, 0x0000FE0000000001);

  const char* expected =
      "ori $v0, $zero, 127\n"
      "dsll32 $v0, $v0, 9\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW7S41");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW7S42) {
  __ LoadConst64(mips64::V0, 0x0001FC0000000001);

  const char* expected =
      "ori $v0, $zero, 127\n"
      "dsll32 $v0, $v0, 10\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW7S42");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW7S43) {
  __ LoadConst64(mips64::V0, 0x0003F80000000001);

  const char* expected =
      "ori $v0, $zero, 127\n"
      "dsll32 $v0, $v0, 11\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW7S43");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW7S44) {
  __ LoadConst64(mips64::V0, 0x0007F00000000001);

  const char* expected =
      "ori $v0, $zero, 127\n"
      "dsll32 $v0, $v0, 12\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW7S44");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW7S45) {
  __ LoadConst64(mips64::V0, 0x000FE00000000001);

  const char* expected =
      "ori $v0, $zero, 127\n"
      "dsll32 $v0, $v0, 13\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW7S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW7S46) {
  __ LoadConst64(mips64::V0, 0x001FC00000000001);

  const char* expected =
      "ori $v0, $zero, 127\n"
      "dsll32 $v0, $v0, 14\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW7S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW7S47) {
  __ LoadConst64(mips64::V0, 0x003F800000000001);

  const char* expected =
      "ori $v0, $zero, 127\n"
      "dsll32 $v0, $v0, 15\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW7S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW8S24) {
  __ LoadConst64(mips64::V0, 0x00000000FF000001);

  const char* expected =
      "ori $v0, $zero, 255\n"
      "dsll $v0, $v0, 24\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW8S24");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW8S25) {
  __ LoadConst64(mips64::V0, 0x00000001FE000001);

  const char* expected =
      "ori $v0, $zero, 255\n"
      "dsll $v0, $v0, 25\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW8S25");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW8S26) {
  __ LoadConst64(mips64::V0, 0x00000003FC000001);

  const char* expected =
      "ori $v0, $zero, 255\n"
      "dsll $v0, $v0, 26\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW8S26");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW8S27) {
  __ LoadConst64(mips64::V0, 0x00000007F8000001);

  const char* expected =
      "ori $v0, $zero, 255\n"
      "dsll $v0, $v0, 27\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW8S27");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW8S28) {
  __ LoadConst64(mips64::V0, 0x0000000FF0000001);

  const char* expected =
      "ori $v0, $zero, 255\n"
      "dsll $v0, $v0, 28\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW8S28");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW8S29) {
  __ LoadConst64(mips64::V0, 0x0000001FE0000001);

  const char* expected =
      "ori $v0, $zero, 255\n"
      "dsll $v0, $v0, 29\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW8S29");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW8S30) {
  __ LoadConst64(mips64::V0, 0x0000003FC0000001);

  const char* expected =
      "ori $v0, $zero, 255\n"
      "dsll $v0, $v0, 30\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW8S30");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW8S31) {
  __ LoadConst64(mips64::V0, 0x0000007F80000001);

  const char* expected =
      "ori $v0, $zero, 255\n"
      "dsll $v0, $v0, 31\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW8S31");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW8S40) {
  __ LoadConst64(mips64::V0, 0x0000FF0000000001);

  const char* expected =
      "ori $v0, $zero, 255\n"
      "dsll32 $v0, $v0, 8\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW8S40");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW8S41) {
  __ LoadConst64(mips64::V0, 0x0001FE0000000001);

  const char* expected =
      "ori $v0, $zero, 255\n"
      "dsll32 $v0, $v0, 9\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW8S41");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW8S42) {
  __ LoadConst64(mips64::V0, 0x0003FC0000000001);

  const char* expected =
      "ori $v0, $zero, 255\n"
      "dsll32 $v0, $v0, 10\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW8S42");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW8S43) {
  __ LoadConst64(mips64::V0, 0x0007F80000000001);

  const char* expected =
      "ori $v0, $zero, 255\n"
      "dsll32 $v0, $v0, 11\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW8S43");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW8S44) {
  __ LoadConst64(mips64::V0, 0x000FF00000000001);

  const char* expected =
      "ori $v0, $zero, 255\n"
      "dsll32 $v0, $v0, 12\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW8S44");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW8S45) {
  __ LoadConst64(mips64::V0, 0x001FE00000000001);

  const char* expected =
      "ori $v0, $zero, 255\n"
      "dsll32 $v0, $v0, 13\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW8S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW8S46) {
  __ LoadConst64(mips64::V0, 0x003FC00000000001);

  const char* expected =
      "ori $v0, $zero, 255\n"
      "dsll32 $v0, $v0, 14\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW8S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW8S47) {
  __ LoadConst64(mips64::V0, 0x007F800000000001);

  const char* expected =
      "ori $v0, $zero, 255\n"
      "dsll32 $v0, $v0, 15\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW8S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW9S23) {
  __ LoadConst64(mips64::V0, 0x00000000FF800001);

  const char* expected =
      "ori $v0, $zero, 511\n"
      "dsll $v0, $v0, 23\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW9S23");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW9S24) {
  __ LoadConst64(mips64::V0, 0x00000001FF000001);

  const char* expected =
      "ori $v0, $zero, 511\n"
      "dsll $v0, $v0, 24\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW9S24");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW9S25) {
  __ LoadConst64(mips64::V0, 0x00000003FE000001);

  const char* expected =
      "ori $v0, $zero, 511\n"
      "dsll $v0, $v0, 25\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW9S25");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW9S26) {
  __ LoadConst64(mips64::V0, 0x00000007FC000001);

  const char* expected =
      "ori $v0, $zero, 511\n"
      "dsll $v0, $v0, 26\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW9S26");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW9S27) {
  __ LoadConst64(mips64::V0, 0x0000000FF8000001);

  const char* expected =
      "ori $v0, $zero, 511\n"
      "dsll $v0, $v0, 27\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW9S27");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW9S28) {
  __ LoadConst64(mips64::V0, 0x0000001FF0000001);

  const char* expected =
      "ori $v0, $zero, 511\n"
      "dsll $v0, $v0, 28\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW9S28");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW9S29) {
  __ LoadConst64(mips64::V0, 0x0000003FE0000001);

  const char* expected =
      "ori $v0, $zero, 511\n"
      "dsll $v0, $v0, 29\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW9S29");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW9S30) {
  __ LoadConst64(mips64::V0, 0x0000007FC0000001);

  const char* expected =
      "ori $v0, $zero, 511\n"
      "dsll $v0, $v0, 30\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW9S30");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW9S31) {
  __ LoadConst64(mips64::V0, 0x000000FF80000001);

  const char* expected =
      "ori $v0, $zero, 511\n"
      "dsll $v0, $v0, 31\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW9S31");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW9S39) {
  __ LoadConst64(mips64::V0, 0x0000FF8000000001);

  const char* expected =
      "ori $v0, $zero, 511\n"
      "dsll32 $v0, $v0, 7\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW9S39");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW9S40) {
  __ LoadConst64(mips64::V0, 0x0001FF0000000001);

  const char* expected =
      "ori $v0, $zero, 511\n"
      "dsll32 $v0, $v0, 8\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW9S40");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW9S41) {
  __ LoadConst64(mips64::V0, 0x0003FE0000000001);

  const char* expected =
      "ori $v0, $zero, 511\n"
      "dsll32 $v0, $v0, 9\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW9S41");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW9S42) {
  __ LoadConst64(mips64::V0, 0x0007FC0000000001);

  const char* expected =
      "ori $v0, $zero, 511\n"
      "dsll32 $v0, $v0, 10\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW9S42");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW9S43) {
  __ LoadConst64(mips64::V0, 0x000FF80000000001);

  const char* expected =
      "ori $v0, $zero, 511\n"
      "dsll32 $v0, $v0, 11\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW9S43");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW9S44) {
  __ LoadConst64(mips64::V0, 0x001FF00000000001);

  const char* expected =
      "ori $v0, $zero, 511\n"
      "dsll32 $v0, $v0, 12\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW9S44");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW9S45) {
  __ LoadConst64(mips64::V0, 0x003FE00000000001);

  const char* expected =
      "ori $v0, $zero, 511\n"
      "dsll32 $v0, $v0, 13\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW9S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW9S46) {
  __ LoadConst64(mips64::V0, 0x007FC00000000001);

  const char* expected =
      "ori $v0, $zero, 511\n"
      "dsll32 $v0, $v0, 14\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW9S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW9S47) {
  __ LoadConst64(mips64::V0, 0x00FF800000000001);

  const char* expected =
      "ori $v0, $zero, 511\n"
      "dsll32 $v0, $v0, 15\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW9S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW10S22) {
  __ LoadConst64(mips64::V0, 0x00000000FFC00001);

  const char* expected =
      "ori $v0, $zero, 1023\n"
      "dsll $v0, $v0, 22\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW10S22");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW10S23) {
  __ LoadConst64(mips64::V0, 0x00000001FF800001);

  const char* expected =
      "ori $v0, $zero, 1023\n"
      "dsll $v0, $v0, 23\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW10S23");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW10S24) {
  __ LoadConst64(mips64::V0, 0x00000003FF000001);

  const char* expected =
      "ori $v0, $zero, 1023\n"
      "dsll $v0, $v0, 24\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW10S24");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW10S25) {
  __ LoadConst64(mips64::V0, 0x00000007FE000001);

  const char* expected =
      "ori $v0, $zero, 1023\n"
      "dsll $v0, $v0, 25\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW10S25");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW10S26) {
  __ LoadConst64(mips64::V0, 0x0000000FFC000001);

  const char* expected =
      "ori $v0, $zero, 1023\n"
      "dsll $v0, $v0, 26\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW10S26");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW10S27) {
  __ LoadConst64(mips64::V0, 0x0000001FF8000001);

  const char* expected =
      "ori $v0, $zero, 1023\n"
      "dsll $v0, $v0, 27\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW10S27");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW10S28) {
  __ LoadConst64(mips64::V0, 0x0000003FF0000001);

  const char* expected =
      "ori $v0, $zero, 1023\n"
      "dsll $v0, $v0, 28\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW10S28");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW10S29) {
  __ LoadConst64(mips64::V0, 0x0000007FE0000001);

  const char* expected =
      "ori $v0, $zero, 1023\n"
      "dsll $v0, $v0, 29\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW10S29");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW10S30) {
  __ LoadConst64(mips64::V0, 0x000000FFC0000001);

  const char* expected =
      "ori $v0, $zero, 1023\n"
      "dsll $v0, $v0, 30\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW10S30");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW10S31) {
  __ LoadConst64(mips64::V0, 0x000001FF80000001);

  const char* expected =
      "ori $v0, $zero, 1023\n"
      "dsll $v0, $v0, 31\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW10S31");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW10S38) {
  __ LoadConst64(mips64::V0, 0x0000FFC000000001);

  const char* expected =
      "ori $v0, $zero, 1023\n"
      "dsll32 $v0, $v0, 6\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW10S38");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW10S39) {
  __ LoadConst64(mips64::V0, 0x0001FF8000000001);

  const char* expected =
      "ori $v0, $zero, 1023\n"
      "dsll32 $v0, $v0, 7\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW10S39");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW10S40) {
  __ LoadConst64(mips64::V0, 0x0003FF0000000001);

  const char* expected =
      "ori $v0, $zero, 1023\n"
      "dsll32 $v0, $v0, 8\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW10S40");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW10S41) {
  __ LoadConst64(mips64::V0, 0x0007FE0000000001);

  const char* expected =
      "ori $v0, $zero, 1023\n"
      "dsll32 $v0, $v0, 9\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW10S41");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW10S42) {
  __ LoadConst64(mips64::V0, 0x000FFC0000000001);

  const char* expected =
      "ori $v0, $zero, 1023\n"
      "dsll32 $v0, $v0, 10\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW10S42");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW10S43) {
  __ LoadConst64(mips64::V0, 0x001FF80000000001);

  const char* expected =
      "ori $v0, $zero, 1023\n"
      "dsll32 $v0, $v0, 11\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW10S43");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW10S44) {
  __ LoadConst64(mips64::V0, 0x003FF00000000001);

  const char* expected =
      "ori $v0, $zero, 1023\n"
      "dsll32 $v0, $v0, 12\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW10S44");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW10S45) {
  __ LoadConst64(mips64::V0, 0x007FE00000000001);

  const char* expected =
      "ori $v0, $zero, 1023\n"
      "dsll32 $v0, $v0, 13\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW10S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW10S46) {
  __ LoadConst64(mips64::V0, 0x00FFC00000000001);

  const char* expected =
      "ori $v0, $zero, 1023\n"
      "dsll32 $v0, $v0, 14\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW10S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW10S47) {
  __ LoadConst64(mips64::V0, 0x01FF800000000001);

  const char* expected =
      "ori $v0, $zero, 1023\n"
      "dsll32 $v0, $v0, 15\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW10S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW11S21) {
  __ LoadConst64(mips64::V0, 0x00000000FFE00001);

  const char* expected =
      "ori $v0, $zero, 2047\n"
      "dsll $v0, $v0, 21\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW11S21");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW11S22) {
  __ LoadConst64(mips64::V0, 0x00000001FFC00001);

  const char* expected =
      "ori $v0, $zero, 2047\n"
      "dsll $v0, $v0, 22\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW11S22");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW11S23) {
  __ LoadConst64(mips64::V0, 0x00000003FF800001);

  const char* expected =
      "ori $v0, $zero, 2047\n"
      "dsll $v0, $v0, 23\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW11S23");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW11S24) {
  __ LoadConst64(mips64::V0, 0x00000007FF000001);

  const char* expected =
      "ori $v0, $zero, 2047\n"
      "dsll $v0, $v0, 24\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW11S24");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW11S25) {
  __ LoadConst64(mips64::V0, 0x0000000FFE000001);

  const char* expected =
      "ori $v0, $zero, 2047\n"
      "dsll $v0, $v0, 25\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW11S25");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW11S26) {
  __ LoadConst64(mips64::V0, 0x0000001FFC000001);

  const char* expected =
      "ori $v0, $zero, 2047\n"
      "dsll $v0, $v0, 26\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW11S26");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW11S27) {
  __ LoadConst64(mips64::V0, 0x0000003FF8000001);

  const char* expected =
      "ori $v0, $zero, 2047\n"
      "dsll $v0, $v0, 27\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW11S27");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW11S28) {
  __ LoadConst64(mips64::V0, 0x0000007FF0000001);

  const char* expected =
      "ori $v0, $zero, 2047\n"
      "dsll $v0, $v0, 28\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW11S28");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW11S29) {
  __ LoadConst64(mips64::V0, 0x000000FFE0000001);

  const char* expected =
      "ori $v0, $zero, 2047\n"
      "dsll $v0, $v0, 29\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW11S29");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW11S30) {
  __ LoadConst64(mips64::V0, 0x000001FFC0000001);

  const char* expected =
      "ori $v0, $zero, 2047\n"
      "dsll $v0, $v0, 30\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW11S30");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW11S31) {
  __ LoadConst64(mips64::V0, 0x000003FF80000001);

  const char* expected =
      "ori $v0, $zero, 2047\n"
      "dsll $v0, $v0, 31\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW11S31");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW11S37) {
  __ LoadConst64(mips64::V0, 0x0000FFE000000001);

  const char* expected =
      "ori $v0, $zero, 2047\n"
      "dsll32 $v0, $v0, 5\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW11S37");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW11S38) {
  __ LoadConst64(mips64::V0, 0x0001FFC000000001);

  const char* expected =
      "ori $v0, $zero, 2047\n"
      "dsll32 $v0, $v0, 6\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW11S38");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW11S39) {
  __ LoadConst64(mips64::V0, 0x0003FF8000000001);

  const char* expected =
      "ori $v0, $zero, 2047\n"
      "dsll32 $v0, $v0, 7\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW11S39");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW11S40) {
  __ LoadConst64(mips64::V0, 0x0007FF0000000001);

  const char* expected =
      "ori $v0, $zero, 2047\n"
      "dsll32 $v0, $v0, 8\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW11S40");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW11S41) {
  __ LoadConst64(mips64::V0, 0x000FFE0000000001);

  const char* expected =
      "ori $v0, $zero, 2047\n"
      "dsll32 $v0, $v0, 9\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW11S41");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW11S42) {
  __ LoadConst64(mips64::V0, 0x001FFC0000000001);

  const char* expected =
      "ori $v0, $zero, 2047\n"
      "dsll32 $v0, $v0, 10\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW11S42");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW11S43) {
  __ LoadConst64(mips64::V0, 0x003FF80000000001);

  const char* expected =
      "ori $v0, $zero, 2047\n"
      "dsll32 $v0, $v0, 11\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW11S43");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW11S44) {
  __ LoadConst64(mips64::V0, 0x007FF00000000001);

  const char* expected =
      "ori $v0, $zero, 2047\n"
      "dsll32 $v0, $v0, 12\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW11S44");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW11S45) {
  __ LoadConst64(mips64::V0, 0x00FFE00000000001);

  const char* expected =
      "ori $v0, $zero, 2047\n"
      "dsll32 $v0, $v0, 13\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW11S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW11S46) {
  __ LoadConst64(mips64::V0, 0x01FFC00000000001);

  const char* expected =
      "ori $v0, $zero, 2047\n"
      "dsll32 $v0, $v0, 14\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW11S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW11S47) {
  __ LoadConst64(mips64::V0, 0x03FF800000000001);

  const char* expected =
      "ori $v0, $zero, 2047\n"
      "dsll32 $v0, $v0, 15\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW11S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW12S20) {
  __ LoadConst64(mips64::V0, 0x00000000FFF00001);

  const char* expected =
      "ori $v0, $zero, 4095\n"
      "dsll $v0, $v0, 20\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW12S20");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW12S21) {
  __ LoadConst64(mips64::V0, 0x00000001FFE00001);

  const char* expected =
      "ori $v0, $zero, 4095\n"
      "dsll $v0, $v0, 21\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW12S21");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW12S22) {
  __ LoadConst64(mips64::V0, 0x00000003FFC00001);

  const char* expected =
      "ori $v0, $zero, 4095\n"
      "dsll $v0, $v0, 22\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW12S22");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW12S23) {
  __ LoadConst64(mips64::V0, 0x00000007FF800001);

  const char* expected =
      "ori $v0, $zero, 4095\n"
      "dsll $v0, $v0, 23\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW12S23");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW12S24) {
  __ LoadConst64(mips64::V0, 0x0000000FFF000001);

  const char* expected =
      "ori $v0, $zero, 4095\n"
      "dsll $v0, $v0, 24\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW12S24");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW12S25) {
  __ LoadConst64(mips64::V0, 0x0000001FFE000001);

  const char* expected =
      "ori $v0, $zero, 4095\n"
      "dsll $v0, $v0, 25\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW12S25");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW12S26) {
  __ LoadConst64(mips64::V0, 0x0000003FFC000001);

  const char* expected =
      "ori $v0, $zero, 4095\n"
      "dsll $v0, $v0, 26\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW12S26");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW12S27) {
  __ LoadConst64(mips64::V0, 0x0000007FF8000001);

  const char* expected =
      "ori $v0, $zero, 4095\n"
      "dsll $v0, $v0, 27\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW12S27");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW12S28) {
  __ LoadConst64(mips64::V0, 0x000000FFF0000001);

  const char* expected =
      "ori $v0, $zero, 4095\n"
      "dsll $v0, $v0, 28\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW12S28");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW12S29) {
  __ LoadConst64(mips64::V0, 0x000001FFE0000001);

  const char* expected =
      "ori $v0, $zero, 4095\n"
      "dsll $v0, $v0, 29\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW12S29");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW12S30) {
  __ LoadConst64(mips64::V0, 0x000003FFC0000001);

  const char* expected =
      "ori $v0, $zero, 4095\n"
      "dsll $v0, $v0, 30\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW12S30");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW12S31) {
  __ LoadConst64(mips64::V0, 0x000007FF80000001);

  const char* expected =
      "ori $v0, $zero, 4095\n"
      "dsll $v0, $v0, 31\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW12S31");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW12S36) {
  __ LoadConst64(mips64::V0, 0x0000FFF000000001);

  const char* expected =
      "ori $v0, $zero, 4095\n"
      "dsll32 $v0, $v0, 4\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW12S36");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW12S37) {
  __ LoadConst64(mips64::V0, 0x0001FFE000000001);

  const char* expected =
      "ori $v0, $zero, 4095\n"
      "dsll32 $v0, $v0, 5\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW12S37");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW12S38) {
  __ LoadConst64(mips64::V0, 0x0003FFC000000001);

  const char* expected =
      "ori $v0, $zero, 4095\n"
      "dsll32 $v0, $v0, 6\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW12S38");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW12S39) {
  __ LoadConst64(mips64::V0, 0x0007FF8000000001);

  const char* expected =
      "ori $v0, $zero, 4095\n"
      "dsll32 $v0, $v0, 7\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW12S39");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW12S40) {
  __ LoadConst64(mips64::V0, 0x000FFF0000000001);

  const char* expected =
      "ori $v0, $zero, 4095\n"
      "dsll32 $v0, $v0, 8\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW12S40");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW12S41) {
  __ LoadConst64(mips64::V0, 0x001FFE0000000001);

  const char* expected =
      "ori $v0, $zero, 4095\n"
      "dsll32 $v0, $v0, 9\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW12S41");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW12S42) {
  __ LoadConst64(mips64::V0, 0x003FFC0000000001);

  const char* expected =
      "ori $v0, $zero, 4095\n"
      "dsll32 $v0, $v0, 10\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW12S42");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW12S43) {
  __ LoadConst64(mips64::V0, 0x007FF80000000001);

  const char* expected =
      "ori $v0, $zero, 4095\n"
      "dsll32 $v0, $v0, 11\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW12S43");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW12S44) {
  __ LoadConst64(mips64::V0, 0x00FFF00000000001);

  const char* expected =
      "ori $v0, $zero, 4095\n"
      "dsll32 $v0, $v0, 12\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW12S44");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW12S45) {
  __ LoadConst64(mips64::V0, 0x01FFE00000000001);

  const char* expected =
      "ori $v0, $zero, 4095\n"
      "dsll32 $v0, $v0, 13\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW12S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW12S46) {
  __ LoadConst64(mips64::V0, 0x03FFC00000000001);

  const char* expected =
      "ori $v0, $zero, 4095\n"
      "dsll32 $v0, $v0, 14\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW12S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW12S47) {
  __ LoadConst64(mips64::V0, 0x07FF800000000001);

  const char* expected =
      "ori $v0, $zero, 4095\n"
      "dsll32 $v0, $v0, 15\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW12S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW13S19) {
  __ LoadConst64(mips64::V0, 0x00000000FFF80001);

  const char* expected =
      "ori $v0, $zero, 8191\n"
      "dsll $v0, $v0, 19\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW13S19");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW13S20) {
  __ LoadConst64(mips64::V0, 0x00000001FFF00001);

  const char* expected =
      "ori $v0, $zero, 8191\n"
      "dsll $v0, $v0, 20\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW13S20");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW13S21) {
  __ LoadConst64(mips64::V0, 0x00000003FFE00001);

  const char* expected =
      "ori $v0, $zero, 8191\n"
      "dsll $v0, $v0, 21\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW13S21");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW13S22) {
  __ LoadConst64(mips64::V0, 0x00000007FFC00001);

  const char* expected =
      "ori $v0, $zero, 8191\n"
      "dsll $v0, $v0, 22\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW13S22");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW13S23) {
  __ LoadConst64(mips64::V0, 0x0000000FFF800001);

  const char* expected =
      "ori $v0, $zero, 8191\n"
      "dsll $v0, $v0, 23\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW13S23");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW13S24) {
  __ LoadConst64(mips64::V0, 0x0000001FFF000001);

  const char* expected =
      "ori $v0, $zero, 8191\n"
      "dsll $v0, $v0, 24\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW13S24");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW13S25) {
  __ LoadConst64(mips64::V0, 0x0000003FFE000001);

  const char* expected =
      "ori $v0, $zero, 8191\n"
      "dsll $v0, $v0, 25\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW13S25");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW13S26) {
  __ LoadConst64(mips64::V0, 0x0000007FFC000001);

  const char* expected =
      "ori $v0, $zero, 8191\n"
      "dsll $v0, $v0, 26\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW13S26");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW13S27) {
  __ LoadConst64(mips64::V0, 0x000000FFF8000001);

  const char* expected =
      "ori $v0, $zero, 8191\n"
      "dsll $v0, $v0, 27\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW13S27");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW13S28) {
  __ LoadConst64(mips64::V0, 0x000001FFF0000001);

  const char* expected =
      "ori $v0, $zero, 8191\n"
      "dsll $v0, $v0, 28\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW13S28");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW13S29) {
  __ LoadConst64(mips64::V0, 0x000003FFE0000001);

  const char* expected =
      "ori $v0, $zero, 8191\n"
      "dsll $v0, $v0, 29\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW13S29");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW13S30) {
  __ LoadConst64(mips64::V0, 0x000007FFC0000001);

  const char* expected =
      "ori $v0, $zero, 8191\n"
      "dsll $v0, $v0, 30\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW13S30");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW13S31) {
  __ LoadConst64(mips64::V0, 0x00000FFF80000001);

  const char* expected =
      "ori $v0, $zero, 8191\n"
      "dsll $v0, $v0, 31\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW13S31");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW13S35) {
  __ LoadConst64(mips64::V0, 0x0000FFF800000001);

  const char* expected =
      "ori $v0, $zero, 8191\n"
      "dsll32 $v0, $v0, 3\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW13S35");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW13S36) {
  __ LoadConst64(mips64::V0, 0x0001FFF000000001);

  const char* expected =
      "ori $v0, $zero, 8191\n"
      "dsll32 $v0, $v0, 4\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW13S36");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW13S37) {
  __ LoadConst64(mips64::V0, 0x0003FFE000000001);

  const char* expected =
      "ori $v0, $zero, 8191\n"
      "dsll32 $v0, $v0, 5\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW13S37");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW13S38) {
  __ LoadConst64(mips64::V0, 0x0007FFC000000001);

  const char* expected =
      "ori $v0, $zero, 8191\n"
      "dsll32 $v0, $v0, 6\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW13S38");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW13S39) {
  __ LoadConst64(mips64::V0, 0x000FFF8000000001);

  const char* expected =
      "ori $v0, $zero, 8191\n"
      "dsll32 $v0, $v0, 7\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW13S39");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW13S40) {
  __ LoadConst64(mips64::V0, 0x001FFF0000000001);

  const char* expected =
      "ori $v0, $zero, 8191\n"
      "dsll32 $v0, $v0, 8\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW13S40");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW13S41) {
  __ LoadConst64(mips64::V0, 0x003FFE0000000001);

  const char* expected =
      "ori $v0, $zero, 8191\n"
      "dsll32 $v0, $v0, 9\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW13S41");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW13S42) {
  __ LoadConst64(mips64::V0, 0x007FFC0000000001);

  const char* expected =
      "ori $v0, $zero, 8191\n"
      "dsll32 $v0, $v0, 10\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW13S42");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW13S43) {
  __ LoadConst64(mips64::V0, 0x00FFF80000000001);

  const char* expected =
      "ori $v0, $zero, 8191\n"
      "dsll32 $v0, $v0, 11\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW13S43");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW13S44) {
  __ LoadConst64(mips64::V0, 0x01FFF00000000001);

  const char* expected =
      "ori $v0, $zero, 8191\n"
      "dsll32 $v0, $v0, 12\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW13S44");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW13S45) {
  __ LoadConst64(mips64::V0, 0x03FFE00000000001);

  const char* expected =
      "ori $v0, $zero, 8191\n"
      "dsll32 $v0, $v0, 13\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW13S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW13S46) {
  __ LoadConst64(mips64::V0, 0x07FFC00000000001);

  const char* expected =
      "ori $v0, $zero, 8191\n"
      "dsll32 $v0, $v0, 14\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW13S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW13S47) {
  __ LoadConst64(mips64::V0, 0x0FFF800000000001);

  const char* expected =
      "ori $v0, $zero, 8191\n"
      "dsll32 $v0, $v0, 15\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW13S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW14S18) {
  __ LoadConst64(mips64::V0, 0x00000000FFFC0001);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll $v0, $v0, 18\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW14S18");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW14S19) {
  __ LoadConst64(mips64::V0, 0x00000001FFF80001);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll $v0, $v0, 19\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW14S19");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW14S20) {
  __ LoadConst64(mips64::V0, 0x00000003FFF00001);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll $v0, $v0, 20\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW14S20");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW14S21) {
  __ LoadConst64(mips64::V0, 0x00000007FFE00001);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll $v0, $v0, 21\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW14S21");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW14S22) {
  __ LoadConst64(mips64::V0, 0x0000000FFFC00001);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll $v0, $v0, 22\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW14S22");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW14S23) {
  __ LoadConst64(mips64::V0, 0x0000001FFF800001);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll $v0, $v0, 23\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW14S23");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW14S24) {
  __ LoadConst64(mips64::V0, 0x0000003FFF000001);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll $v0, $v0, 24\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW14S24");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW14S25) {
  __ LoadConst64(mips64::V0, 0x0000007FFE000001);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll $v0, $v0, 25\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW14S25");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW14S26) {
  __ LoadConst64(mips64::V0, 0x000000FFFC000001);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll $v0, $v0, 26\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW14S26");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW14S27) {
  __ LoadConst64(mips64::V0, 0x000001FFF8000001);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll $v0, $v0, 27\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW14S27");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW14S28) {
  __ LoadConst64(mips64::V0, 0x000003FFF0000001);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll $v0, $v0, 28\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW14S28");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW14S29) {
  __ LoadConst64(mips64::V0, 0x000007FFE0000001);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll $v0, $v0, 29\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW14S29");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW14S30) {
  __ LoadConst64(mips64::V0, 0x00000FFFC0000001);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll $v0, $v0, 30\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW14S30");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW14S31) {
  __ LoadConst64(mips64::V0, 0x00001FFF80000001);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll $v0, $v0, 31\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW14S31");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW14S34) {
  __ LoadConst64(mips64::V0, 0x0000FFFC00000001);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll32 $v0, $v0, 2\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW14S34");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW14S35) {
  __ LoadConst64(mips64::V0, 0x0001FFF800000001);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll32 $v0, $v0, 3\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW14S35");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW14S36) {
  __ LoadConst64(mips64::V0, 0x0003FFF000000001);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll32 $v0, $v0, 4\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW14S36");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW14S37) {
  __ LoadConst64(mips64::V0, 0x0007FFE000000001);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll32 $v0, $v0, 5\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW14S37");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW14S38) {
  __ LoadConst64(mips64::V0, 0x000FFFC000000001);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll32 $v0, $v0, 6\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW14S38");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW14S39) {
  __ LoadConst64(mips64::V0, 0x001FFF8000000001);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll32 $v0, $v0, 7\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW14S39");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW14S40) {
  __ LoadConst64(mips64::V0, 0x003FFF0000000001);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll32 $v0, $v0, 8\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW14S40");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW14S41) {
  __ LoadConst64(mips64::V0, 0x007FFE0000000001);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll32 $v0, $v0, 9\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW14S41");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW14S42) {
  __ LoadConst64(mips64::V0, 0x00FFFC0000000001);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll32 $v0, $v0, 10\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW14S42");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW14S43) {
  __ LoadConst64(mips64::V0, 0x01FFF80000000001);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll32 $v0, $v0, 11\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW14S43");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW14S44) {
  __ LoadConst64(mips64::V0, 0x03FFF00000000001);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll32 $v0, $v0, 12\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW14S44");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW14S45) {
  __ LoadConst64(mips64::V0, 0x07FFE00000000001);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll32 $v0, $v0, 13\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW14S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW14S46) {
  __ LoadConst64(mips64::V0, 0x0FFFC00000000001);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll32 $v0, $v0, 14\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW14S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW14S47) {
  __ LoadConst64(mips64::V0, 0x1FFF800000000001);

  const char* expected =
      "ori $v0, $zero, 16383\n"
      "dsll32 $v0, $v0, 15\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW14S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW15S17) {
  __ LoadConst64(mips64::V0, 0x00000000FFFE0001);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll $v0, $v0, 17\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW15S17");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW15S18) {
  __ LoadConst64(mips64::V0, 0x00000001FFFC0001);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll $v0, $v0, 18\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW15S18");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW15S19) {
  __ LoadConst64(mips64::V0, 0x00000003FFF80001);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll $v0, $v0, 19\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW15S19");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW15S20) {
  __ LoadConst64(mips64::V0, 0x00000007FFF00001);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll $v0, $v0, 20\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW15S20");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW15S21) {
  __ LoadConst64(mips64::V0, 0x0000000FFFE00001);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll $v0, $v0, 21\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW15S21");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW15S22) {
  __ LoadConst64(mips64::V0, 0x0000001FFFC00001);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll $v0, $v0, 22\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW15S22");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW15S23) {
  __ LoadConst64(mips64::V0, 0x0000003FFF800001);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll $v0, $v0, 23\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW15S23");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW15S24) {
  __ LoadConst64(mips64::V0, 0x0000007FFF000001);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll $v0, $v0, 24\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW15S24");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW15S25) {
  __ LoadConst64(mips64::V0, 0x000000FFFE000001);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll $v0, $v0, 25\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW15S25");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW15S26) {
  __ LoadConst64(mips64::V0, 0x000001FFFC000001);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll $v0, $v0, 26\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW15S26");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW15S27) {
  __ LoadConst64(mips64::V0, 0x000003FFF8000001);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll $v0, $v0, 27\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW15S27");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW15S28) {
  __ LoadConst64(mips64::V0, 0x000007FFF0000001);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll $v0, $v0, 28\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW15S28");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW15S29) {
  __ LoadConst64(mips64::V0, 0x00000FFFE0000001);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll $v0, $v0, 29\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW15S29");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW15S30) {
  __ LoadConst64(mips64::V0, 0x00001FFFC0000001);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll $v0, $v0, 30\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW15S30");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW15S31) {
  __ LoadConst64(mips64::V0, 0x00003FFF80000001);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll $v0, $v0, 31\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW15S31");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW15S33) {
  __ LoadConst64(mips64::V0, 0x0000FFFE00000001);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll32 $v0, $v0, 1\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW15S33");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW15S34) {
  __ LoadConst64(mips64::V0, 0x0001FFFC00000001);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll32 $v0, $v0, 2\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW15S34");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW15S35) {
  __ LoadConst64(mips64::V0, 0x0003FFF800000001);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll32 $v0, $v0, 3\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW15S35");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW15S36) {
  __ LoadConst64(mips64::V0, 0x0007FFF000000001);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll32 $v0, $v0, 4\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW15S36");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW15S37) {
  __ LoadConst64(mips64::V0, 0x000FFFE000000001);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll32 $v0, $v0, 5\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW15S37");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW15S38) {
  __ LoadConst64(mips64::V0, 0x001FFFC000000001);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll32 $v0, $v0, 6\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW15S38");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW15S39) {
  __ LoadConst64(mips64::V0, 0x003FFF8000000001);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll32 $v0, $v0, 7\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW15S39");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW15S40) {
  __ LoadConst64(mips64::V0, 0x007FFF0000000001);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll32 $v0, $v0, 8\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW15S40");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW15S41) {
  __ LoadConst64(mips64::V0, 0x00FFFE0000000001);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll32 $v0, $v0, 9\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW15S41");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW15S42) {
  __ LoadConst64(mips64::V0, 0x01FFFC0000000001);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll32 $v0, $v0, 10\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW15S42");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW15S43) {
  __ LoadConst64(mips64::V0, 0x03FFF80000000001);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll32 $v0, $v0, 11\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW15S43");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW15S44) {
  __ LoadConst64(mips64::V0, 0x07FFF00000000001);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll32 $v0, $v0, 12\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW15S44");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW15S45) {
  __ LoadConst64(mips64::V0, 0x0FFFE00000000001);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll32 $v0, $v0, 13\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW15S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW15S46) {
  __ LoadConst64(mips64::V0, 0x1FFFC00000000001);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll32 $v0, $v0, 14\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW15S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW15S47) {
  __ LoadConst64(mips64::V0, 0x3FFF800000000001);

  const char* expected =
      "ori $v0, $zero, 32767\n"
      "dsll32 $v0, $v0, 15\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW15S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW16S16) {
  __ LoadConst64(mips64::V0, 0x00000000FFFF0001);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll $v0, $v0, 16\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW16S16");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW16S17) {
  __ LoadConst64(mips64::V0, 0x00000001FFFE0001);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll $v0, $v0, 17\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW16S17");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW16S18) {
  __ LoadConst64(mips64::V0, 0x00000003FFFC0001);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll $v0, $v0, 18\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW16S18");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW16S19) {
  __ LoadConst64(mips64::V0, 0x00000007FFF80001);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll $v0, $v0, 19\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW16S19");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW16S20) {
  __ LoadConst64(mips64::V0, 0x0000000FFFF00001);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll $v0, $v0, 20\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW16S20");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW16S21) {
  __ LoadConst64(mips64::V0, 0x0000001FFFE00001);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll $v0, $v0, 21\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW16S21");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW16S22) {
  __ LoadConst64(mips64::V0, 0x0000003FFFC00001);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll $v0, $v0, 22\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW16S22");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW16S23) {
  __ LoadConst64(mips64::V0, 0x0000007FFF800001);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll $v0, $v0, 23\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW16S23");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW16S24) {
  __ LoadConst64(mips64::V0, 0x000000FFFF000001);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll $v0, $v0, 24\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW16S24");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW16S25) {
  __ LoadConst64(mips64::V0, 0x000001FFFE000001);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll $v0, $v0, 25\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW16S25");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW16S26) {
  __ LoadConst64(mips64::V0, 0x000003FFFC000001);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll $v0, $v0, 26\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW16S26");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW16S27) {
  __ LoadConst64(mips64::V0, 0x000007FFF8000001);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll $v0, $v0, 27\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW16S27");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW16S28) {
  __ LoadConst64(mips64::V0, 0x00000FFFF0000001);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll $v0, $v0, 28\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW16S28");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW16S29) {
  __ LoadConst64(mips64::V0, 0x00001FFFE0000001);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll $v0, $v0, 29\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW16S29");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW16S30) {
  __ LoadConst64(mips64::V0, 0x00003FFFC0000001);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll $v0, $v0, 30\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW16S30");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW16S31) {
  __ LoadConst64(mips64::V0, 0x00007FFF80000001);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll $v0, $v0, 31\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW16S31");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW16S32) {
  __ LoadConst64(mips64::V0, 0x0000FFFF00000001);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll32 $v0, $v0, 0\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW16S32");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW16S33) {
  __ LoadConst64(mips64::V0, 0x0001FFFE00000001);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll32 $v0, $v0, 1\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW16S33");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW16S34) {
  __ LoadConst64(mips64::V0, 0x0003FFFC00000001);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll32 $v0, $v0, 2\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW16S34");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW16S35) {
  __ LoadConst64(mips64::V0, 0x0007FFF800000001);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll32 $v0, $v0, 3\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW16S35");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW16S36) {
  __ LoadConst64(mips64::V0, 0x000FFFF000000001);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll32 $v0, $v0, 4\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW16S36");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW16S37) {
  __ LoadConst64(mips64::V0, 0x001FFFE000000001);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll32 $v0, $v0, 5\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW16S37");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW16S38) {
  __ LoadConst64(mips64::V0, 0x003FFFC000000001);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll32 $v0, $v0, 6\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW16S38");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW16S39) {
  __ LoadConst64(mips64::V0, 0x007FFF8000000001);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll32 $v0, $v0, 7\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW16S39");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW16S40) {
  __ LoadConst64(mips64::V0, 0x00FFFF0000000001);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll32 $v0, $v0, 8\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW16S40");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW16S41) {
  __ LoadConst64(mips64::V0, 0x01FFFE0000000001);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll32 $v0, $v0, 9\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW16S41");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW16S42) {
  __ LoadConst64(mips64::V0, 0x03FFFC0000000001);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll32 $v0, $v0, 10\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW16S42");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW16S43) {
  __ LoadConst64(mips64::V0, 0x07FFF80000000001);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll32 $v0, $v0, 11\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW16S43");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW16S44) {
  __ LoadConst64(mips64::V0, 0x0FFFF00000000001);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll32 $v0, $v0, 12\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW16S44");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW16S45) {
  __ LoadConst64(mips64::V0, 0x1FFFE00000000001);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll32 $v0, $v0, 13\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW16S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW16S46) {
  __ LoadConst64(mips64::V0, 0x3FFFC00000000001);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll32 $v0, $v0, 14\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW16S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriShiftOriW16S47) {
  __ LoadConst64(mips64::V0, 0x7FFF800000000001);

  const char* expected =
      "ori $v0, $zero, 65535\n"
      "dsll32 $v0, $v0, 15\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64OriShiftOriW16S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW2S30) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFF40000001);

  const char* expected =
      "daddiu $v0, $zero, -3\n"
      "dsll $v0, $v0, 30\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW2S30");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW2S31) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFE80000001);

  const char* expected =
      "daddiu $v0, $zero, -3\n"
      "dsll $v0, $v0, 31\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW2S31");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW2S46) {
  __ LoadConst64(mips64::V0, 0xFFFF400000000001);

  const char* expected =
      "daddiu $v0, $zero, -3\n"
      "dsll32 $v0, $v0, 14\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW2S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW2S47) {
  __ LoadConst64(mips64::V0, 0xFFFE800000000001);

  const char* expected =
      "daddiu $v0, $zero, -3\n"
      "dsll32 $v0, $v0, 15\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW2S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW3S29) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFF20000001);

  const char* expected =
      "daddiu $v0, $zero, -7\n"
      "dsll $v0, $v0, 29\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW3S29");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW3S30) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFE40000001);

  const char* expected =
      "daddiu $v0, $zero, -7\n"
      "dsll $v0, $v0, 30\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW3S30");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW3S31) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFC80000001);

  const char* expected =
      "daddiu $v0, $zero, -7\n"
      "dsll $v0, $v0, 31\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW3S31");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW3S45) {
  __ LoadConst64(mips64::V0, 0xFFFF200000000001);

  const char* expected =
      "daddiu $v0, $zero, -7\n"
      "dsll32 $v0, $v0, 13\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW3S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW3S46) {
  __ LoadConst64(mips64::V0, 0xFFFE400000000001);

  const char* expected =
      "daddiu $v0, $zero, -7\n"
      "dsll32 $v0, $v0, 14\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW3S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW3S47) {
  __ LoadConst64(mips64::V0, 0xFFFC800000000001);

  const char* expected =
      "daddiu $v0, $zero, -7\n"
      "dsll32 $v0, $v0, 15\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW3S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW4S28) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFF10000001);

  const char* expected =
      "daddiu $v0, $zero, -15\n"
      "dsll $v0, $v0, 28\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW4S28");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW4S29) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFE20000001);

  const char* expected =
      "daddiu $v0, $zero, -15\n"
      "dsll $v0, $v0, 29\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW4S29");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW4S30) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFC40000001);

  const char* expected =
      "daddiu $v0, $zero, -15\n"
      "dsll $v0, $v0, 30\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW4S30");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW4S31) {
  __ LoadConst64(mips64::V0, 0xFFFFFFF880000001);

  const char* expected =
      "daddiu $v0, $zero, -15\n"
      "dsll $v0, $v0, 31\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW4S31");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW4S44) {
  __ LoadConst64(mips64::V0, 0xFFFF100000000001);

  const char* expected =
      "daddiu $v0, $zero, -15\n"
      "dsll32 $v0, $v0, 12\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW4S44");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW4S45) {
  __ LoadConst64(mips64::V0, 0xFFFE200000000001);

  const char* expected =
      "daddiu $v0, $zero, -15\n"
      "dsll32 $v0, $v0, 13\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW4S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW4S46) {
  __ LoadConst64(mips64::V0, 0xFFFC400000000001);

  const char* expected =
      "daddiu $v0, $zero, -15\n"
      "dsll32 $v0, $v0, 14\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW4S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW4S47) {
  __ LoadConst64(mips64::V0, 0xFFF8800000000001);

  const char* expected =
      "daddiu $v0, $zero, -15\n"
      "dsll32 $v0, $v0, 15\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW4S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW5S27) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFF08000001);

  const char* expected =
      "daddiu $v0, $zero, -31\n"
      "dsll $v0, $v0, 27\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW5S27");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW5S28) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFE10000001);

  const char* expected =
      "daddiu $v0, $zero, -31\n"
      "dsll $v0, $v0, 28\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW5S28");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW5S29) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFC20000001);

  const char* expected =
      "daddiu $v0, $zero, -31\n"
      "dsll $v0, $v0, 29\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW5S29");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW5S30) {
  __ LoadConst64(mips64::V0, 0xFFFFFFF840000001);

  const char* expected =
      "daddiu $v0, $zero, -31\n"
      "dsll $v0, $v0, 30\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW5S30");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW5S31) {
  __ LoadConst64(mips64::V0, 0xFFFFFFF080000001);

  const char* expected =
      "daddiu $v0, $zero, -31\n"
      "dsll $v0, $v0, 31\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW5S31");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW5S43) {
  __ LoadConst64(mips64::V0, 0xFFFF080000000001);

  const char* expected =
      "daddiu $v0, $zero, -31\n"
      "dsll32 $v0, $v0, 11\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW5S43");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW5S44) {
  __ LoadConst64(mips64::V0, 0xFFFE100000000001);

  const char* expected =
      "daddiu $v0, $zero, -31\n"
      "dsll32 $v0, $v0, 12\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW5S44");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW5S45) {
  __ LoadConst64(mips64::V0, 0xFFFC200000000001);

  const char* expected =
      "daddiu $v0, $zero, -31\n"
      "dsll32 $v0, $v0, 13\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW5S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW5S46) {
  __ LoadConst64(mips64::V0, 0xFFF8400000000001);

  const char* expected =
      "daddiu $v0, $zero, -31\n"
      "dsll32 $v0, $v0, 14\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW5S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW5S47) {
  __ LoadConst64(mips64::V0, 0xFFF0800000000001);

  const char* expected =
      "daddiu $v0, $zero, -31\n"
      "dsll32 $v0, $v0, 15\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW5S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW6S26) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFF04000001);

  const char* expected =
      "daddiu $v0, $zero, -63\n"
      "dsll $v0, $v0, 26\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW6S26");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW6S27) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFE08000001);

  const char* expected =
      "daddiu $v0, $zero, -63\n"
      "dsll $v0, $v0, 27\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW6S27");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW6S28) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFC10000001);

  const char* expected =
      "daddiu $v0, $zero, -63\n"
      "dsll $v0, $v0, 28\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW6S28");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW6S29) {
  __ LoadConst64(mips64::V0, 0xFFFFFFF820000001);

  const char* expected =
      "daddiu $v0, $zero, -63\n"
      "dsll $v0, $v0, 29\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW6S29");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW6S30) {
  __ LoadConst64(mips64::V0, 0xFFFFFFF040000001);

  const char* expected =
      "daddiu $v0, $zero, -63\n"
      "dsll $v0, $v0, 30\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW6S30");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW6S31) {
  __ LoadConst64(mips64::V0, 0xFFFFFFE080000001);

  const char* expected =
      "daddiu $v0, $zero, -63\n"
      "dsll $v0, $v0, 31\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW6S31");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW6S42) {
  __ LoadConst64(mips64::V0, 0xFFFF040000000001);

  const char* expected =
      "daddiu $v0, $zero, -63\n"
      "dsll32 $v0, $v0, 10\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW6S42");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW6S43) {
  __ LoadConst64(mips64::V0, 0xFFFE080000000001);

  const char* expected =
      "daddiu $v0, $zero, -63\n"
      "dsll32 $v0, $v0, 11\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW6S43");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW6S44) {
  __ LoadConst64(mips64::V0, 0xFFFC100000000001);

  const char* expected =
      "daddiu $v0, $zero, -63\n"
      "dsll32 $v0, $v0, 12\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW6S44");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW6S45) {
  __ LoadConst64(mips64::V0, 0xFFF8200000000001);

  const char* expected =
      "daddiu $v0, $zero, -63\n"
      "dsll32 $v0, $v0, 13\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW6S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW6S46) {
  __ LoadConst64(mips64::V0, 0xFFF0400000000001);

  const char* expected =
      "daddiu $v0, $zero, -63\n"
      "dsll32 $v0, $v0, 14\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW6S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW6S47) {
  __ LoadConst64(mips64::V0, 0xFFE0800000000001);

  const char* expected =
      "daddiu $v0, $zero, -63\n"
      "dsll32 $v0, $v0, 15\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW6S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW7S25) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFF02000001);

  const char* expected =
      "daddiu $v0, $zero, -127\n"
      "dsll $v0, $v0, 25\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW7S25");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW7S26) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFE04000001);

  const char* expected =
      "daddiu $v0, $zero, -127\n"
      "dsll $v0, $v0, 26\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW7S26");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW7S27) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFC08000001);

  const char* expected =
      "daddiu $v0, $zero, -127\n"
      "dsll $v0, $v0, 27\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW7S27");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW7S28) {
  __ LoadConst64(mips64::V0, 0xFFFFFFF810000001);

  const char* expected =
      "daddiu $v0, $zero, -127\n"
      "dsll $v0, $v0, 28\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW7S28");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW7S29) {
  __ LoadConst64(mips64::V0, 0xFFFFFFF020000001);

  const char* expected =
      "daddiu $v0, $zero, -127\n"
      "dsll $v0, $v0, 29\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW7S29");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW7S30) {
  __ LoadConst64(mips64::V0, 0xFFFFFFE040000001);

  const char* expected =
      "daddiu $v0, $zero, -127\n"
      "dsll $v0, $v0, 30\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW7S30");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW7S31) {
  __ LoadConst64(mips64::V0, 0xFFFFFFC080000001);

  const char* expected =
      "daddiu $v0, $zero, -127\n"
      "dsll $v0, $v0, 31\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW7S31");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW7S41) {
  __ LoadConst64(mips64::V0, 0xFFFF020000000001);

  const char* expected =
      "daddiu $v0, $zero, -127\n"
      "dsll32 $v0, $v0, 9\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW7S41");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW7S42) {
  __ LoadConst64(mips64::V0, 0xFFFE040000000001);

  const char* expected =
      "daddiu $v0, $zero, -127\n"
      "dsll32 $v0, $v0, 10\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW7S42");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW7S43) {
  __ LoadConst64(mips64::V0, 0xFFFC080000000001);

  const char* expected =
      "daddiu $v0, $zero, -127\n"
      "dsll32 $v0, $v0, 11\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW7S43");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW7S44) {
  __ LoadConst64(mips64::V0, 0xFFF8100000000001);

  const char* expected =
      "daddiu $v0, $zero, -127\n"
      "dsll32 $v0, $v0, 12\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW7S44");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW7S45) {
  __ LoadConst64(mips64::V0, 0xFFF0200000000001);

  const char* expected =
      "daddiu $v0, $zero, -127\n"
      "dsll32 $v0, $v0, 13\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW7S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW7S46) {
  __ LoadConst64(mips64::V0, 0xFFE0400000000001);

  const char* expected =
      "daddiu $v0, $zero, -127\n"
      "dsll32 $v0, $v0, 14\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW7S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW7S47) {
  __ LoadConst64(mips64::V0, 0xFFC0800000000001);

  const char* expected =
      "daddiu $v0, $zero, -127\n"
      "dsll32 $v0, $v0, 15\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW7S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW8S24) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFF01000001);

  const char* expected =
      "daddiu $v0, $zero, -255\n"
      "dsll $v0, $v0, 24\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW8S24");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW8S25) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFE02000001);

  const char* expected =
      "daddiu $v0, $zero, -255\n"
      "dsll $v0, $v0, 25\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW8S25");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW8S26) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFC04000001);

  const char* expected =
      "daddiu $v0, $zero, -255\n"
      "dsll $v0, $v0, 26\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW8S26");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW8S27) {
  __ LoadConst64(mips64::V0, 0xFFFFFFF808000001);

  const char* expected =
      "daddiu $v0, $zero, -255\n"
      "dsll $v0, $v0, 27\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW8S27");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW8S28) {
  __ LoadConst64(mips64::V0, 0xFFFFFFF010000001);

  const char* expected =
      "daddiu $v0, $zero, -255\n"
      "dsll $v0, $v0, 28\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW8S28");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW8S29) {
  __ LoadConst64(mips64::V0, 0xFFFFFFE020000001);

  const char* expected =
      "daddiu $v0, $zero, -255\n"
      "dsll $v0, $v0, 29\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW8S29");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW8S30) {
  __ LoadConst64(mips64::V0, 0xFFFFFFC040000001);

  const char* expected =
      "daddiu $v0, $zero, -255\n"
      "dsll $v0, $v0, 30\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW8S30");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW8S31) {
  __ LoadConst64(mips64::V0, 0xFFFFFF8080000001);

  const char* expected =
      "daddiu $v0, $zero, -255\n"
      "dsll $v0, $v0, 31\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW8S31");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW8S40) {
  __ LoadConst64(mips64::V0, 0xFFFF010000000001);

  const char* expected =
      "daddiu $v0, $zero, -255\n"
      "dsll32 $v0, $v0, 8\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW8S40");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW8S41) {
  __ LoadConst64(mips64::V0, 0xFFFE020000000001);

  const char* expected =
      "daddiu $v0, $zero, -255\n"
      "dsll32 $v0, $v0, 9\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW8S41");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW8S42) {
  __ LoadConst64(mips64::V0, 0xFFFC040000000001);

  const char* expected =
      "daddiu $v0, $zero, -255\n"
      "dsll32 $v0, $v0, 10\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW8S42");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW8S43) {
  __ LoadConst64(mips64::V0, 0xFFF8080000000001);

  const char* expected =
      "daddiu $v0, $zero, -255\n"
      "dsll32 $v0, $v0, 11\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW8S43");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW8S44) {
  __ LoadConst64(mips64::V0, 0xFFF0100000000001);

  const char* expected =
      "daddiu $v0, $zero, -255\n"
      "dsll32 $v0, $v0, 12\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW8S44");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW8S45) {
  __ LoadConst64(mips64::V0, 0xFFE0200000000001);

  const char* expected =
      "daddiu $v0, $zero, -255\n"
      "dsll32 $v0, $v0, 13\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW8S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW8S46) {
  __ LoadConst64(mips64::V0, 0xFFC0400000000001);

  const char* expected =
      "daddiu $v0, $zero, -255\n"
      "dsll32 $v0, $v0, 14\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW8S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW8S47) {
  __ LoadConst64(mips64::V0, 0xFF80800000000001);

  const char* expected =
      "daddiu $v0, $zero, -255\n"
      "dsll32 $v0, $v0, 15\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW8S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW9S23) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFF00800001);

  const char* expected =
      "daddiu $v0, $zero, -511\n"
      "dsll $v0, $v0, 23\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW9S23");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW9S24) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFE01000001);

  const char* expected =
      "daddiu $v0, $zero, -511\n"
      "dsll $v0, $v0, 24\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW9S24");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW9S25) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFC02000001);

  const char* expected =
      "daddiu $v0, $zero, -511\n"
      "dsll $v0, $v0, 25\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW9S25");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW9S26) {
  __ LoadConst64(mips64::V0, 0xFFFFFFF804000001);

  const char* expected =
      "daddiu $v0, $zero, -511\n"
      "dsll $v0, $v0, 26\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW9S26");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW9S27) {
  __ LoadConst64(mips64::V0, 0xFFFFFFF008000001);

  const char* expected =
      "daddiu $v0, $zero, -511\n"
      "dsll $v0, $v0, 27\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW9S27");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW9S28) {
  __ LoadConst64(mips64::V0, 0xFFFFFFE010000001);

  const char* expected =
      "daddiu $v0, $zero, -511\n"
      "dsll $v0, $v0, 28\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW9S28");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW9S29) {
  __ LoadConst64(mips64::V0, 0xFFFFFFC020000001);

  const char* expected =
      "daddiu $v0, $zero, -511\n"
      "dsll $v0, $v0, 29\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW9S29");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW9S30) {
  __ LoadConst64(mips64::V0, 0xFFFFFF8040000001);

  const char* expected =
      "daddiu $v0, $zero, -511\n"
      "dsll $v0, $v0, 30\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW9S30");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW9S31) {
  __ LoadConst64(mips64::V0, 0xFFFFFF0080000001);

  const char* expected =
      "daddiu $v0, $zero, -511\n"
      "dsll $v0, $v0, 31\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW9S31");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW9S39) {
  __ LoadConst64(mips64::V0, 0xFFFF008000000001);

  const char* expected =
      "daddiu $v0, $zero, -511\n"
      "dsll32 $v0, $v0, 7\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW9S39");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW9S40) {
  __ LoadConst64(mips64::V0, 0xFFFE010000000001);

  const char* expected =
      "daddiu $v0, $zero, -511\n"
      "dsll32 $v0, $v0, 8\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW9S40");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW9S41) {
  __ LoadConst64(mips64::V0, 0xFFFC020000000001);

  const char* expected =
      "daddiu $v0, $zero, -511\n"
      "dsll32 $v0, $v0, 9\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW9S41");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW9S42) {
  __ LoadConst64(mips64::V0, 0xFFF8040000000001);

  const char* expected =
      "daddiu $v0, $zero, -511\n"
      "dsll32 $v0, $v0, 10\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW9S42");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW9S43) {
  __ LoadConst64(mips64::V0, 0xFFF0080000000001);

  const char* expected =
      "daddiu $v0, $zero, -511\n"
      "dsll32 $v0, $v0, 11\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW9S43");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW9S44) {
  __ LoadConst64(mips64::V0, 0xFFE0100000000001);

  const char* expected =
      "daddiu $v0, $zero, -511\n"
      "dsll32 $v0, $v0, 12\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW9S44");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW9S45) {
  __ LoadConst64(mips64::V0, 0xFFC0200000000001);

  const char* expected =
      "daddiu $v0, $zero, -511\n"
      "dsll32 $v0, $v0, 13\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW9S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW9S46) {
  __ LoadConst64(mips64::V0, 0xFF80400000000001);

  const char* expected =
      "daddiu $v0, $zero, -511\n"
      "dsll32 $v0, $v0, 14\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW9S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW9S47) {
  __ LoadConst64(mips64::V0, 0xFF00800000000001);

  const char* expected =
      "daddiu $v0, $zero, -511\n"
      "dsll32 $v0, $v0, 15\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW9S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW10S22) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFF00400001);

  const char* expected =
      "daddiu $v0, $zero, -1023\n"
      "dsll $v0, $v0, 22\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW10S22");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW10S23) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFE00800001);

  const char* expected =
      "daddiu $v0, $zero, -1023\n"
      "dsll $v0, $v0, 23\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW10S23");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW10S24) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFC01000001);

  const char* expected =
      "daddiu $v0, $zero, -1023\n"
      "dsll $v0, $v0, 24\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW10S24");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW10S25) {
  __ LoadConst64(mips64::V0, 0xFFFFFFF802000001);

  const char* expected =
      "daddiu $v0, $zero, -1023\n"
      "dsll $v0, $v0, 25\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW10S25");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW10S26) {
  __ LoadConst64(mips64::V0, 0xFFFFFFF004000001);

  const char* expected =
      "daddiu $v0, $zero, -1023\n"
      "dsll $v0, $v0, 26\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW10S26");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW10S27) {
  __ LoadConst64(mips64::V0, 0xFFFFFFE008000001);

  const char* expected =
      "daddiu $v0, $zero, -1023\n"
      "dsll $v0, $v0, 27\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW10S27");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW10S28) {
  __ LoadConst64(mips64::V0, 0xFFFFFFC010000001);

  const char* expected =
      "daddiu $v0, $zero, -1023\n"
      "dsll $v0, $v0, 28\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW10S28");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW10S29) {
  __ LoadConst64(mips64::V0, 0xFFFFFF8020000001);

  const char* expected =
      "daddiu $v0, $zero, -1023\n"
      "dsll $v0, $v0, 29\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW10S29");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW10S30) {
  __ LoadConst64(mips64::V0, 0xFFFFFF0040000001);

  const char* expected =
      "daddiu $v0, $zero, -1023\n"
      "dsll $v0, $v0, 30\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW10S30");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW10S31) {
  __ LoadConst64(mips64::V0, 0xFFFFFE0080000001);

  const char* expected =
      "daddiu $v0, $zero, -1023\n"
      "dsll $v0, $v0, 31\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW10S31");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW10S38) {
  __ LoadConst64(mips64::V0, 0xFFFF004000000001);

  const char* expected =
      "daddiu $v0, $zero, -1023\n"
      "dsll32 $v0, $v0, 6\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW10S38");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW10S39) {
  __ LoadConst64(mips64::V0, 0xFFFE008000000001);

  const char* expected =
      "daddiu $v0, $zero, -1023\n"
      "dsll32 $v0, $v0, 7\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW10S39");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW10S40) {
  __ LoadConst64(mips64::V0, 0xFFFC010000000001);

  const char* expected =
      "daddiu $v0, $zero, -1023\n"
      "dsll32 $v0, $v0, 8\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW10S40");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW10S41) {
  __ LoadConst64(mips64::V0, 0xFFF8020000000001);

  const char* expected =
      "daddiu $v0, $zero, -1023\n"
      "dsll32 $v0, $v0, 9\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW10S41");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW10S42) {
  __ LoadConst64(mips64::V0, 0xFFF0040000000001);

  const char* expected =
      "daddiu $v0, $zero, -1023\n"
      "dsll32 $v0, $v0, 10\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW10S42");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW10S43) {
  __ LoadConst64(mips64::V0, 0xFFE0080000000001);

  const char* expected =
      "daddiu $v0, $zero, -1023\n"
      "dsll32 $v0, $v0, 11\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW10S43");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW10S44) {
  __ LoadConst64(mips64::V0, 0xFFC0100000000001);

  const char* expected =
      "daddiu $v0, $zero, -1023\n"
      "dsll32 $v0, $v0, 12\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW10S44");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW10S45) {
  __ LoadConst64(mips64::V0, 0xFF80200000000001);

  const char* expected =
      "daddiu $v0, $zero, -1023\n"
      "dsll32 $v0, $v0, 13\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW10S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW10S46) {
  __ LoadConst64(mips64::V0, 0xFF00400000000001);

  const char* expected =
      "daddiu $v0, $zero, -1023\n"
      "dsll32 $v0, $v0, 14\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW10S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW10S47) {
  __ LoadConst64(mips64::V0, 0xFE00800000000001);

  const char* expected =
      "daddiu $v0, $zero, -1023\n"
      "dsll32 $v0, $v0, 15\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW10S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW11S21) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFF00200001);

  const char* expected =
      "daddiu $v0, $zero, -2047\n"
      "dsll $v0, $v0, 21\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW11S21");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW11S22) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFE00400001);

  const char* expected =
      "daddiu $v0, $zero, -2047\n"
      "dsll $v0, $v0, 22\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW11S22");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW11S23) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFC00800001);

  const char* expected =
      "daddiu $v0, $zero, -2047\n"
      "dsll $v0, $v0, 23\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW11S23");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW11S24) {
  __ LoadConst64(mips64::V0, 0xFFFFFFF801000001);

  const char* expected =
      "daddiu $v0, $zero, -2047\n"
      "dsll $v0, $v0, 24\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW11S24");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW11S25) {
  __ LoadConst64(mips64::V0, 0xFFFFFFF002000001);

  const char* expected =
      "daddiu $v0, $zero, -2047\n"
      "dsll $v0, $v0, 25\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW11S25");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW11S26) {
  __ LoadConst64(mips64::V0, 0xFFFFFFE004000001);

  const char* expected =
      "daddiu $v0, $zero, -2047\n"
      "dsll $v0, $v0, 26\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW11S26");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW11S27) {
  __ LoadConst64(mips64::V0, 0xFFFFFFC008000001);

  const char* expected =
      "daddiu $v0, $zero, -2047\n"
      "dsll $v0, $v0, 27\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW11S27");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW11S28) {
  __ LoadConst64(mips64::V0, 0xFFFFFF8010000001);

  const char* expected =
      "daddiu $v0, $zero, -2047\n"
      "dsll $v0, $v0, 28\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW11S28");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW11S29) {
  __ LoadConst64(mips64::V0, 0xFFFFFF0020000001);

  const char* expected =
      "daddiu $v0, $zero, -2047\n"
      "dsll $v0, $v0, 29\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW11S29");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW11S30) {
  __ LoadConst64(mips64::V0, 0xFFFFFE0040000001);

  const char* expected =
      "daddiu $v0, $zero, -2047\n"
      "dsll $v0, $v0, 30\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW11S30");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW11S31) {
  __ LoadConst64(mips64::V0, 0xFFFFFC0080000001);

  const char* expected =
      "daddiu $v0, $zero, -2047\n"
      "dsll $v0, $v0, 31\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW11S31");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW11S37) {
  __ LoadConst64(mips64::V0, 0xFFFF002000000001);

  const char* expected =
      "daddiu $v0, $zero, -2047\n"
      "dsll32 $v0, $v0, 5\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW11S37");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW11S38) {
  __ LoadConst64(mips64::V0, 0xFFFE004000000001);

  const char* expected =
      "daddiu $v0, $zero, -2047\n"
      "dsll32 $v0, $v0, 6\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW11S38");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW11S39) {
  __ LoadConst64(mips64::V0, 0xFFFC008000000001);

  const char* expected =
      "daddiu $v0, $zero, -2047\n"
      "dsll32 $v0, $v0, 7\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW11S39");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW11S40) {
  __ LoadConst64(mips64::V0, 0xFFF8010000000001);

  const char* expected =
      "daddiu $v0, $zero, -2047\n"
      "dsll32 $v0, $v0, 8\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW11S40");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW11S41) {
  __ LoadConst64(mips64::V0, 0xFFF0020000000001);

  const char* expected =
      "daddiu $v0, $zero, -2047\n"
      "dsll32 $v0, $v0, 9\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW11S41");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW11S42) {
  __ LoadConst64(mips64::V0, 0xFFE0040000000001);

  const char* expected =
      "daddiu $v0, $zero, -2047\n"
      "dsll32 $v0, $v0, 10\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW11S42");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW11S43) {
  __ LoadConst64(mips64::V0, 0xFFC0080000000001);

  const char* expected =
      "daddiu $v0, $zero, -2047\n"
      "dsll32 $v0, $v0, 11\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW11S43");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW11S44) {
  __ LoadConst64(mips64::V0, 0xFF80100000000001);

  const char* expected =
      "daddiu $v0, $zero, -2047\n"
      "dsll32 $v0, $v0, 12\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW11S44");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW11S45) {
  __ LoadConst64(mips64::V0, 0xFF00200000000001);

  const char* expected =
      "daddiu $v0, $zero, -2047\n"
      "dsll32 $v0, $v0, 13\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW11S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW11S46) {
  __ LoadConst64(mips64::V0, 0xFE00400000000001);

  const char* expected =
      "daddiu $v0, $zero, -2047\n"
      "dsll32 $v0, $v0, 14\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW11S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW11S47) {
  __ LoadConst64(mips64::V0, 0xFC00800000000001);

  const char* expected =
      "daddiu $v0, $zero, -2047\n"
      "dsll32 $v0, $v0, 15\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW11S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW12S20) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFF00100001);

  const char* expected =
      "daddiu $v0, $zero, -4095\n"
      "dsll $v0, $v0, 20\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW12S20");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW12S21) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFE00200001);

  const char* expected =
      "daddiu $v0, $zero, -4095\n"
      "dsll $v0, $v0, 21\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW12S21");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW12S22) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFC00400001);

  const char* expected =
      "daddiu $v0, $zero, -4095\n"
      "dsll $v0, $v0, 22\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW12S22");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW12S23) {
  __ LoadConst64(mips64::V0, 0xFFFFFFF800800001);

  const char* expected =
      "daddiu $v0, $zero, -4095\n"
      "dsll $v0, $v0, 23\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW12S23");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW12S24) {
  __ LoadConst64(mips64::V0, 0xFFFFFFF001000001);

  const char* expected =
      "daddiu $v0, $zero, -4095\n"
      "dsll $v0, $v0, 24\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW12S24");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW12S25) {
  __ LoadConst64(mips64::V0, 0xFFFFFFE002000001);

  const char* expected =
      "daddiu $v0, $zero, -4095\n"
      "dsll $v0, $v0, 25\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW12S25");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW12S26) {
  __ LoadConst64(mips64::V0, 0xFFFFFFC004000001);

  const char* expected =
      "daddiu $v0, $zero, -4095\n"
      "dsll $v0, $v0, 26\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW12S26");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW12S27) {
  __ LoadConst64(mips64::V0, 0xFFFFFF8008000001);

  const char* expected =
      "daddiu $v0, $zero, -4095\n"
      "dsll $v0, $v0, 27\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW12S27");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW12S28) {
  __ LoadConst64(mips64::V0, 0xFFFFFF0010000001);

  const char* expected =
      "daddiu $v0, $zero, -4095\n"
      "dsll $v0, $v0, 28\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW12S28");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW12S29) {
  __ LoadConst64(mips64::V0, 0xFFFFFE0020000001);

  const char* expected =
      "daddiu $v0, $zero, -4095\n"
      "dsll $v0, $v0, 29\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW12S29");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW12S30) {
  __ LoadConst64(mips64::V0, 0xFFFFFC0040000001);

  const char* expected =
      "daddiu $v0, $zero, -4095\n"
      "dsll $v0, $v0, 30\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW12S30");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW12S31) {
  __ LoadConst64(mips64::V0, 0xFFFFF80080000001);

  const char* expected =
      "daddiu $v0, $zero, -4095\n"
      "dsll $v0, $v0, 31\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW12S31");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW12S36) {
  __ LoadConst64(mips64::V0, 0xFFFF001000000001);

  const char* expected =
      "daddiu $v0, $zero, -4095\n"
      "dsll32 $v0, $v0, 4\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW12S36");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW12S37) {
  __ LoadConst64(mips64::V0, 0xFFFE002000000001);

  const char* expected =
      "daddiu $v0, $zero, -4095\n"
      "dsll32 $v0, $v0, 5\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW12S37");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW12S38) {
  __ LoadConst64(mips64::V0, 0xFFFC004000000001);

  const char* expected =
      "daddiu $v0, $zero, -4095\n"
      "dsll32 $v0, $v0, 6\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW12S38");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW12S39) {
  __ LoadConst64(mips64::V0, 0xFFF8008000000001);

  const char* expected =
      "daddiu $v0, $zero, -4095\n"
      "dsll32 $v0, $v0, 7\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW12S39");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW12S40) {
  __ LoadConst64(mips64::V0, 0xFFF0010000000001);

  const char* expected =
      "daddiu $v0, $zero, -4095\n"
      "dsll32 $v0, $v0, 8\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW12S40");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW12S41) {
  __ LoadConst64(mips64::V0, 0xFFE0020000000001);

  const char* expected =
      "daddiu $v0, $zero, -4095\n"
      "dsll32 $v0, $v0, 9\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW12S41");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW12S42) {
  __ LoadConst64(mips64::V0, 0xFFC0040000000001);

  const char* expected =
      "daddiu $v0, $zero, -4095\n"
      "dsll32 $v0, $v0, 10\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW12S42");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW12S43) {
  __ LoadConst64(mips64::V0, 0xFF80080000000001);

  const char* expected =
      "daddiu $v0, $zero, -4095\n"
      "dsll32 $v0, $v0, 11\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW12S43");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW12S44) {
  __ LoadConst64(mips64::V0, 0xFF00100000000001);

  const char* expected =
      "daddiu $v0, $zero, -4095\n"
      "dsll32 $v0, $v0, 12\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW12S44");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW12S45) {
  __ LoadConst64(mips64::V0, 0xFE00200000000001);

  const char* expected =
      "daddiu $v0, $zero, -4095\n"
      "dsll32 $v0, $v0, 13\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW12S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW12S46) {
  __ LoadConst64(mips64::V0, 0xFC00400000000001);

  const char* expected =
      "daddiu $v0, $zero, -4095\n"
      "dsll32 $v0, $v0, 14\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW12S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW12S47) {
  __ LoadConst64(mips64::V0, 0xF800800000000001);

  const char* expected =
      "daddiu $v0, $zero, -4095\n"
      "dsll32 $v0, $v0, 15\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW12S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW13S19) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFF00080001);

  const char* expected =
      "daddiu $v0, $zero, -8191\n"
      "dsll $v0, $v0, 19\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW13S19");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW13S20) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFE00100001);

  const char* expected =
      "daddiu $v0, $zero, -8191\n"
      "dsll $v0, $v0, 20\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW13S20");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW13S21) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFC00200001);

  const char* expected =
      "daddiu $v0, $zero, -8191\n"
      "dsll $v0, $v0, 21\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW13S21");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW13S22) {
  __ LoadConst64(mips64::V0, 0xFFFFFFF800400001);

  const char* expected =
      "daddiu $v0, $zero, -8191\n"
      "dsll $v0, $v0, 22\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW13S22");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW13S23) {
  __ LoadConst64(mips64::V0, 0xFFFFFFF000800001);

  const char* expected =
      "daddiu $v0, $zero, -8191\n"
      "dsll $v0, $v0, 23\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW13S23");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW13S24) {
  __ LoadConst64(mips64::V0, 0xFFFFFFE001000001);

  const char* expected =
      "daddiu $v0, $zero, -8191\n"
      "dsll $v0, $v0, 24\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW13S24");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW13S25) {
  __ LoadConst64(mips64::V0, 0xFFFFFFC002000001);

  const char* expected =
      "daddiu $v0, $zero, -8191\n"
      "dsll $v0, $v0, 25\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW13S25");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW13S26) {
  __ LoadConst64(mips64::V0, 0xFFFFFF8004000001);

  const char* expected =
      "daddiu $v0, $zero, -8191\n"
      "dsll $v0, $v0, 26\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW13S26");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW13S27) {
  __ LoadConst64(mips64::V0, 0xFFFFFF0008000001);

  const char* expected =
      "daddiu $v0, $zero, -8191\n"
      "dsll $v0, $v0, 27\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW13S27");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW13S28) {
  __ LoadConst64(mips64::V0, 0xFFFFFE0010000001);

  const char* expected =
      "daddiu $v0, $zero, -8191\n"
      "dsll $v0, $v0, 28\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW13S28");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW13S29) {
  __ LoadConst64(mips64::V0, 0xFFFFFC0020000001);

  const char* expected =
      "daddiu $v0, $zero, -8191\n"
      "dsll $v0, $v0, 29\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW13S29");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW13S30) {
  __ LoadConst64(mips64::V0, 0xFFFFF80040000001);

  const char* expected =
      "daddiu $v0, $zero, -8191\n"
      "dsll $v0, $v0, 30\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW13S30");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW13S31) {
  __ LoadConst64(mips64::V0, 0xFFFFF00080000001);

  const char* expected =
      "daddiu $v0, $zero, -8191\n"
      "dsll $v0, $v0, 31\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW13S31");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW13S35) {
  __ LoadConst64(mips64::V0, 0xFFFF000800000001);

  const char* expected =
      "daddiu $v0, $zero, -8191\n"
      "dsll32 $v0, $v0, 3\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW13S35");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW13S36) {
  __ LoadConst64(mips64::V0, 0xFFFE001000000001);

  const char* expected =
      "daddiu $v0, $zero, -8191\n"
      "dsll32 $v0, $v0, 4\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW13S36");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW13S37) {
  __ LoadConst64(mips64::V0, 0xFFFC002000000001);

  const char* expected =
      "daddiu $v0, $zero, -8191\n"
      "dsll32 $v0, $v0, 5\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW13S37");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW13S38) {
  __ LoadConst64(mips64::V0, 0xFFF8004000000001);

  const char* expected =
      "daddiu $v0, $zero, -8191\n"
      "dsll32 $v0, $v0, 6\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW13S38");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW13S39) {
  __ LoadConst64(mips64::V0, 0xFFF0008000000001);

  const char* expected =
      "daddiu $v0, $zero, -8191\n"
      "dsll32 $v0, $v0, 7\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW13S39");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW13S40) {
  __ LoadConst64(mips64::V0, 0xFFE0010000000001);

  const char* expected =
      "daddiu $v0, $zero, -8191\n"
      "dsll32 $v0, $v0, 8\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW13S40");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW13S41) {
  __ LoadConst64(mips64::V0, 0xFFC0020000000001);

  const char* expected =
      "daddiu $v0, $zero, -8191\n"
      "dsll32 $v0, $v0, 9\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW13S41");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW13S42) {
  __ LoadConst64(mips64::V0, 0xFF80040000000001);

  const char* expected =
      "daddiu $v0, $zero, -8191\n"
      "dsll32 $v0, $v0, 10\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW13S42");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW13S43) {
  __ LoadConst64(mips64::V0, 0xFF00080000000001);

  const char* expected =
      "daddiu $v0, $zero, -8191\n"
      "dsll32 $v0, $v0, 11\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW13S43");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW13S44) {
  __ LoadConst64(mips64::V0, 0xFE00100000000001);

  const char* expected =
      "daddiu $v0, $zero, -8191\n"
      "dsll32 $v0, $v0, 12\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW13S44");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW13S45) {
  __ LoadConst64(mips64::V0, 0xFC00200000000001);

  const char* expected =
      "daddiu $v0, $zero, -8191\n"
      "dsll32 $v0, $v0, 13\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW13S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW13S46) {
  __ LoadConst64(mips64::V0, 0xF800400000000001);

  const char* expected =
      "daddiu $v0, $zero, -8191\n"
      "dsll32 $v0, $v0, 14\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW13S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW13S47) {
  __ LoadConst64(mips64::V0, 0xF000800000000001);

  const char* expected =
      "daddiu $v0, $zero, -8191\n"
      "dsll32 $v0, $v0, 15\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW13S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW14S18) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFF00040001);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll $v0, $v0, 18\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW14S18");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW14S19) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFE00080001);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll $v0, $v0, 19\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW14S19");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW14S20) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFC00100001);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll $v0, $v0, 20\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW14S20");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW14S21) {
  __ LoadConst64(mips64::V0, 0xFFFFFFF800200001);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll $v0, $v0, 21\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW14S21");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW14S22) {
  __ LoadConst64(mips64::V0, 0xFFFFFFF000400001);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll $v0, $v0, 22\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW14S22");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW14S23) {
  __ LoadConst64(mips64::V0, 0xFFFFFFE000800001);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll $v0, $v0, 23\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW14S23");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW14S24) {
  __ LoadConst64(mips64::V0, 0xFFFFFFC001000001);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll $v0, $v0, 24\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW14S24");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW14S25) {
  __ LoadConst64(mips64::V0, 0xFFFFFF8002000001);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll $v0, $v0, 25\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW14S25");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW14S26) {
  __ LoadConst64(mips64::V0, 0xFFFFFF0004000001);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll $v0, $v0, 26\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW14S26");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW14S27) {
  __ LoadConst64(mips64::V0, 0xFFFFFE0008000001);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll $v0, $v0, 27\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW14S27");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW14S28) {
  __ LoadConst64(mips64::V0, 0xFFFFFC0010000001);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll $v0, $v0, 28\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW14S28");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW14S29) {
  __ LoadConst64(mips64::V0, 0xFFFFF80020000001);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll $v0, $v0, 29\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW14S29");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW14S30) {
  __ LoadConst64(mips64::V0, 0xFFFFF00040000001);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll $v0, $v0, 30\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW14S30");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW14S31) {
  __ LoadConst64(mips64::V0, 0xFFFFE00080000001);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll $v0, $v0, 31\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW14S31");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW14S34) {
  __ LoadConst64(mips64::V0, 0xFFFF000400000001);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll32 $v0, $v0, 2\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW14S34");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW14S35) {
  __ LoadConst64(mips64::V0, 0xFFFE000800000001);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll32 $v0, $v0, 3\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW14S35");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW14S36) {
  __ LoadConst64(mips64::V0, 0xFFFC001000000001);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll32 $v0, $v0, 4\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW14S36");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW14S37) {
  __ LoadConst64(mips64::V0, 0xFFF8002000000001);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll32 $v0, $v0, 5\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW14S37");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW14S38) {
  __ LoadConst64(mips64::V0, 0xFFF0004000000001);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll32 $v0, $v0, 6\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW14S38");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW14S39) {
  __ LoadConst64(mips64::V0, 0xFFE0008000000001);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll32 $v0, $v0, 7\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW14S39");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW14S40) {
  __ LoadConst64(mips64::V0, 0xFFC0010000000001);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll32 $v0, $v0, 8\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW14S40");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW14S41) {
  __ LoadConst64(mips64::V0, 0xFF80020000000001);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll32 $v0, $v0, 9\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW14S41");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW14S42) {
  __ LoadConst64(mips64::V0, 0xFF00040000000001);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll32 $v0, $v0, 10\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW14S42");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW14S43) {
  __ LoadConst64(mips64::V0, 0xFE00080000000001);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll32 $v0, $v0, 11\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW14S43");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW14S44) {
  __ LoadConst64(mips64::V0, 0xFC00100000000001);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll32 $v0, $v0, 12\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW14S44");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW14S45) {
  __ LoadConst64(mips64::V0, 0xF800200000000001);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll32 $v0, $v0, 13\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW14S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW14S46) {
  __ LoadConst64(mips64::V0, 0xF000400000000001);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll32 $v0, $v0, 14\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW14S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW14S47) {
  __ LoadConst64(mips64::V0, 0xE000800000000001);

  const char* expected =
      "daddiu $v0, $zero, -16383\n"
      "dsll32 $v0, $v0, 15\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW14S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW15S17) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFF00020001);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll $v0, $v0, 17\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW15S17");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW15S18) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFE00040001);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll $v0, $v0, 18\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW15S18");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW15S19) {
  __ LoadConst64(mips64::V0, 0xFFFFFFFC00080001);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll $v0, $v0, 19\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW15S19");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW15S20) {
  __ LoadConst64(mips64::V0, 0xFFFFFFF800100001);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll $v0, $v0, 20\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW15S20");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW15S21) {
  __ LoadConst64(mips64::V0, 0xFFFFFFF000200001);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll $v0, $v0, 21\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW15S21");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW15S22) {
  __ LoadConst64(mips64::V0, 0xFFFFFFE000400001);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll $v0, $v0, 22\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW15S22");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW15S23) {
  __ LoadConst64(mips64::V0, 0xFFFFFFC000800001);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll $v0, $v0, 23\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW15S23");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW15S24) {
  __ LoadConst64(mips64::V0, 0xFFFFFF8001000001);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll $v0, $v0, 24\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW15S24");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW15S25) {
  __ LoadConst64(mips64::V0, 0xFFFFFF0002000001);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll $v0, $v0, 25\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW15S25");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW15S26) {
  __ LoadConst64(mips64::V0, 0xFFFFFE0004000001);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll $v0, $v0, 26\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW15S26");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW15S27) {
  __ LoadConst64(mips64::V0, 0xFFFFFC0008000001);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll $v0, $v0, 27\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW15S27");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW15S28) {
  __ LoadConst64(mips64::V0, 0xFFFFF80010000001);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll $v0, $v0, 28\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW15S28");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW15S29) {
  __ LoadConst64(mips64::V0, 0xFFFFF00020000001);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll $v0, $v0, 29\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW15S29");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW15S30) {
  __ LoadConst64(mips64::V0, 0xFFFFE00040000001);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll $v0, $v0, 30\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW15S30");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW15S31) {
  __ LoadConst64(mips64::V0, 0xFFFFC00080000001);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll $v0, $v0, 31\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW15S31");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW15S33) {
  __ LoadConst64(mips64::V0, 0xFFFF000200000001);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll32 $v0, $v0, 1\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW15S33");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW15S34) {
  __ LoadConst64(mips64::V0, 0xFFFE000400000001);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll32 $v0, $v0, 2\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW15S34");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW15S35) {
  __ LoadConst64(mips64::V0, 0xFFFC000800000001);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll32 $v0, $v0, 3\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW15S35");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW15S36) {
  __ LoadConst64(mips64::V0, 0xFFF8001000000001);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll32 $v0, $v0, 4\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW15S36");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW15S37) {
  __ LoadConst64(mips64::V0, 0xFFF0002000000001);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll32 $v0, $v0, 5\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW15S37");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW15S38) {
  __ LoadConst64(mips64::V0, 0xFFE0004000000001);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll32 $v0, $v0, 6\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW15S38");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW15S39) {
  __ LoadConst64(mips64::V0, 0xFFC0008000000001);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll32 $v0, $v0, 7\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW15S39");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW15S40) {
  __ LoadConst64(mips64::V0, 0xFF80010000000001);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll32 $v0, $v0, 8\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW15S40");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW15S41) {
  __ LoadConst64(mips64::V0, 0xFF00020000000001);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll32 $v0, $v0, 9\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW15S41");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW15S42) {
  __ LoadConst64(mips64::V0, 0xFE00040000000001);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll32 $v0, $v0, 10\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW15S42");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW15S43) {
  __ LoadConst64(mips64::V0, 0xFC00080000000001);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll32 $v0, $v0, 11\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW15S43");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW15S44) {
  __ LoadConst64(mips64::V0, 0xF800100000000001);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll32 $v0, $v0, 12\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW15S44");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW15S45) {
  __ LoadConst64(mips64::V0, 0xF000200000000001);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll32 $v0, $v0, 13\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW15S45");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW15S46) {
  __ LoadConst64(mips64::V0, 0xE000400000000001);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll32 $v0, $v0, 14\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW15S46");
}

TEST_F(AssemblerMIPS64Test, LoadConst64DaddiuShiftOriW15S47) {
  __ LoadConst64(mips64::V0, 0xC000800000000001);

  const char* expected =
      "daddiu $v0, $zero, -32767\n"
      "dsll32 $v0, $v0, 15\n"
      "ori $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64DaddiuShiftOriW15S47");
}

TEST_F(AssemblerMIPS64Test, LoadConst64LuiDahiDati) {
  __ LoadConst64(mips64::V0, 0x0001000200030000);

  const char* expected =
      "lui $v0, 3\n"
      "dahi $v0, $v0, 2\n"
      "dati $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64LuiDahiDati");
}

TEST_F(AssemblerMIPS64Test, LoadConst64NLuiDahiDati) {
  __ LoadConst64(mips64::V0, 0x00010002FFFD0000);

  const char* expected =
      "lui $v0, 0xFFFD\n"
      "dahi $v0, $v0, 3\n"
      "dati $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64NLuiDahiDati");
}

TEST_F(AssemblerMIPS64Test, LoadConst64LuiNDahiDati) {
  __ LoadConst64(mips64::V0, 0x0001FFFE00030000);

  const char* expected =
      "lui $v0, 3\n"
      "dahi $v0, $v0, 0xFFFE\n"
      "dati $v0, $v0, 2\n";
  DriverStr(expected, "LoadConst64LuiNDahiDati");
}

TEST_F(AssemblerMIPS64Test, LoadConst64NLuiNDahiDati) {
  __ LoadConst64(mips64::V0, 0x0001FFFEFFFD0000);

  const char* expected =
      "lui $v0, 0xFFFD\n"
      "dahi $v0, $v0, 0xFFFF\n"
      "dati $v0, $v0, 2\n";
  DriverStr(expected, "LoadConst64NLuiNDahiDati");
}

TEST_F(AssemblerMIPS64Test, LoadConst64LuiDahiNDati) {
  __ LoadConst64(mips64::V0, 0xFFEE000200030000);

  const char* expected =
      "lui $v0, 3\n"
      "dahi $v0, $v0, 2\n"
      "dati $v0, $v0, 0xFFEE\n";
  DriverStr(expected, "LoadConst64LuiDahiNDati");
}

TEST_F(AssemblerMIPS64Test, LoadConst64NLuiDahiNDati) {
  __ LoadConst64(mips64::V0, 0xFFEE0002FFFD0000);

  const char* expected =
      "lui $v0, 0xFFFD\n"
      "dahi $v0, $v0, 3\n"
      "dati $v0, $v0, 0xFFEE\n";
  DriverStr(expected, "LoadConst64NLuiDahiNDati");
}

TEST_F(AssemblerMIPS64Test, LoadConst64LuiNDahiNDati) {
  __ LoadConst64(mips64::V0, 0xFFEEFFFE00030000);

  const char* expected =
      "lui $v0, 3\n"
      "dahi $v0, $v0, 0xFFFE\n"
      "dati $v0, $v0, 0xFFEF\n";
  DriverStr(expected, "LoadConst64LuiNDahiNDati");
}

TEST_F(AssemblerMIPS64Test, LoadConst64NLuiNDahiNDati) {
  __ LoadConst64(mips64::V0, 0xFFEEFFFEFFFD0000);

  const char* expected =
      "lui $v0, 0xFFFD\n"
      "dahi $v0, $v0, 0xFFFF\n"
      "dati $v0, $v0, 0xFFEF\n";
  DriverStr(expected, "LoadConst64NLuiNDahiNDati");
}

TEST_F(AssemblerMIPS64Test, LoadConst64LuiOriDahiDati) {
  __ LoadConst64(mips64::V0, 0x0001000200030004);

  const char* expected =
      "lui $v0, 3\n"
      "ori $v0, $v0, 4\n"
      "dahi $v0, $v0, 2\n"
      "dati $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64LuiOriDahiDati");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriDahiDati) {
  __ LoadConst64(mips64::V0, 0x0081000200000004);

  const char* expected =
      "ori $v0, $zero, 4\n"
      "dahi $v0, $v0, 2\n"
      "dati $v0, $v0, 129\n";
  DriverStr(expected, "LoadConst64OriDahiDati");
}

TEST_F(AssemblerMIPS64Test, LoadConst64NOriDahiDati) {
  __ LoadConst64(mips64::V0, 0x00010002FFFFFFFC);

  const char* expected =
      "lui $v0, 65535\n"
      "ori $v0, $v0, 65532\n"
      "dahi $v0, $v0, 3\n"
      "dati $v0, $v0, 1\n";
  DriverStr(expected, "LoadConst64NOriDahiDati");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriNDahiDati) {
  __ LoadConst64(mips64::V0, 0x0081FFFE00000004);

  const char* expected =
      "ori $v0, $zero, 4\n"
      "dahi $v0, $v0, 0xFFFE\n"
      "dati $v0, $v0, 130\n";
  DriverStr(expected, "LoadConst64OriNDahiDati");
}

TEST_F(AssemblerMIPS64Test, LoadConst64NOriNDahiDati) {
  __ LoadConst64(mips64::V0, 0x0081FFFEFFFFFFFC);

  const char* expected =
      "lui $v0, 65535\n"
      "ori $v0, $v0, 65532\n"
      "dahi $v0, $v0, 0xFFFF\n"
      "dati $v0, $v0, 130\n";
  DriverStr(expected, "LoadConst64NOriNDahiDati");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriDahiNDati) {
  __ LoadConst64(mips64::V0, 0xFF7F000200000004);

  const char* expected =
      "ori $v0, $zero, 4\n"
      "dahi $v0, $v0, 2\n"
      "dati $v0, $v0, 0xFF7F\n";
  DriverStr(expected, "LoadConst64OriDahiNDati");
}

TEST_F(AssemblerMIPS64Test, LoadConst64NOriDahiNDati) {
  __ LoadConst64(mips64::V0, 0xFF7F0002FFFFFFFC);

  const char* expected =
      "lui $v0, 65535\n"
      "ori $v0, $v0, 65532\n"
      "dahi $v0, $v0, 3\n"
      "dati $v0, $v0, 0xFF7F\n";
  DriverStr(expected, "LoadConst64NOriDahiNDati");
}

TEST_F(AssemblerMIPS64Test, LoadConst64OriNDahiNDati) {
  __ LoadConst64(mips64::V0, 0xFF7FFFFE00000004);

  const char* expected =
      "ori $v0, $zero, 4\n"
      "dahi $v0, $v0, 0xFFFE\n"
      "dati $v0, $v0, 0xFF80\n";
  DriverStr(expected, "LoadConst64OriNDahiNDati");
}

TEST_F(AssemblerMIPS64Test, LoadConst64NOriNDahiNDati) {
  __ LoadConst64(mips64::V0, 0xFF7FFFFEFFFFFFFC);

  const char* expected =
      "lui $v0, 65535\n"
      "ori $v0, $v0, 65532\n"
      "dahi $v0, $v0, 0xFFFF\n"
      "dati $v0, $v0, 0xFF80\n";
  DriverStr(expected, "LoadConst64NOriNDahiNDati");
}

TEST_F(AssemblerMIPS64Test, LoadConst64Mask1) {
  __ LoadConst64(mips64::V0, 0x5555555555555555);

  const char* expected =
      "lui $v0, 0x5555\n"
      "ori $v0, $v0, 0x5555\n"
      "dinsu $v0, $v0, 32, 32\n";
  DriverStr(expected, "LoadConst64Mask1");
}

TEST_F(AssemblerMIPS64Test, LoadConst64Mask2) {
  __ LoadConst64(mips64::V0, 0x3333333333333333);

  const char* expected =
      "lui $v0, 0x3333\n"
      "ori $v0, $v0, 0x3333\n"
      "dinsu $v0, $v0, 32, 32\n";
  DriverStr(expected, "LoadConst64Mask2");
}

TEST_F(AssemblerMIPS64Test, LoadConst64Mask3) {
  __ LoadConst64(mips64::V0, 0x0F0F0F0F0F0F0F0F);

  const char* expected =
      "lui $v0, 0x0F0F\n"
      "ori $v0, $v0, 0x0F0F\n"
      "dinsu $v0, $v0, 32, 32\n";
  DriverStr(expected, "LoadConst64Mask3");
}

TEST_F(AssemblerMIPS64Test, LoadConst64Mask4) {
  __ LoadConst64(mips64::V0, 0x0101010101010101);

  const char* expected =
      "lui $v0, 0x0101\n"
      "ori $v0, $v0, 0x0101\n"
      "dinsu $v0, $v0, 32, 32\n";
  DriverStr(expected, "LoadConst64Mask4");
}

// ori
TEST_F(AssemblerMIPS64Test, LoadConst64Dinsu1) {
  __ LoadConst64(mips64::V0, 0x0000010100000101);

  const char* expected =
      "ori $v0, $zero, 0x0101\n"
      "dahi $v0, $v0, 0x0101\n";
  DriverStr(expected, "LoadConst64Dinsu1");
}

// daddiu
TEST_F(AssemblerMIPS64Test, LoadConst64Dinsu2) {
  __ LoadConst64(mips64::V0, 0xFFFFFEFEFFFFFEFE);

  const char* expected =
      "daddiu $v0, $zero, 65278\n"
      "dinsu $v0, $v0, 32, 32\n";
  DriverStr(expected, "LoadConst64Dinsu2");
}

// daddiu
TEST_F(AssemblerMIPS64Test, LoadConst64Dinsu2a) {
  __ LoadConst64(mips64::V0, 0xFFFEFEFEFFFEFEFE);

  const char* expected =
      "lui $v0, 65534\n"
      "ori $v0, $v0, 65278\n"
      "dinsu $v0, $v0, 32, 32\n";
  DriverStr(expected, "LoadConst64Dinsu2a");
}

// lui (non-negative value)
TEST_F(AssemblerMIPS64Test, LoadConst64Dinsu3) {
  __ LoadConst64(mips64::V0, 0x7FFF00007FFF0000);

  const char* expected =
      "lui $v0, 32767\n"
      "dati $v0, $v0, 32767\n";
  DriverStr(expected, "LoadConst64Dinsu3");
}

// lui (negative value)
TEST_F(AssemblerMIPS64Test, LoadConst64Dinsu4) {
  __ LoadConst64(mips64::V0, 0x8001000080010000);

  const char* expected =
      "lui $v0, 32769\n"
      "dinsu $v0, $v0, 32, 32\n";
  DriverStr(expected, "LoadConst64Dinsu4");
}

// lui (non-negative value) & ori
TEST_F(AssemblerMIPS64Test, LoadConst64Dinsu5) {
  __ LoadConst64(mips64::V0, 0x7FFF00017FFF0001);

  const char* expected =
      "lui $v0, 32767\n"
      "ori $v0, $v0, 1\n"
      "dinsu $v0, $v0, 32, 32\n";
  DriverStr(expected, "LoadConst64Dinsu5");
}

// lui (negative value) & ori
TEST_F(AssemblerMIPS64Test, LoadConst64Dinsu6) {
  __ LoadConst64(mips64::V0, 0x8001FFFE8001FFFE);

  const char* expected =
      "lui $v0, 32769\n"
      "ori $v0, $v0, 65534\n"
      "dinsu $v0, $v0, 32, 32\n";
  DriverStr(expected, "LoadConst64Dinsu6");
}

// lui (negative value) & ori
TEST_F(AssemblerMIPS64Test, LoadConst64Dinsu7) {
  __ LoadConst64(mips64::V0, 0x8001FFFE8001FFFE);

  const char* expected =
      "lui $v0, 32769\n"
      "ori $v0, $v0, 65534\n"
      "dinsu $v0, $v0, 32, 32\n";
  DriverStr(expected, "LoadConst64Dinsu7");
}

#undef __

}  // namespace art
