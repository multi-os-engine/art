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

import java.lang.reflect.Method;

public class Main {

  public static void main(String[] args) throws Exception {
    cmpLong();
    cmpFloat();
    cmpDouble();
  }

  private static void cmpLong() throws Exception {
    $stopinliner$expectLt(3L, 5L);
    $stopinliner$expectGt(5L, 3L);
    $stopinliner$expectLt(Long.MIN_VALUE, Long.MAX_VALUE);
    $stopinliner$expectGt(Long.MAX_VALUE, Long.MIN_VALUE);

    expectEquals(0, smaliCmpLong(0L, 0L));
    expectEquals(0, smaliCmpLong(1L, 1L));
    expectEquals(-1, smaliCmpLong(1L, 2L));
    expectEquals(1, smaliCmpLong(2L, 1L));
    expectEquals(-1, smaliCmpLong(Long.MIN_VALUE, Long.MAX_VALUE));
    expectEquals(1, smaliCmpLong(Long.MAX_VALUE, Long.MIN_VALUE));
    expectEquals(0, smaliCmpLong(Long.MIN_VALUE, Long.MIN_VALUE));
    expectEquals(0, smaliCmpLong(Long.MAX_VALUE, Long.MAX_VALUE));
  }

  private static void cmpFloat() throws Exception {
    $stopinliner$expectLt(3.1F, 5.1F);
    $stopinliner$expectGt(5.1F, 3.1F);
    $stopinliner$expectLt(Float.MIN_VALUE, Float.MAX_VALUE);
    $stopinliner$expectGt(Float.MAX_VALUE, Float.MIN_VALUE);
    $stopinliner$expectFalse(3.1F, Float.NaN);
    $stopinliner$expectFalse(Float.NaN, 3.1F);

    expectEquals(0, smaliCmpGtFloat(0F, 0F));
    expectEquals(0, smaliCmpGtFloat(1F, 1F));
    expectEquals(-1, smaliCmpGtFloat(1.1F, 2.1F));
    expectEquals(1, smaliCmpGtFloat(2.1F, 1.1F));
    expectEquals(-1, smaliCmpGtFloat(Float.MIN_VALUE, Float.MAX_VALUE));
    expectEquals(1, smaliCmpGtFloat(Float.MAX_VALUE, Float.MIN_VALUE));
    expectEquals(0, smaliCmpGtFloat(Float.MIN_VALUE, Float.MIN_VALUE));
    expectEquals(0, smaliCmpGtFloat(Float.MAX_VALUE, Float.MAX_VALUE));
    expectEquals(1, smaliCmpGtFloat(5F, Float.NaN));
    expectEquals(1, smaliCmpGtFloat(Float.NaN, 5F));

    expectEquals(0, smaliCmpLtFloat(0F, 0F));
    expectEquals(0, smaliCmpLtFloat(1F, 1F));
    expectEquals(-1, smaliCmpLtFloat(1.1F, 2.1F));
    expectEquals(1, smaliCmpLtFloat(2.1F, 1.1F));
    expectEquals(-1, smaliCmpLtFloat(Float.MIN_VALUE, Float.MAX_VALUE));
    expectEquals(1, smaliCmpLtFloat(Float.MAX_VALUE, Float.MIN_VALUE));
    expectEquals(0, smaliCmpLtFloat(Float.MIN_VALUE, Float.MIN_VALUE));
    expectEquals(0, smaliCmpLtFloat(Float.MAX_VALUE, Float.MAX_VALUE));
    expectEquals(-1, smaliCmpLtFloat(5F, Float.NaN));
    expectEquals(-1, smaliCmpLtFloat(Float.NaN, 5F));
  }

  private static void cmpDouble() throws Exception {
    $stopinliner$expectLt(3.1D, 5.1D);
    $stopinliner$expectGt(5.1D, 3.1D);
    $stopinliner$expectLt(Double.MIN_VALUE, Double.MAX_VALUE);
    $stopinliner$expectGt(Double.MAX_VALUE, Double.MIN_VALUE);
    $stopinliner$expectFalse(3.1D, Double.NaN);
    $stopinliner$expectFalse(Double.NaN, 3.1D);

    expectEquals(0, smaliCmpGtDouble(0D, 0D));
    expectEquals(0, smaliCmpGtDouble(1D, 1D));
    expectEquals(-1, smaliCmpGtDouble(1.1D, 2.1D));
    expectEquals(1, smaliCmpGtDouble(2.1D, 1.1D));
    expectEquals(-1, smaliCmpGtDouble(Double.MIN_VALUE, Double.MAX_VALUE));
    expectEquals(1, smaliCmpGtDouble(Double.MAX_VALUE, Double.MIN_VALUE));
    expectEquals(0, smaliCmpGtDouble(Double.MIN_VALUE, Double.MIN_VALUE));
    expectEquals(0, smaliCmpGtDouble(Double.MAX_VALUE, Double.MAX_VALUE));
    expectEquals(1, smaliCmpGtDouble(5D, Double.NaN));
    expectEquals(1, smaliCmpGtDouble(Double.NaN, 5D));

    expectEquals(0, smaliCmpLtDouble(0D, 0D));
    expectEquals(0, smaliCmpLtDouble(1D, 1D));
    expectEquals(-1, smaliCmpLtDouble(1.1D, 2.1D));
    expectEquals(1, smaliCmpLtDouble(2.1D, 1.1D));
    expectEquals(-1, smaliCmpLtDouble(Double.MIN_VALUE, Double.MAX_VALUE));
    expectEquals(1, smaliCmpLtDouble(Double.MAX_VALUE, Double.MIN_VALUE));
    expectEquals(0, smaliCmpLtDouble(Double.MIN_VALUE, Double.MIN_VALUE));
    expectEquals(0, smaliCmpLtDouble(Double.MAX_VALUE, Double.MAX_VALUE));
    expectEquals(-1, smaliCmpLtDouble(5D, Double.NaN));
    expectEquals(-1, smaliCmpLtDouble(Float.NaN, 5D));
  }

