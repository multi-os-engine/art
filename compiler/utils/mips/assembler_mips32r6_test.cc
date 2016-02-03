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

#include "assembler_mips.h"

#include <map>

#include "base/stl_util.h"
#include "utils/assembler_test.h"

#define __ GetAssembler()->

namespace art {

struct MIPSCpuRegisterCompare {
  bool operator()(const mips::Register& a, const mips::Register& b) const {
    return a < b;
  }
};

class AssemblerMIPS32r6Test : public AssemblerTest<mips::MipsAssembler,
                                               mips::Register,
                                               mips::FRegister,
                                               uint32_t> {
 public:
  typedef AssemblerTest<mips::MipsAssembler, mips::Register, mips::FRegister, uint32_t> Base;

 protected:
  // Get the typically used name for this architecture, e.g., aarch64, x86-64, ...
  std::string GetArchitectureString() OVERRIDE {
    return "mips";
  }

  std::string GetAssemblerParameters() OVERRIDE {
    return " --no-warn -32 -march=mips32r6";
  }

  std::string GetDisassembleParameters() OVERRIDE {
    return " -D -bbinary -mmips:isa32r6";
  }

  mips::MipsAssembler* CreateAssembler() OVERRIDE {
    const MipsInstructionSetFeatures* pisf =
        MipsInstructionSetFeatures::FromVariant("mips32r6", nullptr);  // TODO: leak
    return new mips::MipsAssembler(pisf);
  }

  void SetUpHelpers() OVERRIDE {
    if (registers_.size() == 0) {
      registers_.push_back(new mips::Register(mips::ZERO));
      registers_.push_back(new mips::Register(mips::AT));
      registers_.push_back(new mips::Register(mips::V0));
      registers_.push_back(new mips::Register(mips::V1));
      registers_.push_back(new mips::Register(mips::A0));
      registers_.push_back(new mips::Register(mips::A1));
      registers_.push_back(new mips::Register(mips::A2));
      registers_.push_back(new mips::Register(mips::A3));
      registers_.push_back(new mips::Register(mips::T0));
      registers_.push_back(new mips::Register(mips::T1));
      registers_.push_back(new mips::Register(mips::T2));
      registers_.push_back(new mips::Register(mips::T3));
      registers_.push_back(new mips::Register(mips::T4));
      registers_.push_back(new mips::Register(mips::T5));
      registers_.push_back(new mips::Register(mips::T6));
      registers_.push_back(new mips::Register(mips::T7));
      registers_.push_back(new mips::Register(mips::S0));
      registers_.push_back(new mips::Register(mips::S1));
      registers_.push_back(new mips::Register(mips::S2));
      registers_.push_back(new mips::Register(mips::S3));
      registers_.push_back(new mips::Register(mips::S4));
      registers_.push_back(new mips::Register(mips::S5));
      registers_.push_back(new mips::Register(mips::S6));
      registers_.push_back(new mips::Register(mips::S7));
      registers_.push_back(new mips::Register(mips::T8));
      registers_.push_back(new mips::Register(mips::T9));
      registers_.push_back(new mips::Register(mips::K0));
      registers_.push_back(new mips::Register(mips::K1));
      registers_.push_back(new mips::Register(mips::GP));
      registers_.push_back(new mips::Register(mips::SP));
      registers_.push_back(new mips::Register(mips::FP));
      registers_.push_back(new mips::Register(mips::RA));

      secondary_register_names_.emplace(mips::Register(mips::ZERO), "zero");
      secondary_register_names_.emplace(mips::Register(mips::AT), "at");
      secondary_register_names_.emplace(mips::Register(mips::V0), "v0");
      secondary_register_names_.emplace(mips::Register(mips::V1), "v1");
      secondary_register_names_.emplace(mips::Register(mips::A0), "a0");
      secondary_register_names_.emplace(mips::Register(mips::A1), "a1");
      secondary_register_names_.emplace(mips::Register(mips::A2), "a2");
      secondary_register_names_.emplace(mips::Register(mips::A3), "a3");
      secondary_register_names_.emplace(mips::Register(mips::T0), "t0");
      secondary_register_names_.emplace(mips::Register(mips::T1), "t1");
      secondary_register_names_.emplace(mips::Register(mips::T2), "t2");
      secondary_register_names_.emplace(mips::Register(mips::T3), "t3");
      secondary_register_names_.emplace(mips::Register(mips::T4), "t4");
      secondary_register_names_.emplace(mips::Register(mips::T5), "t5");
      secondary_register_names_.emplace(mips::Register(mips::T6), "t6");
      secondary_register_names_.emplace(mips::Register(mips::T7), "t7");
      secondary_register_names_.emplace(mips::Register(mips::S0), "s0");
      secondary_register_names_.emplace(mips::Register(mips::S1), "s1");
      secondary_register_names_.emplace(mips::Register(mips::S2), "s2");
      secondary_register_names_.emplace(mips::Register(mips::S3), "s3");
      secondary_register_names_.emplace(mips::Register(mips::S4), "s4");
      secondary_register_names_.emplace(mips::Register(mips::S5), "s5");
      secondary_register_names_.emplace(mips::Register(mips::S6), "s6");
      secondary_register_names_.emplace(mips::Register(mips::S7), "s7");
      secondary_register_names_.emplace(mips::Register(mips::T8), "t8");
      secondary_register_names_.emplace(mips::Register(mips::T9), "t9");
      secondary_register_names_.emplace(mips::Register(mips::K0), "k0");
      secondary_register_names_.emplace(mips::Register(mips::K1), "k1");
      secondary_register_names_.emplace(mips::Register(mips::GP), "gp");
      secondary_register_names_.emplace(mips::Register(mips::SP), "sp");
      secondary_register_names_.emplace(mips::Register(mips::FP), "fp");
      secondary_register_names_.emplace(mips::Register(mips::RA), "ra");

      fp_registers_.push_back(new mips::FRegister(mips::F0));
      fp_registers_.push_back(new mips::FRegister(mips::F1));
      fp_registers_.push_back(new mips::FRegister(mips::F2));
      fp_registers_.push_back(new mips::FRegister(mips::F3));
      fp_registers_.push_back(new mips::FRegister(mips::F4));
      fp_registers_.push_back(new mips::FRegister(mips::F5));
      fp_registers_.push_back(new mips::FRegister(mips::F6));
      fp_registers_.push_back(new mips::FRegister(mips::F7));
      fp_registers_.push_back(new mips::FRegister(mips::F8));
      fp_registers_.push_back(new mips::FRegister(mips::F9));
      fp_registers_.push_back(new mips::FRegister(mips::F10));
      fp_registers_.push_back(new mips::FRegister(mips::F11));
      fp_registers_.push_back(new mips::FRegister(mips::F12));
      fp_registers_.push_back(new mips::FRegister(mips::F13));
      fp_registers_.push_back(new mips::FRegister(mips::F14));
      fp_registers_.push_back(new mips::FRegister(mips::F15));
      fp_registers_.push_back(new mips::FRegister(mips::F16));
      fp_registers_.push_back(new mips::FRegister(mips::F17));
      fp_registers_.push_back(new mips::FRegister(mips::F18));
      fp_registers_.push_back(new mips::FRegister(mips::F19));
      fp_registers_.push_back(new mips::FRegister(mips::F20));
      fp_registers_.push_back(new mips::FRegister(mips::F21));
      fp_registers_.push_back(new mips::FRegister(mips::F22));
      fp_registers_.push_back(new mips::FRegister(mips::F23));
      fp_registers_.push_back(new mips::FRegister(mips::F24));
      fp_registers_.push_back(new mips::FRegister(mips::F25));
      fp_registers_.push_back(new mips::FRegister(mips::F26));
      fp_registers_.push_back(new mips::FRegister(mips::F27));
      fp_registers_.push_back(new mips::FRegister(mips::F28));
      fp_registers_.push_back(new mips::FRegister(mips::F29));
      fp_registers_.push_back(new mips::FRegister(mips::F30));
      fp_registers_.push_back(new mips::FRegister(mips::F31));
    }
  }

  void TearDown() OVERRIDE {
    AssemblerTest::TearDown();
    STLDeleteElements(&registers_);
    STLDeleteElements(&fp_registers_);
  }

  std::vector<mips::Register*> GetRegisters() OVERRIDE {
    return registers_;
  }

  std::vector<mips::FRegister*> GetFPRegisters() OVERRIDE {
    return fp_registers_;
  }

  uint32_t CreateImmediate(int64_t imm_value) OVERRIDE {
    return imm_value;
  }

  std::string GetSecondaryRegisterName(const mips::Register& reg) OVERRIDE {
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

  void BranchCondTwoRegsHelper(void (mips::MipsAssembler::*f)(mips::Register,
                                                              mips::Register,
                                                              mips::MipsLabel*),
                               std::string instr_name) {
    mips::MipsLabel label;
    (Base::GetAssembler()->*f)(mips::A0, mips::A1, &label);
    constexpr size_t kAdduCount1 = 63;
    for (size_t i = 0; i != kAdduCount1; ++i) {
      __ Addu(mips::ZERO, mips::ZERO, mips::ZERO);
    }
    __ Bind(&label);
    constexpr size_t kAdduCount2 = 64;
    for (size_t i = 0; i != kAdduCount2; ++i) {
      __ Addu(mips::ZERO, mips::ZERO, mips::ZERO);
    }
    (Base::GetAssembler()->*f)(mips::A2, mips::A3, &label);

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
  std::vector<mips::Register*> registers_;
  std::map<mips::Register, std::string, MIPSCpuRegisterCompare> secondary_register_names_;

  std::vector<mips::FRegister*> fp_registers_;
};


TEST_F(AssemblerMIPS32r6Test, Toolchain) {
  EXPECT_TRUE(CheckTools());
}

TEST_F(AssemblerMIPS32r6Test, Addu) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::Addu, "addu ${reg1}, ${reg2}, ${reg3}"), "Addu");
}

