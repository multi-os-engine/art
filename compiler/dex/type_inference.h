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

#ifndef ART_COMPILER_DEX_TYPE_INFERENCE_H_
#define ART_COMPILER_DEX_TYPE_INFERENCE_H_

#include "base/logging.h"
#include "base/arena_object.h"
#include "base/scoped_arena_containers.h"

namespace art {

class ArenaBitVector;
class BasicBlock;
struct CompilationUnit;
class DexFile;
class MirFieldInfo;
class MirMethodInfo;
class MIR;
class MIRGraph;

/**
 * @brief Determine the type of SSA registers.
 *
 * @details
 * Because Dalvik's bytecode is not fully typed, we have to do some work to figure
 * out the sreg type.  For some operations it is clear based on the opcode (i.e.
 * ADD_FLOAT v0, v1, v2), but for others (MOVE), we may never know the "real" type.
 *
 * We perform the type inference operation in three phases:
 *   1. First, we find the definitions that have an easily identifiable type, namely
 *      method parameters (using the type method signature), iget/sget destinations
 *      (using the field type), invoke return values (using the target method signature),
 *      new-instance, new-array and filled-new-array/-range (using the embedded type),
 *      const-class, const-string/-jumbo, check-cast and move_exception.
 *   2. Then we iteratively propagate the type of already known references "downstream",
 *      i.e. only from sources to destinations but recalculating for loops. In this
 *      phase, we basically process Phi, move-object and aget-object.
 *   3. Finally, we iteratively propagate the basic FP/core/reference type (but not
 *      the precise reference type) in both directions, i.e. from inputs to outputs
 *      but also from outputs to inputs, and using the requirements of typed dex
 *      instructions, for example ADD_FLOAT inputs and outputs are marked as FP.
 *      That means we also propagate the type of invoke input arguments to input sregs,
 *      the type of the referenced fields/element to the sreg stored with iput/sput/aput,
 *      move destination type to the move input sreg and the return type of the method
 *      to the sreg it returns.
 *
 * Since the main purpose is to determine the basic FP/core/reference type, we don't
 * need to record the precise reference type, we only record the array type to determine
 * the result types of agets and source type of aputs.
 *
 * One complication is the check-cast instruction that effectively defines a new
 * virtual register that has a different type than the original sreg. We need to
 * track these virtual sregs and insert pseudo-phis where they merge.
 *
 * Note that it's possible to have the same sreg show multiple defined types because
 * dx treats constants as untyped bit patterns.
 */
class TypeInference : public DeletableArenaObject<kArenaAllocMisc> {
 public:
  TypeInference(MIRGraph* mir_graph, ScopedArenaAllocator* alloc);

  bool Apply(BasicBlock* bb);
  void Finish();

 private:
  struct Type {
    static Type Uninitialized() {
      return Type(0u);
    }

    static Type NonArrayRefType() {
      return Type(kFlagLowWord | kFlagNarrow | kFlagRef);
    }

    static Type ShortyType(char shorty);
    static Type DexType(const DexFile* dex_file, uint32_t type_idx);

    static Type ArrayType(uint32_t array_depth, Type nested_type) {
      DCHECK_NE(array_depth, 0u);
      return Type(kFlagNarrow | kFlagRef | kFlagLowWord | (array_depth << kBitArrayDepthStart) |
                  ((nested_type.raw_bits_ & kMaskWideAndType) << kArrayTypeShift));
    }

    bool IsDefined() {
      return raw_bits_ != 0u;
    }

    bool SizeConflict() const {
      // NOTE: Ignore array element conflicts that don't propagate to direct conflicts.
      return (Wide() && Narrow()) || (HighWord() && LowWord());
    }

    bool TypeConflict() const {
      // NOTE: Ignore array element conflicts that don't propagate to direct conflicts.
      return (raw_bits_ & kMaskType) != 0u && !IsPowerOfTwo(raw_bits_ & kMaskType);  // 2+ bits.
    }

