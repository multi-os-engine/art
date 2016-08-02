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

#ifndef ART_COMPILER_UTILS_MACRO_ASSEMBLER_H_
#define ART_COMPILER_UTILS_MACRO_ASSEMBLER_H_

#include "assembler.h"
#include "arch/instruction_set.h"
#include "base/enums.h"
#include "managed_register.h"
#include "offsets.h"
#include "utils/array_ref.h"

namespace art {

class ArenaAllocator;
class InstructionSetFeatures;

template <PointerSize kPointerSize>
class MacroAssembler : public Assembler {
 public:
  static std::unique_ptr<MacroAssembler<kPointerSize>> Create(
      ArenaAllocator* arena,
      InstructionSet instruction_set,
      const InstructionSetFeatures* instruction_set_features = nullptr);

  // Emit code that will create an activation on the stack
  virtual void BuildFrame(size_t frame_size,
                          ManagedRegister method_reg,
                          ArrayRef<const ManagedRegister> callee_save_regs,
                          const ManagedRegisterEntrySpills& entry_spills) = 0;

  // Emit code that will remove an activation from the stack
  virtual void RemoveFrame(size_t frame_size,
                           ArrayRef<const ManagedRegister> callee_save_regs) = 0;

  virtual void IncreaseFrameSize(size_t adjust) = 0;
  virtual void DecreaseFrameSize(size_t adjust) = 0;

  // Store routines
  virtual void Store(FrameOffset offs, ManagedRegister src, size_t size) = 0;
  virtual void StoreRef(FrameOffset dest, ManagedRegister src) = 0;
  virtual void StoreRawPtr(FrameOffset dest, ManagedRegister src) = 0;

  virtual void StoreImmediateToFrame(FrameOffset dest, uint32_t imm,
                                     ManagedRegister scratch) = 0;

  virtual void StoreStackOffsetToThread(ThreadOffset<kPointerSize> thr_offs,
                                        FrameOffset fr_offs,
                                        ManagedRegister scratch) = 0;

  virtual void StoreStackPointerToThread(ThreadOffset<kPointerSize> thr_offs) = 0;

  virtual void StoreSpanning(FrameOffset dest,
                             ManagedRegister src,
                             FrameOffset in_off,
                             ManagedRegister scratch) = 0;

  // Load routines
  virtual void Load(ManagedRegister dest, FrameOffset src, size_t size) = 0;

  virtual void LoadFromThread(ManagedRegister dest,
                              ThreadOffset<kPointerSize> src,
                              size_t size) = 0;

  virtual void LoadRef(ManagedRegister dest, FrameOffset src) = 0;
  // If unpoison_reference is true and kPoisonReference is true, then we negate the read reference.
  virtual void LoadRef(ManagedRegister dest,
                       ManagedRegister base,
                       MemberOffset offs,
                       bool unpoison_reference) = 0;

  virtual void LoadRawPtr(ManagedRegister dest, ManagedRegister base, Offset offs) = 0;

  virtual void LoadRawPtrFromThread(ManagedRegister dest, ThreadOffset<kPointerSize> offs) = 0;

  // Copying routines
  virtual void Move(ManagedRegister dest, ManagedRegister src, size_t size) = 0;

  virtual void CopyRawPtrFromThread(FrameOffset fr_offs,
                                    ThreadOffset<kPointerSize> thr_offs,
                                    ManagedRegister scratch) = 0;

  virtual void CopyRawPtrToThread(ThreadOffset<kPointerSize> thr_offs,
                                  FrameOffset fr_offs,
                                  ManagedRegister scratch) = 0;

  virtual void CopyRef(FrameOffset dest, FrameOffset src, ManagedRegister scratch) = 0;

  virtual void Copy(FrameOffset dest, FrameOffset src, ManagedRegister scratch, size_t size) = 0;

  virtual void Copy(FrameOffset dest,
                    ManagedRegister src_base,
                    Offset src_offset,
                    ManagedRegister scratch,
                    size_t size) = 0;

  virtual void Copy(ManagedRegister dest_base,
                    Offset dest_offset,
                    FrameOffset src,
                    ManagedRegister scratch,
                    size_t size) = 0;

  virtual void Copy(FrameOffset dest,
                    FrameOffset src_base,
                    Offset src_offset,
                    ManagedRegister scratch,
                    size_t size) = 0;

  virtual void Copy(ManagedRegister dest,
                    Offset dest_offset,
                    ManagedRegister src,
                    Offset src_offset,
                    ManagedRegister scratch,
                    size_t size) = 0;

  virtual void Copy(FrameOffset dest,
                    Offset dest_offset,
                    FrameOffset src,
                    Offset src_offset,
                    ManagedRegister scratch,
                    size_t size) = 0;

  virtual void MemoryBarrier(ManagedRegister scratch) = 0;

  // Sign extension
  virtual void SignExtend(ManagedRegister mreg, size_t size) = 0;

  // Zero extension
  virtual void ZeroExtend(ManagedRegister mreg, size_t size) = 0;

  // Exploit fast access in managed code to Thread::Current()
  virtual void GetCurrentThread(ManagedRegister tr) = 0;
  virtual void GetCurrentThread(FrameOffset dest_offset, ManagedRegister scratch) = 0;

  // Set up out_reg to hold a Object** into the handle scope, or to be null if the
  // value is null and null_allowed. in_reg holds a possibly stale reference
  // that can be used to avoid loading the handle scope entry to see if the value is
  // null.
  virtual void CreateHandleScopeEntry(ManagedRegister out_reg,
                                      FrameOffset handlescope_offset,
                                      ManagedRegister in_reg,
                                      bool null_allowed) = 0;

  // Set up out_off to hold a Object** into the handle scope, or to be null if the
  // value is null and null_allowed.
  virtual void CreateHandleScopeEntry(FrameOffset out_off,
                                      FrameOffset handlescope_offset,
                                      ManagedRegister scratch,
                                      bool null_allowed) = 0;

  // src holds a handle scope entry (Object**) load this into dst
  virtual void LoadReferenceFromHandleScope(ManagedRegister dst, ManagedRegister src) = 0;

  // Heap::VerifyObject on src. In some cases (such as a reference to this) we
  // know that src may not be null.
  virtual void VerifyObject(ManagedRegister src, bool could_be_null) = 0;
  virtual void VerifyObject(FrameOffset src, bool could_be_null) = 0;

  // Call to address held at [base+offset]
  virtual void Call(ManagedRegister base, Offset offset, ManagedRegister scratch) = 0;
  virtual void Call(FrameOffset base, Offset offset, ManagedRegister scratch) = 0;
  virtual void CallFromThread(ThreadOffset<kPointerSize> offset, ManagedRegister scratch) = 0;

  // Generate code to check if Thread::Current()->exception_ is non-null
  // and branch to a ExceptionSlowPath if it is.
  virtual void ExceptionPoll(ManagedRegister scratch, size_t stack_adjust) = 0;

  virtual ~MacroAssembler() {}

 protected:
  explicit MacroAssembler(ArenaAllocator* arena) : Assembler(arena) {
  }
};

}  // namespace art

#endif  // ART_COMPILER_UTILS_MACRO_ASSEMBLER_H_
