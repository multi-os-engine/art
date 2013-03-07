/*
 * Copyright (C) 2013 The Android Open Source Project
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

#ifndef ART_SRC_COMPILER_LLVM_ART_IR_BUILDER_H_
#define ART_SRC_COMPILER_LLVM_ART_IR_BUILDER_H_

#include "base/casts.h"
// TODO: move MemBarrierKind out of compiler_enums.h.
#include "compiler/dex/compiler_enums.h"
#include "md_builder.h"
#include "primitive.h"
#include "runtime_support_builder.h"
#include "UniquePtr.h"

#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Value.h>

namespace art {
namespace compiler {
namespace Llvm {

// The LLVM IR builder type we're derived from (we use the default with name preservation on and
// have constant folding enabled).
typedef llvm::IRBuilder< > LlvmIRBuilder;

// Create LLVM IR for code within the ART runtime. An IR builder in the LLVM sense is used for
// creating code into a basic block. For ART we use an IR builder to define an LLVM function, as
// such basic blocks may be created.
class ArtIRBuilder : public LlvmIRBuilder {
 public:
  // Get or create a struct that represents a shadow frame for num_vreg_ registers. The struct
  // contains 2 elements, the first is a header of 4 fields, the second is the vreg array. In the
  // module each shadow frame type will have a suffix of the number of vregs it contains.
  llvm::StructType* GetShadowFrameTy();

  // Return an LLVM type for references to java.lang.Object.
  llvm::PointerType* GetJavaObjectTy() const {
    return java_object_type_;
  }

  // Return an LLVM type for references to java.lang.reflect.AbstractMethod.
  llvm::PointerType* GetJavaMethodTy() const {
    return java_method_type_;
  }

  // Return an LLVM type representing a art::Thread*.
  llvm::PointerType* GetThreadTy() const {
    return thread_type_;
  }

  // For the given Java primitive type return a representative LLVM type.
  llvm::Type* GetJavaType(Primitive::Type type) {
    switch (type) {
      case Primitive::kPrimNot:
        return GetJavaObjectTy();
      case Primitive::kPrimBoolean:
        return getInt1Ty();
      case Primitive::kPrimByte:
        return getInt8Ty();
      case Primitive::kPrimShort:
      case Primitive::kPrimChar:  // LLVM doesn't distinguish between signed and unsigned ints.
        return getInt16Ty();
      case Primitive::kPrimInt:
        return getInt32Ty();
      case Primitive::kPrimLong:
        return getInt64Ty();
      case Primitive::kPrimFloat:
        return getFloatTy();
      case Primitive::kPrimDouble:
        return getDoubleTy();
      case Primitive::kPrimVoid:
        return getVoidTy();
      default:
        LOG(FATAL) << "Unreachable" << type;
        return NULL;
    }
  }

  // Returns the current LLVM function being defined.
  llvm::Function* GetLlvmFunction() const {
    return GetInsertBlock()->getParent();
  }

  // Returns the LLVM module being defined into.
  llvm::Module* GetModule() const {
    return module_;
  }

  // A null Object reference.
  llvm::ConstantPointerNull* GetJavaNull() const;

  // Load the mirror::AbstractMethod* of the method we're defining. Relies upon this being the
  // first argument to the method.
  llvm::Value* LoadCurMethod() const;

  // Remember that vreg was last defined by value so it may be flushed later.
  void RememberShadowFrameVReg(uint32_t vreg_slot, llvm::Value* value) {
    cur_vreg_vals_[vreg_slot] = value;
  }

  // Flush known vreg values to the shadow frame, for example, at the end of basic block.
  void FlushShadowFrameVRegs() {
    for (uint32_t i = 0; i < num_vregs_; ++ i) {
      llvm::Value* value = cur_vreg_vals_[i];
      if (value != NULL) {
        llvm::Value* vreg_ptr = GetShadowFrameVRegPtrForSlot(i);
        CreateStore(value,
                    CreateBitCast(vreg_ptr, value->getType()->getPointerTo()),
                    mdb_->GetTbaaForShadowFrameVReg());
        cur_vreg_vals_[i] = NULL;
      }
    }
  }

  // Flush known vreg values to the shadow frame, pushing the shadow frame if it wasn't already,
  // and update the dex pc within the shadow frame. This operation is performed at safe-points.
  void FlushShadowFrameVRegsAndSetDexPc(uint32_t dex_pc) {
    FlushShadowFrameVRegs();
    EnsureShadowFrameIsPushed();
    CreateStore(getInt32(dex_pc), GetShadowFrameDexPcPtr(), mdb_->GetTbaaForShadowFrameDexPc());
  }

  // Get or create a pointer to a vreg within the shadow frame.
  llvm::Value* GetShadowFrameVRegPtrForSlot(uint32_t vreg) {
    DCHECK_LT(vreg, num_vregs_);
    llvm::Value* result = vreg_ptrs_[vreg];
    if (result == NULL) {
      InsertPoint saved_ip = saveIP();
      SetInsertPoint(shadow_frame_->getNextNode());
      llvm::Value* vreg_slot = getInt32(vreg);
      llvm::Value* gep_index[] = {
          getInt32(0), // No pointer displacement.
          getInt32(1), // VRegs.
          vreg_slot    // Slot.
      };
      llvm::AllocaInst* shadow_frame = GetShadowFrame();
      result = CreateGEP(shadow_frame, gep_index);
      vreg_ptrs_[vreg] = result;
      restoreIP(saved_ip);
    }
    return result;
  }

  // Restore the caller's shadow frame. We lazily populate the shadow frame fields and so this may
  // be a no-op, however, we remember the location where the pop would occur so we can re-insert it
  // later should a push occur.
  void PopShadowFrame();

  // Creates a named basic block within the LLVM function we're defining.
  llvm::BasicBlock* CreateBasicBlock(std::string name) {
    if (kIsDebugBuild) {
      return llvm::BasicBlock::Create(getContext(), name, GetLlvmFunction());
    } else {
      return CreateBasicBlock();
    }
  }

  // Creates an anonymous basic block within the LLVM function we're defining.
  llvm::BasicBlock* CreateBasicBlock() {
    return llvm::BasicBlock::Create(getContext(), "", GetLlvmFunction());
  }

  void CreateMemoryBarrier(MemBarrierKind barrier_kind) {
#if ANDROID_SMP
    // TODO: select atomic ordering according to given barrier kind.
    CreateFence(::llvm::SequentiallyConsistent);
#endif
  }

  // Create a pointer to a field within an Object of the given type.
  llvm::Value* CreateObjectFieldPtr(llvm::Value* object_addr,
                                    MemberOffset offset,
                                    llvm::Type* type) {
    // Convert the ptr to an int, add the offset, then convert back to the given type of ptr.
    llvm::ConstantInt* llvm_offset = getInt32(offset.Int32Value());
    llvm::Type* intptr_type = getInt32Ty();
    llvm::Value* base_addr = CreatePtrToInt(object_addr, intptr_type);
    // TODO: set no signed wrap (nsw) ?
    base_addr = CreateAdd(object_addr, llvm_offset);
    return CreateIntToPtr(base_addr, type->getPointerTo());
  }

  // Load a dex_cache_ field from the current method.
  llvm::Value* LoadFieldFromCurMethod(Primitive::Type type, const char* type_descriptor,
                                      const char* field_name,
                                      art::MemberOffset offset, bool is_const) {
    llvm::Value* method = LoadCurMethod();
    llvm::MDNode* md_node = mdb_->GetTbaaForInstanceField(type,
                                                          type_descriptor,
                                                          "Ljava/lang/reflect/AbstractMethod;",
                                                          field_name,
                                                          is_const);
    return LoadFromObjectOffset(method, offset, GetJavaType(type), md_node);
  }

  // Load with added TBAA info.
  llvm::LoadInst* CreateLoad(llvm::Value* ptr, llvm::MDNode* tbaa_info) {
    llvm::LoadInst* inst = LlvmIRBuilder::CreateLoad(ptr);
    if (tbaa_info != NULL) {
      inst->setMetadata(llvm::LLVMContext::MD_tbaa, tbaa_info);
    }
    return inst;
  }

  // Store with added TBAA info.
  llvm::StoreInst* CreateStore(llvm::Value* val, llvm::Value* ptr, llvm::MDNode* tbaa_info) {
    llvm::StoreInst* inst = LlvmIRBuilder::CreateStore(val, ptr);
    if (tbaa_info != NULL) {
      inst->setMetadata(llvm::LLVMContext::MD_tbaa, tbaa_info);
    }
    return inst;
  }

  // CmpXchg with added TBAA info.
  llvm::AtomicCmpXchgInst* CreateAtomicCmpXchgInst(llvm::Value* ptr, llvm::Value* cmp,
                                                     llvm::Value* val,
                                                     llvm::MDNode* tbaa_info) {
    llvm::AtomicCmpXchgInst* inst = LlvmIRBuilder::CreateAtomicCmpXchg(ptr, cmp, val,
                                                                         llvm::Acquire);
    if (tbaa_info != NULL) {
      inst->setMetadata(llvm::LLVMContext::MD_tbaa, tbaa_info);
    }
    return inst;
  }

  // Weighted branch expectations.
  enum BranchTakenExpectation {
    kLikely,
    kUnknown,
    kUnlikely
  };
  // Create a weighted branch.
  llvm::BranchInst* CreateCondBr(llvm::Value *cond,
                                 llvm::BasicBlock* true_bb, llvm::BasicBlock* false_bb,
                                 BranchTakenExpectation expect) {
    switch (expect) {
      case kLikely:
        return CreateCondBr(cond, true_bb, false_bb, mdb_->BranchLikely());
      case kUnlikely:
        return CreateCondBr(cond, true_bb, false_bb, mdb_->BranchUnlikely());
      default:
        return CreateCondBr(cond, true_bb, false_bb, NULL);
    }
  }

  // Get the meta-data builder.
  ArtMDBuilder* Mdb() const {
    return mdb_;
  }

 protected:
  // Creates a return statement when an exception is known to be pending.
  void CreateExceptionReturn() {
    llvm::Type* ret_type = getCurrentFunctionReturnType();
    if (ret_type->isVoidTy()) {
      CreateRetVoid();
    } else {
      // The return value is ignored when there's an exception. Use LLVM undef value for brevity.
      CreateRet(llvm::UndefValue::get(ret_type));
    }
  }

  // Branch with profile meta-data.
  llvm::BranchInst* CreateCondBr(llvm::Value *cond,
                                 llvm::BasicBlock* true_bb,
                                 llvm::BasicBlock* false_bb,
                                 llvm::MDNode* branch_weight) {
    llvm::BranchInst* branch_inst = LlvmIRBuilder::CreateCondBr(cond, true_bb, false_bb);
    if (branch_weight != NULL) {
      branch_inst->setMetadata(llvm::LLVMContext::MD_prof, branch_weight);
    }
    return branch_inst;
  }

  llvm::Value* SignOrZeroExtendCat1Types(llvm::Value* value, Primitive::Type type) {
    switch (type) {
      case Primitive::kPrimBoolean:
      case Primitive::kPrimChar:
        return CreateZExt(value, GetJavaType(type));
      case Primitive::kPrimByte:
      case Primitive::kPrimShort:
        return CreateSExt(value, GetJavaType(type));
      case Primitive::kPrimVoid:
      case Primitive::kPrimInt:
      case Primitive::kPrimLong:
      case Primitive::kPrimFloat:
      case Primitive::kPrimDouble:
      case Primitive::kPrimNot:
        return value;  // Nothing to do.
      default:
        LOG(FATAL) << "Unknown primitive type: " << type;
        return NULL;
    }
  }

  llvm::Value* TruncateCat1Types(llvm::Value* value, Primitive::Type type) {
    switch (type) {
      case Primitive::kPrimBoolean:
      case Primitive::kPrimChar:
      case Primitive::kPrimByte:
      case Primitive::kPrimShort:
        return CreateTrunc(value, GetJavaType(type));
      case Primitive::kPrimVoid:
      case Primitive::kPrimInt:
      case Primitive::kPrimLong:
      case Primitive::kPrimFloat:
      case Primitive::kPrimDouble:
      case Primitive::kPrimNot:
        return value;  // Nothing to do.
      default:
        LOG(FATAL) << "Unknown primitive type: " << type;
        return NULL;
    }
  }

  // Create a load from an Object of the given type and with a TBAA hint.
  llvm::LoadInst* LoadFromObjectOffset(llvm::Value* object_addr,
                                       MemberOffset offset,
                                       llvm::Type* type,
                                       llvm::MDNode* tbaa_info) {
    return CreateLoad(CreateObjectFieldPtr(object_addr, offset, type), tbaa_info);
  }

  // Create a load from the current Thread of the given type setting the TBAA hint.
  llvm::Value* LoadFromThreadOffset(ThreadOffset offset, llvm::Type* type) {
    return rsb_->LoadFromThreadOffset(offset, type, mdb_->GetTbaaForThread());
  }

  // Create a store to an Object of the given type and with a TBAA hint.
    void StoreToObjectOffset(llvm::Value* object_addr,
                             MemberOffset offset,
                             llvm::Value* value,
                             llvm::Type* type,
                             llvm::MDNode* tbaa_info) {
    CreateStore(value, CreateObjectFieldPtr(object_addr, offset, type), tbaa_info);
  }

  // Create a store to the current Thread of the given type setting the TBAA hint.
  void StoreToThreadOffset(ThreadOffset offset, llvm::Value* value) {
    rsb_->StoreToThreadOffset(offset, value, mdb_->GetTbaaForThread());
  }

  // Create a pointer to number_of_vregs_ within the shadow frame.
  llvm::Value* GetShadowFrameNumberOfVregsPtr() {
    llvm::Value* gep_index[] = {
        getInt32(0), // No pointer displacement.
        getInt32(0), // Header.
        getInt32(0)  // number_of_vregs_.
    };
    llvm::AllocaInst* shadow_frame = GetShadowFrame();
    return CreateGEP(shadow_frame, gep_index);
  }

  // Create a pointer to link_ within the shadow frame.
  llvm::Value* GetShadowFrameLinkPtr() {
    llvm::Value* gep_index[] = {
        getInt32(0), // No pointer displacement.
        getInt32(0), // Header.
        getInt32(1)  // link_.
    };
    llvm::AllocaInst* shadow_frame = GetShadowFrame();
    return CreateGEP(shadow_frame, gep_index);
  }

  // Create a pointer to the method_ within the shadow frame.
  llvm::Value* GetShadowFrameMethodPtr() {
    llvm::Value* gep_index[] = {
        getInt32(0), // No pointer displacement.
        getInt32(0), // Header.
        getInt32(2)  // method_.
    };
    llvm::AllocaInst* shadow_frame = GetShadowFrame();
    return CreateGEP(shadow_frame, gep_index);
  }

  // Get or create a pointer to the dex_pc_ within the shadow frame.
  llvm::Value* GetShadowFrameDexPcPtr() {
    llvm::Value* result = dex_pc_ptr_;
    if (result == NULL) {
      InsertPoint saved_ip = saveIP();
      SetInsertPoint(shadow_frame_->getNextNode());
      llvm::Value* gep_index[] = {
          getInt32(0), // No pointer displacement.
          getInt32(0), // Header.
          getInt32(3)  // Dex PC.
      };
      llvm::AllocaInst* shadow_frame = GetShadowFrame();
      result = CreateGEP(shadow_frame, gep_index);
      dex_pc_ptr_ = result;
      restoreIP(saved_ip);
    }
    return result;
  }

  // Check that all vregs have been flushed into the shadow frame.
  void AssertShadowFrameIsPushedAndVRegsAreFlushed() {
    DCHECK(old_shadow_frame_ != NULL);
    for (uint32_t i = 0; i < num_vregs_; ++ i) {
      DCHECK(cur_vreg_vals_[i] == NULL);
    }
  }

  // Get the shadow frame that vregs are stored into for GC and debugging. If a shadow frame hasn't
  // been created then one is created in the entry block. We don't push the shadow frame nor perform
  // a stack overflow check, that is done in EnsureShadowFrameIsPushed.
  llvm::AllocaInst* GetShadowFrame();

  // Called when a safe-point is encountered. We need to make sure that Thread::Current() has this
  // shadow frame at the top of the stack, we also need to record the old top of stack to restore
  // on exit.
  void EnsureShadowFrameIsPushed();

  static llvm::PointerType* GetPointerToNamedOpaqueStructType(llvm::Module* module,
                                                              const char* name);

  ArtIRBuilder(llvm::Module* module, ArtMDBuilder* mdb, uint32_t num_vregs,
               InstructionSet instruction_set);

  // The module we're generating code into.
  llvm::Module* const module_;

  // Meta-data builder for creating branch weight and TBAA nodes. The meta-data builder's
  // creation/removal is managed by the compiler driver.
  ArtMDBuilder* const mdb_;

  // Helper that creates calls to runtime support. One is allocated per IR builder and is destroyed
  // when the IR builder is destroyed.
  const UniquePtr<RuntimeSupportBuilder> rsb_;

  llvm::PointerType* const java_object_type_;  // The type for art::mirror::Object*.
  llvm::PointerType* const java_method_type_;  // The type for art::mirror::AbstractMethod*.
  llvm::PointerType* const thread_type_;  // The type for art::Thread*.

  // Number of dalvik registers for dex registers, or slots used to hold reference arguments for
  // JNI methods.
  uint32_t num_vregs_;

  // The shadow frame that will be stored into to enable GC and debugging.
  llvm::AllocaInst* shadow_frame_;

  // An alloca and stored to value that holds the shadow frame before this function's shadow frame
  // was pushed. This value is used in the link field of shadow_frame and must be restored when the
  // function exits.
  // TODO: why is this value alloca-ed? It is inherently SSA as it is only stored into once on
  //       method entry.
  llvm::AllocaInst* old_shadow_frame_;

  // Insert points where shadow frame pops have occurred, so we can be lazy yet fix up if push is
  // added.
  std::vector<llvm::IRBuilderBase::InsertPoint> remembered_pop_locations_;

  // Values that need writing back to shadow frame vregs by a flush.
  UniquePtr<llvm::Value*[]> cur_vreg_vals_;
  // Lazily created pointers into the shadow frame's vregs.
  UniquePtr<llvm::Value*[]> vreg_ptrs_;

  // Lazily created pointer to the shadow frame's dex_pc_.
  llvm::Value* dex_pc_ptr_;

};

}  // namespace Llvm
}  // namespace compiler
}  // namespace art

#endif  // ART_SRC_COMPILER_LLVM_ART_IR_BUILDER_H_
