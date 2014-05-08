/*
 * Copyright (C) 2012 The Android Open Source Project
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

/* This file contains codegen for the X86 ISA */

#include "codegen_x86.h"
#include "dex/quick/mir_to_lir-inl.h"
#include "x86_lir.h"

namespace art {

// Macro to templatize functions and instantiate them.
#define X86MIR2LIR(ret, sig) \
  template ret X86Mir2Lir<4>::sig; \
  template ret X86Mir2Lir<8>::sig; \
  template <size_t pointer_size> ret X86Mir2Lir<pointer_size>::sig

/*
 * The sparse table in the literal pool is an array of <key,displacement>
 * pairs.
 */
X86MIR2LIR(void, GenSparseSwitch(MIR* mir, DexOffset table_offset,
                                RegLocation rl_src)) {
  const uint16_t* table = this->cu_->insns + this->current_dalvik_offset_ + table_offset;
  if (this->cu_->verbose) {
    this->DumpSparseSwitchTable(table);
  }
  int entries = table[1];
  const int32_t* keys = reinterpret_cast<const int32_t*>(&table[2]);
  const int32_t* targets = &keys[entries];
  rl_src = this->LoadValue(rl_src, kCoreReg);
  for (int i = 0; i < entries; i++) {
    int key = keys[i];
    BasicBlock* case_block =
        this->mir_graph_->FindBlock(this->current_dalvik_offset_ + targets[i]);
    OpCmpImmBranch(kCondEq, rl_src.reg, key, &this->block_label_list_[case_block->id]);
  }
}

/*
 * Code pattern will look something like:
 *
 * mov  r_val, ..
 * call 0
 * pop  r_start_of_method
 * sub  r_start_of_method, ..
 * mov  r_key_reg, r_val
 * sub  r_key_reg, low_key
 * cmp  r_key_reg, size-1  ; bound check
 * ja   done
 * mov  r_disp, [r_start_of_method + r_key_reg * 4 + table_offset]
 * add  r_start_of_method, r_disp
 * jmp  r_start_of_method
 * done:
 */
X86MIR2LIR(void, GenPackedSwitch(MIR* mir, DexOffset table_offset,
                                RegLocation rl_src)) {
  const uint16_t* table = this->cu_->insns + this->current_dalvik_offset_ + table_offset;
  if (this->cu_->verbose) {
    this->DumpPackedSwitchTable(table);
  }
  // Add the table to the list - we'll process it later
  typename Mir2Lir<pointer_size>::SwitchTable* tab_rec =
      static_cast<typename Mir2Lir<pointer_size>::SwitchTable*>(this->arena_->Alloc(
          sizeof(typename Mir2Lir<pointer_size>::SwitchTable), kArenaAllocData));
  tab_rec->table = table;
  tab_rec->vaddr = this->current_dalvik_offset_;
  int size = table[1];
  tab_rec->targets = static_cast<LIR**>(this->arena_->Alloc(size * sizeof(LIR*), kArenaAllocLIR));
  this->switch_tables_.Insert(tab_rec);

  // Get the switch value
  rl_src = this->LoadValue(rl_src, kCoreReg);
  // NewLIR0(kX86Bkpt);

  // Materialize a pointer to the switch table
  RegStorage start_of_method_reg;
  if (base_of_code_ != nullptr) {
    // We can use the saved value.
    RegLocation rl_method = this->mir_graph_->GetRegLocation(base_of_code_->s_reg_low);
    rl_method = this->LoadValue(rl_method, kCoreReg);
    start_of_method_reg = rl_method.reg;
    store_method_addr_used_ = true;
  } else {
    start_of_method_reg = this->AllocTemp();
    this->NewLIR1(kX86StartOfMethod, start_of_method_reg.GetReg());
  }
  int low_key = this->s4FromSwitchData(&table[2]);
  RegStorage keyReg;
  // Remove the bias, if necessary
  if (low_key == 0) {
    keyReg = rl_src.reg;
  } else {
    keyReg = this->AllocTemp();
    OpRegRegImm(kOpSub, keyReg, rl_src.reg, low_key);
  }
  // Bounds check - if < 0 or >= size continue following switch
  OpRegImm(kOpCmp, keyReg, size-1);
  LIR* branch_over = OpCondBranch(kCondHi, NULL);

  // Load the displacement from the switch table
  RegStorage disp_reg = this->AllocTemp();
  this->NewLIR5(kX86PcRelLoadRA, disp_reg.GetReg(), start_of_method_reg.GetReg(), keyReg.GetReg(),
                2, this->WrapPointer(tab_rec));
  // Add displacement to start of method
  OpRegReg(kOpAdd, start_of_method_reg, disp_reg);
  // ..and go!
  LIR* switch_branch = this->NewLIR1(kX86JmpR, start_of_method_reg.GetReg());
  tab_rec->anchor = switch_branch;

  /* branch_over target here */
  LIR* target = this->NewLIR0(kPseudoTargetLabel);
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
X86MIR2LIR(void, GenFillArrayData(DexOffset table_offset, RegLocation rl_src)) {
  const uint16_t* table = this->cu_->insns + this->current_dalvik_offset_ + table_offset;
  // Add the table to the list - we'll process it later
  typename Mir2Lir<pointer_size>::FillArrayData* tab_rec =
      static_cast<typename Mir2Lir<pointer_size>::FillArrayData*>(this->arena_->Alloc(
          sizeof(typename Mir2Lir<pointer_size>::FillArrayData), kArenaAllocData));
  tab_rec->table = table;
  tab_rec->vaddr = this->current_dalvik_offset_;
  uint16_t width = tab_rec->table[1];
  uint32_t size = tab_rec->table[2] | ((static_cast<uint32_t>(tab_rec->table[3])) << 16);
  tab_rec->size = (size * width) + 8;

  this->fill_array_data_.Insert(tab_rec);

  // Making a call - use explicit registers
  this->FlushAllRegs();   /* Everything to home location */
  this->LoadValueDirectFixed(rl_src, rs_rX86_ARG0);
  // Materialize a pointer to the fill data image
  if (base_of_code_ != nullptr) {
    // We can use the saved value.
    RegLocation rl_method = this->mir_graph_->GetRegLocation(base_of_code_->s_reg_low);
    this->LoadValueDirect(rl_method, rs_rX86_ARG2);
    store_method_addr_used_ = true;
  } else {
    this->NewLIR1(kX86StartOfMethod, rs_rX86_ARG2.GetReg());
  }
  this->NewLIR2(kX86PcRelAdr, rs_rX86_ARG1.GetReg(), this->WrapPointer(tab_rec));
  this->NewLIR2(kX86Add32RR, rs_rX86_ARG1.GetReg(), rs_rX86_ARG2.GetReg());
  this->CallRuntimeHelperRegReg(QUICK_ENTRYPOINT_OFFSET(pointer_size, pHandleFillArrayData),
                                rs_rX86_ARG0, rs_rX86_ARG1, true);
}

X86MIR2LIR(void, GenMoveException(RegLocation rl_dest)) {
  int ex_offset = Thread::ExceptionOffset<pointer_size>().Int32Value();
  RegLocation rl_result = this->EvalLoc(rl_dest, kCoreReg, true);
  this->NewLIR2(kX86Mov32RT, rl_result.reg.GetReg(), ex_offset);
  this->NewLIR2(kX86Mov32TI, ex_offset, 0);
  this->StoreValue(rl_dest, rl_result);
}

/*
 * Mark garbage collection card. Skip if the value we're storing is null.
 */
X86MIR2LIR(void, MarkGCCard(RegStorage val_reg, RegStorage tgt_addr_reg)) {
  RegStorage reg_card_base = this->AllocTemp();
  RegStorage reg_card_no = this->AllocTemp();
  LIR* branch_over = OpCmpImmBranch(kCondEq, val_reg, 0, NULL);
  this->NewLIR2(kX86Mov32RT, reg_card_base.GetReg(),
                Thread::CardTableOffset<pointer_size>().Int32Value());
  OpRegRegImm(kOpLsr, reg_card_no, tgt_addr_reg, gc::accounting::CardTable::kCardShift);
  StoreBaseIndexed(reg_card_base, reg_card_no, reg_card_base, 0, kUnsignedByte);
  LIR* target = this->NewLIR0(kPseudoTargetLabel);
  branch_over->target = target;
  this->FreeTemp(reg_card_base);
  this->FreeTemp(reg_card_no);
}

X86MIR2LIR(void, GenEntrySequence(RegLocation* ArgLocs, RegLocation rl_method)) {
  /*
   * On entry, rX86_ARG0, rX86_ARG1, rX86_ARG2 are live.  Let the register
   * allocation mechanism know so it doesn't try to use any of them when
   * expanding the frame or flushing.  This leaves the utility
   * code with no spare temps.
   */
  this->LockTemp(rs_rX86_ARG0);
  this->LockTemp(rs_rX86_ARG1);
  this->LockTemp(rs_rX86_ARG2);

  /* Build frame, return address already on stack */
  // TODO: 64 bit.
  stack_decrement_ = OpRegImm(kOpSub, rs_rX86_SP, this->frame_size_ - 4);

  /*
   * We can safely skip the stack overflow check if we're
   * a leaf *and* our frame size < fudge factor.
   */
  const bool skip_overflow_check = (this->mir_graph_->MethodIsLeaf() &&
      (static_cast<size_t>(this->frame_size_) < Thread::kStackOverflowReservedBytes));
  this->NewLIR0(kPseudoMethodEntry);
  /* Spill core callee saves */
  SpillCoreRegs();
  /* NOTE: promotion of FP regs currently unsupported, thus no FP spill */
  DCHECK_EQ(this->num_fp_spills_, 0);
  if (!skip_overflow_check) {
    class StackOverflowSlowPath : public Mir2Lir<pointer_size>::LIRSlowPath {
     public:
      StackOverflowSlowPath(Mir2Lir<pointer_size>* m2l, LIR* branch, size_t sp_displace)
          : Mir2Lir<pointer_size>::LIRSlowPath(m2l, m2l->GetCurrentDexPc(), branch, nullptr),
            sp_displace_(sp_displace) {
      }
      void Compile() OVERRIDE {
        this->m2l_->ResetRegPool();
        this->m2l_->ResetDefTracking();
        this->GenerateTargetLabel(kPseudoThrowTarget);
        this->m2l_->OpRegImm(kOpAdd, rs_rX86_SP, sp_displace_);
        this->m2l_->ClobberCallerSave();
        ThreadOffset<pointer_size> func_offset = QUICK_ENTRYPOINT_OFFSET(pointer_size,
                                                                         pThrowStackOverflow);
        // Assumes codegen and target are in thumb2 mode.
        this->m2l_->CallHelper(RegStorage::InvalidReg(), func_offset, false /* MarkSafepointPC */,
                               false /* UseLink */);
      }

     private:
      const size_t sp_displace_;
    };
    // TODO: for large frames we should do something like:
    // spill ebp
    // lea ebp, [esp + frame_size]
    // cmp ebp, fs:[stack_end_]
    // jcc stack_overflow_exception
    // mov esp, ebp
    // in case a signal comes in that's not using an alternate signal stack and the large frame may
    // have moved us outside of the reserved area at the end of the stack.
    // cmp rX86_SP, fs:[stack_end_]; jcc throw_slowpath
    OpRegThreadMem(kOpCmp, rs_rX86_SP, Thread::StackEndOffset<pointer_size>());
    LIR* branch = OpCondBranch(kCondUlt, nullptr);
    this->AddSlowPath(new(this->arena_)StackOverflowSlowPath(this, branch,
                                                             this->frame_size_ - pointer_size));
  }

  this->FlushIns(ArgLocs, rl_method);

  if (base_of_code_ != nullptr) {
    // We have been asked to save the address of the method start for later use.
    setup_method_address_[0] = this->NewLIR1(kX86StartOfMethod, rs_rX86_ARG0.GetReg());
    int displacement = this->SRegOffset(base_of_code_->s_reg_low);
    // Native pointer - must be natural word size.
    setup_method_address_[1] = this->StoreWordDisp(rs_rX86_SP, displacement, rs_rX86_ARG0);
  }

  this->FreeTemp(rs_rX86_ARG0);
  this->FreeTemp(rs_rX86_ARG1);
  this->FreeTemp(rs_rX86_ARG2);
}

X86MIR2LIR(void, GenExitSequence()) {
  /*
   * In the exit path, rX86_RET0/rX86_RET1 are live - make sure they aren't
   * allocated by the register utilities as temps.
   */
  this->LockTemp(rs_rX86_RET0);
  this->LockTemp(rs_rX86_RET1);

  this->NewLIR0(kPseudoMethodExit);
  this->UnSpillCoreRegs();
  /* Remove frame except for return address */
  stack_increment_ = OpRegImm(kOpAdd, rs_rX86_SP, this->frame_size_ - pointer_size);
  this->NewLIR0(kX86Ret);
}

X86MIR2LIR(void, GenSpecialExitSequence()) {
  this->NewLIR0(kX86Ret);
}

}  // namespace art
