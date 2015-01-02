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

#ifndef ART_COMPILER_DEX_GVN_DEAD_CODE_ELIMINATION_H_
#define ART_COMPILER_DEX_GVN_DEAD_CODE_ELIMINATION_H_

#include "global_value_numbering.h"
#include "utils/arena_object.h"
#include "utils/scoped_arena_containers.h"

namespace art {

class ArenaBitVector;
class BasicBlock;
class LocalValueNumbering;
class MIR;
class MIRGraph;

/**
 * @class DeadCodeElimination
 * @details Eliminate dead code based on the results of global value numbering.
 * Also get rid of MOVE insns when we can use the source instead of destination
 * without affecting the vreg values at safepoints; this is useful in methods
 * with a large number of vregs that frequently move values to and from low vregs
 * to accommodate insns that can work only with the low 16 or 256 vregs.
 */
class GvnDeadCodeElimination : public DeletableArenaObject<kArenaAllocMisc> {
 public:
  GvnDeadCodeElimination(const GlobalValueNumbering* gvn, ScopedArenaAllocator* alloc);

  // Apply the DCE to a basic block.
  void Apply(BasicBlock* bb);

  // Check if the SSA representation needs to be recalculated.
  bool RecalculateSsaRep() const {
    return recalculate_ssa_rep_;
  }

 private:
  static constexpr uint16_t kNoValue = GlobalValueNumbering::kNoValue;
  static constexpr uint16_t kNPos = 0xffffu;

  struct VRegValue {
    VRegValue() : value(kNoValue), change(kNPos) { }

    // Value name as reported by GVN, kNoValue if not available.
    uint16_t value;
    // Index of the change in mir_data_ that defined the value, kNPos if initial value for the BB.
    uint16_t change;
  };

  struct MIRData {
    explicit MIRData(MIR* m)
        : mir(m), uses_all_vregs(false), must_keep(false), is_move(false), is_move_src(false),
          revert_def(false), revert_def_high(false), revert_depends(false),
          has_def(false), wide_def(false),
          low_def_over_high_word(false), high_def_over_low_word(false), vreg_def(0u),
          prev_value(), prev_value_high() {
    }

    uint16_t PrevChange(int v_reg) const;
    void SetPrevChange(int v_reg, uint16_t change);
    void RemovePrevChange(int v_reg, MIRData* prev_data);

    MIR* mir;
    bool uses_all_vregs : 1;  // If mir uses all vregs, uses in mir->ssa_rep are irrelevant.
    bool must_keep : 1;
    bool is_move : 1;
    bool is_move_src : 1;

    // Flags for marking sequences that we're trying to eliminate when we find out that
    // one or more vregs revert to a previously held value at the end of the sequence.
    bool revert_def : 1;        // This is the first MIR that changes vreg_def.
    bool revert_def_high : 1;   // This is the first MIR that changes vreg_def + 1.
    bool revert_depends : 1;    // This change depends on one of the MIRs we're trying to kill.

    bool has_def : 1;
    bool wide_def : 1;
    bool low_def_over_high_word : 1;
    bool high_def_over_low_word : 1;
    uint16_t vreg_def;
    VRegValue prev_value;
    VRegValue prev_value_high;   // For wide defs.
  };

  class VRegChains {
   public:
    VRegChains(uint32_t num_vregs, ScopedArenaAllocator* alloc);
    void Reset();
    void AddMIR(const MIRData& data);
    size_t NumMIRs() const;
    const MIRData* GetMIR(size_t pos) const;
    MIRData* GetMIR(size_t pos);

    uint16_t GetLastVRegChange(int v_reg);
    uint16_t GetCurrentVRegValue(int v_reg);

    uint16_t MarkRevertDef(int v_reg);
    uint16_t FindFirstChangeAfter(int v_reg, uint16_t change) const;
    void ReplaceChange(uint16_t old_change, uint16_t new_change);
    void RemoveChange(uint16_t change);

    /* private: */
    const uint32_t num_vregs_;
    VRegValue* const vreg_data_;
    ScopedArenaVector<MIRData> mir_data_;
  };

