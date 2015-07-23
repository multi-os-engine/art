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

  public static void main(String[] args) {
    remInt();
    remLong();
  }

  private static void remInt() {
    expectEquals(2, $opt$noinline$RemConst(6));
    expectEquals(2, $opt$noinline$Rem(6, 4));
    expectEquals(2, $opt$noinline$Rem(6, -4));
    expectEquals(0, $opt$noinline$Rem(6, 3));
    expectEquals(0, $opt$noinline$Rem(6, -3));
    expectEquals(0, $opt$noinline$Rem(6, 1));
    expectEquals(0, $opt$noinline$Rem(6, -1));
    expectEquals(-1, $opt$noinline$Rem(-7, 3));
    expectEquals(-1, $opt$noinline$Rem(-7, -3));
    expectEquals(0, $opt$noinline$Rem(6, 6));
    expectEquals(0, $opt$noinline$Rem(-6, -6));
    expectEquals(7, $opt$noinline$Rem(7, 9));
    expectEquals(7, $opt$noinline$Rem(7, -9));
    expectEquals(-7, $opt$noinline$Rem(-7, 9));
    expectEquals(-7, $opt$noinline$Rem(-7, -9));

    expectEquals(0, $opt$noinline$Rem(Integer.MAX_VALUE, 1));
    expectEquals(0, $opt$noinline$Rem(Integer.MAX_VALUE, -1));
    expectEquals(0, $opt$noinline$Rem(Integer.MIN_VALUE, 1));
    expectEquals(0, $opt$noinline$Rem(Integer.MIN_VALUE, -1)); // no overflow
    expectEquals(-1, $opt$noinline$Rem(Integer.MIN_VALUE, Integer.MAX_VALUE));
    expectEquals(Integer.MAX_VALUE, $opt$noinline$Rem(Integer.MAX_VALUE, Integer.MIN_VALUE));

    expectEquals(0, $opt$noinline$Rem(0, 7));
    expectEquals(0, $opt$noinline$Rem(0, Integer.MAX_VALUE));
    expectEquals(0, $opt$noinline$Rem(0, Integer.MIN_VALUE));

    expectDivisionByZero(0);
    expectDivisionByZero(1);
    expectDivisionByZero(5);
    expectDivisionByZero(Integer.MAX_VALUE);
    expectDivisionByZero(Integer.MIN_VALUE);
  }

  private static void remLong() {
    expectEquals(2L, $opt$noinline$RemConst(6L));
    expectEquals(2L, $opt$noinline$Rem(6L, 4L));
    expectEquals(2L, $opt$noinline$Rem(6L, -4L));
    expectEquals(0L, $opt$noinline$Rem(6L, 3L));
    expectEquals(0L, $opt$noinline$Rem(6L, -3L));
    expectEquals(0L, $opt$noinline$Rem(6L, 1L));
    expectEquals(0L, $opt$noinline$Rem(6L, -1L));
    expectEquals(-1L, $opt$noinline$Rem(-7L, 3L));
    expectEquals(-1L, $opt$noinline$Rem(-7L, -3L));
    expectEquals(0L, $opt$noinline$Rem(6L, 6L));
    expectEquals(0L, $opt$noinline$Rem(-6L, -6L));
    expectEquals(7L, $opt$noinline$Rem(7L, 9L));
    expectEquals(7L, $opt$noinline$Rem(7L, -9L));
    expectEquals(-7L, $opt$noinline$Rem(-7L, 9L));
    expectEquals(-7L, $opt$noinline$Rem(-7L, -9L));

    expectEquals(0L, $opt$noinline$Rem(Long.MAX_VALUE, 1L));
    expectEquals(0L, $opt$noinline$Rem(Long.MAX_VALUE, -1L));
    expectEquals(0L, $opt$noinline$Rem(Long.MIN_VALUE, 1L));
    expectEquals(0L, $opt$noinline$Rem(Long.MIN_VALUE, -1L)); // no overflow
    expectEquals(-1L, $opt$noinline$Rem(Long.MIN_VALUE, Long.MAX_VALUE));
    expectEquals(Long.MAX_VALUE, $opt$noinline$Rem(Long.MAX_VALUE, Long.MIN_VALUE));

    expectEquals(0L, $opt$noinline$Rem(0L, 7L));
    expectEquals(0L, $opt$noinline$Rem(0L, Long.MAX_VALUE));
    expectEquals(0L, $opt$noinline$Rem(0L, Long.MIN_VALUE));

    expectDivisionByZero(0L);
    expectDivisionByZero(1L);
    expectDivisionByZero(5L);
    expectDivisionByZero(Long.MAX_VALUE);
    expectDivisionByZero(Long.MIN_VALUE);
  }

  static boolean doThrow = false;

  static int $opt$noinline$Rem(int a, int b) {
    if (doThrow) { throw new Error(); }  // Try defeating inlining.
    return a % b;
  }

  static int $opt$noinline$RemZero(int a) {
    if (doThrow) { throw new Error(); }  // Try defeating inlining.
    return a % 0;
  }

  // Modulo by literals != 0 should not generate checks.
  static int $opt$noinline$RemConst(int a) {
    if (doThrow) { throw new Error(); }  // Try defeating inlining.
    return a % 4;
  }

  static long $opt$noinline$RemConst(long a) {
    if (doThrow) { throw new Error(); }  // Try defeating inlining.
    return a % 4L;
  }

  static long $opt$noinline$Rem(long a, long b) {
    if (doThrow) { throw new Error(); }  // Try defeating inlining.
    return a % b;
  }

  static long $opt$noinline$RemZero(long a) {
    if (doThrow) { throw new Error(); }  // Try defeating inlining.
    return a % 0L;
  }

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

  public static void expectDivisionByZero(int value) {
    try {
      $opt$noinline$Rem(value, 0);
      throw new Error("Expected RuntimeException when modulo by 0");
    } catch (java.lang.RuntimeException e) {
    }
    try {
      $opt$noinline$RemZero(value);
      throw new Error("Expected RuntimeException when modulo by 0");
    } catch (java.lang.RuntimeException e) {
    }
  }

  public static void expectDivisionByZero(long value) {
    try {
      $opt$noinline$Rem(value, 0L);
      throw new Error("Expected RuntimeException when modulo by 0");
    } catch (java.lang.RuntimeException e) {
    }
    try {
      $opt$noinline$RemZero(value);
      throw new Error("Expected RuntimeException when modulo by 0");
    } catch (java.lang.RuntimeException e) {
    }
  }

}
