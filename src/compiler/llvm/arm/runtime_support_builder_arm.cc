/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include "runtime_support_builder_arm.h"

#include "compiler/llvm/art_ir_builder.h"
#include "thread.h"

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>

#include <vector>

namespace art {
namespace compiler {
namespace Llvm {

static char LDRSTRSuffixByType(ArtIRBuilder* irb, llvm::Type* type) {
  int width = type->isPointerTy() ? kBitsPerWord
                                  : llvm::cast<llvm::IntegerType>(type)->getBitWidth();
  switch (width) {
    case 8:  return 'b';
    case 16: return 'h';
    case 32: return ' ';
    default:
      LOG(FATAL) << "Unsupported width: " << width;
      return ' ';
  }
}

llvm::Value* RuntimeSupportBuilderARM::GetCurrentThread() {
  llvm::Function* func_decl = GetRuntimeSupportFunction(kGetCurrentThread);
  llvm::InlineAsm* func = llvm::InlineAsm::get(func_decl->getFunctionType(),
                                               "mov $0, r9", "=r",
                                               /* hasSideEffects= */ false);
  return irb_->CreateCall(func);
}

llvm::Value* RuntimeSupportBuilderARM::LoadFromThreadOffset(ThreadOffset offset,
                                                            llvm::Type* type,
                                                            llvm::MDNode* tbaa_info) {
  llvm::FunctionType* func_ty = llvm::FunctionType::get(/* Result= */ type,
                                                        /* isVarArg= */ false);
  std::string inline_asm(StringPrintf("ldr%c $0, [r9, #%d]",
                                      LDRSTRSuffixByType(irb_, type),
                                      offset.Int32Value()));
  llvm::InlineAsm* func = llvm::InlineAsm::get(func_ty, inline_asm, "=r", true);
  llvm::CallInst* result = irb_->CreateCall(func);
  result->setOnlyReadsMemory();
  if (tbaa_info != NULL) {
    result->setMetadata(llvm::LLVMContext::MD_tbaa, tbaa_info);
  }
  return result;
}

void RuntimeSupportBuilderARM::StoreToThreadOffset(ThreadOffset offset, llvm::Value* value,
                                                   llvm::MDNode* tbaa_info) {
  llvm::FunctionType* func_ty = llvm::FunctionType::get(/* Result= */ irb_->getVoidTy(),
                                                        /* Params= */ value->getType(),
                                                        /* isVarArg= */ false);
  std::string inline_asm(StringPrintf("str%c $0, [r9, #%d]",
                                      LDRSTRSuffixByType(irb_, value->getType()),
                                      offset.Int32Value()));
  llvm::InlineAsm* func = llvm::InlineAsm::get(func_ty, inline_asm, "r",
                                               /* hasSideEffects= */ true);
  llvm::CallInst* call_inst = irb_->CreateCall(func, value);
  if (tbaa_info != NULL) {
    call_inst->setMetadata(llvm::LLVMContext::MD_tbaa, tbaa_info);
  }
}

::llvm::Value* RuntimeSupportBuilderARM::SetCurrentThread(::llvm::Value* thread) {
  // Separate to two InlineAsm: The first one produces the return value, while the second,
  // sets the current thread.
  // LLVM can delete the first one if the caller in LLVM IR doesn't use the return value.
  //
  // Here we don't call EmitGetCurrentThread, because we mark it as DoesNotAccessMemory and
  // ConstJObject. We denote side effect to "true" below instead, so LLVM won't
  // reorder these instructions incorrectly.
  llvm::Function* func_decl = GetRuntimeSupportFunction(kGetCurrentThread);
  llvm::InlineAsm* func = llvm::InlineAsm::get(func_decl->getFunctionType(),
                                               "mov $0, r9",
                                               "=r",
                                               true);
  llvm::CallInst* old_thread_register = irb_->CreateCall(func);
  old_thread_register->setOnlyReadsMemory();

  llvm::FunctionType* func_ty = llvm::FunctionType::get(/* Result= */ irb_->getVoidTy(),
                                                        /* Params= */ irb_->GetThreadTy(),
                                                        /* isVarArg= */ false);
  func = llvm::InlineAsm::get(func_ty, "mov r9, $0", "r", true);
  irb_->CreateCall(func, thread);
  return old_thread_register;
}

}  // namespace Llvm
}  // namespace compiler
}  // namespace art