TEST_F(AssemblerMIPS32r6Test, Addiu) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Addiu, -16, "addiu ${reg1}, ${reg2}, {imm}"), "Addiu");
}

TEST_F(AssemblerMIPS32r6Test, Subu) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::Subu, "subu ${reg1}, ${reg2}, ${reg3}"), "Subu");
}

TEST_F(AssemblerMIPS32r6Test, And) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::And, "and ${reg1}, ${reg2}, ${reg3}"), "And");
}

TEST_F(AssemblerMIPS32r6Test, Andi) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Andi, 16, "andi ${reg1}, ${reg2}, {imm}"), "Andi");
}

TEST_F(AssemblerMIPS32r6Test, Or) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::Or, "or ${reg1}, ${reg2}, ${reg3}"), "Or");
}

TEST_F(AssemblerMIPS32r6Test, Ori) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Ori, 16, "ori ${reg1}, ${reg2}, {imm}"), "Ori");
}

TEST_F(AssemblerMIPS32r6Test, Xor) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::Xor, "xor ${reg1}, ${reg2}, ${reg3}"), "Xor");
}

TEST_F(AssemblerMIPS32r6Test, Xori) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Xori, 16, "xori ${reg1}, ${reg2}, {imm}"), "Xori");
}

TEST_F(AssemblerMIPS32r6Test, Nor) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::Nor, "nor ${reg1}, ${reg2}, ${reg3}"), "Nor");
}

TEST_F(AssemblerMIPS32r6Test, MulR6) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::MulR6, "mul ${reg1}, ${reg2}, ${reg3}"), "MulR6");
}

TEST_F(AssemblerMIPS32r6Test, MuhR6) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::MuhR6, "muh ${reg1}, ${reg2}, ${reg3}"), "MuhR6");
}

TEST_F(AssemblerMIPS32r6Test, MuhuR6) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::MuhuR6, "muhu ${reg1}, ${reg2}, ${reg3}"), "MuhuR6");
}

TEST_F(AssemblerMIPS32r6Test, DivR6) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::DivR6, "div ${reg1}, ${reg2}, ${reg3}"), "DivR6");
}

TEST_F(AssemblerMIPS32r6Test, ModR6) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::ModR6, "mod ${reg1}, ${reg2}, ${reg3}"), "ModR6");
}

TEST_F(AssemblerMIPS32r6Test, DivuR6) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::DivuR6, "divu ${reg1}, ${reg2}, ${reg3}"), "DivuR6");
}

TEST_F(AssemblerMIPS32r6Test, ModuR6) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::ModuR6, "modu ${reg1}, ${reg2}, ${reg3}"), "ModuR6");
}

//////////
// MISC //
//////////

TEST_F(AssemblerMIPS32r6Test, Bitswap) {
  DriverStr(RepeatRR(&mips::MipsAssembler::Bitswap, "bitswap ${reg1}, ${reg2}"), "bitswap");
}

TEST_F(AssemblerMIPS32r6Test, Seb) {
  DriverStr(RepeatRR(&mips::MipsAssembler::Seb, "seb ${reg1}, ${reg2}"), "Seb");
}

TEST_F(AssemblerMIPS32r6Test, Seh) {
  DriverStr(RepeatRR(&mips::MipsAssembler::Seh, "seh ${reg1}, ${reg2}"), "Seh");
}

TEST_F(AssemblerMIPS32r6Test, Sll) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Sll, 5, "sll ${reg1}, ${reg2}, {imm}"), "Sll");
}

TEST_F(AssemblerMIPS32r6Test, Srl) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Srl, 5, "srl ${reg1}, ${reg2}, {imm}"), "Srl");
}

TEST_F(AssemblerMIPS32r6Test, Sra) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Sra, 5, "sra ${reg1}, ${reg2}, {imm}"), "Sra");
}

TEST_F(AssemblerMIPS32r6Test, Sllv) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::Sllv, "sllv ${reg1}, ${reg2}, ${reg3}"), "Sllv");
}

TEST_F(AssemblerMIPS32r6Test, Srlv) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::Srlv, "srlv ${reg1}, ${reg2}, ${reg3}"), "Srlv");
}

TEST_F(AssemblerMIPS32r6Test, Rotrv) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::Rotrv, "rotrv ${reg1}, ${reg2}, ${reg3}"), "rotrv");
}

TEST_F(AssemblerMIPS32r6Test, Srav) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::Srav, "srav ${reg1}, ${reg2}, ${reg3}"), "Srav");
}

TEST_F(AssemblerMIPS32r6Test, Seleqz) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::Seleqz, "seleqz ${reg1}, ${reg2}, ${reg3}"),
            "seleqz");
}

