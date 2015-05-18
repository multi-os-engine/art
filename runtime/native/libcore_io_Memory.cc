/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "Memory"

#include "JNIHelp.h"
#include "JniConstants.h"
#include "ScopedBytes.h"
#include "ScopedPrimitiveArray.h"
#include "mirror/array.h"
#include "mirror/object.h"
#include "byte_swap_utils-inl.h"
#include "scoped_fast_native_object_access.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

namespace art {

template <typename T> static ALWAYS_INLINE T cast(jlong address) {
  return reinterpret_cast<T>(static_cast<uintptr_t>(address));
}

static void Memory_memmove(JNIEnv* env, jclass, jobject dstObject, jint dstOffset, jobject srcObject, jint srcOffset, jlong length) {
  ScopedBytesRW dstBytes(env, dstObject);
  if (dstBytes.get() == NULL) {
    return;
  }
  ScopedBytesRO srcBytes(env, srcObject);
  if (srcBytes.get() == NULL) {
    return;
  }
  memmove(dstBytes.get() + dstOffset, srcBytes.get() + srcOffset, length);
}

static jbyte Memory_peekByte(JNIEnv*, jclass, jlong srcAddress) {
  return *cast<const jbyte*>(srcAddress);
}

static void Memory_peekByteArray(JNIEnv* env, jclass, jlong srcAddress, jbyteArray dst, jint dstOffset, jint byteCount) {
  env->SetByteArrayRegion(dst, dstOffset, byteCount, cast<const jbyte*>(srcAddress));
}

// Implements the peekXArray methods:
// - For unswapped access, we just use the JNI SetXArrayRegion functions.
// - For swapped access, we use GetXArrayElements and our own copy-and-swap routines.
//  GetXArrayElements is disproportionately cheap on Dalvik because it doesn't copy (as opposed
//  to Hotspot, which always copies). The SWAP_FN copies and swaps in one pass, which is cheaper
//  than copying and then swapping in a second pass. Depending on future VM/GC changes, the
//  swapped case might need to be revisited.
#define PEEKER(SCALAR_TYPE, JNI_NAME, SWAP_TYPE, SWAP_FN) { \
  if (swap) { \
    Scoped ## JNI_NAME ## ArrayRW elements(env, dst); \
    if (elements.get() == NULL) { \
      return; \
    } \
    const SWAP_TYPE* src = cast<const SWAP_TYPE*>(srcAddress); \
    SWAP_FN(reinterpret_cast<SWAP_TYPE*>(elements.get()) + dstOffset, src, count); \
  } else { \
    const SCALAR_TYPE* src = cast<const SCALAR_TYPE*>(srcAddress); \
    env->Set ## JNI_NAME ## ArrayRegion(dst, dstOffset, count, src); \
  } \
}

static void Memory_peekCharArray(JNIEnv* env, jclass, jlong srcAddress, jcharArray dst, jint dstOffset, jint count, jboolean swap) {
  PEEKER(jchar, Char, jshort, SwapShorts);
}

static void Memory_peekDoubleArray(JNIEnv* env, jclass, jlong srcAddress, jdoubleArray dst, jint dstOffset, jint count, jboolean swap) {
  PEEKER(jdouble, Double, jlong, SwapLongs);
}

static void Memory_peekFloatArray(JNIEnv* env, jclass, jlong srcAddress, jfloatArray dst, jint dstOffset, jint count, jboolean swap) {
  PEEKER(jfloat, Float, jint, SwapInts);
}

static void Memory_peekIntArray(JNIEnv* env, jclass, jlong srcAddress, jintArray dst, jint dstOffset, jint count, jboolean swap) {
  PEEKER(jint, Int, jint, SwapInts);
}

static void Memory_peekLongArray(JNIEnv* env, jclass, jlong srcAddress, jlongArray dst, jint dstOffset, jint count, jboolean swap) {
  PEEKER(jlong, Long, jlong, SwapLongs);
}

static void Memory_peekShortArray(JNIEnv* env, jclass, jlong srcAddress, jshortArray dst, jint dstOffset, jint count, jboolean swap) {
  PEEKER(jshort, Short, jshort, SwapShorts);
}

static void Memory_pokeByte(JNIEnv*, jclass, jlong dstAddress, jbyte value) {
  *cast<jbyte*>(dstAddress) = value;
}