    void CheckUnassigned() const {
      DCHECK_EQ(raw_bits_, 0u);
    }

    void CheckPureRef() const {
      DCHECK_EQ(raw_bits_ & (kMaskWideAndType | kMaskWord), kFlagNarrow | kFlagRef | kFlagLowWord);
    }

    void CheckUnassignedOrPureRef() {
      if (Ref()) {
        CheckPureRef();
      } else {
        CheckUnassigned();
      }
    }

    bool Wide() const {
      return IsBitSet(kFlagWide);
    }

    bool Narrow() const {
      return IsBitSet(kFlagNarrow);
    }

    bool Fp() const {
      return IsBitSet(kFlagFp);
    }

    bool Core() const {
      return IsBitSet(kFlagCore);
    }

    bool Ref() const {
      return IsBitSet(kFlagRef);
    }

    bool LowWord() const {
      return IsBitSet(kFlagLowWord);
    }

    bool HighWord() const {
      return IsBitSet(kFlagHighWord);
    }

    uint32_t ArrayDepth() const {
      return raw_bits_ >> kBitArrayDepthStart;
    }

    Type NestedType() const {
      DCHECK_NE(ArrayDepth(), 0u);
      return Type(kFlagLowWord | ((raw_bits_ & kMaskArrayWideAndType) >> kArrayTypeShift));
    }

    Type ComponentType() const {
      DCHECK_NE(ArrayDepth(), 0u);
      Type temp(raw_bits_ - (1u << kBitArrayDepthStart));  // array_depth - 1u;
      return (temp.ArrayDepth() != 0u) ? temp : NestedType();
    }

    bool SetWide() {
      return SetBit(kFlagWide);
    }

    bool SetNarrow() {
      return SetBit(kFlagNarrow);
    }

    bool SetFp() {
      return SetBit(kFlagFp);
    }

    bool SetCore() {
      return SetBit(kFlagCore);
    }

    bool SetRef() {
      return SetBit(kFlagRef);
    }

    bool SetLowWord() {
      return SetBit(kFlagLowWord);
    }

    bool SetHighWord() {
      return SetBit(kFlagHighWord);
    }

    Type ToHighWord() const {
      DCHECK_EQ(raw_bits_ & (kMaskWide | kMaskWord), kFlagWide | kFlagLowWord);
      return Type(raw_bits_ ^ (kFlagLowWord | kFlagHighWord));
    }

    bool EqualArrayRef(Type ref_type) {
      CheckPureRef();
      ref_type.CheckPureRef();
      return ((raw_bits_ ^ ref_type.raw_bits_) & (kMaskArrayWideAndType | kMaskArrayDepth)) == 0;
    }

    bool MergePureRefAndArray(Type ref_type) {
      ref_type.CheckPureRef();
      if (!Ref()) {
        CheckUnassigned();
        *this = ref_type;  // Full copy.
        return true;
      }
      CheckPureRef();
      if (EqualArrayRef(ref_type)) {
        return false;
      }
      // Conflicting array types. Implicitly converted to Object reference.
      *this = NonArrayRefType();
      return true;
    }

    bool MergeHighWord(Type low_word_type) {
      // NOTE: low_word_type may be also Narrow() or HighWord().
      DCHECK(low_word_type.Wide() && low_word_type.LowWord());
      bool changed = MergeBits(low_word_type, kMaskWideAndType);
      changed |= SetHighWord();
      return changed;
    }

    bool Copy(Type type) {
      if (raw_bits_ != type.raw_bits_) {
        raw_bits_ = type.raw_bits_;
        return true;
      }
      return false;
    }

    // Merge non-array flags.
    bool MergeNonArrayFlags(Type src_type) {
      return MergeBits(src_type, ~(kMaskArrayWideAndType | kMaskArrayDepth));
    }

