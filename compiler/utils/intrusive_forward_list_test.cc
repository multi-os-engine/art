/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include <algorithm>
#include <vector>

#include "gtest/gtest.h"
#include "intrusive_forward_list.h"

namespace art {

struct IFLTestValue {
  IFLTestValue(int v) : hook(), value(v) { }  // NOLINT

  IntrusiveForwardListHook hook;
  int value;
};

bool operator==(const IFLTestValue& lhs, const IFLTestValue& rhs) {
  return lhs.value == rhs.value;
}

bool operator<=(const IFLTestValue& lhs, const IFLTestValue& rhs) {
  return lhs.value < rhs.value;
}

TEST(IntrusiveForwardList, IteratorToConstIterator) {
  IntrusiveForwardList<IFLTestValue> ifl;
  IntrusiveForwardList<IFLTestValue>::iterator begin = ifl.begin();
  IntrusiveForwardList<IFLTestValue>::const_iterator cbegin = ifl.cbegin();
  IntrusiveForwardList<IFLTestValue>::const_iterator converted_begin = begin;
  ASSERT_TRUE(converted_begin == cbegin);
}

TEST(IntrusiveForwardList, IteratorOperators) {
  IntrusiveForwardList<IFLTestValue> ifl;
  ASSERT_TRUE(ifl.begin() == ifl.cbegin());
  ASSERT_FALSE(ifl.begin() != ifl.cbegin());
  ASSERT_TRUE(ifl.end() == ifl.cend());
  ASSERT_FALSE(ifl.end() != ifl.cend());

  ASSERT_TRUE(ifl.begin() == ifl.end());  // Empty.
  ASSERT_FALSE(ifl.begin() != ifl.end());  // Empty.

  IFLTestValue value(1);
  ifl.insert_after(ifl.cbefore_begin(), value);

  ASSERT_FALSE(ifl.begin() == ifl.end());  // Not empty.
  ASSERT_TRUE(ifl.begin() != ifl.end());  // Not empty.
}

TEST(IntrusiveForwardList, ConstructRange) {
  std::vector<IFLTestValue> storage { { 1 }, { 2 }, { 7 } };  // NOLINT
  IntrusiveForwardList<IFLTestValue> ifl(storage.begin(), storage.end());
  ASSERT_EQ(storage.size(), static_cast<size_t>(std::distance(ifl.begin(), ifl.end())));
  ASSERT_TRUE(std::equal(storage.begin(), storage.end(), ifl.begin()));
}

}  // namespace art
