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

#include "runtime_support_builder_x86.h"

#include "base/stringprintf.h"
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

llvm::Value* RuntimeSupportBuilderX86::GetCurrentThread() {
  llvm::Function* func_decl = GetRuntimeSupportFunction(kGetCurrentThread);
  std::string inline_asm(StringPrintf("mov %%fs:%d, $0", Thread::SelfOffset().Int32Value()));
  llvm::InlineAsm* func = llvm::InlineAsm::get(func_decl->getFunctionType(),
                                               inline_asm,
                                               "=r",
                                               /* hasSideEffects= */ false);
  llvm::CallInst* result = irb_->CreateCall(func);
  result->setOnlyReadsMemory();
  return result;
}

llvm::Value* RuntimeSupportBuilderX86::LoadFromThreadOffset(ThreadOffset offset,
                                                            llvm::Type* type,
                                                            llvm::MDNode* tbaa_info) {
  llvm::FunctionType* func_ty = llvm::FunctionType::get(/*Result=*/type,
                                                        /*isVarArg=*/false);
  std::string inline_asm(StringPrintf("mov %%fs:%d, $0", offset.Int32Value()));
  llvm::InlineAsm* func = llvm::InlineAsm::get(func_ty, inline_asm, "=r", true);
  llvm::CallInst* result = irb_->CreateCall(func);
  result->setOnlyReadsMemory();
  if (tbaa_info != NULL) {
    result->setMetadata(llvm::LLVMContext::MD_tbaa, tbaa_info);
  }
  return result;
}

void RuntimeSupportBuilderX86::StoreToThreadOffset(ThreadOffset offset, llvm::Value* value,
                                                   llvm::MDNode* tbaa_info) {
  llvm::FunctionType* func_ty = llvm::FunctionType::get(/* Result= */ irb_->getVoidTy(),
                                                        /* Params= */ value->getType(),
                                                        /* isVarArg= */ false);
  std::string inline_asm(StringPrintf("mov $0, %%fs:%d", offset.Int32Value()));
  llvm::InlineAsm* func = llvm::InlineAsm::get(func_ty, inline_asm, "r", true);
  llvm::CallInst* call_inst = irb_->CreateCall(func, value);
  if (tbaa_info != NULL) {
    call_inst->setMetadata(llvm::LLVMContext::MD_tbaa, tbaa_info);
  }
}

llvm::Value* RuntimeSupportBuilderX86::SetCurrentThread(llvm::Value*) {
  /* Nothing to be done. */
  return llvm::UndefValue::get(irb_->GetThreadTy());
}


} // namespace Llvm
} // namespace compiler
} // namespace art
