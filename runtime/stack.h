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

#ifndef ART_RUNTIME_STACK_H_
#define ART_RUNTIME_STACK_H_

#include <stdint.h>
#include <string>

#include "dex_file.h"
#include "instruction_set.h"
#include "mirror/object_reference.h"
#include "throw_location.h"
#include "utils.h"
#include "verify_object.h"

namespace art {

namespace mirror {
  class ArtMethod;
  class Object;
}  // namespace mirror

class Context;
class ShadowFrame;
class HandleScope;
class ScopedObjectAccess;
class Thread;

// The kind of vreg being accessed in calls to Set/GetVReg.
enum VRegKind {
  kReferenceVReg,
  kIntVReg,
  kFloatVReg,
  kLongLoVReg,
  kLongHiVReg,
  kDoubleLoVReg,
  kDoubleHiVReg,
  kConstant,
  kImpreciseConstant,
  kUndefined,
};

// A reference from the shadow stack to a MirrorType object within the Java heap.
template<class MirrorType>
class MANAGED StackReference : public mirror::ObjectReference<false, MirrorType> {
 public:
  StackReference<MirrorType>() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : mirror::ObjectReference<false, MirrorType>(nullptr) {}

  static StackReference<MirrorType> FromMirrorPtr(MirrorType* p)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return StackReference<MirrorType>(p);
  }

 private:
  StackReference<MirrorType>(MirrorType* p) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : mirror::ObjectReference<false, MirrorType>(p) {}
};

// Super class of our stack frame representations.
class ManagedFrame {
 public:
  virtual bool IsShadowFrame() const { return false; }
  virtual bool IsQuickFrame() const { return false; }

  virtual mirror::ArtMethod* GetMethod() const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) = 0;

  virtual uint32_t GetDexPc(bool abort_on_failure = true) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) = 0;

  virtual bool GetVReg(uint16_t vreg, VRegKind kind, uint32_t* val) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) = 0;

  virtual bool SetVReg(uint16_t vreg, uint32_t new_value, VRegKind kind)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) = 0;

  virtual bool GetVRegPair(uint16_t vreg, VRegKind kind_lo, VRegKind kind_hi, uint64_t* val) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) = 0;

  virtual bool SetVRegPair(uint16_t vreg, uint64_t new_value, VRegKind kind_lo, VRegKind kind_hi)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) = 0;

  virtual void SanityCheckFrame() const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) = 0;

  virtual mirror::Object* GetThisObject() const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) = 0;

 protected:
  virtual ~ManagedFrame() {}
};

