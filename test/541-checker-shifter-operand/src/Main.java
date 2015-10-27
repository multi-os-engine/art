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

public class Main {

  // A dummy value to defeat inlining of these routines.
  static boolean doThrow = false;

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

  /**
   * Basic test merging a bitfield move operation (here a type conversion) into
   * the shifter operand.
   */

  /// CHECK-START-ARM64: long Main.$opt$noinline$translate(long, byte) instruction_simplifier_arm64 (before)
  /// CHECK-DAG:   <<l:j\d+>>           ParameterValue
  /// CHECK-DAG:   <<b:b\d+>>           ParameterValue
  /// CHECK:       <<tmp:j\d+>>         TypeConversion [<<b>>]
  /// CHECK:                            Sub [<<l>>,<<tmp>>]

  /// CHECK-START-ARM64: long Main.$opt$noinline$translate(long, byte) instruction_simplifier_arm64 (after)
  /// CHECK-DAG:   <<l:j\d+>>           ParameterValue
  /// CHECK-DAG:   <<b:b\d+>>           ParameterValue
  /// CHECK:                            Arm64DataProcWithShifterOp [<<l>>,<<b>>] (Sub+SXTB)

  /// CHECK-START-ARM64: long Main.$opt$noinline$translate(long, byte) disassembly (after)
  /// CHECK:                            sub x{{\d+}}, x{{\d+}}, w{{\d+}}, sxtb

  public static long $opt$noinline$translate(long l, byte b) {
    if (doThrow) throw new Error();
    long tmp = (long)b;
    return l - tmp;
  }


  /**
   * Test that we do not merge into the shifter operand when the left and right
   * inputs are the the IR.
   */

  /// CHECK-START-ARM64: int Main.$opt$noinline$sameInput(int) instruction_simplifier_arm64 (before)
  /// CHECK:       <<a:i\d+>>           ParameterValue
  /// CHECK:       <<Const2:i\d+>>      IntConstant 2
  /// CHECK:       <<tmp:i\d+>>         Shl [<<a>>,<<Const2>>]
  /// CHECK:                            Add [<<tmp>>,<<tmp>>]

  /// CHECK-START-ARM64: int Main.$opt$noinline$sameInput(int) instruction_simplifier_arm64 (after)
  /// CHECK-DAG:   <<a:i\d+>>           ParameterValue
  /// CHECK-DAG:   <<Const2:i\d+>>      IntConstant 2
  /// CHECK:       <<Shl:i\d+>>         Shl [<<a>>,<<Const2>>]
  /// CHECK:                            Add [<<Shl>>,<<Shl>>]

  /// CHECK-START-ARM64: int Main.$opt$noinline$sameInput(int) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:                        Arm64DataProcWithShifterOp

  public static int $opt$noinline$sameInput(int a) {
    if (doThrow) throw new Error();
    int tmp = a << 2;
    return tmp + tmp;
  }

  /**
   * Check that we perform the merge for multiple uses.
   */

  /// CHECK-START-ARM64: int Main.$opt$noinline$multipleUses(int) instruction_simplifier_arm64 (before)
  /// CHECK:       <<arg:i\d+>>         ParameterValue
  /// CHECK:       <<Const23:i\d+>>     IntConstant 23
  /// CHECK:       <<tmp:i\d+>>         Shl [<<arg>>,<<Const23>>]
  /// CHECK:                            Add [<<tmp>>,{{i\d+}}]
  /// CHECK:                            Add [<<tmp>>,{{i\d+}}]
  /// CHECK:                            Add [<<tmp>>,{{i\d+}}]
  /// CHECK:                            Add [<<tmp>>,{{i\d+}}]
  /// CHECK:                            Add [<<tmp>>,{{i\d+}}]

  /// CHECK-START-ARM64: int Main.$opt$noinline$multipleUses(int) instruction_simplifier_arm64 (after)
  /// CHECK:       <<arg:i\d+>>         ParameterValue
  /// CHECK:                            Arm64DataProcWithShifterOp [{{i\d+}},<<arg>>] (Add+LSL 23)
  /// CHECK:                            Arm64DataProcWithShifterOp [{{i\d+}},<<arg>>] (Add+LSL 23)
  /// CHECK:                            Arm64DataProcWithShifterOp [{{i\d+}},<<arg>>] (Add+LSL 23)
  /// CHECK:                            Arm64DataProcWithShifterOp [{{i\d+}},<<arg>>] (Add+LSL 23)
  /// CHECK:                            Arm64DataProcWithShifterOp [{{i\d+}},<<arg>>] (Add+LSL 23)

