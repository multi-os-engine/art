/*
 * Copyright (C) 2008 The Android Open Source Project
 * Copyright 2014-2016 Intel Corporation
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

#include "java_lang_VMClassLoader.h"

#include "class_linker.h"
#include "jni_internal.h"
#include "mirror/class_loader.h"
#include "mirror/object-inl.h"
#include "scoped_fast_native_object_access.h"
#include "ScopedUtfChars.h"
#include "zip_archive.h"

namespace art {

static jclass VMClassLoader_findLoadedClass(JNIEnv* env, jclass, jobject javaLoader,
                                            jstring javaName) {
  ScopedFastNativeObjectAccess soa(env);
  mirror::ClassLoader* loader = soa.Decode<mirror::ClassLoader*>(javaLoader);
  ScopedUtfChars name(env, javaName);
  if (name.c_str() == nullptr) {
    return nullptr;
  }
  ClassLinker* cl = Runtime::Current()->GetClassLinker();
  std::string descriptor(DotToDescriptor(name.c_str()));
  const size_t descriptor_hash = ComputeModifiedUtf8Hash(descriptor.c_str());
  mirror::Class* c = cl->LookupClass(soa.Self(), descriptor.c_str(), descriptor_hash, loader);
  if (c != nullptr && c->IsResolved()) {
    return soa.AddLocalReference<jclass>(c);
  }
  if (loader != nullptr) {
    // Try the common case.
    StackHandleScope<1> hs(soa.Self());
    cl->FindClassInPathClassLoader(soa, soa.Self(), descriptor.c_str(), descriptor_hash,
                                   hs.NewHandle(loader), &c);
    if (c != nullptr) {
      return soa.AddLocalReference<jclass>(c);
    }
  }
  // Class wasn't resolved so it may be erroneous or not yet ready, force the caller to go into
  // the regular loadClass code.
  return nullptr;
}

/*
 * Returns an array of entries from the boot classpath that could contain resources.
 */
static jobjectArray VMClassLoader_getBootClassPathEntries(JNIEnv* env, jclass) {
#ifndef MOE
  const std::vector<const DexFile*>& path =
      Runtime::Current()->GetClassLinker()->GetBootClassPath();
  jclass stringClass = env->FindClass("java/lang/String");
  jobjectArray array = env->NewObjectArray(path.size(), stringClass, nullptr);
  for (size_t i = 0; i < path.size(); ++i) {
    const DexFile* dex_file = path[i];

    // For multidex locations, e.g., x.jar:classes2.dex, we want to look into x.jar.
    const std::string& location(dex_file->GetBaseLocation());

    jstring javaPath = env->NewStringUTF(location.c_str());
    env->SetObjectArrayElement(array, i, javaPath);
  }
#else
  std::vector<std::string> path;
  Split(Runtime::Current()->GetBootClassPathString(), ':', &path);
  jclass stringClass = env->FindClass("java/lang/String");
  jobjectArray array = env->NewObjectArray(path.size(), stringClass, nullptr);
  for (size_t i = 0; i < path.size(); ++i) {
    const std::string& location(path[i]);

    jstring javaPath = env->NewStringUTF(location.c_str());
    env->SetObjectArrayElement(array, i, javaPath);
  }
#endif
  return array;
}

static JNINativeMethod gMethods[] = {
  NATIVE_METHOD(VMClassLoader, findLoadedClass, "!(Ljava/lang/ClassLoader;Ljava/lang/String;)Ljava/lang/Class;"),
  NATIVE_METHOD(VMClassLoader, getBootClassPathEntries, "()[Ljava/lang/String;"),
};

void register_java_lang_VMClassLoader(JNIEnv* env) {
  REGISTER_NATIVE_METHODS("java/lang/VMClassLoader");
}

}  // namespace art
