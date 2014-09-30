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

#ifndef ART_COMPILER_DEX_QUICK_ARM_CODEGEN_ARM_H_
#define ART_COMPILER_DEX_QUICK_ARM_CODEGEN_ARM_H_

#include "arm_lir.h"
#include "dex/compiler_internals.h"
#include "dex/quick/mir_to_lir.h"
#include "utils/arena_containers.h"

namespace art {

class ArmMir2Lir FINAL : public Mir2Lir {
 protected:
  class InToRegStorageMapper {
   public:
    virtual RegStorage GetNextReg(bool is_double_or_float, bool is_wide) = 0;
    virtual ~InToRegStorageMapper() {
    }
  };

  class InToRegStorageArmMapper : public InToRegStorageMapper {
   public:
    InToRegStorageArmMapper()
        : cur_core_reg_(0), cur_fp_reg_(0), cur_fp_double_reg_(0) {
    }
    virtual ~InToRegStorageArmMapper() {
    }
    RegStorage GetNextReg(bool is_double_or_float, bool is_wide) OVERRIDE;
   private:
    int cur_core_reg_;
    int cur_fp_reg_;
    int cur_fp_double_reg_;
  };

  class InToRegStorageMapping {
   public:
    InToRegStorageMapping()
        : max_mapped_in_(0), is_there_stack_mapped_(false), initialized_(false) {
    }
    void Initialize(RegLocation* arg_locs, int count, InToRegStorageMapper* mapper);
    int GetMaxMappedIn() {
      return max_mapped_in_;
    }
    bool IsThereStackMapped() {
      return is_there_stack_mapped_;
    }
    RegStorage Get(int in_position);
    bool IsInitialized() {
      return initialized_;
    }
   private:
    std::map<int, RegStorage> mapping_;
    int max_mapped_in_;
    bool is_there_stack_mapped_;
    bool initialized_;
  };

  public:
    ArmMir2Lir(CompilationUnit* cu, MIRGraph* mir_graph, ArenaAllocator* arena);

    // Required for target - codegen helpers.
    bool SmallLiteralDivRem(Instruction::Code dalvik_opcode, bool is_div, RegLocation rl_src,
                            RegLocation rl_dest, int lit);
    bool EasyMultiply(RegLocation rl_src, RegLocation rl_dest, int lit) OVERRIDE;
    LIR* CheckSuspendUsingLoad() OVERRIDE;
    RegStorage LoadHelper(QuickEntrypointEnum trampoline) OVERRIDE;
    LIR* LoadBaseDisp(RegStorage r_base, int displacement, RegStorage r_dest,
                      OpSize size, VolatileKind is_volatile) OVERRIDE;
    LIR* LoadBaseIndexed(RegStorage r_base, RegStorage r_index, RegStorage r_dest, int scale,
                         OpSize size) OVERRIDE;
    LIR* LoadConstantNoClobber(RegStorage r_dest, int value);
    LIR* LoadConstantWide(RegStorage r_dest, int64_t value);
    LIR* StoreBaseDisp(RegStorage r_base, int displacement, RegStorage r_src,
                       OpSize size, VolatileKind is_volatile) OVERRIDE;
    LIR* StoreBaseIndexed(RegStorage r_base, RegStorage r_index, RegStorage r_src, int scale,
                          OpSize size) OVERRIDE;
    void MarkGCCard(RegStorage val_reg, RegStorage tgt_addr_reg);

