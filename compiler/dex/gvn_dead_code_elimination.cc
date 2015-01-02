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

#include "gvn_dead_code_elimination.h"

#include "base/bit_vector-inl.h"
#include "base/macros.h"
#include "compiler_enums.h"
#include "dataflow_iterator-inl.h"
#include "dex_instruction.h"
#include "dex/mir_graph.h"
#include "local_value_numbering.h"
#include "utils/arena_bit_vector.h"

namespace art {

extern pthread_mutex_t vmarko_mutex;
extern uint32_t vmarko_killed;
extern uint32_t vmarko_killed_move_rename_dest;
extern uint32_t vmarko_killed_simple;
extern uint32_t vmarko_killed_unused;
extern uint32_t vmarko_killed_move_rename_src;
extern uint32_t vmarko_killed_complex;

pthread_mutex_t vmarko_mutex = PTHREAD_MUTEX_INITIALIZER;
uint32_t vmarko_killed = 0u;
uint32_t vmarko_killed_move_rename_dest = 0u;
uint32_t vmarko_killed_simple = 0u;
uint32_t vmarko_killed_unused = 0u;
uint32_t vmarko_killed_move_rename_src = 0u;
uint32_t vmarko_killed_complex = 0u;

constexpr uint16_t GvnDeadCodeElimination::kNoValue;
constexpr uint16_t GvnDeadCodeElimination::kNPos;

constexpr bool kVerboseDebugKillMoveRenameDest = false;
constexpr bool kVerboseDebugKillUnused = false;
constexpr bool kVerboseDebugKillMoveRenameSrc = false;
constexpr bool kVerboseDebug = false;
constexpr bool kVerboseDebug2 = false;
constexpr bool kVerboseDebug3 = false;
constexpr bool kVerboseDebugKillLongSequences = false;

GvnDeadCodeElimination::GvnDeadCodeElimination(const GlobalValueNumbering* gvn,
                                         ScopedArenaAllocator* alloc)
    : gvn_(gvn),
      mir_graph_(gvn_->GetMirGraph()),
      num_vregs_(mir_graph_->GetNumOfCodeAndTempVRs()),
      bb_(nullptr),
      lvn_(nullptr),
      mir_data_(alloc->Adapter()),
      vreg_data_(alloc->Adapter()),
      past_last_uses_all_change_(0u),
      allowed_dependent_vregs_(new (alloc) ArenaBitVector(alloc, num_vregs_, false)),
      loop_vregs_(new (alloc) ArenaBitVector(alloc, num_vregs_, false)),
      changed_loop_vregs_(new (alloc) ArenaBitVector(alloc, num_vregs_, false)),
      dependent_vregs_(new (alloc) ArenaBitVector(alloc, num_vregs_, false)),
      recalculate_ssa_rep_(false) {
}

void GvnDeadCodeElimination::Apply(BasicBlock* bb) {
  bb_ = bb;
  lvn_ = gvn_->GetLvn(bb->id);

  RecordPass();
  BackwardPass();

  mir_data_.clear();   // TODO: Just DCHECK(mir_data_.empty());
  vreg_data_.clear();  // TODO: Just DCHECK(vreg_data_.empty());
  past_last_uses_all_change_ = 0u;  // TODO: DCHECK_EQ(past_last_uses_all_change_, 0u);
  lvn_ = nullptr;
  bb_ = nullptr;
}

void GvnDeadCodeElimination::RecordPass() {
  // Record MIRs with vreg definition data, eliminate single instructions.
  DCHECK(vreg_data_.empty());
  vreg_data_.resize(mir_graph_->GetNumOfCodeAndTempVRs(), VRegValue());
  DCHECK(mir_data_.empty());
  mir_data_.reserve(100);
  DCHECK_EQ(past_last_uses_all_change_, 0u);
  for (MIR* mir = bb_->first_mir_insn; mir != nullptr; mir = mir->next) {
    if (RecordMIR(mir)) {
      RecordPassTryToKillOverwrittenMove();
      RecordPassTryToKillLastMIR();
    }
  }
}

void GvnDeadCodeElimination::BackwardPass() {
  // Now process MIRs in reverse order, trying to eliminate them.
  allowed_dependent_vregs_->ClearAllBits();  // Implicitly depend on all vregs at the end of BB.
  while (!mir_data_.empty()) {
    BackwardPassTryToKillLastMIR();
    const MIRData& data = mir_data_.back();
    BackwardsUpdateAllowedDependentVRegs(data);
    RevertVRegs(data);
    if (data.uses_all_vregs) {
      DCHECK_EQ(past_last_uses_all_change_, mir_data_.size());
      --past_last_uses_all_change_;
      while (past_last_uses_all_change_ != 0u &&
          !mir_data_[past_last_uses_all_change_ - 1].uses_all_vregs) {
        --past_last_uses_all_change_;
      }
    }
    mir_data_.pop_back();
  }
}

void GvnDeadCodeElimination::RemoveChangeFromVRegChain(int v_reg, uint16_t change) {
  DCHECK_LT(static_cast<size_t>(v_reg), vreg_data_.size());
  DCHECK_LT(vreg_data_[v_reg].change, mir_data_.size());
  DCHECK(mir_data_[change].vreg_def == v_reg || mir_data_[change].vreg_def + 1 == v_reg);
  MIRData* data = &mir_data_[vreg_data_[v_reg].change];
  DCHECK(data->vreg_def == v_reg || data->vreg_def + 1 == v_reg);
  if (vreg_data_[v_reg].change == change) {
    vreg_data_[v_reg] = (data->vreg_def == v_reg) ? data->prev_value : data->prev_value_high;
    return;
  }
  while (true) {
    if (data->vreg_def == v_reg) {
      if (data->prev_value.change == change) {
        if (mir_data_[change].vreg_def == v_reg) {
          data->prev_value = mir_data_[change].prev_value;
          data->low_def_over_high_word = mir_data_[change].low_def_over_high_word;
        } else {
          data->prev_value = mir_data_[change].prev_value_high;
          data->low_def_over_high_word = (data->prev_value.value != kNoValue) &&
              !mir_data_[change].high_def_over_low_word;
        }
        break;
      }
      data = &mir_data_[data->prev_value.change];
    } else {
      if (data->prev_value_high.change == change) {
        if (mir_data_[change].vreg_def == v_reg) {
          data->prev_value_high = mir_data_[change].prev_value;
          data->high_def_over_low_word = (data->prev_value_high.value != kNoValue) &&
              !mir_data_[change].low_def_over_high_word;
        } else {
          data->prev_value_high = mir_data_[change].prev_value_high;
          data->high_def_over_low_word = mir_data_[change].high_def_over_low_word;
        }
        break;
      }
      data = &mir_data_[data->prev_value_high.change];
    }
    DCHECK(data->vreg_def == v_reg || data->vreg_def + 1 == v_reg);
  }
}

uint16_t GvnDeadCodeElimination::MarkLoopDef(int v_reg) {
  DCHECK_LT(static_cast<size_t>(v_reg), vreg_data_.size());
  uint16_t current_value = vreg_data_[v_reg].value;
  DCHECK_NE(current_value, kNoValue);
  uint16_t change = vreg_data_[v_reg].change;
  DCHECK_LT(change, mir_data_.size());
  bool match_high_word = (mir_data_[change].vreg_def != v_reg);
  do {
    MIRData* data = &mir_data_[change];
    DCHECK(data->vreg_def == v_reg || data->vreg_def + 1 == v_reg);
    if (data->vreg_def == v_reg) {  // Low word, use prev_value.
      if (data->prev_value.value == current_value &&
          match_high_word == data->low_def_over_high_word) {
        data->loop_def = true;
        break;
      }
      change = data->prev_value.change;
    } else {  // High word, use prev_value_high.
      if (data->prev_value_high.value == current_value &&
          match_high_word != data->high_def_over_low_word) {
        data->loop_def_high = true;
        break;
      }
      change = data->prev_value_high.change;
    }
  } while (change != kNPos);
  return change;
}

uint16_t GvnDeadCodeElimination::FindFirstValueChangeAfter(int v_reg, uint16_t move_change) const {
  DCHECK_LT(static_cast<size_t>(v_reg), vreg_data_.size());
  DCHECK_LT(move_change, mir_data_.size());
  uint16_t first_change = kNPos;
  uint16_t change = vreg_data_[v_reg].change;
  while (change != kNPos && change > move_change) {
    first_change = change;
    const MIRData* data = &mir_data_[change];
    DCHECK(data->vreg_def == v_reg || data->vreg_def + 1 == v_reg);
    if (data->vreg_def == v_reg) {  // Low word, use prev_value.
      change = data->prev_value.change;
    } else {  // High word, use prev_value_high.
      change = data->prev_value_high.change;
    }
  }
  return first_change;
}

void GvnDeadCodeElimination::KillMIR(MIRData* data) {
  DCHECK(!data->must_keep);
  DCHECK(!data->uses_all_vregs);
  DCHECK(data->has_def);
  DCHECK(data->mir->ssa_rep->num_defs == 1 || data->mir->ssa_rep->num_defs == 2);

  if (kVerboseDebug3) {
    LOG(INFO) << "In BB#" << lvn_->Id() << "@0x" << std::hex << data->mir->offset
        << ": ELIMINATING";
  }
  data->mir->dalvikInsn.opcode = static_cast<Instruction::Code>(kMirOpNop);
  data->mir->ssa_rep->num_uses = 0;
  data->mir->ssa_rep->num_defs = 0;

  pthread_mutex_lock(&vmarko_mutex);
  vmarko_killed += 1u;
  pthread_mutex_unlock(&vmarko_mutex);
}

void GvnDeadCodeElimination::BackwardsUpdateAllowedDependentVRegs(const MIRData& data) {
  if (data.uses_all_vregs) {
    DCHECK(data.must_keep);
    allowed_dependent_vregs_->ClearAllBits();
  } else {
    if (data.has_def) {
      allowed_dependent_vregs_->SetBit(data.vreg_def);
      if (data.wide_def) {
        allowed_dependent_vregs_->SetBit(data.vreg_def + 1);
      }
    }
    for (int i = 0, num_uses = data.mir->ssa_rep->num_uses; i != num_uses; ++i) {
      int v_reg = mir_graph_->SRegToVReg(data.mir->ssa_rep->uses[i]);
      allowed_dependent_vregs_->ClearBit(v_reg);
    }
  }
}

void GvnDeadCodeElimination::RenameDefSReg(uint16_t change, int new_s_reg) {
  DCHECK_LT(change, mir_data_.size());
  DCHECK(mir_data_[change].has_def);
  DCHECK_EQ(mir_data_[change].vreg_def, mir_graph_->SRegToVReg(new_s_reg));
  DCHECK_NE(mir_data_[change].mir->ssa_rep->defs[0], new_s_reg);
  int old_s_reg = mir_data_[change].mir->ssa_rep->defs[0];
  mir_data_[change].mir->ssa_rep->defs[0] = new_s_reg;
  bool wide = mir_data_[change].wide_def;
  if (wide) {
    DCHECK_EQ(mir_data_[change].mir->ssa_rep->defs[1], old_s_reg + 1);
    mir_data_[change].mir->ssa_rep->defs[1] = new_s_reg + 1;
  }
  for (size_t c = change + 1u, end = mir_data_.size(); c != end; ++c) {
    SSARepresentation* ssa_rep = mir_data_[c].mir->ssa_rep;
    for (int i = 0; i != ssa_rep->num_uses; ++i) {
      if (ssa_rep->uses[i] == old_s_reg) {
        ssa_rep->uses[i] = new_s_reg;
        if (wide) {
          ++i;
          DCHECK_LT(i, ssa_rep->num_uses);
          ssa_rep->uses[i] = new_s_reg + 1;
        }
      }
    }
  }
}

void GvnDeadCodeElimination::RenameUses(uint16_t first_change, uint16_t last_change,
                                        int old_s_reg, int new_s_reg) {
  uint32_t old_v_reg = mir_graph_->SRegToVReg(old_s_reg);
  uint32_t new_v_reg = mir_graph_->SRegToVReg(new_s_reg);
  for (size_t c = first_change; c != last_change; ++c) {
    MIR* mir = mir_data_[c].mir;
    uint64_t df_attr = mir_graph_->GetDataFlowAttributes(mir);
    size_t use = 0u;
#define REPLACE_VREG(REG) \
    if ((df_attr & DF_U##REG) != 0) {                                 \
      if (mir->ssa_rep->uses[use] == old_s_reg) {                     \
        DCHECK_EQ(mir->dalvikInsn.v##REG, old_v_reg);                 \
        mir->dalvikInsn.v##REG = new_v_reg;                           \
        mir->ssa_rep->uses[use] = new_s_reg;                          \
        if ((df_attr & DF_##REG##_WIDE) != 0) {                       \
          DCHECK_EQ(mir->ssa_rep->uses[use + 1], old_s_reg + 1);      \
          mir->ssa_rep->uses[use + 1] = new_s_reg + 1;                \
        }                                                             \
      }                                                               \
      use += ((df_attr & DF_##REG##_WIDE) != 0) ? 2 : 1;              \
    }
    REPLACE_VREG(A)
    REPLACE_VREG(B)
    REPLACE_VREG(C)
#undef REPLACE_VREG
    DCHECK_EQ(use, static_cast<size_t>(mir->ssa_rep->num_uses));
  }
}

void GvnDeadCodeElimination::RecordPassTryToKillOverwrittenMove(uint16_t move_change) {
  DCHECK_LT(move_change, mir_data_.size());
  DCHECK(mir_data_[move_change].is_move);
  MIRData* data = &mir_data_[move_change];
  int32_t dest_s_reg = data->mir->ssa_rep->defs[0];
  int32_t src_s_reg = data->mir->ssa_rep->uses[0];
  uint32_t dest_v_reg = mir_graph_->SRegToVReg(dest_s_reg);
  uint32_t src_v_reg = mir_graph_->SRegToVReg(src_s_reg);

  // Check if source vreg has changed since the MOVE.
  uint16_t src_change = FindFirstValueChangeAfter(src_v_reg, move_change);
  bool wide = data->wide_def;
  if (wide) {
    uint16_t src_change_high = FindFirstValueChangeAfter(src_v_reg + 1, move_change);
    if (src_change_high != kNPos && (src_change == kNPos || src_change_high < src_change)) {
      src_change = src_change_high;
    }
  }
  size_t rename_end = mir_data_.size();
  if (src_change != kNPos) {
    // The source vreg has changed. Check if the MOVE dest is used after that change.
    for (size_t c = src_change + 1u, end = mir_data_.size(); c != end; ++c) {
      SSARepresentation* ssa_rep = mir_data_[c].mir->ssa_rep;
      for (int i = 0; i != ssa_rep->num_uses; ++i) {
        if (ssa_rep->uses[i] == dest_s_reg) {
          return;  // MOVE dest is used after src has been overwritten. Can't simply change uses.
        }
      }
    }
    rename_end = src_change + 1u;
  }

  // We can simply change all uses of dest to src.
  RenameUses(move_change + 1u, rename_end, dest_s_reg, src_s_reg);

  if (kVerboseDebugKillMoveRenameDest) {
    CompilationUnit* cu = gvn_->GetCompilationUnit();
    LOG(INFO) << "In BB#" << lvn_->Id() << "@0x" << std::hex << data->mir->offset << std::dec
        << " " << PrettyMethod(cu->method_idx, *cu->dex_file) << ": ELIMINATING "
        << data->mir->dalvikInsn.opcode << " " << dest_v_reg << ", " << src_v_reg
        << " when processing MIR @0x" << std::hex << mir_data_.back().mir->offset;
  }

  // Now, remove the MOVE from the vreg chain(s) and kill it.
  RemoveChangeFromVRegChain(dest_v_reg, move_change);
  if (data->wide_def) {
    RemoveChangeFromVRegChain(dest_v_reg + 1, move_change);
  }
  KillMIR(data);
  data->has_def = false;
  pthread_mutex_lock(&vmarko_mutex);
  vmarko_killed_move_rename_dest += 1u;
  pthread_mutex_unlock(&vmarko_mutex);
}

void GvnDeadCodeElimination::RecordPassTryToKillOverwrittenMove() {
  MIRData* data = &mir_data_.back();
  if (!data->has_def) {
    // TODO: Consider killing MOVEs at return/return-object/return-wide.
    return;
  }
  // Check if we're overwriting a MOVE. For MOVE_WIDE, we may be overwriting partially;
  // if that's the case, check that the other word wasn't previously overwritten.
  if (data->prev_value.change != kNPos && data->prev_value.change >= past_last_uses_all_change_ &&
      mir_data_[data->prev_value.change].is_move) {
    MIRData* move_data = &mir_data_[data->prev_value.change];
    bool newly_overwritten = false;
    if (!move_data->wide_def) {
      // Narrow move; always fully overwritten by the last MIR.
      newly_overwritten = true;
    } else if (data->low_def_over_high_word) {
      // Overwriting only the high word; is the low word still valid?
      DCHECK_EQ(move_data->vreg_def + 1u, data->vreg_def);
      if (vreg_data_[move_data->vreg_def].change == data->prev_value.change) {
        newly_overwritten = true;
      }
    } else if (!data->wide_def) {
      // Overwriting only the low word, is the high word still valid?
      if (vreg_data_[data->vreg_def + 1].change == data->prev_value.change) {
        newly_overwritten = true;
      }
    } else {
      // Overwriting both words; was the high word still from the same move?
      if (data->prev_value_high.change == data->prev_value.change) {
        newly_overwritten = true;
      }
    }
    if (newly_overwritten) {
      RecordPassTryToKillOverwrittenMove(data->prev_value.change);
    }
  }
  if (data->wide_def && data->high_def_over_low_word &&
      data->prev_value_high.change != kNPos &&
      data->prev_value_high.change >= past_last_uses_all_change_ &&
      mir_data_[data->prev_value_high.change].is_move) {
    MIRData* move_data = &mir_data_[data->prev_value_high.change];
    bool newly_overwritten = false;
    if (!move_data->wide_def) {
      // Narrow move; always fully overwritten by the last MIR.
      newly_overwritten = true;
    } else if (vreg_data_[move_data->vreg_def + 1].change == data->prev_value_high.change) {
      // High word is still valid.
      newly_overwritten = true;
    }
    if (newly_overwritten) {
      RecordPassTryToKillOverwrittenMove(data->prev_value_high.change);
    }
  }
}

void GvnDeadCodeElimination::RecordPassTryToKillLastMIR() {
  MIRData* data = &mir_data_.back();
  if (!data->must_keep &&
      data->has_def && vreg_data_[data->vreg_def].value == data->prev_value.value
      && (!data->wide_def ||
          (data->prev_value_high.value == data->prev_value.value &&
           !data->low_def_over_high_word && !data->high_def_over_low_word))) {
    if (kVerboseDebug2) {
      LOG(INFO) << "DCE: In BB#" << bb_->id << "@0x" << std::hex << data->mir->offset
          << " Overwriting vreg " << data->vreg_def << " value " << data->prev_value.value
          << " with the same.";
    }
    RevertVRegs(*data);
    uint16_t prev_change = data->prev_value.change;
    int new_s_reg = data->mir->ssa_rep->defs[0];
    KillMIR(data);
    mir_data_.pop_back();
    if (prev_change != kNPos) {
      RenameDefSReg(prev_change, new_s_reg);
    } else {
      recalculate_ssa_rep_ = true;
    }
    pthread_mutex_lock(&vmarko_mutex);
    vmarko_killed_simple += 1u;
    pthread_mutex_unlock(&vmarko_mutex);
  }
}

size_t GvnDeadCodeElimination::BackwardPassTryToKillLoopVRegDefs() {
  // RAII class to clean mir_data_[.].loop_*.
  class CleanTempDepends {
   public:
    explicit CleanTempDepends(GvnDeadCodeElimination* dce)
        : dce_(dce), start_(kNPos) {
    }

    ~CleanTempDepends() {
      if (start_ != kNPos) {
        for (size_t c = start_, size = dce_->mir_data_.size(); c != size; ++c) {
          dce_->mir_data_[c].loop_def = false;
          dce_->mir_data_[c].loop_def_high = false;
          dce_->mir_data_[c].loop_depends = false;
        }
      }
    }

    void UpdateStart(uint16_t change) {
      if (start_ == kNPos || start_ > change) {
        start_ = change;
      }
    }

    uint16_t Start() const {
      return start_;
    }

   private:
    GvnDeadCodeElimination* dce_;
    uint16_t start_;
  };
  CleanTempDepends clean_temp_depends(this);

  // Mark forced dependencies that change the original values.
  for (uint32_t v_reg : loop_vregs_->Indexes()) {
    uint16_t first_loop_change = MarkLoopDef(v_reg);
    if (first_loop_change == kNPos) {
      return 0u;
    }
    clean_temp_depends.UpdateStart(first_loop_change);
  }
  DCHECK_NE(clean_temp_depends.Start(), kNPos);

  changed_loop_vregs_->ClearAllBits();
  dependent_vregs_->ClearAllBits();
  for (size_t c = clean_temp_depends.Start(), size = mir_data_.size(); c != size; ++c) {
    MIRData* data = &mir_data_[c];
    bool depends = false;
    // Permit changes to loop vregs.
    if (data->loop_def) {
      changed_loop_vregs_->SetBit(data->vreg_def);
      depends = true;  // We need to eliminate this insn.
    }
    if (data->loop_def_high) {
      changed_loop_vregs_->SetBit(data->vreg_def + 1);
      depends = true;  // We need to eliminate this insn.
    }
    // Check for insns that write the loop vregs that we already changed.
    if (!depends && data->has_def &&
        (changed_loop_vregs_->IsBitSet(data->vreg_def) ||
         (data->wide_def && changed_loop_vregs_->IsBitSet(data->vreg_def + 1)))) {
      depends = true;
    }
    if (!depends) {
      // Check for explicit dependency.
      SSARepresentation* ssa_rep = data->mir->ssa_rep;
      for (int i = 0; i != ssa_rep->num_uses; ++i) {
        if (dependent_vregs_->IsBitSet(mir_graph_->SRegToVReg(ssa_rep->uses[i]))) {
          depends = true;
          break;
        }
      }
    }
    // Now check if we can eliminate the insn if we need to.
    if (depends && data->must_keep) {
      return 0u;  // Can't eliminate.
    }
    if (depends && data->has_def &&
        ((loop_vregs_->IsBitSet(data->vreg_def) &&
          changed_loop_vregs_->IsBitSet(data->vreg_def)) ||
         (data->wide_def &&
          loop_vregs_->IsBitSet(data->vreg_def) &&
          changed_loop_vregs_->IsBitSet(data->vreg_def)))) {
      return 0u;  // Can't eliminate; tries to write a loop vreg it's not yet allowed to modify.
    }
    // Finally, update the data.
    if (depends) {
      data->loop_depends = true;
      if (data->has_def) {
        dependent_vregs_->SetBit(data->vreg_def);
        if (data->wide_def) {
          dependent_vregs_->SetBit(data->vreg_def + 1);
        }
      }
    } else {
      if (data->has_def) {
        dependent_vregs_->ClearBit(data->vreg_def);
        if (data->wide_def) {
          dependent_vregs_->ClearBit(data->vreg_def + 1);
        }
      }
    }
  }

  // Check for dependent regs that are needed later.
  size_t overwritten_needed_vregs = 0u;
  for (uint32_t idx : dependent_vregs_->Indexes()) {
    uint16_t change = vreg_data_[idx].change;
    DCHECK_NE(change, kNPos);
    MIRData* data = &mir_data_[change];
    if (data->wide_def &&
        vreg_data_[(idx == data->vreg_def) ? idx + 1u : idx - 1u].change != change) {
      // Wide def with the other half overwritten. Not a real dependency.
      dependent_vregs_->ClearBit(idx);
    } else if (!loop_vregs_->IsBitSet(idx) && !allowed_dependent_vregs_->IsBitSet(idx)) {
      // Overwrites a register needed later.
      ++overwritten_needed_vregs;
    }
  }

  if (overwritten_needed_vregs == 0u) {
    bool is_retry = false;
    DCHECK(mir_data_.back().has_def);
    for (uint32_t v_reg : loop_vregs_->Indexes()) {
      if (mir_data_.back().vreg_def != v_reg &&
          (!mir_data_.back().wide_def || mir_data_.back().vreg_def + 1u != v_reg)) {
        is_retry = true;
      }
    }
    if (kVerboseDebugKillLongSequences || is_retry) {
      std::string loop;
      {
        std::ostringstream oss;
        oss << "{";
        const char* s = " ";
        for (uint32_t idx : loop_vregs_->Indexes()) {
          oss << s << idx;
        }
        oss << " }";
        loop = oss.str();
      }
      std::string dep;
      {
        std::ostringstream oss;
        oss << "{";
        const char* s = " ";
        for (uint32_t idx : dependent_vregs_->Indexes()) {
          oss << s << idx;
        }
        oss << " }";
        dep = oss.str();
      }
      std::string dep_insns;
      {
        std::ostringstream oss;
        oss << "{" << std::hex;
        const char* s = " ";
        for (size_t c = clean_temp_depends.Start(), size = mir_data_.size(); c != size; ++c) {
          if (mir_data_[c].loop_depends) {
            oss << s << mir_data_[c].mir->offset;
            s = ", ";
          }
        }
        oss << " }";
        dep_insns = oss.str();
      }
      CompilationUnit* cu = gvn_->GetCompilationUnit();
      LOG(INFO) << "In BB#" << lvn_->Id() << "@0x" << std::hex << mir_data_.back().mir->offset
          << " " << PrettyMethod(cu->method_idx, *cu->dex_file) << ": ELIMINATING SEQUENCE "
          << mir_data_.back().mir->dalvikInsn.opcode << " revert MIRs " << dep_insns
          << " loop=" << loop << " dep=" << dep << " / start = " << clean_temp_depends.Start()
          << " @0x" << std::hex << mir_data_[clean_temp_depends.Start()].mir->offset
          << (is_retry ? " RETRY" : "");
    }
    // Kill all MIRs marked as dependent.
    allowed_dependent_vregs_->Union(loop_vregs_);
    size_t c = mir_data_.size();
    while (c != clean_temp_depends.Start()) {
      --c;
      MIRData* data = &mir_data_[c];
      if (data->loop_depends) {
        DCHECK(!data->must_keep);
        DCHECK(data->has_def);
        RemoveChangeFromVRegChain(data->vreg_def, c);
        if (data->wide_def) {
          RemoveChangeFromVRegChain(data->vreg_def + 1, c);
        }
        KillMIR(data);
        data->has_def = false;
      }
    }
    recalculate_ssa_rep_ = true;
  }

  return overwritten_needed_vregs;
}

void GvnDeadCodeElimination::BackwardPassTryToKillLastMIR() {
  DCHECK(!mir_data_.empty());
  MIRData* data = &mir_data_.back();
  if (data->must_keep) {
    return;
  }
  DCHECK(!data->uses_all_vregs);
  if (!data->has_def) {
    // Previously eliminated.
    DCHECK_EQ(static_cast<int>(data->mir->dalvikInsn.opcode), static_cast<int>(kMirOpNop));
    return;
  }
  if (allowed_dependent_vregs_->IsBitSet(data->vreg_def) ||
      (data->wide_def && allowed_dependent_vregs_->IsBitSet(data->vreg_def + 1))) {
    if (data->wide_def) {
      // For wide defs, one of the vregs may still be considered needed, fix that.
      allowed_dependent_vregs_->SetBit(data->vreg_def);
      allowed_dependent_vregs_->SetBit(data->vreg_def + 1);
    }
    if (kVerboseDebugKillUnused) {
      CompilationUnit* cu = gvn_->GetCompilationUnit();
      LOG(INFO) << "In BB#" << lvn_->Id() << "@0x" << std::hex << data->mir->offset
          << " " << PrettyMethod(cu->method_idx, *cu->dex_file) << ": ELIMINATING (NOT USED) "
          << data->mir->dalvikInsn.opcode;
    }
    RevertVRegs(*data);
    pthread_mutex_lock(&vmarko_mutex);
    vmarko_killed_unused += 1u;
    pthread_mutex_unlock(&vmarko_mutex);
    KillMIR(data);
    data->has_def = false;
    return;
  }
  if (data->is_move) {
    // TODO: It should be possible to do this in the RecordPass() which will simplify
    // the flow that we're dealing with in BackwardPassTryToKillLoopVRegDefs(). This
    // may affect the need (or lack thereof) for the retry below.

    // If the src vreg isn't needed after this move, try to rename it to the dest vreg.
    // We can only do that if we know the src change and the dest vreg didn't change since then.
    int32_t src_s_reg = data->mir->ssa_rep->uses[0];
    int src_v_reg = mir_graph_->SRegToVReg(src_s_reg);
    uint16_t src_change = vreg_data_[src_v_reg].change;
    if (src_change != kNPos && src_change >= past_last_uses_all_change_ &&
        static_cast<int>(mir_data_[src_change].mir->dalvikInsn.opcode) != kMirOpPhi &&
        (data->prev_value.change == kNPos || data->prev_value.change <= src_change) &&
        (data->prev_value_high.change == kNPos || data->prev_value_high.change <= src_change) &&
        (allowed_dependent_vregs_->IsBitSet(src_v_reg) ||
            (data->wide_def &&
                (allowed_dependent_vregs_->IsBitSet(src_v_reg + 1) ||
                 src_v_reg + 1 == data->vreg_def || src_v_reg == data->vreg_def + 1)))) {
      if (kVerboseDebugKillMoveRenameSrc) {
        CompilationUnit* cu = gvn_->GetCompilationUnit();
        LOG(INFO) << "In BB#" << lvn_->Id() << "@0x" << std::hex << data->mir->offset << std::dec
            << " " << PrettyMethod(cu->method_idx, *cu->dex_file) << ": ELIMINATING (RENAME) "
            << data->mir->dalvikInsn.opcode << " " << data->vreg_def << ", " << src_v_reg;
      }
      // Remove src_change from the vreg chain(s).
      RemoveChangeFromVRegChain(src_v_reg, src_change);
      if (data->wide_def) {
        RemoveChangeFromVRegChain(src_v_reg + 1, src_change);
      }
      // Replace the move change with the src_change, copying all necessary data.
      int32_t dest_s_reg = data->mir->ssa_rep->defs[0];
      MIRData* src_data = &mir_data_[src_change];
      src_data->low_def_over_high_word = data->low_def_over_high_word;
      src_data->high_def_over_low_word = data->high_def_over_low_word;
      src_data->vreg_def = data->vreg_def;
      src_data->prev_value = data->prev_value;
      src_data->prev_value_high = data->prev_value_high;
      src_data->mir->dalvikInsn.vA = data->vreg_def;
      src_data->mir->ssa_rep->defs[0] = dest_s_reg;
      vreg_data_[data->vreg_def].change = src_change;
      if (data->wide_def) {
        DCHECK_EQ(src_data->mir->ssa_rep->defs[1], src_s_reg + 1);
        src_data->mir->ssa_rep->defs[1] = dest_s_reg + 1;
        vreg_data_[data->vreg_def + 1].change = src_change;
      }
      // Rename uses and kill the move.
      RenameUses(src_change + 1, mir_data_.size() - 1u, src_s_reg, dest_s_reg);
      KillMIR(data);
      data->has_def = false;
      pthread_mutex_lock(&vmarko_mutex);
      vmarko_killed_move_rename_src += 1u;
      pthread_mutex_unlock(&vmarko_mutex);
      return;
    }
  }
  loop_vregs_->ClearAllBits();
  loop_vregs_->SetBit(data->vreg_def);
  if (data->wide_def) {
    loop_vregs_->SetBit(data->vreg_def + 1);
  }
  size_t num_dependent_vregs = BackwardPassTryToKillLoopVRegDefs();
  if (num_dependent_vregs != 0u && num_dependent_vregs <= 2u) {
    // Add dependent vregs to loop vregs and try again.
    // TODO: Evaluate if this is actually worth the effort.
    DCHECK_NE(static_cast<int>(data->mir->dalvikInsn.opcode), static_cast<int>(kMirOpNop));
    loop_vregs_->Union(dependent_vregs_);
    size_t result = BackwardPassTryToKillLoopVRegDefs();
    if (static_cast<int>(data->mir->dalvikInsn.opcode) == kMirOpNop) {
      DCHECK_EQ(result, 0u);
      CompilationUnit* cu = gvn_->GetCompilationUnit();
      LOG(INFO) << "In BB#" << lvn_->Id() << "@0x" << std::hex << data->mir->offset
          << " " << PrettyMethod(cu->method_idx, *cu->dex_file) << ": ELIMINATED ON RETRY";
    }
  }
}

void GvnDeadCodeElimination::RevertVRegs(const MIRData& data) {
  if (data.has_def) {
    vreg_data_[data.vreg_def] = data.prev_value;
    if (data.wide_def) {
      vreg_data_[data.vreg_def + 1] = data.prev_value_high;
    }
  }
}

void GvnDeadCodeElimination::InsertInitialValueHigh(int v_reg, uint16_t value) {
  DCHECK_NE(value, kNoValue);
  DCHECK_LT(static_cast<size_t>(v_reg), vreg_data_.size());
  uint16_t change = vreg_data_[v_reg].change;
  if (change == kNPos) {
    vreg_data_[v_reg].value = value;
  } else {
    while (true) {
      MIRData* data = &mir_data_[change];
      DCHECK(data->vreg_def == v_reg || data->vreg_def + 1 == v_reg);
      if (data->vreg_def == v_reg) {  // Low word, use prev_value.
        if (data->prev_value.change == kNPos) {
          DCHECK_EQ(data->prev_value.value, kNoValue);
          data->prev_value.value = value;
          data->low_def_over_high_word = true;
          break;
        }
        change = data->prev_value.change;
      } else {  // High word, use prev_value_high.
        if (data->prev_value_high.change == kNPos) {
          DCHECK_EQ(data->prev_value_high.value, kNoValue);
          data->prev_value_high.value = value;
          break;
        }
        change = data->prev_value_high.change;
      }
    }
  }
}

void GvnDeadCodeElimination::RecordVRegDef(MIRData* data, bool wide, int v_reg,
                                           uint16_t new_value) {
  data->has_def = true;
  data->wide_def = wide;
  data->vreg_def = v_reg;

  if (vreg_data_[v_reg].change != kNPos &&
      mir_data_[vreg_data_[v_reg].change].vreg_def + 1 == v_reg) {
    data->low_def_over_high_word = true;
  }
  data->prev_value = vreg_data_[v_reg];
  DCHECK_LT(static_cast<size_t>(v_reg), vreg_data_.size());
  vreg_data_[v_reg].value = new_value;
  vreg_data_[v_reg].change = mir_data_.size();

  if (wide) {
    if (vreg_data_[v_reg + 1].change != kNPos &&
        mir_data_[vreg_data_[v_reg + 1].change].vreg_def == v_reg + 1) {
      data->high_def_over_low_word = true;
    }
    data->prev_value_high = vreg_data_[v_reg + 1];
    DCHECK_LT(static_cast<size_t>(v_reg + 1), vreg_data_.size());
    vreg_data_[v_reg + 1].value = new_value;
    vreg_data_[v_reg + 1].change = mir_data_.size();
  }
}

void GvnDeadCodeElimination::RecordVRegDef(MIRData* data, MIR* mir) {
  DCHECK(mir->ssa_rep->num_defs == 1 || mir->ssa_rep->num_defs == 2);
  int s_reg = mir->ssa_rep->defs[0];
  bool wide = (mir->ssa_rep->num_defs == 2);
  uint16_t new_value = wide ? lvn_->GetSregValueWide(s_reg) : lvn_->GetSregValue(s_reg);
  DCHECK_NE(new_value, kNoValue);

  int v_reg = mir_graph_->SRegToVReg(s_reg);
  DCHECK_LT(static_cast<size_t>(v_reg), vreg_data_.size());
  if (!wide) {
    if (vreg_data_[v_reg].value == kNoValue) {
      uint16_t old_value = lvn_->GetStartingVregValueNumber(v_reg);
      if (old_value == kNoValue) {
        // Maybe there was a wide value in v_reg before. Do not check for wide value in v_reg-1,
        // that will be done only if we see a definition of v_reg-1, otherwise it's unnecessary.
        old_value = lvn_->GetStartingVregValueNumberWide(v_reg);
        if (old_value != kNoValue) {
          InsertInitialValueHigh(v_reg + 1, old_value);
        }
      }
      vreg_data_[v_reg].value = old_value;
    }
  } else {
    DCHECK_LT(static_cast<size_t>(v_reg + 1), vreg_data_.size());
    bool check_high = true;
    if (vreg_data_[v_reg].value == kNoValue) {
      uint16_t old_value = lvn_->GetStartingVregValueNumberWide(v_reg);
      if (old_value != kNoValue) {
        InsertInitialValueHigh(v_reg + 1, old_value);
        check_high = false;  // High word has been processed.
      } else {
        // Maybe there was a narrow value before. Do not check for wide value in v_reg-1,
        // that will be done only if we see a definition of v_reg-1, otherwise it's unnecessary.
        old_value = lvn_->GetStartingVregValueNumber(v_reg);
      }
      vreg_data_[v_reg].value = old_value;
    }
    if (check_high && vreg_data_[v_reg + 1].value == kNoValue) {
      uint16_t old_value = lvn_->GetStartingVregValueNumber(v_reg + 1);
      if (old_value == kNoValue && static_cast<size_t>(v_reg + 2) < num_vregs_) {
        // Maybe there was a wide value before.
        old_value = lvn_->GetStartingVregValueNumberWide(v_reg + 1);
        if (old_value != kNoValue) {
          InsertInitialValueHigh(v_reg + 2, old_value);
        }
      }
      vreg_data_[v_reg + 1].value = old_value;
    }
  }

  if (kVerboseDebug) {
    LOG(INFO) << "In BB#" << lvn_->Id() << "@0x" << std::hex << mir->offset << " " << v_reg
        << "[" << s_reg << "]: " << vreg_data_[v_reg].value << " -> " << new_value
        << (wide ? " wide" : "");
  }

  RecordVRegDef(data, wide, v_reg, new_value);
}

bool GvnDeadCodeElimination::RecordMIR(MIR* mir) {
  MIRData data(mir);
  uint16_t opcode = mir->dalvikInsn.opcode;
  switch (opcode) {
    case kMirOpPhi: {
      // We can't recognize wide variables in Phi from num_defs == 2 as we've got two Phis instead.
      DCHECK_EQ(mir->ssa_rep->num_defs, 1);
      int s_reg = mir->ssa_rep->defs[0];
      bool wide = false;
      uint16_t new_value = lvn_->GetSregValue(s_reg);
      if (new_value == kNoValue) {
        wide = true;
        new_value = lvn_->GetSregValueWide(s_reg);
        if (new_value == kNoValue) {
          return false;  // Ignore the high word Phi.
        }
      }

      int v_reg = mir_graph_->SRegToVReg(s_reg);
      DCHECK_LT(static_cast<size_t>(v_reg), vreg_data_.size());
      DCHECK_EQ(vreg_data_[v_reg].value, kNoValue);  // No previous def for this v_reg.
      if (wide) {
        DCHECK_LT(static_cast<size_t>(v_reg + 1), vreg_data_.size());
        DCHECK_EQ(vreg_data_[v_reg + 1].value, kNoValue);
      }
      if (kVerboseDebug) {
        LOG(INFO) << "In BB#" << lvn_->Id() << "@0x" << std::hex << mir->offset
            << " PHI " << v_reg << "[" << s_reg << "]: " << new_value << (wide ? " wide" : "");
      }
      RecordVRegDef(&data, wide, v_reg, new_value);
      break;
    }

    case kMirOpNop:
    case Instruction::NOP:
      // Don't record NOPs.
      return false;

    case kMirOpCheck:
      data.must_keep = true;
      data.uses_all_vregs = true;
      break;

    case Instruction::RETURN_VOID:
    case Instruction::RETURN:
    case Instruction::RETURN_OBJECT:
    case Instruction::RETURN_WIDE:
    case Instruction::GOTO:
    case Instruction::GOTO_16:
    case Instruction::GOTO_32:
    case Instruction::PACKED_SWITCH:
    case Instruction::SPARSE_SWITCH:
    case Instruction::IF_EQ:
    case Instruction::IF_NE:
    case Instruction::IF_LT:
    case Instruction::IF_GE:
    case Instruction::IF_GT:
    case Instruction::IF_LE:
    case Instruction::IF_EQZ:
    case Instruction::IF_NEZ:
    case Instruction::IF_LTZ:
    case Instruction::IF_GEZ:
    case Instruction::IF_GTZ:
    case Instruction::IF_LEZ:
    case kMirOpFusedCmplFloat:
    case kMirOpFusedCmpgFloat:
    case kMirOpFusedCmplDouble:
    case kMirOpFusedCmpgDouble:
    case kMirOpFusedCmpLong:
      data.must_keep = true;
      data.uses_all_vregs = true;  // Keep the implicit dependencies on all vregs.
      break;

    case Instruction::CONST_CLASS:
    case Instruction::CONST_STRING:
    case Instruction::CONST_STRING_JUMBO:
      // NOTE: While we're currently treating CONST_CLASS, CONST_STRING and CONST_STRING_JUMBO
      // as throwing but we could conceivably try and eliminate those exceptions if we're
      // retrieving the class/string repeatedly.
      data.must_keep = true;
      data.uses_all_vregs = true;
      RecordVRegDef(&data, mir);
      break;

    case Instruction::MONITOR_ENTER:
    case Instruction::MONITOR_EXIT:
      // We can actually try and optimize across the acquire operation of MONITOR_ENTER,
      // the value names provided by GVN reflect the possible changes to memory visibility.
      // NOTE: In ART, MONITOR_ENTER and MONITOR_EXIT can throw only NPE.
      data.must_keep = true;
      if ((mir->optimization_flags & MIR_IGNORE_NULL_CHECK) == 0) {
        data.uses_all_vregs = true;
      }
      break;

    case Instruction::INVOKE_DIRECT:
    case Instruction::INVOKE_DIRECT_RANGE:
    case Instruction::INVOKE_VIRTUAL:
    case Instruction::INVOKE_VIRTUAL_RANGE:
    case Instruction::INVOKE_SUPER:
    case Instruction::INVOKE_SUPER_RANGE:
    case Instruction::INVOKE_INTERFACE:
    case Instruction::INVOKE_INTERFACE_RANGE:
    case Instruction::INVOKE_STATIC:
    case Instruction::INVOKE_STATIC_RANGE:
    case Instruction::CHECK_CAST:
    case Instruction::THROW:
    case Instruction::FILLED_NEW_ARRAY:
    case Instruction::FILLED_NEW_ARRAY_RANGE:
    case Instruction::FILL_ARRAY_DATA:
      data.must_keep = true;
      data.uses_all_vregs = true;
      break;

    case Instruction::NEW_INSTANCE:
    case Instruction::NEW_ARRAY:
      data.must_keep = true;
      data.uses_all_vregs = true;
      RecordVRegDef(&data, mir);
      break;

    case kMirOpNullCheck:
      DCHECK_EQ(mir->ssa_rep->num_uses, 1);
      if ((mir->optimization_flags & MIR_IGNORE_NULL_CHECK) == 0) {
        data.must_keep = true;
        data.uses_all_vregs = true;
      } else {
        mir->ssa_rep->num_uses = 0;
        mir->dalvikInsn.opcode = static_cast<Instruction::Code>(kMirOpNop);
        return false;
      }
      break;

    case Instruction::MOVE_RESULT:
    case Instruction::MOVE_RESULT_OBJECT:
    case Instruction::MOVE_RESULT_WIDE:
      RecordVRegDef(&data, mir);
      break;

    case Instruction::INSTANCE_OF:
      RecordVRegDef(&data, mir);
      break;

    case Instruction::MOVE_EXCEPTION:
      data.must_keep = true;
      RecordVRegDef(&data, mir);
      break;

    case kMirOpCopy:
    case Instruction::MOVE:
    case Instruction::MOVE_FROM16:
    case Instruction::MOVE_16:
    case Instruction::MOVE_WIDE:
    case Instruction::MOVE_WIDE_FROM16:
    case Instruction::MOVE_WIDE_16:
    case Instruction::MOVE_OBJECT:
    case Instruction::MOVE_OBJECT_FROM16:
    case Instruction::MOVE_OBJECT_16:
      data.is_move = true;
      FALLTHROUGH_INTENDED;
    case Instruction::CONST_4:
    case Instruction::CONST_16:
    case Instruction::CONST:
    case Instruction::CONST_HIGH16:
    case Instruction::CONST_WIDE_16:
    case Instruction::CONST_WIDE_32:
    case Instruction::CONST_WIDE:
    case Instruction::CONST_WIDE_HIGH16:
    case Instruction::ARRAY_LENGTH:
    case Instruction::CMPL_FLOAT:
    case Instruction::CMPG_FLOAT:
    case Instruction::CMPL_DOUBLE:
    case Instruction::CMPG_DOUBLE:
    case Instruction::CMP_LONG:
    case Instruction::NEG_INT:
    case Instruction::NOT_INT:
    case Instruction::NEG_LONG:
    case Instruction::NOT_LONG:
    case Instruction::NEG_FLOAT:
    case Instruction::NEG_DOUBLE:
    case Instruction::INT_TO_LONG:
    case Instruction::INT_TO_FLOAT:
    case Instruction::INT_TO_DOUBLE:
    case Instruction::LONG_TO_INT:
    case Instruction::LONG_TO_FLOAT:
    case Instruction::LONG_TO_DOUBLE:
    case Instruction::FLOAT_TO_INT:
    case Instruction::FLOAT_TO_LONG:
    case Instruction::FLOAT_TO_DOUBLE:
    case Instruction::DOUBLE_TO_INT:
    case Instruction::DOUBLE_TO_LONG:
    case Instruction::DOUBLE_TO_FLOAT:
    case Instruction::INT_TO_BYTE:
    case Instruction::INT_TO_CHAR:
    case Instruction::INT_TO_SHORT:
    case Instruction::ADD_INT:
    case Instruction::SUB_INT:
    case Instruction::MUL_INT:
    case Instruction::AND_INT:
    case Instruction::OR_INT:
    case Instruction::XOR_INT:
    case Instruction::SHL_INT:
    case Instruction::SHR_INT:
    case Instruction::USHR_INT:
    case Instruction::ADD_LONG:
    case Instruction::SUB_LONG:
    case Instruction::MUL_LONG:
    case Instruction::AND_LONG:
    case Instruction::OR_LONG:
    case Instruction::XOR_LONG:
    case Instruction::SHL_LONG:
    case Instruction::SHR_LONG:
    case Instruction::USHR_LONG:
    case Instruction::ADD_FLOAT:
    case Instruction::SUB_FLOAT:
    case Instruction::MUL_FLOAT:
    case Instruction::DIV_FLOAT:
    case Instruction::REM_FLOAT:
    case Instruction::ADD_DOUBLE:
    case Instruction::SUB_DOUBLE:
    case Instruction::MUL_DOUBLE:
    case Instruction::DIV_DOUBLE:
    case Instruction::REM_DOUBLE:
    case Instruction::ADD_INT_2ADDR:
    case Instruction::SUB_INT_2ADDR:
    case Instruction::MUL_INT_2ADDR:
    case Instruction::AND_INT_2ADDR:
    case Instruction::OR_INT_2ADDR:
    case Instruction::XOR_INT_2ADDR:
    case Instruction::SHL_INT_2ADDR:
    case Instruction::SHR_INT_2ADDR:
    case Instruction::USHR_INT_2ADDR:
    case Instruction::ADD_LONG_2ADDR:
    case Instruction::SUB_LONG_2ADDR:
    case Instruction::MUL_LONG_2ADDR:
    case Instruction::AND_LONG_2ADDR:
    case Instruction::OR_LONG_2ADDR:
    case Instruction::XOR_LONG_2ADDR:
    case Instruction::SHL_LONG_2ADDR:
    case Instruction::SHR_LONG_2ADDR:
    case Instruction::USHR_LONG_2ADDR:
    case Instruction::ADD_FLOAT_2ADDR:
    case Instruction::SUB_FLOAT_2ADDR:
    case Instruction::MUL_FLOAT_2ADDR:
    case Instruction::DIV_FLOAT_2ADDR:
    case Instruction::REM_FLOAT_2ADDR:
    case Instruction::ADD_DOUBLE_2ADDR:
    case Instruction::SUB_DOUBLE_2ADDR:
    case Instruction::MUL_DOUBLE_2ADDR:
    case Instruction::DIV_DOUBLE_2ADDR:
    case Instruction::REM_DOUBLE_2ADDR:
    case Instruction::ADD_INT_LIT16:
    case Instruction::RSUB_INT:
    case Instruction::MUL_INT_LIT16:
    case Instruction::AND_INT_LIT16:
    case Instruction::OR_INT_LIT16:
    case Instruction::XOR_INT_LIT16:
    case Instruction::ADD_INT_LIT8:
    case Instruction::RSUB_INT_LIT8:
    case Instruction::MUL_INT_LIT8:
    case Instruction::AND_INT_LIT8:
    case Instruction::OR_INT_LIT8:
    case Instruction::XOR_INT_LIT8:
    case Instruction::SHL_INT_LIT8:
    case Instruction::SHR_INT_LIT8:
    case Instruction::USHR_INT_LIT8:
      RecordVRegDef(&data, mir);
      break;

    case Instruction::DIV_INT:
    case Instruction::REM_INT:
    case Instruction::DIV_LONG:
    case Instruction::REM_LONG:
    case Instruction::DIV_INT_2ADDR:
    case Instruction::REM_INT_2ADDR:
    case Instruction::DIV_LONG_2ADDR:
    case Instruction::REM_LONG_2ADDR:
      RecordVRegDef(&data, mir);
      if ((mir->optimization_flags & MIR_IGNORE_DIV_ZERO_CHECK) == 0) {
        data.must_keep = true;
        data.uses_all_vregs = true;
      }
      break;

    case Instruction::DIV_INT_LIT16:
    case Instruction::REM_INT_LIT16:
    case Instruction::DIV_INT_LIT8:
    case Instruction::REM_INT_LIT8:
      RecordVRegDef(&data, mir);
      if (mir->dalvikInsn.vC == 0) {  // Explicit division by 0?
        data.must_keep = true;
        data.uses_all_vregs = true;
      }
      break;

    case Instruction::AGET_OBJECT:
    case Instruction::AGET:
    case Instruction::AGET_WIDE:
    case Instruction::AGET_BOOLEAN:
    case Instruction::AGET_BYTE:
    case Instruction::AGET_CHAR:
    case Instruction::AGET_SHORT:
      RecordVRegDef(&data, mir);
      if ((mir->optimization_flags & MIR_IGNORE_NULL_CHECK) == 0 ||
          (mir->optimization_flags & MIR_IGNORE_RANGE_CHECK) == 0) {
        data.must_keep = true;
        data.uses_all_vregs = true;
      }
      break;

    case Instruction::APUT_OBJECT:
    case Instruction::APUT:
    case Instruction::APUT_WIDE:
    case Instruction::APUT_BYTE:
    case Instruction::APUT_BOOLEAN:
    case Instruction::APUT_SHORT:
    case Instruction::APUT_CHAR:
      data.must_keep = true;
      if ((mir->optimization_flags & MIR_IGNORE_NULL_CHECK) == 0 ||
          (mir->optimization_flags & MIR_IGNORE_RANGE_CHECK) == 0) {
        data.uses_all_vregs = true;
      }
      break;

    case Instruction::IGET_OBJECT:
    case Instruction::IGET:
    case Instruction::IGET_WIDE:
    case Instruction::IGET_BOOLEAN:
    case Instruction::IGET_BYTE:
    case Instruction::IGET_CHAR:
    case Instruction::IGET_SHORT: {
      RecordVRegDef(&data, mir);
      const MirIFieldLoweringInfo& info = mir_graph_->GetIFieldLoweringInfo(mir);
      if ((mir->optimization_flags & MIR_IGNORE_NULL_CHECK) == 0 ||
          !info.IsResolved() || !info.FastGet()) {
        data.must_keep = true;
        data.uses_all_vregs = true;
      } else {
        data.must_keep = info.IsVolatile();
      }
      break;
    }

    case Instruction::IPUT_OBJECT:
    case Instruction::IPUT:
    case Instruction::IPUT_WIDE:
    case Instruction::IPUT_BOOLEAN:
    case Instruction::IPUT_BYTE:
    case Instruction::IPUT_CHAR:
    case Instruction::IPUT_SHORT: {
      data.must_keep = true;
      const MirIFieldLoweringInfo& info = mir_graph_->GetIFieldLoweringInfo(mir);
      if ((mir->optimization_flags & MIR_IGNORE_NULL_CHECK) == 0 ||
          !info.IsResolved() || !info.FastPut()) {
        data.uses_all_vregs = true;
      }
      break;
    }

    case Instruction::SGET_OBJECT:
    case Instruction::SGET:
    case Instruction::SGET_WIDE:
    case Instruction::SGET_BOOLEAN:
    case Instruction::SGET_BYTE:
    case Instruction::SGET_CHAR:
    case Instruction::SGET_SHORT: {
      RecordVRegDef(&data, mir);
      const MirSFieldLoweringInfo& info = mir_graph_->GetSFieldLoweringInfo(mir);
      if ((mir->optimization_flags & MIR_CLASS_IS_INITIALIZED) == 0 ||
          !info.IsResolved() || !info.FastGet()) {
        data.must_keep = true;
        data.uses_all_vregs = true;
      } else {
        data.must_keep = info.IsVolatile();
      }
      break;
    }

    case Instruction::SPUT_OBJECT:
    case Instruction::SPUT:
    case Instruction::SPUT_WIDE:
    case Instruction::SPUT_BOOLEAN:
    case Instruction::SPUT_BYTE:
    case Instruction::SPUT_CHAR:
    case Instruction::SPUT_SHORT: {
      data.must_keep = true;
      const MirSFieldLoweringInfo& info = mir_graph_->GetSFieldLoweringInfo(mir);
      if ((mir->optimization_flags & MIR_CLASS_IS_INITIALIZED) == 0 ||
          !info.IsResolved() || !info.FastPut()) {
        data.uses_all_vregs = true;
      }
      break;
    }

    default:
      LOG(FATAL) << "Unexpected opcode: " << opcode;
      UNREACHABLE();
      break;
  }
  DCHECK(data.must_keep || !data.uses_all_vregs) << opcode;
  DCHECK(data.must_keep || data.has_def) << opcode;
  if (kVerboseDebug) {
    if (!data.has_def) {
      LOG(INFO) << "In BB#" << lvn_->Id() << "@0x" << std::hex << mir->offset
          << " NO DEFS";
    }
  }
  mir_data_.push_back(data);
  if (data.uses_all_vregs) {
    past_last_uses_all_change_ = mir_data_.size();
  }
  return true;
}

}  // namespace art
