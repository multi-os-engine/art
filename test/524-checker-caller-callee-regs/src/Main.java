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

  //                               | registers available to | regexp
  //                               | the register allocator |
  // ------------------------------|------------------------|-----------------
  // ARM64 callee-saved registers  | [x20-x29]              | x2[0-9]
  // ARM callee-saved registers    | [r4-r11]               | r([4-9]|10|11)

  /**
   * Check that a value live across a function call is allocated in a callee
   * saved register.
   */

  /// CHECK-arm-START:   int Main.$opt$LiveInCall(int) register (after)
  /// CHECK-arm-DAG:     <<Arg:i\d+>>     ParameterValue
  /// CHECK-arm-DAG:     <<Const1:i\d+>>  IntConstant 1
  /// CHECK-arm:         <<t1:i\d+>>      Add [<<Arg>>,<<Const1>>] {{.*->r([4-8]|10|11)}}
  /// CHECK-arm:         <<t2:i\d+>>      InvokeStaticOrDirect
  /// CHECK-arm:                          Sub [<<t1>>,<<t2>>]
  /// CHECK-arm:                          Return

  /// CHECK-arm64-START: int Main.$opt$LiveInCall(int) register (after)
  /// CHECK-arm64-DAG:   <<Arg:i\d+>>     ParameterValue
  /// CHECK-arm64-DAG:   <<Const1:i\d+>>  IntConstant 1
  /// CHECK-arm64:       <<t1:i\d+>>      Add [<<Arg>>,<<Const1>>] {{.*->x2[0-9]}}
  /// CHECK-arm64:       <<t2:i\d+>>      InvokeStaticOrDirect
  /// CHECK-arm64:                        Sub [<<t1>>,<<t2>>]
  /// CHECK-arm64:                        Return

  // TODO: Add tests for other architectures.

  public static int $opt$LiveInCall(int arg) {
    int t1 = arg + 1;
    int t2 = $opt$noinline$function_call(arg);
    return t1 - t2;
  }

  public static void main(String[] args) {
    int arg = 123;
    assertIntEquals($opt$LiveInCall(arg), arg);
  }
}