    // Required for target - register utilities.
    RegStorage CTargetReg(SpecialTargetRegister reg) OVERRIDE;
    RegStorage TargetReg(SpecialTargetRegister reg) OVERRIDE;
    RegStorage TargetReg(SpecialTargetRegister reg, WideKind wide_kind) OVERRIDE {
      if (wide_kind == kWide) {
        DCHECK((kArg0 <= reg && reg < kArg3) || (kFArg0 <= reg && reg < kFArg15) || (kRet0 == reg));
        // TODO: Return 64-bit fp register instead of register pair. But need to be careful on this,
        // as some mir_2lir common code assume 64-bit registers are always register pairs.
        return RegStorage::MakeRegPair(TargetReg(reg),
                                       TargetReg(static_cast<SpecialTargetRegister>(reg + 1)));
      } else {
        return TargetReg(reg);
      }
    }
    RegStorage TargetPtrReg(SpecialTargetRegister reg) OVERRIDE {
      return TargetReg(reg);
    }
    RegStorage TargetReg(SpecialTargetRegister reg, RegLocation loc) OVERRIDE {
      if (loc.ref) {
        return TargetReg(reg, kRef);
      } else {
        return TargetReg(reg, loc.wide ? kWide : kNotWide);
      }
    }

    RegStorage GetArgMappingToPhysicalReg(int arg_num) OVERRIDE;
    RegLocation GetCReturnAlt() OVERRIDE;
    RegLocation GetCReturnWideAlt() OVERRIDE;
    RegLocation LocCReturn() OVERRIDE;
    RegLocation LocCReturnRef() OVERRIDE;
    RegLocation LocCReturnDouble() OVERRIDE;
    RegLocation LocCReturnFloat() OVERRIDE;
    RegLocation LocCReturnWide() OVERRIDE;
    RegLocation LocReturnDouble() OVERRIDE;
    RegLocation LocReturnFloat() OVERRIDE;
    ResourceMask GetRegMaskCommon(const RegStorage& reg) const OVERRIDE;
    void AdjustSpillMask();
    void ClobberCallerSave();
    void FreeCallTemps();
    void LockCallTemps();
    void MarkPreservedSingle(int v_reg, RegStorage reg);
    void MarkPreservedDouble(int v_reg, RegStorage reg);
    void CompilerInitializeRegAlloc();

    // Required for target - miscellaneous.
    void AssembleLIR();
    uint32_t LinkFixupInsns(LIR* head_lir, LIR* tail_lir, CodeOffset offset);
    int AssignInsnOffsets();
    void AssignOffsets();
    static uint8_t* EncodeLIRs(uint8_t* write_pos, LIR* lir);
    void DumpResourceMask(LIR* lir, const ResourceMask& mask, const char* prefix) OVERRIDE;
    void SetupTargetResourceMasks(LIR* lir, uint64_t flags,
                                  ResourceMask* use_mask, ResourceMask* def_mask) OVERRIDE;
    const char* GetTargetInstFmt(int opcode);
    const char* GetTargetInstName(int opcode);
    std::string BuildInsnString(const char* fmt, LIR* lir, unsigned char* base_addr);
    ResourceMask GetPCUseDefEncoding() const OVERRIDE;
    uint64_t GetTargetInstFlags(int opcode);
    size_t GetInsnSize(LIR* lir) OVERRIDE;
    bool IsUnconditionalBranch(LIR* lir);

    // Get the register class for load/store of a field.
    RegisterClass RegClassForFieldLoadStore(OpSize size, bool is_volatile) OVERRIDE;