TEST_F(AssemblerMIPS32r6Test, Selnez) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::Selnez, "selnez ${reg1}, ${reg2}, ${reg3}"),
            "selnez");
}

TEST_F(AssemblerMIPS32r6Test, ClzR6) {
  DriverStr(RepeatRR(&mips::MipsAssembler::ClzR6, "clz ${reg1}, ${reg2}"), "clzR6");
}

TEST_F(AssemblerMIPS32r6Test, CloR6) {
  DriverStr(RepeatRR(&mips::MipsAssembler::CloR6, "clo ${reg1}, ${reg2}"), "cloR6");
}

TEST_F(AssemblerMIPS32r6Test, Lb) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Lb, -16, "lb ${reg1}, {imm}(${reg2})"), "Lb");
}

TEST_F(AssemblerMIPS32r6Test, Lh) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Lh, -16, "lh ${reg1}, {imm}(${reg2})"), "Lh");
}

TEST_F(AssemblerMIPS32r6Test, Lw) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Lw, -16, "lw ${reg1}, {imm}(${reg2})"), "Lw");
}

TEST_F(AssemblerMIPS32r6Test, Lbu) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Lbu, -16, "lbu ${reg1}, {imm}(${reg2})"), "Lbu");
}

TEST_F(AssemblerMIPS32r6Test, Lhu) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Lhu, -16, "lhu ${reg1}, {imm}(${reg2})"), "Lhu");
}

TEST_F(AssemblerMIPS32r6Test, Lui) {
  DriverStr(RepeatRIb(&mips::MipsAssembler::Lui, 16, "lui ${reg}, {imm}"), "Lui");
}

TEST_F(AssemblerMIPS32r6Test, Sb) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Sb, -16, "sb ${reg1}, {imm}(${reg2})"), "Sb");
}

TEST_F(AssemblerMIPS32r6Test, Sh) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Sh, -16, "sh ${reg1}, {imm}(${reg2})"), "Sh");
}

TEST_F(AssemblerMIPS32r6Test, Sw) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Sw, -16, "sw ${reg1}, {imm}(${reg2})"), "Sw");
}

TEST_F(AssemblerMIPS32r6Test, Slt) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::Slt, "slt ${reg1}, ${reg2}, ${reg3}"), "Slt");
}

TEST_F(AssemblerMIPS32r6Test, Sltu) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::Sltu, "sltu ${reg1}, ${reg2}, ${reg3}"), "Sltu");
}

TEST_F(AssemblerMIPS32r6Test, Slti) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Slti, -16, "slti ${reg1}, ${reg2}, {imm}"), "Slti");
}

TEST_F(AssemblerMIPS32r6Test, Sltiu) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Sltiu, -16, "sltiu ${reg1}, ${reg2}, {imm}"), "Sltiu");
}

////////////////////
// FLOATING POINT //
////////////////////

TEST_F(AssemblerMIPS32r6Test, AddS) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::AddS, "add.s ${reg1}, ${reg2}, ${reg3}"), "AddS");
}

TEST_F(AssemblerMIPS32r6Test, AddD) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::AddD, "add.d ${reg1}, ${reg2}, ${reg3}"), "AddD");
}

TEST_F(AssemblerMIPS32r6Test, SubS) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::SubS, "sub.s ${reg1}, ${reg2}, ${reg3}"), "SubS");
}

TEST_F(AssemblerMIPS32r6Test, SubD) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::SubD, "sub.d ${reg1}, ${reg2}, ${reg3}"), "SubD");
}

TEST_F(AssemblerMIPS32r6Test, MulS) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::MulS, "mul.s ${reg1}, ${reg2}, ${reg3}"), "MulS");
}

TEST_F(AssemblerMIPS32r6Test, MulD) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::MulD, "mul.d ${reg1}, ${reg2}, ${reg3}"), "MulD");
}

TEST_F(AssemblerMIPS32r6Test, DivS) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::DivS, "div.s ${reg1}, ${reg2}, ${reg3}"), "DivS");
}

TEST_F(AssemblerMIPS32r6Test, DivD) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::DivD, "div.d ${reg1}, ${reg2}, ${reg3}"), "DivD");
}

TEST_F(AssemblerMIPS32r6Test, MovS) {
  DriverStr(RepeatFF(&mips::MipsAssembler::MovS, "mov.s ${reg1}, ${reg2}"), "MovS");
}

TEST_F(AssemblerMIPS32r6Test, MovD) {
  DriverStr(RepeatFF(&mips::MipsAssembler::MovD, "mov.d ${reg1}, ${reg2}"), "MovD");
}

TEST_F(AssemblerMIPS32r6Test, NegS) {
  DriverStr(RepeatFF(&mips::MipsAssembler::NegS, "neg.s ${reg1}, ${reg2}"), "NegS");
}

TEST_F(AssemblerMIPS32r6Test, NegD) {
  DriverStr(RepeatFF(&mips::MipsAssembler::NegD, "neg.d ${reg1}, ${reg2}"), "NegD");
}

TEST_F(AssemblerMIPS32r6Test, SelS) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::SelS, "sel.s ${reg1}, ${reg2}, ${reg3}"), "sel.s");
}

TEST_F(AssemblerMIPS32r6Test, SelD) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::SelD, "sel.d ${reg1}, ${reg2}, ${reg3}"), "sel.d");
}

TEST_F(AssemblerMIPS32r6Test, ClassS) {
  DriverStr(RepeatFF(&mips::MipsAssembler::ClassS, "class.s ${reg1}, ${reg2}"), "class.s");
}

TEST_F(AssemblerMIPS32r6Test, ClassD) {
  DriverStr(RepeatFF(&mips::MipsAssembler::ClassD, "class.d ${reg1}, ${reg2}"), "class.d");
}

TEST_F(AssemblerMIPS32r6Test, MinS) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::MinS, "min.s ${reg1}, ${reg2}, ${reg3}"), "min.s");
}

TEST_F(AssemblerMIPS32r6Test, MinD) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::MinD, "min.d ${reg1}, ${reg2}, ${reg3}"), "min.d");
}

TEST_F(AssemblerMIPS32r6Test, MaxS) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::MaxS, "max.s ${reg1}, ${reg2}, ${reg3}"), "max.s");
}

TEST_F(AssemblerMIPS32r6Test, MaxD) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::MaxD, "max.d ${reg1}, ${reg2}, ${reg3}"), "max.d");
}

TEST_F(AssemblerMIPS32r6Test, CmpUnS) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpUnS, "cmp.un.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.un.s");
}

TEST_F(AssemblerMIPS32r6Test, CmpEqS) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpEqS, "cmp.eq.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.eq.s");
}

TEST_F(AssemblerMIPS32r6Test, CmpUeqS) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpUeqS, "cmp.ueq.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.ueq.s");
}

TEST_F(AssemblerMIPS32r6Test, CmpLtS) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpLtS, "cmp.lt.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.lt.s");
}

TEST_F(AssemblerMIPS32r6Test, CmpUltS) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpUltS, "cmp.ult.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.ult.s");
}

TEST_F(AssemblerMIPS32r6Test, CmpLeS) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpLeS, "cmp.le.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.le.s");
}

TEST_F(AssemblerMIPS32r6Test, CmpUleS) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpUleS, "cmp.ule.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.ule.s");
}