  public static int $opt$noinline$multipleUses(int arg) {
    if (doThrow) throw new Error();
    int tmp = arg << 23;
    switch (arg) {
      case 1:  return (arg | 1) + tmp;
      case 2:  return (arg | 2) + tmp;
      case 3:  return (arg | 3) + tmp;
      case 4:  return (arg | 4) + tmp;
      case (1 << 20):  return (arg | 5) + tmp;
      default: return 0;
    }
  }

  /**
   * Compare the reuslt of optimized operations to non-optimized operations.
   */

  static byte $noinline$longToByte(long l) {
    if (doThrow) throw new Error();
    return (byte)l;
  }
  static char $noinline$longToChar(long l) {
    if (doThrow) throw new Error();
    return (char)l;
  }
  static  int $noinline$longToInt(long l)  {
    if (doThrow) throw new Error();
    return  (int)l;
  }
  static long $noinline$LongShl(long a, long b, long c) {
    if (doThrow) throw new Error();
    return a + (b << c);
  }
  static long $noinline$LongShr(long a, long b, long c) {
    if (doThrow) throw new Error();
    return a - (b >> c);
  }
  static long $noinline$LongUshr(long a, long b, long c) {
    if (doThrow) throw new Error();
    return a ^ (b >>> c);
  }

  public static void $opt$validateLong(long a, long b) {
    if (doThrow) throw new Error();
    assertLongEquals(a + $noinline$longToByte(b), a + (byte)b);
    assertLongEquals(a + $noinline$longToChar(b), a + (char)b);
    assertLongEquals(a +  $noinline$longToInt(b), a +  (int)b);

    assertLongEquals($noinline$LongShl(a, b, 0),   a + (b <<  0));
    assertLongEquals($noinline$LongShl(a, b, 1),   a + (b <<  1));
    assertLongEquals($noinline$LongShl(a, b, 6),   a + (b <<  6));
    assertLongEquals($noinline$LongShl(a, b, 7),   a + (b <<  7));
    assertLongEquals($noinline$LongShl(a, b, 8),   a + (b <<  8));
    assertLongEquals($noinline$LongShl(a, b, 14),  a + (b << 14));
    assertLongEquals($noinline$LongShl(a, b, 15),  a + (b << 15));
    assertLongEquals($noinline$LongShl(a, b, 16),  a + (b << 16));
    assertLongEquals($noinline$LongShl(a, b, 30),  a + (b << 30));
    assertLongEquals($noinline$LongShl(a, b, 31),  a + (b << 31));
    assertLongEquals($noinline$LongShl(a, b, 32),  a + (b << 32));
    assertLongEquals($noinline$LongShl(a, b, 62),  a + (b << 62));
    assertLongEquals($noinline$LongShl(a, b, 63),  a + (b << 63));

    assertLongEquals($noinline$LongShr(a, b, 0),   a - (b >>  0));
    assertLongEquals($noinline$LongShr(a, b, 1),   a - (b >>  1));
    assertLongEquals($noinline$LongShr(a, b, 6),   a - (b >>  6));
    assertLongEquals($noinline$LongShr(a, b, 7),   a - (b >>  7));
    assertLongEquals($noinline$LongShr(a, b, 8),   a - (b >>  8));
    assertLongEquals($noinline$LongShr(a, b, 14),  a - (b >> 14));
    assertLongEquals($noinline$LongShr(a, b, 15),  a - (b >> 15));
    assertLongEquals($noinline$LongShr(a, b, 16),  a - (b >> 16));
    assertLongEquals($noinline$LongShr(a, b, 30),  a - (b >> 30));
    assertLongEquals($noinline$LongShr(a, b, 31),  a - (b >> 31));
    assertLongEquals($noinline$LongShr(a, b, 32),  a - (b >> 32));
    assertLongEquals($noinline$LongShr(a, b, 62),  a - (b >> 62));
    assertLongEquals($noinline$LongShr(a, b, 63),  a - (b >> 63));

    assertLongEquals($noinline$LongUshr(a, b, 0),   a ^ (b >>>  0));
    assertLongEquals($noinline$LongUshr(a, b, 1),   a ^ (b >>>  1));
    assertLongEquals($noinline$LongUshr(a, b, 6),   a ^ (b >>>  6));
    assertLongEquals($noinline$LongUshr(a, b, 7),   a ^ (b >>>  7));
    assertLongEquals($noinline$LongUshr(a, b, 8),   a ^ (b >>>  8));
    assertLongEquals($noinline$LongUshr(a, b, 14),  a ^ (b >>> 14));
    assertLongEquals($noinline$LongUshr(a, b, 15),  a ^ (b >>> 15));
    assertLongEquals($noinline$LongUshr(a, b, 16),  a ^ (b >>> 16));
    assertLongEquals($noinline$LongUshr(a, b, 30),  a ^ (b >>> 30));
    assertLongEquals($noinline$LongUshr(a, b, 31),  a ^ (b >>> 31));
    assertLongEquals($noinline$LongUshr(a, b, 32),  a ^ (b >>> 32));
    assertLongEquals($noinline$LongUshr(a, b, 62),  a ^ (b >>> 62));
    assertLongEquals($noinline$LongUshr(a, b, 63),  a ^ (b >>> 63));
  }


