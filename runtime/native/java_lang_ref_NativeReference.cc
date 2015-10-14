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

#include "java_lang_ref_NativeReference.h"

#include "base/macros.h"
#include "jni_internal.h"

namespace art {

typedef void (*FreeFunction)(void*);

static void NativeReference_nativeFreeNativeAllocation(JNIEnv*, jclass, jlong ptr, jlong freeFunction) {
  void* nativePtr = reinterpret_cast<void*>(static_cast<uintptr_t>(ptr));
  FreeFunction nativeFree = reinterpret_cast<FreeFunction>(static_cast<uintptr_t>(freeFunction));
  nativeFree(nativePtr);
}

static JNINativeMethod gMethods[] = {
  NATIVE_METHOD(NativeReference, nativeFreeNativeAllocation, "(JJ)V"),
};

void register_java_lang_ref_NativeReference(JNIEnv* env) {
  REGISTER_NATIVE_METHODS("java/lang/ref/NativeReference");
}

}  // namespace art