// ShadowFrame has 3 possible layouts:
//  - portable - a unified array of VRegs and references. Precise references need GC maps.
//  - interpreter - separate VRegs and reference arrays. References are in the reference array.
//  - JNI - just VRegs, but where every VReg holds a reference.
class ShadowFrame : public ManagedFrame {
 public:
  virtual bool IsShadowFrame() const OVERRIDE { return true; }
  virtual void SanityCheckFrame() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) OVERRIDE {}

  // Compute size of ShadowFrame in bytes assuming it has a reference array.
  static size_t ComputeSize(uint32_t num_vregs) {
    return sizeof(ShadowFrame) + (sizeof(uint32_t) * num_vregs) +
           (sizeof(StackReference<mirror::Object>) * num_vregs);
  }

  // Create ShadowFrame in heap for deoptimization.
  static ShadowFrame* Create(uint32_t num_vregs, ShadowFrame* link,
                             mirror::ArtMethod* method, uint32_t dex_pc) {
    uint8_t* memory = new uint8_t[ComputeSize(num_vregs)];
    return Create(num_vregs, link, method, dex_pc, memory);
  }

  // Create ShadowFrame for interpreter using provided memory.
  static ShadowFrame* Create(uint32_t num_vregs, ShadowFrame* link,
                             mirror::ArtMethod* method, uint32_t dex_pc, void* memory) {
    ShadowFrame* sf = new (memory) ShadowFrame(num_vregs, link, method, dex_pc, true);
    return sf;
  }

  virtual ~ShadowFrame() {}

  bool HasReferenceArray() const {
    return kUsePortableCompiler
        ? (number_of_vregs_ & kHasReferenceArray) != 0
        : true;
  }

  uint32_t NumberOfVRegs() const {
    return kUsePortableCompiler
        ? number_of_vregs_ & ~kHasReferenceArray
        : number_of_vregs_;
  }

  void SetNumberOfVRegs(uint32_t number_of_vregs) {
    if (kUsePortableCompiler) {
      number_of_vregs_ = number_of_vregs | (number_of_vregs_ & kHasReferenceArray);
    } else {
      UNUSED(number_of_vregs);
      UNIMPLEMENTED(FATAL) << "Should only be called when portable is enabled";
    }
  }

  uint32_t GetDexPc(bool abort_on_failure = true) const {
    return dex_pc_;
  }

  void SetDexPc(uint32_t dex_pc) {
    dex_pc_ = dex_pc;
  }

  ShadowFrame* GetLink() const {
    return link_;
  }

  void SetLink(ShadowFrame* frame) {
    DCHECK_NE(this, frame);
    link_ = frame;
  }

  virtual bool GetVReg(uint16_t i, VRegKind unused, uint32_t* value) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) OVERRIDE {
    *value = GetVReg(i);
    return true;
  }

  virtual bool SetVReg(uint16_t i, uint32_t new_value, VRegKind unused)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) OVERRIDE {
    SetVReg(i, new_value);
    return true;
  }

  virtual bool GetVRegPair(uint16_t i, VRegKind low, VRegKind high, uint64_t* value) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) OVERRIDE {
    *value = GetVRegLong(i);
    return true;
  }

  virtual bool SetVRegPair(uint16_t i, uint64_t new_value, VRegKind lo, VRegKind high)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) OVERRIDE {
    SetVRegLong(i, new_value);
    return true;
  }

  int32_t GetVReg(uint16_t i) const {
    DCHECK_LT(i, NumberOfVRegs());
    const uint32_t* vreg = &vregs_[i];
    return *reinterpret_cast<const int32_t*>(vreg);
  }

  float GetVRegFloat(size_t i) const {
    DCHECK_LT(i, NumberOfVRegs());
    // NOTE: Strict-aliasing?
    const uint32_t* vreg = &vregs_[i];
    return *reinterpret_cast<const float*>(vreg);
  }

  int64_t GetVRegLong(size_t i) const {
    DCHECK_LT(i, NumberOfVRegs());
    const uint32_t* vreg = &vregs_[i];
    // Alignment attribute required for GCC 4.8
    typedef const int64_t unaligned_int64 __attribute__ ((aligned (4)));
    return *reinterpret_cast<unaligned_int64*>(vreg);
  }

  double GetVRegDouble(size_t i) const {
    DCHECK_LT(i, NumberOfVRegs());
    const uint32_t* vreg = &vregs_[i];
    // Alignment attribute required for GCC 4.8
    typedef const double unaligned_double __attribute__ ((aligned (4)));
    return *reinterpret_cast<unaligned_double*>(vreg);
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  mirror::Object* GetVRegReference(size_t i) const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    DCHECK_LT(i, NumberOfVRegs());
    mirror::Object* ref;
    if (HasReferenceArray()) {
      ref = References()[i].AsMirrorPtr();
    } else {
      const uint32_t* vreg_ptr = &vregs_[i];
      ref = reinterpret_cast<const StackReference<mirror::Object>*>(vreg_ptr)->AsMirrorPtr();
    }
    if (kVerifyFlags & kVerifyReads) {
      VerifyObject(ref);
    }
    return ref;
  }

  // Get view of vregs as range of consecutive arguments starting at i.
  uint32_t* GetVRegArgs(size_t i) {
    return &vregs_[i];
  }

  void SetVReg(size_t i, int32_t val) {
    DCHECK_LT(i, NumberOfVRegs());
    uint32_t* vreg = &vregs_[i];
    *reinterpret_cast<int32_t*>(vreg) = val;
    // This is needed for moving collectors since these can update the vreg references if they
    // happen to agree with references in the reference array.
    if (kMovingCollector && HasReferenceArray()) {
      References()[i].Clear();
    }
  }

  void SetVRegFloat(size_t i, float val) {
    DCHECK_LT(i, NumberOfVRegs());
    uint32_t* vreg = &vregs_[i];
    *reinterpret_cast<float*>(vreg) = val;
    // This is needed for moving collectors since these can update the vreg references if they
    // happen to agree with references in the reference array.
    if (kMovingCollector && HasReferenceArray()) {
      References()[i].Clear();
    }
  }

  void SetVRegLong(size_t i, int64_t val) {
    DCHECK_LT(i, NumberOfVRegs());
    uint32_t* vreg = &vregs_[i];
    // Alignment attribute required for GCC 4.8
    typedef int64_t unaligned_int64 __attribute__ ((aligned (4)));
    *reinterpret_cast<unaligned_int64*>(vreg) = val;
    // This is needed for moving collectors since these can update the vreg references if they
    // happen to agree with references in the reference array.
    if (kMovingCollector && HasReferenceArray()) {
      References()[i].Clear();
      References()[i + 1].Clear();
    }
  }

  void SetVRegDouble(size_t i, double val) {
    DCHECK_LT(i, NumberOfVRegs());
    uint32_t* vreg = &vregs_[i];
    // Alignment attribute required for GCC 4.8
    typedef double unaligned_double __attribute__ ((aligned (4)));
    *reinterpret_cast<unaligned_double*>(vreg) = val;
    // This is needed for moving collectors since these can update the vreg references if they
    // happen to agree with references in the reference array.
    if (kMovingCollector && HasReferenceArray()) {
      References()[i].Clear();
      References()[i + 1].Clear();
    }
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  void SetVRegReference(size_t i, mirror::Object* val) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    DCHECK_LT(i, NumberOfVRegs());
    if (kVerifyFlags & kVerifyWrites) {
      VerifyObject(val);
    }
    uint32_t* vreg = &vregs_[i];
    reinterpret_cast<StackReference<mirror::Object>*>(vreg)->Assign(val);
    if (HasReferenceArray()) {
      References()[i].Assign(val);
    }
  }

  virtual mirror::ArtMethod* GetMethod() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) OVERRIDE {
    DCHECK(method_ != nullptr);
    return method_;
  }

  mirror::ArtMethod** GetMethodAddress() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    DCHECK(method_ != nullptr);
    return &method_;
  }

  virtual mirror::Object* GetThisObject() const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) OVERRIDE;

  mirror::Object* GetThisObject(uint16_t num_ins) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ThrowLocation GetCurrentLocationForThrow() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void SetMethod(mirror::ArtMethod* method) {
    if (kUsePortableCompiler) {
      DCHECK(method != nullptr);
      method_ = method;
    } else {
      UNUSED(method);
      UNIMPLEMENTED(FATAL) << "Should only be called when portable is enabled";
    }
  }

  bool Contains(StackReference<mirror::Object>* shadow_frame_entry_obj) const {
    if (HasReferenceArray()) {
      return ((&References()[0] <= shadow_frame_entry_obj) &&
              (shadow_frame_entry_obj <= (&References()[NumberOfVRegs() - 1])));
    } else {
      uint32_t* shadow_frame_entry = reinterpret_cast<uint32_t*>(shadow_frame_entry_obj);
      return ((&vregs_[0] <= shadow_frame_entry) &&
              (shadow_frame_entry <= (&vregs_[NumberOfVRegs() - 1])));
    }
  }

  static size_t LinkOffset() {
    return OFFSETOF_MEMBER(ShadowFrame, link_);
  }

  static size_t MethodOffset() {
    return OFFSETOF_MEMBER(ShadowFrame, method_);
  }

  static size_t DexPCOffset() {
    return OFFSETOF_MEMBER(ShadowFrame, dex_pc_);
  }

  static size_t NumberOfVRegsOffset() {
    return OFFSETOF_MEMBER(ShadowFrame, number_of_vregs_);
  }

  static size_t VRegsOffset() {
    return OFFSETOF_MEMBER(ShadowFrame, vregs_);
  }

 private:
  ShadowFrame(uint32_t num_vregs, ShadowFrame* link, mirror::ArtMethod* method,
              uint32_t dex_pc, bool has_reference_array)
      : number_of_vregs_(num_vregs), link_(link), method_(method), dex_pc_(dex_pc) {
    if (has_reference_array) {
      if (kUsePortableCompiler) {
        CHECK_LT(num_vregs, static_cast<uint32_t>(kHasReferenceArray));
        number_of_vregs_ |= kHasReferenceArray;
      }
      memset(vregs_, 0, num_vregs * (sizeof(uint32_t) + sizeof(StackReference<mirror::Object>)));
    } else {
      memset(vregs_, 0, num_vregs * sizeof(uint32_t));
    }
  }

  const StackReference<mirror::Object>* References() const {
    DCHECK(HasReferenceArray());
    const uint32_t* vreg_end = &vregs_[NumberOfVRegs()];
    return reinterpret_cast<const StackReference<mirror::Object>*>(vreg_end);
  }

  StackReference<mirror::Object>* References() {
    return const_cast<StackReference<mirror::Object>*>(const_cast<const ShadowFrame*>(this)->References());
  }

  // TODO: Make const with portable enabled.
  /* const */ uint32_t number_of_vregs_;
  static const bool kHasReferenceArray = false;

  // Link to previous shadow frame or NULL.
  ShadowFrame* link_;
  mirror::ArtMethod* method_;
  uint32_t dex_pc_;
  uint32_t vregs_[0];

  DISALLOW_IMPLICIT_CONSTRUCTORS(ShadowFrame);
};