    // Required for target - Dalvik-level generators.
    void GenArithOpLong(Instruction::Code opcode, RegLocation rl_dest, RegLocation rl_src1,
                        RegLocation rl_src2) OVERRIDE;
    void GenArithImmOpLong(Instruction::Code opcode, RegLocation rl_dest,
                           RegLocation rl_src1, RegLocation rl_src2);
    void GenArrayGet(int opt_flags, OpSize size, RegLocation rl_array,
                     RegLocation rl_index, RegLocation rl_dest, int scale);
    void GenArrayPut(int opt_flags, OpSize size, RegLocation rl_array, RegLocation rl_index,
                     RegLocation rl_src, int scale, bool card_mark);
    void GenShiftImmOpLong(Instruction::Code opcode, RegLocation rl_dest,
                           RegLocation rl_src1, RegLocation rl_shift);
    void GenArithOpDouble(Instruction::Code opcode, RegLocation rl_dest, RegLocation rl_src1,
                          RegLocation rl_src2);
    void GenArithOpFloat(Instruction::Code opcode, RegLocation rl_dest, RegLocation rl_src1,
                         RegLocation rl_src2);
    void GenCmpFP(Instruction::Code opcode, RegLocation rl_dest, RegLocation rl_src1,
                  RegLocation rl_src2);
    void GenConversion(Instruction::Code opcode, RegLocation rl_dest, RegLocation rl_src);
    bool GenInlinedAbsFloat(CallInfo* info) OVERRIDE;
    bool GenInlinedAbsDouble(CallInfo* info) OVERRIDE;
    bool GenInlinedCas(CallInfo* info, bool is_long, bool is_object);
    bool GenInlinedMinMax(CallInfo* info, bool is_min, bool is_long);
    bool GenInlinedSqrt(CallInfo* info);
    bool GenInlinedPeek(CallInfo* info, OpSize size);
    bool GenInlinedPoke(CallInfo* info, OpSize size);
    bool GenInlinedArrayCopyCharArray(CallInfo* info) OVERRIDE;
    RegLocation GenDivRem(RegLocation rl_dest, RegStorage reg_lo, RegStorage reg_hi, bool is_div);
    RegLocation GenDivRemLit(RegLocation rl_dest, RegStorage reg_lo, int lit, bool is_div);
    void GenCmpLong(RegLocation rl_dest, RegLocation rl_src1, RegLocation rl_src2);
    void GenDivZeroCheckWide(RegStorage reg);
    void GenEntrySequence(RegLocation* ArgLocs, RegLocation rl_method);
    void GenExitSequence();
    void GenSpecialExitSequence();
    void GenFillArrayData(MIR* mir, DexOffset table_offset, RegLocation rl_src);
    void GenFusedFPCmpBranch(BasicBlock* bb, MIR* mir, bool gt_bias, bool is_double);
    void GenFusedLongCmpBranch(BasicBlock* bb, MIR* mir);
    void GenSelect(BasicBlock* bb, MIR* mir);
    void GenSelectConst32(RegStorage left_op, RegStorage right_op, ConditionCode code,
                          int32_t true_val, int32_t false_val, RegStorage rs_dest,
                          int dest_reg_class) OVERRIDE;
    bool GenMemBarrier(MemBarrierKind barrier_kind);
    void GenMonitorEnter(int opt_flags, RegLocation rl_src);
    void GenMonitorExit(int opt_flags, RegLocation rl_src);
    void GenMoveException(RegLocation rl_dest);
    void GenMultiplyByTwoBitMultiplier(RegLocation rl_src, RegLocation rl_result, int lit,
                                       int first_bit, int second_bit);
    void GenNegDouble(RegLocation rl_dest, RegLocation rl_src);
    void GenNegFloat(RegLocation rl_dest, RegLocation rl_src);
    void GenLargePackedSwitch(MIR* mir, DexOffset table_offset, RegLocation rl_src);
    void GenLargeSparseSwitch(MIR* mir, DexOffset table_offset, RegLocation rl_src);

