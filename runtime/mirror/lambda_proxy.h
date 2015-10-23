/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ART_RUNTIME_MIRROR_LAMBDA_PROXY_H_
#define ART_RUNTIME_MIRROR_LAMBDA_PROXY_H_

#include "lambda/closure.h"
#include "object.h"

namespace art {

struct LambdaProxyOffsets;

namespace mirror {

// C++ mirror of a lambda proxy. Does not yet have a Java-equivalent source file.
class MANAGED LambdaProxy FINAL : public Object {
 public:
  // Note that the runtime subclasses generate the following static fields:

  // private static java.lang.Class[] interfaces;  // Declared interfaces for the lambda interface.
  static constexpr size_t kStaticFieldIndexInterfaces = 0;
  // private static java.lang.Class[][] throws;    // Maps vtable id to list of classes.
  static constexpr size_t kStaticFieldIndexThrows = 1;
  static constexpr size_t kStaticFieldCount = 2;   // Number of fields total.

  // The offset from the start of 'LambdaProxy' object, to the closure_ field, in bytes.
  // -- This is exposed publically in order to avoid exposing 'closure_' publically.
  // -- Only meant to be used in stubs and other compiled code, not in runtime.
  static inline size_t GetInstanceFieldOffsetClosure() {
    return OFFSETOF_MEMBER(LambdaProxy, closure_);
  }

  // Direct methods available on the class:
  static constexpr size_t kDirectMethodIndexConstructor = 0;  // <init>()V
  static constexpr size_t kDirectMethodCount = 1;             // Only the constructor.

  // Instance fields, present in the base class and every generated subclass:

  // private long closure;
  union {
    lambda::Closure* actual;
    uint64_t padding;         // Don't trip up GetObjectSize checks, since the Java code has a long.
  } closure_;

 private:
  // Friends for generating offset tests:
  friend struct art::LambdaProxyOffsets;              // for verifying offset information

  DISALLOW_IMPLICIT_CONSTRUCTORS(LambdaProxy);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_LAMBDA_PROXY_H_
