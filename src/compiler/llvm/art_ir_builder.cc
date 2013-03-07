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

#include "art_ir_builder.h"

#include "base/stringprintf.h"
#include "thread.h"

#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>

namespace art {
namespace compiler {
namespace Llvm {

llvm::StructType* ArtIRBuilder::GetShadowFrameTy() {
  llvm::StructType* result = NULL;
  if (shadow_frame_ != NULL) {
    llvm::Type* found = shadow_frame_->getAllocatedType();
    CHECK(llvm::isa<llvm::StructType>(found));
    result = static_cast<llvm::StructType*>(found);
  } else {
    std::string name(StringPrintf("shadow_frame_%u", num_vregs_));
    // Try to find the existing struct type definition.
    llvm::Type* found = module_->getTypeByName(name);
    if (found != NULL) {
      CHECK(llvm::isa<llvm::StructType>(found));
      result = static_cast<llvm::StructType*>(found);
    } else {
      // Create our type so that we may reference it as a pointer.
      result = llvm::StructType::create(module_->getContext(), name);
      llvm::Type* int32_type = getInt32Ty();  // LLVM has no notion of unsigned integers.
      // Get or create the header portion of the type.
      llvm::StructType* sf_header_type = module_->getTypeByName("shadow_frame_header");
      if (sf_header_type == NULL) {
        sf_header_type = llvm::StructType::create(module_->getContext(), "shadow_frame_header");
        std::vector<llvm::Type*> sf_header_field_types;
        // GEP 0,0,0 - uint32_t number_of_vregs_;
        sf_header_field_types.push_back(int32_type);
        // GEP 0,0,1 - ShadowFrame* link_;
        sf_header_field_types.push_back(llvm::PointerType::get(result, 0));
        // GEP 0,0,2 - mirror::AbstractMethod* method_;
        sf_header_field_types.push_back(GetJavaMethodTy());
        // GET 0,0,3 - uint32_t dex_pc_;
        sf_header_field_types.push_back(int32_type);
        DCHECK(sf_header_type->isOpaque());
        sf_header_type->setBody(sf_header_field_types, /*isPacked=*/false);
      }
      DCHECK(!sf_header_type->isOpaque());
      // GEP 0,1,[0...num_vregs_] - uint32_t vregs_[num_vregs_];
      std::vector<llvm::Type*> sf_field_types;
      sf_field_types.push_back(sf_header_type);
      llvm::ArrayType* vregs_type = llvm::ArrayType::get(int32_type, num_vregs_);
      sf_field_types.push_back(vregs_type);
      DCHECK(result->isOpaque());
      result->setBody(sf_field_types, /*isPacked=*/false);
    }
  }
  DCHECK(!result->isOpaque());
  return result;
}

llvm::ConstantPointerNull* ArtIRBuilder::GetJavaNull() const {
  return llvm::ConstantPointerNull::get(GetJavaObjectTy());
}

llvm::Value* ArtIRBuilder::LoadCurMethod() const {
  return GetLlvmFunction()->arg_begin();
}

llvm::AllocaInst* ArtIRBuilder::GetShadowFrame() {
  llvm::AllocaInst* result = shadow_frame_;
  if (result == NULL) {
    // Insert the alloca instructions at entry so that we don't require a frame base pointer.
    InsertPoint saved_ip = saveIP();
    llvm::StructType* shadow_frame_type = GetShadowFrameTy();
    llvm::BasicBlock& entry_block = GetLlvmFunction()->front();
    SetInsertPoint(&entry_block.front());
    result = CreateAlloca(shadow_frame_type);
    shadow_frame_ = result;
    restoreIP(saved_ip);
  }
  return result;
}

void ArtIRBuilder::EnsureShadowFrameIsPushed() {
  if (old_shadow_frame_ != NULL) {
    // Shadow frame was already pushed.
  }
  llvm::IRBuilderBase::InsertPoint saved_ip = saveIP();
  // Insert the alloca instructions at entry so that we don't require a frame base pointer.
  llvm::StructType* shadow_frame_type = GetShadowFrameTy();
  llvm::BasicBlock& entry_block = GetLlvmFunction()->front();
  if (shadow_frame_ == NULL) {
    SetInsertPoint(&entry_block.front());
    shadow_frame_ = CreateAlloca(shadow_frame_type);
  } else {
    // We've already created the shadow_frame_ alloca so starting inserting after it.
    SetInsertPoint(shadow_frame_->getNextNode());
  }
  old_shadow_frame_ = CreateAlloca(shadow_frame_type->getElementType(0)->getPointerTo());

  // Create a stack-overflow test following the frame creation.
  llvm::Function* frameaddress =
      llvm::Intrinsic::getDeclaration(module_, llvm::Intrinsic::frameaddress);
  // The type of llvm::frameaddress is: i8* @llvm.frameaddress(i32)
  llvm::Value* frame_address = CreateCall(frameaddress, getInt32(0));
  // Get Thread::Current()->stack_end_
  llvm::Value* stack_end = LoadFromThreadOffset(art::Thread::StackEndOffset(),
                                                getInt8Ty()->getPointerTo());
  // Stack overflow when: frame address < thread.stack_end_
  llvm::Value* is_stack_overflow = CreateICmpULT(frame_address, stack_end);
  // Create stack overflow block and split the entry block into the check and its continuation.
  llvm::BasicBlock* bb_throw_soe = CreateBasicBlock("stack_overflow");
  llvm::BasicBlock* bb_cont = entry_block.splitBasicBlock(GetInsertPoint(),
                                                          "stack_overflow_cont");
  // Replace the entry block branch with a new branch that checks for overflow.
  llvm::BranchInst* soe_branch =
      llvm::BranchInst::Create(bb_throw_soe, bb_cont, is_stack_overflow);
  soe_branch->setMetadata(llvm::LLVMContext::MD_prof, mdb_->BranchUnlikely());
  entry_block.getTerminator()->replaceAllUsesWith(soe_branch);

  // Create throw exception in the throw block.
  SetInsertPoint(bb_throw_soe);
  rsb_->ThrowStackOverflowError();
  CreateExceptionReturn();

  // We have a valid stack, now set up the shadow frame and perform the push.
  SetInsertPoint(bb_cont);
  llvm::Value* link = LoadFromThreadOffset(Thread::TopShadowFrameOffset(),
                                           GetShadowFrameTy()->getPointerTo());
  CreateStore(old_shadow_frame_, link, mdb_->GetTbaaForRandomAllocaVariable(false));
  CreateStore(getInt32(num_vregs_), GetShadowFrameNumberOfVregsPtr(),
              mdb_->GetTbaaForShadowFrameNumberOfVRegs());
  CreateStore(LoadCurMethod(), GetShadowFrameMethodPtr(), mdb_->GetTbaaForShadowFrameMethod());
  CreateStore(link, GetShadowFrameLinkPtr(), mdb_->GetTbaaForShadowFrameLink());
  StoreToThreadOffset(Thread::TopShadowFrameOffset(), shadow_frame_);
  while (remembered_pop_locations_.size() > 0) {
    llvm::IRBuilderBase::InsertPoint pop_ip = remembered_pop_locations_.back();
    remembered_pop_locations_.pop_back();
    restoreIP(pop_ip);
    llvm::Value* link = CreateLoad(old_shadow_frame_,
                                   mdb_->GetTbaaForRandomAllocaVariable(false));
    StoreToThreadOffset(Thread::TopShadowFrameOffset(), link);
  }
  restoreIP(saved_ip);
}

void ArtIRBuilder::PopShadowFrame() {
  if (old_shadow_frame_ != NULL) {
    llvm::Value* link = CreateLoad(old_shadow_frame_,
                                   mdb_->GetTbaaForRandomAllocaVariable(false));
    StoreToThreadOffset(Thread::TopShadowFrameOffset(), link);
  } else {
    remembered_pop_locations_.push_back(saveIP());
  }
}

llvm::PointerType* ArtIRBuilder::GetPointerToNamedOpaqueStructType(llvm::Module* module,
                                                                   const char* name) {
  llvm::StructType *type = module->getTypeByName(name);
  if (type == NULL) {
    type = llvm::StructType::create(module->getContext(), name);
  }
  return type->getPointerTo();
}

ArtIRBuilder::ArtIRBuilder(llvm::Module* module, ArtMDBuilder* mdb, uint32_t num_vregs,
                           InstructionSet instruction_set)
    : LlvmIRBuilder(module->getContext()),
      module_(module),
      mdb_(mdb),
      rsb_(RuntimeSupportBuilder::Create(this, instruction_set)),
      java_object_type_(GetPointerToNamedOpaqueStructType(module, "Ljava/lang/Object;")),
      java_method_type_(GetPointerToNamedOpaqueStructType(module, "Ljava/lang/reflect/AbstractMethod;")),
      thread_type_(GetPointerToNamedOpaqueStructType(module, "art::Thread")),
      num_vregs_(num_vregs),
      shadow_frame_(NULL), old_shadow_frame_(NULL), dex_pc_ptr_(NULL) {}

}  // namespace Llvm
}  // namespace compiler
}  // namespace art
