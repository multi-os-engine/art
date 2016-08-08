/*
 * Copyright (C) 2011 The Android Open Source Project
 * Copyright 2014-2016 Intel Corporation
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

#include "llvm_stub_builder.hpp"

#include "llvm/IR/Module.h"
#include "llvm/ADT/TinyPtrVector.h"

#include "base/logging.h"
#include "art_method.h"
#include "thread.h"

#include "llvm_compiler.hpp"
#include "llvm_shadow_frame_builder.hpp"

LLVMStubBuilder::LLVMStubBuilder(llvm::CallingConv::ID jni_calling_convention, bool is_64bit, bool is_windows)
    : jni_calling_convention_(jni_calling_convention), is_64bit_(is_64bit), is_windows_(is_windows) {
    // Cache llvm contexts.
    ref_context_ = new llvm::LLVMContext;
    jni_context_ = new llvm::LLVMContext;
    res_context_ = new llvm::LLVMContext;
    int_context_ = new llvm::LLVMContext;

    // Cache llvm modules.
    ref_module_ = new llvm::Module("reflection bridges", *ref_context_);
    jni_module_ = new llvm::Module("jni bridges", *jni_context_);
    res_module_ = new llvm::Module("resolution trampolines", *res_context_);
    int_module_ = new llvm::Module("interpreter bridges", *int_context_);

    // TwoWordReturn
    res_ret_ty_ =
        llvm::StructType::get(*res_context_, llvm::SmallVector<llvm::Type*, 2> {
      llvm::Type::getInt8PtrTy(*res_context_),
      llvm::Type::getInt8PtrTy(*res_context_)
    }, false);

    // void EnterInterpreterFromInvoke(Thread*, ArtMethod*, uintptr_t*, JValue*)
    llvm::FunctionType* enter_int_ty =
        llvm::FunctionType::get(llvm::Type::getVoidTy(*int_context_),
                                llvm::SmallVector<llvm::Type*, 4> {
      llvm::Type::getInt8PtrTy(*int_context_),
      llvm::Type::getInt8PtrTy(*int_context_),
      llvm::Type::getInt8PtrTy(*int_context_),
      llvm::Type::getInt8PtrTy(*int_context_)
    }, false);

    // void* artGetNativeCode(Thread*, ArtMethod*)
    llvm::FunctionType* find_nat_ty =
        llvm::FunctionType::get(llvm::Type::getInt8PtrTy(*jni_context_),
                                llvm::SmallVector<llvm::Type*, 2> {
      llvm::Type::getInt8PtrTy(*jni_context_),
      llvm::Type::getInt8PtrTy(*jni_context_)
    }, false);

    // void artDeliverPendingExceptionFromCode(Thread*)
    llvm::FunctionType* deliver_exc_ty =
        llvm::FunctionType::get(llvm::Type::getVoidTy(*jni_context_),
                                llvm::SmallVector<llvm::Type*, 1> {
      llvm::Type::getInt8PtrTy(*jni_context_)
    }, false);

    // void artDeliverPendingExceptionFromCode(Thread*)
    llvm::FunctionType* deliver_exc_2_ty =
        llvm::FunctionType::get(llvm::Type::getVoidTy(*res_context_),
                                llvm::SmallVector<llvm::Type*, 1> {
      llvm::Type::getInt8PtrTy(*res_context_)
    }, false);

    // ArtMethod* GetResolutionMethod()
    llvm::FunctionType* res_get_res_method_ty =
        llvm::FunctionType::get(llvm::Type::getInt8PtrTy(*res_context_), false);

    // TwoWordReturn artQuickResolutionTrampoline(ArtMethod*, Object*, Thread*)
    llvm::FunctionType* res_tramp_ty =
        llvm::FunctionType::get(res_ret_ty_,
                                llvm::SmallVector<llvm::Type*, 3> {
      llvm::Type::getInt8PtrTy(*res_context_),
      llvm::Type::getInt8PtrTy(*res_context_),
      llvm::Type::getInt8PtrTy(*res_context_)
    }, false);

    // uint32_t JniMethodStart(Thread*)
    llvm::FunctionType* jni_start_ty =
        llvm::FunctionType::get(llvm::Type::getInt32Ty(*jni_context_),
                                llvm::SmallVector<llvm::Type*, 1> {
      llvm::Type::getInt8PtrTy(*jni_context_)
    }, false);

    // uint32_t JniMethodStartSynchronized(jobject, Thread*)
    llvm::FunctionType* jni_start_synch_ty =
        llvm::FunctionType::get(llvm::Type::getInt32Ty(*jni_context_),
                                llvm::SmallVector<llvm::Type*, 2> {
      llvm::Type::getInt8PtrTy(*jni_context_),
      llvm::Type::getInt8PtrTy(*jni_context_)
    }, false);

    // void JniMethodEnd(uint32_t, Thread*)
    llvm::FunctionType* jni_end_ty =
        llvm::FunctionType::get(llvm::Type::getVoidTy(*jni_context_),
                                llvm::SmallVector<llvm::Type*, 2> {
      llvm::Type::getInt32Ty(*jni_context_),
      llvm::Type::getInt8PtrTy(*jni_context_)
    }, false);

    // void JniMethodEndSynchronized(uint32_t, jobject, Thread*)
    llvm::FunctionType* jni_end_synch_ty =
        llvm::FunctionType::get(llvm::Type::getVoidTy(*jni_context_),
                                llvm::SmallVector<llvm::Type*, 3> {
      llvm::Type::getInt32Ty(*jni_context_),
      llvm::Type::getInt8PtrTy(*jni_context_),
      llvm::Type::getInt8PtrTy(*jni_context_)
    }, false);

    // Object* JniMethodEndWithReference(jobject, uint32_t, Thread*)
    llvm::FunctionType* jni_end_ref_ty =
        llvm::FunctionType::get(llvm::Type::getInt8PtrTy(*jni_context_),
                                llvm::SmallVector<llvm::Type*, 3> {
      llvm::Type::getInt8PtrTy(*jni_context_),
      llvm::Type::getInt32Ty(*jni_context_),
      llvm::Type::getInt8PtrTy(*jni_context_)
    }, false);

    // Object* JniMethodEndWithReferenceSynchronized(jobject, uint32_t, jobject, Thread*)
    llvm::FunctionType* jni_end_ref_synch_ty =
        llvm::FunctionType::get(llvm::Type::getInt8PtrTy(*jni_context_),
                                llvm::SmallVector<llvm::Type*, 4> {
      llvm::Type::getInt8PtrTy(*jni_context_),
      llvm::Type::getInt32Ty(*jni_context_),
      llvm::Type::getInt8PtrTy(*jni_context_),
      llvm::Type::getInt8PtrTy(*jni_context_)
    }, false);

    enter_int_ =
        llvm::Function::Create(enter_int_ty,
                               is_windows_ ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::ExternalWeakLinkage,
                               "EnterInterpreterFromInvoke", int_module_);
    if (is_windows_) {
      enter_int_->setDLLStorageClass(llvm::Function::DLLImportStorageClass);
    }

    find_nat_ =
        llvm::Function::Create(find_nat_ty,
                                is_windows_ ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::ExternalWeakLinkage,
                               "artFindNativeMethod", jni_module_);
    if (is_windows_) {
      find_nat_->setDLLStorageClass(llvm::Function::DLLImportStorageClass);
    }

    deliver_exc_ =
        llvm::Function::Create(deliver_exc_ty,
                               is_windows_ ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::ExternalWeakLinkage,
                               "artDeliverPendingExceptionFromCode", jni_module_);
    deliver_exc_->setDoesNotReturn();
    if (is_windows_) {
      deliver_exc_->setDLLStorageClass(llvm::Function::DLLImportStorageClass);
    }

    deliver_exc_2_ =
        llvm::Function::Create(deliver_exc_2_ty,
                               is_windows_ ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::ExternalWeakLinkage,
                               "artDeliverPendingExceptionFromCode", res_module_);
    deliver_exc_2_->setDoesNotReturn();
    if (is_windows_) {
      deliver_exc_2_->setDLLStorageClass(llvm::Function::DLLImportStorageClass);
    }

    res_get_res_method_ = llvm::Function::Create(res_get_res_method_ty,
                               is_windows_ ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::ExternalWeakLinkage,
                               "GetResolutionMethod", res_module_);
    if (is_windows_) {
      res_get_res_method_->setDLLStorageClass(llvm::Function::DLLImportStorageClass);
    }

    res_tramp_ =
        llvm::Function::Create(res_tramp_ty,
                               is_windows_ ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::ExternalWeakLinkage,
                               "artQuickResolutionTrampoline", res_module_);
    if (is_windows_) {
      res_tramp_->setDLLStorageClass(llvm::Function::DLLImportStorageClass);
    }

    jni_start_ =
        llvm::Function::Create(jni_start_ty,
                               is_windows_ ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::ExternalWeakLinkage,
                               "JniMethodStart", jni_module_);
    if (is_windows_) {
      jni_start_->setDLLStorageClass(llvm::Function::DLLImportStorageClass);
    }

    jni_start_synch_ =
        llvm::Function::Create(jni_start_synch_ty,
                               is_windows_ ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::ExternalWeakLinkage,
                               "JniMethodStartSynchronized", jni_module_);
    if (is_windows_) {
      jni_start_synch_->setDLLStorageClass(llvm::Function::DLLImportStorageClass);
    }

    jni_end_ =
        llvm::Function::Create(jni_end_ty,
                               is_windows_ ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::ExternalWeakLinkage,
                               "JniMethodEnd", jni_module_);
    if (is_windows_) {
      jni_end_->setDLLStorageClass(llvm::Function::DLLImportStorageClass);
    }

    jni_end_synch_ =
        llvm::Function::Create(jni_end_synch_ty,
                               is_windows_ ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::ExternalWeakLinkage,
                               "JniMethodEndSynchronized", jni_module_);
    if (is_windows_) {
      jni_end_synch_->setDLLStorageClass(llvm::Function::DLLImportStorageClass);
    }

    jni_end_ref_ =
        llvm::Function::Create(jni_end_ref_ty,
                               is_windows_ ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::ExternalWeakLinkage,
                               "JniMethodEndWithReference", jni_module_);
    if (is_windows_) {
      jni_end_ref_->setDLLStorageClass(llvm::Function::DLLImportStorageClass);
    }

    jni_end_ref_synch_ =
        llvm::Function::Create(jni_end_ref_synch_ty,
                               is_windows_ ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::ExternalWeakLinkage,
                               "JniMethodEndWithReferenceSynchronized", jni_module_);
    if (is_windows_) {
      jni_end_ref_synch_->setDLLStorageClass(llvm::Function::DLLImportStorageClass);
    }
}

LLVMStubBuilder::~LLVMStubBuilder() {
    delete ref_module_;
    delete jni_module_;
    delete res_module_;
    delete int_module_;
    delete ref_context_;
    delete jni_context_;
    delete res_context_;
    delete int_context_;
}

llvm::Function* LLVMStubBuilder::ReflectionBridgeCompile(const char* shorty, bool is_static) const {
  // Non static reflection bridges are the same with static reflection bridges with one this
  // parameter at the beginning.
  uint32_t shorty_len = strlen(shorty);
  const char* old_shorty = shorty;
  if (!is_static) {
    char* new_shorty = (char*) alloca(shorty_len + 2);
    new_shorty[0] = shorty[0];
    new_shorty[1] = 'L';
    for (uint32_t i = 1; i < shorty_len; i++) {
      new_shorty[i + 1] = shorty[i];
    }
    new_shorty[shorty_len + 1] = 0;
    shorty = new_shorty;
    shorty_len += 1;
  }

  llvm::LLVMContext* context = ref_context_;
  llvm::Module* module = ref_module_;
  auto builder = llvm::IRBuilder<>(*context);

  // Create the callee function type.
  // First two implicit arguments are Thread* and ArtMethod*.
  llvm::Type* callee_ret = LLVMCompiler::GetLLVMType(context, shorty[0]);
  llvm::TinyPtrVector<llvm::Type*> callee_args;
  callee_args.push_back(llvm::Type::getInt8PtrTy(*context));
  callee_args.push_back(llvm::Type::getInt8PtrTy(*context));
  for (uint8_t i = 1; i < shorty_len; i++) {
    callee_args.push_back(LLVMCompiler::GetLLVMType(context, shorty[i]));
  }
  llvm::FunctionType *CalleeFT = llvm::FunctionType::get(callee_ret, callee_args, false);

  // Create the bridge method type.
  // First four arguments are Thread*, ArtMethod*, argument list and the result pointer.
  llvm::Type* arg_element_type = llvm::Type::getIntNTy(*context, is_64bit_ ? 64 : 32);
  llvm::Type* bridge_ret = llvm::Type::getVoidTy(*context);
  llvm::SmallVector<llvm::Type*, 4> bridge_args {
    llvm::Type::getInt8PtrTy(*context),
    llvm::Type::getInt8PtrTy(*context),
    arg_element_type->getPointerTo(),
    llvm::Type::getInt64Ty(*context)->getPointerTo()
  };
  llvm::FunctionType* FT = llvm::FunctionType::get(bridge_ret, bridge_args, false);

  // Create a new function from the bridge method type.
  // The naming pattern is MOE__RB_<is_static>_<shorty>.
  std::string bridge_name = "MOE__RB_";
  bridge_name += is_static ? "STATIC_" : "NOSTATIC_";
  bridge_name += old_shorty;
  llvm::Function* F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                                             bridge_name, module);
  if (is_windows_) {
    F->setDLLStorageClass(llvm::Function::DLLExportStorageClass);
  }

  llvm::BasicBlock *BB = llvm::BasicBlock::Create(*context, "entry", F);
  builder.SetInsertPoint(BB);

  auto arg_it = F->args().begin();
  llvm::Value* self = &*arg_it++;
  llvm::Value* method = &*arg_it++;
  llvm::Value* args = &*arg_it++;
  llvm::Value* result = &*arg_it;

  // Fill up the arguments from the array.
  llvm::Value* long_shift = llvm::ConstantInt::get(*context, llvm::APInt(64, 32, false));
  llvm::TinyPtrVector<llvm::Value*> target_args;
  target_args.push_back(self);
  target_args.push_back(method);
  llvm::ConstantInt* idx_0 = llvm::ConstantInt::get(*context, llvm::APInt(32, 0, false));
  for (uint32_t i = 1, slot = 0; i < shorty_len; i++, slot++) {
    llvm::Value* value;
    value = builder.CreateGEP(args, llvm::ConstantInt::get(*context,
                                                           llvm::APInt(32, slot, false)));
    llvm::Type* target_type = callee_args[i + 1];
    if (target_type->isIntegerTy(64) || target_type->isDoubleTy()) {
      llvm::Type* stored_ptr_type = llvm::Type::getInt32PtrTy(*context);
      llvm::Type* extended_type = llvm::Type::getInt64Ty(*context);
      llvm::Value* value_low =
          builder.CreateLoad(builder.CreatePointerCast(value, stored_ptr_type));
      slot++;
      value = builder.CreateGEP(args, llvm::ConstantInt::get(*context,
                                                           llvm::APInt(32, slot, false)));
      llvm::Value* value_high =
          builder.CreateLoad(builder.CreatePointerCast(value, stored_ptr_type));
      value_low = builder.CreateZExt(value_low, extended_type);
      value_high = builder.CreateZExt(value_high, extended_type);
      value = builder.CreateShl(value_high, long_shift);
      value = builder.CreateAdd(value_low, value);

      if (target_type->isDoubleTy()) {
        value = builder.CreateBitCast(value, target_type);
      }
    } else {
      value = builder.CreatePointerCast(value, target_type->getPointerTo());
      value = builder.CreateLoad(value);
    }
    target_args.push_back(value);
  }

  // Get the code that we need to call.
  uint32_t off;
  if (is_64bit_) {
    off = art::ArtMethod::EntryPointFromQuickCompiledCodeOffset(8).Int32Value();
  } else {
    off = art::ArtMethod::EntryPointFromQuickCompiledCodeOffset(4).Int32Value();
  }
  llvm::Value* code_off =
      llvm::ConstantInt::get(*context, llvm::APInt(32, off, false));
  llvm::Value* target = builder.CreateGEP(method, code_off);
  target = builder.CreateLoad(
      builder.CreatePointerCast(target, CalleeFT->getPointerTo()->getPointerTo()));

  // And finally, call the code.
  llvm::Value* value = builder.CreateCall(target, target_args);
  if (!callee_ret->isVoidTy()) {
    result = builder.CreatePointerCast(result, callee_ret->getPointerTo());
    builder.CreateStore(value, result);
  }

  builder.CreateRetVoid();

  return F;
}

llvm::Function* LLVMStubBuilder::ResolutionTrampolineCompile(const char* shorty, bool is_static) const {
  uint32_t shorty_len = strlen(shorty);
  llvm::LLVMContext* context = res_context_;
  llvm::Module* module = res_module_;
  auto builder = llvm::IRBuilder<>(*context);

  // Create the callee function type.
  // First two implicit arguments are Thread* and ArtMethod*.
  llvm::Type* ret = LLVMCompiler::GetLLVMType(context, shorty[0]);
  llvm::TinyPtrVector<llvm::Type*> args;
  args.push_back(llvm::Type::getInt8PtrTy(*context));
  args.push_back(llvm::Type::getInt8PtrTy(*context));
  if (!is_static) {
    args.push_back(llvm::Type::getInt8PtrTy(*context));
  }
  for (uint8_t i = 1; i < shorty_len; i++) {
    args.push_back(LLVMCompiler::GetLLVMType(context, shorty[i]));
  }
  llvm::FunctionType *CalleeFT = llvm::FunctionType::get(ret, args, false);

  // Create the trampoline method type.
  // First two implicit arguments are Thread* and ArtMethod*.
  llvm::FunctionType* FT = llvm::FunctionType::get(ret, args, false);

  // Create a new function from the trampoline method type.
  // The naming pattern is MOE__RT_<is_static>_<shorty>.
  std::string bridge_name = "MOE__RT_";
  bridge_name += is_static ? "STATIC_" : "NOSTATIC_";
  bridge_name += shorty;
  llvm::Function* F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                                             bridge_name, module);
  if (is_windows_) {
    F->setDLLStorageClass(llvm::Function::DLLExportStorageClass);
  }

  llvm::BasicBlock *BB = llvm::BasicBlock::Create(*context, "entry", F);
  builder.SetInsertPoint(BB);

  auto arg_it = F->args().begin();
  auto arg_it_end = F->args().end();
  llvm::Value* self = &*arg_it++;
  llvm::Value* method = &*arg_it++;
  
  // Use resolution runtime method for shadow frame.
  llvm::Value* runtime_method = builder.CreateCall(res_get_res_method_);

  // Build temporal shadow frame.
  LLVMShadowFrameBuilder shadow_frame_builder(context, &builder, self, runtime_method, is_64bit_);
  llvm::TinyPtrVector<llvm::Value*> references;
  for (; arg_it != arg_it_end; arg_it++) {
    llvm::Value* arg = &*arg_it;
    if (arg->getType()->isPointerTy()) {
      references.push_back(&*arg_it);
    }
  }
  shadow_frame_builder.buildFromReferences(references);

  // Fill up the trampoline arguments.
  llvm::SmallVector<llvm::Value*, 3> tramp_args;
  tramp_args.push_back(method);
  if (is_static) {
    tramp_args.push_back(llvm::ConstantPointerNull::get(llvm::Type::getInt8PtrTy(*context)));
  } else {
    tramp_args.push_back(&*arg_it);
  }
  tramp_args.push_back(self);

  // Call the resolution trampoline.
  llvm::Value* resolution = builder.CreateCall(res_tramp_, tramp_args);

  // If the resolution failed then it was probably because of an exception.
  llvm::Value* code = builder.CreateExtractValue(resolution, 1);
  llvm::BasicBlock* fail = llvm::BasicBlock::Create(*context, "resolution failed", F);
  llvm::BasicBlock* succ = llvm::BasicBlock::Create(*context, "resolution succeeded", F);
  builder.CreateCondBr(builder.CreateIsNull(code), fail, succ);
  builder.SetInsertPoint(fail);
  builder.CreateCall(deliver_exc_2_, llvm::SmallVector<llvm::Value*, 1>{ self });
  builder.CreateBr(succ);
  builder.SetInsertPoint(succ);

  // Use method from the resolution.
  method = builder.CreateExtractValue(resolution, 0);

  // Fill up the callee arguments.
  // Copy reference arguments from the temporal shadow frame.
  llvm::TinyPtrVector<llvm::Value*> target_args;
  target_args.push_back(self);
  target_args.push_back(method);
  arg_it = ++++F->args().begin();
  for (uint32_t ref = 0; arg_it != arg_it_end; arg_it++) {
    llvm::Value* arg = &*arg_it;
    if (arg_it->getType()->isPointerTy()) {
      arg = shadow_frame_builder.getVReg(ref);
      ref++;
    }
    target_args.push_back(arg);
  }

  // Re-link the previous shadow frame.
  shadow_frame_builder.relink();

  // Finally, do the actual call.
  llvm::Value* value =
      builder.CreateCall(builder.CreatePointerCast(code, CalleeFT->getPointerTo()), target_args);

  if (ret->isVoidTy()) {
    builder.CreateRetVoid();
  } else {
    builder.CreateRet(value);
  }

  return F;
}

llvm::Function* LLVMStubBuilder::InterpreterBridgeCompile(const char* shorty, bool is_static) const {
  uint32_t shorty_len = strlen(shorty);
  llvm::LLVMContext* context = int_context_;
  llvm::Module* module = int_module_;
  auto builder = llvm::IRBuilder<>(*context);

  // Create the bridge method type.
  // First two implicit arguments are Thread* and ArtMethod*.
  llvm::Type* ret = LLVMCompiler::GetLLVMType(context, shorty[0]);
  llvm::TinyPtrVector<llvm::Type*> args;
  args.push_back(llvm::Type::getInt8PtrTy(*context));
  args.push_back(llvm::Type::getInt8PtrTy(*context));
  if (!is_static) {
    args.push_back(llvm::Type::getInt8PtrTy(*context));
  }
  for (uint8_t i = 1; i < shorty_len; i++) {
    args.push_back(LLVMCompiler::GetLLVMType(context, shorty[i]));
  }
  llvm::FunctionType* FT = llvm::FunctionType::get(ret, args, false);

  // Create a new function from the bridge method type.
  // The naming pattern is MOE__IB_<is_static>_<shorty>.
  std::string bridge_name = "MOE__IB_";
  bridge_name += is_static ? "STATIC_" : "NOSTATIC_";
  bridge_name += shorty;
  llvm::Function* F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                                             bridge_name, module);
  if (is_windows_) {
    F->setDLLStorageClass(llvm::Function::DLLExportStorageClass);
  }

  llvm::BasicBlock *BB = llvm::BasicBlock::Create(*context, "entry", F);
  builder.SetInsertPoint(BB);

  auto arg_it = F->args().begin();
  auto arg_it_end = F->args().end();
  llvm::Value* self = &*arg_it++;
  llvm::Value* method = &*arg_it++;

  // Build argument only shadow frame.
  LLVMShadowFrameBuilder shadow_frame_builder(context, &builder, self, method, is_64bit_);
  llvm::TinyPtrVector<llvm::Value*> arguments;
  for (; arg_it != arg_it_end; arg_it++) {
    llvm::Value* arg = &*arg_it;
    arguments.push_back(&*arg_it);
  }
  llvm::Value* arg_array = shadow_frame_builder.buildArgumentOnlyFromValues(arguments);

  // Create return value holder.
  bool is_void = ret->isVoidTy();
  llvm::Value* result = is_void ?
      llvm::ConstantPointerNull::get(llvm::Type::getInt8PtrTy(*context)) :
      builder.CreatePointerCast(builder.CreateAlloca(llvm::Type::getInt64Ty(*context)),
                                llvm::Type::getInt8PtrTy(*context));

  // Fill up the handler arguments.
  llvm::SmallVector<llvm::Value*, 4> handler_args;
  handler_args.push_back(self);
  handler_args.push_back(method);
  handler_args.push_back(builder.CreatePointerCast(arg_array, llvm::Type::getInt8PtrTy(*context)));
  handler_args.push_back(result);

  // Call the handler.
  builder.CreateCall(enter_int_, handler_args);

  // Return value from the holder.
  if (is_void) {
    builder.CreateRetVoid();
  } else {
    builder.CreateRet(builder.CreateLoad(builder.CreatePointerCast(result, ret->getPointerTo())));
  }

  return F;
}

llvm::Function* LLVMStubBuilder::JniBridgeCompile(const char* shorty, bool is_synchronized,
                                          bool is_static) const {
  uint32_t shorty_len = strlen(shorty);
  llvm::LLVMContext* context = jni_context_;
  llvm::Module* module = jni_module_;
  auto builder = llvm::IRBuilder<>(*context);

  // Create the callee function type.
  // First two implicit arguments are JNIEnv* and jobject/jclass.
  llvm::Type* ret = LLVMCompiler::GetLLVMType(context, shorty[0]);
  llvm::TinyPtrVector<llvm::Type*> args;
  args.push_back(llvm::Type::getInt8PtrTy(*context));
  args.push_back(llvm::Type::getInt8PtrTy(*context));
  for (uint8_t i = 1; i < shorty_len; i++) {
    args.push_back(LLVMCompiler::GetLLVMType(context, shorty[i]));
  }
  llvm::FunctionType *CalleeFT = llvm::FunctionType::get(ret, args, false);

  // Create the bridge method type.
  // First two implicit arguments are Thread* and ArtMethod*.
  llvm::TinyPtrVector<llvm::Type*> bridge_args(args);
  if (!is_static) {
    bridge_args.insert(bridge_args.begin(), llvm::Type::getInt8PtrTy(*context));
  }
  llvm::FunctionType* FT = llvm::FunctionType::get(ret, bridge_args, false);

  // Create a new function from the bridge method type.
  // The naming pattern is MOE__NB_<is_sync>_<is_static>_<shorty>.
  std::string bridge_name = "MOE__NB_";
  bridge_name += is_synchronized ? "SYNC_" : "NOSYNC_";
  bridge_name += is_static ? "STATIC_" : "NOSTATIC_";
  bridge_name += shorty;
  llvm::Function* F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                                             bridge_name, module);
  if (is_windows_) {
    F->setDLLStorageClass(llvm::Function::DLLExportStorageClass);
  }

  llvm::BasicBlock *BB = llvm::BasicBlock::Create(*context, "entry", F);
  builder.SetInsertPoint(BB);

  auto arg_it = F->args().begin();
  auto arg_it_end = F->args().end();
  llvm::Value* self = &*arg_it++;
  llvm::Value* method = &*arg_it++;

  // Build shadow frame.
  LLVMShadowFrameBuilder shadow_frame_builder(context, &builder, self, method, is_64bit_);
  llvm::TinyPtrVector<llvm::Value*> references;
  uint32_t off;
  if (is_static) {
    // Class* can be accessed by loading it from the ArtMethod*.
    if (is_static) {
      off = art::ArtMethod::DeclaringClassOffset().Int32Value();
      llvm::Value* decl_off = llvm::ConstantInt::get(*context, llvm::APInt(32, off, false));
      llvm::Value* decl = builder.CreateGEP(method, decl_off);
      decl = builder.CreatePointerCast(decl, llvm::Type::getInt8PtrTy(*context)->getPointerTo());
      decl = builder.CreateLoad(decl);
      references.push_back(decl);
    }
  }
  for (; arg_it != arg_it_end; arg_it++) {
    llvm::Value* arg = &*arg_it;
    if (arg->getType()->isPointerTy()) {
      references.push_back(&*arg_it);
    }
  }
  uint32_t num_refs = references.size() + (is_static ? 1 : 0);
  shadow_frame_builder.buildFromReferences(references);

  // Call JNI method start listener.
  llvm::SmallVector<llvm::Value*, 2> jni_start_args;
  llvm::Function* jni_start;
  if (is_synchronized) {
    jni_start = jni_start_synch_;
    jni_start_args = {
      builder.CreatePointerCast(shadow_frame_builder.getVRegRef(0),
                                llvm::Type::getInt8PtrTy(*context)),
      self
    };
  } else {
    jni_start = jni_start_;
    jni_start_args = { self };
  }
  llvm::Value* cookie = builder.CreateCall(jni_start, jni_start_args);

  // Get JNIEnv* from the Thread*.
  // Compute pointer to the current handle scope in the Thread*.
  llvm::Value* self_casted =
      builder.CreatePointerCast(self, llvm::Type::getInt8PtrTy(*context));
  if (is_64bit_) {
    off = art::Thread::JniEnvOffset<8>().Int32Value();
  } else {
    off = art::Thread::JniEnvOffset<4>().Int32Value();
  }
  llvm::Value* jnienv_off =
      llvm::ConstantInt::get(*context, llvm::APInt(32, off, false));
  llvm::Value* jnienv_tls = builder.CreateGEP(self_casted, jnienv_off);
  jnienv_tls = builder.CreatePointerCast(jnienv_tls,
      llvm::Type::getInt8PtrTy(*context)->getPointerTo());
  jnienv_tls = builder.CreateLoad(jnienv_tls);

  // Get the native code to call and if that is not present,
  // then find one by calling artFindNativeMethod. If it fails to find it,
  // then deliver the exception by calling artDeliverPendingExceptionFromCode.
  if (is_64bit_) {
    off = art::ArtMethod::EntryPointFromJniOffset(8).Int32Value();
  } else {
    off = art::ArtMethod::EntryPointFromJniOffset(4).Int32Value();
  }
  llvm::Value* code_off =
      llvm::ConstantInt::get(*context, llvm::APInt(32, off, false));
  llvm::Value* target_ptr = builder.CreateGEP(method, code_off);
  target_ptr = builder.CreatePointerCast(target_ptr,
                                         llvm::Type::getInt8PtrTy(*context)->getPointerTo());
  llvm::Value* target = builder.CreateLoad(target_ptr);
  llvm::Value* is_null = builder.CreateIsNull(target);
  llvm::BasicBlock* not_linked = llvm::BasicBlock::Create(*context, "not linked", F);
  llvm::BasicBlock* linked = llvm::BasicBlock::Create(*context, "linked", F);
  builder.CreateCondBr(is_null, not_linked, linked);
  builder.SetInsertPoint(not_linked);
  target = builder.CreateCall(find_nat_,
                              llvm::SmallVector<llvm::Value*, 2>{ self, method });
  is_null = builder.CreateIsNull(target);
  llvm::BasicBlock* err = llvm::BasicBlock::Create(*context, "pending exception", F);
  llvm::BasicBlock* no_err = llvm::BasicBlock::Create(*context, "no pending exception", F);
  builder.CreateCondBr(is_null, err, no_err);
  builder.SetInsertPoint(err);
  llvm::CallInst* call =
      builder.CreateCall(deliver_exc_,
                         llvm::SmallVector<llvm::Value*, 1>{ self });
  builder.CreateBr(linked);
  builder.SetInsertPoint(no_err);
  builder.CreateStore(target, target_ptr);
  builder.CreateBr(linked);
  builder.SetInsertPoint(linked);
  target = builder.CreateLoad(target_ptr);

  // Fill up the arguments.
  // References are set from the handle scope and everything else from the bridge arguments.
  llvm::TinyPtrVector<llvm::Value*> target_args;
  target_args.push_back(jnienv_tls);
  arg_it = ++++F->args().begin();
  {
    uint32_t ref = 0;
    if (is_static) {
      target_args.push_back(builder.CreatePointerCast(shadow_frame_builder.getVRegRef(ref),
                                                      llvm::Type::getInt8PtrTy(*context)));
      ref++;
    }
    for (; arg_it != arg_it_end; arg_it++) {
      llvm::Value* arg = &*arg_it;
      if (arg->getType()->isPointerTy()) {
        arg = shadow_frame_builder.getVRegRef(ref);
        llvm::Value* value = builder.CreateLoad(arg);
        llvm::Value* is_null = builder.CreateIsNull(value);
        arg = builder.CreateSelect(is_null, value,
            builder.CreatePointerCast(arg, llvm::Type::getInt8PtrTy(*context)));
        target_args.push_back(arg);
        ref++;
      } else {
        target_args.push_back(arg);
      }
    }
  }

  // And finally, call the function.
  llvm::CallInst* value =
      builder.CreateCall(builder.CreatePointerCast(target, CalleeFT->getPointerTo()), target_args);
  value->setCallingConv(jni_calling_convention_);

  // Call JNI method end listener.
  bool is_ref_ret = shorty[0] == 'L';
  llvm::SmallVector<llvm::Value*, 4> jni_end_args;
  llvm::Function* jni_end;
  if (is_synchronized) {
    if (is_ref_ret) {
      jni_end = jni_end_ref_synch_;
      jni_end_args = {
        value,
        cookie,
        builder.CreatePointerCast(shadow_frame_builder.getVRegRef(0),
                                  llvm::Type::getInt8PtrTy(*context)),
        self
      };
    } else {
      jni_end = jni_end_synch_;
      jni_end_args = {
        cookie,
        builder.CreatePointerCast(shadow_frame_builder.getVRegRef(0),
                                  llvm::Type::getInt8PtrTy(*context)),
        self
      };
    }
  } else {
    if (is_ref_ret) {
      jni_end = jni_end_ref_;
      jni_end_args = {
        value,
        cookie,
        self
      };
    } else {
      jni_end = jni_end_;
      jni_end_args = {
        cookie,
        self
      };
    }
  }

  llvm::Value* jni_end_ret = builder.CreateCall(jni_end, jni_end_args);

  // Re-link the previous shadow frame.
  shadow_frame_builder.relink();

  if (ret->isVoidTy()) {
    builder.CreateRetVoid();
  } else if (is_ref_ret) {
    builder.CreateRet(jni_end_ret);
  } else {
    builder.CreateRet(value);
  }

  return F;
}
