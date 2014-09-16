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

#include "stack.h"

#include "arch/context.h"
#include "base/hex_dump.h"
#include "mirror/art_method-inl.h"
#include "mirror/class-inl.h"
#include "mirror/object.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "quick/quick_method_frame_info.h"
#include "runtime.h"
#include "thread.h"
#include "thread_list.h"
#include "throw_location.h"
#include "verify_object-inl.h"
#include "vmap_table.h"

namespace art {

mirror::Object* ShadowFrame::GetThisObject() const {
  mirror::ArtMethod* m = GetMethod();
  if (m->IsStatic()) {
    return NULL;
  } else if (m->IsNative()) {
    return GetVRegReference(0);
  } else {
    const DexFile::CodeItem* code_item = m->GetCodeItem();
    CHECK(code_item != NULL) << PrettyMethod(m);
    uint16_t reg = code_item->registers_size_ - code_item->ins_size_;
    return GetVRegReference(reg);
  }
}

mirror::Object* ShadowFrame::GetThisObject(uint16_t num_ins) const {
  mirror::ArtMethod* m = GetMethod();
  if (m->IsStatic()) {
    return NULL;
  } else {
    return GetVRegReference(NumberOfVRegs() - num_ins);
  }
}

ThrowLocation ShadowFrame::GetCurrentLocationForThrow() const {
  return ThrowLocation(GetThisObject(), GetMethod(), GetDexPc());
}

size_t ManagedStack::NumJniShadowFrameReferences() const {
  size_t count = 0;
  for (const ManagedStack* current_fragment = this; current_fragment != NULL;
       current_fragment = current_fragment->GetLink()) {
    for (ShadowFrame* current_frame = current_fragment->top_shadow_frame_; current_frame != NULL;
         current_frame = current_frame->GetLink()) {
      if (current_frame->GetMethod()->IsNative()) {
        // The JNI ShadowFrame only contains references. (For indirect reference.)
        count += current_frame->NumberOfVRegs();
      }
    }
  }
  return count;
}

bool ManagedStack::ShadowFramesContain(StackReference<mirror::Object>* shadow_frame_entry) const {
  for (const ManagedStack* current_fragment = this; current_fragment != NULL;
       current_fragment = current_fragment->GetLink()) {
    for (ShadowFrame* current_frame = current_fragment->top_shadow_frame_; current_frame != NULL;
         current_frame = current_frame->GetLink()) {
      if (current_frame->Contains(shadow_frame_entry)) {
        return true;
      }
    }
  }
  return false;
}

uint32_t QuickFrame::GetDexPc(bool abort_on_failure) const {
  return GetMethod()->ToDexPc(pc_, abort_on_failure);
}

mirror::Object* QuickFrame::GetThisObject() const {
  mirror::ArtMethod* m = GetMethod();
  if (m->IsStatic()) {
    return nullptr;
  } else if (m->IsNative()) {
    HandleScope* hs = reinterpret_cast<HandleScope*>(
        GetSp() + m->GetHandleScopeOffsetInBytes());
    return hs->GetReference(0);
  } else if (m->IsOptimized()) {
    // TODO: Implement, currently only used for exceptions when jdwp is enabled.
    LOG(WARNING) << "StackVisitor::GetThisObject is unimplemented with the optimizing compiler";
    return nullptr;
  } else {
    const DexFile::CodeItem* code_item = m->GetCodeItem();
    if (code_item == nullptr) {
      UNIMPLEMENTED(ERROR) << "Failed to determine this object of abstract or proxy method: "
          << PrettyMethod(m);
      return nullptr;
    } else {
      uint16_t reg = code_item->registers_size_ - code_item->ins_size_;
      uint32_t val;
      GetVReg(reg, kReferenceVReg, &val);
      return reinterpret_cast<mirror::Object*>(val);
    }
  }
}

size_t QuickFrame::GetNativePcOffset() const {
  return GetMethod()->NativePcOffset(pc_);
}

bool QuickFrame::GetVReg(uint16_t vreg, VRegKind kind, uint32_t* val) const {
  mirror::ArtMethod* m = GetMethod();
  const void* code_pointer = m->GetQuickOatCodePointer();
  DCHECK(code_pointer != nullptr);
  const VmapTable vmap_table(m->GetVmapTable(code_pointer));
  QuickMethodFrameInfo frame_info = m->GetQuickFrameInfo(code_pointer);
  uint32_t vmap_offset;
  // TODO: IsInContext stops before spotting floating point registers.
  if (vmap_table.IsInContext(vreg, kind, &vmap_offset)) {
    bool is_float = (kind == kFloatVReg) || (kind == kDoubleLoVReg) || (kind == kDoubleHiVReg);
    uint32_t spill_mask = is_float ? frame_info.FpSpillMask() : frame_info.CoreSpillMask();
    uint32_t reg = vmap_table.ComputeRegister(spill_mask, vmap_offset, kind);
    uintptr_t ptr_val;
    bool success = is_float ? GetFPR(reg, &ptr_val) : GetGPR(reg, &ptr_val);
    if (!success) {
      return false;
    }
    bool target64 = Is64BitInstructionSet(kRuntimeISA);
    if (target64) {
      bool wide_lo = (kind == kLongLoVReg) || (kind == kDoubleLoVReg);
      bool wide_hi = (kind == kLongHiVReg) || (kind == kDoubleHiVReg);
      int64_t value_long = static_cast<int64_t>(ptr_val);
      if (wide_lo) {
        ptr_val = static_cast<uintptr_t>(value_long & 0xFFFFFFFF);
      } else if (wide_hi) {
        ptr_val = static_cast<uintptr_t>(value_long >> 32);
      }
    }
    *val = ptr_val;
    return true;
  } else {
    const DexFile::CodeItem* code_item = m->GetCodeItem();
    DCHECK(code_item != nullptr) << PrettyMethod(m);  // Can't be NULL or how would we compile
                                                      // its instructions?
    *val = *GetVRegAddr(code_item, frame_info.CoreSpillMask(),
                        frame_info.FpSpillMask(), frame_info.FrameSizeInBytes(), vreg);
    return true;
  }
}

bool QuickFrame::GetVRegPair(uint16_t vreg,
                             VRegKind kind_lo,
                             VRegKind kind_hi,
                             uint64_t* val) const {
  if (kind_lo == kLongLoVReg) {
    DCHECK_EQ(kind_hi, kLongHiVReg);
  } else if (kind_lo == kDoubleLoVReg) {
    DCHECK_EQ(kind_hi, kDoubleHiVReg);
  } else {
    LOG(FATAL) << "Expected long or double: kind_lo=" << kind_lo << ", kind_hi=" << kind_hi;
  }
  mirror::ArtMethod* m = GetMethod();
  const void* code_pointer = m->GetQuickOatCodePointer();
  DCHECK(code_pointer != nullptr);
  const VmapTable vmap_table(m->GetVmapTable(code_pointer));
  QuickMethodFrameInfo frame_info = m->GetQuickFrameInfo(code_pointer);
  uint32_t vmap_offset_lo, vmap_offset_hi;
  // TODO: IsInContext stops before spotting floating point registers.
  if (vmap_table.IsInContext(vreg, kind_lo, &vmap_offset_lo) &&
      vmap_table.IsInContext(vreg + 1, kind_hi, &vmap_offset_hi)) {
    bool is_float = (kind_lo == kDoubleLoVReg);
    uint32_t spill_mask = is_float ? frame_info.FpSpillMask() : frame_info.CoreSpillMask();
    uint32_t reg_lo = vmap_table.ComputeRegister(spill_mask, vmap_offset_lo, kind_lo);
    uint32_t reg_hi = vmap_table.ComputeRegister(spill_mask, vmap_offset_hi, kind_hi);
    uintptr_t ptr_val_lo, ptr_val_hi;
    bool success = is_float ? GetFPR(reg_lo, &ptr_val_lo) : GetGPR(reg_lo, &ptr_val_lo);
    success &= is_float ? GetFPR(reg_hi, &ptr_val_hi) : GetGPR(reg_hi, &ptr_val_hi);
    if (!success) {
      return false;
    }
    bool target64 = Is64BitInstructionSet(kRuntimeISA);
    if (target64) {
      int64_t value_long_lo = static_cast<int64_t>(ptr_val_lo);
      int64_t value_long_hi = static_cast<int64_t>(ptr_val_hi);
      ptr_val_lo = static_cast<uintptr_t>(value_long_lo & 0xFFFFFFFF);
      ptr_val_hi = static_cast<uintptr_t>(value_long_hi >> 32);
    }
    *val = (static_cast<uint64_t>(ptr_val_hi) << 32) | static_cast<uint32_t>(ptr_val_lo);
    return true;
  } else {
    const DexFile::CodeItem* code_item = m->GetCodeItem();
    DCHECK(code_item != nullptr) << PrettyMethod(m);  // Can't be NULL or how would we compile
                                                      // its instructions?
    uint32_t* addr = GetVRegAddr(code_item, frame_info.CoreSpillMask(),
                                 frame_info.FpSpillMask(), frame_info.FrameSizeInBytes(), vreg);
    *val = *reinterpret_cast<uint64_t*>(addr);
    return true;
  }
}

bool QuickFrame::SetVReg(uint16_t vreg, uint32_t new_value, VRegKind kind) {
  mirror::ArtMethod* m = GetMethod();
  const void* code_pointer = m->GetQuickOatCodePointer();
  DCHECK(code_pointer != nullptr);
  const VmapTable vmap_table(m->GetVmapTable(code_pointer));
  QuickMethodFrameInfo frame_info = m->GetQuickFrameInfo(code_pointer);
  uint32_t vmap_offset;
  // TODO: IsInContext stops before spotting floating point registers.
  if (vmap_table.IsInContext(vreg, kind, &vmap_offset)) {
    bool is_float = (kind == kFloatVReg) || (kind == kDoubleLoVReg) || (kind == kDoubleHiVReg);
    uint32_t spill_mask = is_float ? frame_info.FpSpillMask() : frame_info.CoreSpillMask();
    const uint32_t reg = vmap_table.ComputeRegister(spill_mask, vmap_offset, kind);
    bool target64 = Is64BitInstructionSet(kRuntimeISA);
    // Deal with 32 or 64-bit wide registers in a way that builds on all targets.
    if (target64) {
      bool wide_lo = (kind == kLongLoVReg) || (kind == kDoubleLoVReg);
      bool wide_hi = (kind == kLongHiVReg) || (kind == kDoubleHiVReg);
      if (wide_lo || wide_hi) {
        uintptr_t old_reg_val;
        bool success = is_float ? GetFPR(reg, &old_reg_val) : GetGPR(reg, &old_reg_val);
        if (!success) {
          return false;
        }
        uint64_t new_vreg_portion = static_cast<uint64_t>(new_value);
        uint64_t old_reg_val_as_wide = static_cast<uint64_t>(old_reg_val);
        uint64_t mask = 0xffffffff;
        if (wide_lo) {
          mask = mask << 32;
        } else {
          new_vreg_portion = new_vreg_portion << 32;
        }
        new_value = static_cast<uintptr_t>((old_reg_val_as_wide & mask) | new_vreg_portion);
      }
    }
    if (is_float) {
      return SetFPR(reg, new_value);
    } else {
      return SetGPR(reg, new_value);
    }
  } else {
    const DexFile::CodeItem* code_item = m->GetCodeItem();
    DCHECK(code_item != nullptr) << PrettyMethod(m);  // Can't be NULL or how would we compile
                                                      // its instructions?
    uint32_t* addr = GetVRegAddr(code_item, frame_info.CoreSpillMask(),
                                 frame_info.FpSpillMask(), frame_info.FrameSizeInBytes(), vreg);
    *addr = new_value;
    return true;
  }
}

bool QuickFrame::SetVRegPair(uint16_t vreg,
                             uint64_t new_value,
                             VRegKind kind_lo,
                             VRegKind kind_hi) {
  if (kind_lo == kLongLoVReg) {
    DCHECK_EQ(kind_hi, kLongHiVReg);
  } else if (kind_lo == kDoubleLoVReg) {
    DCHECK_EQ(kind_hi, kDoubleHiVReg);
  } else {
    LOG(FATAL) << "Expected long or double: kind_lo=" << kind_lo << ", kind_hi=" << kind_hi;
  }
  mirror::ArtMethod* m = GetMethod();
  const void* code_pointer = m->GetQuickOatCodePointer();
  DCHECK(code_pointer != nullptr);
  const VmapTable vmap_table(m->GetVmapTable(code_pointer));
  QuickMethodFrameInfo frame_info = m->GetQuickFrameInfo(code_pointer);
  uint32_t vmap_offset_lo, vmap_offset_hi;
  // TODO: IsInContext stops before spotting floating point registers.
  if (vmap_table.IsInContext(vreg, kind_lo, &vmap_offset_lo) &&
      vmap_table.IsInContext(vreg + 1, kind_hi, &vmap_offset_hi)) {
    bool is_float = (kind_lo == kDoubleLoVReg);
    uint32_t spill_mask = is_float ? frame_info.FpSpillMask() : frame_info.CoreSpillMask();
    uint32_t reg_lo = vmap_table.ComputeRegister(spill_mask, vmap_offset_lo, kind_lo);
    uint32_t reg_hi = vmap_table.ComputeRegister(spill_mask, vmap_offset_hi, kind_hi);
    uintptr_t new_value_lo = static_cast<uintptr_t>(new_value & 0xFFFFFFFF);
    uintptr_t new_value_hi = static_cast<uintptr_t>(new_value >> 32);
    bool target64 = Is64BitInstructionSet(kRuntimeISA);
    // Deal with 32 or 64-bit wide registers in a way that builds on all targets.
    if (target64) {
      uintptr_t old_reg_val_lo, old_reg_val_hi;
      bool success = is_float ? GetFPR(reg_lo, &old_reg_val_lo) : GetGPR(reg_lo, &old_reg_val_lo);
      success &= is_float ? GetFPR(reg_hi, &old_reg_val_hi) : GetGPR(reg_hi, &old_reg_val_hi);
      if (!success) {
        return false;
      }
      uint64_t new_vreg_portion_lo = static_cast<uint64_t>(new_value_lo);
      uint64_t new_vreg_portion_hi = static_cast<uint64_t>(new_value_hi) << 32;
      uint64_t old_reg_val_lo_as_wide = static_cast<uint64_t>(old_reg_val_lo);
      uint64_t old_reg_val_hi_as_wide = static_cast<uint64_t>(old_reg_val_hi);
      uint64_t mask_lo = static_cast<uint64_t>(0xffffffff) << 32;
      uint64_t mask_hi = 0xffffffff;
      new_value_lo = static_cast<uintptr_t>((old_reg_val_lo_as_wide & mask_lo) | new_vreg_portion_lo);
      new_value_hi = static_cast<uintptr_t>((old_reg_val_hi_as_wide & mask_hi) | new_vreg_portion_hi);
    }
    bool success = is_float ? SetFPR(reg_lo, new_value_lo) : SetGPR(reg_lo, new_value_lo);
    success &= is_float ? SetFPR(reg_hi, new_value_hi) : SetGPR(reg_hi, new_value_hi);
    return success;
  } else {
    const DexFile::CodeItem* code_item = m->GetCodeItem();
    DCHECK(code_item != nullptr) << PrettyMethod(m);  // Can't be NULL or how would we compile
                                                      // its instructions?
    uint32_t* addr = GetVRegAddr(code_item, frame_info.CoreSpillMask(),
                                 frame_info.FpSpillMask(), frame_info.FrameSizeInBytes(), vreg);
    *reinterpret_cast<uint64_t*>(addr) = new_value;
    return true;
  }
}

uintptr_t* QuickFrame::GetGPRAddress(uint32_t reg) const {
  return context_->GetGPRAddress(reg);
}

bool QuickFrame::GetGPR(uint32_t reg, uintptr_t* val) const {
  return context_->GetGPR(reg, val);
}

bool QuickFrame::SetGPR(uint32_t reg, uintptr_t value) {
  return context_->SetGPR(reg, value);
}

bool QuickFrame::GetFPR(uint32_t reg, uintptr_t* val) const {
  return context_->GetFPR(reg, val);
}

bool QuickFrame::SetFPR(uint32_t reg, uintptr_t value) {
  return context_->SetFPR(reg, value);
}

uintptr_t QuickFrame::GetReturnPc() const {
  uintptr_t pc_addr = GetSp() + GetMethod()->GetReturnPcOffsetInBytes();
  return *reinterpret_cast<uintptr_t*>(pc_addr);
}

void QuickFrame::SetReturnPc(uintptr_t new_ret_pc) {
  uintptr_t pc_addr = GetSp() + GetMethod()->GetReturnPcOffsetInBytes();
  *reinterpret_cast<uintptr_t*>(pc_addr) = new_ret_pc;
}

QuickFrame QuickFrame::GetCaller() const {
  mirror::ArtMethod* method = GetMethod();
  size_t frame_size = method->GetFrameSizeInBytes();
  uintptr_t return_pc = GetReturnPc();
  uintptr_t next_frame = GetSp() + frame_size;
  return QuickFrame(reinterpret_cast<StackReference<mirror::ArtMethod>*>(next_frame),
                    return_pc,
                    context_);
}

mirror::Object* QuickFrame::GetJniThis() const {
  // Skip Method*; handle scope comes next;
  return reinterpret_cast<HandleScope*>(
      GetSp() + sizeof(StackReference<mirror::ArtMethod>))->GetReference(0);
}

StackVisitor::StackVisitor(Thread* thread, Context* context)
    : thread_(thread), current_frame_(nullptr), num_frames_(0), cur_depth_(0), context_(context) {
  DCHECK(thread == Thread::Current() || thread->IsSuspended()) << *thread;
}

StackVisitor::StackVisitor(Thread* thread, Context* context, size_t num_frames)
    : thread_(thread), current_frame_(nullptr), num_frames_(num_frames), cur_depth_(0),
      context_(context) {
  DCHECK(thread == Thread::Current() || thread->IsSuspended()) << *thread;
}

size_t StackVisitor::ComputeNumFrames(Thread* thread) {
  struct NumFramesVisitor : public StackVisitor {
    explicit NumFramesVisitor(Thread* thread)
        : StackVisitor(thread, NULL), frames(0) {}

    bool VisitFrame() OVERRIDE {
      frames++;
      return true;
    }

    size_t frames;
  };
  NumFramesVisitor visitor(thread);
  visitor.WalkStack(true);
  return visitor.frames;
}

bool StackVisitor::GetNextMethodAndDexPc(mirror::ArtMethod** next_method, uint32_t* next_dex_pc) {
  struct HasMoreFramesVisitor : public StackVisitor {
    explicit HasMoreFramesVisitor(Thread* thread, size_t num_frames, size_t frame_height)
        : StackVisitor(thread, nullptr, num_frames), frame_height_(frame_height),
          found_frame_(false), has_more_frames_(false), next_method_(nullptr), next_dex_pc_(0) {
    }

    bool VisitFrame() OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      if (found_frame_) {
        mirror::ArtMethod* method = GetMethod();
        if (method != nullptr && !method->IsRuntimeMethod()) {
          has_more_frames_ = true;
          next_method_ = method;
          next_dex_pc_ = GetDexPc();
          return false;  // End stack walk once next method is found.
        }
      } else if (GetFrameHeight() == frame_height_) {
        found_frame_ = true;
      }
      return true;
    }

    size_t frame_height_;
    bool found_frame_;
    bool has_more_frames_;
    mirror::ArtMethod* next_method_;
    uint32_t next_dex_pc_;
  };
  HasMoreFramesVisitor visitor(thread_, GetNumFrames(), GetFrameHeight());
  visitor.WalkStack(true);
  *next_method = visitor.next_method_;
  *next_dex_pc = visitor.next_dex_pc_;
  return visitor.has_more_frames_;
}