    // Required for target - single operation generators.
    LIR* OpUnconditionalBranch(LIR* target);
    LIR* OpCmpBranch(ConditionCode cond, RegStorage src1, RegStorage src2, LIR* target);
    LIR* OpCmpImmBranch(ConditionCode cond, RegStorage reg, int check_value, LIR* target);
    LIR* OpCondBranch(ConditionCode cc, LIR* target);
    LIR* OpDecAndBranch(ConditionCode c_code, RegStorage reg, LIR* target);
    LIR* OpFpRegCopy(RegStorage r_dest, RegStorage r_src);
    LIR* OpIT(ConditionCode cond, const char* guide);
    void UpdateIT(LIR* it, const char* new_guide);
    void OpEndIT(LIR* it);
    LIR* OpMem(OpKind op, RegStorage r_base, int disp);
    LIR* OpPcRelLoad(RegStorage reg, LIR* target);
    LIR* OpReg(OpKind op, RegStorage r_dest_src);
    void OpRegCopy(RegStorage r_dest, RegStorage r_src);
    LIR* OpRegCopyNoInsert(RegStorage r_dest, RegStorage r_src);
    LIR* OpRegImm(OpKind op, RegStorage r_dest_src1, int value);
    LIR* OpRegReg(OpKind op, RegStorage r_dest_src1, RegStorage r_src2);
    LIR* OpMovRegMem(RegStorage r_dest, RegStorage r_base, int offset, MoveType move_type);
    LIR* OpMovMemReg(RegStorage r_base, int offset, RegStorage r_src, MoveType move_type);
    LIR* OpCondRegReg(OpKind op, ConditionCode cc, RegStorage r_dest, RegStorage r_src);
    LIR* OpRegRegImm(OpKind op, RegStorage r_dest, RegStorage r_src1, int value);
    LIR* OpRegRegReg(OpKind op, RegStorage r_dest, RegStorage r_src1, RegStorage r_src2);
    LIR* OpTestSuspend(LIR* target);
    LIR* OpVldm(RegStorage r_base, int count);
    LIR* OpVstm(RegStorage r_base, int count);
    void OpRegCopyWide(RegStorage dest, RegStorage src);

    LIR* LoadBaseDispBody(RegStorage r_base, int displacement, RegStorage r_dest, OpSize size);
    LIR* StoreBaseDispBody(RegStorage r_base, int displacement, RegStorage r_src, OpSize size);
    LIR* OpRegRegRegShift(OpKind op, RegStorage r_dest, RegStorage r_src1, RegStorage r_src2,
                          int shift);
    LIR* OpRegRegShift(OpKind op, RegStorage r_dest_src1, RegStorage r_src2, int shift);
    static const ArmEncodingMap EncodingMap[kArmLast];
    int EncodeShift(int code, int amount);
    int ModifiedImmediate(uint32_t value);
    ArmConditionCode ArmConditionEncoding(ConditionCode code);
    bool InexpensiveConstantInt(int32_t value);
    bool InexpensiveConstantFloat(int32_t value);
    bool InexpensiveConstantLong(int64_t value);
    bool InexpensiveConstantDouble(int64_t value);
    RegStorage AllocPreservedDouble(int s_reg);
    RegStorage AllocPreservedSingle(int s_reg);

    bool WideGPRsAreAliases() OVERRIDE {
      return false;  // Wide GPRs are formed by pairing.
    }
    bool WideFPRsAreAliases() OVERRIDE {
      return false;  // Wide FPRs are formed by pairing.
    }

    NextCallInsn GetNextSDCallInsn() OVERRIDE;

    /*
     * @brief Generate a relative call to the method that will be patched at link time.
     * @param target_method The MethodReference of the method to be invoked.
     * @param type How the method will be invoked.
     * @returns Call instruction
     */
    LIR* CallWithLinkerFixup(const MethodReference& target_method, InvokeType type);

    /*
     * @brief Generate the actual call insn based on the method info.
     * @param method_info the lowering info for the method call.
     * @returns Call instruction
     */
    LIR* GenCallInsn(const MirMethodLoweringInfo& method_info) OVERRIDE;

    /*
     * @brief Handle ARM specific literals.
     */
    void InstallLiteralPools() OVERRIDE;

    LIR* InvokeTrampoline(OpKind op, RegStorage r_tgt, QuickEntrypointEnum trampoline) OVERRIDE;
    size_t GetInstructionOffset(LIR* lir);

