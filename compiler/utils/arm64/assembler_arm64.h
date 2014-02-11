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

#ifndef ART_COMPILER_UTILS_ARM64_ASSEMBLER_ARM64_H_
#define ART_COMPILER_UTILS_ARM64_ASSEMBLER_ARM64_H_

#include <vector>

#include "base/logging.h"
#include "constants_arm64.h"
#include "utils/arm64/managed_register_arm64.h"
#include "utils/assembler.h"
#include "offsets.h"
#include "utils.h"
#include "UniquePtr.h"
#include "a64/macro-assembler-a64.h"
#include "a64/disasm-a64.h"

namespace art {
namespace arm64 {

#define MEM_OP(x...)      vixl::MemOperand(x)
#define COND_OP(x)        static_cast<vixl::Condition>(x)

enum Condition {
  kNoCondition = -1,
  EQ = 0,
  NE = 1,
  HS = 2,
  LO = 3,
  MI = 4,
  PL = 5,
  VS = 6,
  VC = 7,
  HI = 8,
  LS = 9,
  GE = 10,
  LT = 11,
  GT = 12,
  LE = 13,
  AL = 14,    // always
  NV = 15,    // behaves as always/al.
  kMaxCondition = 16,
};

enum LoadOperandType {
  kLoadSignedByte,
  kLoadUnsignedByte,
  kLoadSignedHalfword,
  kLoadUnsignedHalfword,
  kLoadWord,
  kLoadCoreWord,
  kLoadSWord,
  kLoadDWord
};

enum StoreOperandType {
  kStoreByte,
  kStoreHalfword,
  kStoreWord,
  kStoreCoreWord,
  kStoreSWord,
  kStoreDWord
};

class Arm64Exception;

class Arm64Assembler : public Assembler {
 public:
  Arm64Assembler() : vixl_buf_(new byte[BUF_SIZE]),
  vixl_masm_(new vixl::MacroAssembler(vixl_buf_, BUF_SIZE)) {}

  // TODO: disassembly only for debug build
  virtual ~Arm64Assembler() {
#ifndef NDEBUG
    vixl::Decoder *decoder = new vixl::Decoder();
    vixl::PrintDisassembler *test = new vixl::PrintDisassembler(stdout);
    decoder->AppendVisitor(test);

    for (size_t i = 0; i < CodeSize() / vixl::kInstructionSize; ++i) {
      vixl::Instruction *instr =
        reinterpret_cast<vixl::Instruction*>(vixl_buf_ + i * vixl::kInstructionSize);
      decoder->Decode(instr);
    }
#endif
    delete[] vixl_buf_;
  }

  // Emit slow paths queued during assembly
  virtual void EmitSlowPaths();

  // Size of generated code
  virtual size_t CodeSize() const;

  // Copy instructions out of assembly buffer into the given region of memory
  virtual void FinalizeInstructions(const MemoryRegion& region);

  // Emit code that will create an activation on the stack
  virtual void BuildFrame(size_t frame_size, ManagedRegister method_reg,
                          const std::vector<ManagedRegister>& callee_save_regs,
                          const std::vector<ManagedRegister>& entry_spills);

  // Emit code that will remove an activation from the stack
  virtual void RemoveFrame(size_t frame_size,
                           const std::vector<ManagedRegister>& callee_save_regs);

  virtual void IncreaseFrameSize(size_t adjust);
  virtual void DecreaseFrameSize(size_t adjust);

  // Store routines
  virtual void Store(FrameOffset offs, ManagedRegister src, size_t size);
  virtual void StoreRef(FrameOffset dest, ManagedRegister src);
  virtual void StoreRawPtr(FrameOffset dest, ManagedRegister src);
  virtual void StoreImmediateToFrame(FrameOffset dest, uint32_t imm,
                                     ManagedRegister scratch);
  virtual void StoreImmediateToThread(ThreadOffset dest, uint32_t imm,
                                      ManagedRegister scratch);
  virtual void StoreStackOffsetToThread(ThreadOffset thr_offs,
                                        FrameOffset fr_offs,
                                        ManagedRegister scratch);
  virtual void StoreStackPointerToThread(ThreadOffset thr_offs);
  virtual void StoreSpanning(FrameOffset dest, ManagedRegister src,
                             FrameOffset in_off, ManagedRegister scratch);

  // Load routines
  virtual void Load(ManagedRegister dest, FrameOffset src, size_t size);
  virtual void Load(ManagedRegister dest, ThreadOffset src, size_t size);
  virtual void LoadRef(ManagedRegister dest, FrameOffset  src);
  virtual void LoadRef(ManagedRegister dest, ManagedRegister base,
                       MemberOffset offs);
  virtual void LoadRawPtr(ManagedRegister dest, ManagedRegister base,
                          Offset offs);
  virtual void LoadRawPtrFromThread(ManagedRegister dest,
                                    ThreadOffset offs);
  // Copying routines
  virtual void Move(ManagedRegister dest, ManagedRegister src, size_t size);
  virtual void CopyRawPtrFromThread(FrameOffset fr_offs, ThreadOffset thr_offs,
                                    ManagedRegister scratch);
  virtual void CopyRawPtrToThread(ThreadOffset thr_offs, FrameOffset fr_offs,
                                  ManagedRegister scratch);
  virtual void CopyRef(FrameOffset dest, FrameOffset src,
                       ManagedRegister scratch);
  virtual void Copy(FrameOffset dest, FrameOffset src, ManagedRegister scratch, size_t size);
  virtual void Copy(FrameOffset dest, ManagedRegister src_base, Offset src_offset,
                    ManagedRegister scratch, size_t size);
  virtual void Copy(ManagedRegister dest_base, Offset dest_offset, FrameOffset src,
                    ManagedRegister scratch, size_t size);
  virtual void Copy(FrameOffset dest, FrameOffset src_base, Offset src_offset,
                    ManagedRegister scratch, size_t size);
  virtual void Copy(ManagedRegister dest, Offset dest_offset,
                    ManagedRegister src, Offset src_offset,
                    ManagedRegister scratch, size_t size);
  virtual void Copy(FrameOffset dest, Offset dest_offset, FrameOffset src, Offset src_offset,
                    ManagedRegister scratch, size_t size);
  virtual void MemoryBarrier(ManagedRegister scratch);

