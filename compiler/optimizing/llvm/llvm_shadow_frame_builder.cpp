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

#include "llvm_shadow_frame_builder.hpp"

#include "llvm/ADT/TinyPtrVector.h"

#include "thread.h"

LLVMShadowFrameBuilder::LLVMShadowFrameBuilder(llvm::LLVMContext* context, llvm::IRBuilder<>* builder, llvm::Value* self,
                   llvm::Value* method, bool is_64bit) : context(context), builder(builder),
                   self(self), method(method), is_64bit(is_64bit) {}

void LLVMShadowFrameBuilder::buildFromReferences(llvm::ArrayRef<llvm::Value*> references) {
    buildFromVirtualRegisters(references);
}

void LLVMShadowFrameBuilder::buildFromVirtualRegisters(llvm::ArrayRef<llvm::Value*> vregs) {
  const auto Int8PtrT = llvm::Type::getInt8PtrTy(*context);
  const auto Int16T = llvm::Type::getInt16Ty(*context);
  const auto Int32T = llvm::Type::getInt32Ty(*context);

  // Create a shadow frame structure type.
  // The first field links to the previous shadow frame.
  // The second field contains the ArtMethod*.
  // After that four unused fields follow.
  // The seventh field contains the number of vregs. (For JNI we only have reference vregs.)
  // After that the dex pc follows.
  // Before the vregs there are two fields that we don't need for now.
  // And at the end we can find the actual vreg values.
  uint32_t num_regs = 0;
  llvm::TinyPtrVector<llvm::Type*> shadow_frame_fields;
  shadow_frame_fields.push_back(Int8PtrT);
  shadow_frame_fields.push_back(Int8PtrT);
  shadow_frame_fields.push_back(Int8PtrT);
  shadow_frame_fields.push_back(Int8PtrT);
  shadow_frame_fields.push_back(Int8PtrT);
  shadow_frame_fields.push_back(Int8PtrT);
  shadow_frame_fields.push_back(Int32T);
  shadow_frame_fields.push_back(Int32T);
  shadow_frame_fields.push_back(Int16T);
  shadow_frame_fields.push_back(Int16T);
  for (llvm::Value* vreg : vregs) {
    llvm::Type* type = vreg->getType();
    if (type->isIntegerTy(64) || type->isDoubleTy()) {
      num_regs++;
      shadow_frame_fields.push_back(Int8PtrT);
    }
    shadow_frame_fields.push_back(Int8PtrT);
    num_regs++;
  }

  llvm::StructType* shadow_frame_ty = llvm::StructType::get(*context, shadow_frame_fields,
                                                            false);

  // Allocate space for the shadow_frame in the stack.
  shadow_frame = builder->CreateAlloca(shadow_frame_ty);

  // Set vreg number in the shadow frame.
  builder->CreateStore(llvm::ConstantInt::get(Int32T, num_regs),
      builder->CreateConstGEP2_32(shadow_frame_ty, shadow_frame, 0, 6));

  // Cache dex pc ptr.
  dex_pc_ptr = builder->CreateConstGEP2_32(shadow_frame_ty, shadow_frame, 0, 7);

  // Set unused fields in the shadow frame.
  builder->CreateStore(llvm::ConstantPointerNull::get(Int8PtrT),
      builder->CreateConstGEP2_32(shadow_frame_ty, shadow_frame, 0, 2));
  builder->CreateStore(llvm::ConstantPointerNull::get(Int8PtrT),
      builder->CreateConstGEP2_32(shadow_frame_ty, shadow_frame, 0, 3));
  builder->CreateStore(llvm::ConstantPointerNull::get(Int8PtrT),
      builder->CreateConstGEP2_32(shadow_frame_ty, shadow_frame, 0, 4));
  builder->CreateStore(llvm::ConstantPointerNull::get(Int8PtrT),
      builder->CreateConstGEP2_32(shadow_frame_ty, shadow_frame, 0, 5));
  builder->CreateStore(llvm::ConstantInt::get(Int16T, 0),
      builder->CreateConstGEP2_32(shadow_frame_ty, shadow_frame, 0, 8));
  builder->CreateStore(llvm::ConstantInt::get(Int16T, 0),
      builder->CreateConstGEP2_32(shadow_frame_ty, shadow_frame, 0, 9));

  // Update vregs and dex pc.
  update(vregs, 0);

  // Link in our shadow frame.
  llvm::Value* self_casted =
      builder->CreatePointerCast(self, llvm::Type::getInt8PtrTy(*context));
  uint32_t off;
  if (is_64bit) {
    off = art::Thread::TopShadowFrameOffset<8>().Int32Value();
  } else {
    off = art::Thread::TopShadowFrameOffset<4>().Int32Value();
  }
  llvm::Value* shadow_frame_off =
      llvm::ConstantInt::get(*context, llvm::APInt(32, off, false));
  shadow_frame_tls =
      builder->CreatePointerCast(builder->CreateGEP(self_casted, shadow_frame_off),
                                shadow_frame_ty->getPointerTo()->getPointerTo());
  shadow_frame_tls = builder->CreatePointerCast(shadow_frame_tls,
                                               shadow_frame_ty->getPointerTo()->getPointerTo());
  current_shadow_frame = builder->CreateLoad(shadow_frame_tls);
  llvm::Value* shadow_frame_link_ptr = builder->CreateConstGEP2_32(shadow_frame_ty, shadow_frame, 0, 0);
  shadow_frame_link_ptr =
      builder->CreatePointerCast(shadow_frame_link_ptr,
                                shadow_frame_ty->getPointerTo()->getPointerTo());
  builder->CreateStore(current_shadow_frame, shadow_frame_link_ptr);
  builder->CreateStore(shadow_frame, shadow_frame_tls);

  // Set method in the shadow frame.
  llvm::Value* method_ptr = builder->CreateConstGEP2_32(shadow_frame_ty, shadow_frame, 0, 1);
  builder->CreateStore(method, method_ptr);
}

