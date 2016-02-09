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

  static int sv = 0;

  // The instruction order after scheduling depends on the instruction costs and
  // scheduling heuristics. For Cortex-A53, below expected order after scheduler
  // performs better than the order before scheduler.

  /// CHECK-START-ARM64: int Main.arrayAccess(int) scheduler (before)
  /// CHECK:    <<Add:i\d+>>          Add
  /// CHECK:    <<Array:l\d+>>        Arm64IntermediateAddress
  /// CHECK:                          ArrayGet [<<Array>>,<<Add>>]

  /// CHECK-START-ARM64: int Main.arrayAccess(int) scheduler (after)
  /// CHECK:    <<Array:l\d+>>        Arm64IntermediateAddress
  /// CHECK:    <<Add:i\d+>>          Add
  /// CHECK:                          ArrayGet [<<Array>>,<<Add>>]
  public static int arrayAccess(int arg1) {
    int res = 0;
    int [] array = new int[10];
    for (int i = 1; i < 10; i++) {
      res -= array[i-1];
      array[i] += res;
    }
    return res;
  }

  /// CHECK-START-ARM64: int Main.intDiv(int) scheduler (before)
  /// CHECK:               Sub
  /// CHECK:               DivZeroCheck
  /// CHECK:               Div
  /// CHECK:               StaticFieldSet

  /// CHECK-START-ARM64: int Main.intDiv(int) scheduler (after)
  /// CHECK:               Sub
  /// CHECK-NOT:           StaticFieldSet
  /// CHECK:               DivZeroCheck
  /// CHECK-NOT:           Sub
  /// CHECK:               Div
  public static int intDiv(int arg) {
    int res = 0;
    int tmp = arg;
    for (int i = 1; i < arg; i++) {
      tmp -= i;
      res = res/i;  // div-zero check barrier.
      sv++;
    }
    res += tmp;
    return res;
  }

  public static void main(String[] args) {
    if ((arrayAccess(10) + intDiv(10)) != -35) {
      System.out.println("FAIL");
    }
  }
}
