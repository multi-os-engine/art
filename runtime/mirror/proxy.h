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

#ifndef ART_RUNTIME_MIRROR_PROXY_H_
#define ART_RUNTIME_MIRROR_PROXY_H_

#include "object.h"

namespace art {

struct ProxyOffsets;

namespace mirror {

// C++ mirror of java.lang.reflect.Proxy.
#ifdef MOE_WINDOWS
#pragma pack(push, 1)
#endif
class MANAGED Proxy FINAL : public Object {
 private:
#if defined(MOE) && defined(__LP64__)
  uint32_t reference_padding_ ATTRIBUTE_UNUSED;
#endif

  HeapReference<Object> h_;

  friend struct art::ProxyOffsets;  // for verifying offset information
  DISALLOW_IMPLICIT_CONSTRUCTORS(Proxy);
};
#ifdef MOE_WINDOWS
#pragma pack(pop)
#endif

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_PROXY_H_
