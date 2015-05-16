/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ART_RUNTIME_INTERPRETER_UNSTARTED_RUNTIME_H_
#define ART_RUNTIME_INTERPRETER_UNSTARTED_RUNTIME_H_

#include "interpreter.h"

#include "dex_file.h"
#include "jvalue.h"

namespace art {

class Thread;
class ShadowFrame;

namespace mirror {

class ArtMethod;
class Object;

}  // namespace mirror

namespace interpreter {

// Support for an unstarted runtime. These are special handwritten implementations for select
// libcore native and non-native methods so we can compile-time initialize classes in the boot
// image.
//
// While it would technically be OK to only expose the public functions, a class was chosen to
// wrap this so the actual implementations are exposed for testing. This is also why the private
// methods are not documented here - they are not intended to be used directly except in
// testing.

class UnstartedRuntime {
 public:
  static void Initialize();

  static void Invoke(Thread* self,
                     const DexFile::CodeItem* code_item,
                     ShadowFrame* shadow_frame,
                     JValue* result,
                     size_t arg_offset)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static void Jni(Thread* self,
                  mirror::ArtMethod* method,
                  mirror::Object* receiver,
                  uint32_t* args,
                  JValue* result)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 private:
  // Methods that intercept available libcore implementations.
#define UNSTARTED_DIRECT(Name)                             \
  static void Unstarted ## Name(Thread* self,              \
                                ShadowFrame* shadow_frame, \
                                JValue* result,            \
                                size_t arg_offset)         \
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)

  UNSTARTED_DIRECT(ClassForName);
  UNSTARTED_DIRECT(ClassForNameLong);
  UNSTARTED_DIRECT(ClassClassForName);
  UNSTARTED_DIRECT(ClassNewInstance);
  UNSTARTED_DIRECT(ClassGetDeclaredField);
  UNSTARTED_DIRECT(VmClassLoaderFindLoadedClass);
  UNSTARTED_DIRECT(VoidLookupType);
  UNSTARTED_DIRECT(SystemArraycopy);
  UNSTARTED_DIRECT(ThreadLocalGet);
  UNSTARTED_DIRECT(MathCeil);
  UNSTARTED_DIRECT(ArtMethodGetMethodName);
  UNSTARTED_DIRECT(ObjectHashCode);
  UNSTARTED_DIRECT(DoubleDoubleToRawLongBits);
  UNSTARTED_DIRECT(DexCacheGetDexNative);
  UNSTARTED_DIRECT(MemoryPeekEntry);
  UNSTARTED_DIRECT(MemoryPeekArrayEntry);
  UNSTARTED_DIRECT(SecurityGetSecurityPropertiesReader);
  UNSTARTED_DIRECT(StringGetCharsNoCheck);
  UNSTARTED_DIRECT(StringCharAt);
  UNSTARTED_DIRECT(StringFactoryNewStringFromChars);
  UNSTARTED_DIRECT(StringFastSubstring);
#undef UNSTARTED_DIRECT

  // Methods that are native.
#define UNSTARTED_JNI(Name)                                   \
  static void UnstartedJNI ## Name(Thread* self,              \
                                   mirror::ArtMethod* method, \
                                   mirror::Object* receiver,  \
                                   uint32_t* args,            \
                                   JValue* result)            \
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)

  UNSTARTED_JNI(VMRuntimeNewUnpaddedArray);
  UNSTARTED_JNI(VMStackGetCallingClassLoader);
  UNSTARTED_JNI(VMStackGetStackClass2);
  UNSTARTED_JNI(MathLog);
  UNSTARTED_JNI(MathExp);
  UNSTARTED_JNI(ClassGetNameNative);
  UNSTARTED_JNI(FloatFloatToRawIntBits);
  UNSTARTED_JNI(FloatIntBitsToFloat);
  UNSTARTED_JNI(ObjectInternalClone);
  UNSTARTED_JNI(ObjectNotifyAll);
  UNSTARTED_JNI(StringCompareTo);
  UNSTARTED_JNI(StringIntern);
  UNSTARTED_JNI(StringFastIndexOf);
  UNSTARTED_JNI(ArrayCreateMultiArray);
  UNSTARTED_JNI(ArrayCreateObjectArray);
  UNSTARTED_JNI(ThrowableNativeFillInStackTrace);
  UNSTARTED_JNI(SystemIdentityHashCode);
  UNSTARTED_JNI(ByteOrderIsLittleEndian);
  UNSTARTED_JNI(UnsafeCompareAndSwapInt);
  UNSTARTED_JNI(UnsafePutObject);
  UNSTARTED_JNI(UnsafeGetArrayBaseOffsetForComponentType);
  UNSTARTED_JNI(UnsafeGetArrayIndexScaleForComponentType);
#undef UNSTARTED_JNI

  static void InitializeInvokeHandlers();
  static void InitializeJNIHandlers();

  friend class UnstartedRuntimeTest;
};

}  // namespace interpreter
}  // namespace art

#endif  // ART_RUNTIME_INTERPRETER_UNSTARTED_RUNTIME_H_