   private:
    enum FlagBits {
      kBitWide = 0,
      kBitNarrow,
      kBitFp,
      kBitCore,
      kBitRef,
      kBitLowWord,
      kBitHighWord,
      kBitArrayWide,
      kBitArrayNarrow,
      kBitArrayFp,
      kBitArrayCore,
      kBitArrayRef,
      kBitArrayDepthStart,
    };
    static constexpr size_t kArrayDepthBits = sizeof(uint32_t) * 8u - kBitArrayDepthStart;

    static constexpr uint32_t kFlagWide = 1u << kBitWide;
    static constexpr uint32_t kFlagNarrow = 1u << kBitNarrow;
    static constexpr uint32_t kFlagFp = 1u << kBitFp;
    static constexpr uint32_t kFlagCore = 1u << kBitCore;
    static constexpr uint32_t kFlagRef = 1u << kBitRef;
    static constexpr uint32_t kFlagLowWord = 1u << kBitLowWord;
    static constexpr uint32_t kFlagHighWord = 1u << kBitHighWord;
    static constexpr uint32_t kFlagArrayWide = 1u << kBitArrayWide;
    static constexpr uint32_t kFlagArrayNarrow = 1u << kBitArrayNarrow;
    static constexpr uint32_t kFlagArrayFp = 1u << kBitArrayFp;
    static constexpr uint32_t kFlagArrayCore = 1u << kBitArrayCore;
    static constexpr uint32_t kFlagArrayRef = 1u << kBitArrayRef;

    static constexpr uint32_t kMaskWide = kFlagWide | kFlagNarrow;
    static constexpr uint32_t kMaskType = kFlagFp | kFlagCore | kFlagRef;
    static constexpr uint32_t kMaskWord = kFlagLowWord | kFlagHighWord;
    static constexpr uint32_t kMaskArrayWide = kFlagArrayWide | kFlagArrayNarrow;
    static constexpr uint32_t kMaskArrayType = kFlagArrayFp | kFlagArrayCore | kFlagArrayRef;
    static constexpr uint32_t kMaskWideAndType = kMaskWide | kMaskType;
    static constexpr uint32_t kMaskArrayWideAndType = kMaskArrayWide | kMaskArrayType;

    static constexpr size_t kArrayTypeShift = kBitArrayWide - kBitWide;
    static constexpr uint32_t kMaskArrayDepth = static_cast<uint32_t>(-1) << kArrayTypeShift;
    static_assert(kArrayTypeShift == kBitArrayNarrow - kBitNarrow, "shift mismatch");
    static_assert(kArrayTypeShift == kBitArrayFp - kBitFp, "shift mismatch");
    static_assert(kArrayTypeShift == kBitArrayCore - kBitCore, "shift mismatch");
    static_assert(kArrayTypeShift == kBitArrayRef - kBitRef, "shift mismatch");
    static_assert((kMaskWide << kArrayTypeShift) == kMaskArrayWide, "shift mismatch");
    static_assert((kMaskType << kArrayTypeShift) == kMaskArrayType, "shift mismatch");
    static_assert((kMaskWideAndType << kArrayTypeShift) == kMaskArrayWideAndType, "shift mismatch");

    explicit Type(uint32_t raw_bits) : raw_bits_(raw_bits) { }

    bool IsBitSet(uint32_t flag) const {
      return (raw_bits_ & flag) != 0u;
    }

    bool SetBit(uint32_t flag) {
      if ((raw_bits_ & flag) == 0u) {
        raw_bits_ |= flag;
        return true;
      }
      return false;
    }

    bool MergeBits(Type src_type, uint32_t mask) {
      uint32_t new_bits = raw_bits_ | (src_type.raw_bits_ & mask);
      if (new_bits != raw_bits_) {
        raw_bits_ = new_bits;
        return true;
      }
      return false;
    }

    uint32_t raw_bits_;
  };

  struct MethodSignature {
    Type return_type;
    size_t num_params;
    Type* param_types;
  };

  struct SplitSRegData {
    int32_t current_mod_s_reg;
    int32_t* starting_mod_s_reg;        // Indexed by BasicBlock::id.
    int32_t* ending_mod_s_reg;          // Indexed by BasicBlock::id.

