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

#ifndef ART_RUNTIME_MIRROR_PROXY_INL_H_
#define ART_RUNTIME_MIRROR_PROXY_INL_H_

#include "object.h"
#include "art_field.h"

namespace art {
namespace mirror {

inline ObjectArray<Class>* SynthesizedProxyClass::GetInterfaces() {
  // First static field.
  DCHECK(GetSFields()->Get(0)->IsArtField());
  DCHECK_STREQ(GetSFields()->Get(0)->GetName(), "interfaces");
  return GetFieldObject<ObjectArray<Class>>(SFieldsOffset());
}

inline ObjectArray<ObjectArray<Class>>* SynthesizedProxyClass::GetThrows() {
  // Second static field.
  DCHECK(GetSFields()->Get(1)->IsArtField());
  DCHECK_STREQ(GetSFields()->Get(1)->GetName(), "throws");
  return GetFieldObject<ObjectArray<ObjectArray<Class>>>(
      MemberOffset(SFieldsOffset().Uint32Value() + sizeof(HeapReference<ObjectArray<Class>>)));
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_PROXY_INL_H_