  static boolean $opt$noinline$lt(long a, long b) {
    return a < b;
  }

  static boolean $opt$noinline$lt(float a, float b) {
    return a < b;
  }

  static boolean $opt$noinline$lt(double a, double b) {
    return a < b;
  }

  static boolean $opt$noinline$gt(long a, long b) {
    return a > b;
  }

  static boolean $opt$noinline$gt(float a, float b) {
    return a > b;
  }

  static boolean $opt$noinline$gt(double a, double b) {
    return a > b;
  }

  // Wrappers around methods located in file cmp.smali.

  private static int smaliCmpLong(long a, long b) throws Exception {
    Class<?> c = Class.forName("TestCmp");
    Method m = c.getMethod("$opt$CmpLong", long.class, long.class);
    int result = (Integer)m.invoke(null, a, b);
    return result;
  }

  private static int smaliCmpGtFloat(float a, float b) throws Exception {
    Class<?> c = Class.forName("TestCmp");
    Method m = c.getMethod("$opt$CmpGtFloat", float.class, float.class);
    int result = (Integer)m.invoke(null, a, b);
    return result;
  }

  private static int smaliCmpLtFloat(float a, float b) throws Exception {
    Class<?> c = Class.forName("TestCmp");
    Method m = c.getMethod("$opt$CmpLtFloat", float.class, float.class);
    int result = (Integer)m.invoke(null, a, b);
    return result;
  }

  private static int smaliCmpGtDouble(double a, double b) throws Exception {
    Class<?> c = Class.forName("TestCmp");
    Method m = c.getMethod("$opt$CmpGtDouble", double.class, double.class);
    int result = (Integer)m.invoke(null, a, b);
    return result;
  }

  private static int smaliCmpLtDouble(double a, double b) throws Exception {
    Class<?> c = Class.forName("TestCmp");
    Method m = c.getMethod("$opt$CmpLtDouble", double.class, double.class);
    int result = (Integer)m.invoke(null, a, b);
    return result;
  }

    public static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void $stopinliner$expectLt(long a, long b) {
    if (!$opt$noinline$lt(a, b)) {
      throw new Error("Expected: " + a + " < " + b);
    }
  }

  public static void $stopinliner$expectGt(long a, long b) {
    if (!$opt$noinline$gt(a, b)) {
      throw new Error("Expected: " + a + " > " + b);
    }
  }

  public static void $stopinliner$expectLt(float a, float b) {
    if (!$opt$noinline$lt(a, b)) {
      throw new Error("Expected: " + a + " < " + b);
    }
  }

  public static void $stopinliner$expectGt(float a, float b) {
    if (!$opt$noinline$gt(a, b)) {
      throw new Error("Expected: " + a + " > " + b);
    }
  }

  public static void $stopinliner$expectFalse(float a, float b) {
    if ($opt$noinline$lt(a, b)) {
      throw new Error("Not expecting: " + a + " < " + b);
    }
    if ($opt$noinline$gt(a, b)) {
      throw new Error("Not expecting: " + a + " > " + b);
    }
  }

  public static void $stopinliner$expectLt(double a, double b) {
    if (!$opt$noinline$lt(a, b)) {
      throw new Error("Expected: " + a + " < " + b);
    }
  }

  public static void $stopinliner$expectGt(double a, double b) {
    if (!$opt$noinline$gt(a, b)) {
      throw new Error("Expected: " + a + " > " + b);
    }
  }

  public static void $stopinliner$expectFalse(double a, double b) {
    if ($opt$noinline$lt(a, b)) {
      throw new Error("Not expecting: " + a + " < " + b);
    }
    if ($opt$noinline$gt(a, b)) {
      throw new Error("Not expecting: " + a + " > " + b);
    }
  }

}
