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

public class Main {

  public static void main(String[] args) {
    testNoInline(args);
    System.out.println(staticField);
    testInline(args);
    System.out.println(staticField);
    testNonConstantInputs(args);
    System.out.println(staticField);
    testNonConstantEqual(args);
    System.out.println(staticField);
    testGreaterCondition(args);
    System.out.println(staticField);
  }

  // Test when a condition is the input of the if.

  /// CHECK-START: void Main.testNoInline(java.lang.String[]) dead_code_elimination (before)
  /// CHECK: <<Const0:i\d+>>   IntConstant 0
  /// CHECK:                   If
  /// CHECK: <<Phi:i\d+>>      Phi
  /// CHECK: <<Equal:z\d+>>    Equal [<<Phi>>,<<Const0>>]
  /// CHECK:                   If [<<Equal>>]

  /// CHECK-START: void Main.testNoInline(java.lang.String[]) dead_code_elimination (after)
  /// CHECK:      If
  /// CHECK-NOT:  Phi
  /// CHECK-NOT:  Equal
  /// CHECK-NOT:  If
  public static void testNoInline(String[] args) {
    boolean myVar = false;
    if (args.length == 42) {
      myVar = true;
    } else {
      staticField = 32;
      myVar = false;
    }
    if (myVar) {
      staticField = 12;
    } else {
      staticField = 54;
    }
  }

  // Test when the phi is the input of the if.

  /// CHECK-START: void Main.testInline(java.lang.String[]) dead_code_elimination_final (before)
  /// CHECK-DAG: <<Const0:i\d+>>   IntConstant 0
  /// CHECK-DAG:                   If
  /// CHECK-DAG: <<Phi:i\d+>>      Phi
  /// CHECK-DAG:                   If [<<Phi>>]

  /// CHECK-START: void Main.testInline(java.lang.String[]) dead_code_elimination_final (after)
  /// CHECK:      If
  /// CHECK-NOT:  Phi
  /// CHECK-NOT:  If
  public static void testInline(String[] args) {
    boolean myVar = $inline$doTest(args);
    if (myVar) {
      staticField = 12;
    } else {
      staticField = 54;
    }
  }

  public static boolean $inline$doTest(String[] args) {
    boolean myVar;
    if (args.length == 42) {
      myVar = true;
    } else {
      staticField = 32;
      myVar = false;
    }
    return myVar;
  }

  // Test when one input is not a constant. We can only optimize the constant input.

  /// CHECK-START: void Main.testNonConstantInputs(java.lang.String[]) dead_code_elimination (before)
  /// CHECK-DAG: <<Const34:i\d+>>         IntConstant 34
  /// CHECK-DAG: <<Const42:i\d+>>         IntConstant 42
  /// CHECK-DAG:                          If
  /// CHECK-DAG: <<StaticFieldGet:i\d+>>  StaticFieldGet
  /// CHECK-DAG: <<Phi:i\d+>>             Phi [<<Const34>>,<<StaticFieldGet>>]
  /// CHECK-DAG: <<NotEqual:z\d+>>        NotEqual [<<Phi>>,<<Const42>>]
  /// CHECK-DAG:                          If [<<NotEqual>>]

  /// CHECK-START: void Main.testNonConstantInputs(java.lang.String[]) dead_code_elimination (after)
  /// CHECK-DAG: <<Const42:i\d+>>         IntConstant 42
  /// CHECK-DAG:                          If
  /// CHECK-DAG: <<StaticFieldGet:i\d+>>  StaticFieldGet
  /// CHECK-NOT:                          Phi
  /// CHECK-DAG: <<NotEqual:z\d+>>        NotEqual [<<StaticFieldGet>>,<<Const42>>]
  /// CHECK-DAG:                          If [<<NotEqual>>]
  public static void testNonConstantInputs(String[] args) {
    int a = 42;;
    if (args.length == 42) {
      a = 34;
    } else {
      staticField = 32;
      a = otherStaticField;
    }
    if (a == 42) {
      staticField = 12;
    } else {
      staticField = 54;
    }
  }

  // Test with a condition.

  /// CHECK-START: void Main.testGreaterCondition(java.lang.String[]) dead_code_elimination (before)
  /// CHECK-DAG: <<Const34:i\d+>>         IntConstant 34
  /// CHECK-DAG: <<Const22:i\d+>>         IntConstant 22
  /// CHECK-DAG: <<Const25:i\d+>>         IntConstant 25
  /// CHECK-DAG:                          If
  /// CHECK-DAG: <<Phi:i\d+>>             Phi [<<Const34>>,<<Const22>>]
  /// CHECK-DAG: <<GE:z\d+>>              GreaterThanOrEqual [<<Phi>>,<<Const25>>]
  /// CHECK-DAG:                          If [<<GE>>]

  /// CHECK-START: void Main.testGreaterCondition(java.lang.String[]) dead_code_elimination (after)
  /// CHECK-DAG:                          If
  /// CHECK-NOT:                          Phi
  /// CHECK-NOT:                          GreaterThanOrEqual
  /// CHECK-NOT:                          If
  public static void testGreaterCondition(String[] args) {
    int a = 42;;
    if (args.length == 42) {
      a = 34;
    } else {
      staticField = 32;
      a = 22;
    }
    if (a < 25) {
      staticField = 12;
    } else {
      staticField = 54;
    }
  }

  // Test when comparing non constants.

  /// CHECK-START: void Main.testNonConstantEqual(java.lang.String[]) dead_code_elimination (before)
  /// CHECK-DAG: <<Const34:i\d+>>         IntConstant 34
  /// CHECK-DAG: <<Const42:i\d+>>         IntConstant 42
  /// CHECK-DAG:                          If
  /// CHECK-DAG: <<StaticFieldGet:i\d+>>  StaticFieldGet
  /// CHECK-DAG: <<Phi:i\d+>>             Phi [<<Const34>>,<<StaticFieldGet>>]
  /// CHECK-DAG: <<NotEqual:z\d+>>        NotEqual [<<Phi>>,<<StaticFieldGet>>]
  /// CHECK-DAG:                          If [<<NotEqual>>]

  /// CHECK-START: void Main.testNonConstantEqual(java.lang.String[]) dead_code_elimination (after)
  /// CHECK-DAG: <<Const34:i\d+>>         IntConstant 34
  /// CHECK-DAG:                          If
  /// CHECK-DAG: <<StaticFieldGet:i\d+>>  StaticFieldGet
  /// CHECK-NOT:                          Phi
  /// CHECK-DAG: <<NotEqual:z\d+>>        NotEqual [<<Const34>>,<<StaticFieldGet>>]
  /// CHECK-DAG:                          If [<<NotEqual>>]
  public static void testNonConstantEqual(String[] args) {
    int a = 42;
    int b = otherStaticField;
    if (args.length == 42) {
      a = 34;
    } else {
      staticField = 32;
      a = b;
    }
    if (a == b) {
      staticField = 12;
    } else {
      staticField = 54;
    }
  }

  static int staticField;
  static int otherStaticField;
}