TEST_F(AssemblerMIPS32r6Test, CmpOrS) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpOrS, "cmp.or.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.or.s");
}

TEST_F(AssemblerMIPS32r6Test, CmpUneS) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpUneS, "cmp.une.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.une.s");
}

TEST_F(AssemblerMIPS32r6Test, CmpNeS) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpNeS, "cmp.ne.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.ne.s");
}

TEST_F(AssemblerMIPS32r6Test, CmpUnD) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpUnD, "cmp.un.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.un.d");
}

TEST_F(AssemblerMIPS32r6Test, CmpEqD) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpEqD, "cmp.eq.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.eq.d");
}

TEST_F(AssemblerMIPS32r6Test, CmpUeqD) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpUeqD, "cmp.ueq.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.ueq.d");
}

TEST_F(AssemblerMIPS32r6Test, CmpLtD) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpLtD, "cmp.lt.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.lt.d");
}

TEST_F(AssemblerMIPS32r6Test, CmpUltD) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpUltD, "cmp.ult.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.ult.d");
}

TEST_F(AssemblerMIPS32r6Test, CmpLeD) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpLeD, "cmp.le.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.le.d");
}

TEST_F(AssemblerMIPS32r6Test, CmpUleD) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpUleD, "cmp.ule.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.ule.d");
}

TEST_F(AssemblerMIPS32r6Test, CmpOrD) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpOrD, "cmp.or.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.or.d");
}

TEST_F(AssemblerMIPS32r6Test, CmpUneD) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpUneD, "cmp.une.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.une.d");
}

TEST_F(AssemblerMIPS32r6Test, CmpNeD) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpNeD, "cmp.ne.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.ne.d");
}

TEST_F(AssemblerMIPS32r6Test, CvtSW) {
  DriverStr(RepeatFF(&mips::MipsAssembler::Cvtsw, "cvt.s.w ${reg1}, ${reg2}"), "CvtSW");
}

TEST_F(AssemblerMIPS32r6Test, CvtDW) {
  DriverStr(RepeatFF(&mips::MipsAssembler::Cvtdw, "cvt.d.w ${reg1}, ${reg2}"), "CvtDW");
}

TEST_F(AssemblerMIPS32r6Test, CvtSD) {
  DriverStr(RepeatFF(&mips::MipsAssembler::Cvtsd, "cvt.s.d ${reg1}, ${reg2}"), "CvtSD");
}

TEST_F(AssemblerMIPS32r6Test, CvtDS) {
  DriverStr(RepeatFF(&mips::MipsAssembler::Cvtds, "cvt.d.s ${reg1}, ${reg2}"), "CvtDS");
}

TEST_F(AssemblerMIPS32r6Test, Mfc1) {
  DriverStr(RepeatRF(&mips::MipsAssembler::Mfc1, "mfc1 ${reg1}, ${reg2}"), "Mfc1");
}

TEST_F(AssemblerMIPS32r6Test, Mtc1) {
  DriverStr(RepeatRF(&mips::MipsAssembler::Mtc1, "mtc1 ${reg1}, ${reg2}"), "Mtc1");
}

TEST_F(AssemblerMIPS32r6Test, Mfhc1) {
  DriverStr(RepeatRF(&mips::MipsAssembler::Mfhc1, "mfhc1 ${reg1}, ${reg2}"), "Mfhc1");
}

TEST_F(AssemblerMIPS32r6Test, Mthc1) {
  DriverStr(RepeatRF(&mips::MipsAssembler::Mthc1, "mthc1 ${reg1}, ${reg2}"), "Mthc1");
}

TEST_F(AssemblerMIPS32r6Test, Lwc1) {
  DriverStr(RepeatFRIb(&mips::MipsAssembler::Lwc1, -16, "lwc1 ${reg1}, {imm}(${reg2})"), "Lwc1");
}

TEST_F(AssemblerMIPS32r6Test, Ldc1) {
  DriverStr(RepeatFRIb(&mips::MipsAssembler::Ldc1, -16, "ldc1 ${reg1}, {imm}(${reg2})"), "Ldc1");
}

TEST_F(AssemblerMIPS32r6Test, Swc1) {
  DriverStr(RepeatFRIb(&mips::MipsAssembler::Swc1, -16, "swc1 ${reg1}, {imm}(${reg2})"), "Swc1");
}

TEST_F(AssemblerMIPS32r6Test, Sdc1) {
  DriverStr(RepeatFRIb(&mips::MipsAssembler::Sdc1, -16, "sdc1 ${reg1}, {imm}(${reg2})"), "Sdc1");
}

TEST_F(AssemblerMIPS32r6Test, Move) {
  DriverStr(RepeatRR(&mips::MipsAssembler::Move, "or ${reg1}, ${reg2}, $zero"), "Move");
}

TEST_F(AssemblerMIPS32r6Test, Clear) {
  DriverStr(RepeatR(&mips::MipsAssembler::Clear, "or ${reg}, $zero, $zero"), "Clear");
}

TEST_F(AssemblerMIPS32r6Test, Not) {
  DriverStr(RepeatRR(&mips::MipsAssembler::Not, "nor ${reg1}, ${reg2}, $zero"), "Not");
}

