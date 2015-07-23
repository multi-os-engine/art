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

  public static void expectApproxEquals(float a, float b, float maxDelta) {
    boolean aproxEquals = (a > b)
      ? ((a - b) < maxDelta)
      : ((b - a) < maxDelta);
    if (!aproxEquals) {
      throw new Error("Expected: " + a + ", found: " + b + ", with delta: " + maxDelta);
    }
  }

  public static void expectApproxEquals(double a, double b, double maxDelta) {
    boolean aproxEquals = (a > b)
      ? ((a - b) < maxDelta)
      : ((b - a) < maxDelta);
    if (!aproxEquals) {
      throw new Error("Expected: " + a + ", found: " + b + ", with delta: " + maxDelta);
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
    mul();
  }

  public static void mul() {
    mulInt();
    mulLong();
    mulFloat();
    mulDouble();
  }

  private static void mulInt() {
    expectEquals(15, $opt$noinline$Mul(5, 3));
    expectEquals(0, $opt$noinline$Mul(0, 0));
    expectEquals(0, $opt$noinline$Mul(0, 3));
    expectEquals(0, $opt$noinline$Mul(3, 0));
    expectEquals(-3, $opt$noinline$Mul(1, -3));
    expectEquals(36, $opt$noinline$Mul(-12, -3));
    expectEquals(33, $opt$noinline$Mul(1, 3) * 11);
    expectEquals(671088645, $opt$noinline$Mul(134217729, 5)); // (2^27 + 1) * 5
  }

  private static void mulLong() {
    expectEquals(15L, $opt$noinline$Mul(5L, 3L));
    expectEquals(0L, $opt$noinline$Mul(0L, 0L));
    expectEquals(0L, $opt$noinline$Mul(0L, 3L));
    expectEquals(0L, $opt$noinline$Mul(3L, 0L));
    expectEquals(-3L, $opt$noinline$Mul(1L, -3L));
    expectEquals(36L, $opt$noinline$Mul(-12L, -3L));
    expectEquals(33L, $opt$noinline$Mul(1L, 3L) * 11L);
    expectEquals(240518168583L, $opt$noinline$Mul(34359738369L, 7L)); // (2^35 + 1) * 7
  }

  private static void mulFloat() {
    expectApproxEquals(15F, $opt$noinline$Mul(5F, 3F), 0.0001F);
    expectApproxEquals(0F, $opt$noinline$Mul(0F, 0F), 0.0001F);
    expectApproxEquals(0F, $opt$noinline$Mul(0F, 3F), 0.0001F);
    expectApproxEquals(0F, $opt$noinline$Mul(3F, 0F), 0.0001F);
    expectApproxEquals(-3F, $opt$noinline$Mul(1F, -3F), 0.0001F);
    expectApproxEquals(36F, $opt$noinline$Mul(-12F, -3F), 0.0001F);
    expectApproxEquals(33F, $opt$noinline$Mul(1F, 3F) * 11F, 0.0001F);
    expectApproxEquals(0.02F, 0.1F * 0.2F, 0.0001F);
    expectApproxEquals(-0.1F, -0.5F * 0.2F, 0.0001F);

    expectNaN($opt$noinline$Mul(0F, Float.POSITIVE_INFINITY));
    expectNaN($opt$noinline$Mul(0F, Float.NEGATIVE_INFINITY));
    expectNaN($opt$noinline$Mul(Float.NaN, 11F));
    expectNaN($opt$noinline$Mul(Float.NaN, -11F));
    expectNaN($opt$noinline$Mul(Float.NaN, Float.NEGATIVE_INFINITY));
    expectNaN($opt$noinline$Mul(Float.NaN, Float.POSITIVE_INFINITY));

    expectEquals(Float.POSITIVE_INFINITY, $opt$noinline$Mul(2F, 3.40282346638528860e+38F));
    expectEquals(Float.POSITIVE_INFINITY, $opt$noinline$Mul(2F, Float.POSITIVE_INFINITY));
    expectEquals(Float.NEGATIVE_INFINITY, $opt$noinline$Mul(-2F, Float.POSITIVE_INFINITY));
    expectEquals(Float.NEGATIVE_INFINITY, $opt$noinline$Mul(-2F, 3.40282346638528860e+38F));
    expectEquals(Float.NEGATIVE_INFINITY, $opt$noinline$Mul(2F, Float.NEGATIVE_INFINITY));
    expectEquals(Float.POSITIVE_INFINITY, $opt$noinline$Mul(-2F, Float.NEGATIVE_INFINITY));
    expectEquals(Float.NEGATIVE_INFINITY, $opt$noinline$Mul(Float.POSITIVE_INFINITY, Float.NEGATIVE_INFINITY));
    expectEquals(Float.POSITIVE_INFINITY, $opt$noinline$Mul(Float.POSITIVE_INFINITY, Float.POSITIVE_INFINITY));
    expectEquals(Float.POSITIVE_INFINITY, $opt$noinline$Mul(Float.NEGATIVE_INFINITY, Float.NEGATIVE_INFINITY));
  }

  private static void mulDouble() {
    expectApproxEquals(15D, $opt$noinline$Mul(5D, 3D), 0.0001D);
    expectApproxEquals(0D, $opt$noinline$Mul(0D, 0D), 0.0001D);
    expectApproxEquals(0D, $opt$noinline$Mul(0D, 3D), 0.0001D);
    expectApproxEquals(0D, $opt$noinline$Mul(3D, 0D), 0.0001D);
    expectApproxEquals(-3D, $opt$noinline$Mul(1D, -3D), 0.0001D);
    expectApproxEquals(36D, $opt$noinline$Mul(-12D, -3D), 0.0001D);
    expectApproxEquals(33D, $opt$noinline$Mul(1D, 3D) * 11D, 0.0001D);
    expectApproxEquals(0.02D, 0.1D * 0.2D, 0.0001D);
    expectApproxEquals(-0.1D, -0.5D * 0.2D, 0.0001D);

    expectNaN($opt$noinline$Mul(0D, Double.POSITIVE_INFINITY));
    expectNaN($opt$noinline$Mul(0D, Double.NEGATIVE_INFINITY));
    expectNaN($opt$noinline$Mul(Double.NaN, 11D));
    expectNaN($opt$noinline$Mul(Double.NaN, -11D));
    expectNaN($opt$noinline$Mul(Double.NaN, Double.NEGATIVE_INFINITY));
    expectNaN($opt$noinline$Mul(Double.NaN, Double.POSITIVE_INFINITY));

    expectEquals(Double.POSITIVE_INFINITY, $opt$noinline$Mul(2D, 1.79769313486231570e+308));
    expectEquals(Double.POSITIVE_INFINITY, $opt$noinline$Mul(2D, Double.POSITIVE_INFINITY));
    expectEquals(Double.NEGATIVE_INFINITY, $opt$noinline$Mul(-2D, Double.POSITIVE_INFINITY));
    expectEquals(Double.NEGATIVE_INFINITY, $opt$noinline$Mul(-2D, 1.79769313486231570e+308));
    expectEquals(Double.NEGATIVE_INFINITY, $opt$noinline$Mul(2D, Double.NEGATIVE_INFINITY));
    expectEquals(Double.POSITIVE_INFINITY, $opt$noinline$Mul(-2D, Double.NEGATIVE_INFINITY));
    expectEquals(Double.NEGATIVE_INFINITY, $opt$noinline$Mul(Double.POSITIVE_INFINITY, Double.NEGATIVE_INFINITY));
    expectEquals(Double.POSITIVE_INFINITY, $opt$noinline$Mul(Double.POSITIVE_INFINITY, Double.POSITIVE_INFINITY));
    expectEquals(Double.POSITIVE_INFINITY, $opt$noinline$Mul(Double.NEGATIVE_INFINITY, Double.NEGATIVE_INFINITY));
  }

  static boolean doThrow = false;

  static int $opt$noinline$Mul(int a, int b) {
    if (doThrow) { throw new Error(); }  // Try defeating inlining.
    return a * b;
  }

  static long $opt$noinline$Mul(long a, long b) {
    if (doThrow) { throw new Error(); }  // Try defeating inlining.
    return a * b;
  }

  static float $opt$noinline$Mul(float a, float b) {
    if (doThrow) { throw new Error(); }  // Try defeating inlining.
    return a * b;
  }

  static double $opt$noinline$Mul(double a, double b) {
    if (doThrow) { throw new Error(); }  // Try defeating inlining.
    return a * b;
  }
}
