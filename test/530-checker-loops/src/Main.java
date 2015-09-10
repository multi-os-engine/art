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

//
// Test on loop optimizations.
//
public class Main {

  static int sResult;

  //
  // Various sequence variables where bound checks can be removed from loop.
  //

  /// CHECK-START: int Main.Linear(int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck
  /// CHECK-START: int Main.Linear(int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  private static int Linear(int[] x) {
    int result = 0;
    for (int i = 0; i < x.length; i++) {
      result += x[i];
    }
    return result;
  }

  /// CHECK-START: int Main.LinearDown(int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck
  /// CHECK-START: int Main.LinearDown(int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  private static int LinearDown(int[] x) {
    int result = 0;
    for (int i = x.length - 1; i >= 0; i--) {
      result += x[i];
    }
    return result;
  }

  /// CHECK-START: int Main.LinearObscure(int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck
  /// CHECK-START: int Main.LinearObscure(int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  private static int LinearObscure(int[] x) {
    int result = 0;
    for (int i = x.length - 1; i >= 0; i--) {
      int k = i + 5;
      result += x[k - 5];
    }
    return result;
  }

  /// CHECK-START: int Main.WrapAround(int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck
  /// CHECK-START: int Main.WrapAround(int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  private static int WrapAround(int[] x) {
    // Loop with wrap around (length - 1, 0, 1, 2, ..).
    int w = x.length - 1;
    int result = 0;
    for (int i = 0; i < x.length; i++) {
      result += x[w];
      w = i;
    }
    return result;
  }

  /// CHECK-START: int[] Main.LinearWithParameter(int) BCE (before)
  /// CHECK-DAG: BoundsCheck
  /// CHECK-START: int[] Main.LinearWithParameter(int) BCE (after)
  /// CHECK-NOT: BoundsCheck
  private static int[] LinearWithParameter(int n) {
    int[] x = new int[n];
    for (int i = 0; i < n; i++) {
      x[i] = i;
    }
    return x;
  }

  /// CHECK-START: int Main.PeriodicIdiom(int) BCE (before)
  /// CHECK-DAG: BoundsCheck
  /// CHECK-START: int Main.PeriodicIdiom(int) BCE (after)
  /// CHECK-NOT: BoundsCheck
  private static int PeriodicIdiom(int tc) {
    int[] x = { 1, 3 };
    // Loop with periodic sequence (0, 1).
    int k = 0;
    int result = 0;
    for (int i = 0; i < tc; i++) {
      result += x[k];  // only one used
      k = 1 - k;
    }
    return result;
  }

  /// CHECK-START: int Main.PeriodicSequence2(int) BCE (before)
  /// CHECK-DAG: BoundsCheck
  /// CHECK-START: int Main.PeriodicSequence2(int) BCE (after)
  /// CHECK-NOT: BoundsCheck
  private static int PeriodicSequence2(int tc) {
    int[] x = { 1, 3 };
    // Loop with periodic sequence (0, 1).
    int k = 0;
    int l = 1;
    int result = 0;
    for (int i = 0; i < tc; i++) {
      result += x[k];  // only one used
      int t = l;
      l = k;
      k = t;
    }
    return result;
  }

  /// CHECK-START: int Main.PeriodicSequence4(int) BCE (before)
  /// CHECK-DAG: BoundsCheck
  /// CHECK-DAG: BoundsCheck
  /// CHECK-DAG: BoundsCheck
  /// CHECK-DAG: BoundsCheck
  /// CHECK-START: int Main.PeriodicSequence4(int) BCE (after)
  /// CHECK-NOT: BoundsCheck
  private static int PeriodicSequence4(int tc) {
    int[] x = { 1, 3, 5, 7 };
    // Loop with periodic sequence (0, 1, 2, 3).
    int k = 0;
    int l = 1;
    int m = 2;
    int n = 3;
    int result = 0;
    for (int i = 0; i < tc; i++) {
      result += x[k] + x[l] + x[m] + x[n];  // all used
      int t = n;
      n = k;
      k = l;
      l = m;
      m = t;
    }
    return result;
  }

  //
  // Cases that actually go out of bounds. The verifier
  // ensures exceptions are thrown at the right places.
  //

  private static void LowerOOO(int[] x) {
    for (int i = -1; i < x.length; i++) {
      sResult += x[i];
    }
  }

  private static void UpperOOO(int[] x) {
    for (int i = 0; i <= x.length; i++) {
      sResult += x[i];
    }
  }

  //
  // Verifier.
  //

  public static void main(String[] args) {
    int[] empty = { };
    int[] x = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };

    // Linear and wrap-around.
    expectEquals(0, Linear(empty));
    expectEquals(55, Linear(x));
    expectEquals(0, LinearDown(empty));
    expectEquals(55, LinearDown(x));
    expectEquals(0, LinearObscure(empty));
    expectEquals(55, LinearObscure(x));
    expectEquals(0, WrapAround(empty));
    expectEquals(55, WrapAround(x));

    // Linear with parameter.
    for (int n = 0; n < 32; n++) {
      int[] r = LinearWithParameter(n);
      expectEquals(n, r.length);
      for (int i = 0; i < n; i++) {
        expectEquals(i, r[i]);
      }
    }

    // Periodic adds (1, 3), one at the time.
    expectEquals(0, PeriodicIdiom(-1));
    for (int tc = 0; tc < 32; tc++) {
      int expected = (tc >> 1) << 2;
      if ((tc & 1) != 0)
        expected += 1;
      expectEquals(expected, PeriodicIdiom(tc));
    }

    // Periodic adds (1, 3), one at the time.
    expectEquals(0, PeriodicSequence2(-1));
    for (int tc = 0; tc < 32; tc++) {
      int expected = (tc >> 1) << 2;
      if ((tc & 1) != 0)
        expected += 1;
      expectEquals(expected, PeriodicSequence2(tc));
    }

    // Periodic adds (1, 3, 5, 7), all at once.
    expectEquals(0, PeriodicSequence4(-1));
    for (int tc = 0; tc < 32; tc++) {
      expectEquals(tc * 16, PeriodicSequence4(tc));
    }

    // Lower bound goes OOO.
    sResult = 0;
    try {
      LowerOOO(x);
    } catch (ArrayIndexOutOfBoundsException e) {
      sResult += 1000;
    }
    expectEquals(1000, sResult);

    // Upper bound goes OOO.
    sResult = 0;
    try {
      UpperOOO(x);
    } catch (ArrayIndexOutOfBoundsException e) {
      sResult += 1000;
    }
    expectEquals(1055, sResult);

  }

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}
