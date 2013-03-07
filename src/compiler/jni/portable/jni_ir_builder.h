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

#ifndef ART_SRC_COMPILER_JNI_PORTABLE_JNI_IR_BUILDER_H_
#define ART_SRC_COMPILER_JNI_PORTABLE_JNI_IR_BUILDER_H_

#include "compiler/llvm/art_ir_builder.h"
#include "thread.h"

namespace art {
namespace compiler {
namespace jni {
namespace portable {

// Create IR relevant to the portable JNI compiler.
class JniIRBuilder : public Llvm::ArtIRBuilder {
 public:
  // Return an LLVM type for JNI's jobject type.
  llvm::PointerType* GetJniObjectTy() const {
    return jni_object_type_;
  }

  // Return an LLVM type for JNIEnv*.
  llvm::PointerType* GetJniEnvTy() const {
    return jni_env_type_;
  }

  // For the given JNI primitive type return a representative LLVM type.
  llvm::Type* GetJniType(Primitive::Type type) {
    switch (type) {
      case Primitive::kPrimNot:
        return GetJniObjectTy();
      case Primitive::kPrimBoolean:
      case Primitive::kPrimByte:  // LLVM doesn't distinguish between signed and unsigned ints.
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

  // Load the JNIEnv corresponding to the current method.
  llvm::Value* LoadJniEnv() {
    return LoadFromThreadOffset(Thread::JniEnvOffset(), GetJniEnvTy());
  }

  llvm::Value* JniMethodStart(bool is_synchronized, llvm::Value* this_or_class) {
    return rsb_->JniMethodStart(is_synchronized, this_or_class);
  }

  llvm::Value* JniMethodEnd(bool is_return_ref, bool is_synchronized, llvm::Value* ret_val,
                            llvm::Value* local_ref_cookie, llvm::Value* this_or_class) {
    return rsb_->JniMethodEnd(is_return_ref, is_synchronized, ret_val, local_ref_cookie,
                              this_or_class);
  }

  JniIRBuilder(llvm::Module* module, Llvm::ArtMDBuilder* mdb, uint32_t num_vregs,
               InstructionSet instruction_set)
      : Llvm::ArtIRBuilder(module, mdb, num_vregs, instruction_set),
        jni_object_type_(GetPointerToNamedOpaqueStructType(module, "jobject")),
        jni_env_type_(GetPointerToNamedOpaqueStructType(module, "JNIEnv")) {}

 private:
  llvm::PointerType* const jni_object_type_;  // The type for jobject.
  llvm::PointerType* const jni_env_type_;  // The type for JNIEnv*.
};

}  // namespace portable
}  // namespace jni
}  // namespace compiler
}  // namespace art

#endif  // ART_SRC_COMPILER_JNI_PORTABLE_JNI_IR_BUILDER_H_
