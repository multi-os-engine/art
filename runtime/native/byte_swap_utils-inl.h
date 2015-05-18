/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ART_RUNTIME_NATIVE_BYTE_SWAP_UTILS_INL_H_
#define ART_RUNTIME_NATIVE_BYTE_SWAP_UTILS_INL_H_

#include "JNIHelp.h"

#if defined(__APPLE__)
#include <libkern/OSByteOrder.h>
#define bswap_16 OSSwapInt16
#define bswap_32 OSSwapInt32
#define bswap_64 OSSwapInt64
#else
#include <byteswap.h>
#endif

namespace art {

// Use packed structures for access to unaligned data on targets with alignment restrictions.
// The compiler will generate appropriate code to access these structures without
// generating alignment exceptions.
template <typename T> inline T GetUnaligned(const T* address) {
  struct unaligned { T v; } __attribute__ ((packed));  // NOLINT(whitespace/parens) mistaken for a function call.
  const unaligned* p = reinterpret_cast<const unaligned*>(address);
  return p->v;
}

template <typename T> inline void PutUnaligned(T* address, T v) {
  struct unaligned { T v; } __attribute__ ((packed));  // NOLINT(whitespace/parens) mistaken for a function call.
  unaligned* p = reinterpret_cast<unaligned*>(address);
  p->v = v;
}

// Byte-swap 2 jshort values packed in a jint.
inline jint bswap_2x16(jint v) {
  // v is initially ABCD
#if defined(__mips__) && defined(__mips_isa_rev) && (__mips_isa_rev >= 2)
  __asm__ volatile ("wsbh %0, %0" : "+r" (v)); // v=BADC NOLINT
#else
  v = bswap_32(v);                          // v=DCBA
  v = (v << 16) | ((v >> 16) & 0xffff);     // v=BADC
#endif
  return v;
}

inline void SwapShorts(jshort* dst_shorts, const jshort* src_shorts, size_t count) {
  // Do 32-bit swaps as long as possible...
  jint* dst = reinterpret_cast<jint*>(dst_shorts);
  const jint* src = reinterpret_cast<const jint*>(src_shorts);
  for (size_t i = 0; i < count / 2; ++i) {
    jint v = GetUnaligned<jint>(src++);
    PutUnaligned<jint>(dst++, bswap_2x16(v));
  }
  if ((count % 2) != 0) {
    jshort v = GetUnaligned<jshort>(reinterpret_cast<const jshort*>(src));
    PutUnaligned<jshort>(reinterpret_cast<jshort*>(dst), bswap_16(v));
  }
}

inline void SwapInts(jint* dst_ints, const jint* src_ints, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    jint v = GetUnaligned<int>(src_ints++);
    PutUnaligned<jint>(dst_ints++, bswap_32(v));
  }
}

inline void SwapLongs(jlong* dst_longs, const jlong* src_longs, size_t count) {
  jint* dst = reinterpret_cast<jint*>(dst_longs);
  const jint* src = reinterpret_cast<const jint*>(src_longs);
  for (size_t i = 0; i < count; ++i) {
    jint v1 = GetUnaligned<jint>(src++);
    jint v2 = GetUnaligned<jint>(src++);
    PutUnaligned<jint>(dst++, bswap_32(v2));
    PutUnaligned<jint>(dst++, bswap_32(v1));
  }
}

}  // namespace art

#endif  // ART_RUNTIME_NATIVE_BYTE_SWAP_UTILS_INL_H_
