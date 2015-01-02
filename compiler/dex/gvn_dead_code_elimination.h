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
        : mir(m), uses_all_vregs(false), must_keep(false), is_move(false),
          loop_def(false), loop_def_high(false), loop_depends(false),
          has_def(false), wide_def(false),
          low_def_over_high_word(false), high_def_over_low_word(false), vreg_def(0u),
          prev_value(), prev_value_high() {
    }

    MIR* mir;
    bool uses_all_vregs : 1;  // If mir uses all vregs, uses in mir->ssa_rep are irrelevant.
    bool must_keep : 1;
    bool is_move : 1;
    bool loop_def : 1;        // TODO: document.
    bool loop_def_high : 1;   // TODO: document.
    bool loop_depends : 1;    // TODO: document.
    bool has_def : 1;
    bool wide_def : 1;
    bool low_def_over_high_word : 1;
    bool high_def_over_low_word : 1;
    uint16_t vreg_def;
    VRegValue prev_value;
    VRegValue prev_value_high;   // For wide defs.
  };

  void RecordPass();
  void BackwardPass();

  void RemoveChangeFromVRegChain(int v_reg, uint16_t change);
  uint16_t MarkLoopDef(int v_reg);
  uint16_t FindFirstValueChangeAfter(int v_reg, uint16_t move_change) const;
  void KillMIR(MIRData* data);

  // Update dependent vregs going backwards through a MIR.
  void BackwardsUpdateAllowedDependentVRegs(const MIRData& data);

  size_t BackwardPassTryToKillLoopVRegDefs();
  void BackwardPassTryToKillLastMIR();

  void RenameDefSReg(uint16_t change, int new_s_reg);
  void RenameUses(uint16_t first_change, uint16_t last_change,
                  int old_s_reg, int new_s_reg);
  void RecordPassTryToKillOverwrittenMove(uint16_t move_change);
  void RecordPassTryToKillOverwrittenMove();
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
  ScopedArenaVector<MIRData> mir_data_;
  ScopedArenaVector<VRegValue> vreg_data_;
  size_t past_last_uses_all_change_;

  // Data used when processing MIRs in reverse order.
  ArenaBitVector* allowed_dependent_vregs_;  // vregs that are not needed later.
  ArenaBitVector* loop_vregs_;
  ArenaBitVector* changed_loop_vregs_;
  ArenaBitVector* dependent_vregs_;

  bool recalculate_ssa_rep_;
};

}  // namespace art

#endif  // ART_COMPILER_DEX_GVN_DEAD_CODE_ELIMINATION_H_
