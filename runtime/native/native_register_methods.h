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

#ifndef ART_RUNTIME_NATIVE_NATIVE_REGISTER_METHODS_H_
#define ART_RUNTIME_NATIVE_NATIVE_REGISTER_METHODS_H_

#include "jni.h"

namespace art {

void register_dalvik_system_DexFile(JNIEnv* env);
void register_dalvik_system_VMDebug(JNIEnv* env);
void register_dalvik_system_VMRuntime(JNIEnv* env);
void register_dalvik_system_VMStack(JNIEnv* env);
void register_dalvik_system_ZygoteHooks(JNIEnv* env);
void register_java_lang_Class(JNIEnv* env);
void register_java_lang_DexCache(JNIEnv* env);
void register_java_lang_Object(JNIEnv* env);
void register_java_lang_ref_FinalizerReference(JNIEnv* env);
void register_java_lang_reflect_Array(JNIEnv* env);
void register_java_lang_reflect_Constructor(JNIEnv* env);
void register_java_lang_reflect_Field(JNIEnv* env);
void register_java_lang_reflect_Method(JNIEnv* env);
void register_java_lang_reflect_Proxy(JNIEnv* env);
void register_java_lang_ref_Reference(JNIEnv* env);
void register_java_lang_Runtime(JNIEnv* env);
void register_java_lang_String(JNIEnv* env);
void register_java_lang_System(JNIEnv* env);
void register_java_lang_Thread(JNIEnv* env);
void register_java_lang_Throwable(JNIEnv* env);
void register_java_lang_VMClassLoader(JNIEnv* env);
void register_java_util_concurrent_atomic_AtomicLong(JNIEnv* env);
void register_org_apache_harmony_dalvik_ddmc_DdmServer(JNIEnv* env);
void register_org_apache_harmony_dalvik_ddmc_DdmVmInternal(JNIEnv* env);
void register_sun_misc_Unsafe(JNIEnv* env);

}  // namespace art

#endif  // ART_RUNTIME_NATIVE_NATIVE_REGISTER_METHODS_H_
