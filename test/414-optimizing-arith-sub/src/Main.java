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

// Note that $opt$ is a marker for the optimizing compiler to ensure
// it does compile the method.
public class Main {

  public static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void expectEquals(long expected, long result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void expectEquals(float expected, float result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void expectEquals(double expected, double result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void expectApproxEquals(float a, float b) {
    float maxDelta = 0.0001F;
    boolean aproxEquals = (a > b) ? ((a - b) < maxDelta) : ((b - a) < maxDelta);
    if (!aproxEquals) {
      throw new Error("Expected: " + a + ", found: " + b + ", with delta: " + maxDelta + " " + (a - b));
    }
  }

  public static void expectApproxEquals(double a, double b) {
    double maxDelta = 0.00001D;
    boolean aproxEquals = (a > b) ? ((a - b) < maxDelta) : ((b - a) < maxDelta);
    if (!aproxEquals) {
      throw new Error("Expected: " + a + ", found: " + b + ", with delta: " + maxDelta + " " + (a - b));
    }
  }

  public static void expectNaN(float a) {
    if (a == a) {
      throw new Error("Expected NaN: " + a);
    }
  }

  public static void expectNaN(double a) {
    if (a == a) {
      throw new Error("Expected NaN: " + a);
    }
  }

  public static void main(String[] args) {
    subInt();
    subLong();
    subFloat();
    subDouble();
  }

  private static void subInt() {
    expectEquals(2, $opt$noinline$Sub(5, 3));
    expectEquals(0, $opt$noinline$Sub(0, 0));
    expectEquals(-3, $opt$noinline$Sub(0, 3));
    expectEquals(3, $opt$noinline$Sub(3, 0));
    expectEquals(4, $opt$noinline$Sub(1, -3));
    expectEquals(-9, $opt$noinline$Sub(-12, -3));
    expectEquals(134217724, $opt$noinline$Sub(134217729, 5)); // (2^27 + 1) - 5
  }

  private static void subLong() {
    expectEquals(2L, $opt$noinline$Sub(5L, 3L));
    expectEquals(0L, $opt$noinline$Sub(0L, 0L));
    expectEquals(-3L, $opt$noinline$Sub(0L, 3L));
    expectEquals(3L, $opt$noinline$Sub(3L, 0L));
    expectEquals(4L, $opt$noinline$Sub(1L, -3L));
    expectEquals(-9L, $opt$noinline$Sub(-12L, -3L));
    expectEquals(134217724L, $opt$noinline$Sub(134217729L, 5L)); // (2^27 + 1) - 5
    expectEquals(34359738362L, $opt$noinline$Sub(34359738369L, 7L)); // (2^35 + 1) - 7
  }

  private static void subFloat() {
    expectApproxEquals(2F, $opt$noinline$Sub(5F, 3F));
    expectApproxEquals(0F, $opt$noinline$Sub(0F, 0F));
    expectApproxEquals(-3F, $opt$noinline$Sub(0F, 3F));
    expectApproxEquals(3F, $opt$noinline$Sub(3F, 0F));
    expectApproxEquals(4F, $opt$noinline$Sub(1F, -3F));
    expectApproxEquals(-9F, $opt$noinline$Sub(-12F, -3F));
    expectApproxEquals(34359738362F, $opt$noinline$Sub(34359738369F, 7F)); // (2^35 + 1) - 7
    expectApproxEquals(-0.1F, $opt$noinline$Sub(0.1F, 0.2F));
    expectApproxEquals(0.2F, $opt$noinline$Sub(-0.5F, -0.7F));

    expectNaN($opt$noinline$Sub(Float.NEGATIVE_INFINITY, Float.NEGATIVE_INFINITY));
    expectNaN($opt$noinline$Sub(Float.POSITIVE_INFINITY, Float.POSITIVE_INFINITY));
    expectNaN($opt$noinline$Sub(Float.NaN, 11F));
    expectNaN($opt$noinline$Sub(Float.NaN, -11F));
    expectNaN($opt$noinline$Sub(Float.NaN, Float.NEGATIVE_INFINITY));
    expectNaN($opt$noinline$Sub(Float.NaN, Float.POSITIVE_INFINITY));

    expectEquals(Float.NEGATIVE_INFINITY, $opt$noinline$Sub(-Float.MAX_VALUE, Float.MAX_VALUE));
    expectEquals(Float.NEGATIVE_INFINITY, $opt$noinline$Sub(2F, Float.POSITIVE_INFINITY));
    expectEquals(Float.POSITIVE_INFINITY, $opt$noinline$Sub(Float.MAX_VALUE, -Float.MAX_VALUE));
    expectEquals(Float.POSITIVE_INFINITY, $opt$noinline$Sub(2F, Float.NEGATIVE_INFINITY));
    expectEquals(Float.POSITIVE_INFINITY, $opt$noinline$Sub(Float.POSITIVE_INFINITY, Float.NEGATIVE_INFINITY));
    expectEquals(Float.NEGATIVE_INFINITY, $opt$noinline$Sub(Float.NEGATIVE_INFINITY, Float.POSITIVE_INFINITY));
  }

  private static void subDouble() {
    expectApproxEquals(2D, $opt$noinline$Sub(5D, 3D));
    expectApproxEquals(0D, $opt$noinline$Sub(0D, 0D));
    expectApproxEquals(-3D, $opt$noinline$Sub(0D, 3D));
    expectApproxEquals(3D, $opt$noinline$Sub(3D, 0D));
    expectApproxEquals(4D, $opt$noinline$Sub(1D, -3D));
    expectApproxEquals(-9D, $opt$noinline$Sub(-12D, -3D));
    expectApproxEquals(134217724D, $opt$noinline$Sub(134217729D, 5D)); // (2^27 + 1) - 5
    expectApproxEquals(34359738362D, $opt$noinline$Sub(34359738369D, 7D)); // (2^35 + 1) - 7
    expectApproxEquals(-0.1D, $opt$noinline$Sub(0.1D, 0.2D));
    expectApproxEquals(0.2D, $opt$noinline$Sub(-0.5D, -0.7D));

    expectNaN($opt$noinline$Sub(Double.NEGATIVE_INFINITY, Double.NEGATIVE_INFINITY));
    expectNaN($opt$noinline$Sub(Double.POSITIVE_INFINITY, Double.POSITIVE_INFINITY));
    expectNaN($opt$noinline$Sub(Double.NaN, 11D));
    expectNaN($opt$noinline$Sub(Double.NaN, -11D));
    expectNaN($opt$noinline$Sub(Double.NaN, Double.NEGATIVE_INFINITY));
    expectNaN($opt$noinline$Sub(Double.NaN, Double.POSITIVE_INFINITY));

    expectEquals(Double.NEGATIVE_INFINITY, $opt$noinline$Sub(-Double.MAX_VALUE, Double.MAX_VALUE));
    expectEquals(Double.NEGATIVE_INFINITY, $opt$noinline$Sub(2D, Double.POSITIVE_INFINITY));
    expectEquals(Double.POSITIVE_INFINITY, $opt$noinline$Sub(Double.MAX_VALUE, -Double.MAX_VALUE));
    expectEquals(Double.POSITIVE_INFINITY, $opt$noinline$Sub(2D, Double.NEGATIVE_INFINITY));
    expectEquals(Double.POSITIVE_INFINITY, $opt$noinline$Sub(Double.POSITIVE_INFINITY, Double.NEGATIVE_INFINITY));
    expectEquals(Double.NEGATIVE_INFINITY, $opt$noinline$Sub(Double.NEGATIVE_INFINITY, Double.POSITIVE_INFINITY));
  }

  static boolean doThrow = false;

  static int $opt$noinline$Sub(int a, int b) {
    if (doThrow) {
      // Try defeating inlining.
      throw new Error();
    }
    return a - b;
  }

  static long $opt$noinline$Sub(long a, long b) {
    if (doThrow) {
      // Try defeating inlining.
      throw new Error();
    }
    return a - b;
  }

  static float $opt$noinline$Sub(float a, float b) {
    if (doThrow) {
      // Try defeating inlining.
      throw new Error();
    }
    return a - b;
  }

  static double $opt$noinline$Sub(double a, double b) {
    if (doThrow) {
      // Try defeating inlining.
      throw new Error();
    }
    return a - b;
  }

}
