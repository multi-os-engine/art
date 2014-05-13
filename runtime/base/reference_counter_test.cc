/*
 * Copyright (C) 2014 The Android Open Source Project
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

#include "reference_counter.h"

#include "gtest/gtest.h"

namespace art {

TEST(ReferenceCounter, IncAndDec) {
  ReferenceCounter counter;
  ASSERT_EQ(counter.GetCounter(), 0U);
  ASSERT_TRUE(counter.Increment());
  ASSERT_EQ(counter.GetCounter(), 1U);
  ASSERT_FALSE(counter.Increment());
  ASSERT_EQ(counter.GetCounter(), 2U);
  ASSERT_FALSE(counter.Decrement());
  ASSERT_EQ(counter.GetCounter(), 1U);
  ASSERT_TRUE(counter.Decrement());
  ASSERT_EQ(counter.GetCounter(), 0U);
  ASSERT_TRUE(counter.Increment());
  ASSERT_EQ(counter.GetCounter(), 1U);
}

TEST(ReferenceCounter, Init) {
  ReferenceCounter counter;
  ASSERT_EQ(counter.GetCounter(), 0U);
  ASSERT_TRUE(counter.Increment());
  ASSERT_EQ(counter.GetCounter(), 1U);
  counter.Reset();
  ASSERT_EQ(counter.GetCounter(), 0U);
}

}  // namespace art
