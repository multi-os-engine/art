/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ART_RUNTIME_NATIVE_BRIDGE_ART_INTERFACE_H_
#define ART_RUNTIME_NATIVE_BRIDGE_ART_INTERFACE_H_

#include "jni.h"
#include "mirror/art_method-inl.h"
#include "mirror/class-inl.h"

namespace android {

// ART interfaces to native-bridge.
struct NativeBridgeRuntimeCallbacks {
  // Get shorty of a Java method. The shorty is supposed to be persistent in memory.
  //
  // Parameters:
  //   env [IN] pointer to JNIenv.
  //   mid [IN] Java methodID.
  // Returns:
  //   short descriptor for method.
  const char* (*getMethodShorty)(JNIEnv* env, jmethodID mid);

  // Get number of native methods for specified class.
  //
  // Parameters:
  //   env [IN] pointer to JNIenv.
  //   clazz [IN] Java class object.
  // Returns:
  //   number of native methods.
  uint32_t (*getNativeMethodCount)(JNIEnv* env, jclass clazz);

  // Get at most 'method_count' native methods for specified class 'clazz'. Results are outputed
  // via 'methods' [OUT]. The signature pointer in JNINativeMethod is reused as the method shorty.
  //
  // Parameters:
  //   env [IN] pointer to JNIenv.
  //   clazz [IN] Java class object.
  //   methods [OUT] array of method with the name, shorty, and fnPtr.
  //   method_count [IN] max number of elements in methods.
  // Returns:
  //   number of method it actually wrote to methods.
  uint32_t (*getNativeMethods)(JNIEnv* env, jclass clazz, JNINativeMethod* methods,
                               uint32_t method_count);
};

};  // namespace android

namespace art {

  extern android::NativeBridgeRuntimeCallbacks NativeBridgeArtCallbacks;

};  // namespace art

#endif  // ART_RUNTIME_NATIVE_BRIDGE_ART_INTERFACE_H_