/**
 * Represents a frame compiled with Quick. The layout is the following:
 *
 *     +-------------------------------+
 *     | IN[ins-1]                     |  {Note: resides in caller's frame}
 *     |       .                       |
 *     | IN[0]                         |
 *     | caller's ArtMethod            |  ... StackReference<ArtMethod>
 *     +===============================+  {Note: start of callee's frame}
 *     | core callee-save spill        |  {variable sized}
 *     +-------------------------------+
 *     | fp callee-save spill          |
 *     +-------------------------------+
 *     | filler word                   |  {For compatibility, if V[locals-1] used as wide
 *     +-------------------------------+
 *     | V[locals-1]                   |
 *     | V[locals-2]                   |
 *     |      .                        |
 *     |      .                        |  ... (reg == 2)
 *     | V[1]                          |  ... (reg == 1)
 *     | V[0]                          |  ... (reg == 0) <---- "locals_start"
 *     +-------------------------------+
 *     | stack alignment padding       |  {0 to (kStackAlignWords-1) of padding}
 *     +-------------------------------+
 *     | Compiler temp region          |  ... (reg >= max_num_special_temps)
 *     |      .                        |
 *     |      .                        |
 *     | V[max_num_special_temps + 1]  |
 *     | V[max_num_special_temps + 0]  |
 *     +-------------------------------+
 *     | OUT[outs-1]                   |
 *     | OUT[outs-2]                   |
 *     |       .                       |
 *     | OUT[0]                        |
 *     | StackReference<ArtMethod>     |  ... (reg == num_total_code_regs == special_temp_value) <<== sp, 16-byte aligned
 *     +===============================+
 */
