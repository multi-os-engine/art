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

#include "base/logging.h"
#include "class_linker.h"
#include "compiled_method.h"
#include "compiler/driver/compiler_driver.h"
#include "compiler/driver/dex_compilation_unit.h"
#include "compiler/llvm/art_ir_builder.h"
#include "compiler/llvm/runtime_support_builder.h"
#include "dex_file-inl.h"
#include "jni_ir_builder.h"
#include "mirror/abstract_method.h"
#include "runtime.h"
#include "stack.h"
#include "thread.h"

#include <llvm/ADT/SmallVector.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>

namespace art {
namespace compiler {
namespace jni {
namespace portable {

enum JniFunctionType {
  kManagedAbi,
  kJniAbi
};

// Create a FunctionType for either the JNI bridge method or the native method to be called.
static llvm::FunctionType* GetFunctionType(JniIRBuilder* irb,
                                           bool is_static, const char* shorty, uint32_t shorty_len,
                                           JniFunctionType type) {
  // Get return type
  llvm::Type* ret_type = irb->GetJavaType(Primitive::GetType(shorty[0]));

  // Get argument type
  std::vector<llvm::Type*> args_type;

  if (type == kManagedAbi) {
    args_type.push_back(irb->GetJavaMethodTy());  // AbstractMethod*.
    if (!is_static) {
      args_type.push_back(irb->GetJavaObjectTy());  // "this".
    }
    for (uint32_t i = 1; i < shorty_len; ++i) {
      args_type.push_back(irb->GetJavaType(Primitive::GetType(shorty[i])));
    }
  } else {
    args_type.push_back(irb->GetJniEnvTy());  // JNIEnv*.
    args_type.push_back(irb->GetJniObjectTy());  // jobject (this) or jclass (declaring class).
    for (uint32_t i = 1; i < shorty_len; ++i) {
      args_type.push_back(irb->GetJniType(Primitive::GetType(shorty[i])));
    }
  }
  return ::llvm::FunctionType::get(ret_type, args_type, false);
}

// Create a bridge from portable code to native code, handshaking with the GC and marshaling
// arguments.
using art::compiler::Llvm::RuntimeSupportBuilder;
CompiledMethod* JniCompilePortable(art::compiler::driver::CompilerDriver* compiler_driver,
                                   uint32_t access_flags, uint32_t method_idx,
                                   const DexFile& dex_file) {
  VLOG(compiler) << "JNI compiling " << PrettyMethod(method_idx, dex_file)
      << " using portable codegen.";

  const bool is_static = (access_flags & kAccStatic) != 0;
  const bool is_synchronized = (access_flags & kAccSynchronized) != 0;
  DexFile::MethodId const& method_id = dex_file.GetMethodId(method_idx);
  uint32_t shorty_len;
  const char* shorty = dex_file.GetMethodShorty(method_id, &shorty_len);
  // There will always be 1 reference in the JNI bridge's shadow frame for the jclass (declaring
  // class) or jobject ("this") argument.
  uint32_t num_vregs = 1;
  for (uint32_t i = 1; i < shorty_len; ++i) {
    if (shorty[i] == 'L') {
      num_vregs++;
    }
  }
  JniIRBuilder irb(compiler_driver->GetLlvmModuleAtStartOfCompile(),
                   compiler_driver->GetLlvmMdBuilder(),
                   num_vregs,
                   compiler_driver->GetInstructionSet());

  // Create the function as called from by the managed ABI.
  std::string func_name(StringPrintf("jni_%s",
                                     MangleForJni(PrettyMethod(method_idx, dex_file)).c_str()));

  llvm::FunctionType* func_type = GetFunctionType(&irb, is_static, shorty, shorty_len, kManagedAbi);

  // Create function and set up IR builder.
  llvm::Function* func = llvm::Function::Create(func_type, llvm::Function::InternalLinkage,
                                                func_name, irb.GetModule());
  llvm::BasicBlock* basic_block = ::llvm::BasicBlock::Create(irb.getContext(), "entry", func);
  irb.SetInsertPoint(basic_block);

  llvm::Function::arg_iterator arg_begin(func->arg_begin());
  llvm::Function::arg_iterator arg_end(func->arg_end());
  llvm::Function::arg_iterator arg_iter(arg_begin);

  DCHECK_NE(arg_iter, arg_end);

  uint32_t cur_vreg = 0;  // Current shadow frame vreg being assigned.
  // Skip method argument.
  arg_iter++;
  // Add "this" or declaring class to shadow frame, remember the value for synchronized methods.
  llvm::Value* this_or_class;
  if (!is_static) {
    this_or_class = arg_iter;
    arg_iter++;
  } else {
    this_or_class = irb.LoadFieldFromCurMethod(Primitive::kPrimNot,
                                               "Ljava/lang/Class;",
                                               "declaringClass",
                                               mirror::AbstractMethod::DeclaringClassOffset(),
                                               true);
  }
  irb.RememberShadowFrameVReg(cur_vreg, this_or_class);
  cur_vreg++;
  // Add reference parameters to shadow frame.
  for (uint32_t i = 1; i < shorty_len; i++) {
    if (shorty[i] == 'L') {
      irb.RememberShadowFrameVReg(cur_vreg, arg_iter);
      cur_vreg++;
    }
    arg_iter++;
  }
  // Flush all the reference arguments onto the stack which will cause the shadow frame to be
  // pushed.
  irb.FlushShadowFrameVRegsAndSetDexPc(DexFile::kDexNoIndex);

  // Build the outgoing arguments.
  cur_vreg = 0;
  arg_iter = arg_begin;
  std::vector<llvm::Value*> jni_args;  // Arguments being set up for call to native code.
  jni_args.push_back(irb.LoadJniEnv());  // Place JNI env into outgoing native arguments.
  jni_args.push_back(irb.GetShadowFrameVRegPtrForSlot(cur_vreg));  // Place "this"/declaring class.
  cur_vreg++;
  for (uint32_t i = 1; i < shorty_len; i++) {
    if (shorty[i] == 'L') {
      // Compare reference arguments with null, push the vreg address if non-null.
      llvm::Value* java_null = irb.GetJavaNull();
      llvm::Value* equal_null = irb.CreateICmpEQ(arg_iter, java_null);
      llvm::Value* arg = irb.CreateSelect(equal_null, java_null,
                                          irb.GetShadowFrameVRegPtrForSlot(cur_vreg));
      jni_args.push_back(arg);
      cur_vreg++;
    } else {
      jni_args.push_back(arg_iter);
    }
    arg_iter++;
  }

  llvm::Value* saved_local_ref_cookie = irb.JniMethodStart(is_synchronized, this_or_class);

  // Call!!!
  llvm::Value* code_addr = irb.LoadFieldFromCurMethod(Primitive::kPrimInt,
                                                      "I",
                                                      "nativeMethod",
                                                      mirror::AbstractMethod::NativeMethodOffset(),
                                                      true);
  llvm::Value* ret_val = irb.CreateCall(code_addr, jni_args);

  bool is_return_ref = (shorty[0] == 'L');
  ret_val = irb.JniMethodEnd(is_return_ref, is_synchronized, ret_val, saved_local_ref_cookie,
                             this_or_class);

  // Pop the shadow frame
  irb.PopShadowFrame();

  // Return!
  if (shorty[0] == 'V') {
    irb.CreateRetVoid();
  } else {
    irb.CreateRet(ret_val);
  }

  return compiler_driver->MaterializeLlvmCode(func, NULL, func_name);
}

}  // namespace portable
}  // namespace jni
}  // namespace compiler
}  // namespace art
