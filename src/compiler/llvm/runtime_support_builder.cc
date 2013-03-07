/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "runtime_support_builder.h"

#include "art_ir_builder.h"
#include "compiler/llvm/arm/runtime_support_builder_arm.h"
#include "compiler/llvm/x86/runtime_support_builder_x86.h"
#include "gc/card_table.h"
#include "monitor.h"
#include "mirror/object.h"
#include "thread.h"

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>

namespace art {
namespace compiler {
namespace Llvm {

enum RuntimeFunctionArgType {
  kNone,          // Place holder for fixed size arrays.
  kVoid,
  kBoolean,
  kByte,
  kChar,
  kShort,
  kInt,
  kLong,
  kFloat,
  kDouble,
  kThread,      // A art::Thread* argument.
  kJavaObject,  // A java.lang.Object argument.
  kJavaMethod,  // A java.lang.reflect.AbstractMethod argument.
  kJavaClass,   // A java.lang.Class argument.
};

enum RuntimeFunctionAttribute {
  kAttrNone     = 0,
  // A pure function.
  kAttrReadNone = 1 << 0,
  // Function that doesn't modify the memory state. Note that one should set this flag carefully
  // when the intrinsic may throw exception. Since the thread state is implicitly modified when an
  // exception is thrown.
  kAttrReadOnly = 1 << 1,
};

static const size_t kMaxArgs = 5;
#define NO_ARGS                       {kNone, kNone, kNone, kNone, kNone}
#define ONE_ARG(arg1)                 {arg1,  kNone, kNone, kNone, kNone}
#define TWO_ARGS(arg1, arg2)          {arg1,  arg2,  kNone, kNone, kNone}
#define THREE_ARGS(arg1, arg2, arg3)  {arg1,  arg2,  arg3,  kNone, kNone}

struct RuntimeSupportFunctionDefinition {
  RuntimeSupportBuilder::RuntimeId id;
  const char* name;
  RuntimeFunctionArgType return_type;
  RuntimeFunctionArgType arg_types[kMaxArgs];
  int32_t attributes;
};

const RuntimeSupportFunctionDefinition functions[] = {
  // A Thread::Current() call in code, marked as read-none (pure) so that calls can be CSE-d.
  {RuntimeSupportBuilder::kGetCurrentThread, "art_portable_get_current_thread",
      kThread, NO_ARGS,
      kAttrReadNone
  },
  // Used on ARM to set R9, returning the old contents.
  {RuntimeSupportBuilder::kSetCurrentThread, "art_portable_set_current_thread",
      kThread, ONE_ARG(kThread),
      kAttrNone
  },
  // Slow path to go initialize a class prior to use in a field load or such.
  {RuntimeSupportBuilder::kInitializeStaticStorage, "art_portable_initialize_static_storage",
      kJavaClass, THREE_ARGS(kThread, kJavaMethod, kInt),
      kAttrNone
  },
  // Slow path static field getters. These are passed the field index and current method. They may
  // throw an exception.
  {RuntimeSupportBuilder::kGetStaticObject, "art_portable_get_static_object",
      kJavaObject, TWO_ARGS(kInt, kJavaMethod),
      kAttrNone
  },
  {RuntimeSupportBuilder::kGetStaticBoolean, "art_portable_get_static_boolean",
      kBoolean, TWO_ARGS(kInt, kJavaMethod),
      kAttrNone
  },
  {RuntimeSupportBuilder::kGetStaticByte, "art_portable_get_static_byte",
      kByte, TWO_ARGS(kInt, kJavaMethod),
      kAttrNone
  },
  {RuntimeSupportBuilder::kGetStaticChar, "art_portable_get_static_char",
      kChar, TWO_ARGS(kInt, kJavaMethod),
      kAttrNone
  },
  {RuntimeSupportBuilder::kGetStaticShort, "art_portable_get_static_short",
      kShort, TWO_ARGS(kInt, kJavaMethod),
      kAttrNone
  },
  {RuntimeSupportBuilder::kGetStaticInt, "art_portable_get_static_int",
      kInt, TWO_ARGS(kInt, kJavaMethod),
      kAttrNone
  },
  {RuntimeSupportBuilder::kGetStaticLong, "art_portable_get_static_long",
      kLong, TWO_ARGS(kInt, kJavaMethod),
      kAttrNone
  },
  {RuntimeSupportBuilder::kGetStaticFloat, "art_portable_get_static_float",
      kFloat, TWO_ARGS(kInt, kJavaMethod),
      kAttrNone
  },
  {RuntimeSupportBuilder::kGetStaticDouble, "art_portable_get_static_double",
      kDouble, TWO_ARGS(kInt, kJavaMethod),
      kAttrNone
  },
  // Slow path instance field getters. These are passed the field index, current method and the
  // object to read from. They may throw an exception.
  {RuntimeSupportBuilder::kGetInstanceObject, "art_portable_get_instance_object",
      kJavaObject, THREE_ARGS(kInt, kJavaMethod, kJavaObject),
      kAttrNone
  },
  {RuntimeSupportBuilder::kGetInstanceBoolean, "art_portable_get_instance_boolean",
      kBoolean, THREE_ARGS(kInt, kJavaMethod, kJavaObject),
      kAttrNone
  },
  {RuntimeSupportBuilder::kGetInstanceByte, "art_portable_get_instance_byte",
      kByte, THREE_ARGS(kInt, kJavaMethod, kJavaObject),
      kAttrNone
  },
  {RuntimeSupportBuilder::kGetInstanceChar, "art_portable_get_instance_char",
      kChar, THREE_ARGS(kInt, kJavaMethod, kJavaObject),
      kAttrNone
  },
  {RuntimeSupportBuilder::kGetInstanceShort, "art_portable_get_instance_short",
      kShort, THREE_ARGS(kInt, kJavaMethod, kJavaObject),
      kAttrNone
  },
  {RuntimeSupportBuilder::kGetInstanceInt, "art_portable_get_instance_int",
      kInt, THREE_ARGS(kInt, kJavaMethod, kJavaObject),
      kAttrNone
  },
  {RuntimeSupportBuilder::kGetInstanceLong, "art_portable_get_instance_long",
      kLong, THREE_ARGS(kInt, kJavaMethod, kJavaObject),
      kAttrNone
  },
  {RuntimeSupportBuilder::kGetInstanceFloat, "art_portable_get_instance_float",
      kFloat, THREE_ARGS(kInt, kJavaMethod, kJavaObject),
      kAttrNone
  },
  {RuntimeSupportBuilder::kGetInstanceDouble, "art_portable_get_instance_double",
      kDouble, THREE_ARGS(kInt, kJavaMethod, kJavaObject),
      kAttrNone
  },
  // Call to helper routine that throws stack overflow error.
  {RuntimeSupportBuilder::kThrowStackOverflowError, "art_portable_stack_overflow_error",
      kVoid, NO_ARGS,
      kAttrNone
  },
  // Slow path static field setters. These are passed the field index, current method and value to
  // store. They may throw an exception and return whether an exception is pending by returning a
  // non-zero value.
  {RuntimeSupportBuilder::kSetStaticObject, "art_portable_set_static_object",
      kInt, THREE_ARGS(kInt, kJavaMethod, kJavaObject),
      kAttrNone
  },
  {RuntimeSupportBuilder::kSetStaticBoolean, "art_portable_set_static_boolean",
      kInt, THREE_ARGS(kInt, kJavaMethod, kBoolean),
      kAttrNone
  },
  {RuntimeSupportBuilder::kSetStaticByte, "art_portable_set_static_byte",
      kInt, THREE_ARGS(kInt, kJavaMethod, kByte),
      kAttrNone
  },
  {RuntimeSupportBuilder::kSetStaticChar, "art_portable_set_static_char",
      kInt, THREE_ARGS(kInt, kJavaMethod, kChar),
      kAttrNone
  },
  {RuntimeSupportBuilder::kSetStaticShort, "art_portable_set_static_short",
      kInt, THREE_ARGS(kInt, kJavaMethod, kShort),
      kAttrNone
  },
  {RuntimeSupportBuilder::kSetStaticInt, "art_portable_set_static_int",
      kInt, THREE_ARGS(kInt, kJavaMethod, kInt),
      kAttrNone
  },
  {RuntimeSupportBuilder::kSetStaticLong, "art_portable_set_static_long",
      kInt, THREE_ARGS(kInt, kJavaMethod, kLong),
      kAttrNone
  },
  {RuntimeSupportBuilder::kSetStaticFloat, "art_portable_set_static_float",
      kInt, THREE_ARGS(kInt, kJavaMethod, kFloat),
      kAttrNone
  },
  {RuntimeSupportBuilder::kSetStaticDouble, "art_portable_set_static_double",
      kInt, THREE_ARGS(kInt, kJavaMethod, kDouble),
      kAttrNone
  },
  // Slow path instance field setters. These are passed the field index, current method and value to
  // store. They may throw an exception and return whether an exception is pending by returning a
  // non-zero value.
  {RuntimeSupportBuilder::kSetInstanceObject, "art_portable_set_instance_object",
      kInt, THREE_ARGS(kInt, kJavaMethod, kJavaObject),
      kAttrNone
  },
  {RuntimeSupportBuilder::kSetInstanceBoolean, "art_portable_set_instance_boolean",
      kInt, THREE_ARGS(kInt, kJavaMethod, kBoolean),
      kAttrNone
  },
  {RuntimeSupportBuilder::kSetInstanceByte, "art_portable_set_instance_byte",
      kInt, THREE_ARGS(kInt, kJavaMethod, kByte),
      kAttrNone
  },
  {RuntimeSupportBuilder::kSetInstanceChar, "art_portable_set_instance_char",
      kInt, THREE_ARGS(kInt, kJavaMethod, kChar),
      kAttrNone
  },
  {RuntimeSupportBuilder::kSetInstanceShort, "art_portable_set_instance_short",
      kInt, THREE_ARGS(kInt, kJavaMethod, kShort),
      kAttrNone
  },
  {RuntimeSupportBuilder::kSetInstanceInt, "art_portable_set_instance_int",
      kInt, THREE_ARGS(kInt, kJavaMethod, kInt),
      kAttrNone
  },
  {RuntimeSupportBuilder::kSetInstanceLong, "art_portable_set_instance_long",
      kInt, THREE_ARGS(kInt, kJavaMethod, kLong),
      kAttrNone
  },
  {RuntimeSupportBuilder::kSetInstanceFloat, "art_portable_set_instance_float",
      kInt, THREE_ARGS(kInt, kJavaMethod, kFloat),
      kAttrNone
  },
  {RuntimeSupportBuilder::kSetInstanceDouble, "art_portable_set_instance_double",
      kInt, THREE_ARGS(kInt, kJavaMethod, kDouble),
      kAttrNone
  },
  // Unconditionally throw a null pointer exception.
  {RuntimeSupportBuilder::kThrowNullPointerException, "art_portable_throw_npe_from_code",
      kVoid, NO_ARGS,
      kAttrNone
  },
  // Unconditionally throw an array index out of bounds exception. Is passed the failing index and
  // the array's length.
  {RuntimeSupportBuilder::kThrowArrayIndexOutOfBoundsException, "art_portable_throw_aioobe_from_code",
      kVoid, TWO_ARGS(kInt, kInt),
      kAttrNone
  },
  // Unconditionally throw a stack overflow error.
  {RuntimeSupportBuilder::kThrowStackOverflowError, "art_portable_throw_stack_overflow_from_code",
      kVoid, NO_ARGS,
      kAttrNone
  },
  // Unconditionally throw a arithmetic exception for the reason of divide by zero.
  {RuntimeSupportBuilder::kThrowDivZeroArithmeticException, "art_portable_throw_div_zero_from_code",
      kVoid, NO_ARGS,
      kAttrNone
  },
  // Perform a self suspend check, returns a no-zero value to force unwinding for deoptimization.
  {RuntimeSupportBuilder::kTestSuspend, "art_portable_test_suspend_from_code",
      kInt, NO_ARGS,
      kAttrNone
  },
  // Slow path object locking.
  {RuntimeSupportBuilder::kLockObject, "art_portable_lock_object_from_code",
      kVoid, TWO_ARGS(kThread, kJavaObject),
      kAttrNone
  },
  // Slow path object unlocking.
  {RuntimeSupportBuilder::kUnlockObject, "art_portable_unlock_object_from_code",
      kVoid, TWO_ARGS(kThread, kJavaObject),
      kAttrNone
  },
  // Release mutator lock on way to JNI call.
  {RuntimeSupportBuilder::kJniMethodStart, "art_portable_jni_method_start",
      kInt, ONE_ARG(kThread),
      kAttrNone
  },
  // Synchronize on class/this, release mutator lock on way to JNI call.
  {RuntimeSupportBuilder::kJniMethodStartSynchronized, "art_portable_jni_method_start_synchronized",
      kInt, TWO_ARGS(kThread, kJavaObject),
      kAttrNone
  },
  // Re-acquire mutator lock and become runnable, release any monitors taken on entry.
  {RuntimeSupportBuilder::kJniMethodEndWithReferenceSynchronized, "art_portable_jni_method_start_synchronized",
      kInt, TWO_ARGS(kThread, kJavaObject),
      kAttrNone
  },
  //kJniMethodEndWithReferenceSynchronized,
  //kJniMethodEndWithReference,
  //kJniMethodEndSynchronized,
  //kJniMethodEnd,

};

RuntimeSupportBuilder* RuntimeSupportBuilder::Create(ArtIRBuilder* irb,
                                                     InstructionSet instruction_set) {
  switch (instruction_set) {
    case kArm:
    case kThumb2:
      return new RuntimeSupportBuilderARM(irb);
    case kX86:
      return new RuntimeSupportBuilderX86(irb);
    default:
      UNIMPLEMENTED(FATAL) << instruction_set;
      return NULL;
  }
}

static llvm::Type* MapArgTypeToLlvmType(ArtIRBuilder* irb, RuntimeFunctionArgType r_type) {
  switch (r_type) {
    case kVoid:
      return irb->getVoidTy();
    case kBoolean:
      return irb->getInt1Ty();
    case kByte:
      return irb->getInt8Ty();
    case kChar:
    case kShort:
      return irb->getInt16Ty();
    case kInt:
      return irb->getInt32Ty();
    case kLong:
      return irb->getInt64Ty();
    case kFloat:
      return irb->getFloatTy();
    case kDouble:
      return irb->getDoubleTy();
    case kThread:
      return irb->GetThreadTy();
    case kJavaObject:
      return irb->GetJavaObjectTy();
    default:
      UNIMPLEMENTED(FATAL) << r_type;
      return NULL;
  }
}

llvm::Function* RuntimeSupportBuilder::GetRuntimeSupportFunction(RuntimeId id) {
  CHECK(id >= 0 && id < MAX_ID) << "Unknown runtime function id: " << id;
  DCHECK_EQ(functions[id].id, id);
  llvm::Function* result = runtime_support_func_decls_[id];
  if (result == NULL) {
    llvm::Module* module = irb_->GetModule();
    const char* name = functions[id].name;
    result = module->getFunction(name);
    if (result == NULL) {  // Function not already declared in module so declare it.
      // Build argument list and return type.
      std::vector<llvm::Type*> function_args;
      for (size_t i = 0; i < kMaxArgs; ++i) {
        RuntimeFunctionArgType arg_type = functions[id].arg_types[i];
        if (arg_type == kNone) {
          break;
        }
        llvm::Type* llvm_type = MapArgTypeToLlvmType(irb_, arg_type);
        function_args.push_back(llvm_type);
      }
      llvm::Type* return_type = MapArgTypeToLlvmType(irb_, functions[id].return_type);
      llvm::FunctionType* function_type =
          llvm::FunctionType::get(return_type, function_args, false /* isVarArg */);
      // Create the function.
      result = llvm::Function::Create(function_type, llvm::GlobalValue::ExternalLinkage, name,
                                      module);
      // Add special attributes to aid optimization.
      if ((functions[id].attributes & kAttrReadNone) != 0) {
        result->setDoesNotAccessMemory();
      }
      if ((functions[id].attributes & kAttrReadOnly) != 0) {
        result->setOnlyReadsMemory();
      }
      result->setDoesNotThrow();
      // Add special attributes to arguments to aid optimization.
      for (llvm::Function::arg_iterator arg_iter = result->arg_begin(),
           arg_end = result->arg_end(); arg_iter != arg_end; ++arg_iter) {
        if (arg_iter->getType()->isPointerTy()) {
          std::vector< ::llvm::Attribute::AttrKind> attributes;
          if (true) { // (id != kSetStaticObject) && (id != kSetInstanceObject)) {
            // Set the no capture attribute allowing the caller to know the parameter provided
            // cannot be reloaded from another load to a different part of memory.
            attributes.push_back(::llvm::Attribute::NoCapture);
          }
          // All pointer arguments don't alias.
          attributes.push_back(::llvm::Attribute::NoAlias);
          llvm::AttributeSet attribute_set = llvm::AttributeSet::get(irb_->getContext(),
                                                                     arg_iter->getArgNo(),
                                                                     attributes);
          arg_iter->addAttr(attribute_set);
        }
      }
    }
    runtime_support_func_decls_[id] = result;
  }
  return result;
}

llvm::Value* RuntimeSupportBuilder::SetCurrentThread(llvm::Value* thread) {
  llvm::Function* func = GetRuntimeSupportFunction(kSetCurrentThread);
  return irb_->CreateCall(func, thread);
}

llvm::Value* RuntimeSupportBuilder::GetCurrentThread() {
  return irb_->CreateCall(GetRuntimeSupportFunction(kGetCurrentThread));
}

llvm::Value* RuntimeSupportBuilder::InitializeStaticStorage(uint32_t type_idx) {
  return irb_->CreateCall3(GetRuntimeSupportFunction(kInitializeStaticStorage),
                           GetCurrentThread(), irb_->LoadCurMethod(), irb_->getInt32(type_idx));
}

llvm::Value* RuntimeSupportBuilder::GetStatic(Primitive::Type type, uint32_t field_idx) {
  llvm::Function* func;
  switch (type) {
    case Primitive::kPrimNot:
      func = GetRuntimeSupportFunction(kGetStaticObject);
      break;
    case Primitive::kPrimBoolean:
      func = GetRuntimeSupportFunction(kGetStaticBoolean);
      break;
    case Primitive::kPrimByte:
      func = GetRuntimeSupportFunction(kGetStaticByte);
      break;
    case Primitive::kPrimChar:
      func = GetRuntimeSupportFunction(kGetStaticChar);
      break;
    case Primitive::kPrimShort:
      func = GetRuntimeSupportFunction(kGetStaticShort);
      break;
    case Primitive::kPrimInt:
      func = GetRuntimeSupportFunction(kGetStaticInt);
      break;
    case Primitive::kPrimLong:
      func = GetRuntimeSupportFunction(kGetStaticLong);
      break;
    case Primitive::kPrimFloat:
      func = GetRuntimeSupportFunction(kGetStaticFloat);
      break;
    case Primitive::kPrimDouble:
      func = GetRuntimeSupportFunction(kGetStaticDouble);
      break;
    default:
      UNIMPLEMENTED(FATAL) << "Unknown runtime get static " << type;
      func = NULL;
      break;
  }
  // Note: for brevity in the generated code, we don't pass the current thread to the getter as
  //       it is only used in the slow path case.
  return irb_->CreateCall2(func, irb_->getInt32(field_idx), irb_->LoadCurMethod());
}

llvm::Value* RuntimeSupportBuilder::GetInstance(Primitive::Type type, uint32_t field_idx,
                                                llvm::Value* object) {
  llvm::Function* func;
  switch (type) {
    case Primitive::kPrimNot:
      func = GetRuntimeSupportFunction(kGetInstanceObject);
      break;
    case Primitive::kPrimBoolean:
      func = GetRuntimeSupportFunction(kGetInstanceBoolean);
      break;
    case Primitive::kPrimByte:
      func = GetRuntimeSupportFunction(kGetInstanceByte);
      break;
    case Primitive::kPrimChar:
      func = GetRuntimeSupportFunction(kGetInstanceChar);
      break;
    case Primitive::kPrimShort:
      func = GetRuntimeSupportFunction(kGetInstanceShort);
      break;
    case Primitive::kPrimInt:
      func = GetRuntimeSupportFunction(kGetInstanceInt);
      break;
    case Primitive::kPrimLong:
      func = GetRuntimeSupportFunction(kGetInstanceLong);
      break;
    case Primitive::kPrimFloat:
      func = GetRuntimeSupportFunction(kGetInstanceFloat);
      break;
    case Primitive::kPrimDouble:
      func = GetRuntimeSupportFunction(kGetInstanceDouble);
      break;
    default:
      UNIMPLEMENTED(FATAL) << "Unknown runtime get instance " << type;
      func = NULL;
      break;
  }
  // Note: for brevity in the generated code, we don't pass the current thread to the getter as
  //       it is only used in the slow path case.
  return irb_->CreateCall3(func, irb_->getInt32(field_idx), irb_->LoadCurMethod(), object);
}

llvm::Value* RuntimeSupportBuilder::SetStatic(llvm::Value* value, Primitive::Type type,
                                              uint32_t field_idx) {
  llvm::Function* func;
  switch (type) {
    case Primitive::kPrimNot:
      func = GetRuntimeSupportFunction(kSetStaticObject);
      break;
    case Primitive::kPrimBoolean:
      func = GetRuntimeSupportFunction(kSetStaticBoolean);
      break;
    case Primitive::kPrimByte:
      func = GetRuntimeSupportFunction(kSetStaticByte);
      break;
    case Primitive::kPrimChar:
      func = GetRuntimeSupportFunction(kSetStaticChar);
      break;
    case Primitive::kPrimShort:
      func = GetRuntimeSupportFunction(kSetStaticShort);
      break;
    case Primitive::kPrimInt:
      func = GetRuntimeSupportFunction(kSetStaticInt);
      break;
    case Primitive::kPrimLong:
      func = GetRuntimeSupportFunction(kSetStaticLong);
      break;
    case Primitive::kPrimFloat:
      func = GetRuntimeSupportFunction(kSetStaticFloat);
      break;
    case Primitive::kPrimDouble:
      func = GetRuntimeSupportFunction(kSetStaticDouble);
      break;
    default:
      UNIMPLEMENTED(FATAL) << "Unknown runtime set static " << type;
      func = NULL;
      break;
  }
  // Note: for brevity in the generated code, we don't pass the current thread to the getter as
  //       it is only used in the slow path case.
  return irb_->CreateCall3(func, irb_->getInt32(field_idx), irb_->LoadCurMethod(), value);
}

llvm::Value* RuntimeSupportBuilder::SetInstance(llvm::Value* object, llvm::Value* value,
                                                Primitive::Type type, uint32_t field_idx) {
  llvm::Function* func;
  switch (type) {
    case Primitive::kPrimNot:
      func = GetRuntimeSupportFunction(kSetInstanceObject);
      break;
    case Primitive::kPrimBoolean:
      func = GetRuntimeSupportFunction(kSetInstanceBoolean);
      break;
    case Primitive::kPrimByte:
      func = GetRuntimeSupportFunction(kSetInstanceByte);
      break;
    case Primitive::kPrimChar:
      func = GetRuntimeSupportFunction(kSetInstanceChar);
      break;
    case Primitive::kPrimShort:
      func = GetRuntimeSupportFunction(kSetInstanceShort);
      break;
    case Primitive::kPrimInt:
      func = GetRuntimeSupportFunction(kSetInstanceInt);
      break;
    case Primitive::kPrimLong:
      func = GetRuntimeSupportFunction(kSetInstanceLong);
      break;
    case Primitive::kPrimFloat:
      func = GetRuntimeSupportFunction(kSetInstanceFloat);
      break;
    case Primitive::kPrimDouble:
      func = GetRuntimeSupportFunction(kSetInstanceDouble);
      break;
    default:
      UNIMPLEMENTED(FATAL) << "Unknown runtime set static " << type;
      func = NULL;
      break;
  }
  // Note: for brevity in the generated code, we don't pass the current thread to the getter as
  //       it is only used in the slow path case.
  return irb_->CreateCall4(func, irb_->getInt32(field_idx), irb_->LoadCurMethod(), object, value);
}

void RuntimeSupportBuilder::ThrowStackOverflowError() {
  irb_->CreateCall(GetRuntimeSupportFunction(kThrowStackOverflowError));
}

void RuntimeSupportBuilder::ThrowDivZeroArithmeticException() {
  irb_->CreateCall(GetRuntimeSupportFunction(kThrowDivZeroArithmeticException));
}

void RuntimeSupportBuilder::ThrowNullPointerException() {
  irb_->CreateCall(GetRuntimeSupportFunction(kThrowNullPointerException));
}

void RuntimeSupportBuilder::ThrowArrayIndexOutOfBoundsException(llvm::Value* index,
                                                                llvm::Value* array_length) {
  irb_->CreateCall2(GetRuntimeSupportFunction(kThrowArrayIndexOutOfBoundsException), index, array_length);
}


void RuntimeSupportBuilder::TestSuspend(llvm::BasicBlock* unwind_bb, llvm::BasicBlock* cont_bb) {
  llvm::Value* unwind_if_non_zero = irb_->CreateCall(GetRuntimeSupportFunction(kTestSuspend));
  llvm::Value* do_unwind = irb_->CreateICmpNE(unwind_if_non_zero, irb_->getInt32(0));
  irb_->CreateCondBr(do_unwind, unwind_bb, cont_bb, ArtIRBuilder::kUnlikely);
}

llvm::Value* RuntimeSupportBuilder::JniMethodStart(bool is_synchronized, llvm::Value* this_or_class) {
  RuntimeSupportBuilder::RuntimeId func_id =
      is_synchronized ? RuntimeSupportBuilder::kJniMethodStartSynchronized
                      : RuntimeSupportBuilder::kJniMethodStart;
  llvm::SmallVector<llvm::Value*, 2> args;
  args.push_back(GetCurrentThread());
  if (is_synchronized) {
    args.push_back(this_or_class);
  }
  llvm::Value* local_ref_cookie = irb_->CreateCall(GetRuntimeSupportFunction(func_id), args);
  return local_ref_cookie;
}

llvm::Value* RuntimeSupportBuilder::JniMethodEnd(bool is_return_ref, bool is_synchronized,
                                                 llvm::Value* ret_val,
                                                 llvm::Value* local_ref_cookie,
                                                 llvm::Value* this_or_class) {
  RuntimeSupportBuilder::RuntimeId func_id =
      is_return_ref ? (is_synchronized ? RuntimeSupportBuilder::kJniMethodEndWithReferenceSynchronized
                                       : RuntimeSupportBuilder::kJniMethodEndWithReference)
                    : (is_synchronized ? RuntimeSupportBuilder::kJniMethodEndSynchronized
                                       : RuntimeSupportBuilder::kJniMethodEnd);
  llvm::SmallVector< ::llvm::Value*, 4> args;
  args.push_back(GetCurrentThread());
  if (is_return_ref) {
    args.push_back(ret_val);
  }
  args.push_back(local_ref_cookie);
  if (is_synchronized) {
    args.push_back(this_or_class);
  }

  llvm::Value* decoded_jobject = irb_->CreateCall(GetRuntimeSupportFunction(func_id), args);

  // Return decoded jobject if return reference.
  if (is_return_ref) {
    ret_val = decoded_jobject;
  }
  return ret_val;
}

void RuntimeSupportBuilder::LockObject(::llvm::Value* object) {
  llvm::Type* prim_int_type = irb_->GetJavaType(Primitive::kPrimInt);
  llvm::MDNode* tbaa_for_monitor_field = irb_->Mdb()->GetTbaaForInstanceField(Primitive::kPrimInt,
                                                                              "I",
                                                                              "Ljava/lang/Object;",
                                                                              "shadow$_monitor_",
                                                                              false);
  llvm::Value* monitor_ptr = irb_->CreateObjectFieldPtr(object, mirror::Object::MonitorOffset(),
                                                        prim_int_type);
  llvm::Value* monitor = irb_->CreateLoad(monitor_ptr, tbaa_for_monitor_field);

  llvm::Value* monitor_without_hash = irb_->CreateAnd(monitor,
                                                      ~(LW_HASH_STATE_MASK << LW_HASH_STATE_SHIFT));

  // Is thin lock, unheld and not recursively acquired.
  llvm::Value* unheld = irb_->CreateICmpEQ(monitor_without_hash, irb_->getInt32(0));

  llvm::BasicBlock* bb_fast = irb_->CreateBasicBlock("lock_fast");
  llvm::BasicBlock* bb_slow = irb_->CreateBasicBlock("lock_slow");
  llvm::BasicBlock* bb_cont = irb_->CreateBasicBlock("lock_cont");
  irb_->CreateCondBr(unheld, bb_fast, bb_slow, ArtIRBuilder::kLikely);

  irb_->SetInsertPoint(bb_fast);

  // Calculate new monitor: new = old | (lock_id << LW_LOCK_OWNER_SHIFT)
  llvm::Value* lock_id = LoadFromThreadOffset(Thread::ThinLockIdOffset(), prim_int_type,
                                              irb_->Mdb()->GetTbaaForThread());

  llvm::Value* owner = irb_->CreateShl(lock_id, LW_LOCK_OWNER_SHIFT);
  llvm::Value* new_monitor = irb_->CreateOr(monitor, owner);

  // Atomically update monitor.
  llvm::Value* old_monitor = irb_->CreateAtomicCmpXchgInst(monitor_ptr, monitor, new_monitor,
                                                           tbaa_for_monitor_field);

  llvm::Value* retry_slow_path = irb_->CreateICmpEQ(old_monitor, monitor);
  irb_->CreateCondBr(retry_slow_path, bb_cont, bb_slow, ArtIRBuilder::kLikely);

  irb_->SetInsertPoint(bb_slow);
  llvm::Function* slow_func = GetRuntimeSupportFunction(kLockObject);
  irb_->CreateCall2(slow_func, object, GetCurrentThread());
  irb_->CreateBr(bb_cont);

  irb_->SetInsertPoint(bb_cont);
}

void RuntimeSupportBuilder::UnlockObject(llvm::Value* object) {
  llvm::Type* prim_int_type = irb_->GetJavaType(Primitive::kPrimInt);

  llvm::Value* lock_id = LoadFromThreadOffset(Thread::ThinLockIdOffset(), prim_int_type,
                                              irb_->Mdb()->GetTbaaForThread());
  llvm::Value* my_monitor = irb_->CreateShl(lock_id, LW_LOCK_OWNER_SHIFT);

  llvm::MDNode* tbaa_for_monitor_field = irb_->Mdb()->GetTbaaForInstanceField(Primitive::kPrimInt,
                                                                              "I",
                                                                              "Ljava/lang/Object;",
                                                                              "shadow$_monitor_",
                                                                              false);
  llvm::Value* monitor_ptr = irb_->CreateObjectFieldPtr(object, mirror::Object::MonitorOffset(),
                                                        prim_int_type);
  llvm::Value* monitor = irb_->CreateLoad(monitor_ptr, tbaa_for_monitor_field);
  llvm::Value* hash_state = irb_->CreateAnd(monitor, (LW_HASH_STATE_MASK << LW_HASH_STATE_SHIFT));
  llvm::Value* real_monitor = irb_->CreateAnd(monitor, ~(LW_HASH_STATE_MASK << LW_HASH_STATE_SHIFT));

  // Is thin lock, held by us and not recursively acquired
  llvm::Value* is_fast_path = irb_->CreateICmpEQ(real_monitor, my_monitor);

  llvm::BasicBlock* bb_fast = irb_->CreateBasicBlock("unlock_fast");
  llvm::BasicBlock* bb_slow = irb_->CreateBasicBlock("unlock_slow");
  llvm::BasicBlock* bb_cont = irb_->CreateBasicBlock("unlock_cont");
  irb_->CreateCondBr(is_fast_path, bb_fast, bb_slow, ArtIRBuilder::kLikely);

  irb_->SetInsertPoint(bb_fast);
  // Set all bits to zero (except hash state)
  irb_->CreateStore(monitor_ptr, hash_state, tbaa_for_monitor_field);
  irb_->CreateBr(bb_cont);

  irb_->SetInsertPoint(bb_slow);
  llvm::Function* slow_func = GetRuntimeSupportFunction(kUnlockObject);
  irb_->CreateCall2(slow_func, object, GetCurrentThread());
  irb_->CreateBr(bb_cont);

  irb_->SetInsertPoint(bb_cont);
}


void RuntimeSupportBuilder::EmitMarkGCCard(llvm::Value* value, llvm::Value* target_addr) {
  llvm::BasicBlock* bb_mark_gc_card = irb_->CreateBasicBlock("mark_gc_card");
  llvm::BasicBlock* bb_cont = irb_->CreateBasicBlock("mark_gc_card_cont");

  llvm::Value* not_null = irb_->CreateIsNotNull(value);
  irb_->CreateCondBr(not_null, bb_mark_gc_card, bb_cont, ArtIRBuilder::kUnknown);

  irb_->SetInsertPoint(bb_mark_gc_card);
  llvm::Value* card_table = LoadFromThreadOffset(Thread::CardTableOffset(),
                                                 irb_->getInt8Ty()->getPointerTo(),
                                                 irb_->Mdb()->GetTbaaForThread());
  llvm::Type* intptr_type = irb_->getInt32Ty();
  llvm::Value* target_addr_int = irb_->CreatePtrToInt(target_addr, intptr_type);
  llvm::Value* card_no = irb_->CreateLShr(target_addr_int, CardTable::kCardShift);
  llvm::Value* card_table_entry = irb_->CreateGEP(card_table, card_no);
  // TODO: its arranged that the low byte of card_table == kCardDirty, use?
  irb_->CreateStore(irb_->getInt8(CardTable::kCardDirty), card_table_entry,
                    irb_->Mdb()->GetTbaaForCardTable());
  irb_->CreateBr(bb_cont);

  irb_->SetInsertPoint(bb_cont);
}

}  // namespace llvm
}  // namespace compiler
}  // namespace art