class QuickFrame : public ManagedFrame {
 public:
  QuickFrame(uintptr_t sp, uintptr_t pc, Context* context) : sp_(sp), pc_(pc), context_(context) {}

  virtual bool IsQuickFrame() const OVERRIDE { return true; }

  virtual uint32_t GetDexPc(bool abort_on_failure = true) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) OVERRIDE;

  virtual void SanityCheckFrame() const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) OVERRIDE;

  virtual mirror::Object* GetThisObject() const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) OVERRIDE;

  virtual bool GetVReg(uint16_t vreg, VRegKind kind, uint32_t* val) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) OVERRIDE;

  virtual bool SetVReg(uint16_t vreg, uint32_t new_value, VRegKind kind)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) OVERRIDE;;

  virtual bool GetVRegPair(uint16_t vreg, VRegKind kind_lo, VRegKind kind_hi, uint64_t* val) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) OVERRIDE;

  virtual bool SetVRegPair(uint16_t vreg, uint64_t new_value, VRegKind kind_lo, VRegKind kind_hi)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) OVERRIDE;

  mirror::Object* GetJniThis() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static int GetOutVROffset(uint16_t out_num, InstructionSet isa) {
    // According to stack model, the first out is above the Method referernce.
    return sizeof(StackReference<mirror::ArtMethod>) + (out_num * sizeof(uint32_t));
  }

  virtual mirror::ArtMethod* GetMethod() const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) OVERRIDE {
    return reinterpret_cast<StackReference<mirror::ArtMethod>*>(sp_)->AsMirrorPtr();
  }

  uintptr_t GetReturnPc() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void SetReturnPc(uintptr_t new_ret_pc) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  QuickFrame GetCaller() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  uintptr_t GetPc() const { return pc_; }
  void SetPc(uintptr_t pc) { pc_ = pc; }
  uintptr_t GetSp() const { return sp_; }
  Context* GetContext() const { return context_; }

  void SetMethod(mirror::ArtMethod* method) const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    reinterpret_cast<StackReference<mirror::ArtMethod>*>(sp_)->Assign(method);
  }

  uintptr_t* GetGPRAddress(uint32_t reg) const;

  // This is a fast-path for getting/setting values in a quick frame.
  uint32_t* GetVRegAddr(const DexFile::CodeItem* code_item,
                        uint32_t core_spills, uint32_t fp_spills, size_t frame_size,
                        uint16_t vreg) const {
    int offset = GetVRegOffset(code_item, core_spills, fp_spills, frame_size, vreg, kRuntimeISA);
    uintptr_t vreg_addr = sp_ + offset;
    return reinterpret_cast<uint32_t*>(vreg_addr);
  }

  /*
   * Return sp-relative offset for a Dalvik virtual register, compiler
   * spill or Method* in bytes using Method*.
   * Note that (reg == -1) denotes an invalid Dalvik register. For the
   * positive values, the Dalvik registers come first, followed by the
   * Method*, followed by other special temporaries if any, followed by
   * regular compiler temporary. As of now we only have the Method* as
   * as a special compiler temporary.
   * A compiler temporary can be thought of as a virtual register that
   * does not exist in the dex but holds intermediate values to help
   * optimizations and code generation. A special compiler temporary is
   * one whose location in frame is well known while non-special ones
   * do not have a requirement on location in frame as long as code
   * generator itself knows how to access them.
   */
  static int GetVRegOffset(const DexFile::CodeItem* code_item,
                           uint32_t core_spills, uint32_t fp_spills,
                           size_t frame_size, int reg, InstructionSet isa) {
    DCHECK_EQ(frame_size & (kStackAlignment - 1), 0U);
    DCHECK_NE(reg, -1);
    int spill_size = POPCOUNT(core_spills) * GetBytesPerGprSpillLocation(isa)
        + POPCOUNT(fp_spills) * GetBytesPerFprSpillLocation(isa)
        + sizeof(uint32_t);  // Filler.
    int num_regs = code_item->registers_size_ - code_item->ins_size_;
    int temp_threshold = code_item->registers_size_;
    const int max_num_special_temps = 1;
    if (reg == temp_threshold) {
      // The current method pointer corresponds to special location on stack.
      return 0;
    } else if (reg >= temp_threshold + max_num_special_temps) {
      /*
       * Special temporaries may have custom locations and the logic above deals with that.
       * However, non-special temporaries are placed relative to the outs.
       */
      int temps_start = sizeof(StackReference<mirror::ArtMethod>) + code_item->outs_size_ * sizeof(uint32_t);
      int relative_offset = (reg - (temp_threshold + max_num_special_temps)) * sizeof(uint32_t);
      return temps_start + relative_offset;
    }  else if (reg < num_regs) {
      int locals_start = frame_size - spill_size - num_regs * sizeof(uint32_t);
      return locals_start + (reg * sizeof(uint32_t));
    } else {
      // Handle ins.
      return frame_size + ((reg - num_regs) * sizeof(uint32_t)) +
          sizeof(StackReference<mirror::ArtMethod>);
    }
  }

  uintptr_t* CalleeSaveAddress(int num, size_t frame_size) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    // Callee saves are held at the top of the frame
    DCHECK(GetMethod() != nullptr);
    uintptr_t save_addr = sp_ + frame_size - ((num + 1) * kPointerSize);
