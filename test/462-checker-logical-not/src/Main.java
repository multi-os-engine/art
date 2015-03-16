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

public class Main {

  public static void assertBoolEquals(boolean expected, boolean result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void assertIntEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
  
  /**
   * Tiny three-register program exercising int constant folding
   * on negation.
   */

  // CHECK-START: boolean Main.Not(boolean) boolean_not_simplifier (before)
  // CHECK-DAG:     [[Param:z\d+]]    ParameterValue
  // CHECK-DAG:     [[Const0:i\d+]]   IntConstant 0
  // CHECK-DAG:     [[Const1:i\d+]]   IntConstant 1
  // CHECK-DAG:     [[NotEq:z\d+]]    NotEqual [ [[Param]] [[Const0]] ]
  // CHECK-DAG:                       If [ [[NotEq]] ]
  // CHECK-DAG:     [[Phi:i\d+]]      Phi [ [[Const1]] [[Const0]] ]
  // CHECK-DAG:                       Return [ [[Phi]] ]

  // CHECK-START: boolean Main.Not(boolean) boolean_not_simplifier (after)
  // CHECK-DAG:     [[Param:z\d+]]    ParameterValue
  // CHECK-DAG:     [[Const0:i\d+]]   IntConstant 0
  // CHECK-DAG:     [[Eq:z\d+]]       Equal [ [[Param]] [[Const0]] ]
  // CHECK-DAG:                       Return [ [[Eq]] ]

  // CHECK-START: boolean Main.Not(boolean) boolean_not_simplifier (after)
  // CHECK-NOT:                       NotEqual
  // CHECK-NOT:                       If
  // CHECK-NOT:                       Phi
  
  public static boolean Not(boolean x) {
    return !x;
  }

  public static boolean Different(boolean x, boolean y) {
    boolean not_x = !x;
    return not_x == y;
  }

  public static void main(String[] args) {
    assertBoolEquals(false, Not(true));
    assertBoolEquals(true, Not(false));
    assertBoolEquals(false, Different(true, true));
    assertBoolEquals(false, Different(false, false));
    assertBoolEquals(true, Different(false, true));
    assertBoolEquals(true, Different(true, false));
//    assertIntEquals(42, AddOneIfFalse(42, true));
//    assertIntEquals(43, AddOneIfFalse(42, false));
  }
}
