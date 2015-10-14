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

#include "dalvik_system_NativeAllocation.h"

#include "base/macros.h"
#include "jni_internal.h"

namespace art {

typedef void (*FreeFunction)(void*);

static void NativeAllocation_nativeFreeNativeAllocation(JNIEnv*, jclass, jlong ptr, jlong freeFunction) {
  void* nativePtr = reinterpret_cast<void*>(static_cast<uintptr_t>(ptr));
  FreeFunction nativeFree = reinterpret_cast<FreeFunction>(static_cast<uintptr_t>(freeFunction));
  if (nativeFree != nullptr) {
    nativeFree(nativePtr);
  }
}

static JNINativeMethod gMethods[] = {
  NATIVE_METHOD(NativeAllocation, nativeFreeNativeAllocation, "(JJ)V"),
};

void register_dalvik_system_NativeAllocation(JNIEnv* env) {
  REGISTER_NATIVE_METHODS("dalvik/system/NativeAllocation");
}

}  // namespace art
