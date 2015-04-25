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

#ifndef ART_RUNTIME_BASE_CASTS_H_
#define ART_RUNTIME_BASE_CASTS_H_

#include <assert.h>
#include <limits>
#include <string.h>
#include <type_traits>

#include "base/logging.h"
#include "base/macros.h"

namespace art {

// Use implicit_cast as a safe version of static_cast or const_cast
// for upcasting in the type hierarchy (i.e. casting a pointer to Foo
// to a pointer to SuperclassOfFoo or casting a pointer to Foo to
// a const pointer to Foo).
// When you use implicit_cast, the compiler checks that the cast is safe.
// Such explicit implicit_casts are necessary in surprisingly many
// situations where C++ demands an exact type match instead of an
// argument type convertable to a target type.
//
// The From type can be inferred, so the preferred syntax for using
// implicit_cast is the same as for static_cast etc.:
//
//   implicit_cast<ToType>(expr)
//
// implicit_cast would have been part of the C++ standard library,
// but the proposal was submitted too late.  It will probably make
// its way into the language in the future.
template<typename To, typename From>
inline To implicit_cast(From const &f) {
  return f;
}

// When you upcast (that is, cast a pointer from type Foo to type
// SuperclassOfFoo), it's fine to use implicit_cast<>, since upcasts
// always succeed.  When you downcast (that is, cast a pointer from
// type Foo to type SubclassOfFoo), static_cast<> isn't safe, because
// how do you know the pointer is really of type SubclassOfFoo?  It
// could be a bare Foo, or of type DifferentSubclassOfFoo.  Thus,
// when you downcast, you should use this macro.  In debug mode, we
// use dynamic_cast<> to double-check the downcast is legal (we die
// if it's not).  In normal mode, we do the efficient static_cast<>
// instead.  Thus, it's important to test in debug mode to make sure
// the cast is legal!
//    This is the only place in the code we should use dynamic_cast<>.
// In particular, you SHOULDN'T be using dynamic_cast<> in order to
// do RTTI (eg code like this:
//    if (dynamic_cast<Subclass1>(foo)) HandleASubclass1Object(foo);
//    if (dynamic_cast<Subclass2>(foo)) HandleASubclass2Object(foo);
// You should design the code some other way not to need this.

template<typename To, typename From>     // use like this: down_cast<T*>(foo);
inline To down_cast(From* f) {                   // so we only accept pointers
  static_assert(std::is_base_of<From, typename std::remove_pointer<To>::type>::value,
                "down_cast unsafe as To is not a subtype of From");

  return static_cast<To>(f);
}

template <class Dest, class Source>
inline Dest bit_cast(const Source& source) {
  // Compile time assertion: sizeof(Dest) == sizeof(Source)
  // A compile error here means your Dest and Source have different sizes.
  static_assert(sizeof(Dest) == sizeof(Source), "sizes should be equal");
  Dest dest;
  memcpy(&dest, &source, sizeof(dest));
  return dest;
}

// A version of static_cast that DCHECKs that the value can be precisely represented
// when converting to Dest.
template <typename Dest, typename Source>
inline Dest dchecked_integral_cast(const Source source) {
  // Compile time assertion: sizeof(Dest) >= sizeof(Source)
  static_assert(sizeof(Dest) >= sizeof(Source), "sizes should be equal");
  // Compile time assertion: Source and Dest are integral types.
  static_assert(std::is_integral<Source>::value, "Source must be integral");
  static_assert(std::is_integral<Dest>::value, "Dest must be integral");
  // Checking only makes sense if Source is signed and Dest is unsigned, or the reverse.
  static_assert(std::is_signed<Source>::value != std::is_signed<Dest>::value,
                "Source and Dest are of the same signedness");

  if (std::is_signed<Source>::value) {
    DCHECK_GE(source, 0);
  } else {
    // Dest is signed. Check is only necessary if sizeof(Dest) == sizeof(Source).
    if (sizeof(Dest) == sizeof(Source)) {
      DCHECK_LE(source, static_cast<Source>(std::numeric_limits<Dest>::max()));
    }
  }
  return static_cast<Dest>(source);
}

}  // namespace art

#endif  // ART_RUNTIME_BASE_CASTS_H_