  // Sign extension
  virtual void SignExtend(ManagedRegister mreg, size_t size);

  // Zero extension
  virtual void ZeroExtend(ManagedRegister mreg, size_t size);

  // Exploit fast access in managed code to Thread::Current()
  virtual void GetCurrentThread(ManagedRegister tr);
  virtual void GetCurrentThread(FrameOffset dest_offset,
                                ManagedRegister scratch);

  // Set up out_reg to hold a Object** into the SIRT, or to be NULL if the
  // value is null and null_allowed. in_reg holds a possibly stale reference
  // that can be used to avoid loading the SIRT entry to see if the value is
  // NULL.
  virtual void CreateSirtEntry(ManagedRegister out_reg, FrameOffset sirt_offset,
                               ManagedRegister in_reg, bool null_allowed);

  // Set up out_off to hold a Object** into the SIRT, or to be NULL if the
  // value is null and null_allowed.
  virtual void CreateSirtEntry(FrameOffset out_off, FrameOffset sirt_offset,
                               ManagedRegister scratch, bool null_allowed);

  // src holds a SIRT entry (Object**) load this into dst
  virtual void LoadReferenceFromSirt(ManagedRegister dst,
                                     ManagedRegister src);

  // Heap::VerifyObject on src. In some cases (such as a reference to this) we
  // know that src may not be null.
  virtual void VerifyObject(ManagedRegister src, bool could_be_null);
  virtual void VerifyObject(FrameOffset src, bool could_be_null);

  // Call to address held at [base+offset]
  virtual void Call(ManagedRegister base, Offset offset,
                    ManagedRegister scratch);
  virtual void Call(FrameOffset base, Offset offset,
                    ManagedRegister scratch);
  virtual void Call(ThreadOffset offset, ManagedRegister scratch);

  // Generate code to check if Thread::Current()->exception_ is non-null
  // and branch to a ExceptionSlowPath if it is.
  virtual void ExceptionPoll(ManagedRegister scratch, size_t stack_adjust);

 private:
  static vixl::Register reg_x(int code) {
    CHECK(code < kNumberOfCoreRegisters) << code;
    if (code == SP) {
      return vixl::sp;
    }
    return vixl::Register::XRegFromCode(code);
  }

  static vixl::Register reg_w(int code) {
    return vixl::Register::WRegFromCode(code);
  }

  static vixl::FPRegister reg_d(int code) {
    return vixl::FPRegister::DRegFromCode(code);
  }

  static vixl::FPRegister reg_s(int code) {
    return vixl::FPRegister::SRegFromCode(code);
  }

  // Emits Exception block
  void EmitExceptionPoll(Arm64Exception *exception);

  void StoreWToOffset(StoreOperandType type, WRegister source,
                      Register base, int32_t offset);
  void StoreToOffset(Register source, Register base, int32_t offset);
  void StoreSToOffset(SRegister source, Register base, int32_t offset);
  void StoreDToOffset(DRegister source, Register base, int32_t offset);

  void LoadImmediate(Register dest, int32_t value, Condition cond = AL);
  void Load(Arm64ManagedRegister dst, Register src, int32_t src_offset, size_t size);
  void LoadWFromOffset(LoadOperandType type, WRegister dest,
                      Register base, int32_t offset);
  void LoadFromOffset(Register dest, Register base, int32_t offset);
  void LoadSFromOffset(SRegister dest, Register base, int32_t offset);
  void LoadDFromOffset(DRegister dest, Register base, int32_t offset);
  void AddConstant(Register rd, int32_t value, Condition cond = AL);
  void AddConstant(Register rd, Register rn, int32_t value, Condition cond = AL);

  // vixl buffer size
  static constexpr size_t BUF_SIZE = 4096;

  // vixl buffer
  byte* vixl_buf_;

  // unique ptr - vixl assembler
  UniquePtr<vixl::MacroAssembler> vixl_masm_;

  // list of exception blocks to generate at the end of the code cache
  std::vector<Arm64Exception*> exception_blocks_;
};

class Arm64Exception {
 private:
  explicit Arm64Exception(Arm64ManagedRegister scratch, size_t stack_adjust)
      : scratch_(scratch), stack_adjust_(stack_adjust) {
    }

  vixl::Label* Entry() { return &exception_entry_; }

  // register used for passing Thread::Current()->exception_
  const Arm64ManagedRegister scratch_;

  // stack adjust for ExceptionPool
  const size_t stack_adjust_;

  vixl::Label exception_entry_;

  friend class Arm64Assembler;
  DISALLOW_COPY_AND_ASSIGN(Arm64Exception);
};

}  // namespace arm64
}  // namespace art

#endif  // ART_COMPILER_UTILS_ARM64_ASSEMBLER_ARM64_H_