#if defined(__i386__) || defined(__x86_64__)
    save_addr -= kPointerSize;  // account for return address
#endif
    return reinterpret_cast<uintptr_t*>(save_addr);
  }

  size_t GetNativePcOffset() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 private:
  bool GetGPR(uint32_t reg, uintptr_t* val) const;
  bool SetGPR(uint32_t reg, uintptr_t value);
  bool GetFPR(uint32_t reg, uintptr_t* val) const;
  bool SetFPR(uint32_t reg, uintptr_t value);

  uintptr_t sp_;
  uintptr_t pc_;
  Context* context_;
};

// The managed stack is used to record fragments of managed code stacks. Managed code stacks
// may either be shadow frames or lists of frames using fixed frame sizes. Transition records are
// necessary for transitions between code using different frame layouts and transitions into native
// code.
class PACKED(4) ManagedStack {
 public:
  ManagedStack()
      : link_(NULL), top_shadow_frame_(NULL), top_compiled_frame_sp_(0) {}

  void PushManagedStackFragment(ManagedStack* fragment) {
    // Copy this top fragment into given fragment.
    memcpy(fragment, this, sizeof(ManagedStack));
    // Clear this fragment, which has become the top.
    memset(this, 0, sizeof(ManagedStack));
    // Link our top fragment onto the given fragment.
    link_ = fragment;
  }

