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

#include "art_method.h"
#include "lambda/art_lambda_method.h"
#include "lambda/closure.h"

#include "utils.h"
#include <numeric>
#include <stdint.h>
#include "gtest/gtest.h"

#define EXPECT_NULL(expected) EXPECT_EQ(reinterpret_cast<const void*>(expected), \
                                        reinterpret_cast<void*>(nullptr));

namespace std {
  using Closure = art::lambda::Closure;

  // Specialize std::default_delete so it knows how to properly delete closures
  // through the way we allocate them in this test.
  //
  // This is test-only because we don't want the rest of Art to do this.
  template <>
  struct default_delete<Closure> {
    void operator()(Closure* closure) const {
      delete[] reinterpret_cast<char*>(closure);
    }
  };
}  // namespace std

namespace art {
namespace lambda {

class ClosureTest : public ::testing::Test {
 public:
  ClosureTest() = default;
  ~ClosureTest() = default;

 protected:
  static void SetUpTestCase() {
  }

  virtual void SetUp() {
    // Create a completely dummy method here.
    // It's "OK" because the Closure never needs to look inside of the ArtMethod
    // (it just needs to be non-null).
    uintptr_t ignore = 0xbadbad;
    fake_method_ = reinterpret_cast<ArtMethod*>(ignore);
  }

  static ::testing::AssertionResult IsResultSuccessful(bool result) {
    if (result) {
      return ::testing::AssertionSuccess();
    } else {
      return ::testing::AssertionFailure();
    }
  }


  // Create a closure that captures the static variables from 'args' by-value.
  // The lambda method's captured variables types must match the ones in 'args'.
  template <typename ... Args>
  static std::unique_ptr<Closure> CreateClosureStaticVariables(ArtLambdaMethod* lambda_method,
                                                               Args&& ... args) {
    constexpr size_t header_size = sizeof(ArtLambdaMethod*);
    const size_t static_size = GetArgsSize(args ...) + header_size;
    EXPECT_GE(static_size, sizeof(Closure));

    // Can't just 'new' the Closure since we don't know the size up front.
    char* closure_as_char_array = new char[static_size];
    Closure* closure_ptr = new (closure_as_char_array) Closure;

    // Set up the data
    closure_ptr->lambda_info_ = lambda_method;
    CopyArgs(closure_ptr->captured_[0].static_variables_, args ...);

    // Make sure the entire thing is deleted once the unique_ptr goes out of scope.
    return std::unique_ptr<Closure>(closure_ptr);  // NOLINT [whitespace/braces] [5]
  }

  template <typename T, typename ... Args>
  static void CopyArgs(uint8_t destination[], T&& arg, Args&& ... args) {
    memcpy(destination, &arg, sizeof(arg));
    CopyArgs(destination + sizeof(arg), args ...);
  }

  // Base case: Done.
  static void CopyArgs(uint8_t destination[]) {
    UNUSED(destination);
  }

  template <typename T, typename ... Args>
  static constexpr size_t GetArgsSize(T&& arg, Args&& ... args) {
    return sizeof(arg) + GetArgsSize(args ...);
  }

  // Base case: Done.
  static constexpr size_t GetArgsSize() {
    return 0;
  }

  template <typename T>
  void TestPrimitive(const char *descriptor, T value) {
    char shorty[] = {*descriptor, '\0'};

    ArtLambdaMethod lambda_method{fake_method_,                    // NOLINT [whitespace/braces] [5]
                                  descriptor,                      // NOLINT [whitespace/blank_line] [2]
                                  shorty,
                                 };

    std::unique_ptr<Closure> closure = CreateClosureStaticVariables(&lambda_method, value);

    EXPECT_EQ(sizeof(ArtLambdaMethod*) + sizeof(value), closure->GetSize());
    EXPECT_EQ(1u, closure->GetNumberCapturedVariables());
    EXPECT_EQ(static_cast<uint32_t>(value), closure->GetCapturedPrimitiveNarrow(0));
  }

  ArtMethod* fake_method_;
};

TEST_F(ClosureTest, TestTrivial) {
  ArtLambdaMethod lambda_method{fake_method_,                    // NOLINT [whitespace/braces] [5]
                                "",  // No captured variables    // NOLINT [whitespace/blank_line] [2]
                                "",  // No captured variables
                               };

  std::unique_ptr<Closure> closure = CreateClosureStaticVariables(&lambda_method);

  EXPECT_EQ(sizeof(ArtLambdaMethod*), closure->GetSize());
  EXPECT_EQ(0u, closure->GetNumberCapturedVariables());
}  // TEST_F

TEST_F(ClosureTest, TestInt) {
  TestPrimitive("I", int32_t(0xdeadbeef));
}  // TEST_F

}  // namespace lambda
}  // namespace art
