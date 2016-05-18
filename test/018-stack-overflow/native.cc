/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "base/macros.h"
#include "globals.h"
#include "jni.h"
#include <stdlib.h>
#include <stdio.h>

namespace art {

static void call_java(JNIEnv* env, jclass clazz) {
  jmethodID unsafeStackOverflow = env->GetStaticMethodID(clazz, "unsafeStackOverflow", "()V");
  env->CallStaticVoidMethod(clazz, unsafeStackOverflow);
}

#ifdef SAFE_STACK
static volatile char* sink;
static void consume_unsafe_stack(JNIEnv* env, jclass clazz, uintptr_t target) {
  char buf[4096];
  sink = &buf[0];
  uintptr_t ptr = reinterpret_cast<uintptr_t>(__builtin___get_unsafe_stack_ptr());
  if (ptr > target) {
    consume_unsafe_stack(env, clazz, target);
  } else {
    call_java(env, clazz);
  }
}
#endif

// Use almost all available unsafe stack and then call back into Java.
extern "C" JNIEXPORT jint JNICALL
Java_Main_nativeUnsafeStackAlmostOverflow(JNIEnv* env, jclass clazz) {
#ifdef SAFE_STACK
  uintptr_t base = reinterpret_cast<uintptr_t>(__builtin___get_unsafe_stack_start());
  if (base != 0) {
    // Skip the guard page plus some more in order to safely return to Java.
    uintptr_t target = base + 10 * kPageSize;
    consume_unsafe_stack(env, clazz, target);
  }
#else
  call_java(env, clazz);
#endif
  return 0;
}

}  // namespace art
