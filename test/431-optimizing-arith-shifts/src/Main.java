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
    shlInt();
    shlLong();
    shrInt();
    shrLong();
    ushrInt();
    ushrLong();
  }

  private static void shlInt() {
    expectEquals(48, $opt$noinline$ShlConst2(12));
    expectEquals(12, $opt$noinline$ShlConst0(12));
    expectEquals(-48, $opt$noinline$Shl(-12, 2));
    expectEquals(1024, $opt$noinline$Shl(32, 5));

    expectEquals(7, $opt$noinline$Shl(7, 0));
    expectEquals(14, $opt$noinline$Shl(7, 1));
    expectEquals(0, $opt$noinline$Shl(0, 30));

    expectEquals(1073741824L, $opt$noinline$Shl(1, 30));
    expectEquals(Integer.MIN_VALUE, $opt$noinline$Shl(1, 31));  // overflow
    expectEquals(Integer.MIN_VALUE, $opt$noinline$Shl(1073741824, 1));  // overflow
    expectEquals(1073741824, $opt$noinline$Shl(268435456, 2));

    // Only the 5 lower bits should be used for shifting (& 0x1f).
    expectEquals(7, $opt$noinline$Shl(7, 32));  // 32 & 0x1f = 0
    expectEquals(14, $opt$noinline$Shl(7, 33));  // 33 & 0x1f = 1
    expectEquals(32, $opt$noinline$Shl(1, 101));  // 101 & 0x1f = 5

    expectEquals(Integer.MIN_VALUE, $opt$noinline$Shl(1, -1));  // -1 & 0x1f = 31
    expectEquals(14, $opt$noinline$Shl(7, -31));  // -31 & 0x1f = 1
    expectEquals(7, $opt$noinline$Shl(7, -32));  // -32 & 0x1f = 0
    expectEquals(-536870912, $opt$noinline$Shl(7, -3));  // -3 & 0x1f = 29

    expectEquals(Integer.MIN_VALUE, $opt$noinline$Shl(7, Integer.MAX_VALUE));
    expectEquals(7, $opt$noinline$Shl(7, Integer.MIN_VALUE));
  }

  private static void shlLong() {
    expectEquals(48L, $opt$noinline$ShlConst2(12L));
    expectEquals(12L, $opt$noinline$ShlConst0(12L));
    expectEquals(-48L, $opt$noinline$Shl(-12L, 2L));
    expectEquals(1024L, $opt$noinline$Shl(32L, 5L));

    expectEquals(7L, $opt$noinline$Shl(7L, 0L));
    expectEquals(14L, $opt$noinline$Shl(7L, 1L));
    expectEquals(0L, $opt$noinline$Shl(0L, 30L));

    expectEquals(1073741824L, $opt$noinline$Shl(1L, 30L));
    expectEquals(2147483648L, $opt$noinline$Shl(1L, 31L));
    expectEquals(2147483648L, $opt$noinline$Shl(1073741824L, 1L));

    // Long shifts can use up to 6 lower bits.
    expectEquals(4294967296L, $opt$noinline$Shl(1L, 32L));
    expectEquals(60129542144L, $opt$noinline$Shl(7L, 33L));
    expectEquals(Long.MIN_VALUE, $opt$noinline$Shl(1L, 63L));  // overflow

    // Only the 6 lower bits should be used for shifting (& 0x3f).
    expectEquals(7L, $opt$noinline$Shl(7L, 64L));  // 64 & 0x3f = 0
    expectEquals(14L, $opt$noinline$Shl(7L, 65L));  // 65 & 0x3f = 1
    expectEquals(137438953472L, $opt$noinline$Shl(1L, 101L));  // 101 & 0x3f = 37

    expectEquals(Long.MIN_VALUE, $opt$noinline$Shl(1L, -1L));  // -1 & 0x3f = 63
    expectEquals(14L, $opt$noinline$Shl(7L, -63L));  // -63 & 0x3f = 1
    expectEquals(7L, $opt$noinline$Shl(7L, -64L));  // -64 & 0x3f = 0
    expectEquals(2305843009213693952L, $opt$noinline$Shl(1L, -3L));  // -3 & 0x3f = 61

    expectEquals(Long.MIN_VALUE, $opt$noinline$Shl(7L, Long.MAX_VALUE));
    expectEquals(7L, $opt$noinline$Shl(7L, Long.MIN_VALUE));

    // Exercise some special cases handled by backends/simplifier.
    expectEquals(24L, $opt$noinline$ShlConst1(12L));
    expectEquals(0x2345678900000000L, $opt$noinline$ShlConst32(0x123456789L));
    expectEquals(0x2490249000000000L, $opt$noinline$ShlConst33(0x12481248L));
    expectEquals(0x4920492000000000L, $opt$noinline$ShlConst34(0x12481248L));
    expectEquals(0x9240924000000000L, $opt$noinline$ShlConst35(0x12481248L));
  }

  private static void shrInt() {
    expectEquals(3, $opt$noinline$ShrConst2(12));
    expectEquals(12, $opt$noinline$ShrConst0(12));
    expectEquals(-3, $opt$noinline$Shr(-12, 2));
    expectEquals(1, $opt$noinline$Shr(32, 5));

    expectEquals(7, $opt$noinline$Shr(7, 0));
    expectEquals(3, $opt$noinline$Shr(7, 1));
    expectEquals(0, $opt$noinline$Shr(0, 30));
    expectEquals(0, $opt$noinline$Shr(1, 30));
    expectEquals(-1, $opt$noinline$Shr(-1, 30));

    expectEquals(0, $opt$noinline$Shr(Integer.MAX_VALUE, 31));
    expectEquals(-1, $opt$noinline$Shr(Integer.MIN_VALUE, 31));

    // Only the 5 lower bits should be used for shifting (& 0x1f).
    expectEquals(7, $opt$noinline$Shr(7, 32));  // 32 & 0x1f = 0
    expectEquals(3, $opt$noinline$Shr(7, 33));  // 33 & 0x1f = 1

    expectEquals(0, $opt$noinline$Shr(1, -1));  // -1 & 0x1f = 31
    expectEquals(3, $opt$noinline$Shr(7, -31));  // -31 & 0x1f = 1
    expectEquals(7, $opt$noinline$Shr(7, -32));  // -32 & 0x1f = 0
    expectEquals(-4, $opt$noinline$Shr(Integer.MIN_VALUE, -3));  // -3 & 0x1f = 29

    expectEquals(0, $opt$noinline$Shr(7, Integer.MAX_VALUE));
    expectEquals(7, $opt$noinline$Shr(7, Integer.MIN_VALUE));
  }

  private static void shrLong() {
    expectEquals(3L, $opt$noinline$ShrConst2(12L));
    expectEquals(12L, $opt$noinline$ShrConst0(12L));
    expectEquals(-3L, $opt$noinline$Shr(-12L, 2L));
    expectEquals(1, $opt$noinline$Shr(32, 5));

    expectEquals(7L, $opt$noinline$Shr(7L, 0L));
    expectEquals(3L, $opt$noinline$Shr(7L, 1L));
    expectEquals(0L, $opt$noinline$Shr(0L, 30L));
    expectEquals(0L, $opt$noinline$Shr(1L, 30L));
    expectEquals(-1L, $opt$noinline$Shr(-1L, 30L));


    expectEquals(1L, $opt$noinline$Shr(1073741824L, 30L));
    expectEquals(1L, $opt$noinline$Shr(2147483648L, 31L));
    expectEquals(1073741824L, $opt$noinline$Shr(2147483648L, 1L));

    // Long shifts can use up to 6 lower bits.
    expectEquals(1L, $opt$noinline$Shr(4294967296L, 32L));
    expectEquals(7L, $opt$noinline$Shr(60129542144L, 33L));
    expectEquals(0L, $opt$noinline$Shr(Long.MAX_VALUE, 63L));
    expectEquals(-1L, $opt$noinline$Shr(Long.MIN_VALUE, 63L));

    // Only the 6 lower bits should be used for shifting (& 0x3f).
    expectEquals(7L, $opt$noinline$Shr(7L, 64L));  // 64 & 0x3f = 0
    expectEquals(3L, $opt$noinline$Shr(7L, 65L));  // 65 & 0x3f = 1

    expectEquals(-1L, $opt$noinline$Shr(Long.MIN_VALUE, -1L));  // -1 & 0x3f = 63
    expectEquals(3L, $opt$noinline$Shr(7L, -63L));  // -63 & 0x3f = 1
    expectEquals(7L, $opt$noinline$Shr(7L, -64L));  // -64 & 0x3f = 0
    expectEquals(1L, $opt$noinline$Shr(2305843009213693952L, -3L));  // -3 & 0x3f = 61
    expectEquals(-4L, $opt$noinline$Shr(Integer.MIN_VALUE, -3));  // -3 & 0x1f = 29

    expectEquals(0L, $opt$noinline$Shr(7L, Long.MAX_VALUE));
    expectEquals(7L, $opt$noinline$Shr(7L, Long.MIN_VALUE));
  }

  private static void ushrInt() {
    expectEquals(3, $opt$noinline$UShrConst2(12));
    expectEquals(12, $opt$noinline$UShrConst0(12));
    expectEquals(1073741821, $opt$noinline$UShr(-12, 2));
    expectEquals(1, $opt$noinline$UShr(32, 5));

    expectEquals(7, $opt$noinline$UShr(7, 0));
    expectEquals(3, $opt$noinline$UShr(7, 1));
    expectEquals(0, $opt$noinline$UShr(0, 30));
    expectEquals(0, $opt$noinline$UShr(1, 30));
    expectEquals(3, $opt$noinline$UShr(-1, 30));

    expectEquals(0, $opt$noinline$UShr(Integer.MAX_VALUE, 31));
    expectEquals(1, $opt$noinline$UShr(Integer.MIN_VALUE, 31));

    // Only the 5 lower bits should be used for shifting (& 0x1f).
    expectEquals(7, $opt$noinline$UShr(7, 32));  // 32 & 0x1f = 0
    expectEquals(3, $opt$noinline$UShr(7, 33));  // 33 & 0x1f = 1

    expectEquals(0, $opt$noinline$UShr(1, -1));  // -1 & 0x1f = 31
    expectEquals(3, $opt$noinline$UShr(7, -31));  // -31 & 0x1f = 1
    expectEquals(7, $opt$noinline$UShr(7, -32));  // -32 & 0x1f = 0
    expectEquals(4, $opt$noinline$UShr(Integer.MIN_VALUE, -3));  // -3 & 0x1f = 29

    expectEquals(0, $opt$noinline$UShr(7, Integer.MAX_VALUE));
    expectEquals(7, $opt$noinline$UShr(7, Integer.MIN_VALUE));
  }

  private static void ushrLong() {
    expectEquals(3L, $opt$noinline$UShrConst2(12L));
    expectEquals(12L, $opt$noinline$UShrConst0(12L));
    expectEquals(4611686018427387901L, $opt$noinline$UShr(-12L, 2L));
    expectEquals(1, $opt$noinline$UShr(32, 5));

    expectEquals(7L, $opt$noinline$UShr(7L, 0L));
    expectEquals(3L, $opt$noinline$UShr(7L, 1L));
    expectEquals(0L, $opt$noinline$UShr(0L, 30L));
    expectEquals(0L, $opt$noinline$UShr(1L, 30L));
    expectEquals(17179869183L, $opt$noinline$UShr(-1L, 30L));


    expectEquals(1L, $opt$noinline$UShr(1073741824L, 30L));
    expectEquals(1L, $opt$noinline$UShr(2147483648L, 31L));
    expectEquals(1073741824L, $opt$noinline$UShr(2147483648L, 1L));

    // Long shifts can use use up to 6 lower bits.
    expectEquals(1L, $opt$noinline$UShr(4294967296L, 32L));
    expectEquals(7L, $opt$noinline$UShr(60129542144L, 33L));
    expectEquals(0L, $opt$noinline$UShr(Long.MAX_VALUE, 63L));
    expectEquals(1L, $opt$noinline$UShr(Long.MIN_VALUE, 63L));

    // Only the 6 lower bits should be used for shifting (& 0x3f).
    expectEquals(7L, $opt$noinline$UShr(7L, 64L));  // 64 & 0x3f = 0
    expectEquals(3L, $opt$noinline$UShr(7L, 65L));  // 65 & 0x3f = 1

    expectEquals(1L, $opt$noinline$UShr(Long.MIN_VALUE, -1L));  // -1 & 0x3f = 63
    expectEquals(3L, $opt$noinline$UShr(7L, -63L));  // -63 & 0x3f = 1
    expectEquals(7L, $opt$noinline$UShr(7L, -64L));  // -64 & 0x3f = 0
    expectEquals(1L, $opt$noinline$UShr(2305843009213693952L, -3L));  // -3 & 0x3f = 61
    expectEquals(4L, $opt$noinline$UShr(Long.MIN_VALUE, -3L));  // -3 & 0x3f = 61

    expectEquals(0L, $opt$noinline$UShr(7L, Long.MAX_VALUE));
    expectEquals(7L, $opt$noinline$UShr(7L, Long.MIN_VALUE));
  }

  static boolean doThrow = false;

  static int $opt$noinline$Shl(int a, int b) {
    if (doThrow) {
      // Try defeating inlining.
      throw new Error();
    }
    return a << b;
  }

  static long $opt$noinline$Shl(long a, long b) {
    if (doThrow) {
      // Try defeating inlining.
      throw new Error();
    }
    return a << b;
  }

  static int $opt$noinline$Shr(int a, int b) {
    if (doThrow) {
      // Try defeating inlining.
      throw new Error();
    }
    return a >> b;
  }

  static long $opt$noinline$Shr(long a, long b) {
    if (doThrow) {
      // Try defeating inlining.
      throw new Error();
    }
    return a >> b;
  }

  static int $opt$noinline$UShr(int a, int b) {
    if (doThrow) {
      // Try defeating inlining.
      throw new Error();
    }
    return a >>> b;
  }

  static long $opt$noinline$UShr(long a, long b) {
    if (doThrow) {
      // Try defeating inlining.
      throw new Error();
    }
    return a >>> b;
  }

  static int $opt$noinline$ShlConst2(int a) {
    if (doThrow) {
      // Try defeating inlining.
      throw new Error();
    }
    return a << 2;
  }

  static long $opt$noinline$ShlConst2(long a) {
    if (doThrow) {
      // Try defeating inlining.
      throw new Error();
    }
    return a << 2L;
  }

  static int $opt$noinline$ShrConst2(int a) {
    if (doThrow) {
      // Try defeating inlining.
      throw new Error();
    }
    return a >> 2;
  }

  static long $opt$noinline$ShrConst2(long a) {
    if (doThrow) {
      // Try defeating inlining.
      throw new Error();
    }
    return a >> 2L;
  }

  static int $opt$noinline$UShrConst2(int a) {
    if (doThrow) {
      // Try defeating inlining.
      throw new Error();
    }
    return a >>> 2;
  }

  static long $opt$noinline$UShrConst2(long a) {
    if (doThrow) {
      // Try defeating inlining.
      throw new Error();
    }
    return a >>> 2L;
  }

  static int $opt$noinline$ShlConst0(int a) {
    if (doThrow) {
      // Try defeating inlining.
      throw new Error();
    }
    return a << 0;
  }

  static long $opt$noinline$ShlConst0(long a) {
    if (doThrow) {
      // Try defeating inlining.
      throw new Error();
    }
    return a << 0L;
  }

  static int $opt$noinline$ShrConst0(int a) {
    if (doThrow) {
      // Try defeating inlining.
      throw new Error();
    }
    return a >> 0;
  }

  static long $opt$noinline$ShrConst0(long a) {
    if (doThrow) {
      // Try defeating inlining.
      throw new Error();
    }
    return a >> 0L;
  }

  static int $opt$noinline$UShrConst0(int a) {
    if (doThrow) {
      // Try defeating inlining.
      throw new Error();
    }
    return a >>> 0;
  }

  static long $opt$noinline$UShrConst0(long a) {
    if (doThrow) {
      // Try defeating inlining.
      throw new Error();
    }
    return a >>> 0L;
  }

  static long $opt$noinline$ShlConst1(long a) {
    if (doThrow) {
      // Try defeating inlining.
      throw new Error();
    }
    return a << 1L;
  }

  static long $opt$noinline$ShlConst32(long a) {
    if (doThrow) {
      // Try defeating inlining.
      throw new Error();
    }
    return a << 32L;
  }

  static long $opt$noinline$ShlConst33(long a) {
    if (doThrow) {
      // Try defeating inlining.
      throw new Error();
    }
    return a << 33L;
  }

  static long $opt$noinline$ShlConst34(long a) {
    if (doThrow) {
      // Try defeating inlining.
      throw new Error();
    }
    return a << 34L;
  }

  static long $opt$noinline$ShlConst35(long a) {
    if (doThrow) {
      // Try defeating inlining.
      throw new Error();
    }
    return a << 35L;
  }

}