void StackVisitor::DescribeStack(Thread* thread) {
  struct DescribeStackVisitor : public StackVisitor {
    explicit DescribeStackVisitor(Thread* thread)
        : StackVisitor(thread, NULL) {}

    bool VisitFrame() OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      LOG(INFO) << "Frame Id=" << GetFrameId() << " " << DescribeLocation();
      return true;
    }
  };
  DescribeStackVisitor visitor(thread);
  visitor.WalkStack(true);
}

std::string StackVisitor::DescribeLocation() const {
  std::string result("Visiting method '");
  mirror::ArtMethod* m = GetMethod();
  if (m == NULL) {
    return "upcall";
  }
  result += PrettyMethod(m);
  result += StringPrintf("' at dex PC 0x%04x", GetDexPc());
  if (IsQuickFrame()) {
    result += StringPrintf(" (native PC %p)", reinterpret_cast<void*>(GetQuickFrame()->GetPc()));
  }
  return result;
}

static instrumentation::InstrumentationStackFrame& GetInstrumentationStackFrame(Thread* thread,
                                                                                uint32_t depth) {
  CHECK_LT(depth, thread->GetInstrumentationStack()->size());
  return thread->GetInstrumentationStack()->at(depth);
}

void StackVisitor::SanityCheckFrame() const {
  if (kIsDebugBuild) {
    mirror::ArtMethod* method = GetMethod();
    CHECK_EQ(method->GetClass(), mirror::ArtMethod::GetJavaLangReflectArtMethod());
    current_frame_->SanityCheckFrame();
  }
}

