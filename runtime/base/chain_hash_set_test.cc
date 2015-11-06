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

#include "chain_hash_set.h"

#include <string>

#include <gtest/gtest.h>
#include "hash_set.h"

namespace art {

struct IsEmptyFnString {
  void MakeEmpty(std::string& item) const {
    item.clear();
  }
  bool IsEmpty(const std::string& item) const {
    return item.empty();
  }
};

struct HashFn {
  size_t operator()(const std::string& str) {
    if (str == "test") {
      return 0;
    }
    if (str == "test1") {
      return 1;
    }
    if (str == "test11") {
      return 1;
    }
    if (str == "test111") {
      return 1;
    }
    if (str == "test2") {
      return 2;
    }
    return 3;
  }
};

TEST(ChainHashSetTester, TestSingleEntry) {
  ChainHashSet<std::string, IsEmptyFnString, HashFn> hash_set;
  HashSet<std::string, IsEmptyFnString> given_hash_set;
  const std::string test_string = "test";
  ASSERT_TRUE(hash_set.Empty());
  ASSERT_EQ(hash_set.Size(), 0U);
  given_hash_set.Insert(test_string);
  // Fill the hash_set with the given data.
  hash_set.Fill(given_hash_set);
  ASSERT_EQ(hash_set.Size(), given_hash_set.Size());
  auto it = hash_set.Find(test_string);
  ASSERT_EQ(*it, test_string);
  // Test erasing.
  auto after_it = hash_set.Erase(it);
  ASSERT_TRUE(after_it == hash_set.end());
  ASSERT_TRUE(hash_set.Empty());
  ASSERT_EQ(hash_set.Size(), 0U);
  it = hash_set.Find(test_string);
  ASSERT_TRUE(it == hash_set.end());
}

TEST(ChainHashSetTester, TestFillWithoutCollisions) {
  ChainHashSet<std::string, IsEmptyFnString, HashFn> hash_set;
  HashSet<std::string, IsEmptyFnString> given_hash_set;
  given_hash_set.Insert("test");
  given_hash_set.Insert("test1");
  given_hash_set.Insert("test2");
  // Fill the hash_set with the given data.
  hash_set.Fill(given_hash_set);
  ASSERT_EQ(hash_set.Size(), given_hash_set.Size());
  // Check all elements exist.
  size_t amount = 0;
  for (const std::string& s : hash_set) {
    auto it = given_hash_set.Find(s);
    ASSERT_TRUE(it != given_hash_set.end());
    ASSERT_EQ(s, *it);
    ++amount;
  }
  ASSERT_TRUE(amount == given_hash_set.Size());
}

TEST(ChainHashSetTester, TestFillWithCollisions) {
  ChainHashSet<std::string, IsEmptyFnString, HashFn> hash_set;
  HashSet<std::string, IsEmptyFnString> given_hash_set;
  given_hash_set.Insert("test");
  given_hash_set.Insert("test1");
  given_hash_set.Insert("test");
  given_hash_set.Insert("test1");
  // Fill the hash_set with the given data.
  hash_set.Fill(given_hash_set);
  ASSERT_EQ(hash_set.Size(), given_hash_set.Size());
  // Check all elements exist.
  size_t amount = 0;
  for (const std::string& s : hash_set) {
    auto it = given_hash_set.Find(s);
    ASSERT_TRUE(it != given_hash_set.end());
    ASSERT_EQ(s, *it);
    ++amount;
  }
  ASSERT_TRUE(amount == given_hash_set.Size());
}

TEST(ChainHashSetTester, TestFindInEmptyTable) {
  ChainHashSet<std::string, IsEmptyFnString, HashFn> hash_set;
  ASSERT_EQ(hash_set.Size(), 0U);
  auto it = hash_set.Find("test");
  ASSERT_TRUE(it == hash_set.end());
}

TEST(ChainHashSetTester, TestFindEmptySlotByHash) {
  ChainHashSet<std::string, IsEmptyFnString, HashFn> hash_set(0.1);
  HashSet<std::string, IsEmptyFnString> given_hash_set;
  // Insert into the slot #0.
  given_hash_set.Insert("test");
  // Fill the hash_set with the given data.
  hash_set.Fill(given_hash_set);
  ASSERT_EQ(hash_set.Size(), given_hash_set.Size());
  // Try to find in the empty slot #4.
  auto it = hash_set.Find("test2");
  ASSERT_TRUE(it == hash_set.end());
}

TEST(ChainHashSetTester, TestFindWithoutCollisions) {
  ChainHashSet<std::string, IsEmptyFnString, HashFn> hash_set;
  HashSet<std::string, IsEmptyFnString> given_hash_set;
  given_hash_set.Insert("test");
  given_hash_set.Insert("test1");
  given_hash_set.Insert("test2");
  // Fill the hash_set with the given data.
  hash_set.Fill(given_hash_set);
  ASSERT_EQ(hash_set.Size(), given_hash_set.Size());
  // Check all elements exist.
  for (auto g_it = given_hash_set.begin(); g_it != given_hash_set.end(); ++g_it) {
    auto it = hash_set.Find(*g_it);
    ASSERT_TRUE(it != hash_set.end());
    ASSERT_EQ(*it, *g_it);
  }
  // Check an element doesn't exist.
  auto it = hash_set.Find("test3");
  ASSERT_TRUE(it == hash_set.end());
}

TEST(ChainHashSetTester, TestFindWithCollisions) {
  ChainHashSet<std::string, IsEmptyFnString, HashFn> hash_set;
  HashSet<std::string, IsEmptyFnString> given_hash_set;
  static constexpr size_t count = 90;
  // Fill the given_hash_set with the same data.
  for (size_t i = 0; i < count/3; ++i) {
    given_hash_set.Insert("test");
    given_hash_set.Insert("test1");
    given_hash_set.Insert("test");
  }
  // Fill the hash_set with the given data.
  hash_set.Fill(given_hash_set);
  ASSERT_EQ(hash_set.Size(), given_hash_set.Size());
  // Check all elements exist.
  for (auto g_it = given_hash_set.begin(); g_it != given_hash_set.end(); ++g_it) {
    auto it = hash_set.Find(*g_it);
    ASSERT_TRUE(it != hash_set.end());
    ASSERT_EQ(*it, *g_it);
  }
  // Check an element doesn't exist.
  auto it = hash_set.Find("test2");
  ASSERT_TRUE(it == hash_set.end());
  it = hash_set.Find("test3");
  ASSERT_TRUE(it == hash_set.end());
}

TEST(ChainHashSetTester, TestEraseLastElementInChain) {
  ChainHashSet<std::string, IsEmptyFnString, HashFn> hash_set;
  HashSet<std::string, IsEmptyFnString> given_hash_set;
  given_hash_set.Insert("test");
  given_hash_set.Insert("test1");
  // Fill the hash_set with the given data.
  hash_set.Fill(given_hash_set);
  ASSERT_EQ(hash_set.Size(), given_hash_set.Size());
  auto it = hash_set.Find("test");
  ASSERT_TRUE(it != hash_set.end());
  auto after_errase_it = hash_set.Erase(it);
  ASSERT_TRUE(after_errase_it == ++it);
  it = hash_set.Find("test");
  ASSERT_TRUE(it == hash_set.end());
  it = hash_set.Find("test1");
  ASSERT_TRUE(it != hash_set.end());
}

TEST(ChainHashSetTester, TestErasePenultimateElementInChain) {
  ChainHashSet<std::string, IsEmptyFnString, HashFn> hash_set;
  HashSet<std::string, IsEmptyFnString> given_hash_set;
  given_hash_set.Insert("test");
  given_hash_set.Insert("test1");
  given_hash_set.Insert("test11");
  // Fill the hash_set with the given data.
  hash_set.Fill(given_hash_set);
  ASSERT_EQ(hash_set.Size(), given_hash_set.Size());
  auto it = hash_set.Find("test1");
  ASSERT_TRUE(it != hash_set.end());
  auto after_errase_it = hash_set.Erase(it);
  ASSERT_TRUE(after_errase_it == it);
  it = hash_set.Find("test1");
  ASSERT_TRUE(it == hash_set.end());
  it = hash_set.Find("test11");
  ASSERT_TRUE(it != hash_set.end());
}

TEST(ChainHashSetTester, TestEraseMidlElementInChain) {
  ChainHashSet<std::string, IsEmptyFnString, HashFn> hash_set;
  HashSet<std::string, IsEmptyFnString> given_hash_set;
  given_hash_set.Insert("test1");
  given_hash_set.Insert("test11");
  given_hash_set.Insert("test111");
  given_hash_set.Insert("test111");
  given_hash_set.Insert("test2");
  // Fill the hash_set with the given data.
  hash_set.Fill(given_hash_set);
  ASSERT_EQ(hash_set.Size(), given_hash_set.Size());
  auto it = hash_set.Find("test1");
  ASSERT_TRUE(it != hash_set.end());
  auto after_errase_it = hash_set.Erase(it);
  ASSERT_TRUE(after_errase_it == it);
  it = hash_set.Find("test1");
  ASSERT_TRUE(it == hash_set.end());
  it = hash_set.Find("test11");
  ASSERT_TRUE(it != hash_set.end());
}

}  // namespace art
