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

/* This file contains codegen for the Mips64 ISA */

#include "codegen_mips64.h"

#include "base/logging.h"
#include "dex/mir_graph.h"
#include "dex/quick/mir_to_lir-inl.h"
#include "entrypoints/quick/quick_entrypoints.h"
#include "gc/accounting/card_table.h"
#include "mips64_lir.h"
#include "mirror/art_method.h"
#include "mirror/object_array-inl.h"

namespace art {

bool Mips64Mir2Lir::GenSpecialCase(BasicBlock* bb, MIR* mir, const InlineMethod& special) {
  // TODO
  UNUSED(bb, mir, special);
  return false;
}

/*
 * The lack of pc-relative loads on Mips64 presents somewhat of a challenge
 * for our PIC switch table strategy.  To materialize the current location
 * we'll do a dummy JAL and reference our tables using rRA as the
 * base register.  Note that rRA will be used both as the base to
 * locate the switch table data and as the reference base for the switch
 * target offsets stored in the table.  We'll use a special pseudo-instruction
 * to represent the jal and trigger the construction of the
 * switch table offsets (which will happen after final assembly and all
 * labels are fixed).
 *
 * The test loop will look something like:
 *
 *   ori   r_end, rZERO, #table_size  ; size in bytes
 *   jal   BaseLabel         ; stores "return address" (BaseLabel) in rRA
 *   nop                     ; opportunistically fill
 * BaseLabel:
 *   addiu r_base, rRA, <table> - <BaseLabel>    ; table relative to BaseLabel
     addu  r_end, r_end, r_base                   ; end of table
 *   lw    r_val, [rSP, v_reg_off]                ; Test Value
 * loop:
 *   beq   r_base, r_end, done
 *   lw    r_key, 0(r_base)
 *   addu  r_base, 8
 *   bne   r_val, r_key, loop
 *   lw    r_disp, -4(r_base)
 *   addu  rRA, r_disp
 *   jalr  rZERO, rRA
 * done:
 *
 */
void Mips64Mir2Lir::GenLargeSparseSwitch(MIR* mir, DexOffset table_offset, RegLocation rl_src) {
  const uint16_t* table = mir_graph_->GetTable(mir, table_offset);
  // Add the table to the list - we'll process it later.
  SwitchTable* tab_rec = static_cast<SwitchTable*>(arena_->Alloc(sizeof(SwitchTable),
                                                   kArenaAllocData));
  tab_rec->switch_mir = mir;
  tab_rec->table = table;
  tab_rec->vaddr = current_dalvik_offset_;
  int elements = table[1];
  switch_tables_.push_back(tab_rec);

  // The table is composed of 8-byte key/disp pairs.
  int byte_size = elements * 8;

  int size_hi = byte_size >> 16;
  int size_lo = byte_size & 0xffff;

  RegStorage r_end = AllocTempWide();
  if (size_hi) {
    NewLIR2(kMips64Lui, r_end.GetReg(), size_hi);
  }
  // Must prevent code motion for the curr pc pair.
  GenBarrier();  // Scheduling barrier.
  NewLIR0(kMips64CurrPC);  // Really a jal to .+8.
  // Now, fill the branch delay slot.
  if (size_hi) {
    NewLIR3(kMips64Ori, r_end.GetReg(), r_end.GetReg(), size_lo);
  } else {
    NewLIR3(kMips64Ori, r_end.GetReg(), rZERO, size_lo);
  }
  GenBarrier();  // Scheduling barrier.

  // Construct BaseLabel and set up table base register.
  LIR* base_label = NewLIR0(kPseudoTargetLabel);
  // Remember base label so offsets can be computed later.
  tab_rec->anchor = base_label;
  RegStorage r_base = AllocTempWide();
  NewLIR4(kMips64Delta, r_base.GetReg(), 0, WrapPointer(base_label), WrapPointer(tab_rec));
  OpRegRegReg(kOpAdd, r_end, r_end, r_base);

  // Grab switch test value.
  rl_src = LoadValue(rl_src, kCoreReg);

  // Test loop.
  RegStorage r_key = AllocTemp();
  LIR* loop_label = NewLIR0(kPseudoTargetLabel);
  LIR* exit_branch = OpCmpBranch(kCondEq, r_base, r_end, NULL);
  Load32Disp(r_base, 0, r_key);
  OpRegImm(kOpAdd, r_base, 8);
  OpCmpBranch(kCondNe, rl_src.reg, r_key, loop_label);
  RegStorage r_disp = AllocTemp();
  Load32Disp(r_base, -4, r_disp);
  OpRegRegReg(kOpAdd, TargetReg(kLr, kWide), TargetReg(kLr, kWide), r_disp);
  OpReg(kOpBx, TargetReg(kLr, kWide));

  // Loop exit.
  LIR* exit_label = NewLIR0(kPseudoTargetLabel);
  exit_branch->target = exit_label;
}

/*
 * Code pattern will look something like:
 *
 *   lw    r_val
 *   jal   BaseLabel         ; stores "return address" (BaseLabel) in rRA
 *   nop                     ; opportunistically fill
 *   [subiu r_val, bias]      ; Remove bias if low_val != 0
 *   bound check -> done
 *   lw    r_disp, [rRA, r_val]
 *   addu  rRA, r_disp
 *   jalr  rZERO, rRA
 * done:
 */
void Mips64Mir2Lir::GenLargePackedSwitch(MIR* mir, DexOffset table_offset, RegLocation rl_src) {
  const uint16_t* table = mir_graph_->GetTable(mir, table_offset);
  // Add the table to the list - we'll process it later.
  SwitchTable* tab_rec =
      static_cast<SwitchTable*>(arena_->Alloc(sizeof(SwitchTable), kArenaAllocData));
  tab_rec->switch_mir = mir;
  tab_rec->table = table;
  tab_rec->vaddr = current_dalvik_offset_;
  int size = table[1];
  switch_tables_.push_back(tab_rec);

  // Get the switch value.
  rl_src = LoadValue(rl_src, kCoreReg);

  // Prepare the bias.  If too big, handle 1st stage here.
  int low_key = s4FromSwitchData(&table[2]);
  bool large_bias = false;
  RegStorage r_key;
  if (low_key == 0) {
    r_key = rl_src.reg;
  } else if ((low_key & 0xffff) != low_key) {
    r_key = AllocTemp();
    LoadConstant(r_key, low_key);
    large_bias = true;
  } else {
    r_key = AllocTemp();
  }

  // Must prevent code motion for the curr pc pair.
  GenBarrier();
  NewLIR0(kMips64CurrPC);  // Really a jal to .+8.
  // Now, fill the branch delay slot with bias strip.
  if (low_key == 0) {
    NewLIR0(kMips64Nop);
  } else {
    if (large_bias) {
      OpRegRegReg(kOpSub, r_key, rl_src.reg, r_key);
    } else {
      OpRegRegImm(kOpSub, r_key, rl_src.reg, low_key);
    }
  }
  GenBarrier();  // Scheduling barrier.

  // Construct BaseLabel and set up table base register.
  LIR* base_label = NewLIR0(kPseudoTargetLabel);
  // Remember base label so offsets can be computed later.
  tab_rec->anchor = base_label;

  // Bounds check - if < 0 or >= size continue following switch.
  LIR* branch_over = OpCmpImmBranch(kCondHi, r_key, size-1, NULL);

  // Materialize the table base pointer.
  RegStorage r_base = AllocTempWide();
  NewLIR4(kMips64Delta, r_base.GetReg(), 0, WrapPointer(base_label), WrapPointer(tab_rec));

  // Load the displacement from the switch table.
  RegStorage r_disp = AllocTemp();
  LoadBaseIndexed(r_base, r_key, r_disp, 2, k32);

  // Add to rAP and go.
  OpRegRegReg(kOpAdd, TargetReg(kLr, kWide), TargetReg(kLr, kWide), r_disp);
  OpReg(kOpBx, TargetReg(kLr, kWide));

  // Branch_over target here.
  LIR* target = NewLIR0(kPseudoTargetLabel);
  branch_over->target = target;
}

/*
 * Array data table format:
 *  ushort ident = 0x0300   magic value
 *  ushort width            width of each element in the table
 *  uint   size             number of elements in the table
 *  ubyte  data[size*width] table of data values (may contain a single-byte
 *                          padding at the end)
 *
 * Total size is 4+(width * size + 1)/2 16-bit code units.
 */
void Mips64Mir2Lir::GenFillArrayData(MIR* mir, DexOffset table_offset, RegLocation rl_src) {
  const uint16_t* table = mir_graph_->GetTable(mir, table_offset);
  // Add the table to the list - we'll process it later.
  FillArrayData* tab_rec = reinterpret_cast<FillArrayData*>(arena_->Alloc(sizeof(FillArrayData),
                                                            kArenaAllocData));
  tab_rec->table = table;
  tab_rec->vaddr = current_dalvik_offset_;
  uint16_t width = tab_rec->table[1];
  uint32_t size = tab_rec->table[2] | ((static_cast<uint32_t>(tab_rec->table[3])) << 16);
  tab_rec->size = (size * width) + 8;

  fill_array_data_.push_back(tab_rec);

  // Making a call - use explicit registers.
  FlushAllRegs();  // Everything to home location.
  LockCallTemps();
  LoadValueDirectFixed(rl_src, rs_rMIPS64_ARG0);

  // Must prevent code motion for the curr pc pair.
  GenBarrier();
  NewLIR0(kMips64CurrPC);  // Really a jal to .+8.
  // Now, fill the branch delay slot with the helper load.
  RegStorage r_tgt = LoadHelper(kQuickHandleFillArrayData);
  GenBarrier();  // Scheduling barrier.

  // Construct BaseLabel and set up table base register.
  LIR* base_label = NewLIR0(kPseudoTargetLabel);

  // Materialize a pointer to the fill data image.
  NewLIR4(kMips64Delta, rMIPS64_ARG1, 0, WrapPointer(base_label), WrapPointer(tab_rec));

  // And go...
  ClobberCallerSave();
  LIR* call_inst = OpReg(kOpBlx, r_tgt);  // ( array*, fill_data* )
  MarkSafepointPC(call_inst);
}

void Mips64Mir2Lir::GenMoveException(RegLocation rl_dest) {
  int ex_offset = Thread::ExceptionOffset<8>().Int32Value();
  RegLocation rl_result = EvalLoc(rl_dest, kRefReg, true);
  RegStorage reset_reg = AllocTempRef();
  LoadRefDisp(rs_rMIPS64_SELF, ex_offset, rl_result.reg, kNotVolatile);
  LoadConstant(reset_reg, 0);
  StoreRefDisp(rs_rMIPS64_SELF, ex_offset, reset_reg, kNotVolatile);
  FreeTemp(reset_reg);
  StoreValue(rl_dest, rl_result);
}

void Mips64Mir2Lir::UnconditionallyMarkGCCard(RegStorage tgt_addr_reg) {
  RegStorage reg_card_base = AllocTempWide();
  RegStorage reg_card_no = AllocTempWide();
  // NOTE: native pointer.
  LoadWordDisp(rs_rMIPS64_SELF, Thread::CardTableOffset<8>().Int32Value(), reg_card_base);
  OpRegRegImm(kOpLsr, reg_card_no, tgt_addr_reg, gc::accounting::CardTable::kCardShift);
  StoreBaseIndexed(reg_card_base, reg_card_no, As32BitReg(reg_card_base), 0, kUnsignedByte);
  FreeTemp(reg_card_base);
  FreeTemp(reg_card_no);
}

void Mips64Mir2Lir::GenEntrySequence(RegLocation* ArgLocs, RegLocation rl_method) {
  int spill_count = num_core_spills_ + num_fp_spills_;
  /*
   * On entry, rMIPS64_ARG0, rMIPS64_ARG1, rMIPS64_ARG2, rMIPS64_ARG3,
   * rMIPS64_ARG4, rMIPS64_ARG5, rMIPS64_ARG6 & rMIPS64_ARG7  are live.
   * Let the register allocation mechanism know so it doesn't try to
   * use any of them when expanding the frame or flushing.
   */
  LockTemp(rs_rMIPS64_ARG0);
  LockTemp(rs_rMIPS64_ARG1);
  LockTemp(rs_rMIPS64_ARG2);
  LockTemp(rs_rMIPS64_ARG3);
  LockTemp(rs_rMIPS64_ARG4);
  LockTemp(rs_rMIPS64_ARG5);
  LockTemp(rs_rMIPS64_ARG6);
  LockTemp(rs_rMIPS64_ARG7);

  /*
   * We can safely skip the stack overflow check if we're
   * a leaf *and* our frame size < fudge factor.
   */
  bool skip_overflow_check = mir_graph_->MethodIsLeaf() && !FrameNeedsStackCheck(frame_size_,
                                                                                 kMips64);
  NewLIR0(kPseudoMethodEntry);
  RegStorage check_reg = AllocTempWide();
  RegStorage new_sp = AllocTempWide();
  if (!skip_overflow_check) {
    // Load stack limit.
    LoadWordDisp(rs_rMIPS64_SELF, Thread::StackEndOffset<8>().Int32Value(), check_reg);
  }
  // Spill core callee saves.
  SpillCoreRegs();
  // NOTE: promotion of FP regs currently unsupported, thus no FP spill.
  DCHECK_EQ(num_fp_spills_, 0);
  const int frame_sub = frame_size_ - spill_count * 8;
  if (!skip_overflow_check) {
    class StackOverflowSlowPath : public LIRSlowPath {
     public:
      StackOverflowSlowPath(Mir2Lir* m2l, LIR* branch, size_t sp_displace)
          : LIRSlowPath(m2l, m2l->GetCurrentDexPc(), branch, nullptr), sp_displace_(sp_displace) {
      }
      void Compile() OVERRIDE {
        m2l_->ResetRegPool();
        m2l_->ResetDefTracking();
        GenerateTargetLabel(kPseudoThrowTarget);
        // Load RA from the top of the frame.
        m2l_->LoadWordDisp(rs_rMIPS64_SP, sp_displace_ - 8, rs_rRAd);
        m2l_->OpRegImm(kOpAdd, rs_rMIPS64_SP, sp_displace_);
        m2l_->ClobberCallerSave();
        RegStorage r_tgt = m2l_->CallHelperSetup(kQuickThrowStackOverflow);  // Doesn't clobber LR.
        m2l_->CallHelper(r_tgt, kQuickThrowStackOverflow, false /* MarkSafepointPC */,
                         false /* UseLink */);
      }

     private:
      const size_t sp_displace_;
    };
    OpRegRegImm(kOpSub, new_sp, rs_rMIPS64_SP, frame_sub);
    LIR* branch = OpCmpBranch(kCondUlt, new_sp, check_reg, nullptr);
    AddSlowPath(new(arena_)StackOverflowSlowPath(this, branch, spill_count * 8));
    // TODO: avoid copy for small frame sizes.
    OpRegCopy(rs_rMIPS64_SP, new_sp);  // Establish stack.
  } else {
    OpRegImm(kOpSub, rs_rMIPS64_SP, frame_sub);
  }

  FlushIns(ArgLocs, rl_method);

  FreeTemp(rs_rMIPS64_ARG0);
  FreeTemp(rs_rMIPS64_ARG1);
  FreeTemp(rs_rMIPS64_ARG2);
  FreeTemp(rs_rMIPS64_ARG3);
  FreeTemp(rs_rMIPS64_ARG4);
  FreeTemp(rs_rMIPS64_ARG5);
  FreeTemp(rs_rMIPS64_ARG6);
  FreeTemp(rs_rMIPS64_ARG7);
}

void Mips64Mir2Lir::GenExitSequence() {
  /*
   * In the exit path, rMIPS64_RET0/rMIPS64_RET1 are live - make sure they aren't
   * allocated by the register utilities as temps.
   */
  LockTemp(rs_rMIPS64_RET0);
  LockTemp(rs_rMIPS64_RET1);

  NewLIR0(kPseudoMethodExit);
  UnSpillCoreRegs();
  OpReg(kOpBx, rs_rRAd);
}

void Mips64Mir2Lir::GenSpecialExitSequence() {
  OpReg(kOpBx, rs_rRAd);
}

void Mips64Mir2Lir::GenSpecialEntryForSuspend() {
  // Keep 16-byte stack alignment - push A0, i.e. ArtMethod* and RA.
  core_spill_mask_ = (1u << rs_rRAd.GetRegNum());
  num_core_spills_ = 1u;
  fp_spill_mask_ = 0u;
  num_fp_spills_ = 0u;
  frame_size_ = 16u;
  core_vmap_table_.clear();
  fp_vmap_table_.clear();
  OpRegImm(kOpSub, rs_rMIPS64_SP, frame_size_);
  StoreWordDisp(rs_rMIPS64_SP, frame_size_ - 8, rs_rRAd);
  StoreWordDisp(rs_rMIPS64_SP, 0, rs_rA0d);
}

void Mips64Mir2Lir::GenSpecialExitForSuspend() {
  // Pop the frame. Don't pop ArtMethod*, it's no longer needed.
  LoadWordDisp(rs_rMIPS64_SP, frame_size_ - 8, rs_rRAd);
  OpRegImm(kOpAdd, rs_rMIPS64_SP, frame_size_);
}

/*
 * Bit of a hack here - in the absence of a real scheduling pass,
 * emit the next instruction in static & direct invoke sequences.
 */
static int Mips64NextSDCallInsn(CompilationUnit* cu, CallInfo* info ATTRIBUTE_UNUSED, int state,
                                const MethodReference& target_method, uint32_t,
                                uintptr_t direct_code, uintptr_t direct_method, InvokeType type) {
  Mir2Lir* cg = static_cast<Mir2Lir*>(cu->cg.get());
  if (direct_code != 0 && direct_method != 0) {
    switch (state) {
    case 0:  // Get the current Method* [sets kArg0]
      if (direct_code != static_cast<uintptr_t>(-1)) {
        cg->LoadConstant(cg->TargetPtrReg(kInvokeTgt), direct_code);
      } else {
        cg->LoadCodeAddress(target_method, type, kInvokeTgt);
      }
      if (direct_method != static_cast<uintptr_t>(-1)) {
        cg->LoadConstant(cg->TargetReg(kArg0, kRef), direct_method);
      } else {
        cg->LoadMethodAddress(target_method, type, kArg0);
      }
      break;
    default:
      return -1;
    }
  } else {
    RegStorage arg0_ref = cg->TargetReg(kArg0, kRef);
    switch (state) {
    case 0:  // Get the current Method* [sets kArg0]
      // TUNING: we can save a reg copy if Method* has been promoted.
      cg->LoadCurrMethodDirect(arg0_ref);
      break;
    case 1:  // Get method->dex_cache_resolved_methods_
      cg->LoadRefDisp(arg0_ref, mirror::ArtMethod::DexCacheResolvedMethodsOffset().Int32Value(),
                      arg0_ref,       kNotVolatile);
      // Set up direct code if known.
      if (direct_code != 0) {
        if (direct_code != static_cast<uintptr_t>(-1)) {
          cg->LoadConstant(cg->TargetPtrReg(kInvokeTgt), direct_code);
        } else {
          CHECK_LT(target_method.dex_method_index, target_method.dex_file->NumMethodIds());
          cg->LoadCodeAddress(target_method, type, kInvokeTgt);
        }
      }
      break;
    case 2:  // Grab target method*
      CHECK_EQ(cu->dex_file, target_method.dex_file);
      cg->LoadRefDisp(arg0_ref, mirror::ObjectArray<mirror::Object>::
                          OffsetOfElement(target_method.dex_method_index).Int32Value(), arg0_ref,
                      kNotVolatile);
      break;
    case 3:  // Grab the code from the method*
      if (direct_code == 0) {
        int32_t offset = mirror::ArtMethod::EntryPointFromQuickCompiledCodeOffset(
            InstructionSetPointerSize(cu->instruction_set)).Int32Value();
        // Get the compiled code address [use *alt_from or kArg0, set kInvokeTgt]
        cg->LoadWordDisp(arg0_ref, offset, cg->TargetPtrReg(kInvokeTgt));
      }
      break;
    default:
      return -1;
    }
  }
  return state + 1;
}

NextCallInsn Mips64Mir2Lir::GetNextSDCallInsn() {
  return Mips64NextSDCallInsn;
}

LIR* Mips64Mir2Lir::GenCallInsn(const MirMethodLoweringInfo& method_info ATTRIBUTE_UNUSED) {
  return OpReg(kOpBlx, TargetPtrReg(kInvokeTgt));
}

}  // namespace art