void QuickFrame::SanityCheckFrame() const {
  GetMethod()->AssertPcIsWithinQuickCode(pc_);
  // Frame sanity.
  size_t frame_size = GetMethod()->GetFrameSizeInBytes();
  CHECK_NE(frame_size, 0u);
  // A rough guess at an upper size we expect to see for a frame.
  // 256 registers
  // 2 words HandleScope overhead
  // 3+3 register spills
  // TODO: this seems architecture specific for the case of JNI frames.
  // TODO: 083-compiler-regressions ManyFloatArgs shows this estimate is wrong.
  // const size_t kMaxExpectedFrameSize = (256 + 2 + 3 + 3) * sizeof(word);
  const size_t kMaxExpectedFrameSize = 2 * KB;
  CHECK_LE(frame_size, kMaxExpectedFrameSize);
  size_t return_pc_offset = GetMethod()->GetReturnPcOffsetInBytes();
  CHECK_LT(return_pc_offset, frame_size);
}

class QuickFrameIterator {
 public:
  QuickFrameIterator(StackReference<mirror::ArtMethod>* top_quick_frame,
                     bool exit_stubs_installed,
                     uint32_t* instrumentation_stack_depth,
                     StackVisitor* stack_visitor)
      : frame_(top_quick_frame, 0, stack_visitor->context_),
        exit_stubs_installed_(exit_stubs_installed),
        instrumentation_stack_depth_(instrumentation_stack_depth),
        stack_visitor_(stack_visitor) {}