static void Memory_pokeByteArray(JNIEnv* env, jclass, jlong dstAddress, jbyteArray src, jint offset, jint length) {
  env->GetByteArrayRegion(src, offset, length, cast<jbyte*>(dstAddress));
}

// Implements the pokeXArray methods:
// - For unswapped access, we just use the JNI GetXArrayRegion functions.
// - For swapped access, we use GetXArrayElements and our own copy-and-swap routines.
//  GetXArrayElements is disproportionately cheap on Dalvik because it doesn't copy (as opposed
//  to Hotspot, which always copies). The SWAP_FN copies and swaps in one pass, which is cheaper
//  than copying and then swapping in a second pass. Depending on future VM/GC changes, the
//  swapped case might need to be revisited.
#define POKER(SCALAR_TYPE, JNI_NAME, SWAP_TYPE, SWAP_FN) { \
  if (swap) { \
    Scoped ## JNI_NAME ## ArrayRO elements(env, src); \
    if (elements.get() == NULL) { \
      return; \
    } \
    const SWAP_TYPE* _poker_src = reinterpret_cast<const SWAP_TYPE*>(elements.get()) + srcOffset; \
    SWAP_FN(cast<SWAP_TYPE*>(dstAddress), _poker_src, count); \
  } else { \
    env->Get ## JNI_NAME ## ArrayRegion(src, srcOffset, count, cast<SCALAR_TYPE*>(dstAddress)); \
  } \
}

static void Memory_pokeCharArray(JNIEnv* env, jclass, jlong dstAddress, jcharArray src, jint srcOffset, jint count, jboolean swap) {
  POKER(jchar, Char, jshort, SwapShorts);
}

static void Memory_pokeDoubleArray(JNIEnv* env, jclass, jlong dstAddress, jdoubleArray src, jint srcOffset, jint count, jboolean swap) {
  POKER(jdouble, Double, jlong, SwapLongs);
}

static void Memory_pokeFloatArray(JNIEnv* env, jclass, jlong dstAddress, jfloatArray src, jint srcOffset, jint count, jboolean swap) {
  POKER(jfloat, Float, jint, SwapInts);
}

static void Memory_pokeIntArray(JNIEnv* env, jclass, jlong dstAddress, jintArray src, jint srcOffset, jint count, jboolean swap) {
  POKER(jint, Int, jint, SwapInts);
}

static void Memory_pokeLongArray(JNIEnv* env, jclass, jlong dstAddress, jlongArray src, jint srcOffset, jint count, jboolean swap) {
  POKER(jlong, Long, jlong, SwapLongs);
}

static void Memory_pokeShortArray(JNIEnv* env, jclass, jlong dstAddress, jshortArray src, jint srcOffset, jint count, jboolean swap) {
  POKER(jshort, Short, jshort, SwapShorts);
}

static jshort Memory_peekShortNative(JNIEnv*, jclass, jlong srcAddress) {
  return GetUnaligned<jshort>(cast<const jshort*>(srcAddress));
}

static void Memory_pokeShortNative(JNIEnv*, jclass, jlong dstAddress, jshort value) {
  PutUnaligned<jshort>(cast<jshort*>(dstAddress), value);
}

static jint Memory_peekIntNative(JNIEnv*, jclass, jlong srcAddress) {
  return GetUnaligned<jint>(cast<const jint*>(srcAddress));
}

static void Memory_pokeIntNative(JNIEnv*, jclass, jlong dstAddress, jint value) {
  PutUnaligned<jint>(cast<jint*>(dstAddress), value);
}

static jlong Memory_peekLongNative(JNIEnv*, jclass, jlong srcAddress) {
  return GetUnaligned<jlong>(cast<const jlong*>(srcAddress));
}

static void Memory_pokeLongNative(JNIEnv*, jclass, jlong dstAddress, jlong value) {
  PutUnaligned<jlong>(cast<jlong*>(dstAddress), value);
}

static void unsafeBulkCopy(jbyte* dst, const jbyte* src, jint byteCount,
    jint sizeofElement, jboolean swap) {
  if (!swap) {
    memcpy(dst, src, byteCount);
    return;
  }

  if (sizeofElement == 2) {
    jshort* dstShorts = reinterpret_cast<jshort*>(dst);
    const jshort* srcShorts = reinterpret_cast<const jshort*>(src);
    SwapShorts(dstShorts, srcShorts, byteCount / 2);
  } else if (sizeofElement == 4) {
    jint* dstInts = reinterpret_cast<jint*>(dst);
    const jint* srcInts = reinterpret_cast<const jint*>(src);
    SwapInts(dstInts, srcInts, byteCount / 4);
  } else if (sizeofElement == 8) {
    jlong* dstLongs = reinterpret_cast<jlong*>(dst);
    const jlong* srcLongs = reinterpret_cast<const jlong*>(src);
    SwapLongs(dstLongs, srcLongs, byteCount / 8);
  }
}

