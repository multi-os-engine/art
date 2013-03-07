/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include "dex_file-inl.h"
#include "mirror/abstract_method-inl.h"
#include "mirror/field-inl.h"
#include "mirror/object-inl.h"
#include "runtime_support.h"

using namespace art;

#define DEFINE_GET(name, rtype, getter, type, size) \
  extern "C" rtype art_portable_get_static_ ## name(uint32_t field_idx, \
                                                    mirror::AbstractMethod* referrer) \
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) { \
    mirror::Field* field = FindFieldFast(field_idx, referrer, Static ## type ## Read, size); \
    if (LIKELY(field != NULL)) { \
      return field-> getter (field->GetDeclaringClass()); \
    } \
    field = FindFieldFromCode(field_idx, referrer, Thread::Current(), Static ## type ## Read,  size); \
    if (LIKELY(field != NULL)) { \
      return field-> getter (field->GetDeclaringClass()); \
    } \
    return 0; \
  } \
  extern "C" rtype art_portable_get_instance_ ## name(uint32_t field_idx, \
                                                      mirror::AbstractMethod* referrer, \
                                                      mirror::Object* obj) \
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) { \
    mirror::Field* field = FindFieldFast(field_idx, referrer, Instance ## type ## Read, size); \
    if (LIKELY(field != NULL && obj != NULL)) { \
      return field-> getter (obj); \
    } \
    Thread* self = Thread::Current(); \
    field = FindFieldFromCode(field_idx, referrer, self, Instance ## type ## Read,  size); \
    if (LIKELY(field != NULL)) { \
      if (UNLIKELY(obj == NULL)) { \
        ThrowLocation throw_location = self->GetCurrentLocationForThrow(); \
        ThrowNullPointerExceptionForFieldAccess(throw_location, field, true); \
      } else { \
        return field-> getter (obj); \
      } \
    } \
    return 0; \
  }


DEFINE_GET(object, mirror::Object*, GetObj, Object, sizeof(mirror::Object*))
DEFINE_GET(boolean, jboolean, GetBoolean, Primitive, Primitive::FieldSize(Primitive::kPrimBoolean))
DEFINE_GET(byte, jbyte, GetByte, Primitive, Primitive::FieldSize(Primitive::kPrimByte))
DEFINE_GET(char, jchar, GetChar, Primitive, Primitive::FieldSize(Primitive::kPrimChar))
DEFINE_GET(short, jshort, GetShort, Primitive, Primitive::FieldSize(Primitive::kPrimShort))
DEFINE_GET(int, jint, GetInt, Primitive, Primitive::FieldSize(Primitive::kPrimInt))
DEFINE_GET(long, jlong, GetInt, Primitive, Primitive::FieldSize(Primitive::kPrimLong))
DEFINE_GET(float, jfloat, GetFloat, Primitive, Primitive::FieldSize(Primitive::kPrimFloat))
DEFINE_GET(double, jdouble, GetDouble, Primitive, Primitive::FieldSize(Primitive::kPrimDouble))

#define DEFINE_SET(name, atype, setter, type, size) \
  extern "C" int art_portable_set_static_ ## name(uint32_t field_idx, \
                                                  mirror::AbstractMethod* referrer, \
                                                  atype new_value) \
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) { \
    mirror::Field* field = FindFieldFast(field_idx, referrer, Static ## type ## Write, size); \
    if (LIKELY(field != NULL)) { \
      field-> setter (field->GetDeclaringClass(), new_value); \
      return 0; \
    } \
    field = FindFieldFromCode(field_idx, referrer, Thread::Current(), Static ## type ## Write,  size); \
    if (LIKELY(field != NULL)) { \
      field-> setter (field->GetDeclaringClass(), new_value); \
      return 0; \
    } \
    return -1; \
  } \
  extern "C" int art_portable_set_instance_ ## name(uint32_t field_idx, \
                                                    mirror::AbstractMethod* referrer, \
                                                    mirror::Object* obj, \
                                                    atype new_value) \
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) { \
    mirror::Field* field = FindFieldFast(field_idx, referrer, Instance ## type ## Write, size); \
    if (LIKELY(field != NULL && obj != NULL)) { \
      field-> setter (obj, new_value); \
      return 0; \
    } \
    Thread* self = Thread::Current(); \
    field = FindFieldFromCode(field_idx, referrer, self, Instance ## type ## Write,  size); \
    if (LIKELY(field != NULL)) { \
      if (UNLIKELY(obj == NULL)) { \
        ThrowLocation throw_location = self->GetCurrentLocationForThrow(); \
        ThrowNullPointerExceptionForFieldAccess(throw_location, field, true); \
      } else { \
        field-> setter (obj, new_value); \
        return 0; \
      } \
    } \
    return -1; \
  }

DEFINE_SET(object, mirror::Object*, SetObj, Object, sizeof(mirror::Object*))
DEFINE_SET(boolean, jboolean, SetBoolean, Primitive, Primitive::FieldSize(Primitive::kPrimBoolean))
DEFINE_SET(byte, jbyte, SetByte, Primitive, Primitive::FieldSize(Primitive::kPrimByte))
DEFINE_SET(char, jchar, SetChar, Primitive, Primitive::FieldSize(Primitive::kPrimChar))
DEFINE_SET(short, jshort, SetShort, Primitive, Primitive::FieldSize(Primitive::kPrimShort))
DEFINE_SET(int, jint, SetInt, Primitive, Primitive::FieldSize(Primitive::kPrimInt))
DEFINE_SET(long, jlong, SetInt, Primitive, Primitive::FieldSize(Primitive::kPrimLong))
DEFINE_SET(float, jfloat, SetFloat, Primitive, Primitive::FieldSize(Primitive::kPrimFloat))
DEFINE_SET(double, jdouble, SetDouble, Primitive, Primitive::FieldSize(Primitive::kPrimDouble))