llvm::AllocaInst* LLVMShadowFrameBuilder::buildArgumentOnlyFromValues(llvm::ArrayRef<llvm::Value*> arguments) {
  size_t count = 0;
  for (llvm::Value* argument : arguments) {
    llvm::Type* type = argument->getType();
    if (type->isDoubleTy() || type->isIntegerTy(64)) {
      count++;
    }
    count++;
  }

  // Create argument array type from the incoming values.
  llvm::ArrayType* array_ty =
      llvm::ArrayType::get(llvm::Type::getInt8PtrTy(*context), count);

  // Allocate the array on the stack.
  llvm::AllocaInst* array = builder->CreateAlloca(array_ty);
  
  fillValueArray(array, arguments, 0);
  
  return array;
}

void LLVMShadowFrameBuilder::fillValueArray(llvm::Value* array, llvm::ArrayRef<llvm::Value*> arguments, uint32_t index) {
  llvm::ConstantInt* idx_0 = llvm::ConstantInt::get(*context, llvm::APInt(32, 0, false));

  // Fill up the array.
  uint32_t i = index;
  for (llvm::Value* argument : arguments) {
    llvm::Type* type = argument->getType();

    if (type->isDoubleTy() || type->isIntegerTy(64)) {
      llvm::Type* stored_ptr_type = llvm::Type::getInt32PtrTy(*context);
      llvm::Type* extended_type = llvm::Type::getInt64Ty(*context);

      if (type->isDoubleTy()) {
        argument = builder->CreateBitCast(argument, extended_type);
      }

      llvm::Value* idx = llvm::ConstantInt::get(*context, llvm::APInt(32, i, false));
      llvm::Value* half = builder->CreateTrunc(
          builder->CreateAnd(argument, 0xFFFFFFFF), stored_ptr_type->getPointerElementType());
      llvm::Value* arg_ptr =
          builder->CreateGEP(array, llvm::SmallVector<llvm::Value*, 2>{ idx_0, idx });
      arg_ptr = builder->CreatePointerCast(arg_ptr, stored_ptr_type);
      builder->CreateStore(half, arg_ptr);
      i++;

      idx = llvm::ConstantInt::get(*context, llvm::APInt(32, i, false));
      half = builder->CreateTrunc(
          builder->CreateAShr(argument, 32), stored_ptr_type->getPointerElementType());
      arg_ptr =
          builder->CreateGEP(array, llvm::SmallVector<llvm::Value*, 2>{ idx_0, idx });
      arg_ptr = builder->CreatePointerCast(arg_ptr, stored_ptr_type);
      builder->CreateStore(half, arg_ptr);
      i++;
    } else {
      llvm::Value* idx = llvm::ConstantInt::get(*context, llvm::APInt(32, i, false));
      llvm::Value* arg_ptr =
          builder->CreateGEP(array, llvm::SmallVector<llvm::Value*, 2>{ idx_0, idx });
      arg_ptr = builder->CreatePointerCast(arg_ptr, argument->getType()->getPointerTo());
      builder->CreateStore(argument, arg_ptr);
      i++;
    }
  }
}

void LLVMShadowFrameBuilder::update(llvm::ArrayRef<llvm::Value*> vregs, uint32_t dex_pc) {
  const auto Int32T = llvm::Type::getInt32Ty(*context);
  
  // Set dex pc in the shadow frame.
  builder->CreateStore(llvm::ConstantInt::get(Int32T, dex_pc), dex_pc_ptr);
  
  // Fill up vregs in the shadow frame.
  fillValueArray(shadow_frame, vregs, 10);
}

void LLVMShadowFrameBuilder::relink() {
  CHECK(current_shadow_frame != nullptr);
  CHECK(shadow_frame_tls != nullptr);
  builder->CreateStore(current_shadow_frame, shadow_frame_tls);
}

llvm::Value* LLVMShadowFrameBuilder::getVRegRef(uint32_t i) {
  llvm::ConstantInt* idx_0 = llvm::ConstantInt::get(*context, llvm::APInt(32, 0, false));
  llvm::ConstantInt* idx = llvm::ConstantInt::get(*context, llvm::APInt(32, i + 10, false));
  llvm::Value* value =
      builder->CreateGEP(shadow_frame, llvm::SmallVector<llvm::Value*, 2>{ idx_0, idx });
  return value;
}

llvm::Value* LLVMShadowFrameBuilder::getVReg(uint32_t i) {
  llvm::Value* value = builder->CreateLoad(getVRegRef(i));
  return value;
}
