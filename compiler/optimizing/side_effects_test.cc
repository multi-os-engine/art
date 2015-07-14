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

#include "gtest/gtest.h"
#include "nodes.h"
#include "primitive.h"

namespace art {

//
// Helper methods.
//

void testDefAndUseSanity(SideEffects def, SideEffects use) {
  EXPECT_TRUE(def.HasDefs());
  EXPECT_FALSE(def.HasAllDefs());
  EXPECT_FALSE(def.HasUses());
  EXPECT_FALSE(use.HasDefs());
  EXPECT_FALSE(use.HasAllDefs());
  EXPECT_TRUE(use.HasUses());

  // All-dependences.
  SideEffects all = SideEffects::All();
  EXPECT_TRUE(all.MayDependOn(def));
  EXPECT_FALSE(def.MayDependOn(all));
  EXPECT_FALSE(all.MayDependOn(use));
  EXPECT_TRUE(use.MayDependOn(all));

  // None-dependences.
  SideEffects non = SideEffects::None();
  EXPECT_FALSE(non.MayDependOn(def));
  EXPECT_FALSE(def.MayDependOn(non));
  EXPECT_FALSE(non.MayDependOn(use));
  EXPECT_FALSE(use.MayDependOn(non));
}

void testDefUse(SideEffects def, SideEffects use) {
  testDefAndUseSanity(def, use);

  // Dependence only in one direction.
  EXPECT_FALSE(def.MayDependOn(use));
  EXPECT_TRUE(use.MayDependOn(def));
}

void testNoDefUse(SideEffects def, SideEffects use) {
  testDefAndUseSanity(def, use);

  // No dependence in any direction.
  EXPECT_FALSE(def.MayDependOn(use));
  EXPECT_FALSE(use.MayDependOn(def));
}

//
// Actual tests.
//

TEST(SideEffectsTest, DefUse) {
  // Apply test to each individual primitive type.
  for (Primitive::Type type = Primitive::kPrimNot;
      type <= Primitive::kPrimVoid;
      type = Primitive::Type(type + 1)) {
    // Same primitive type and access type: proper def-use.
    testDefUse(SideEffects::FieldDefOfThisType(type),
               SideEffects::FieldUseOfThisType(type));
    testDefUse(SideEffects::ArrayDefOfThisType(type),
               SideEffects::ArrayUseOfThisType(type));
    // Same primitive type but different access type: no def-use.
    testNoDefUse(SideEffects::FieldDefOfThisType(type),
                 SideEffects::ArrayUseOfThisType(type));
    testNoDefUse(SideEffects::ArrayDefOfThisType(type),
                 SideEffects::FieldUseOfThisType(type));
  }

  // Different primitive type, same access type: no def-use.
  testNoDefUse(SideEffects::FieldDefOfThisType(Primitive::kPrimInt),
               SideEffects::FieldUseOfThisType(Primitive::kPrimFloat));
  testNoDefUse(SideEffects::ArrayDefOfThisType(Primitive::kPrimInt),
               SideEffects::ArrayUseOfThisType(Primitive::kPrimFloat));
  // Everything different: no def-use.
  testNoDefUse(SideEffects::FieldDefOfThisType(Primitive::kPrimInt),
               SideEffects::ArrayUseOfThisType(Primitive::kPrimFloat));
  testNoDefUse(SideEffects::ArrayDefOfThisType(Primitive::kPrimInt),
               SideEffects::FieldUseOfThisType(Primitive::kPrimFloat));
}

TEST(SideEffectsTest, AllDefs) {
  SideEffects defs = SideEffects::None();
  // Keep taking the union of different defs.
  for (Primitive::Type type = Primitive::kPrimNot;
        type <= Primitive::kPrimVoid;
        type = Primitive::Type(type + 1)) {
    EXPECT_FALSE(defs.HasAllDefs());
    defs = defs.Union(SideEffects::FieldDefOfThisType(type));
    EXPECT_FALSE(defs.HasAllDefs());
    defs = defs.Union(SideEffects::ArrayDefOfThisType(type));
  }
  // Only when all defs are added is this true.
  EXPECT_TRUE(defs.HasAllDefs());
}

TEST(SideEffectsTest, BitStrings) {
  // Bit impure, since we inspect internal representation,
  // but still good to know debugging output is correct.
  EXPECT_STREQ(
      "|0000|0000000000|0000000000|0000000000|0000000000|0000000000|0000000000|",
      SideEffects::None().ToString().c_str());
  EXPECT_STREQ(
      "|0000|0000000000|0000000000|1111111111|1111111111|1111111111|1111111111|",
      SideEffects::All().ToString().c_str());
  EXPECT_STREQ(
      "|0000|0000000000|0000000000|0000000000|0000000000|0000000000|0000000001|",
      SideEffects::FieldDefOfThisType(Primitive::kPrimNot).ToString().c_str());
  EXPECT_STREQ(
      "|0000|0000000000|0000000000|0000000000|0000000000|0000000001|0000000000|",
      SideEffects::ArrayDefOfThisType(Primitive::kPrimNot).ToString().c_str());
  EXPECT_STREQ(
      "|0000|0000000000|0000000000|0000000000|0000000001|0000000000|0000000000|",
      SideEffects::FieldUseOfThisType(Primitive::kPrimNot).ToString().c_str());
  EXPECT_STREQ(
      "|0000|0000000000|0000000000|0000000001|0000000000|0000000000|0000000000|",
      SideEffects::ArrayUseOfThisType(Primitive::kPrimNot).ToString().c_str());
  EXPECT_STREQ(
      "|0000|0000000000|0000000000|1000000000|0000000000|0000000000|0000000000|",
      SideEffects::ArrayUseOfThisType(Primitive::kPrimVoid).ToString().c_str());
}

}  // namespace art
