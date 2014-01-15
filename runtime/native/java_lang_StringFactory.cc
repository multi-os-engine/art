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
#include "mirror/object-inl.h"
#include "mirror/string.h"
#include "scoped_thread_state_change.h"
#include "ScopedLocalRef.h"
#include "ScopedPrimitiveArray.h"

namespace art {

static jstring StringFactory_newStringFromBytes(JNIEnv* env, jclass, jbyteArray java_data,
                                                jint high, jint offset, jint byte_count) {
  ScopedObjectAccess soa(env);
  ScopedByteArrayRO scoped_data(env, java_data);
  int32_t scoped_data_size = scoped_data.size();
  if ((offset | byte_count) < 0 || byte_count > scoped_data_size - offset) {
    ThrowLocation throw_location = soa.Self()->GetCurrentLocationForThrow();
    soa.Self()->ThrowNewExceptionF(throw_location, "Ljava/lang/StringIndexOutOfBoundsException;",
                                   "length=%d; regionStart=%d; regionLength=%d", scoped_data_size,
                                   offset, byte_count);
    return NULL;
  }
  const uint8_t* byte_data = reinterpret_cast<const uint8_t*>(scoped_data.get() + offset);
  mirror::String* result = mirror::String::AllocFromBytes(soa.Self(), byte_count, byte_data, high);
  return soa.AddLocalReference<jstring>(result);
}

static jstring StringFactory_newStringFromChars(JNIEnv* env, jclass, jcharArray java_data,
                                                jint offset, jint char_count) {
  ScopedObjectAccess soa(env);
  ScopedCharArrayRO scoped_data(env, java_data);
  int32_t scoped_data_size = scoped_data.size();
  if ((offset | char_count) < 0 || char_count > scoped_data_size - offset) {
    ThrowLocation throw_location = soa.Self()->GetCurrentLocationForThrow();
    soa.Self()->ThrowNewExceptionF(throw_location, "Ljava/lang/StringIndexOutOfBoundsException;",
                                   "length=%d; regionStart=%d; regionLength=%d", scoped_data_size,
                                   offset, char_count);
    return NULL;
  }
  const uint16_t* char_data = scoped_data.get() + offset;
  mirror::String* result = mirror::String::AllocFromUtf16(soa.Self(), char_count, char_data);
  return soa.AddLocalReference<jstring>(result);
}

static jstring StringFactory_newStringFromCharsNoCheck(JNIEnv* env, jclass, jint offset,
                                                       jint char_count, jcharArray java_data) {
  ScopedObjectAccess soa(env);
  ScopedCharArrayRO scoped_data(env, java_data);
  const uint16_t* char_data = scoped_data.get() + offset;
  mirror::String* result = mirror::String::AllocFromUtf16(soa.Self(), char_count, char_data);
  return soa.AddLocalReference<jstring>(result);
}

static jstring StringFactory_newStringFromString(JNIEnv* env, jclass, jstring to_copy) {
  ScopedObjectAccess soa(env);
  mirror::String* s = soa.Decode<mirror::String*>(to_copy);
  mirror::String* result = mirror::String::AllocFromUtf16(soa.Self(), s->GetLength(), s->GetValue());
  return soa.AddLocalReference<jstring>(result);
}

static JNINativeMethod gMethods[] = {
  NATIVE_METHOD(StringFactory, newStringFromBytes, "!([BIII)Ljava/lang/String;"),
  NATIVE_METHOD(StringFactory, newStringFromChars, "!([CII)Ljava/lang/String;"),
  NATIVE_METHOD(StringFactory, newStringFromCharsNoCheck, "!(II[C)Ljava/lang/String;"),
  NATIVE_METHOD(StringFactory, newStringFromString, "!(Ljava/lang/String;)Ljava/lang/String;"),
};

void register_java_lang_StringFactory(JNIEnv* env) {
  REGISTER_NATIVE_METHODS("java/lang/StringFactory");
}

}  // namespace art
