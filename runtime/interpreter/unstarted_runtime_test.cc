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

#include "unstarted_runtime.h"

#include "class_linker.h"
#include "common_runtime_test.h"
#include "handle.h"
#include "handle_scope-inl.h"
#include "mirror/class_loader.h"
#include "mirror/string-inl.h"
#include "runtime.h"
#include "scoped_thread_state_change.h"
#include "thread.h"

namespace art {
namespace interpreter {

class UnstartedRuntimeTest : public CommonRuntimeTest {
 protected:
  // Re-expose all UnstartedRuntime implementations so we don't need to declare a million
  // test friends.

  // Methods that intercept available libcore implementations.
#define UNSTARTED_DIRECT(Name)                                                                \
  static void Unstarted ## Name(Thread* self,                                                 \
                                ShadowFrame* shadow_frame,                                    \
                                JValue* result,                                               \
                                size_t arg_offset)                                            \
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {                                           \
    interpreter::UnstartedRuntime::Unstarted ## Name(self, shadow_frame, result, arg_offset); \
  }

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
#define UNSTARTED_JNI(Name)                                                                    \
  static void UnstartedJNI ## Name(Thread* self,                                               \
                                   mirror::ArtMethod* method,                                  \
                                   mirror::Object* receiver,                                   \
                                   uint32_t* args,                                             \
                                   JValue* result)                                             \
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {                                            \
    interpreter::UnstartedRuntime::UnstartedJNI ## Name(self, method, receiver, args, result); \
  }

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
};

// TODO: Concrete UnstartedRuntime calls.

TEST_F(UnstartedRuntimeTest, StringCharAt) {
  Thread* self = Thread::Current();

  ScopedObjectAccess soa(self);
  // TODO: Actual UTF.
  constexpr const char* base_string = "abcdefghijklmnop";
  mirror::String* test_string = mirror::String::AllocFromModifiedUtf8(self, base_string);

  JValue result;
  ShadowFrame* tmp = ShadowFrame::CreateDeoptimizedFrame(10, nullptr, nullptr, 0);

  for (int32_t i = 0; i < (int32_t)strlen(base_string); ++i) {
    tmp->SetVRegReference(0, test_string);
    tmp->SetVReg(1, i);

    UnstartedStringCharAt(self, tmp, &result, 0);

    EXPECT_EQ(result.GetI(), base_string[i]);
  }

  ShadowFrame::DeleteDeoptimizedFrame(tmp);
}

}  // namespace interpreter
}  // namespace art
