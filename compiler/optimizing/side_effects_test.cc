/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not read this file except in compliance with the License.
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

#include "gtest/gtest.h"
#include "nodes.h"
#include "primitive.h"

namespace art {

/**
 * Tests for the SideEffects class.
 */

//
// Helper methods.
//

void testWriteAndReadSanity(SideEffects write, SideEffects read) {
  EXPECT_FALSE(write.DoesNone());
  EXPECT_FALSE(read.DoesNone());

  EXPECT_TRUE(write.DoesWrite());
  EXPECT_FALSE(write.DoesRead());
  EXPECT_FALSE(read.DoesWrite());
  EXPECT_TRUE(read.DoesRead());

  // All-dependences.
  SideEffects all = SideEffects::All();
  EXPECT_TRUE(all.MayDependOn(write));
  EXPECT_FALSE(write.MayDependOn(all));
  EXPECT_FALSE(all.MayDependOn(read));
  EXPECT_TRUE(read.MayDependOn(all));

  // None-dependences.
  SideEffects none = SideEffects::None();
  EXPECT_FALSE(none.MayDependOn(write));
  EXPECT_FALSE(write.MayDependOn(none));
  EXPECT_FALSE(none.MayDependOn(read));
  EXPECT_FALSE(read.MayDependOn(none));
}

void testWriteAndReadDependence(SideEffects write, SideEffects read) {
  testWriteAndReadSanity(write, read);

  // Dependence only in one direction.
  EXPECT_FALSE(write.MayDependOn(read));
  EXPECT_TRUE(read.MayDependOn(write));
}

void testNoWriteAndReadDependence(SideEffects write, SideEffects read) {
  testWriteAndReadSanity(write, read);

  // No dependence in any direction.
  EXPECT_FALSE(write.MayDependOn(read));
  EXPECT_FALSE(read.MayDependOn(write));
}

//
// Actual tests.
//

TEST(SideEffectsTest, All) {
  SideEffects all = SideEffects::All();
  EXPECT_TRUE(all.DoesWrite());
  EXPECT_TRUE(all.DoesRead());
  EXPECT_FALSE(all.DoesNone());
  EXPECT_TRUE(all.DoesAll());
}

TEST(SideEffectsTest, None) {
  SideEffects none = SideEffects::None();
  EXPECT_FALSE(none.DoesWrite());
  EXPECT_FALSE(none.DoesRead());
  EXPECT_TRUE(none.DoesNone());
  EXPECT_FALSE(none.DoesAll());
}

TEST(SideEffectsTest, DependencesAndNoDependences) {
  // Apply test to each individual primitive type.
  for (Primitive::Type type = Primitive::kPrimNot;
      type < Primitive::kPrimVoid;
      type = Primitive::Type(type + 1)) {
    // Same primitive type and access type: proper write/read dep.
    testWriteAndReadDependence(
        SideEffects::FieldWriteOfThisType(type),
        SideEffects::FieldReadOfThisType(type));
    testWriteAndReadDependence(
        SideEffects::ArrayWriteOfThisType(type),
        SideEffects::ArrayReadOfThisType(type));
    // Same primitive type but different access type: no write/read dep.
    testNoWriteAndReadDependence(
        SideEffects::FieldWriteOfThisType(type),
        SideEffects::ArrayReadOfThisType(type));
    testNoWriteAndReadDependence(
        SideEffects::ArrayWriteOfThisType(type),
        SideEffects::FieldReadOfThisType(type));
  }

  // Different primitive type, same access type: no write/read dep.
  testNoWriteAndReadDependence(
      SideEffects::FieldWriteOfThisType(Primitive::kPrimInt),
      SideEffects::FieldReadOfThisType(Primitive::kPrimFloat));
  testNoWriteAndReadDependence(
      SideEffects::ArrayWriteOfThisType(Primitive::kPrimInt),
      SideEffects::ArrayReadOfThisType(Primitive::kPrimFloat));
  // Everything different: no write/read dep.
  testNoWriteAndReadDependence(
      SideEffects::FieldWriteOfThisType(Primitive::kPrimInt),
      SideEffects::ArrayReadOfThisType(Primitive::kPrimFloat));
  testNoWriteAndReadDependence(
      SideEffects::ArrayWriteOfThisType(Primitive::kPrimInt),
      SideEffects::FieldReadOfThisType(Primitive::kPrimFloat));
}

TEST(SideEffectsTest, AllWritesAndReads) {
  SideEffects s = SideEffects::None();
  // Keep taking the union of different writes and reads.
  for (Primitive::Type type = Primitive::kPrimNot;
        type < Primitive::kPrimVoid;
        type = Primitive::Type(type + 1)) {
    EXPECT_FALSE(s.DoesAll());
    s = s.Union(SideEffects::FieldWriteOfThisType(type));
    s = s.Union(SideEffects::ArrayWriteOfThisType(type));
    s = s.Union(SideEffects::FieldReadOfThisType(type));
    s = s.Union(SideEffects::ArrayReadOfThisType(type));
  }
  // Only when all writes and reads are added is this true.
  EXPECT_TRUE(s.DoesAll());
}

TEST(SideEffectsTest, BitStrings) {
  EXPECT_STREQ(
      "|||||",
      SideEffects::None().ToString().c_str());
  EXPECT_STREQ(
      "|DFJISCBZL|DFJISCBZL|DFJISCBZL|DFJISCBZL|",
      SideEffects::All().ToString().c_str());
  EXPECT_STREQ(
      "||||L|",
      SideEffects::FieldWriteOfThisType(Primitive::kPrimNot).ToString().c_str());
  EXPECT_STREQ(
      "|||Z||",
      SideEffects::ArrayWriteOfThisType(Primitive::kPrimBoolean).ToString().c_str());
  EXPECT_STREQ(
      "||B|||",
      SideEffects::FieldReadOfThisType(Primitive::kPrimByte).ToString().c_str());
  EXPECT_STREQ(
      "|D||||",
      SideEffects::ArrayReadOfThisType(Primitive::kPrimDouble).ToString().c_str());
  SideEffects s = SideEffects::None();
  s = s.Union(SideEffects::FieldWriteOfThisType(Primitive::kPrimChar));
  s = s.Union(SideEffects::FieldWriteOfThisType(Primitive::kPrimLong));
  s = s.Union(SideEffects::ArrayWriteOfThisType(Primitive::kPrimShort));
  s = s.Union(SideEffects::FieldReadOfThisType(Primitive::kPrimInt));
  s = s.Union(SideEffects::ArrayReadOfThisType(Primitive::kPrimFloat));
  s = s.Union(SideEffects::ArrayReadOfThisType(Primitive::kPrimDouble));
  EXPECT_STREQ(
      "|DF|I|S|JC|",
      s.ToString().c_str());
}

}  // namespace art
