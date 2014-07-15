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

#ifndef ART_RUNTIME_NATIVE_BRIDGE_H_
#define ART_RUNTIME_NATIVE_BRIDGE_H_

#include "jni.h"
#include "base/mutex.h"

namespace art {

extern "C" const char* GetMethodShorty(JNIEnv* env, jmethodID mid);
extern "C" int GetNativeMethodCount(JNIEnv* env, jclass clazz);
extern "C" int GetNativeMethods(JNIEnv*, jclass clazz, JNINativeMethod* methods, uint32_t method_count);

// ART interfaces to native-bridge
typedef struct {
  // Log utility, reserve unused
  int   (*logger               )(int prio, const char* tag, const char* fmt, ...);
  // Get shorty of a Java method. The shorty is supposed to be persistent in memory.
  //
  // Parameters:
  //   env [IN] pointer to JNIenv
  //   mid [IN] Java methodID
  // Returns:
  //   short descriptor for method
  const char* (*getMethodShorty)(JNIEnv* env, jmethodID mid);
  // Get number of native methods for specified class.
  //
  // Parameters:
  //   env [IN] pointer to JNIenv
  //   clazz [IN] Java class object
  // Returns:
  //   number of native methods
  int   (*getNativeMethodCount )(JNIEnv* env, jclass clazz);
  // Get at most 'method_count' native methods for specified class 'clazz'. Results are outputed
  // via 'methods' [OUT]. The signature pointer in JNINativeMethod is reused as the method shorty.
  //
  // Parameters:
  //   env [IN] pointer to JNIenv
  //   clazz [IN] Java class object
  //   methods [OUT] array of method with the name, shorty, and fnPtr
  //   method_count [IN] max number of elements in methods
  // Returns:
  //   number of method it actually wrote to methods
  int   (*getNativeMethods     )(JNIEnv* env, jclass clazz, JNINativeMethod* methods,
                                 uint32_t method_count);
} vm_itf_t;

// Native-bridge interfaces to ART
typedef struct {
  // Initialize native-bridge. Native-bridge's internal implementation must ensure MT safety
  // and that native-bridge is initialized only once. OK to call this interface for already
  // initialized native-bridge.
  //
  // Parameters:
  //   vm_itf [IN] the pointer to vm_itf_t callbacks
  // Returns:
  //   TRUE for initialization success, FALSE for initialization fail.
  bool  (*initialize   )(vm_itf_t* vm_itf);
  // Load a shared library that is supported by the native-bridge.
  //
  // Parameters:
  //   libpath [IN] path to the shared library
  //   flag [IN] the stardard RTLD_XXX defined in bionic dlfcn.h
  // Returns:
  //   The opaque handle of shared library if sucessful, otherwise NULL
  void* (*loadLibrary  )(const char* libpath, int flag);
  // Get a native-bridge trampoline for specified native method. The trampoline has same
  // sigature as the native method.
  //
  // Parameters:
  //   handle [IN] the handle returned from loadLibrary
  //   shorty [IN] short descriptor of native method
  //   len [IN] length of shorty
  // Returns:
  //   address of trampoline of successful, otherwise NULL
  void* (*getTrampoline)(void* handle, const char* name, const char* shorty, uint32_t len);
  // Check whether native library is valid and is for an ABI that is supported by native-bridge.
  //
  // Parameters:
  //   libpath [IN] path to the shared library
  // Returns:
  //   TRUE if library is supported by native-bridge, FALSE otherwise
  bool  (*isSupported  )(const char* libpath);
} nb_itf_t;

// Class that wraps the native-bridge interfaces
class NativeBridge {
 public:
  static void* LoadLibrary(const char* libpath, int flag);
  static void* GetTrampoline(void* handle, const char* name, const char* shorty, uint32_t len);
  static bool  IsSupported(const char* libpath);

 private:
  static bool  Init();
  static bool  initialized_ GUARDED_BY(lock_);
  static nb_itf_t* nb_itf_;
  static vm_itf_t  vm_itf_;
  static Mutex lock_;
};

// Default library name for native-brdige
#define DEFAULT_NATIVE_BRIDGE "libnativebridge.so"
// Property that defines the library name of native-bridge
#define PROP_NATIVE_BRIDGE "persist.native.bridge"
// Property that enables native-bridge
#define PROP_ENABLE_NATIVE_BRIDGE "persist.enable.native.bridge"
// The symbol name exposed by native-bridge with the type of nb_itf_t
#define NATIVE_BRIDGE_ITF "NativeBridgeItf"

};  // namespace art

#endif  // ART_RUNTIME_NATIVE_BRIDGE_H_
