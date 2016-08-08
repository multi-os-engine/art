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

#ifndef llvm_shadow_frame_builder_hpp
#define llvm_shadow_frame_builder_hpp

#include "llvm/IR/IRBuilder.h"

class LLVMShadowFrameBuilder {
  llvm::LLVMContext* context;
  llvm::IRBuilder<>* builder;
  llvm::Value* self;
  llvm::Value* method;
  bool is_64bit;

  llvm::Value* shadow_frame_tls;
  llvm::AllocaInst* shadow_frame;
  llvm::Value* current_shadow_frame;
  llvm::Value* dex_pc_ptr;

public:
  LLVMShadowFrameBuilder(llvm::LLVMContext* context, llvm::IRBuilder<>* builder, llvm::Value* self,
                         llvm::Value* method, bool is_64bit);

  void buildFromReferences(llvm::ArrayRef<llvm::Value*> references);
  void buildFromVirtualRegisters(llvm::ArrayRef<llvm::Value*> vregs);
  
  llvm::AllocaInst* buildArgumentOnlyFromValues(llvm::ArrayRef<llvm::Value*> arguments);

  void update(llvm::ArrayRef<llvm::Value*> vregs, uint32_t dex_pc);

  void relink();

  llvm::Value* getVRegRef(uint32_t i);

  llvm::Value* getVReg(uint32_t i);
  
private:
  void fillValueArray(llvm::Value* array, llvm::ArrayRef<llvm::Value*> arguments, uint32_t index);
};

#endif /* llvm_shadow_frame_builder_hpp */