TEST_F(AssemblerMIPS32r6Test, LoadFromOffset) {
  __ LoadFromOffset(mips::kLoadSignedByte, mips::A0, mips::A0, 0);
  __ LoadFromOffset(mips::kLoadSignedByte, mips::A0, mips::A1, 0);
  __ LoadFromOffset(mips::kLoadSignedByte, mips::A0, mips::A1, 256);
  __ LoadFromOffset(mips::kLoadSignedByte, mips::A0, mips::A1, 1000);
  __ LoadFromOffset(mips::kLoadSignedByte, mips::A0, mips::A1, 0x8000);
  __ LoadFromOffset(mips::kLoadSignedByte, mips::A0, mips::A1, 0x10000);
  __ LoadFromOffset(mips::kLoadSignedByte, mips::A0, mips::A1, 0x12345678);
  __ LoadFromOffset(mips::kLoadSignedByte, mips::A0, mips::A1, -256);
  __ LoadFromOffset(mips::kLoadSignedByte, mips::A0, mips::A1, 0xFFFF8000);
  __ LoadFromOffset(mips::kLoadSignedByte, mips::A0, mips::A1, 0xABCDEF00);

  __ LoadFromOffset(mips::kLoadUnsignedByte, mips::A0, mips::A0, 0);
  __ LoadFromOffset(mips::kLoadUnsignedByte, mips::A0, mips::A1, 0);
  __ LoadFromOffset(mips::kLoadUnsignedByte, mips::A0, mips::A1, 256);
  __ LoadFromOffset(mips::kLoadUnsignedByte, mips::A0, mips::A1, 1000);
  __ LoadFromOffset(mips::kLoadUnsignedByte, mips::A0, mips::A1, 0x8000);
  __ LoadFromOffset(mips::kLoadUnsignedByte, mips::A0, mips::A1, 0x10000);
  __ LoadFromOffset(mips::kLoadUnsignedByte, mips::A0, mips::A1, 0x12345678);
  __ LoadFromOffset(mips::kLoadUnsignedByte, mips::A0, mips::A1, -256);
  __ LoadFromOffset(mips::kLoadUnsignedByte, mips::A0, mips::A1, 0xFFFF8000);
  __ LoadFromOffset(mips::kLoadUnsignedByte, mips::A0, mips::A1, 0xABCDEF00);

  __ LoadFromOffset(mips::kLoadSignedHalfword, mips::A0, mips::A0, 0);
  __ LoadFromOffset(mips::kLoadSignedHalfword, mips::A0, mips::A1, 0);
  __ LoadFromOffset(mips::kLoadSignedHalfword, mips::A0, mips::A1, 256);
  __ LoadFromOffset(mips::kLoadSignedHalfword, mips::A0, mips::A1, 1000);
  __ LoadFromOffset(mips::kLoadSignedHalfword, mips::A0, mips::A1, 0x8000);
  __ LoadFromOffset(mips::kLoadSignedHalfword, mips::A0, mips::A1, 0x10000);
  __ LoadFromOffset(mips::kLoadSignedHalfword, mips::A0, mips::A1, 0x12345678);
  __ LoadFromOffset(mips::kLoadSignedHalfword, mips::A0, mips::A1, -256);
  __ LoadFromOffset(mips::kLoadSignedHalfword, mips::A0, mips::A1, 0xFFFF8000);
  __ LoadFromOffset(mips::kLoadSignedHalfword, mips::A0, mips::A1, 0xABCDEF00);

  __ LoadFromOffset(mips::kLoadUnsignedHalfword, mips::A0, mips::A0, 0);
  __ LoadFromOffset(mips::kLoadUnsignedHalfword, mips::A0, mips::A1, 0);
  __ LoadFromOffset(mips::kLoadUnsignedHalfword, mips::A0, mips::A1, 256);
  __ LoadFromOffset(mips::kLoadUnsignedHalfword, mips::A0, mips::A1, 1000);
  __ LoadFromOffset(mips::kLoadUnsignedHalfword, mips::A0, mips::A1, 0x8000);
  __ LoadFromOffset(mips::kLoadUnsignedHalfword, mips::A0, mips::A1, 0x10000);
  __ LoadFromOffset(mips::kLoadUnsignedHalfword, mips::A0, mips::A1, 0x12345678);
  __ LoadFromOffset(mips::kLoadUnsignedHalfword, mips::A0, mips::A1, -256);
  __ LoadFromOffset(mips::kLoadUnsignedHalfword, mips::A0, mips::A1, 0xFFFF8000);
  __ LoadFromOffset(mips::kLoadUnsignedHalfword, mips::A0, mips::A1, 0xABCDEF00);

  __ LoadFromOffset(mips::kLoadWord, mips::A0, mips::A0, 0);
  __ LoadFromOffset(mips::kLoadWord, mips::A0, mips::A1, 0);
  __ LoadFromOffset(mips::kLoadWord, mips::A0, mips::A1, 256);
  __ LoadFromOffset(mips::kLoadWord, mips::A0, mips::A1, 1000);
  __ LoadFromOffset(mips::kLoadWord, mips::A0, mips::A1, 0x8000);
  __ LoadFromOffset(mips::kLoadWord, mips::A0, mips::A1, 0x10000);
  __ LoadFromOffset(mips::kLoadWord, mips::A0, mips::A1, 0x12345678);
  __ LoadFromOffset(mips::kLoadWord, mips::A0, mips::A1, -256);
  __ LoadFromOffset(mips::kLoadWord, mips::A0, mips::A1, 0xFFFF8000);
  __ LoadFromOffset(mips::kLoadWord, mips::A0, mips::A1, 0xABCDEF00);

  __ LoadFromOffset(mips::kLoadDoubleword, mips::A0, mips::A0, 0);
  __ LoadFromOffset(mips::kLoadDoubleword, mips::A0, mips::A1, 0);
  __ LoadFromOffset(mips::kLoadDoubleword, mips::A1, mips::A0, 0);
  __ LoadFromOffset(mips::kLoadDoubleword, mips::A0, mips::A2, 0);
  __ LoadFromOffset(mips::kLoadDoubleword, mips::A0, mips::A2, 256);
  __ LoadFromOffset(mips::kLoadDoubleword, mips::A0, mips::A2, 1000);
  __ LoadFromOffset(mips::kLoadDoubleword, mips::A0, mips::A2, 0x8000);
  __ LoadFromOffset(mips::kLoadDoubleword, mips::A0, mips::A2, 0x10000);
  __ LoadFromOffset(mips::kLoadDoubleword, mips::A0, mips::A2, 0x12345678);
  __ LoadFromOffset(mips::kLoadDoubleword, mips::A0, mips::A2, -256);
  __ LoadFromOffset(mips::kLoadDoubleword, mips::A0, mips::A2, 0xFFFF8000);
  __ LoadFromOffset(mips::kLoadDoubleword, mips::A0, mips::A2, 0xABCDEF00);

  const char* expected =
      "lb $a0, 0($a0)\n"
      "lb $a0, 0($a1)\n"
      "lb $a0, 256($a1)\n"
      "lb $a0, 1000($a1)\n"
      "ori $at, $zero, 0x8000\n"
      "addu $at, $at, $a1\n"
      "lb $a0, 0($at)\n"
      "lui $at, 1\n"
      "addu $at, $at, $a1\n"
      "lb $a0, 0($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, 0x5678\n"
      "addu $at, $at, $a1\n"
      "lb $a0, 0($at)\n"
      "lb $a0, -256($a1)\n"
      "lb $a0, 0xFFFF8000($a1)\n"
      "lui $at, 0xABCD\n"
      "ori $at, 0xEF00\n"
      "addu $at, $at, $a1\n"
      "lb $a0, 0($at)\n"

      "lbu $a0, 0($a0)\n"
      "lbu $a0, 0($a1)\n"
      "lbu $a0, 256($a1)\n"
      "lbu $a0, 1000($a1)\n"
      "ori $at, $zero, 0x8000\n"
      "addu $at, $at, $a1\n"
      "lbu $a0, 0($at)\n"
      "lui $at, 1\n"
      "addu $at, $at, $a1\n"
      "lbu $a0, 0($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, 0x5678\n"
      "addu $at, $at, $a1\n"
      "lbu $a0, 0($at)\n"
      "lbu $a0, -256($a1)\n"
      "lbu $a0, 0xFFFF8000($a1)\n"
      "lui $at, 0xABCD\n"
      "ori $at, 0xEF00\n"
      "addu $at, $at, $a1\n"
      "lbu $a0, 0($at)\n"

      "lh $a0, 0($a0)\n"
      "lh $a0, 0($a1)\n"
      "lh $a0, 256($a1)\n"
      "lh $a0, 1000($a1)\n"
      "ori $at, $zero, 0x8000\n"
      "addu $at, $at, $a1\n"
      "lh $a0, 0($at)\n"
      "lui $at, 1\n"
      "addu $at, $at, $a1\n"
      "lh $a0, 0($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, 0x5678\n"
      "addu $at, $at, $a1\n"
      "lh $a0, 0($at)\n"
      "lh $a0, -256($a1)\n"
      "lh $a0, 0xFFFF8000($a1)\n"
      "lui $at, 0xABCD\n"
      "ori $at, 0xEF00\n"
      "addu $at, $at, $a1\n"
      "lh $a0, 0($at)\n"

      "lhu $a0, 0($a0)\n"
      "lhu $a0, 0($a1)\n"
      "lhu $a0, 256($a1)\n"
      "lhu $a0, 1000($a1)\n"
      "ori $at, $zero, 0x8000\n"
      "addu $at, $at, $a1\n"
      "lhu $a0, 0($at)\n"
      "lui $at, 1\n"
      "addu $at, $at, $a1\n"
      "lhu $a0, 0($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, 0x5678\n"
      "addu $at, $at, $a1\n"
      "lhu $a0, 0($at)\n"
      "lhu $a0, -256($a1)\n"
      "lhu $a0, 0xFFFF8000($a1)\n"
      "lui $at, 0xABCD\n"
      "ori $at, 0xEF00\n"
      "addu $at, $at, $a1\n"
      "lhu $a0, 0($at)\n"

      "lw $a0, 0($a0)\n"
      "lw $a0, 0($a1)\n"
      "lw $a0, 256($a1)\n"
      "lw $a0, 1000($a1)\n"
      "ori $at, $zero, 0x8000\n"
      "addu $at, $at, $a1\n"
      "lw $a0, 0($at)\n"
      "lui $at, 1\n"
      "addu $at, $at, $a1\n"
      "lw $a0, 0($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, 0x5678\n"
      "addu $at, $at, $a1\n"
      "lw $a0, 0($at)\n"
      "lw $a0, -256($a1)\n"
      "lw $a0, 0xFFFF8000($a1)\n"
      "lui $at, 0xABCD\n"
      "ori $at, 0xEF00\n"
      "addu $at, $at, $a1\n"
      "lw $a0, 0($at)\n"

      "lw $a1, 4($a0)\n"
      "lw $a0, 0($a0)\n"
      "lw $a0, 0($a1)\n"
      "lw $a1, 4($a1)\n"
      "lw $a1, 0($a0)\n"
      "lw $a2, 4($a0)\n"
      "lw $a0, 0($a2)\n"
      "lw $a1, 4($a2)\n"
      "lw $a0, 256($a2)\n"
      "lw $a1, 260($a2)\n"
      "lw $a0, 1000($a2)\n"
      "lw $a1, 1004($a2)\n"
      "ori $at, $zero, 0x8000\n"
      "addu $at, $at, $a2\n"
      "lw $a0, 0($at)\n"
      "lw $a1, 4($at)\n"
      "lui $at, 1\n"
      "addu $at, $at, $a2\n"
      "lw $a0, 0($at)\n"
      "lw $a1, 4($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, 0x5678\n"
      "addu $at, $at, $a2\n"
      "lw $a0, 0($at)\n"
      "lw $a1, 4($at)\n"
      "lw $a0, -256($a2)\n"
      "lw $a1, -252($a2)\n"
      "lw $a0, 0xFFFF8000($a2)\n"
      "lw $a1, 0xFFFF8004($a2)\n"
      "lui $at, 0xABCD\n"
      "ori $at, 0xEF00\n"
      "addu $at, $at, $a2\n"
      "lw $a0, 0($at)\n"
      "lw $a1, 4($at)\n";
  DriverStr(expected, "LoadFromOffset");
}

