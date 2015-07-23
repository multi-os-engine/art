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

  public static void main(String[] args) {
    andInt();
    andLong();

    orInt();
    orLong();

    xorInt();
    xorLong();
  }

  private static void andInt() {
    expectEquals(1, $opt$noinline$And(5, 3));
    expectEquals(0, $opt$noinline$And(0, 0));
    expectEquals(0, $opt$noinline$And(0, 3));
    expectEquals(0, $opt$noinline$And(3, 0));
    expectEquals(1, $opt$noinline$And(1, -3));
    expectEquals(-12, $opt$noinline$And(-12, -3));

    expectEquals(1, $opt$noinline$AndLit8(1));
    expectEquals(0, $opt$noinline$AndLit8(0));
    expectEquals(0, $opt$noinline$AndLit8(0));
    expectEquals(3, $opt$noinline$AndLit8(3));
    expectEquals(4, $opt$noinline$AndLit8(-12));

    expectEquals(0, $opt$noinline$AndLit16(1));
    expectEquals(0, $opt$noinline$AndLit16(0));
    expectEquals(0, $opt$noinline$AndLit16(0));
    expectEquals(0, $opt$noinline$AndLit16(3));
    expectEquals(65280, $opt$noinline$AndLit16(-12));
  }

  private static void andLong() {
    expectEquals(1L, $opt$noinline$And(5L, 3L));
    expectEquals(0L, $opt$noinline$And(0L, 0L));
    expectEquals(0L, $opt$noinline$And(0L, 3L));
    expectEquals(0L, $opt$noinline$And(3L, 0L));
    expectEquals(1L, $opt$noinline$And(1L, -3L));
    expectEquals(-12L, $opt$noinline$And(-12L, -3L));

    expectEquals(1L, $opt$noinline$AndLit8(1L));
    expectEquals(0L, $opt$noinline$AndLit8(0L));
    expectEquals(0L, $opt$noinline$AndLit8(0L));
    expectEquals(3L, $opt$noinline$AndLit8(3L));
    expectEquals(4L, $opt$noinline$AndLit8(-12L));

    expectEquals(0L, $opt$noinline$AndLit16(1L));
    expectEquals(0L, $opt$noinline$AndLit16(0L));
    expectEquals(0L, $opt$noinline$AndLit16(0L));
    expectEquals(0L, $opt$noinline$AndLit16(3L));
    expectEquals(65280L, $opt$noinline$AndLit16(-12L));
  }

  static boolean doThrow = false;

  static int $opt$noinline$And(int a, int b) {
    if (doThrow) { throw new Error(); }  // Try defeating inlining.
    return a & b;
  }

  static int $opt$noinline$AndLit8(int a) {
    if (doThrow) { throw new Error(); }  // Try defeating inlining.
    return a & 0xF;
  }

  static int $opt$noinline$AndLit16(int a) {
    if (doThrow) { throw new Error(); }  // Try defeating inlining.
    return a & 0xFF00;
  }

  static long $opt$noinline$And(long a, long b) {
    if (doThrow) { throw new Error(); }  // Try defeating inlining.
    return a & b;
  }

  static long $opt$noinline$AndLit8(long a) {
    if (doThrow) { throw new Error(); }  // Try defeating inlining.
    return a & 0xF;
  }

  static long $opt$noinline$AndLit16(long a) {
    if (doThrow) { throw new Error(); }  // Try defeating inlining.
    return a & 0xFF00;
  }

  private static void orInt() {
    expectEquals(7, $opt$noinline$Or(5, 3));
    expectEquals(0, $opt$noinline$Or(0, 0));
    expectEquals(3, $opt$noinline$Or(0, 3));
    expectEquals(3, $opt$noinline$Or(3, 0));
    expectEquals(-3, $opt$noinline$Or(1, -3));
    expectEquals(-3, $opt$noinline$Or(-12, -3));

    expectEquals(15, $opt$noinline$OrLit8(1));
    expectEquals(15, $opt$noinline$OrLit8(0));
    expectEquals(15, $opt$noinline$OrLit8(3));
    expectEquals(-1, $opt$noinline$OrLit8(-12));

    expectEquals(0xFF01, $opt$noinline$OrLit16(1));
    expectEquals(0xFF00, $opt$noinline$OrLit16(0));
    expectEquals(0xFF03, $opt$noinline$OrLit16(3));
    expectEquals(-12, $opt$noinline$OrLit16(-12));
  }

  private static void orLong() {
    expectEquals(7L, $opt$noinline$Or(5L, 3L));
    expectEquals(0L, $opt$noinline$Or(0L, 0L));
    expectEquals(3L, $opt$noinline$Or(0L, 3L));
    expectEquals(3L, $opt$noinline$Or(3L, 0L));
    expectEquals(-3L, $opt$noinline$Or(1L, -3L));
    expectEquals(-3L, $opt$noinline$Or(-12L, -3L));

    expectEquals(15L, $opt$noinline$OrLit8(1L));
    expectEquals(15L, $opt$noinline$OrLit8(0L));
    expectEquals(15L, $opt$noinline$OrLit8(3L));
    expectEquals(-1L, $opt$noinline$OrLit8(-12L));

    expectEquals(0xFF01L, $opt$noinline$OrLit16(1L));
    expectEquals(0xFF00L, $opt$noinline$OrLit16(0L));
    expectEquals(0xFF03L, $opt$noinline$OrLit16(3L));
    expectEquals(-12L, $opt$noinline$OrLit16(-12L));
  }

  static int $opt$noinline$Or(int a, int b) {
    if (doThrow) { throw new Error(); }  // Try defeating inlining.
    return a | b;
  }

  static int $opt$noinline$OrLit8(int a) {
    if (doThrow) { throw new Error(); }  // Try defeating inlining.
    return a | 0xF;
  }

  static int $opt$noinline$OrLit16(int a) {
    if (doThrow) { throw new Error(); }  // Try defeating inlining.
    return a | 0xFF00;
  }

  static long $opt$noinline$Or(long a, long b) {
    if (doThrow) { throw new Error(); }  // Try defeating inlining.
    return a | b;
  }

  static long $opt$noinline$OrLit8(long a) {
    if (doThrow) { throw new Error(); }  // Try defeating inlining.
    return a | 0xF;
  }

  static long $opt$noinline$OrLit16(long a) {
    if (doThrow) { throw new Error(); }  // Try defeating inlining.
    return a | 0xFF00;
  }

  private static void xorInt() {
    expectEquals(6, $opt$noinline$Xor(5, 3));
    expectEquals(0, $opt$noinline$Xor(0, 0));
    expectEquals(3, $opt$noinline$Xor(0, 3));
    expectEquals(3, $opt$noinline$Xor(3, 0));
    expectEquals(-4, $opt$noinline$Xor(1, -3));
    expectEquals(9, $opt$noinline$Xor(-12, -3));

    expectEquals(14, $opt$noinline$XorLit8(1));
    expectEquals(15, $opt$noinline$XorLit8(0));
    expectEquals(12, $opt$noinline$XorLit8(3));
    expectEquals(-5, $opt$noinline$XorLit8(-12));

    expectEquals(0xFF01, $opt$noinline$XorLit16(1));
    expectEquals(0xFF00, $opt$noinline$XorLit16(0));
    expectEquals(0xFF03, $opt$noinline$XorLit16(3));
    expectEquals(-0xFF0c, $opt$noinline$XorLit16(-12));
  }

  private static void xorLong() {
    expectEquals(6L, $opt$noinline$Xor(5L, 3L));
    expectEquals(0L, $opt$noinline$Xor(0L, 0L));
    expectEquals(3L, $opt$noinline$Xor(0L, 3L));
    expectEquals(3L, $opt$noinline$Xor(3L, 0L));
    expectEquals(-4L, $opt$noinline$Xor(1L, -3L));
    expectEquals(9L, $opt$noinline$Xor(-12L, -3L));

    expectEquals(14L, $opt$noinline$XorLit8(1L));
    expectEquals(15L, $opt$noinline$XorLit8(0L));
    expectEquals(12L, $opt$noinline$XorLit8(3L));
    expectEquals(-5L, $opt$noinline$XorLit8(-12L));

    expectEquals(0xFF01L, $opt$noinline$XorLit16(1L));
    expectEquals(0xFF00L, $opt$noinline$XorLit16(0L));
    expectEquals(0xFF03L, $opt$noinline$XorLit16(3L));
    expectEquals(-0xFF0cL, $opt$noinline$XorLit16(-12L));
  }

  static int $opt$noinline$Xor(int a, int b) {
    if (doThrow) { throw new Error(); }  // Try defeating inlining.
    return a ^ b;
  }

  static int $opt$noinline$XorLit8(int a) {
    if (doThrow) { throw new Error(); }  // Try defeating inlining.
    return a ^ 0xF;
  }

  static int $opt$noinline$XorLit16(int a) {
    if (doThrow) { throw new Error(); }  // Try defeating inlining.
    return a ^ 0xFF00;
  }

  static long $opt$noinline$Xor(long a, long b) {
    if (doThrow) { throw new Error(); }  // Try defeating inlining.
    return a ^ b;
  }

  static long $opt$noinline$XorLit8(long a) {
    if (doThrow) { throw new Error(); }  // Try defeating inlining.
    return a ^ 0xF;
  }

  static long $opt$noinline$XorLit16(long a) {
    if (doThrow) { throw new Error(); }  // Try defeating inlining.
    return a ^ 0xFF00;
  }
}
