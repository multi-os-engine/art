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

  // This function always returns 1.
  // We use try catch to prevent the function from being inlined.
  public static int notInlinedFunctionCall(int arg) {
    try {
      return 1 % arg;
    } catch (Exception unused) {
      return 1;
    }
  }

  // Notes:
  // On ARM64 the callee-saved registers are [r19-r30]. So a regexp to check for
  // a valid callee-saved register index is `19|(2[0-9])|30`
  // The caller-saved registers are [r0-r18]. So a regexp to check for a valid
  // caller-saved register index is `[0-9]|(1[0-8])`

  /**
   * Check that a value live across a function call is allocated in a callee
   * saved register.
   */

  // CHECK-START: int Main.liveInCall(int) register (after)
  // CHECK-DAG:     [[Arg:i\d+]]     ParameterValue
  // CHECK-DAG:     [[Const1:i\d+]]  IntConstant 1
  // CHECK-DAG:     [[Add:i\d+]]     Add [ [[Arg]] [[Const1]] ] -> XCore: {{19|(2[0-9])|30}}
  // CHECK-DAG:                      Invoke
  // CHECK-DAG:                      Add
  // CHECK-DAG:                      Return

  public static int liveInCall(int arg) {
    int t1 = arg + 1;
    int t2 = notInlinedFunctionCall(arg);
    return t1 + t2;
  }

  public static void main(String[] args) {
    int arg = 123;
    assertIntEquals(liveInCall(arg), arg + 2);
  }
}