TEST_F(AssemblerMIPS32r6Test, LoadSFromOffset) {
  __ LoadSFromOffset(mips::F0, mips::A0, 0);
  __ LoadSFromOffset(mips::F0, mips::A0, 4);
  __ LoadSFromOffset(mips::F0, mips::A0, 256);
  __ LoadSFromOffset(mips::F0, mips::A0, 0x8000);
  __ LoadSFromOffset(mips::F0, mips::A0, 0x10000);
  __ LoadSFromOffset(mips::F0, mips::A0, 0x12345678);
  __ LoadSFromOffset(mips::F0, mips::A0, -256);
  __ LoadSFromOffset(mips::F0, mips::A0, 0xFFFF8000);
  __ LoadSFromOffset(mips::F0, mips::A0, 0xABCDEF00);

  const char* expected =
      "lwc1 $f0, 0($a0)\n"
      "lwc1 $f0, 4($a0)\n"
      "lwc1 $f0, 256($a0)\n"
      "ori $at, $zero, 0x8000\n"
      "addu $at, $at, $a0\n"
      "lwc1 $f0, 0($at)\n"
      "lui $at, 1\n"
      "addu $at, $at, $a0\n"
      "lwc1 $f0, 0($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, 0x5678\n"
      "addu $at, $at, $a0\n"
      "lwc1 $f0, 0($at)\n"
      "lwc1 $f0, -256($a0)\n"
      "lwc1 $f0, 0xFFFF8000($a0)\n"
      "lui $at, 0xABCD\n"
      "ori $at, 0xEF00\n"
      "addu $at, $at, $a0\n"
      "lwc1 $f0, 0($at)\n";
  DriverStr(expected, "LoadSFromOffset");
}


TEST_F(AssemblerMIPS32r6Test, LoadDFromOffset) {
  __ LoadDFromOffset(mips::F0, mips::A0, 0);
  __ LoadDFromOffset(mips::F0, mips::A0, 4);
  __ LoadDFromOffset(mips::F0, mips::A0, 256);
  __ LoadDFromOffset(mips::F0, mips::A0, 0x8000);
  __ LoadDFromOffset(mips::F0, mips::A0, 0x10000);
  __ LoadDFromOffset(mips::F0, mips::A0, 0x12345678);
  __ LoadDFromOffset(mips::F0, mips::A0, -256);
  __ LoadDFromOffset(mips::F0, mips::A0, 0xFFFF8000);
  __ LoadDFromOffset(mips::F0, mips::A0, 0xABCDEF00);

  const char* expected =
      ".set noreorder\n"
      ".set nomacro\n"
      ".set noat\n"
      "ldc1 $f0, 0($a0)\n"
      "lwc1 $f0, 4($a0)\n"
      "lw $t8, 8($a0)\n"
      "mthc1 $t8, $f0\n"
      "ldc1 $f0, 256($a0)\n"
      "ori $at, $zero, 0x8000\n"
      "addu $at, $at, $a0\n"
      "ldc1 $f0, 0($at)\n"
      "lui $at, 1\n"
      "addu $at, $at, $a0\n"
      "ldc1 $f0, 0($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, 0x5678\n"
      "addu $at, $at, $a0\n"
      "ldc1 $f0, 0($at)\n"
      "ldc1 $f0, -256($a0)\n"
      "ldc1 $f0, 0xFFFF8000($a0)\n"
      "lui $at, 0xABCD\n"
      "ori $at, 0xEF00\n"
      "addu $at, $at, $a0\n"
      "ldc1 $f0, 0($at)\n";
  DriverStr(expected, "LoadDFromOffset");
}

