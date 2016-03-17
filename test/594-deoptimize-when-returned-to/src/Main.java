/* Copyright (C) 2011 The Android Open Source Project
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

class Foo {
  static Foo sFoo = new Foo();

  // For testing return value of either int or floating-point type.
  public static int i1 = 67890;
  public static double d1 = 3.1415926;

  Foo() {}

  Foo(final float f, final double d, final int i, final long l, final char c) {
    Thread t = new Thread() {
      public void run() {
        specialTestMethod1(c, i, l, f, d);
      }
    };
    t.start();

    // Test a virtual call to make sure we can parse the return result after deoptimization.
    int res = sFoo.sleepReturningInt(500);

    // Deoptimize the current method run by this thread.
    Main.deoptimizeCurrentMethod();

    // Deoptimized when this method is returned to.
    Main.assertIsInterpreted();

    // Deoptimize specialTestMethod's run by thread t.
    Main.deoptimizeSpecialTestMethods();

    try {
      t.join();
    } catch (InterruptedException e) {
      e.printStackTrace();
    }

    // After deoptimization, make sure all parameters and return result.
    // are correct.
    assertIntEquals(Main.i, i);
    assertLongEquals(Main.l, l);
    assertCharEquals(Main.c, c);
    assertFloatEquals(Main.f, f);
    assertDoubleEquals(Main.d, d);
    assertIntEquals(i1, res);
  }

  static boolean sFlag = false;

  static void specialTestMethod1(char c, int i, long l, float f, double d) throws Error {
    // Disable inlining.
    if (sFlag) throw new Error();

    double res = specialTestMethod2(i, f, d, c, l);

    // Deoptimized when this method is returned to.
    Main.assertIsInterpreted();

    // After deoptimization, make sure all parameters and return result.
    // are correct.
    assertIntEquals(Main.i, i);
    assertLongEquals(Main.l, l);
    assertCharEquals(Main.c, c);
    assertFloatEquals(Main.f, f);
    assertDoubleEquals(Main.d, d);
    assertDoubleEquals(d1, res);
  }

  static double specialTestMethod2(int i, float f, double d, char c, long l) throws Error {
    // Disable inlining.
    if (sFlag) throw new Error();

    // Test a static call to make sure we can parse the return result after deoptimizatio.
    double res = sleepReturningDouble(1500);

    // Deoptimized when this method is returned to.
    Main.assertIsInterpreted();

    // After deoptimization, make sure all parameters and return result.
    // are correct.
    assertIntEquals(Main.i, i);
    assertLongEquals(Main.l, l);
    assertCharEquals(Main.c, c);
    assertFloatEquals(Main.f, f);
    assertDoubleEquals(Main.d, d);
    assertDoubleEquals(d1, res);

    return res;
  }

  // Test integer return result after deoptimization.
  int sleepReturningInt(long ms) {
    // Disable inlining.
    if (sFlag) throw new Error();
    try {
      Thread.sleep(ms);
    } catch (InterruptedException e) {
    }
    return i1;
  }

  // Add an inline frame to make sure we can handle inlined frame in stack walking.
  static double sleepReturningDouble(long ms) {
    return sleepReturningDoubleInternal(ms);
  }

  // Test double return result after deoptimization.
  static double sleepReturningDoubleInternal(long ms) {
    // Disable inlining.
    if (sFlag) throw new Error();
    try {
      Thread.sleep(ms);
    } catch (InterruptedException e) {
    }
    return d1;
  }

  public static void assertIntEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void assertLongEquals(long expected, long result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void assertCharEquals(char expected, char result) {
    if (expected != result) {
      // Values are cast to int to display numeric values instead of
      // (UTF-16 encoded) characters.
      throw new Error("Expected: " + (int)expected + ", found: " + (int)result);
    }
  }

  public static void assertFloatEquals(float expected, float result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void assertDoubleEquals(double expected, double result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}

public class Main {
  // For testing local values to make sure they are preserved after deoptimization.
  public static char c = 'x';
  public static int i = 0xdeadbeef;
  public static long l = 0xbaddcafedeadbeefL;
  public static float f = 0.1234F;
  public static double d = 0.5678D;

  public static void main(String[] args) {
    System.loadLibrary(args[0]);
    new Foo(f, d, i, l, c);
  }

  public static native void assertIsInterpreted();
  public static native void deoptimizeCurrentMethod();
  public static native void deoptimizeSpecialTestMethods();
}