  void PopManagedStackFragment(const ManagedStack& fragment) {
    DCHECK(&fragment == link_);
    // Copy this given fragment back to the top.
    memcpy(this, &fragment, sizeof(ManagedStack));
  }

  ManagedStack* GetLink() const {
    return link_;
  }

  uintptr_t GetTopCompiledFrameSp() const {
    return top_compiled_frame_sp_;
  }

  void SetTopCompiledFrameSp(uintptr_t sp) {
    top_compiled_frame_sp_ = sp;
  }

  static size_t TopCompiledFrameSpOffset() {
    return OFFSETOF_MEMBER(ManagedStack, top_compiled_frame_sp_);
  }

  ShadowFrame* PushShadowFrame(ShadowFrame* new_top_frame) {
    DCHECK_EQ(top_compiled_frame_sp_, 0u);
    ShadowFrame* old_frame = top_shadow_frame_;
    top_shadow_frame_ = new_top_frame;
    new_top_frame->SetLink(old_frame);
    return old_frame;
  }

  ShadowFrame* PopShadowFrame() {
    DCHECK_EQ(top_compiled_frame_sp_, 0u);
    DCHECK(top_shadow_frame_ != nullptr);
    ShadowFrame* frame = top_shadow_frame_;
    top_shadow_frame_ = frame->GetLink();
    return frame;
  }

  ShadowFrame* GetTopShadowFrame() const {
    return top_shadow_frame_;
  }

  void SetTopShadowFrame(ShadowFrame* top) {
    DCHECK_EQ(top_compiled_frame_sp_, 0u);
    top_shadow_frame_ = top;
  }

  static size_t TopShadowFrameOffset() {
    return OFFSETOF_MEMBER(ManagedStack, top_shadow_frame_);
  }

  mirror::ArtMethod* GetTopMethod() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return top_shadow_frame_ != nullptr
        ? top_shadow_frame_->GetMethod()
        : QuickFrame(top_compiled_frame_sp_, 0, nullptr).GetMethod();
  }

  size_t NumJniShadowFrameReferences() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool ShadowFramesContain(StackReference<mirror::Object>* shadow_frame_entry) const;

 private:
  ManagedStack* link_;
  ShadowFrame* top_shadow_frame_;
  uintptr_t top_compiled_frame_sp_;
};