TEST_F(AssemblerMIPS32r6Test, StoreToOffset) {
  __ StoreToOffset(mips::kStoreByte, mips::A0, mips::A0, 0);
  __ StoreToOffset(mips::kStoreByte, mips::A0, mips::A1, 0);
  __ StoreToOffset(mips::kStoreByte, mips::A0, mips::A1, 256);
  __ StoreToOffset(mips::kStoreByte, mips::A0, mips::A1, 1000);
  __ StoreToOffset(mips::kStoreByte, mips::A0, mips::A1, 0x8000);
  __ StoreToOffset(mips::kStoreByte, mips::A0, mips::A1, 0x10000);
  __ StoreToOffset(mips::kStoreByte, mips::A0, mips::A1, 0x12345678);
  __ StoreToOffset(mips::kStoreByte, mips::A0, mips::A1, -256);
  __ StoreToOffset(mips::kStoreByte, mips::A0, mips::A1, 0xFFFF8000);
  __ StoreToOffset(mips::kStoreByte, mips::A0, mips::A1, 0xABCDEF00);

  __ StoreToOffset(mips::kStoreHalfword, mips::A0, mips::A0, 0);
  __ StoreToOffset(mips::kStoreHalfword, mips::A0, mips::A1, 0);
  __ StoreToOffset(mips::kStoreHalfword, mips::A0, mips::A1, 256);
  __ StoreToOffset(mips::kStoreHalfword, mips::A0, mips::A1, 1000);
  __ StoreToOffset(mips::kStoreHalfword, mips::A0, mips::A1, 0x8000);
  __ StoreToOffset(mips::kStoreHalfword, mips::A0, mips::A1, 0x10000);
  __ StoreToOffset(mips::kStoreHalfword, mips::A0, mips::A1, 0x12345678);
  __ StoreToOffset(mips::kStoreHalfword, mips::A0, mips::A1, -256);
  __ StoreToOffset(mips::kStoreHalfword, mips::A0, mips::A1, 0xFFFF8000);
  __ StoreToOffset(mips::kStoreHalfword, mips::A0, mips::A1, 0xABCDEF00);

  __ StoreToOffset(mips::kStoreWord, mips::A0, mips::A0, 0);
  __ StoreToOffset(mips::kStoreWord, mips::A0, mips::A1, 0);
  __ StoreToOffset(mips::kStoreWord, mips::A0, mips::A1, 256);
  __ StoreToOffset(mips::kStoreWord, mips::A0, mips::A1, 1000);
  __ StoreToOffset(mips::kStoreWord, mips::A0, mips::A1, 0x8000);
  __ StoreToOffset(mips::kStoreWord, mips::A0, mips::A1, 0x10000);
  __ StoreToOffset(mips::kStoreWord, mips::A0, mips::A1, 0x12345678);
  __ StoreToOffset(mips::kStoreWord, mips::A0, mips::A1, -256);
  __ StoreToOffset(mips::kStoreWord, mips::A0, mips::A1, 0xFFFF8000);
  __ StoreToOffset(mips::kStoreWord, mips::A0, mips::A1, 0xABCDEF00);

  __ StoreToOffset(mips::kStoreDoubleword, mips::A0, mips::A2, 0);
  __ StoreToOffset(mips::kStoreDoubleword, mips::A0, mips::A2, 256);
  __ StoreToOffset(mips::kStoreDoubleword, mips::A0, mips::A2, 1000);
  __ StoreToOffset(mips::kStoreDoubleword, mips::A0, mips::A2, 0x8000);
  __ StoreToOffset(mips::kStoreDoubleword, mips::A0, mips::A2, 0x10000);
  __ StoreToOffset(mips::kStoreDoubleword, mips::A0, mips::A2, 0x12345678);
  __ StoreToOffset(mips::kStoreDoubleword, mips::A0, mips::A2, -256);
  __ StoreToOffset(mips::kStoreDoubleword, mips::A0, mips::A2, 0xFFFF8000);
  __ StoreToOffset(mips::kStoreDoubleword, mips::A0, mips::A2, 0xABCDEF00);

  const char* expected =
      "sb $a0, 0($a0)\n"
      "sb $a0, 0($a1)\n"
      "sb $a0, 256($a1)\n"
      "sb $a0, 1000($a1)\n"
      "ori $at, $zero, 0x8000\n"
      "addu $at, $at, $a1\n"
      "sb $a0, 0($at)\n"
      "lui $at, 1\n"
      "addu $at, $at, $a1\n"
      "sb $a0, 0($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, 0x5678\n"
      "addu $at, $at, $a1\n"
      "sb $a0, 0($at)\n"
      "sb $a0, -256($a1)\n"
      "sb $a0, 0xFFFF8000($a1)\n"
      "lui $at, 0xABCD\n"
      "ori $at, 0xEF00\n"
      "addu $at, $at, $a1\n"
      "sb $a0, 0($at)\n"

      "sh $a0, 0($a0)\n"
      "sh $a0, 0($a1)\n"
      "sh $a0, 256($a1)\n"
      "sh $a0, 1000($a1)\n"
      "ori $at, $zero, 0x8000\n"
      "addu $at, $at, $a1\n"
      "sh $a0, 0($at)\n"
      "lui $at, 1\n"
      "addu $at, $at, $a1\n"
      "sh $a0, 0($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, 0x5678\n"
      "addu $at, $at, $a1\n"
      "sh $a0, 0($at)\n"
      "sh $a0, -256($a1)\n"
      "sh $a0, 0xFFFF8000($a1)\n"
      "lui $at, 0xABCD\n"
      "ori $at, 0xEF00\n"
      "addu $at, $at, $a1\n"
      "sh $a0, 0($at)\n"

      "sw $a0, 0($a0)\n"
      "sw $a0, 0($a1)\n"
      "sw $a0, 256($a1)\n"
      "sw $a0, 1000($a1)\n"
      "ori $at, $zero, 0x8000\n"
      "addu $at, $at, $a1\n"
      "sw $a0, 0($at)\n"
      "lui $at, 1\n"
      "addu $at, $at, $a1\n"
      "sw $a0, 0($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, 0x5678\n"
      "addu $at, $at, $a1\n"
      "sw $a0, 0($at)\n"
      "sw $a0, -256($a1)\n"
      "sw $a0, 0xFFFF8000($a1)\n"
      "lui $at, 0xABCD\n"
      "ori $at, 0xEF00\n"
      "addu $at, $at, $a1\n"
      "sw $a0, 0($at)\n"

      "sw $a0, 0($a2)\n"
      "sw $a1, 4($a2)\n"
      "sw $a0, 256($a2)\n"
      "sw $a1, 260($a2)\n"
      "sw $a0, 1000($a2)\n"
      "sw $a1, 1004($a2)\n"
      "ori $at, $zero, 0x8000\n"
      "addu $at, $at, $a2\n"
      "sw $a0, 0($at)\n"
      "sw $a1, 4($at)\n"
      "lui $at, 1\n"
      "addu $at, $at, $a2\n"
      "sw $a0, 0($at)\n"
      "sw $a1, 4($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, 0x5678\n"
      "addu $at, $at, $a2\n"
      "sw $a0, 0($at)\n"
      "sw $a1, 4($at)\n"
      "sw $a0, -256($a2)\n"
      "sw $a1, -252($a2)\n"
      "sw $a0, 0xFFFF8000($a2)\n"
      "sw $a1, 0xFFFF8004($a2)\n"
      "lui $at, 0xABCD\n"
      "ori $at, 0xEF00\n"
      "addu $at, $at, $a2\n"
      "sw $a0, 0($at)\n"
      "sw $a1, 4($at)\n";
  DriverStr(expected, "StoreToOffset");
}