  static byte intToByte(int i) {
    if (doThrow) throw new Error();
    return (byte)i;
  }
  static char intToChar(int i) {
    if (doThrow) throw new Error();
    return (char)i;
  }
  static long intToLong(int i)  {
    if (doThrow) throw new Error();
    return (long)i;
  }
  static int $noinline$IntShl(int a, int b, int c) {
    if (doThrow) throw new Error();
    return a + (b << c);
  }
  static int $noinline$IntShr(int a, int b, int c) {
    if (doThrow) throw new Error();
    return a - (b >> c);
  }
  static int $noinline$IntUshr(int a, int b, int c) {
    if (doThrow) throw new Error();
    return a ^ (b >>> c);
  }

  public static void $opt$validateInt(int a, int b) {
    if (doThrow) throw new Error();
    assertLongEquals(a + intToByte(b), a + (byte)b);
    assertLongEquals(a + intToChar(b), a + (char)b);
    assertLongEquals(a + intToLong(b), a + (long)b);

    assertLongEquals($noinline$IntShl(a, b, 0),   a + (b <<  0));
    assertLongEquals($noinline$IntShl(a, b, 1),   a + (b <<  1));
    assertLongEquals($noinline$IntShl(a, b, 6),   a + (b <<  6));
    assertLongEquals($noinline$IntShl(a, b, 7),   a + (b <<  7));
    assertLongEquals($noinline$IntShl(a, b, 8),   a + (b <<  8));
    assertLongEquals($noinline$IntShl(a, b, 14),  a + (b << 14));
    assertLongEquals($noinline$IntShl(a, b, 15),  a + (b << 15));
    assertLongEquals($noinline$IntShl(a, b, 16),  a + (b << 16));
    assertLongEquals($noinline$IntShl(a, b, 30),  a + (b << 30));
    assertLongEquals($noinline$IntShl(a, b, 31),  a + (b << 31));
    assertLongEquals($noinline$IntShl(a, b, 32),  a + (b << 32));
    assertLongEquals($noinline$IntShl(a, b, 62),  a + (b << 62));
    assertLongEquals($noinline$IntShl(a, b, 63),  a + (b << 63));

    assertLongEquals($noinline$IntShr(a, b, 0),   a - (b >>  0));
    assertLongEquals($noinline$IntShr(a, b, 1),   a - (b >>  1));
    assertLongEquals($noinline$IntShr(a, b, 6),   a - (b >>  6));
    assertLongEquals($noinline$IntShr(a, b, 7),   a - (b >>  7));
    assertLongEquals($noinline$IntShr(a, b, 8),   a - (b >>  8));
    assertLongEquals($noinline$IntShr(a, b, 14),  a - (b >> 14));
    assertLongEquals($noinline$IntShr(a, b, 15),  a - (b >> 15));
    assertLongEquals($noinline$IntShr(a, b, 16),  a - (b >> 16));
    assertLongEquals($noinline$IntShr(a, b, 30),  a - (b >> 30));
    assertLongEquals($noinline$IntShr(a, b, 31),  a - (b >> 31));
    assertLongEquals($noinline$IntShr(a, b, 32),  a - (b >> 32));
    assertLongEquals($noinline$IntShr(a, b, 62),  a - (b >> 62));
    assertLongEquals($noinline$IntShr(a, b, 63),  a - (b >> 63));

    assertLongEquals($noinline$IntUshr(a, b, 0),   a ^ (b >>>  0));
    assertLongEquals($noinline$IntUshr(a, b, 1),   a ^ (b >>>  1));
    assertLongEquals($noinline$IntUshr(a, b, 6),   a ^ (b >>>  6));
    assertLongEquals($noinline$IntUshr(a, b, 7),   a ^ (b >>>  7));
    assertLongEquals($noinline$IntUshr(a, b, 8),   a ^ (b >>>  8));
    assertLongEquals($noinline$IntUshr(a, b, 14),  a ^ (b >>> 14));
    assertLongEquals($noinline$IntUshr(a, b, 15),  a ^ (b >>> 15));
    assertLongEquals($noinline$IntUshr(a, b, 16),  a ^ (b >>> 16));
    assertLongEquals($noinline$IntUshr(a, b, 30),  a ^ (b >>> 30));
    assertLongEquals($noinline$IntUshr(a, b, 31),  a ^ (b >>> 31));
    assertLongEquals($noinline$IntUshr(a, b, 32),  a ^ (b >>> 32));
    assertLongEquals($noinline$IntUshr(a, b, 62),  a ^ (b >>> 62));
    assertLongEquals($noinline$IntUshr(a, b, 63),  a ^ (b >>> 63));
  }


