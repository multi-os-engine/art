/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_RUNTIME_MIRROR_STRING_H_
#define ART_RUNTIME_MIRROR_STRING_H_

#include "class.h"
#include "gtest/gtest.h"
#include "object_callbacks.h"

namespace art {

struct StringClassOffsets;
struct StringOffsets;
class StringPiece;

namespace mirror {

// C++ mirror of java.lang.String
class MANAGED String : public Object {
 public:
  enum StringFactoryMethodIndex {
    kEmptyString                = 0xfffffff0,
    kStringFromBytes_B          = 0xfffffff1,
    kStringFromBytes_BI         = 0xfffffff2,
    kStringFromBytes_BII        = 0xfffffff3,
    kStringFromBytes_BIII       = 0xfffffff4,
    kStringFromBytes_BIIString  = 0xfffffff5,
    kStringFromBytes_BString    = 0xfffffff6,
    kStringFromBytes_BIICharset = 0xfffffff7,
    kStringFromBytes_BCharset   = 0xfffffff8,
    kStringFromChars_C          = 0xfffffff9,
    kStringFromChars_CII        = 0xfffffffa,
    kStringFromCharsNoCheck     = 0xfffffffb,
    kStringFromString           = 0xfffffffc,
    kStringFromStringBuffer     = 0xfffffffd,
    kStringFromCodePoints       = 0xfffffffe,
    kStringFromStringBuilder    = 0xffffffff,
  };

  static MemberOffset CountOffset() {
    return OFFSET_OF_OBJECT_MEMBER(String, count_);
  }

  static MemberOffset ValueOffset() {
    return OFFSET_OF_OBJECT_MEMBER(String, value_);
  }

  const uint16_t* GetValue() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    const byte* raw_addr = reinterpret_cast<const byte*>(this) + ValueOffset().Int32Value();
    return reinterpret_cast<const uint16_t*>(raw_addr);
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  size_t SizeOf() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  int32_t GetCount() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetField32<kVerifyFlags>(OFFSET_OF_OBJECT_MEMBER(String, count_), false);
  }

  void SetCount(int32_t new_count) {
    DCHECK_LE(0, new_count);
    // We use non transactional version since we can't undo this write. We also disable checking
    // since it would fail during a transaction.
    SetField32<false, false, kVerifyNone>(OFFSET_OF_OBJECT_MEMBER(String, count_), new_count, false);
  }


  int32_t GetHashCode() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void ComputeHashCode() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  int32_t GetUtfLength() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  uint16_t CharAt(int32_t index) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  String* Intern() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static String* AllocFromString(Thread* self, int32_t string_length, SirtRef<String>& string,
                                int32_t offset, int32_t hash_code = 0)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static String* AllocFromCharArray(Thread* self, int32_t array_length, SirtRef<CharArray>& array,
                                    int32_t offset, int32_t hash_code = 0)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static String* AllocFromUtf16(Thread* self, int32_t utf16_length, const uint16_t* utf16_data_in,
                                int32_t hash_code = 0)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static String* AllocFromByteArray(Thread* self, int32_t byte_length, SirtRef<ByteArray>& array,
                                    int32_t offset, int32_t high_byte = 0, int32_t hash_code = 0)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static String* AllocFromModifiedUtf8(Thread* self, const char* utf)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static String* AllocFromModifiedUtf8(Thread* self, int32_t utf16_length, const char* utf8_data_in)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  template <bool kIsInstrumented>
  static String* Alloc(Thread* self, int32_t utf16_length)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool Equals(const char* modified_utf8) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // TODO: do we need this overload? give it a more intention-revealing name.
  bool Equals(const StringPiece& modified_utf8)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool Equals(String* that) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Compare UTF-16 code point values not in a locale-sensitive manner
  int Compare(int32_t utf16_length, const char* utf8_data_in);

  // TODO: do we need this overload? give it a more intention-revealing name.
  bool Equals(const uint16_t* that_chars, int32_t that_offset,
              int32_t that_length)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Create a modified UTF-8 encoded std::string from a java/lang/String object.
  std::string ToModifiedUtf8() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  int32_t FastIndexOf(int32_t ch, int32_t start) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  int32_t CompareTo(String* other) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  CharArray* ToCharArray(Thread* self) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static Class* GetJavaLangString() {
    DCHECK(java_lang_String_ != NULL);
    return java_lang_String_;
  }

  static void SetClass(Class* java_lang_String);
  static void ResetClass();
  static void VisitRoots(RootCallback* callback, void* arg)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static uint32_t GetStringFactoryMethodIndex(std::string signature);
  static std::string GetStringFactoryMethodSignature(uint32_t index);
  static const char* GetStringFactoryMethodName(std::string signature);
  static ArtMethod* GetStringFactoryMethodForStringInit(std::string signature)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 private:
  void SetHashCode(int32_t new_hash_code) {
    // Hash code is invariant so use non-transactional mode. Also disable check as we may run inside
    // a transaction.
    DCHECK_EQ(0, GetField32(OFFSET_OF_OBJECT_MEMBER(String, hash_code_), false));
    SetField32<false, false, kVerifyNone>(OFFSET_OF_OBJECT_MEMBER(String, hash_code_), new_hash_code, false);
  }

  // Field order required by test "ValidateFieldOrderOfJavaCppUnionClasses".
  int32_t count_;

  uint32_t hash_code_;

  int32_t value_[0];

  static Class* java_lang_String_;

  friend struct art::StringOffsets;  // for verifying offset information
  FRIEND_TEST(ObjectTest, StringLength);  // for SetOffset and SetCount
  DISALLOW_IMPLICIT_CONSTRUCTORS(String);
};

class MANAGED StringClass : public Class {
 private:
  HeapReference<CharArray> ASCII_;
  HeapReference<Object> CASE_INSENSITIVE_ORDER_;
  uint32_t REPLACEMENT_CHAR_;
  int64_t serialVersionUID_;
  friend struct art::StringClassOffsets;  // for verifying offset information
  DISALLOW_IMPLICIT_CONSTRUCTORS(StringClass);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_STRING_H_