static void Memory_unsafeBulkGet(JNIEnv* env, jclass, jobject dstObject, jint dstOffset,
    jint byteCount, jbyteArray srcArray, jint srcOffset, jint sizeofElement, jboolean swap) {
  ScopedByteArrayRO srcBytes(env, srcArray);
  if (srcBytes.get() == NULL) {
    return;
  }
  jarray dstArray = reinterpret_cast<jarray>(dstObject);
  jbyte* dstBytes = reinterpret_cast<jbyte*>(env->GetPrimitiveArrayCritical(dstArray, NULL));
  if (dstBytes == NULL) {
    return;
  }
  jbyte* dst = dstBytes + dstOffset*sizeofElement;
  const jbyte* src = srcBytes.get() + srcOffset;
  unsafeBulkCopy(dst, src, byteCount, sizeofElement, swap);
  env->ReleasePrimitiveArrayCritical(dstArray, dstBytes, 0);
}

static void Memory_unsafeBulkPut(JNIEnv* env, jclass, jbyteArray dstArray, jint dstOffset,
    jint byteCount, jobject srcObject, jint srcOffset, jint sizeofElement, jboolean swap) {
  ScopedByteArrayRW dstBytes(env, dstArray);
  if (dstBytes.get() == NULL) {
    return;
  }
  jarray srcArray = reinterpret_cast<jarray>(srcObject);
  jbyte* srcBytes = reinterpret_cast<jbyte*>(env->GetPrimitiveArrayCritical(srcArray, NULL));
  if (srcBytes == NULL) {
    return;
  }
  jbyte* dst = dstBytes.get() + dstOffset;
  const jbyte* src = srcBytes + srcOffset*sizeofElement;
  unsafeBulkCopy(dst, src, byteCount, sizeofElement, swap);
  env->ReleasePrimitiveArrayCritical(srcArray, srcBytes, 0);
}

static jint Memory_peekInt(JNIEnv* env, jclass, jbyteArray srcArray, jint srcOffset,
                          jboolean swap) {
  ScopedFastNativeObjectAccess soa(env);
  mirror::Array* src = soa.Decode<mirror::Array*>(srcArray);
  const void* ptr = src->GetRawData(1U /* component_size */, srcOffset);
  const jint value = GetUnaligned<jint>(reinterpret_cast<const jint*>(ptr));
  if (swap == JNI_FALSE) {
    return value;
  } else {
    return bswap_32(value);
  }
}

static jlong Memory_peekLong(JNIEnv* env, jclass, jbyteArray srcArray, jint srcOffset,
                             jboolean swap) {
  ScopedFastNativeObjectAccess soa(env);
  mirror::Array* src = soa.Decode<mirror::Array*>(srcArray);
  const void* ptr = src->GetRawData(1U /* component_size */, srcOffset);
  const jlong value = GetUnaligned<jlong>(reinterpret_cast<const jlong*>(ptr));
  if (swap == JNI_FALSE) {
    return value;
  } else {
    return bswap_64(value);
  }
}

static jshort Memory_peekShort(JNIEnv* env, jclass, jbyteArray srcArray, jint srcOffset,
                              jboolean swap) {
  ScopedFastNativeObjectAccess soa(env);
  mirror::Array* src = soa.Decode<mirror::Array*>(srcArray);
  const void* ptr = src->GetRawData(1U /* component_size */, srcOffset);
  const jshort value = GetUnaligned<jshort>(reinterpret_cast<const jshort*>(ptr));
  if (swap == JNI_FALSE) {
    return value;
  } else {
    return bswap_16(value);
  }
}

static void Memory_pokeInt(JNIEnv* env, jclass, jbyteArray dstArray, jint dstOffset,
                           jint value, jboolean swap) {
  ScopedFastNativeObjectAccess soa(env);
  mirror::Array* src = soa.Decode<mirror::Array*>(dstArray);
  void* ptr = src->GetRawData(1U /* component_size */, dstOffset);

  PutUnaligned<jint>(reinterpret_cast<jint*>(ptr), (swap == JNI_FALSE) ? value : bswap_32(value));
}

