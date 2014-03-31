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

/* This file contains register alloction support. */

#include "dex/compiler_ir.h"
#include "dex/compiler_internals.h"
#include "mir_to_lir-inl.h"

namespace art {

/*
 * Free all allocated temps in the temp pools.  Note that this does
 * not affect the "liveness" of a temp register, which will stay
 * live until it is either explicitly killed or reallocated.
 */
void Mir2Lir::ResetRegPool() {
  GrowableArray<RegisterInfo*>::Iterator iter(&tempreg_info_);
  for (RegisterInfo* info = iter.Next(); info != NULL; info = iter.Next()) {
    info->in_use = false;
  }
  // Reset temp tracking sanity check.
  if (kIsDebugBuild) {
    live_sreg_ = INVALID_SREG;
  }
}

 /*
  * Set up temp & preserved register pools specialized by target.
  * Note: num_regs may be zero.
  */
void Mir2Lir::CompilerInitPool(RegisterInfo* regs, int* reg_nums, int num) {
  for (int i = 0; i < num; i++) {
    uint32_t reg_number = reg_nums[i];
    regs[i].reg = reg_number;
    regs[i].in_use = false;
    regs[i].is_temp = false;
    regs[i].wide_value = false;
    regs[i].live = false;
    regs[i].dirty = false;
    // TODO: use RegStorage for reg as well and change reg_nums to RegStorage[].
    // TODO: add SetAlias function.
    regs[i].alias = RegStorage::InvalidReg();
    regs[i].s_reg = INVALID_SREG;
    size_t map_size = reginfo_map_.Size();
    if (reg_number >= map_size) {
      for (uint32_t i = 0; i < ((reg_number - map_size) + 1); i++) {
        reginfo_map_.Insert(NULL);
      }
    }
    reginfo_map_.Put(reg_number, &regs[i]);
  }
}

void Mir2Lir::DumpRegPool(RegisterInfo* p, int num_regs) {
  LOG(INFO) << "================================================";
  for (int i = 0; i < num_regs; i++) {
    LOG(INFO) << StringPrintf(
        "R[%d]: T:%d, U:%d, W:%d, p:%d, LV:%d, D:%d, SR:%d",
        p[i].reg, p[i].is_temp, p[i].in_use, p[i].wide_value, p[i].partner,
        p[i].live, p[i].dirty, p[i].s_reg);
  }
  LOG(INFO) << "================================================";
}

void Mir2Lir::DumpCoreRegPool() {
  DumpRegPool(reg_pool_->core_regs, reg_pool_->num_core_regs);
}

void Mir2Lir::DumpFpRegPool() {
  DumpRegPool(reg_pool_->FPRegs, reg_pool_->num_fp_regs);
}

void Mir2Lir::DumpRegPools() {
  LOG(INFO) << "Core registers";
  DumpCoreRegPool();
  LOG(INFO) << "FP registers";
  DumpFpRegPool();
}

void Mir2Lir::Clobber(RegStorage reg) {
  if (reg.IsPair()) {
    ClobberBody(GetRegInfo(reg.GetLowReg()));
    ClobberBody(GetRegInfo(reg.GetHighReg()));
  } else {
    ClobberBody(GetRegInfo(reg.GetReg()));
  }
}

void Mir2Lir::ClobberSRegBody(RegisterInfo* p, int num_regs, int s_reg) {
  // TODO: distinguish between 32 and 64-bit usages of s_regs.
  for (int i = 0; i< num_regs; i++) {
    if (p[i].s_reg == s_reg) {
      // NOTE: a single s_reg to appear multiple times, so we can't short-circuit.
      if (p[i].is_temp) {
        p[i].live = false;
      }
      p[i].def_start = NULL;
      p[i].def_end = NULL;
    }
  }
}

/*
 * Break the association between a Dalvik vreg and a physical temp register of either register
 * class.
 * TODO: Ideally, the public version of this code should not exist.  Besides its local usage
 * in the register utilities, is is also used by code gen routines to work around a deficiency in
 * local register allocation, which fails to distinguish between the "in" and "out" identities
 * of Dalvik vregs.  This can result in useless register copies when the same Dalvik vreg
 * is used both as the source and destination register of an operation in which the type
 * changes (for example: INT_TO_FLOAT v1, v1).  Revisit when improved register allocation is
 * addressed.
 */
void Mir2Lir::ClobberSReg(int s_reg) {
  /* Reset live temp tracking sanity checker */
  if (kIsDebugBuild) {
    if (s_reg == live_sreg_) {
      live_sreg_ = INVALID_SREG;
    }
  }
  ClobberSRegBody(reg_pool_->core_regs, reg_pool_->num_core_regs, s_reg);
  ClobberSRegBody(reg_pool_->FPRegs, reg_pool_->num_fp_regs, s_reg);
}

/*
 * SSA names associated with the initial definitions of Dalvik
 * registers are the same as the Dalvik register number (and
 * thus take the same position in the promotion_map.  However,
 * the special Method* and compiler temp resisters use negative
 * v_reg numbers to distinguish them and can have an arbitrary
 * ssa name (above the last original Dalvik register).  This function
 * maps SSA names to positions in the promotion_map array.
 */
int Mir2Lir::SRegToPMap(int s_reg) {
  DCHECK_LT(s_reg, mir_graph_->GetNumSSARegs());
  DCHECK_GE(s_reg, 0);
  int v_reg = mir_graph_->SRegToVReg(s_reg);
  if (v_reg >= 0) {
    DCHECK_LT(v_reg, cu_->num_dalvik_registers);
    return v_reg;
  } else {
    /*
     * It must be the case that the v_reg for temporary is less than or equal to the
     * base reg for temps. For that reason, "position" must be zero or positive.
     */
    unsigned int position = std::abs(v_reg) - std::abs(static_cast<int>(kVRegTempBaseReg));

    // The temporaries are placed after dalvik registers in the promotion map
    DCHECK_LT(position, mir_graph_->GetNumUsedCompilerTemps());
    return cu_->num_dalvik_registers + position;
  }
}

void Mir2Lir::RecordCorePromotion(RegStorage reg, int s_reg) {
  int p_map_idx = SRegToPMap(s_reg);
  int v_reg = mir_graph_->SRegToVReg(s_reg);
  int reg_num = reg.GetReg();
  GetRegInfo(reg_num)->in_use = true;
  reg_num &= RegStorage::kRegNumMask;
  core_spill_mask_ |= (1 << reg_num);
  // Include reg for later sort
  core_vmap_table_.push_back(reg_num << VREG_NUM_WIDTH | (v_reg & ((1 << VREG_NUM_WIDTH) - 1)));
  num_core_spills_++;
  promotion_map_[p_map_idx].core_location = kLocPhysReg;
  promotion_map_[p_map_idx].core_reg = reg_num;
}

/* Reserve a callee-save register.  Return -1 if none available */
RegStorage Mir2Lir::AllocPreservedCoreReg(int s_reg) {
  RegStorage res;
  RegisterInfo* core_regs = reg_pool_->core_regs;
  for (int i = 0; i < reg_pool_->num_core_regs; i++) {
    if (!core_regs[i].is_temp && !core_regs[i].in_use) {
      res = RegStorage::Solo32(core_regs[i].reg);
      RecordCorePromotion(res, s_reg);
      break;
    }
  }
  return res;
}

void Mir2Lir::RecordFpPromotion(RegStorage reg, int s_reg) {
  int p_map_idx = SRegToPMap(s_reg);
  int v_reg = mir_graph_->SRegToVReg(s_reg);
  int reg_num = reg.GetReg();
  GetRegInfo(reg_num)->in_use = true;
  MarkPreservedSingle(v_reg, reg_num);
  promotion_map_[p_map_idx].fp_location = kLocPhysReg;
  promotion_map_[p_map_idx].FpReg = reg_num;
}

// Reserve a callee-save fp single register.
RegStorage Mir2Lir::AllocPreservedSingle(int s_reg) {
  RegStorage res;
  RegisterInfo* FPRegs = reg_pool_->FPRegs;
  for (int i = 0; i < reg_pool_->num_fp_regs; i++) {
    if (!FPRegs[i].is_temp && !FPRegs[i].in_use) {
      res = RegStorage::Solo32(FPRegs[i].reg);
      RecordFpPromotion(res, s_reg);
      break;
    }
  }
  return res;
}

RegStorage Mir2Lir::AllocTempBody(RegisterInfo* p, int num_regs, int* next_temp,
                                  bool required) {
  int next = *next_temp;
  for (int i = 0; i< num_regs; i++) {
    if (next >= num_regs)
      next = 0;
    if (p[next].is_temp && !p[next].in_use && !p[next].live) {
      Clobber(p[next].reg);
      p[next].in_use = true;
      p[next].wide_value = false;
      *next_temp = next + 1;
      return RegStorage::Solo32(p[next].reg);
    }
    next++;
  }
  next = *next_temp;
  for (int i = 0; i< num_regs; i++) {
    if (next >= num_regs)
      next = 0;
    if (p[next].is_temp && !p[next].in_use) {
      Clobber(p[next].reg);
      p[next].in_use = true;
      p[next].wide_value = false;
      *next_temp = next + 1;
      return RegStorage::Solo32(p[next].reg);
    }
    next++;
  }
  if (required) {
    CodegenDump();
    DumpRegPool(reg_pool_->core_regs,
          reg_pool_->num_core_regs);
    LOG(FATAL) << "No free temp registers";
  }
  return RegStorage::InvalidReg();  // No register available
}

/* Return a temp if one is available, -1 otherwise */
RegStorage Mir2Lir::AllocFreeTemp() {
  return AllocTempBody(reg_pool_->core_regs,
             reg_pool_->num_core_regs,
             &reg_pool_->next_core_reg, false);
}

RegStorage Mir2Lir::AllocTemp() {
  return AllocTempBody(reg_pool_->core_regs,
             reg_pool_->num_core_regs,
             &reg_pool_->next_core_reg, true);
}

RegStorage Mir2Lir::AllocTempFloat() {
  return AllocTempBody(reg_pool_->FPRegs,
             reg_pool_->num_fp_regs,
             &reg_pool_->next_fp_reg, true);
}

Mir2Lir::RegisterInfo* Mir2Lir::AllocLiveBody(RegisterInfo* p, int num_regs, int s_reg) {
  if (s_reg == -1)
    return NULL;
  for (int i = 0; i < num_regs; i++) {
    if ((p[i].s_reg == s_reg) && p[i].live) {
      if (p[i].is_temp)
        p[i].in_use = true;
      return &p[i];
    }
  }
  return NULL;
}

Mir2Lir::RegisterInfo* Mir2Lir::AllocLive(int s_reg, int reg_class) {
  RegisterInfo* res = NULL;
  switch (reg_class) {
    case kAnyReg:
      res = AllocLiveBody(reg_pool_->FPRegs,
                reg_pool_->num_fp_regs, s_reg);
      if (res)
        break;
      /* Intentional fallthrough */
    case kCoreReg:
      res = AllocLiveBody(reg_pool_->core_regs,
                reg_pool_->num_core_regs, s_reg);
      break;
    case kFPReg:
      res = AllocLiveBody(reg_pool_->FPRegs,
                reg_pool_->num_fp_regs, s_reg);
      break;
    default:
      LOG(FATAL) << "Invalid register type";
  }
  return res;
}

// Deprecate?  Just use the RegStorage version?
void Mir2Lir::FreeTemp(int reg) {
  RegisterInfo* p = GetRegInfo(reg);
  if (p->is_temp) {
    p->in_use = false;
    // TODO:  Should we also free partner if pair?
  }
  p->wide_value = false;
}

void Mir2Lir::FreeTemp(RegStorage reg) {
  if (reg.IsPair()) {
    FreeTemp(reg.GetLowReg());
    FreeTemp(reg.GetHighReg());
  } else {
    FreeTemp(reg.GetReg());
  }
}

bool Mir2Lir::IsLive(RegStorage reg) {
  bool res;
  if (reg.IsPair()) {
    RegisterInfo* p_lo = GetRegInfo(reg.GetLowReg());
    RegisterInfo* p_hi = GetRegInfo(reg.GetHighReg());
    res = p_lo->live || p_hi->live;
  } else {
    RegisterInfo* p = GetRegInfo(reg.GetReg());
    res = p->live;
  }
  return res;
}

bool Mir2Lir::IsTemp(RegStorage reg) {
  bool res;
  if (reg.IsPair()) {
    RegisterInfo* p_lo = GetRegInfo(reg.GetLowReg());
    RegisterInfo* p_hi = GetRegInfo(reg.GetHighReg());
    res = p_lo->is_temp || p_hi->is_temp;
  } else {
    RegisterInfo* p = GetRegInfo(reg.GetReg());
    res = p->is_temp;
  }
  return res;
}

bool Mir2Lir::IsPromoted(RegStorage reg) {
  bool res;
  if (reg.IsPair()) {
    RegisterInfo* p_lo = GetRegInfo(reg.GetLowReg());
    RegisterInfo* p_hi = GetRegInfo(reg.GetHighReg());
    res = !p_lo->is_temp || !p_hi->is_temp;
  } else {
    RegisterInfo* p = GetRegInfo(reg.GetReg());
    res = !p->is_temp;
  }
  return res;
}

bool Mir2Lir::IsDirty(RegStorage reg) {
  bool res;
  if (reg.IsPair()) {
    RegisterInfo* p_lo = GetRegInfo(reg.GetLowReg());
    RegisterInfo* p_hi = GetRegInfo(reg.GetHighReg());
    res = p_lo->dirty || p_hi->dirty;
  } else {
    RegisterInfo* p = GetRegInfo(reg.GetReg());
    res = p->dirty;
  }
  return res;
}

/*
 * Similar to AllocTemp(), but forces the allocation of a specific
 * register.  No check is made to see if the register was previously
 * allocated.  Use with caution.
 */
void Mir2Lir::LockTemp(RegStorage reg) {
  DCHECK(IsTemp(reg));
  if (reg.IsPair()) {
    RegisterInfo* p_lo = GetRegInfo(reg.GetLowReg());
    RegisterInfo* p_hi = GetRegInfo(reg.GetHighReg());
    p_lo->in_use = true;
    p_lo->live = false;
    p_hi->in_use = true;
    p_hi->live = false;
  } else {
    RegisterInfo* p = GetRegInfo(reg.GetReg());
    p->in_use = true;
    p->live = false;
  }
}

void Mir2Lir::ResetDef(RegStorage reg) {
  if (reg.IsPair()) {
    ResetDefBody(GetRegInfo(reg.GetLow()));
    ResetDefBody(GetRegInfo(reg.GetHigh()));
  } else {
    ResetDefBody(GetRegInfo(reg));
  }
}

void Mir2Lir::NullifyRange(RegStorage reg, int s_reg) {
  RegisterInfo* info = nullptr;
  RegStorage rs = reg.IsPair() ? reg.GetLow() : reg;
  if (IsTemp(rs)) {
    info = GetRegInfo(reg.GetReg());
  }
  if ((info != nullptr) && (info->def_start != nullptr) && (info->def_end != nullptr)) {
    DCHECK_EQ(info->s_reg, s_reg);  // Make sure we're on the same page.
    for (LIR* p = info->def_start;; p = p->next) {
      NopLIR(p);
      if (p == info->def_end) {
        break;
      }
    }
  }
}

/*
 * Mark the beginning and end LIR of a def sequence.  Note that
 * on entry start points to the LIR prior to the beginning of the
 * sequence.
 */
void Mir2Lir::MarkDef(RegLocation rl, LIR *start, LIR *finish) {
  DCHECK(!rl.wide);
  DCHECK(start && start->next);
  DCHECK(finish);
  RegisterInfo* p = GetRegInfo(rl.reg.GetReg());
  p->def_start = start->next;
  p->def_end = finish;
}

/*
 * Mark the beginning and end LIR of a def sequence.  Note that
 * on entry start points to the LIR prior to the beginning of the
 * sequence.
 */
void Mir2Lir::MarkDefWide(RegLocation rl, LIR *start, LIR *finish) {
  DCHECK(rl.wide);
  DCHECK(start && start->next);
  DCHECK(finish);
  RegisterInfo* p;
  if (rl.reg.IsPair()) {
    p = GetRegInfo(rl.reg.GetLow());
    ResetDef(rl.reg.GetHigh());  // Only track low of pair
  } else {
    p = GetRegInfo(rl.reg);
  }
  p->def_start = start->next;
  p->def_end = finish;
}

RegLocation Mir2Lir::WideToNarrow(RegLocation rl) {
  DCHECK(rl.wide);
  if (rl.location == kLocPhysReg) {
    if (rl.reg.IsPair()) {
      RegisterInfo* info_lo = GetRegInfo(rl.reg.GetLowReg());
      RegisterInfo* info_hi = GetRegInfo(rl.reg.GetHighReg());
      if (info_lo->is_temp) {
        info_lo->wide_value = false;
        info_lo->def_start = NULL;
        info_lo->def_end = NULL;
      }
      if (info_hi->is_temp) {
        info_hi->wide_value = false;
        info_hi->def_start = NULL;
        info_hi->def_end = NULL;
      }
      rl.reg = RegStorage::Solo32(rl.reg.GetLowReg());
    }
  }
  rl.wide = false;
  return rl;
}

void Mir2Lir::ResetDefLoc(RegLocation rl) {
  DCHECK(!rl.wide);
  if (IsTemp(rl.reg) && !(cu_->disable_opt & (1 << kSuppressLoads))) {
    NullifyRange(rl.reg, rl.s_reg_low);
  }
  ResetDef(rl.reg);
}

void Mir2Lir::ResetDefLocWide(RegLocation rl) {
  DCHECK(rl.wide);
  // If pair, only track low reg of pair.
  RegStorage rs = rl.reg.IsPair() ? rl.reg.GetLow() : rl.reg;
  if (IsTemp(rs) && !(cu_->disable_opt & (1 << kSuppressLoads))) {
    NullifyRange(rs, rl.s_reg_low);
  }
  ResetDef(rs);
}

void Mir2Lir::ResetDefTracking() {
  for (int i = 0; i< reg_pool_->num_core_regs; i++) {
    ResetDefBody(&reg_pool_->core_regs[i]);
  }
  for (int i = 0; i< reg_pool_->num_fp_regs; i++) {
    ResetDefBody(&reg_pool_->FPRegs[i]);
  }
}

void Mir2Lir::ClobberAllRegs() {
  GrowableArray<RegisterInfo*>::Iterator iter(&tempreg_info_);
  for (RegisterInfo* info = iter.Next(); info != NULL; info = iter.Next()) {
    info->live = false;
    info->s_reg = INVALID_SREG;
    info->def_start = NULL;
    info->def_end = NULL;
    info->wide_value = false;
  }
}

void Mir2Lir::FlushRegWide(RegStorage reg) {
  if (reg.IsPair()) {
    RegisterInfo* info1 = GetRegInfo(reg.GetLowReg());
    RegisterInfo* info2 = GetRegInfo(reg.GetHighReg());
    DCHECK(info1 && info2 && info1->wide_value && info2->wide_value &&
         (info1->partner == info2->reg) &&
         (info2->partner == info1->reg));
    if ((info1->live && info1->dirty) || (info2->live && info2->dirty)) {
      if (!(info1->is_temp && info2->is_temp)) {
        /* Should not happen.  If it does, there's a problem in eval_loc */
        LOG(FATAL) << "Long half-temp, half-promoted";
      }

      info1->dirty = false;
      info2->dirty = false;
      if (mir_graph_->SRegToVReg(info2->s_reg) < mir_graph_->SRegToVReg(info1->s_reg))
        info1 = info2;
      int v_reg = mir_graph_->SRegToVReg(info1->s_reg);
      StoreBaseDispWide(TargetReg(kSp), VRegOffset(v_reg), reg);
    }
  } else {
    RegisterInfo* info = GetRegInfo(reg.GetReg());
    if (info->live && info->dirty) {
      info->dirty = false;
      int v_reg = mir_graph_->SRegToVReg(info->s_reg);
      StoreBaseDispWide(TargetReg(kSp), VRegOffset(v_reg), reg);
    }
  }
}

void Mir2Lir::FlushReg(RegStorage reg) {
  DCHECK(!reg.IsPair());
  RegisterInfo* info = GetRegInfo(reg.GetReg());
  if (info->live && info->dirty) {
    info->dirty = false;
    int v_reg = mir_graph_->SRegToVReg(info->s_reg);
    StoreBaseDisp(TargetReg(kSp), VRegOffset(v_reg), reg, kWord);
  }
}

void Mir2Lir::FlushSpecificReg(RegisterInfo* info) {
  if (info->wide_value) {
    if (info->reg == info->partner) {
      FlushRegWide(RegStorage(RegStorage::k64BitSolo, info->reg));
    } else {
      FlushRegWide(RegStorage(RegStorage::k64BitPair, info->reg, info->partner));
    }
  } else {
    FlushReg(RegStorage::Solo32(info->reg));
  }
}

// Make sure nothing is live and dirty
void Mir2Lir::FlushAllRegsBody(RegisterInfo* info, int num_regs) {
  for (int i = 0; i < num_regs; i++) {
    if (info[i].live && info[i].dirty) {
      FlushSpecificReg(&info[i]);
    }
  }
}

void Mir2Lir::FlushAllRegs() {
  FlushAllRegsBody(reg_pool_->core_regs,
           reg_pool_->num_core_regs);
  FlushAllRegsBody(reg_pool_->FPRegs,
           reg_pool_->num_fp_regs);
  ClobberAllRegs();
}


// TUNING: rewrite all of this reg stuff.  Probably use an attribute table
bool Mir2Lir::RegClassMatches(int reg_class, RegStorage reg) {
  int reg_num = reg.IsPair() ? reg.GetLowReg() : reg.GetReg();
  if (reg_class == kAnyReg) {
    return true;
  } else if (reg_class == kCoreReg) {
    return !IsFpReg(reg_num);
  } else {
    return IsFpReg(reg_num);
  }
}

// TODO: should we have a RegLocation version of this?
// TODO: What should the behaviour up high sreg be?  Will temp reuse still work?
// FIXME: Need stated rules about liveness of wide Dalvik values stored in pair or 64BitSolo.
// WORKING RULE: only track the low sreg of a pair.
// TODO: Need to make sure AllocLive abides by this new rule.
void Mir2Lir::MarkLive(RegLocation loc) {
  RegStorage rs = loc.reg.IsPair() ? loc.reg.GetLow() : loc.reg;
  RegisterInfo* info = GetRegInfo(rs);
  if ((info->s_reg == loc.s_reg_low) && info->live) {
    return;  /* already live */
  } else if (loc.s_reg_low != INVALID_SREG) {
    ClobberSReg(loc.s_reg_low);
    if (info->is_temp) {
      info->live = true;
    }
  } else {
    /* Can't be live if no associated s_reg */
    DCHECK(info->is_temp);
    info->live = false;
  }
  info->s_reg = loc.s_reg_low;
}

void Mir2Lir::MarkTemp(int reg) {
  RegisterInfo* info = GetRegInfo(reg);
  tempreg_info_.Insert(info);
  info->is_temp = true;
}

void Mir2Lir::MarkTemp(RegStorage reg) {
  DCHECK(!reg.IsPair());
  RegisterInfo* info = GetRegInfo(reg);
  tempreg_info_.Insert(info);
  info->is_temp = true;
}

void Mir2Lir::UnmarkTemp(RegStorage reg) {
  DCHECK(!reg.IsPair());
  RegisterInfo* info = GetRegInfo(reg);
  tempreg_info_.Delete(info);
  info->is_temp = false;
}

void Mir2Lir::MarkWide(RegStorage reg) {
  if (reg.IsPair()) {
    RegisterInfo* info_lo = GetRegInfo(reg.GetLowReg());
    RegisterInfo* info_hi = GetRegInfo(reg.GetHighReg());
    info_lo->wide_value = info_hi->wide_value = true;
    info_lo->partner = reg.GetHighReg();
    info_hi->partner = reg.GetLowReg();
  } else {
    RegisterInfo* info = GetRegInfo(reg.GetReg());
    info->wide_value = true;
    info->partner = reg.GetReg();
  }
}

void Mir2Lir::MarkClean(RegLocation loc) {
  if (loc.reg.IsPair()) {
    RegisterInfo* info = GetRegInfo(loc.reg.GetLowReg());
    info->dirty = false;
    info = GetRegInfo(loc.reg.GetHighReg());
    info->dirty = false;
  } else {
    RegisterInfo* info = GetRegInfo(loc.reg.GetReg());
    info->dirty = false;
  }
}

// FIXME: need to verify rules/assumptions about how wide values are treated in 64BitSolos.
void Mir2Lir::MarkDirty(RegLocation loc) {
  if (loc.home) {
    // If already home, can't be dirty
    return;
  }
  if (loc.reg.IsPair()) {
    RegisterInfo* info = GetRegInfo(loc.reg.GetLowReg());
    info->dirty = true;
    info = GetRegInfo(loc.reg.GetHighReg());
    info->dirty = true;
  } else {
    RegisterInfo* info = GetRegInfo(loc.reg.GetReg());
    info->dirty = true;
  }
}

void Mir2Lir::MarkInUse(int reg) {
    RegisterInfo* info = GetRegInfo(reg);
    info->in_use = true;
}

void Mir2Lir::MarkInUse(RegStorage reg) {
  if (reg.IsPair()) {
    MarkInUse(reg.GetLowReg());
    MarkInUse(reg.GetHighReg());
  } else {
    MarkInUse(reg.GetReg());
  }
}

void Mir2Lir::CopyRegInfo(int new_reg, int old_reg) {
  RegisterInfo* new_info = GetRegInfo(new_reg);
  RegisterInfo* old_info = GetRegInfo(old_reg);
  // Target temp, live, dirty status must not change
  bool is_temp = new_info->is_temp;
  bool live = new_info->live;
  bool dirty = new_info->dirty;
  *new_info = *old_info;
  // Restore target's temp, live, dirty status
  new_info->is_temp = is_temp;
  new_info->live = live;
  new_info->dirty = dirty;
  new_info->reg = new_reg;
}

void Mir2Lir::CopyRegInfo(RegStorage new_reg, RegStorage old_reg) {
  DCHECK(!new_reg.IsPair());
  DCHECK(!old_reg.IsPair());
  CopyRegInfo(new_reg.GetReg(), old_reg.GetReg());
}

void Mir2Lir::CopyRegInfoWide(RegStorage new_reg, RegStorage old_reg) {
  bool new_pair = new_reg.IsPair();
  bool old_pair = old_reg.IsPair();
  if (new_pair == old_pair) {
    if (new_pair) {
      CopyRegInfo(new_reg.GetLowReg(), old_reg.GetLowReg());
      CopyRegInfo(new_reg.GetHighReg(), old_reg.GetHighReg());
    } else {
      CopyRegInfo(new_reg, old_reg);
    }
  } else if (new_pair) {
    // New is a pair, old is a solo.  Will need to fix up sreg of high.
    CopyRegInfo(new_reg.GetLowReg(), old_reg.GetReg());
    CopyRegInfo(new_reg.GetHighReg(), old_reg.GetReg());
    GetRegInfo(new_reg.GetHighReg())->s_reg = GetRegInfo(new_reg.GetLowReg())->s_reg + 1;
  } else {
    // New is a solo, old is a pair.
    CopyRegInfo(new_reg, old_reg.GetLow());
  }
}

bool Mir2Lir::CheckCorePoolSanity() {
  for (static int i = 0; i < reg_pool_->num_core_regs; i++) {
    if (reg_pool_->core_regs[i].wide_value) {
      int my_reg = reg_pool_->core_regs[i].reg;
      int my_sreg = reg_pool_->core_regs[i].s_reg;
      int partner_reg = reg_pool_->core_regs[i].partner;
      RegisterInfo* partner = GetRegInfo(partner_reg);
      DCHECK(partner != NULL);
      DCHECK(partner->wide_value);
      DCHECK_EQ(my_reg, partner->partner);
      int partner_sreg = partner->s_reg;
      if (my_sreg == INVALID_SREG) {
        DCHECK_EQ(partner_sreg, INVALID_SREG);
      } else {
        int diff = my_sreg - partner_sreg;
        DCHECK((diff == 0) || (diff == -1) || (diff == 1));
      }
    }
    if (!reg_pool_->core_regs[i].live) {
      DCHECK(reg_pool_->core_regs[i].def_start == NULL);
      DCHECK(reg_pool_->core_regs[i].def_end == NULL);
    }
  }
  return true;
}

/*
 * Return an updated location record with current in-register status.
 * If the value lives in live temps, reflect that fact.  No code
 * is generated.  If the live value is part of an older pair,
 * clobber both low and high.
 * TUNING: clobbering both is a bit heavy-handed, but the alternative
 * is a bit complex when dealing with FP regs.  Examine code to see
 * if it's worthwhile trying to be more clever here.
 */

RegLocation Mir2Lir::UpdateLoc(RegLocation loc) {
  DCHECK(!loc.wide);
  DCHECK(CheckCorePoolSanity());
  if (loc.location != kLocPhysReg) {
    DCHECK((loc.location == kLocDalvikFrame) ||
         (loc.location == kLocCompilerTemp));
    RegisterInfo* info_lo = AllocLive(loc.s_reg_low, kAnyReg);
    if (info_lo) {
      if (info_lo->wide_value) {
        Clobber(info_lo->reg);
        if (info_lo->reg != info_lo->partner) {
          Clobber(info_lo->partner);
        }
        FreeTemp(info_lo->reg);
      } else {
        // FIXME: either roll x86 to solo regs, or make construction target dependent.
        // Perhaps better to just use RegStorage at the lowest level?
        loc.reg = (cu_->instruction_set == kX86) ?
            RegStorage(RegStorage::k32BitVector, info_lo->reg) :
            RegStorage(RegStorage::k32BitSolo, info_lo->reg);
        loc.location = kLocPhysReg;
      }
    }
  }
  return loc;
}

// FIXME: Needs much rework.
/* see comments for update_loc */
RegLocation Mir2Lir::UpdateLocWide(RegLocation loc) {
  DCHECK(loc.wide);
  DCHECK(CheckCorePoolSanity());
  if (loc.location != kLocPhysReg) {
    DCHECK((loc.location == kLocDalvikFrame) ||
         (loc.location == kLocCompilerTemp));
    // Are the dalvik regs already live in physical registers?
    RegisterInfo* info_lo = AllocLive(loc.s_reg_low, kAnyReg);
    RegisterInfo* info_hi;
    bool register_pair = ((info_lo != nullptr) && (info_lo->wide_value) &&
                          (info_lo->reg != info_lo->partner));
    if (register_pair) {
      // Look for the allocation status of the high reg.
      info_hi = AllocLive(GetSRegHi(loc.s_reg_low), kAnyReg);
    } else {
      info_hi = info_lo;
    }
    bool match = true;
    match = match && (info_lo != NULL);
    match = match && (info_hi != NULL);
    // Are they both core or both FP?
    match = match && (IsFpReg(info_lo->reg) == IsFpReg(info_hi->reg));
    // If a pair of floating point singles, are they properly aligned?
    // TODO: eliminate this case once ARM and MIPS pair->double update complete.
    if (match && register_pair && IsFpReg(info_lo->reg)) {
      match &= ((info_lo->reg & 0x1) == 0);
      match &= ((info_hi->reg - info_lo->reg) == 1);
    }
    // If previously used as a pair, it is the same pair?
    if (match && register_pair) {
      match = (info_lo->wide_value == info_hi->wide_value);
      match &= ((info_lo->reg == info_hi->partner) &&
            (info_hi->reg == info_lo->partner));
    }
    if (match && !register_pair) {
      // If not a pair, low must be wide.
      match = info_lo->wide_value;
    }
    if (match) {
      // Can reuse - update the register usage info
      loc.location = kLocPhysReg;
      if (register_pair) {
        loc.reg = RegStorage(RegStorage::k64BitPair, info_lo->reg, info_hi->reg);
        DCHECK(!IsFpReg(loc.reg.GetLowReg()) || ((loc.reg.GetLowReg() & 0x1) == 0));
      } else {
        loc.reg = RegStorage(RegStorage::k64BitSolo, info_lo->reg);
      }
      MarkWide(loc.reg);
      return loc;
    }
    // Can't easily reuse - clobber and free any overlaps
    if (info_lo) {
      Clobber(info_lo->reg);
      FreeTemp(info_lo->reg);
      if (info_lo->wide_value && (info_lo->reg != info_lo->partner))
        Clobber(info_lo->partner);
    }
    if (info_hi && (info_hi != info_lo)) {
      Clobber(info_hi->reg);
      FreeTemp(info_hi->reg);
      if (info_hi->wide_value && (info_hi->reg != info_hi->partner)) {
        Clobber(info_hi->partner);
      }
    }
  }
  return loc;
}


/* For use in cases we don't know (or care) width */
RegLocation Mir2Lir::UpdateRawLoc(RegLocation loc) {
  if (loc.wide)
    return UpdateLocWide(loc);
  else
    return UpdateLoc(loc);
}

RegLocation Mir2Lir::EvalLocWide(RegLocation loc, int reg_class, bool update) {
  DCHECK(loc.wide);

  loc = UpdateLocWide(loc);

  /* If already in registers, we can assume proper form.  Right reg class? */
  if (loc.location == kLocPhysReg) {
    if (!RegClassMatches(reg_class, loc.reg)) {
      /* Wrong register class.  Reallocate and copy */
      RegStorage new_regs = AllocTypedTempWide(loc.fp, reg_class);
      OpRegCopyWide(new_regs, loc.reg);
      CopyRegInfoWide(new_regs, loc.reg);
      Clobber(loc.reg);
      loc.reg = new_regs;
      MarkWide(loc.reg);
    }
    return loc;
  }

  DCHECK_NE(loc.s_reg_low, INVALID_SREG);
  DCHECK_NE(GetSRegHi(loc.s_reg_low), INVALID_SREG);

  loc.reg = AllocTypedTempWide(loc.fp, reg_class);
  MarkWide(loc.reg);

  if (update) {
    loc.location = kLocPhysReg;
    MarkLive(loc);
  }
  return loc;
}

RegLocation Mir2Lir::EvalLoc(RegLocation loc, int reg_class, bool update) {
  if (loc.wide) {
    return EvalLocWide(loc, reg_class, update);
  }

  loc = UpdateLoc(loc);

  if (loc.location == kLocPhysReg) {
    if (!RegClassMatches(reg_class, loc.reg)) {
      /* Wrong register class.  Realloc, copy and transfer ownership */
      RegStorage new_reg = AllocTypedTemp(loc.fp, reg_class);
      OpRegCopy(new_reg, loc.reg);
      CopyRegInfo(new_reg, loc.reg);
      Clobber(loc.reg);
      loc.reg = new_reg;
    }
    return loc;
  }

  DCHECK_NE(loc.s_reg_low, INVALID_SREG);

  loc.reg = AllocTypedTemp(loc.fp, reg_class);

  if (update) {
    loc.location = kLocPhysReg;
    MarkLive(loc);
  }
  return loc;
}

/* USE SSA names to count references of base Dalvik v_regs. */
void Mir2Lir::CountRefs(RefCounts* core_counts, RefCounts* fp_counts, size_t num_regs) {
  for (int i = 0; i < mir_graph_->GetNumSSARegs(); i++) {
    RegLocation loc = mir_graph_->reg_location_[i];
    RefCounts* counts = loc.fp ? fp_counts : core_counts;
    int p_map_idx = SRegToPMap(loc.s_reg_low);
    if (loc.fp) {
      if (loc.wide) {
        // Treat doubles as a unit, using upper half of fp_counts array.
        counts[p_map_idx + num_regs].count += mir_graph_->GetUseCount(i);
        i++;
      } else {
        counts[p_map_idx].count += mir_graph_->GetUseCount(i);
      }
    } else if (!IsInexpensiveConstant(loc)) {
      counts[p_map_idx].count += mir_graph_->GetUseCount(i);
    }
  }
}

/* qsort callback function, sort descending */
static int SortCounts(const void *val1, const void *val2) {
  const Mir2Lir::RefCounts* op1 = reinterpret_cast<const Mir2Lir::RefCounts*>(val1);
  const Mir2Lir::RefCounts* op2 = reinterpret_cast<const Mir2Lir::RefCounts*>(val2);
  // Note that we fall back to sorting on reg so we get stable output
  // on differing qsort implementations (such as on host and target or
  // between local host and build servers).
  return (op1->count == op2->count)
          ? (op1->s_reg - op2->s_reg)
          : (op1->count < op2->count ? 1 : -1);
}

void Mir2Lir::DumpCounts(const RefCounts* arr, int size, const char* msg) {
  LOG(INFO) << msg;
  for (int i = 0; i < size; i++) {
    if ((arr[i].s_reg & STARTING_DOUBLE_SREG) != 0) {
      LOG(INFO) << "s_reg[D" << (arr[i].s_reg & ~STARTING_DOUBLE_SREG) << "]: " << arr[i].count;
    } else {
      LOG(INFO) << "s_reg[" << arr[i].s_reg << "]: " << arr[i].count;
    }
  }
}

/*
 * Note: some portions of this code required even if the kPromoteRegs
 * optimization is disabled.
 */
void Mir2Lir::DoPromotion() {
  int dalvik_regs = cu_->num_dalvik_registers;
  int num_regs = dalvik_regs + mir_graph_->GetNumUsedCompilerTemps();
  const int promotion_threshold = 1;
  // Allocate the promotion map - one entry for each Dalvik vReg or compiler temp
  promotion_map_ = static_cast<PromotionMap*>
      (arena_->Alloc(num_regs * sizeof(promotion_map_[0]), kArenaAllocRegAlloc));

  // Allow target code to add any special registers
  AdjustSpillMask();

  /*
   * Simple register promotion. Just do a static count of the uses
   * of Dalvik registers.  Note that we examine the SSA names, but
   * count based on original Dalvik register name.  Count refs
   * separately based on type in order to give allocation
   * preference to fp doubles - which must be allocated sequential
   * physical single fp registers starting with an even-numbered
   * reg.
   * TUNING: replace with linear scan once we have the ability
   * to describe register live ranges for GC.
   */
  RefCounts *core_regs =
      static_cast<RefCounts*>(arena_->Alloc(sizeof(RefCounts) * num_regs,
                                            kArenaAllocRegAlloc));
  RefCounts *FpRegs =
      static_cast<RefCounts *>(arena_->Alloc(sizeof(RefCounts) * num_regs * 2,
                                             kArenaAllocRegAlloc));
  // Set ssa names for original Dalvik registers
  for (int i = 0; i < dalvik_regs; i++) {
    core_regs[i].s_reg = FpRegs[i].s_reg = i;
  }

  // Set ssa names for compiler temporaries
  for (unsigned int ct_idx = 0; ct_idx < mir_graph_->GetNumUsedCompilerTemps(); ct_idx++) {
    CompilerTemp* ct = mir_graph_->GetCompilerTemp(ct_idx);
    core_regs[dalvik_regs + ct_idx].s_reg = ct->s_reg_low;
    FpRegs[dalvik_regs + ct_idx].s_reg = ct->s_reg_low;
    FpRegs[num_regs + dalvik_regs + ct_idx].s_reg = ct->s_reg_low;
  }

  // Duplicate in upper half to represent possible fp double starting sregs.
  for (int i = 0; i < num_regs; i++) {
    FpRegs[num_regs + i].s_reg = FpRegs[i].s_reg | STARTING_DOUBLE_SREG;
  }

  // Sum use counts of SSA regs by original Dalvik vreg.
  CountRefs(core_regs, FpRegs, num_regs);


  // Sort the count arrays
  qsort(core_regs, num_regs, sizeof(RefCounts), SortCounts);
  qsort(FpRegs, num_regs * 2, sizeof(RefCounts), SortCounts);

  if (cu_->verbose) {
    DumpCounts(core_regs, num_regs, "Core regs after sort");
    DumpCounts(FpRegs, num_regs * 2, "Fp regs after sort");
  }

  if (!(cu_->disable_opt & (1 << kPromoteRegs))) {
    // Promote FpRegs
    for (int i = 0; (i < (num_regs * 2)) && (FpRegs[i].count >= promotion_threshold); i++) {
      int p_map_idx = SRegToPMap(FpRegs[i].s_reg & ~STARTING_DOUBLE_SREG);
      if ((FpRegs[i].s_reg & STARTING_DOUBLE_SREG) != 0) {
        if ((promotion_map_[p_map_idx].fp_location != kLocPhysReg) &&
            (promotion_map_[p_map_idx + 1].fp_location != kLocPhysReg)) {
          int low_sreg = FpRegs[i].s_reg & ~STARTING_DOUBLE_SREG;
          // Ignore result - if can't alloc double may still be able to alloc singles.
          AllocPreservedDouble(low_sreg);
        }
      } else if (promotion_map_[p_map_idx].fp_location != kLocPhysReg) {
        RegStorage reg = AllocPreservedSingle(FpRegs[i].s_reg);
        if (!reg.Valid()) {
          break;  // No more left.
        }
      }
    }

    // Promote core regs
    for (int i = 0; (i < num_regs) &&
            (core_regs[i].count >= promotion_threshold); i++) {
      int p_map_idx = SRegToPMap(core_regs[i].s_reg);
      if (promotion_map_[p_map_idx].core_location !=
          kLocPhysReg) {
        RegStorage reg = AllocPreservedCoreReg(core_regs[i].s_reg);
        if (!reg.Valid()) {
           break;  // No more left
        }
      }
    }
  }

  // Now, update SSA names to new home locations
  for (int i = 0; i < mir_graph_->GetNumSSARegs(); i++) {
    RegLocation *curr = &mir_graph_->reg_location_[i];
    int p_map_idx = SRegToPMap(curr->s_reg_low);
    if (!curr->wide) {
      if (curr->fp) {
        if (promotion_map_[p_map_idx].fp_location == kLocPhysReg) {
          curr->location = kLocPhysReg;
          curr->reg = RegStorage::Solo32(promotion_map_[p_map_idx].FpReg);
          curr->home = true;
        }
      } else {
        if (promotion_map_[p_map_idx].core_location == kLocPhysReg) {
          curr->location = kLocPhysReg;
          curr->reg = RegStorage::Solo32(promotion_map_[p_map_idx].core_reg);
          curr->home = true;
        }
      }
    } else {
      if (curr->high_word) {
        continue;
      }
      if (curr->fp) {
        if ((promotion_map_[p_map_idx].fp_location == kLocPhysReg) &&
            (promotion_map_[p_map_idx+1].fp_location == kLocPhysReg)) {
          int low_reg = promotion_map_[p_map_idx].FpReg;
          int high_reg = promotion_map_[p_map_idx+1].FpReg;
          // Doubles require pair of singles starting at even reg
          // TODO: move target-specific restrictions out of here.
          if (((low_reg & 0x1) == 0) && ((low_reg + 1) == high_reg)) {
            curr->location = kLocPhysReg;
            if (cu_->instruction_set == kThumb2) {
              int dreg = ((low_reg & RegStorage::kRegNumMask) >> 1) | RegStorage::kFloat |
                  RegStorage::kDouble;
              curr->reg = RegStorage(RegStorage::k64BitSolo, dreg);
            } else {
              curr->reg = RegStorage(RegStorage::k64BitPair, low_reg, high_reg);
            }
            curr->home = true;
          }
        }
      } else {
        if ((promotion_map_[p_map_idx].core_location == kLocPhysReg)
           && (promotion_map_[p_map_idx+1].core_location ==
           kLocPhysReg)) {
          curr->location = kLocPhysReg;
          curr->reg = RegStorage(RegStorage::k64BitPair, promotion_map_[p_map_idx].core_reg,
                                 promotion_map_[p_map_idx+1].core_reg);
          curr->home = true;
        }
      }
    }
  }
  if (cu_->verbose) {
    DumpPromotionMap();
  }
}

/* Returns sp-relative offset in bytes for a VReg */
int Mir2Lir::VRegOffset(int v_reg) {
  return StackVisitor::GetVRegOffset(cu_->code_item, core_spill_mask_,
                                     fp_spill_mask_, frame_size_, v_reg);
}

/* Returns sp-relative offset in bytes for a SReg */
int Mir2Lir::SRegOffset(int s_reg) {
  return VRegOffset(mir_graph_->SRegToVReg(s_reg));
}

/* Mark register usage state and return long retloc */
RegLocation Mir2Lir::GetReturnWide(bool is_double) {
  RegLocation gpr_res = LocCReturnWide();
  RegLocation fpr_res = LocCReturnDouble();
  RegLocation res = is_double ? fpr_res : gpr_res;
  if (res.reg.IsPair()) {
    Clobber(res.reg);
    LockTemp(res.reg);
    // Does this wide value live in two registers or one vector register?
    if (res.reg.GetLowReg() != res.reg.GetHighReg()) {
      // FIXME: I think we want to mark these as wide as well.
      MarkWide(res.reg);
    }
  } else {
    Clobber(res.reg);
    LockTemp(res.reg);
    MarkWide(res.reg);
  }
  return res;
}

RegLocation Mir2Lir::GetReturn(bool is_float) {
  RegLocation gpr_res = LocCReturn();
  RegLocation fpr_res = LocCReturnFloat();
  RegLocation res = is_float ? fpr_res : gpr_res;
  Clobber(res.reg);
  if (cu_->instruction_set == kMips) {
    MarkInUse(res.reg);
  } else {
    LockTemp(res.reg);
  }
  return res;
}

void Mir2Lir::SimpleRegAlloc() {
  DoPromotion();

  if (cu_->verbose && !(cu_->disable_opt & (1 << kPromoteRegs))) {
    LOG(INFO) << "After Promotion";
    mir_graph_->DumpRegLocTable(mir_graph_->reg_location_, mir_graph_->GetNumSSARegs());
  }

  /* Set the frame size */
  frame_size_ = ComputeFrameSize();
}

/*
 * Get the "real" sreg number associated with an s_reg slot.  In general,
 * s_reg values passed through codegen are the SSA names created by
 * dataflow analysis and refer to slot numbers in the mir_graph_->reg_location
 * array.  However, renaming is accomplished by simply replacing RegLocation
 * entries in the reglocation[] array.  Therefore, when location
 * records for operands are first created, we need to ask the locRecord
 * identified by the dataflow pass what it's new name is.
 */
int Mir2Lir::GetSRegHi(int lowSreg) {
  return (lowSreg == INVALID_SREG) ? INVALID_SREG : lowSreg + 1;
}

bool Mir2Lir::LiveOut(int s_reg) {
  // For now.
  return true;
}

}  // namespace art
