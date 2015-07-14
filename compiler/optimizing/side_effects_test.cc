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
  EXPECT_FALSE(def.IsNone());
  EXPECT_FALSE(use.IsNone());

  EXPECT_TRUE(def.HasAnyDef());
  EXPECT_FALSE(def.HasAnyUse());
  EXPECT_FALSE(use.HasAnyDef());
  EXPECT_TRUE(use.HasAnyUse());

  // All-dependences.
  SideEffects all = SideEffects::All();
  EXPECT_TRUE(all.MayDependOn(def));
  EXPECT_FALSE(def.MayDependOn(all));
  EXPECT_FALSE(all.MayDependOn(use));
  EXPECT_TRUE(use.MayDependOn(all));

  // None-dependences.
  SideEffects none = SideEffects::None();
  EXPECT_FALSE(none.MayDependOn(def));
  EXPECT_FALSE(def.MayDependOn(none));
  EXPECT_FALSE(none.MayDependOn(use));
  EXPECT_FALSE(use.MayDependOn(none));
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


TEST(SideEffectsTest, All) {
  SideEffects all = SideEffects::All();
  EXPECT_TRUE(def.HasAnyDef());
  EXPECT_TRUE(def.HasAnyUse());
  EXPECT_FALSE(def.IsNone());
  EXPECT_TRUE(def.IsAll());
}

TEST(SideEffectsTest, None) {
  SideEffects none = SideEffects::None();
  EXPECT_FALSE(none.HasAnyDef());
  EXPECT_FALSE(none.HasAnyUse());
  EXPECT_TRUE(none.IsNone());
  EXPECT_FALSE(none.IsAll());
}

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

TEST(SideEffectsTest, AllDefsAndUses) {
  SideEffects s = SideEffects::None();
  // Keep taking the union of different defs and uses.
  for (Primitive::Type type = Primitive::kPrimNot;
        type <= Primitive::kPrimVoid;
        type = Primitive::Type(type + 1)) {
    EXPECT_FALSE(s.IsAll());
    s = s.Union(SideEffects::FieldDefOfThisType(type));
    s = s.Union(SideEffects::ArrayDefOfThisType(type));
    s = s.Union(SideEffects::FieldUseOfThisType(type));
    s = s.Union(SideEffects::ArrayUseOfThisType(type));
  }
  // Only when all defs and uses are added is this true.
  EXPECT_TRUE(s.IsAll());
}

TEST(SideEffectsTest, BitStrings) {
  EXPECT_STREQ(
      "|||||"
      SideEffects::None().ToString().c_str());
  EXPECT_STREQ(
      "|VDFJISCBZL|VDFJISCBZL|VDFJISCBZL|VDFJISCBZL|"
      SideEffects::All().ToString().c_str());
  EXPECT_STREQ(
      "||||L|"
      SideEffects::FieldDefOfThisType(Primitive::kPrimNot).ToString().c_str());
  EXPECT_STREQ(
      "|||L||"
      SideEffects::ArrayDefOfThisType(Primitive::kPrimNot).ToString().c_str());
  EXPECT_STREQ(
      "||L|||"
      SideEffects::FieldUseOfThisType(Primitive::kPrimNot).ToString().c_str());
  EXPECT_STREQ(
      "|L||||"
      SideEffects::ArrayUseOfThisType(Primitive::kPrimNot).ToString().c_str());
  EXPECT_STREQ(
      "|V||||"
      SideEffects::ArrayUseOfThisType(Primitive::kPrimVoid).ToString().c_str());
}

}  // namespace art
