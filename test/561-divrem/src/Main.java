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
  public static void assertEquals(int expected, int actual) {
    if (expected != actual) {
      throw new Error("Expected " + expected + ", got " + actual);
    }
  }

  public static void assertEquals(long expected, long actual) {
    if (expected != actual) {
      throw new Error("Expected " + expected + ", got " + actual);
    }
  }

  public static void main(String[] args) {
    assertEquals(0, $noinline$div(1));
    assertEquals(1, $noinline$rem(1));

    assertEquals(0, $noinline$div(-1));
    assertEquals(-1, $noinline$rem(-1));

    assertEquals(0, $noinline$div(0));
    assertEquals(0, $noinline$rem(0));

    assertEquals(1, $noinline$div(Integer.MIN_VALUE));
    assertEquals(0, $noinline$rem(Integer.MIN_VALUE));

    assertEquals(0, $noinline$div(Integer.MAX_VALUE));
    assertEquals(Integer.MAX_VALUE, $noinline$rem(Integer.MAX_VALUE));

    assertEquals(0, $noinline$div(Integer.MAX_VALUE - 1));
    assertEquals(Integer.MAX_VALUE - 1, $noinline$rem(Integer.MAX_VALUE - 1));

    assertEquals(0, $noinline$div(Integer.MIN_VALUE + 1));
    assertEquals(Integer.MIN_VALUE + 1, $noinline$rem(Integer.MIN_VALUE + 1));

    assertEquals(0L, $noinline$div(1));
    assertEquals(1L, $noinline$rem(1));

    assertEquals(0L, $noinline$div(-1));
    assertEquals(-1L, $noinline$rem(-1));

    assertEquals(0L, $noinline$div(0));
    assertEquals(0L, $noinline$rem(0));

    assertEquals(1L, $noinline$div(Long.MIN_VALUE));
    assertEquals(0L, $noinline$rem(Long.MIN_VALUE));

    assertEquals(0L, $noinline$div(Long.MAX_VALUE));
    assertEquals(Long.MAX_VALUE, $noinline$rem(Long.MAX_VALUE));

    assertEquals(0L, $noinline$div(Long.MAX_VALUE - 1));
    assertEquals(Long.MAX_VALUE - 1, $noinline$rem(Long.MAX_VALUE - 1));

    assertEquals(0L, $noinline$div(Integer.MIN_VALUE + 1));
    assertEquals(Long.MIN_VALUE + 1, $noinline$rem(Long.MIN_VALUE + 1));
  }

  public static int $noinline$div(int value) {
    if (doThrow) {
      throw new Error("");
    }
    return value / Integer.MIN_VALUE;
  }

  public static int $noinline$rem(int value) {
    if (doThrow) {
      throw new Error("");
    }
    return value % Integer.MIN_VALUE;
  }

  public static long $noinline$div(long value) {
    if (doThrow) {
      throw new Error("");
    }
    return value / Long.MIN_VALUE;
  }

  public static long $noinline$rem(long value) {
    if (doThrow) {
      throw new Error("");
    }
    return value % Long.MIN_VALUE;
  }

  static boolean doThrow = false;
}
