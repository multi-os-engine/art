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

public class Main {

  public int v1, v2;

  public static void assertIntEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  static boolean doThrow = false;

  // This function always returns 1.
  // We use 'throw' to prevent the function from being inlined.
  public static int $opt$noinline$function_call(int arg) {
    if (doThrow) throw new Error();
    return 1 % arg;
  }


  /**
   * Check that a value live across a function call is allocated in a callee
   * saved register.
   */

  /// CHECK-START-X86: int Main.$opt$GenerateSelect(int) generate_selects_x86 (before)
  /// CHECK-DAG:   <<Arg:i\d+>>      ParameterValue
  /// CHECK-DAG:   <<Const100:i\d+>> IntConstant 100
  /// CHECK-DAG:   <<Less:z\d+>>     LessThanOrEqual [<<Arg>>,<<Const100>>]
  /// CHECK-DAG:   <<v1:i\d+>>       InstanceFieldGet
  /// CHECK-DAG:   <<v2:i\d+>>       InstanceFieldGet
  /// CHECK-DAG:   <<phi:i\d+>>      Phi [<<v1>>,<<v2>>]
  /// CHECK-DAG:                     Return [<<phi>>]

  /// CHECK-START-X86: int Main.$opt$GenerateSelect(int) generate_selects_x86 (after)
  /// CHECK-DAG:   <<Arg:i\d+>>      ParameterValue
  /// CHECK-DAG:   <<Const100:i\d+>> IntConstant 100
  /// CHECK-NOT:                     LessThanOrEqual
  /// CHECK:       <<v1:i\d+>>       InstanceFieldGet
  /// CHECK:       <<v2:i\d+>>       InstanceFieldGet
  /// CHECK:       <<select:i\d+>>   ConditionalSelect [<<v1>>,<<v2>>,<<Arg>>,<<Const100>>]
  /// CHECK:                         Return [<<select>>]

  /// CHECK-START-X86_64: int Main.$opt$GenerateSelect(int) generate_selects_x86_64 (before)
  /// CHECK-DAG:   <<Arg:i\d+>>      ParameterValue
  /// CHECK-DAG:   <<Const100:i\d+>> IntConstant 100
  /// CHECK-DAG:   <<Less:z\d+>>     LessThanOrEqual [<<Arg>>,<<Const100>>]
  /// CHECK-DAG:   <<v1:i\d+>>       InstanceFieldGet
  /// CHECK-DAG:   <<v2:i\d+>>       InstanceFieldGet
  /// CHECK-DAG:   <<phi:i\d+>>      Phi [<<v1>>,<<v2>>]
  /// CHECK-DAG:                     Return [<<phi>>]

  /// CHECK-START-X86_64: int Main.$opt$GenerateSelect(int) generate_selects_x86_64 (after)
  /// CHECK-DAG:   <<Arg:i\d+>>      ParameterValue
  /// CHECK-DAG:   <<Const100:i\d+>> IntConstant 100
  /// CHECK-NOT:                     LessThanOrEqual
  /// CHECK:       <<v1:i\d+>>       InstanceFieldGet
  /// CHECK:       <<v2:i\d+>>       InstanceFieldGet
  /// CHECK:       <<select:i\d+>>   ConditionalSelect [<<v1>>,<<v2>>,<<Arg>>,<<Const100>>]
  /// CHECK:                         Return [<<select>>]


  public int $opt$GenerateSelect(int arg) {
    if (arg > 100) {
      return v1;
    }
    return v2;
  }

  public static void main(String[] args) {
    Main p = new Main();
    p.v1 = 100;
    p.v2 = -1000;
    assertIntEquals(p.$opt$GenerateSelect(0), -1000);
    assertIntEquals(p.$opt$GenerateSelect(101), 100);
  }
}
