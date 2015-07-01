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

#include <assert.h>

#include "jni.h"
#include "scoped_thread_state_change.h"
#include "thread.h"

namespace art {

namespace {

extern "C" JNIEXPORT jint JNICALL Java_Main_perfJniEmptyCall(JNIEnv*, jobject java_this) {
  assert(java_this != nullptr);
  return 0;
}

extern "C" JNIEXPORT jint JNICALL Java_Main_perfSOACall(JNIEnv*, jobject java_this) {
  ScopedObjectAccess soa(Thread::Current());
  assert(java_this != nullptr);
  return 0;
}

extern "C" JNIEXPORT jint JNICALL Java_Main_perfSOAUncheckedCall(JNIEnv*, jobject java_this) {
  ScopedObjectAccessUnchecked soa(Thread::Current());
  assert(java_this != nullptr);
  return 0;
}

}  // namespace

}  // namespace art
