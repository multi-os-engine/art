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

#ifndef llvm_stub_builder_hpp
#define llvm_stub_builder_hpp

#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/TargetRegistry.h"

class LLVMStubBuilder {

  bool is_64bit_;
  bool is_windows_;

  llvm::LLVMContext* ref_context_;
  llvm::Module* ref_module_;

  llvm::LLVMContext* jni_context_;
  llvm::Module* jni_module_;

  llvm::LLVMContext* res_context_;
  llvm::Module* res_module_;

  llvm::LLVMContext* int_context_;
  llvm::Module* int_module_;

  llvm::Function* res_get_res_method_;
  llvm::Function* res_tramp_;
  llvm::Type* res_ret_ty_;

  llvm::Function* enter_int_;

  llvm::Function* find_nat_;

  llvm::Function* deliver_exc_;
  llvm::Function* deliver_exc_2_;

  llvm::Function* jni_start_;
  llvm::Function* jni_start_synch_;

  llvm::Function* jni_end_;
  llvm::Function* jni_end_synch_;
  llvm::Function* jni_end_ref_;
  llvm::Function* jni_end_ref_synch_;

  llvm::CallingConv::ID jni_calling_convention_;

public:
  LLVMStubBuilder(llvm::CallingConv::ID jni_calling_convention, bool is_64bit, bool is_windows);
  ~LLVMStubBuilder();

  llvm::Function* ReflectionBridgeCompile(const char* shorty, bool is_static) const;
  llvm::Function* ResolutionTrampolineCompile(const char* shorty, bool is_static) const;
  llvm::Function* InterpreterBridgeCompile(const char* shorty, bool is_static) const;
  llvm::Function* JniBridgeCompile(const char* shorty, bool is_synchronized, bool is_static) const;

  inline llvm::Module* GetReflectionBridgeModule() {
    return ref_module_;
  }

  inline llvm::Module* GetResolutionTrampolineModule() {
    return res_module_;
  }

  inline llvm::Module* GetInterpreterBridgeModule() {
    return int_module_;
  }

  inline llvm::Module* GetJniBridgeModule() {
    return jni_module_;
  }

};

#endif /* llvm_stub_builder_hpp */