static void Memory_pokeLong(JNIEnv* env, jclass, jbyteArray dstArray, jint dstOffset,
                            jlong value, jboolean swap) {
  ScopedFastNativeObjectAccess soa(env);
  mirror::Array* src = soa.Decode<mirror::Array*>(dstArray);
  void* ptr = src->GetRawData(1U /* component_size */, dstOffset);

  PutUnaligned<jlong>(reinterpret_cast<jlong*>(ptr), (swap == JNI_FALSE) ? value : bswap_64(value));
}

static void Memory_pokeShort(JNIEnv* env, jclass, jbyteArray dstArray, jint dstOffset,
                             jshort value, jboolean swap) {
  ScopedFastNativeObjectAccess soa(env);
  mirror::Array* src = soa.Decode<mirror::Array*>(dstArray);
  void* ptr = src->GetRawData(1U /* component_size */, dstOffset);

  PutUnaligned<jshort>(reinterpret_cast<jshort*>(ptr), (swap == JNI_FALSE) ? value : bswap_16(value));
}

static JNINativeMethod gMethods[] = {
  NATIVE_METHOD(Memory, memmove, "(Ljava/lang/Object;ILjava/lang/Object;IJ)V"),
  NATIVE_METHOD(Memory, peekByte, "!(J)B"),
  NATIVE_METHOD(Memory, peekByteArray, "(J[BII)V"),
  NATIVE_METHOD(Memory, peekCharArray, "(J[CIIZ)V"),
  NATIVE_METHOD(Memory, peekDoubleArray, "(J[DIIZ)V"),
  NATIVE_METHOD(Memory, peekFloatArray, "(J[FIIZ)V"),

  NATIVE_METHOD(Memory, peekInt, "!([BIZ)I"),
  NATIVE_METHOD(Memory, peekIntNative, "!(J)I"),
  NATIVE_METHOD(Memory, peekIntArray, "(J[IIIZ)V"),

  NATIVE_METHOD(Memory, peekLong, "!([BIZ)J"),
  NATIVE_METHOD(Memory, peekLongNative, "!(J)J"),
  NATIVE_METHOD(Memory, peekLongArray, "(J[JIIZ)V"),

  NATIVE_METHOD(Memory, peekShort, "!([BIZ)S"),
  NATIVE_METHOD(Memory, peekShortNative, "!(J)S"),
  NATIVE_METHOD(Memory, peekShortArray, "(J[SIIZ)V"),

  NATIVE_METHOD(Memory, pokeByte, "!(JB)V"),
  NATIVE_METHOD(Memory, pokeByteArray, "(J[BII)V"),
  NATIVE_METHOD(Memory, pokeCharArray, "(J[CIIZ)V"),
  NATIVE_METHOD(Memory, pokeDoubleArray, "(J[DIIZ)V"),
  NATIVE_METHOD(Memory, pokeFloatArray, "(J[FIIZ)V"),

  NATIVE_METHOD(Memory, pokeInt, "!([BIIZ)V"),
  NATIVE_METHOD(Memory, pokeIntNative, "!(JI)V"),
  NATIVE_METHOD(Memory, pokeIntArray, "(J[IIIZ)V"),

  NATIVE_METHOD(Memory, pokeLong, "!([BIJZ)V"),
  NATIVE_METHOD(Memory, pokeLongNative, "!(JJ)V"),
  NATIVE_METHOD(Memory, pokeLongArray, "(J[JIIZ)V"),

  NATIVE_METHOD(Memory, pokeShort, "!([BISZ)V"),
  NATIVE_METHOD(Memory, pokeShortNative, "!(JS)V"),
  NATIVE_METHOD(Memory, pokeShortArray, "(J[SIIZ)V"),

  NATIVE_METHOD(Memory, unsafeBulkGet, "(Ljava/lang/Object;II[BIIZ)V"),
  NATIVE_METHOD(Memory, unsafeBulkPut, "([BIILjava/lang/Object;IIZ)V"),
};


void register_libcore_io_Memory(JNIEnv* env) {
  jniRegisterNativeMethods(env, "libcore/io/Memory", gMethods, NELEM(gMethods));
}

}  // namespace art
