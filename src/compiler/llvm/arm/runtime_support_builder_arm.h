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

#ifndef ART_SRC_COMPILER_LLVM_ARM_RUNTIME_SUPPORT_BUILDER_ARM_H_
#define ART_SRC_COMPILER_LLVM_ARM_RUNTIME_SUPPORT_BUILDER_ARM_H_

#include "compiler/llvm/runtime_support_builder.h"

namespace art {
namespace compiler {
namespace Llvm {

class RuntimeSupportBuilderARM : public RuntimeSupportBuilder {
 public:
  RuntimeSupportBuilderARM(ArtIRBuilder* irb) : RuntimeSupportBuilder(irb) {}

  virtual llvm::Value* GetCurrentThread();
  virtual llvm::Value* LoadFromThreadOffset(ThreadOffset offset, llvm::Type* type,
                                            llvm::MDNode* tbaa_info);
  virtual void StoreToThreadOffset(ThreadOffset offset, llvm::Value* value,
                                   llvm::MDNode* tbaa_info);
  virtual llvm::Value* SetCurrentThread(llvm::Value* thread);
};

} // namespace Llvm
} // namespace compiler
} // namespace art

#endif // ART_SRC_COMPILER_LLVM_ARM_RUNTIME_SUPPORT_BUILDER_ARM_H_
