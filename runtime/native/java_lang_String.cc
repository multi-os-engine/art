/*
 * Copyright (C) 2008 The Android Open Source Project
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

#include "common_throws.h"
#include "jni_internal.h"
#include "mirror/array.h"
#include "mirror/object-inl.h"
#include "mirror/string.h"
#include "scoped_fast_native_object_access.h"
#include "scoped_thread_state_change.h"
#include "ScopedLocalRef.h"
#include "verify_object-inl.h"

namespace art {

static jchar String_charAt(JNIEnv* env, jobject java_this, jint index) {
  ScopedObjectAccess soa(env);
  return soa.Decode<mirror::String*>(java_this)->CharAt(index);
}

static jint String_compareTo(JNIEnv* env, jobject javaThis, jobject javaRhs) {
  ScopedFastNativeObjectAccess soa(env);
  if (UNLIKELY(javaRhs == NULL)) {
    ThrowNullPointerException(NULL, "rhs == null");
    return -1;
  } else {
    return soa.Decode<mirror::String*>(javaThis)->CompareTo(soa.Decode<mirror::String*>(javaRhs));
  }
}

static jstring String_concat(JNIEnv* env, jobject java_this, jobject java_string_arg) {
  ScopedObjectAccess soa(env);
  mirror::String* string_this = soa.Decode<mirror::String*>(java_this);
  mirror::String* string_arg = soa.Decode<mirror::String*>(java_string_arg);
  int32_t length_this = string_this->GetCount();
  int32_t length_arg = string_arg->GetCount();
  if (length_arg > 0 && length_this > 0) {
    int32_t length_total = length_this + length_arg;
    uint16_t* buffer = new uint16_t[length_total];
    memcpy(buffer, string_this->GetValue(), length_this * sizeof(uint16_t));
    memcpy(buffer + length_this, string_arg->GetValue(), length_arg * sizeof(uint16_t));
    mirror::String* result = mirror::String::AllocFromUtf16(soa.Self(), length_total, buffer);
    return soa.AddLocalReference<jstring>(result);
  }
  jobject string_original = (length_this == 0) ? java_string_arg : java_this;
  return reinterpret_cast<jstring>(string_original);
}

static jint String_fastIndexOf(JNIEnv* env, jobject java_this, jint ch, jint start) {
  ScopedFastNativeObjectAccess soa(env);
  // This method does not handle supplementary characters. They're dealt with in managed code.
  DCHECK_LE(ch, 0xffff);

  mirror::String* s = soa.Decode<mirror::String*>(java_this);
  return s->FastIndexOf(ch, start);
}

static jstring String_fastSubstring(JNIEnv* env, jobject java_this, jint start, jint length) {
  ScopedObjectAccess soa(env);
  SirtRef<mirror::String> sirt_string(soa.Self(), soa.Decode<mirror::String*>(java_this));
  mirror::String* result = mirror::String::AllocFromString(soa.Self(), length, sirt_string, start);
  return soa.AddLocalReference<jstring>(result);
}

static jstring String_intern(JNIEnv* env, jobject javaThis) {
  ScopedFastNativeObjectAccess soa(env);
  mirror::String* s = soa.Decode<mirror::String*>(javaThis);
  mirror::String* result = s->Intern();
  return soa.AddLocalReference<jstring>(result);
}

static jcharArray String_toCharArray(JNIEnv* env, jobject java_this) {
  ScopedObjectAccess soa(env);
  mirror::String* s = soa.Decode<mirror::String*>(java_this);
  return soa.AddLocalReference<jcharArray>(s->ToCharArray(soa.Self()));
}

static JNINativeMethod gMethods[] = {
  NATIVE_METHOD(String, charAt, "!(I)C"),
  NATIVE_METHOD(String, compareTo, "!(Ljava/lang/String;)I"),
  NATIVE_METHOD(String, concat, "!(Ljava/lang/String;)Ljava/lang/String;"),
  NATIVE_METHOD(String, fastIndexOf, "!(II)I"),
  NATIVE_METHOD(String, fastSubstring, "!(II)Ljava/lang/String;"),
  NATIVE_METHOD(String, intern, "!()Ljava/lang/String;"),
  NATIVE_METHOD(String, toCharArray, "!()[C"),
};

void register_java_lang_String(JNIEnv* env) {
  REGISTER_NATIVE_METHODS("java/lang/String");
}

}  // namespace art
