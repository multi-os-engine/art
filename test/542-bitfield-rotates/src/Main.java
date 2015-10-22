/*
 * Copyright (C) 2007 The Android Open Source Project
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

  public static void assertIntEquals(int expected, int actual) {
    if (expected != actual) {
      throw new Error("Expected: " + expected + ", found: " + actual);
    }
  }

  public static void assertLongEquals(long expected, long actual) {
    if (expected != actual) {
      throw new Error("Expected: " + expected + ", found: " + actual);
    }
  }

  public static void main(String args[]) throws Exception {
    test_Integer_right_v_csubv();
    test_Long_right_v_csubv();

    test_Integer_left_csubv_v();
    test_Long_left_csubv_v();

    test_Integer_right_v_negv();
    test_Long_right_v_negv();

    test_Integer_left_negv_v();
    test_Long_left_negv_v();
  }

  public static int rotate_int_right_reg_v_csubv(int value, int distance) {
    return (value >>> distance) | (value << (32 - distance));
  }

  public static void test_Integer_right_v_csubv() throws Exception {
    assertIntEquals(rotate_int_right_reg_v_csubv(0x11, 0), 0x11);

    assertIntEquals(rotate_int_right_reg_v_csubv(0x11, 1), 0x80000008);
    assertIntEquals(rotate_int_right_reg_v_csubv(0x11, Integer.SIZE - 1), 0x22);
    assertIntEquals(rotate_int_right_reg_v_csubv(0x11, Integer.SIZE), 0x11);
    assertIntEquals(rotate_int_right_reg_v_csubv(0x11, Integer.SIZE + 1), 0x80000008);

    assertIntEquals(rotate_int_right_reg_v_csubv(0x11, -1), 0x22);
    assertIntEquals(rotate_int_right_reg_v_csubv(0x11, -(Integer.SIZE - 1)), 0x80000008);
    assertIntEquals(rotate_int_right_reg_v_csubv(0x11, -Integer.SIZE), 0x11);
    assertIntEquals(rotate_int_right_reg_v_csubv(0x11, -(Integer.SIZE + 1)), 0x22);

    assertIntEquals(rotate_int_right_reg_v_csubv(0x80000000, 1), 0x40000000);
  }

  public static long rotate_long_right_reg_v_csubv(long value, int distance) {
    return (value >>> distance) | (value << (64 - distance));
  }

  public static void test_Long_right_v_csubv() throws Exception {
    assertLongEquals(rotate_long_right_reg_v_csubv(0x11, 0), 0x11);

    assertLongEquals(rotate_long_right_reg_v_csubv(0x11, 1), 0x8000000000000008L);
    assertLongEquals(rotate_long_right_reg_v_csubv(0x11, Long.SIZE - 1), 0x22);
    assertLongEquals(rotate_long_right_reg_v_csubv(0x11, Long.SIZE), 0x11);
    assertLongEquals(rotate_long_right_reg_v_csubv(0x11, Long.SIZE + 1), 0x8000000000000008L);

    assertLongEquals(rotate_long_right_reg_v_csubv(0x11, -1), 0x22);
    assertLongEquals(rotate_long_right_reg_v_csubv(0x11, -(Long.SIZE - 1)), 0x8000000000000008L);
    assertLongEquals(rotate_long_right_reg_v_csubv(0x11, -Long.SIZE), 0x11);
    assertLongEquals(rotate_long_right_reg_v_csubv(0x11, -(Long.SIZE + 1)), 0x22);

    assertLongEquals(rotate_long_right_reg_v_csubv(0x8000000000000000L, 1), 0x4000000000000000L);
  }

  public static int rotate_int_left_reg_csubv_v(int value, int distance) {
    return (value >>> (32 - distance)) | (value << distance);
  }

  public static void test_Integer_left_csubv_v() throws Exception {
    assertIntEquals(rotate_int_left_reg_csubv_v(0x11, 0), 0x11);

    assertIntEquals(rotate_int_left_reg_csubv_v(0x11, 1), 0x22);
    assertIntEquals(rotate_int_left_reg_csubv_v(0x11, Integer.SIZE - 1), 0x80000008);
    assertIntEquals(rotate_int_left_reg_csubv_v(0x11, Integer.SIZE), 0x11);
    assertIntEquals(rotate_int_left_reg_csubv_v(0x11, Integer.SIZE + 1), 0x22);

    assertIntEquals(rotate_int_left_reg_csubv_v(0x11, -1), 0x80000008);
    assertIntEquals(rotate_int_left_reg_csubv_v(0x11, -(Integer.SIZE - 1)), 0x22);
    assertIntEquals(rotate_int_left_reg_csubv_v(0x11, -Integer.SIZE), 0x11);
    assertIntEquals(rotate_int_left_reg_csubv_v(0x11, -(Integer.SIZE + 1)), 0x80000008);

    assertIntEquals(rotate_int_left_reg_csubv_v(0xC0000000, 1), 0x80000001);
  }

  public static long rotate_long_left_reg_csubv_v(long value, int distance) {
    return (value >>> (64 - distance)) | (value << distance);
  }

  public static void test_Long_left_csubv_v() throws Exception {
    assertLongEquals(rotate_long_left_reg_csubv_v(0x11, 0), 0x11);

    assertLongEquals(rotate_long_left_reg_csubv_v(0x11, 1), 0x22);
    assertLongEquals(rotate_long_left_reg_csubv_v(0x11, Long.SIZE - 1), 0x8000000000000008L);
    assertLongEquals(rotate_long_left_reg_csubv_v(0x11, Long.SIZE), 0x11);
    assertLongEquals(rotate_long_left_reg_csubv_v(0x11, Long.SIZE + 1), 0x22);

    assertLongEquals(rotate_long_left_reg_csubv_v(0x11, -1), 0x8000000000000008L);
    assertLongEquals(rotate_long_left_reg_csubv_v(0x11, -(Long.SIZE - 1)), 0x22);
    assertLongEquals(rotate_long_left_reg_csubv_v(0x11, -Long.SIZE), 0x11);
    assertLongEquals(rotate_long_left_reg_csubv_v(0x11, -(Long.SIZE + 1)), 0x8000000000000008L);

    assertLongEquals(rotate_long_left_reg_csubv_v(0xC000000000000000L, 1), 0x8000000000000001L);
  }

  public static int rotate_int_right_reg_v_negv(int value, int distance) {
    return (value >>> distance) | (value << -distance);
  }

  public static void test_Integer_right_v_negv() throws Exception {
    assertIntEquals(rotate_int_right_reg_v_negv(0x11, 0), 0x11);

    assertIntEquals(rotate_int_right_reg_v_negv(0x11, 1), 0x80000008);
    assertIntEquals(rotate_int_right_reg_v_negv(0x11, Integer.SIZE - 1), 0x22);
    assertIntEquals(rotate_int_right_reg_v_negv(0x11, Integer.SIZE), 0x11);
    assertIntEquals(rotate_int_right_reg_v_negv(0x11, Integer.SIZE + 1), 0x80000008);

    assertIntEquals(rotate_int_right_reg_v_negv(0x11, -1), 0x22);
    assertIntEquals(rotate_int_right_reg_v_negv(0x11, -(Integer.SIZE - 1)), 0x80000008);
    assertIntEquals(rotate_int_right_reg_v_negv(0x11, -Integer.SIZE), 0x11);
    assertIntEquals(rotate_int_right_reg_v_negv(0x11, -(Integer.SIZE + 1)), 0x22);

    assertIntEquals(rotate_int_right_reg_v_negv(0x80000000, 1), 0x40000000);
  }

  public static long rotate_long_right_reg_v_negv(long value, int distance) {
    return (value >>> distance) | (value << -distance);
  }

  public static void test_Long_right_v_negv() throws Exception {
    assertLongEquals(rotate_long_right_reg_v_negv(0x11, 0), 0x11);

    assertLongEquals(rotate_long_right_reg_v_negv(0x11, 1), 0x8000000000000008L);
    assertLongEquals(rotate_long_right_reg_v_negv(0x11, Long.SIZE - 1), 0x22);
    assertLongEquals(rotate_long_right_reg_v_negv(0x11, Long.SIZE), 0x11);
    assertLongEquals(rotate_long_right_reg_v_negv(0x11, Long.SIZE + 1), 0x8000000000000008L);

    assertLongEquals(rotate_long_right_reg_v_negv(0x11, -1), 0x22);
    assertLongEquals(rotate_long_right_reg_v_negv(0x11, -(Long.SIZE - 1)), 0x8000000000000008L);
    assertLongEquals(rotate_long_right_reg_v_negv(0x11, -Long.SIZE), 0x11);
    assertLongEquals(rotate_long_right_reg_v_negv(0x11, -(Long.SIZE + 1)), 0x22);

    assertLongEquals(rotate_long_right_reg_v_negv(0x8000000000000000L, 1), 0x4000000000000000L);
  }

  public static int rotate_int_left_reg_negv_v(int value, int distance) {
    return (value >>> -distance) | (value << distance);
  }

  public static void test_Integer_left_negv_v() throws Exception {
    assertIntEquals(rotate_int_left_reg_negv_v(0x11, 0), 0x11);

    assertIntEquals(rotate_int_left_reg_negv_v(0x11, 1), 0x22);
    assertIntEquals(rotate_int_left_reg_negv_v(0x11, Integer.SIZE - 1), 0x80000008);
    assertIntEquals(rotate_int_left_reg_negv_v(0x11, Integer.SIZE), 0x11);
    assertIntEquals(rotate_int_left_reg_negv_v(0x11, Integer.SIZE + 1), 0x22);

    assertIntEquals(rotate_int_left_reg_negv_v(0x11, -1), 0x80000008);
    assertIntEquals(rotate_int_left_reg_negv_v(0x11, -(Integer.SIZE - 1)), 0x22);
    assertIntEquals(rotate_int_left_reg_negv_v(0x11, -Integer.SIZE), 0x11);
    assertIntEquals(rotate_int_left_reg_negv_v(0x11, -(Integer.SIZE + 1)), 0x80000008);

    assertIntEquals(rotate_int_left_reg_negv_v(0xC0000000, 1), 0x80000001);
  }

  public static long rotate_long_left_reg_negv_v(long value, int distance) {
    return (value >>> -distance) | (value << distance);
  }

  public static void test_Long_left_negv_v() throws Exception {
    assertLongEquals(rotate_long_left_reg_negv_v(0x11, 0), 0x11);

    assertLongEquals(rotate_long_left_reg_negv_v(0x11, 1), 0x22);
    assertLongEquals(rotate_long_left_reg_negv_v(0x11, Long.SIZE - 1), 0x8000000000000008L);
    assertLongEquals(rotate_long_left_reg_negv_v(0x11, Long.SIZE), 0x11);
    assertLongEquals(rotate_long_left_reg_negv_v(0x11, Long.SIZE + 1), 0x22);

    assertLongEquals(rotate_long_left_reg_negv_v(0x11, -1), 0x8000000000000008L);
    assertLongEquals(rotate_long_left_reg_negv_v(0x11, -(Long.SIZE - 1)), 0x22);
    assertLongEquals(rotate_long_left_reg_negv_v(0x11, -Long.SIZE), 0x11);
    assertLongEquals(rotate_long_left_reg_negv_v(0x11, -(Long.SIZE + 1)), 0x8000000000000008L);

    assertLongEquals(rotate_long_left_reg_negv_v(0xC000000000000000L, 1), 0x8000000000000001L);
  }

}