  public static void main(String[] args) {
    assertLongEquals(10000 - 3, $opt$noinline$translate((long)10000, (byte)3));
    assertLongEquals(-10000 - -3, $opt$noinline$translate((long)-10000, (byte)-3));

    assertIntEquals(4096, $opt$noinline$sameInput(512));
    assertIntEquals(-8192, $opt$noinline$sameInput(-1024));

    assertIntEquals(((1 << 23) | 1), $opt$noinline$multipleUses(1));
    assertIntEquals(((1 << 20) | 5), $opt$noinline$multipleUses(1 << 20));

    long inputs[] = {
      -((1 <<  7) - 1), -((1 <<  7)), -((1 <<  7) + 1),
      -((1 << 15) - 1), -((1 << 15)), -((1 << 15) + 1),
      -((1 << 16) - 1), -((1 << 16)), -((1 << 16) + 1),
      -((1 << 31) - 1), -((1 << 31)), -((1 << 31) + 1),
      -((1 << 32) - 1), -((1 << 32)), -((1 << 32) + 1),
      -((1 << 63) - 1), -((1 << 63)), -((1 << 63) + 1),
      -42, -314, -2718281828L, -0x123456789L, -0x987654321L,
      -1, -20, -300, -4000, -50000, -600000, -7000000, -80000000,
      0,
      1, 20, 300, 4000, 50000, 600000, 7000000, 80000000,
      42,  314,  2718281828L,  0x123456789L,  0x987654321L,
      (1 <<  7) - 1, (1 <<  7), (1 <<  7) + 1,
      (1 <<  8) - 1, (1 <<  8), (1 <<  8) + 1,
      (1 << 15) - 1, (1 << 15), (1 << 15) + 1,
      (1 << 16) - 1, (1 << 16), (1 << 16) + 1,
      (1 << 31) - 1, (1 << 31), (1 << 31) + 1,
      (1 << 32) - 1, (1 << 32), (1 << 32) + 1,
      (1 << 63) - 1, (1 << 63), (1 << 63) + 1,
      Long.MIN_VALUE, Long.MAX_VALUE
    };
    for (int i = 0; i < inputs.length; i++) {
      for (int j = 0; j < inputs.length; j++) {
        $opt$validateLong(inputs[i], inputs[j]);
        $opt$validateInt((int)inputs[i], (int)inputs[j]);
      }
    }

  }
}