TEST_F(AssemblerMIPS32r6Test, StoreSToOffset) {
  __ StoreSToOffset(mips::F0, mips::A0, 0);
  __ StoreSToOffset(mips::F0, mips::A0, 4);
  __ StoreSToOffset(mips::F0, mips::A0, 256);
  __ StoreSToOffset(mips::F0, mips::A0, 0x8000);
  __ StoreSToOffset(mips::F0, mips::A0, 0x10000);
  __ StoreSToOffset(mips::F0, mips::A0, 0x12345678);
  __ StoreSToOffset(mips::F0, mips::A0, -256);
  __ StoreSToOffset(mips::F0, mips::A0, 0xFFFF8000);
  __ StoreSToOffset(mips::F0, mips::A0, 0xABCDEF00);

  const char* expected =
      "swc1 $f0, 0($a0)\n"
      "swc1 $f0, 4($a0)\n"
      "swc1 $f0, 256($a0)\n"
      "ori $at, $zero, 0x8000\n"
      "addu $at, $at, $a0\n"
      "swc1 $f0, 0($at)\n"
      "lui $at, 1\n"
      "addu $at, $at, $a0\n"
      "swc1 $f0, 0($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, 0x5678\n"
      "addu $at, $at, $a0\n"
      "swc1 $f0, 0($at)\n"
      "swc1 $f0, -256($a0)\n"
      "swc1 $f0, 0xFFFF8000($a0)\n"
      "lui $at, 0xABCD\n"
      "ori $at, 0xEF00\n"
      "addu $at, $at, $a0\n"
      "swc1 $f0, 0($at)\n";
  DriverStr(expected, "StoreSToOffset");
}

TEST_F(AssemblerMIPS32r6Test, StoreDToOffset) {
  __ StoreDToOffset(mips::F0, mips::A0, 0);
  __ StoreDToOffset(mips::F0, mips::A0, 4);
  __ StoreDToOffset(mips::F0, mips::A0, 256);
  __ StoreDToOffset(mips::F0, mips::A0, 0x8000);
  __ StoreDToOffset(mips::F0, mips::A0, 0x10000);
  __ StoreDToOffset(mips::F0, mips::A0, 0x12345678);
  __ StoreDToOffset(mips::F0, mips::A0, -256);
  __ StoreDToOffset(mips::F0, mips::A0, 0xFFFF8000);
  __ StoreDToOffset(mips::F0, mips::A0, 0xABCDEF00);

  const char* expected =
      "sdc1 $f0, 0($a0)\n"
      "mfhc1 $t8, $f0\n"
      "swc1 $f0, 4($a0)\n"
      "sw $t8, 8($a0)\n"
      "sdc1 $f0, 256($a0)\n"
      "ori $at, $zero, 0x8000\n"
      "addu $at, $at, $a0\n"
      "sdc1 $f0, 0($at)\n"
      "lui $at, 1\n"
      "addu $at, $at, $a0\n"
      "sdc1 $f0, 0($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, 0x5678\n"
      "addu $at, $at, $a0\n"
      "sdc1 $f0, 0($at)\n"
      "sdc1 $f0, -256($a0)\n"
      "sdc1 $f0, 0xFFFF8000($a0)\n"
      "lui $at, 0xABCD\n"
      "ori $at, 0xEF00\n"
      "addu $at, $at, $a0\n"
      "sdc1 $f0, 0($at)\n";
  DriverStr(expected, "StoreDToOffset");
}

//////////////
// BRANCHES //
//////////////

// TODO: MipsAssembler::Auipc
//       MipsAssembler::Addiupc
//       MipsAssembler::Bc
//       MipsAssembler::Jic
//       MipsAssembler::Jialc
//       MipsAssembler::Bltc
//       MipsAssembler::Bltzc
//       MipsAssembler::Bgtzc
//       MipsAssembler::Bgec
//       MipsAssembler::Bgezc
//       MipsAssembler::Blezc
//       MipsAssembler::Bltuc
//       MipsAssembler::Bgeuc
//       MipsAssembler::Beqc
//       MipsAssembler::Bnec
//       MipsAssembler::Beqzc
//       MipsAssembler::Bnezc
//       MipsAssembler::Bc1eqz
//       MipsAssembler::Bc1nez
//       MipsAssembler::Buncond
//       MipsAssembler::Bcond
//       MipsAssembler::Call

// TODO:  AssemblerMIPS32r6Test.B
//        AssemblerMIPS32r6Test.Beq
//        AssemblerMIPS32r6Test.Bne
//        AssemblerMIPS32r6Test.Beqz
//        AssemblerMIPS32r6Test.Bnez
//        AssemblerMIPS32r6Test.Bltz
//        AssemblerMIPS32r6Test.Bgez
//        AssemblerMIPS32r6Test.Blez
//        AssemblerMIPS32r6Test.Bgtz
//        AssemblerMIPS32r6Test.Blt
//        AssemblerMIPS32r6Test.Bge
//        AssemblerMIPS32r6Test.Bltu
//        AssemblerMIPS32r6Test.Bgeu

///////////////////////
// Loading Constants //
///////////////////////

TEST_F(AssemblerMIPS32r6Test, LoadConst32) {
  // IsUint<16>(value)
  __ LoadConst32(mips::V0, 0);
  __ LoadConst32(mips::V0, 65535);
  // IsInt<16>(value)
  __ LoadConst32(mips::V0, -1);
  __ LoadConst32(mips::V0, -32768);
  // Everything else
  __ LoadConst32(mips::V0, 65536);
  __ LoadConst32(mips::V0, 65537);
  __ LoadConst32(mips::V0, 2147483647);
  __ LoadConst32(mips::V0, -32769);
  __ LoadConst32(mips::V0, -65536);
  __ LoadConst32(mips::V0, -65537);
  __ LoadConst32(mips::V0, -2147483647);
  __ LoadConst32(mips::V0, -2147483648);

  const char* expected =
      // IsUint<16>(value)
      "ori $v0, $zero, 0\n"         // __ LoadConst32(mips::V0, 0);
      "ori $v0, $zero, 65535\n"     // __ LoadConst32(mips::V0, 65535);
      // IsInt<16>(value)
      "addiu $v0, $zero, -1\n"      // __ LoadConst32(mips::V0, -1);
      "addiu $v0, $zero, -32768\n"  // __ LoadConst32(mips::V0, -32768);
      // Everything else
      "lui $v0, 1\n"                // __ LoadConst32(mips::V0, 65536);
      "lui $v0, 1\n"                // __ LoadConst32(mips::V0, 65537);
      "ori $v0, 1\n"                //                 "
      "lui $v0, 32767\n"            // __ LoadConst32(mips::V0, 2147483647);
      "ori $v0, 65535\n"            //                 "
      "lui $v0, 65535\n"            // __ LoadConst32(mips::V0, -32769);
      "ori $v0, 32767\n"            //                 "
      "lui $v0, 65535\n"            // __ LoadConst32(mips::V0, -65536);
      "lui $v0, 65534\n"            // __ LoadConst32(mips::V0, -65537);
      "ori $v0, 65535\n"            //                 "
      "lui $v0, 32768\n"            // __ LoadConst32(mips::V0, -2147483647);
      "ori $v0, 1\n"                //                 "
      "lui $v0, 32768\n";           // __ LoadConst32(mips::V0, -2147483648);
  DriverStr(expected, "LoadConst32");
}

#undef __

}  // namespace art
