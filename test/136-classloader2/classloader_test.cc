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

#include <iostream>

#include "base/macros.h"
#include "jni.h"

namespace art {

static jstring GetJstring(JNIEnv* env, const std::string& str) {
  return env->NewStringUTF(str.c_str());
}

extern "C" JNIEXPORT jboolean JNICALL Java_SecondMain_nativeDo(JNIEnv* env,
                                                               jobject obj ATTRIBUTE_UNUSED,
                                                               jobject appLoader,
                                                               jclass appLoaderClass) {
  std::cout << "in native: env=" << env << " appLoader=" << appLoader <<  " appLoaderClass="
            << appLoaderClass;

  jmethodID midFindClass = env->GetMethodID(appLoaderClass, "findClass",
                                            "(Ljava/lang/String;)Ljava/lang/Class;");
  std::cout << "got mid for findClass: " << midFindClass;

  jstring coreClsName = GetJstring(env, "Core");
  jobject coreClass = env->CallObjectMethod(appLoader, midFindClass, coreClsName);

  std::cout << "got core class: " << coreClass;

  jmethodID midHashCode = env->GetMethodID((jclass)coreClass, "hashCode", "()I");
  std::cout << "now calling hash code...";
  jint hash = env->CallIntMethod(coreClass, midHashCode);
  std::cout << "native core hash:" << hash;

  return JNI_TRUE;
}

}  // namespace art
