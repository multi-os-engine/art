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

#include "utils/string_reference.h"

#include <memory>

#include "gtest/gtest.h"
#include "utils/test_dex_file_builder.h"

namespace art {

TEST(StringReference, ValueComparator) {
  // This is a regression test for the StringReferenceValueComparator using the wrong
  // dex file to get the string data from a StringId. We construct two dex files with
  // just a single string with the same length but different value. This creates dex
  // files that have the same layout, so the byte offset read from the StringId in one
  // dex file, when used in the other dex file still points to valid string data, except
  // that it's the wrong string. Without the fix the strings would then compare equal.
  TestDexFileBuilder builder1;
  builder1.AddString("String1");
  std::unique_ptr<const DexFile> dex_file1 = builder1.Build("dummy location 1");
  ASSERT_EQ(1u, dex_file1->NumStringIds());
  StringReference sr1(dex_file1.get(), 0);

  TestDexFileBuilder builder2;
  builder2.AddString("String2");
  std::unique_ptr<const DexFile> dex_file2 = builder2.Build("dummy location 2");
  ASSERT_EQ(1u, dex_file2->NumStringIds());
  StringReference sr2(dex_file2.get(), 0);

  StringReferenceValueComparator cmp;
  EXPECT_TRUE(cmp(sr1, sr2));  // "String1" < "String2" is true.
  EXPECT_FALSE(cmp(sr2, sr1));  // "String2" < "String1" is false.
}

}  // namespace art