  void RecordPass();
  void BackwardPass();

  void ValidateVRegChain(int v_reg);
  void KillMIR(MIRData* data);
  void ChangeBinOp2AddrToPlainBinOp(MIR* mir);

  // Update dependent vregs going backwards through a MIR.
  void BackwardsUpdateAllowedDependentVRegs(const MIRData& data);

  size_t BackwardPassTryToKillRevertVRegs();
  void BackwardPassTryToKillLastMIR();

  bool IsSRegUsed(uint16_t first_change, uint16_t last_change, int s_reg) const;
  void RenameDefSReg(uint16_t change, int new_s_reg);
  void RenameUses(uint16_t first_change, uint16_t last_change,
                  int old_s_reg, int new_s_reg);
  void RecordPassKillMoveByRenamingSrcDef(uint16_t src_change, uint16_t move_change);
  void RecordPassTryToKillOverwrittenMoveOrMoveSrc(uint16_t check_change);
  void RecordPassTryToKillOverwrittenMoveOrMoveSrc();
  void RecordPassTryToKillLastMIR();

  void RevertVRegs(const MIRData& data);
  void InsertInitialValueHigh(int v_reg, uint16_t value);
  void RecordVRegDef(MIRData* data, bool wide, int v_reg, uint16_t new_value);
  void RecordVRegDef(MIRData* data, MIR* mir);
  bool RecordMIR(MIR* mir);

  const GlobalValueNumbering* const gvn_;
  MIRGraph* const mir_graph_;
  size_t num_vregs_;

  BasicBlock* bb_;
  const LocalValueNumbering* lvn_;
  VRegChains vreg_chains_;
  size_t no_uses_all_since_;  // The change index after the last change with uses_all_vregs set.

  // Data used when processing MIRs in reverse order.
  ArenaBitVector* allowed_dependent_vregs_;   // vregs that are not needed later.
  ArenaBitVector* revert_vregs_;              // vregs that revert to a previous value.
  ArenaBitVector* changed_revert_vregs_;
  ArenaBitVector* dependent_vregs_;

  bool recalculate_ssa_rep_;

  struct VRegDataProxy {
    explicit VRegDataProxy(GvnDeadCodeElimination* dce) : dce_(dce) { }
    size_t size() const {
      return dce_->vreg_chains_.num_vregs_;
    }
    VRegValue& operator[](size_t n) {
      DCHECK_LT(n, size());
      return dce_->vreg_chains_.vreg_data_[n];
    }
    const VRegValue& operator[](size_t n) const {
      DCHECK_LT(n, size());
      return dce_->vreg_chains_.vreg_data_[n];
    }
    GvnDeadCodeElimination* const dce_;
  };
  VRegDataProxy vreg_data_;

  struct MIRDataProxy {
    explicit MIRDataProxy(GvnDeadCodeElimination* dce) : dce_(dce) { }
    size_t size() const {
      return dce_->vreg_chains_.mir_data_.size();
    }
    MIRData& operator[](size_t n) {
      DCHECK_LT(n, size());
      return dce_->vreg_chains_.mir_data_[n];
    }
    const MIRData& operator[](size_t n) const {
      DCHECK_LT(n, size());
      return dce_->vreg_chains_.mir_data_[n];
    }
    bool empty() const {
      return dce_->vreg_chains_.mir_data_.empty();
    }
    MIRData& back() {
      DCHECK(!empty());
      return dce_->vreg_chains_.mir_data_.back();
    }
    const MIRData& back() const {
      DCHECK(!empty());
      return dce_->vreg_chains_.mir_data_.back();
    }
    void push_back(const MIRData& data) {
      dce_->vreg_chains_.mir_data_.push_back(data);
    }
    void pop_back() {
      DCHECK(!empty());
      return dce_->vreg_chains_.mir_data_.pop_back();
    }
    GvnDeadCodeElimination* const dce_;
  };
  MIRDataProxy mir_data_;
};

}  // namespace art

#endif  // ART_COMPILER_DEX_GVN_DEAD_CODE_ELIMINATION_H_