    void FlushIns(RegLocation* ArgLocs, RegLocation rl_method) OVERRIDE;
    int GenDalvikArgsNoRange(CallInfo* info, int call_state, LIR** pcrLabel,
                             NextCallInsn next_call_insn,
                             const MethodReference& target_method,
                             uint32_t vtable_idx,
                             uintptr_t direct_code, uintptr_t direct_method, InvokeType type,
                             bool skip_this) OVERRIDE;
    int GenDalvikArgsRange(CallInfo* info, int call_state, LIR** pcrLabel,
                           NextCallInsn next_call_insn,
                           const MethodReference& target_method,
                           uint32_t vtable_idx,
                           uintptr_t direct_code, uintptr_t direct_method, InvokeType type,
                           bool skip_this) OVERRIDE;

  private:
    void GenNegLong(RegLocation rl_dest, RegLocation rl_src);
    void GenMulLong(Instruction::Code opcode, RegLocation rl_dest, RegLocation rl_src1,
                    RegLocation rl_src2);
    void GenFusedLongCmpImmBranch(BasicBlock* bb, RegLocation rl_src1, int64_t val,
                                  ConditionCode ccode);
    LIR* LoadFPConstantValue(int r_dest, int value);
    LIR* LoadStoreUsingInsnWithOffsetImm8Shl2(ArmOpcode opcode, RegStorage r_base,
                                              int displacement, RegStorage r_src_dest,
                                              RegStorage r_work = RegStorage::InvalidReg());
    void ReplaceFixup(LIR* prev_lir, LIR* orig_lir, LIR* new_lir);
    void InsertFixupBefore(LIR* prev_lir, LIR* orig_lir, LIR* new_lir);
    void AssignDataOffsets();
    RegLocation GenDivRem(RegLocation rl_dest, RegLocation rl_src1, RegLocation rl_src2,
                          bool is_div, bool check_zero);
    RegLocation GenDivRemLit(RegLocation rl_dest, RegLocation rl_src1, int lit, bool is_div);
    typedef struct {
      OpKind op;
      uint32_t shift;
    } EasyMultiplyOp;
    bool GetEasyMultiplyOp(int lit, EasyMultiplyOp* op);
    bool GetEasyMultiplyTwoOps(int lit, EasyMultiplyOp* ops);
    void GenEasyMultiplyTwoOps(RegStorage r_dest, RegStorage r_src, EasyMultiplyOp* ops);

    static constexpr ResourceMask GetRegMaskArm(RegStorage reg);
    static constexpr ResourceMask EncodeArmRegList(int reg_list);
    static constexpr ResourceMask EncodeArmRegFpcsList(int reg_list);

    ArenaVector<LIR*> call_method_insns_;

    /**
     * @brief Given float register pair, returns Solo64 float register.
     * @param reg #RegStorage containing a float register pair (e.g. @c s2 and @c s3).
     * @return A Solo64 float mapping to the register pair (e.g. @c d1).
     */
    static RegStorage As64BitFloatReg(RegStorage reg) {
      DCHECK(reg.IsFloat());

      RegStorage low = reg.GetLow();
      RegStorage high = reg.GetHigh();
      DCHECK(low.GetRegNum() %2 == 0 && low.GetRegNum() + 1 == high.GetRegNum());

      return RegStorage::FloatSolo64(low.GetRegNum() / 2);
    }

    /**
     * @brief Given Solo64 float register, returns float register pair.
     * @param reg #RegStorage containing a Solo64 float register (e.g. @c d1).
     * @return A float register pair mapping to the Solo64 float pair (e.g. @c s2 and s3).
     */
    static RegStorage As64BitFloatRegPair(RegStorage reg) {
      DCHECK(reg.IsDouble() && reg.Is64BitSolo());

      int reg_num = reg.GetRegNum();
      return RegStorage::MakeRegPair(RegStorage::FloatSolo32(reg_num * 2),
                                     RegStorage::FloatSolo32(reg_num * 2 + 1));
    }

    InToRegStorageMapping in_to_reg_storage_mapping_;
};

}  // namespace art

#endif  // ART_COMPILER_DEX_QUICK_ARM_CODEGEN_ARM_H_