class StackVisitor {
 protected:
  StackVisitor(Thread* thread, Context* context) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 public:
  virtual ~StackVisitor() {}

  // Return 'true' if we should continue to visit more frames, 'false' to stop.
  virtual bool VisitFrame() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) = 0;

  void WalkStack(bool include_transitions = false) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  mirror::ArtMethod* GetMethod() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return current_frame_ == nullptr
        ? nullptr
        : current_frame_->GetMethod();
  }

  bool IsShadowFrame() const {
    return current_frame_ != nullptr && current_frame_->IsShadowFrame();
  }

  bool IsQuickFrame() const {
    return current_frame_ != nullptr && current_frame_->IsQuickFrame();
  }

  uint32_t GetDexPc(bool abort_on_failure = true) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return current_frame_->GetDexPc(abort_on_failure);
  }

  mirror::Object* GetThisObject() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return current_frame_->GetThisObject();
  }

  // Returns the height of the stack in the managed stack frames, including transitions.
  size_t GetFrameHeight() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetNumFrames() - cur_depth_ - 1;
  }

  // Returns a frame ID for JDWP use, starting from 1.
  size_t GetFrameId() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetFrameHeight() + 1;
  }

  size_t GetNumFrames() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    if (num_frames_ == 0) {
      num_frames_ = ComputeNumFrames(thread_);
    }
    return num_frames_;
  }

  size_t GetFrameDepth() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return cur_depth_;
  }

  // Get the method and dex pc immediately after the one that's currently being visited.
  bool GetNextMethodAndDexPc(mirror::ArtMethod** next_method, uint32_t* next_dex_pc)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool GetVReg(uint16_t vreg, VRegKind kind, uint32_t* val) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return current_frame_->GetVReg(vreg, kind, val);
  }

  uint32_t GetVReg(uint16_t vreg, VRegKind kind) const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    uint32_t val;
    bool success = GetVReg(vreg, kind, &val);
    CHECK(success) << "Failed to read vreg " << vreg << " of kind " << kind;
    return val;
  }

  bool GetVRegPair(uint16_t vreg, VRegKind kind_lo, VRegKind kind_hi, uint64_t* val) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return current_frame_->GetVRegPair(vreg, kind_lo, kind_hi, val);
  }

  uint64_t GetVRegPair(uint16_t vreg, VRegKind kind_lo, VRegKind kind_hi) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    uint64_t val;
    bool success = GetVRegPair(vreg, kind_lo, kind_hi, &val);
    CHECK(success) << "Failed to read vreg pair " << vreg
                   << " of kind [" << kind_lo << "," << kind_hi << "]";
    return val;
  }

  bool SetVReg(uint16_t vreg, uint32_t new_value, VRegKind kind)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return current_frame_->SetVReg(vreg, new_value, kind);
  }

  bool SetVRegPair(uint16_t vreg, uint64_t new_value, VRegKind kind_lo, VRegKind kind_hi)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return current_frame_->SetVRegPair(vreg, new_value, kind_lo, kind_hi);
  }

  std::string DescribeLocation() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static size_t ComputeNumFrames(Thread* thread) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static void DescribeStack(Thread* thread) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  QuickFrame* GetQuickFrame() const {
    DCHECK(current_frame_->IsQuickFrame());
    return reinterpret_cast<QuickFrame*>(current_frame_);
  }

  ShadowFrame* GetShadowFrame() const {
    DCHECK(current_frame_->IsShadowFrame());
    return reinterpret_cast<ShadowFrame*>(current_frame_);
  }

 private:
  // Private constructor known in the case that num_frames_ has already been computed.
  StackVisitor(Thread* thread, Context* context, size_t num_frames)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void SanityCheckFrame() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  Thread* const thread_;
  ManagedFrame* current_frame_;
  // Lazily computed, number of frames in the stack.
  size_t num_frames_;
  // Depth of the frame we're currently at.
  size_t cur_depth_;

 protected:
  Context* const context_;

  friend class QuickFrameIterator;
};

}  // namespace art

#endif  // ART_RUNTIME_STACK_H_
