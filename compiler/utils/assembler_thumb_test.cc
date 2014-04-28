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

#include "gtest/gtest.h"
#include "utils/arm/assembler_thumb2.h"
#include "base/hex_dump.h"
#include <fstream>

namespace art {
namespace arm {

void dump(std::vector<uint8_t>& code) {
  int pid = getpid();
  char filename[256];
  snprintf(filename, sizeof(filename), "/tmp/thumb-test-%d.S", pid);
  std::ofstream out(filename);
  if (out) {
    out << ".section \".text\"\n";
    out << ".syntax unified\n";
    out << ".arch armv7-a\n";
    out << ".thumb\n";
    out << ".thumb_func\n";
    out << ".type testfunc, #function\n";
    out << ".global testfunc\n";
    out << "testfunc:\n";
    out << ".fnstart\n";

    for (uint32_t i = 0 ; i < code.size(); ++i) {
      out << ".byte " << (static_cast<int>(code[i]) & 0xff) << "\n";
    }
    out << ".fnend\n";
    out << ".size testfunc, .-testfunc\n";
  }
  out.close();
  std::cout << filename << "\n";

  char cmd[256];

  // Assemble the .S
  snprintf(cmd, sizeof(cmd), "arm-eabi-as %s -o %s.o", filename, filename);
  system(cmd);

  // Remove the $d symbols to prevent the disassembler dumping the instructions
  // as .word
  snprintf(cmd, sizeof(cmd), "arm-eabi-objcopy -N '$d' %s.o %s.oo", filename, filename);
  system(cmd);

  // Disassemble.
  snprintf(cmd, sizeof(cmd), "arm-eabi-objdump -d %s.oo", filename);
  system(cmd);

  unlink(filename);

  snprintf(filename, sizeof(filename), "%s.o", filename);
  unlink(filename);

  snprintf(filename, sizeof(filename), "%so", filename);
  unlink(filename);
}

#define __ assembler->

TEST(Thumb2, SimpleMov) {
  arm::Thumb2Assembler* assembler = static_cast<arm::Thumb2Assembler*>(Assembler::Create(kThumb2));

  __ mov(R0, ShifterOperand(R1));
  __ mov(R8, ShifterOperand(R9));

  size_t cs = __ CodeSize();
  std::vector<uint8_t> managed_code(cs);
  MemoryRegion code(&managed_code[0], managed_code.size());
  __ FinalizeInstructions(code);
  dump(managed_code);
  delete assembler;
}

TEST(Thumb2, SimpleMov32) {
  arm::Thumb2Assembler* assembler = static_cast<arm::Thumb2Assembler*>(Assembler::Create(kThumb2));
  assembler->Force32Bit();

  __ mov(R0, ShifterOperand(R1));
  __ mov(R8, ShifterOperand(R9));

  size_t cs = __ CodeSize();
  std::vector<uint8_t> managed_code(cs);
  MemoryRegion code(&managed_code[0], managed_code.size());
  __ FinalizeInstructions(code);
  dump(managed_code);
  delete assembler;
}

TEST(Thumb2, SimpleMovAdd) {
  arm::Thumb2Assembler* assembler = static_cast<arm::Thumb2Assembler*>(Assembler::Create(kThumb2));

  __ mov(R0, ShifterOperand(R1));
  __ add(R0, R1, ShifterOperand(R2));
  __ add(R0, R1, ShifterOperand());

  size_t cs = __ CodeSize();
  std::vector<uint8_t> managed_code(cs);
  MemoryRegion code(&managed_code[0], managed_code.size());
  __ FinalizeInstructions(code);
  dump(managed_code);
  delete assembler;
}

TEST(Thumb2, DataProcessingRegister) {
  arm::Thumb2Assembler* assembler = static_cast<arm::Thumb2Assembler*>(Assembler::Create(kThumb2));

  __ mov(R0, ShifterOperand(R1));
  __ mvn(R0, ShifterOperand(R1));

  // 32 bit variants.
  __ add(R0, R1, ShifterOperand(R2));
  __ sub(R0, R1, ShifterOperand(R2));
  __ and_(R0, R1, ShifterOperand(R2));
  __ orr(R0, R1, ShifterOperand(R2));
  __ eor(R0, R1, ShifterOperand(R2));
  __ bic(R0, R1, ShifterOperand(R2));
  __ adc(R0, R1, ShifterOperand(R2));
  __ sbc(R0, R1, ShifterOperand(R2));
  __ rsb(R0, R1, ShifterOperand(R2));

  // 16 bit variants.
  __ add(R0, R1, ShifterOperand());
  __ sub(R0, R1, ShifterOperand());
  __ and_(R0, R1, ShifterOperand());
  __ orr(R0, R1, ShifterOperand());
  __ eor(R0, R1, ShifterOperand());
  __ bic(R0, R1, ShifterOperand());
  __ adc(R0, R1, ShifterOperand());
  __ sbc(R0, R1, ShifterOperand());
  __ rsb(R0, R1, ShifterOperand());

  __ tst(R0, ShifterOperand(R1));
  __ teq(R0, ShifterOperand(R1));
  __ cmp(R0, ShifterOperand(R1));
  __ cmn(R0, ShifterOperand(R1));

  size_t cs = __ CodeSize();
  std::vector<uint8_t> managed_code(cs);
  MemoryRegion code(&managed_code[0], managed_code.size());
  __ FinalizeInstructions(code);
  dump(managed_code);
  delete assembler;
}

TEST(Thumb2, DataProcessingImmediate) {
  arm::Thumb2Assembler* assembler = static_cast<arm::Thumb2Assembler*>(Assembler::Create(kThumb2));

  __ mov(R0, ShifterOperand(0x55));
  __ mvn(R0, ShifterOperand(0x55));
  __ add(R0, R1, ShifterOperand(0x55));
  __ sub(R0, R1, ShifterOperand(0x55));
  __ and_(R0, R1, ShifterOperand(0x55));
  __ orr(R0, R1, ShifterOperand(0x55));
  __ eor(R0, R1, ShifterOperand(0x55));
  __ bic(R0, R1, ShifterOperand(0x55));
  __ adc(R0, R1, ShifterOperand(0x55));
  __ sbc(R0, R1, ShifterOperand(0x55));
  __ rsb(R0, R1, ShifterOperand(0x55));

  __ tst(R0, ShifterOperand(0x55));
  __ teq(R0, ShifterOperand(0x55));
  __ cmp(R0, ShifterOperand(0x55));
  __ cmn(R0, ShifterOperand(0x55));

  __ add(R0, R1, ShifterOperand(5));
  __ sub(R0, R1, ShifterOperand(5));

  size_t cs = __ CodeSize();
  std::vector<uint8_t> managed_code(cs);
  MemoryRegion code(&managed_code[0], managed_code.size());
  __ FinalizeInstructions(code);
  dump(managed_code);
  delete assembler;
}

TEST(Thumb2, DataProcessingModifiedImmediate) {
  arm::Thumb2Assembler* assembler = static_cast<arm::Thumb2Assembler*>(Assembler::Create(kThumb2));

  __ mov(R0, ShifterOperand(0x550055));
  __ mvn(R0, ShifterOperand(0x550055));
  __ add(R0, R1, ShifterOperand(0x550055));
  __ sub(R0, R1, ShifterOperand(0x550055));
  __ and_(R0, R1, ShifterOperand(0x550055));
  __ orr(R0, R1, ShifterOperand(0x550055));
  __ eor(R0, R1, ShifterOperand(0x550055));
  __ bic(R0, R1, ShifterOperand(0x550055));
  __ adc(R0, R1, ShifterOperand(0x550055));
  __ sbc(R0, R1, ShifterOperand(0x550055));
  __ rsb(R0, R1, ShifterOperand(0x550055));

  __ tst(R0, ShifterOperand(0x550055));
  __ teq(R0, ShifterOperand(0x550055));
  __ cmp(R0, ShifterOperand(0x550055));
  __ cmn(R0, ShifterOperand(0x550055));

  size_t cs = __ CodeSize();
  std::vector<uint8_t> managed_code(cs);
  MemoryRegion code(&managed_code[0], managed_code.size());
  __ FinalizeInstructions(code);
  dump(managed_code);
  delete assembler;
}


TEST(Thumb2, DataProcessingModifiedImmediates) {
  arm::Thumb2Assembler* assembler = static_cast<arm::Thumb2Assembler*>(Assembler::Create(kThumb2));

  __ mov(R0, ShifterOperand(0x550055));
  __ mov(R0, ShifterOperand(0x55005500));
  __ mov(R0, ShifterOperand(0x55555555));
  __ mov(R0, ShifterOperand(0xd5000000));       // rotated to first position
  __ mov(R0, ShifterOperand(0x6a000000));       // rotated to second position
  __ mov(R0, ShifterOperand(0x350));            // rotated to 2nd last position
  __ mov(R0, ShifterOperand(0x1a8));            // rotated to last position

  size_t cs = __ CodeSize();
  std::vector<uint8_t> managed_code(cs);
  MemoryRegion code(&managed_code[0], managed_code.size());
  __ FinalizeInstructions(code);
  dump(managed_code);
  delete assembler;
}

TEST(Thumb2, DataProcessingShiftedRegister) {
  arm::Thumb2Assembler* assembler = static_cast<arm::Thumb2Assembler*>(Assembler::Create(kThumb2));

  __ mov(R3, ShifterOperand(R4, LSL, 4));
  __ mov(R3, ShifterOperand(R4, LSR, 5));
  __ mov(R3, ShifterOperand(R4, ASR, 6));
  __ mov(R3, ShifterOperand(R4, ROR, 7));
  __ mov(R3, ShifterOperand(R4, ROR));

  // 32 bit variants.
  __ mov(R8, ShifterOperand(R4, LSL, 4));
  __ mov(R8, ShifterOperand(R4, LSR, 5));
  __ mov(R8, ShifterOperand(R4, ASR, 6));
  __ mov(R8, ShifterOperand(R4, ROR, 7));
  __ mov(R8, ShifterOperand(R4, RRX));

  size_t cs = __ CodeSize();
  std::vector<uint8_t> managed_code(cs);
  MemoryRegion code(&managed_code[0], managed_code.size());
  __ FinalizeInstructions(code);
  dump(managed_code);
  delete assembler;
}


TEST(Thumb2, BasicLoad) {
  arm::Thumb2Assembler* assembler = static_cast<arm::Thumb2Assembler*>(Assembler::Create(kThumb2));

  __ ldr(R3, Address(R4, 24));
  __ ldrb(R3, Address(R4, 24));
  __ ldrh(R3, Address(R4, 24));
  __ ldrsb(R3, Address(R4, 24));
  __ ldrsh(R3, Address(R4, 24));

  __ ldr(R3, Address(SP, 24));

  // 32 bit variants
  __ ldr(R8, Address(R4, 24));
  __ ldrb(R8, Address(R4, 24));
  __ ldrh(R8, Address(R4, 24));
  __ ldrsb(R8, Address(R4, 24));
  __ ldrsh(R8, Address(R4, 24));

  size_t cs = __ CodeSize();
  std::vector<uint8_t> managed_code(cs);
  MemoryRegion code(&managed_code[0], managed_code.size());
  __ FinalizeInstructions(code);
  dump(managed_code);
  delete assembler;
}


TEST(Thumb2, BasicStore) {
  arm::Thumb2Assembler* assembler = static_cast<arm::Thumb2Assembler*>(Assembler::Create(kThumb2));

  __ str(R3, Address(R4, 24));
  __ strb(R3, Address(R4, 24));
  __ strh(R3, Address(R4, 24));

  __ str(R3, Address(SP, 24));

  // 32 bit variants.
  __ str(R8, Address(R4, 24));
  __ strb(R8, Address(R4, 24));
  __ strh(R8, Address(R4, 24));

  size_t cs = __ CodeSize();
  std::vector<uint8_t> managed_code(cs);
  MemoryRegion code(&managed_code[0], managed_code.size());
  __ FinalizeInstructions(code);
  dump(managed_code);
  delete assembler;
}

TEST(Thumb2, ComplexLoad) {
  arm::Thumb2Assembler* assembler = static_cast<arm::Thumb2Assembler*>(Assembler::Create(kThumb2));

  __ ldr(R3, Address(R4, 24, Address::Mode::Offset));
  __ ldr(R3, Address(R4, 24, Address::Mode::PreIndex));
  __ ldr(R3, Address(R4, 24, Address::Mode::PostIndex));
  __ ldr(R3, Address(R4, 24, Address::Mode::NegOffset));
  __ ldr(R3, Address(R4, 24, Address::Mode::NegPreIndex));
  __ ldr(R3, Address(R4, 24, Address::Mode::NegPostIndex));

  __ ldrb(R3, Address(R4, 24, Address::Mode::Offset));
  __ ldrb(R3, Address(R4, 24, Address::Mode::PreIndex));
  __ ldrb(R3, Address(R4, 24, Address::Mode::PostIndex));
  __ ldrb(R3, Address(R4, 24, Address::Mode::NegOffset));
  __ ldrb(R3, Address(R4, 24, Address::Mode::NegPreIndex));
  __ ldrb(R3, Address(R4, 24, Address::Mode::NegPostIndex));

  __ ldrh(R3, Address(R4, 24, Address::Mode::Offset));
  __ ldrh(R3, Address(R4, 24, Address::Mode::PreIndex));
  __ ldrh(R3, Address(R4, 24, Address::Mode::PostIndex));
  __ ldrh(R3, Address(R4, 24, Address::Mode::NegOffset));
  __ ldrh(R3, Address(R4, 24, Address::Mode::NegPreIndex));
  __ ldrh(R3, Address(R4, 24, Address::Mode::NegPostIndex));

  __ ldrsb(R3, Address(R4, 24, Address::Mode::Offset));
  __ ldrsb(R3, Address(R4, 24, Address::Mode::PreIndex));
  __ ldrsb(R3, Address(R4, 24, Address::Mode::PostIndex));
  __ ldrsb(R3, Address(R4, 24, Address::Mode::NegOffset));
  __ ldrsb(R3, Address(R4, 24, Address::Mode::NegPreIndex));
  __ ldrsb(R3, Address(R4, 24, Address::Mode::NegPostIndex));

  __ ldrsh(R3, Address(R4, 24, Address::Mode::Offset));
  __ ldrsh(R3, Address(R4, 24, Address::Mode::PreIndex));
  __ ldrsh(R3, Address(R4, 24, Address::Mode::PostIndex));
  __ ldrsh(R3, Address(R4, 24, Address::Mode::NegOffset));
  __ ldrsh(R3, Address(R4, 24, Address::Mode::NegPreIndex));
  __ ldrsh(R3, Address(R4, 24, Address::Mode::NegPostIndex));

  size_t cs = __ CodeSize();
  std::vector<uint8_t> managed_code(cs);
  MemoryRegion code(&managed_code[0], managed_code.size());
  __ FinalizeInstructions(code);
  dump(managed_code);
  delete assembler;
}


TEST(Thumb2, ComplexStore) {
  arm::Thumb2Assembler* assembler = static_cast<arm::Thumb2Assembler*>(Assembler::Create(kThumb2));

  __ str(R3, Address(R4, 24, Address::Mode::Offset));
  __ str(R3, Address(R4, 24, Address::Mode::PreIndex));
  __ str(R3, Address(R4, 24, Address::Mode::PostIndex));
  __ str(R3, Address(R4, 24, Address::Mode::NegOffset));
  __ str(R3, Address(R4, 24, Address::Mode::NegPreIndex));
  __ str(R3, Address(R4, 24, Address::Mode::NegPostIndex));

  __ strb(R3, Address(R4, 24, Address::Mode::Offset));
  __ strb(R3, Address(R4, 24, Address::Mode::PreIndex));
  __ strb(R3, Address(R4, 24, Address::Mode::PostIndex));
  __ strb(R3, Address(R4, 24, Address::Mode::NegOffset));
  __ strb(R3, Address(R4, 24, Address::Mode::NegPreIndex));
  __ strb(R3, Address(R4, 24, Address::Mode::NegPostIndex));

  __ strh(R3, Address(R4, 24, Address::Mode::Offset));
  __ strh(R3, Address(R4, 24, Address::Mode::PreIndex));
  __ strh(R3, Address(R4, 24, Address::Mode::PostIndex));
  __ strh(R3, Address(R4, 24, Address::Mode::NegOffset));
  __ strh(R3, Address(R4, 24, Address::Mode::NegPreIndex));
  __ strh(R3, Address(R4, 24, Address::Mode::NegPostIndex));

  size_t cs = __ CodeSize();
  std::vector<uint8_t> managed_code(cs);
  MemoryRegion code(&managed_code[0], managed_code.size());
  __ FinalizeInstructions(code);
  dump(managed_code);
  delete assembler;
}

TEST(Thumb2, NegativeLoadStore) {
  arm::Thumb2Assembler* assembler = static_cast<arm::Thumb2Assembler*>(Assembler::Create(kThumb2));

  __ ldr(R3, Address(R4, -24, Address::Mode::Offset));
  __ ldr(R3, Address(R4, -24, Address::Mode::PreIndex));
  __ ldr(R3, Address(R4, -24, Address::Mode::PostIndex));
  __ ldr(R3, Address(R4, -24, Address::Mode::NegOffset));
  __ ldr(R3, Address(R4, -24, Address::Mode::NegPreIndex));
  __ ldr(R3, Address(R4, -24, Address::Mode::NegPostIndex));

  __ ldrb(R3, Address(R4, -24, Address::Mode::Offset));
  __ ldrb(R3, Address(R4, -24, Address::Mode::PreIndex));
  __ ldrb(R3, Address(R4, -24, Address::Mode::PostIndex));
  __ ldrb(R3, Address(R4, -24, Address::Mode::NegOffset));
  __ ldrb(R3, Address(R4, -24, Address::Mode::NegPreIndex));
  __ ldrb(R3, Address(R4, -24, Address::Mode::NegPostIndex));

  __ ldrh(R3, Address(R4, -24, Address::Mode::Offset));
  __ ldrh(R3, Address(R4, -24, Address::Mode::PreIndex));
  __ ldrh(R3, Address(R4, -24, Address::Mode::PostIndex));
  __ ldrh(R3, Address(R4, -24, Address::Mode::NegOffset));
  __ ldrh(R3, Address(R4, -24, Address::Mode::NegPreIndex));
  __ ldrh(R3, Address(R4, -24, Address::Mode::NegPostIndex));

  __ ldrsb(R3, Address(R4, -24, Address::Mode::Offset));
  __ ldrsb(R3, Address(R4, -24, Address::Mode::PreIndex));
  __ ldrsb(R3, Address(R4, -24, Address::Mode::PostIndex));
  __ ldrsb(R3, Address(R4, -24, Address::Mode::NegOffset));
  __ ldrsb(R3, Address(R4, -24, Address::Mode::NegPreIndex));
  __ ldrsb(R3, Address(R4, -24, Address::Mode::NegPostIndex));

  __ ldrsh(R3, Address(R4, -24, Address::Mode::Offset));
  __ ldrsh(R3, Address(R4, -24, Address::Mode::PreIndex));
  __ ldrsh(R3, Address(R4, -24, Address::Mode::PostIndex));
  __ ldrsh(R3, Address(R4, -24, Address::Mode::NegOffset));
  __ ldrsh(R3, Address(R4, -24, Address::Mode::NegPreIndex));
  __ ldrsh(R3, Address(R4, -24, Address::Mode::NegPostIndex));

  __ str(R3, Address(R4, -24, Address::Mode::Offset));
  __ str(R3, Address(R4, -24, Address::Mode::PreIndex));
  __ str(R3, Address(R4, -24, Address::Mode::PostIndex));
  __ str(R3, Address(R4, -24, Address::Mode::NegOffset));
  __ str(R3, Address(R4, -24, Address::Mode::NegPreIndex));
  __ str(R3, Address(R4, -24, Address::Mode::NegPostIndex));

  __ strb(R3, Address(R4, -24, Address::Mode::Offset));
  __ strb(R3, Address(R4, -24, Address::Mode::PreIndex));
  __ strb(R3, Address(R4, -24, Address::Mode::PostIndex));
  __ strb(R3, Address(R4, -24, Address::Mode::NegOffset));
  __ strb(R3, Address(R4, -24, Address::Mode::NegPreIndex));
  __ strb(R3, Address(R4, -24, Address::Mode::NegPostIndex));

  __ strh(R3, Address(R4, -24, Address::Mode::Offset));
  __ strh(R3, Address(R4, -24, Address::Mode::PreIndex));
  __ strh(R3, Address(R4, -24, Address::Mode::PostIndex));
  __ strh(R3, Address(R4, -24, Address::Mode::NegOffset));
  __ strh(R3, Address(R4, -24, Address::Mode::NegPreIndex));
  __ strh(R3, Address(R4, -24, Address::Mode::NegPostIndex));

  size_t cs = __ CodeSize();
  std::vector<uint8_t> managed_code(cs);
  MemoryRegion code(&managed_code[0], managed_code.size());
  __ FinalizeInstructions(code);
  dump(managed_code);
  delete assembler;
}

TEST(Thumb2, SimpleLoadStoreDual) {
  arm::Thumb2Assembler* assembler = static_cast<arm::Thumb2Assembler*>(Assembler::Create(kThumb2));

  __ strd(R2, Address(R0, 24, Address::Mode::Offset));
  __ ldrd(R2, Address(R0, 24, Address::Mode::Offset));

  size_t cs = __ CodeSize();
  std::vector<uint8_t> managed_code(cs);
  MemoryRegion code(&managed_code[0], managed_code.size());
  __ FinalizeInstructions(code);
  dump(managed_code);
  delete assembler;
}

TEST(Thumb2, ComplexLoadStoreDual) {
  arm::Thumb2Assembler* assembler = static_cast<arm::Thumb2Assembler*>(Assembler::Create(kThumb2));

  __ strd(R2, Address(R0, 24, Address::Mode::Offset));
  __ strd(R2, Address(R0, 24, Address::Mode::PreIndex));
  __ strd(R2, Address(R0, 24, Address::Mode::PostIndex));
  __ strd(R2, Address(R0, 24, Address::Mode::NegOffset));
  __ strd(R2, Address(R0, 24, Address::Mode::NegPreIndex));
  __ strd(R2, Address(R0, 24, Address::Mode::NegPostIndex));

  __ ldrd(R2, Address(R0, 24, Address::Mode::Offset));
  __ ldrd(R2, Address(R0, 24, Address::Mode::PreIndex));
  __ ldrd(R2, Address(R0, 24, Address::Mode::PostIndex));
  __ ldrd(R2, Address(R0, 24, Address::Mode::NegOffset));
  __ ldrd(R2, Address(R0, 24, Address::Mode::NegPreIndex));
  __ ldrd(R2, Address(R0, 24, Address::Mode::NegPostIndex));

  size_t cs = __ CodeSize();
  std::vector<uint8_t> managed_code(cs);
  MemoryRegion code(&managed_code[0], managed_code.size());
  __ FinalizeInstructions(code);
  dump(managed_code);
  delete assembler;
}

TEST(Thumb2, NegativeLoadStoreDual) {
  arm::Thumb2Assembler* assembler = static_cast<arm::Thumb2Assembler*>(Assembler::Create(kThumb2));

  __ strd(R2, Address(R0, -24, Address::Mode::Offset));
  __ strd(R2, Address(R0, -24, Address::Mode::PreIndex));
  __ strd(R2, Address(R0, -24, Address::Mode::PostIndex));
  __ strd(R2, Address(R0, -24, Address::Mode::NegOffset));
  __ strd(R2, Address(R0, -24, Address::Mode::NegPreIndex));
  __ strd(R2, Address(R0, -24, Address::Mode::NegPostIndex));

  __ ldrd(R2, Address(R0, -24, Address::Mode::Offset));
  __ ldrd(R2, Address(R0, -24, Address::Mode::PreIndex));
  __ ldrd(R2, Address(R0, -24, Address::Mode::PostIndex));
  __ ldrd(R2, Address(R0, -24, Address::Mode::NegOffset));
  __ ldrd(R2, Address(R0, -24, Address::Mode::NegPreIndex));
  __ ldrd(R2, Address(R0, -24, Address::Mode::NegPostIndex));

  size_t cs = __ CodeSize();
  std::vector<uint8_t> managed_code(cs);
  MemoryRegion code(&managed_code[0], managed_code.size());
  __ FinalizeInstructions(code);
  dump(managed_code);
  delete assembler;
}


TEST(Thumb2, SimpleBranch) {
  arm::Thumb2Assembler* assembler = static_cast<arm::Thumb2Assembler*>(Assembler::Create(kThumb2));

  Label l1;
  __ mov(R0, ShifterOperand(2));
  __ Bind(&l1);
  __ mov(R1, ShifterOperand(1));
  __ b(&l1);

  Label l2;
  __ b(&l2);
  __ mov(R1, ShifterOperand(2));
  __ Bind(&l2);
  __ mov(R0, ShifterOperand(3));

  Label l3;
  __ mov(R0, ShifterOperand(2));
  __ Bind(&l3);
  __ mov(R1, ShifterOperand(1));
  __ b(&l3, EQ);

  Label l4;
  __ b(&l4, EQ);
  __ mov(R1, ShifterOperand(2));
  __ Bind(&l4);
  __ mov(R0, ShifterOperand(3));

  // 2 linked labels.
  Label l5;
  __ b(&l5);
  __ mov(R1, ShifterOperand(4));
  __ b(&l5);
  __ mov(R1, ShifterOperand(5));
  __ Bind(&l5);
  __ mov(R0, ShifterOperand(6));

  size_t cs = __ CodeSize();
  std::vector<uint8_t> managed_code(cs);
  MemoryRegion code(&managed_code[0], managed_code.size());
  __ FinalizeInstructions(code);
  dump(managed_code);
  delete assembler;
}

TEST(Thumb2, LongBranch) {
  arm::Thumb2Assembler* assembler = static_cast<arm::Thumb2Assembler*>(Assembler::Create(kThumb2));
  assembler->Force32Bit();
  // 32 bit branches.
  Label l1;
  __ mov(R0, ShifterOperand(2));
  __ Bind(&l1);
  __ mov(R1, ShifterOperand(1));
  __ b(&l1);

  Label l2;
  __ b(&l2);
  __ mov(R1, ShifterOperand(2));
  __ Bind(&l2);
  __ mov(R0, ShifterOperand(3));

  Label l3;
  __ mov(R0, ShifterOperand(2));
  __ Bind(&l3);
  __ mov(R1, ShifterOperand(1));
  __ b(&l3, EQ);

  Label l4;
  __ b(&l4, EQ);
  __ mov(R1, ShifterOperand(2));
  __ Bind(&l4);
  __ mov(R0, ShifterOperand(3));

  // 2 linked labels.
  Label l5;
  __ b(&l5);
  __ mov(R1, ShifterOperand(4));
  __ b(&l5);
  __ mov(R1, ShifterOperand(5));
  __ Bind(&l5);
  __ mov(R0, ShifterOperand(6));

  size_t cs = __ CodeSize();
  std::vector<uint8_t> managed_code(cs);
  MemoryRegion code(&managed_code[0], managed_code.size());
  __ FinalizeInstructions(code);
  dump(managed_code);
  delete assembler;
}

TEST(Thumb2, LoadMultiple) {
  arm::Thumb2Assembler* assembler = static_cast<arm::Thumb2Assembler*>(Assembler::Create(kThumb2));

  // 16 bit.
  __ ldm(DB_W, R4, (1 << R0 | 1 << R3));

  // 32 bit.
  __ ldm(DB_W, R4, (1 << LR | 1 << R11));
  __ ldm(DB, R4, (1 << LR | 1 << R11));

  // Single reg is converted to ldr
  __ ldm(DB_W, R4, (1 << R5));

  size_t cs = __ CodeSize();
  std::vector<uint8_t> managed_code(cs);
  MemoryRegion code(&managed_code[0], managed_code.size());
  __ FinalizeInstructions(code);
  dump(managed_code);
  delete assembler;
}

TEST(Thumb2, StoreMultiple) {
  arm::Thumb2Assembler* assembler = static_cast<arm::Thumb2Assembler*>(Assembler::Create(kThumb2));

  // 16 bit.
  __ stm(IA_W, R4, (1 << R0 | 1 << R3));

  // 32 bit.
  __ stm(IA_W, R4, (1 << LR | 1 << R11));
  __ stm(IA, R4, (1 << LR | 1 << R11));

  // Single reg is converted to str
  __ stm(IA_W, R4, (1 << R5));
  __ stm(IA, R4, (1 << R5));

  size_t cs = __ CodeSize();
  std::vector<uint8_t> managed_code(cs);
  MemoryRegion code(&managed_code[0], managed_code.size());
  __ FinalizeInstructions(code);
  dump(managed_code);
  delete assembler;
}

TEST(Thumb2, MovWMovT) {
  arm::Thumb2Assembler* assembler = static_cast<arm::Thumb2Assembler*>(Assembler::Create(kThumb2));

  __ movw(R4, 0);         // 16 bit.
  __ movw(R4, 0x34);      // 16 bit.
  __ movw(R9, 0x34);      // 32 bit due to high register.
  __ movw(R3, 0x1234);    // 32 bit due to large value.
  __ movw(R9, 0xffff);    // 32 bit due to large value and high register.

  // Always 32 bit.
  __ movt(R0, 0);
  __ movt(R0, 0x1234);
  __ movt(R1, 0xffff);

  size_t cs = __ CodeSize();
  std::vector<uint8_t> managed_code(cs);
  MemoryRegion code(&managed_code[0], managed_code.size());
  __ FinalizeInstructions(code);
  dump(managed_code);
  delete assembler;
}

TEST(Thumb2, SpecialAddSub) {
  arm::Thumb2Assembler* assembler = static_cast<arm::Thumb2Assembler*>(Assembler::Create(kThumb2));

  __ add(R2, SP, ShifterOperand(0x50));   // 16 bit.
  __ add(SP, SP, ShifterOperand(0x50));   // 16 bit.
  __ add(R8, SP, ShifterOperand(0x50));   // 32 bit.

  __ add(R2, SP, ShifterOperand(0xf00));  // 32 bit due to imm size.
  __ add(SP, SP, ShifterOperand(0xf00));  // 32 bit due to imm size.

  __ sub(SP, SP, ShifterOperand(0x50));     // 16 bit
  __ sub(R0, SP, ShifterOperand(0x50));     // 32 bit
  __ sub(R8, SP, ShifterOperand(0x50));     // 32 bit.

  __ sub(SP, SP, ShifterOperand(0xf00));   // 32 bit due to imm size

  size_t cs = __ CodeSize();
  std::vector<uint8_t> managed_code(cs);
  MemoryRegion code(&managed_code[0], managed_code.size());
  __ FinalizeInstructions(code);
  dump(managed_code);
  delete assembler;
}

#undef __
}  // namespace arm
}  // namespace art