    // NOTE: Before AddPseudoPhis(), def_phi_blocks_ marks the blocks
    // with check-casts and the block with the original SSA reg.
    // After AddPseudoPhis(), it marks blocks with pseudo-phis.
    ArenaBitVector* def_phi_blocks_;    // Indexed by BasicBlock::id.
  };

  class CheckCastData : public DeletableArenaObject<kArenaAllocMisc> {
   public:
    CheckCastData(MIRGraph* mir_graph, ScopedArenaAllocator* alloc);

    size_t NumSRegs() const {
      return num_sregs_;
    }

    void AddCheckCast(MIR* check_cast, Type type);
    void AddPseudoPhis();
    void InitializeCheckCastSRegs(Type* sregs) const;
    void MergeCheckCastConflicts(Type* sregs) const;

    void Start(BasicBlock* bb);
    bool ProcessPseudoPhis(BasicBlock* bb, Type* sregs);
    void ProcessCheckCast(MIR* mir);

    SplitSRegData* GetSplitSRegData(int32_t s_reg);

   private:
    BasicBlock* FindDefBlock(MIR* check_cast);
    BasicBlock* FindTopologicallyEarliestPredecessor(BasicBlock* bb);
    bool IsSRegLiveAtStart(BasicBlock* bb, int v_reg, int32_t s_reg);

    MIRGraph* const mir_graph_;
    ScopedArenaAllocator* const alloc_;
    const size_t num_blocks_;
    size_t num_sregs_;

    // Map check-cast mir to special sreg and type.
    ScopedArenaSafeMap<MIR*, std::pair<int32_t, Type>> check_cast_map_;
    ScopedArenaSafeMap<int32_t, SplitSRegData> split_sreg_data_;
  };

  static Type FieldType(const DexFile* dex_file, uint32_t field_idx);
  static Type* PrepareIFieldTypes(const DexFile* dex_file, MIRGraph* mir_graph,
                                  ScopedArenaAllocator* alloc);
  static Type* PrepareSFieldTypes(const DexFile* dex_file, MIRGraph* mir_graph,
                                  ScopedArenaAllocator* alloc);
  static MethodSignature Signature(const DexFile* dex_file, uint32_t method_idx, bool is_static,
                                   ScopedArenaAllocator* alloc);
  static MethodSignature* PrepareSignatures(const DexFile* dex_file, MIRGraph* mir_graph,
                                            ScopedArenaAllocator* alloc);
  static CheckCastData* InitializeCheckCastData(MIRGraph* mir_graph, ScopedArenaAllocator* alloc);

  void InitializeSRegs();
  void PropagateRefs();
  bool PropagateRefs(BasicBlock* bb);

  int32_t ModifiedSReg(int32_t s_reg);
  int32_t PhiInputModifiedSReg(int32_t s_reg, BasicBlock* bb, size_t pred_idx);

  bool UpdateSRegFromLowWordType(int32_t mod_s_reg, Type low_word_type);
  bool InferTypeAndSize(BasicBlock* bb, MIR* mir);

  MIRGraph* const mir_graph_;
  CompilationUnit* const cu_;

  // The type inference propagates types also backwards but this must not happen across
  // check-cast. So we need to effectively split an SSA reg into two at check-cast and
  // keep track of the types separately.
  std::unique_ptr<CheckCastData> check_cast_data_;

  size_t num_sregs_;      // Number of SSA regs or modified SSA regs, see check-cast.
  const Type* const ifields_;                 // Indexed by MIR::meta::ifield_lowering_info.
  const Type* const sfields_;                 // Indexed by MIR::meta::sfield_lowering_info.
  const MethodSignature* const signatures_;   // Indexed by MIR::meta::method_lowering_info.
  const MethodSignature current_method_signature_;
  Type* const sregs_;     // Indexed by SSA reg or modified SSA reg, see check-cast.
};

}  // namespace art

#endif  // ART_COMPILER_DEX_TYPE_INFERENCE_H_