  QuickFrame* Current() { return &frame_; }

  bool Done() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return frame_.GetMethod() == nullptr;
  }

  void Advance() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    if (frame_.GetContext() != nullptr) {
      frame_.GetContext()->FillCalleeSaves(frame_);
    }

    QuickFrame new_frame = frame_.GetCaller();

    if (UNLIKELY(exit_stubs_installed_ && (GetQuickInstrumentationExitPc() == new_frame.GetPc()))) {
      const instrumentation::InstrumentationStackFrame& instrumentation_frame =
          GetInstrumentationStackFrame(stack_visitor_->thread_, *instrumentation_stack_depth_);

      (*instrumentation_stack_depth_)++;
      mirror::ArtMethod* method = frame_.GetMethod();
      if (method == Runtime::Current()->GetCalleeSaveMethod(Runtime::kSaveAll)) {
        // Skip runtime save all callee frames which are used to deliver exceptions.
      } else if (instrumentation_frame.interpreter_entry_) {
        mirror::ArtMethod* callee = Runtime::Current()->GetCalleeSaveMethod(Runtime::kRefsAndArgs);
        CHECK_EQ(frame_.GetMethod(), callee) << "Expected: " << PrettyMethod(callee) << " Found: "
                                            << PrettyMethod(method);
      } else if (instrumentation_frame.method_ != method) {
        LOG(FATAL)  << "Expected: " << PrettyMethod(instrumentation_frame.method_)
                    << " Found: " << PrettyMethod(method);
      }
      if (stack_visitor_->num_frames_ != 0) {
        // Check agreement of frame Ids only if num_frames_ is computed to avoid infinite
        // recursion.
        CHECK(instrumentation_frame.frame_id_ == stack_visitor_->GetFrameId())
              << "Expected: " << instrumentation_frame.frame_id_
              << " Found: " << stack_visitor_->GetFrameId();
      }
      new_frame.SetPc(instrumentation_frame.return_pc_);
    }

    frame_ = new_frame;
  }

 private:
  QuickFrame frame_;
  const bool exit_stubs_installed_;
  uint32_t* const instrumentation_stack_depth_;
  StackVisitor* stack_visitor_;

  DISALLOW_COPY_AND_ASSIGN(QuickFrameIterator);
};

