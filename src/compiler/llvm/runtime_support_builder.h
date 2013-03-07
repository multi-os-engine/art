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

#ifndef ART_SRC_COMPILER_LLVM_RUNTIME_SUPPORT_BUILDER_H_
#define ART_SRC_COMPILER_LLVM_RUNTIME_SUPPORT_BUILDER_H_

#include "base/logging.h"
#include "instruction_set.h"
#include "offsets.h"
#include "primitive.h"

namespace llvm {
  class BasicBlock;
  class Function;
  class MDNode;
  class Type;
  class Value;
}  // namespace llvm

namespace art {
namespace compiler {
namespace Llvm {

class ArtIRBuilder;

class RuntimeSupportBuilder {
 public:
  // Create an architecture specific runtime support builder.
  static RuntimeSupportBuilder* Create(ArtIRBuilder* irb, InstructionSet instruction_set);

  // Return an instruction created in the IR builder that returns Thread::Current().
  virtual llvm::Value* GetCurrentThread();

  // Used during invoke stubs to set a machine register dedicated to holding the current thread.
  // The result is the old value of this register. For some architectures this is a no-op and
  // returns an undefined value.
  virtual llvm::Value* SetCurrentThread(llvm::Value* thread);

  // Load from Thread::Current() at the given offset a value of the given type. This operation is
  // expanded to inline assembly taking advantage of architectural knowledge of where the current
  // thread is held.
  virtual llvm::Value* LoadFromThreadOffset(ThreadOffset offset, llvm::Type* type,
                                            llvm::MDNode* tbaa_info) = 0;

  // Store to Thread::Current() at the given offset the given value.  This operation is
  // expanded to inline assembly taking advantage of architectural knowledge of where the current
  // thread is held.
  virtual void StoreToThreadOffset(ThreadOffset offset, llvm::Value* value,
                                   llvm::MDNode* tbaa_info) = 0;

  // Create a runtime call to initialize the Class associated with type_idx in the current method.
  // Returns the initialized Class.
  llvm::Value* InitializeStaticStorage(uint32_t type_idx);

  // Create a runtime call that will load the given static field by index in the context of the
  // current method and return a value of the appropriate type. This operation may also leave an
  // exception pending.
  llvm::Value* GetStatic(Primitive::Type type, uint32_t field_idx);
  llvm::Value* GetInstance(Primitive::Type type, uint32_t field_idx, llvm::Value* object);

  // Create a runtime call that will store to the given static field in the context of the current
  // method. This operation may also leave an exception pending.
  llvm::Value* SetStatic(llvm::Value* value, Primitive::Type type, uint32_t field_idx);
  llvm::Value* SetInstance(llvm::Value* object, llvm::Value* value, Primitive::Type type,
                           uint32_t field_idx);

  // Generate IR to throw a null pointer exception.
  void ThrowNullPointerException();

  // Generate IR to throw an array index out of bounds exception.
  void ThrowArrayIndexOutOfBoundsException(llvm::Value* index, llvm::Value* array_length);

  // Generate IR to throw a stack overflow error.
  void ThrowStackOverflowError();

  // Generate IR to throw a arithmetic exception for the reason of divide by zero..
  void ThrowDivZeroArithmeticException();

  // Generate IR to call out to a runtime helper for thread suspension. Continue to unwind_bb if
  // deoptimization is happening, otherwise branch to cont_bb.
  void TestSuspend(llvm::BasicBlock* unwind_bb, llvm::BasicBlock* cont_bb);

  // Generate IR to lock/unlock an object and call out to a slow path in the event of contention.
  // The object has already been checked for null.
  void LockObject(llvm::Value* object);
  void UnlockObject(llvm::Value* object);

  virtual void EmitMarkGCCard(llvm::Value* value, llvm::Value* target_addr);

  // Calls the JniMethodStart routine that handles giving away the mutator lock.
  llvm::Value* JniMethodStart(bool is_synchronized, llvm::Value* this_or_class);

  // Calls the JniMethodEnd routine decoding the return value if appropriate.
  llvm::Value* JniMethodEnd(bool is_return_ref, bool is_synchronized, llvm::Value* ret_val,
                            llvm::Value* local_ref_cookie, llvm::Value* this_or_class);

  RuntimeSupportBuilder(ArtIRBuilder* irb) : irb_(irb) {}
  virtual ~RuntimeSupportBuilder() {}

  enum RuntimeId {
    kGetCurrentThread,
    kSetCurrentThread,
    kInitializeStaticStorage,
    kGetStaticObject,
    kGetStaticBoolean,
    kGetStaticByte,
    kGetStaticChar,
    kGetStaticShort,
    kGetStaticInt,
    kGetStaticLong,
    kGetStaticFloat,
    kGetStaticDouble,
    kGetInstanceObject,
    kGetInstanceBoolean,
    kGetInstanceByte,
    kGetInstanceChar,
    kGetInstanceShort,
    kGetInstanceInt,
    kGetInstanceLong,
    kGetInstanceFloat,
    kGetInstanceDouble,
    kSetStaticObject,
    kSetStaticBoolean,
    kSetStaticByte,
    kSetStaticChar,
    kSetStaticShort,
    kSetStaticInt,
    kSetStaticLong,
    kSetStaticFloat,
    kSetStaticDouble,
    kSetInstanceObject,
    kSetInstanceBoolean,
    kSetInstanceByte,
    kSetInstanceChar,
    kSetInstanceShort,
    kSetInstanceInt,
    kSetInstanceLong,
    kSetInstanceFloat,
    kSetInstanceDouble,
    kThrowNullPointerException,
    kThrowArrayIndexOutOfBoundsException,
    kThrowStackOverflowError,
    kThrowDivZeroArithmeticException,
    kTestSuspend,
    kLockObject,
    kUnlockObject,
    kJniMethodStart,
    kJniMethodStartSynchronized,
    kJniMethodEndWithReferenceSynchronized,
    kJniMethodEndWithReference,
    kJniMethodEndSynchronized,
    kJniMethodEnd,
    MAX_ID
  };

 protected:
  // The owning IR builder.
  ArtIRBuilder* const irb_;

  llvm::Function* GetRuntimeSupportFunction(RuntimeId id);

 private:
  // Lazily populated table of runtime support function declarations. If we can't expand a function
  // inline we'll call through to an entry in here.
  llvm::Function* runtime_support_func_decls_[MAX_ID];
};


}  // namespace llvm
}  // namespace compiler
}  // namespace art

#endif // ART_SRC_COMPILER_LLVM_RUNTIME_SUPPORT_BUILDER_H_