void StackVisitor::WalkStack(bool include_transitions) {
  DCHECK(thread_ == Thread::Current() || thread_->IsSuspended());
  CHECK_EQ(cur_depth_, 0U);
  bool exit_stubs_installed = Runtime::Current()->GetInstrumentation()->AreExitStubsInstalled();
  uint32_t instrumentation_stack_depth = 0;

  for (const ManagedStack* current_fragment = thread_->GetManagedStack();
       current_fragment != nullptr;
       current_fragment = current_fragment->GetLink()) {
    ShadowFrame* shadow_frame = current_fragment->GetTopShadowFrame();
    StackReference<mirror::ArtMethod>* top_quick_frame = current_fragment->GetTopQuickFrame();
    if (top_quick_frame != 0) {
      // Handle Quick stack frames.
      DCHECK(shadow_frame == nullptr);
      QuickFrameIterator it(top_quick_frame, exit_stubs_installed, &instrumentation_stack_depth, this);
      DCHECK(!it.Done());
      do {
        current_frame_ = it.Current();
        SanityCheckFrame();
        bool should_continue = VisitFrame();
        if (UNLIKELY(!should_continue)) {
          return;
        }
        it.Advance();
        cur_depth_++;
      } while (!it.Done());
    } else if (shadow_frame != nullptr) {
      // Handle interpreted/portable stack frames.
      current_frame_ = shadow_frame;
      do {
        SanityCheckFrame();
        bool should_continue = VisitFrame();
        if (UNLIKELY(!should_continue)) {
          return;
        }
        cur_depth_++;
        current_frame_ = GetShadowFrame()->GetLink();
      } while (current_frame_ != nullptr);
    } else {
      current_frame_ = nullptr;
    }
    if (include_transitions) {
      bool should_continue = VisitFrame();
      if (!should_continue) {
        return;
      }
    }
    cur_depth_++;
  }
  // Safety measures, avoid having a value for following WalkStack calls.
  current_frame_ = nullptr;
  if (num_frames_ != 0) {
    CHECK_EQ(cur_depth_, num_frames_);
  }
}

}  // namespace art
