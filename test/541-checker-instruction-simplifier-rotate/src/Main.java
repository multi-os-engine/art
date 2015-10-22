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

  //  (i >>> #distance) | (i << #(reg_bits - distance))

  /// CHECK-START-ARM64: int Main.rotate_int_right_constant_c_c(int) instruction_simplifier_arm64 (before)
  /// CHECK:          <<ArgValue:i\d+>>     ParameterValue
  /// CHECK:          <<Const2:i\d+>>       IntConstant 2
  /// CHECK:          <<Const30:i\d+>>      IntConstant 30
  /// CHECK-DAG:      <<UShr:i\d+>>         UShr [<<ArgValue>>,<<Const2>>]
  /// CHECK-DAG:      <<Shl:i\d+>>          Shl [<<ArgValue>>,<<Const30>>]
  /// CHECK:          <<Or:i\d+>>           Or [<<UShr>>,<<Shl>>]
  /// CHECK:                                Return [<<Or>>]

  /// CHECK-START-ARM64: int Main.rotate_int_right_constant_c_c(int) instruction_simplifier_arm64 (after)
  /// CHECK:          <<ArgValue:i\d+>>     ParameterValue
  /// CHECK:          <<Const2:i\d+>>       IntConstant 2
  /// CHECK:          <<Arm64Ror:i\d+>>     Arm64Ror [<<ArgValue>>,<<Const2>>]
  /// CHECK:                                Return [<<Arm64Ror>>]

  /// CHECK-START-ARM64: int Main.rotate_int_right_constant_c_c(int) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:      UShr
  /// CHECK-NOT:      Shl
  public static int rotate_int_right_constant_c_c(int value) {
    return (value >>> 2) | (value << 30);
  }

  /// CHECK-START-ARM64: int Main.rotate_int_right_constant_c_c_0(int) instruction_simplifier_arm64 (after)
  /// CHECK:          <<ArgValue:i\d+>>     ParameterValue
  /// CHECK:          <<Const2:i\d+>>       IntConstant 2
  /// CHECK:          <<Arm64Ror:i\d+>>     Arm64Ror [<<ArgValue>>,<<Const2>>]
  /// CHECK:                                Return [<<Arm64Ror>>]
  public static int rotate_int_right_constant_c_c_0(int value) {
    return (value >>> 2) | (value << 62);
  }


  //  (j >>> #distance) | (j << #(reg_bits - distance))

  /// CHECK-START-ARM64: long Main.rotate_long_right_constant_c_c(long) instruction_simplifier_arm64 (before)
  /// CHECK:          <<ArgValue:j\d+>>     ParameterValue
  /// CHECK:          <<Const2:i\d+>>       IntConstant 2
  /// CHECK:          <<Const62:i\d+>>      IntConstant 62
  /// CHECK-DAG:      <<UShr:j\d+>>         UShr [<<ArgValue>>,<<Const2>>]
  /// CHECK-DAG:      <<Shl:j\d+>>          Shl [<<ArgValue>>,<<Const62>>]
  /// CHECK:          <<Or:j\d+>>           Or [<<UShr>>,<<Shl>>]
  /// CHECK:                                Return [<<Or>>]

  /// CHECK-START-ARM64: long Main.rotate_long_right_constant_c_c(long) instruction_simplifier_arm64 (after)
  /// CHECK:          <<ArgValue:j\d+>>     ParameterValue
  /// CHECK:          <<Const2:i\d+>>       IntConstant 2
  /// CHECK:          <<Arm64Ror:j\d+>>     Arm64Ror [<<ArgValue>>,<<Const2>>]
  /// CHECK:                                Return [<<Arm64Ror>>]

  /// CHECK-START-ARM64: long Main.rotate_long_right_constant_c_c(long) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:      UShr
  /// CHECK-NOT:      Shl
  public static long rotate_long_right_constant_c_c(long value) {
    return (value >>> 2) | (value << 62);
  }

  /// CHECK-START-ARM64: long Main.rotate_long_right_constant_c_c_0(long) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:      Arm64Ror
  public static long rotate_long_right_constant_c_c_0(long value) {
    return (value >>> 2) | (value << 30);
  }


  //  (i >>> #distance) | (i << #-distance)

  /// CHECK-START-ARM64: int Main.rotate_int_right_constant_c_negc(int) instruction_simplifier_arm64 (before)
  /// CHECK:          <<ArgValue:i\d+>>     ParameterValue
  /// CHECK:          <<Const2:i\d+>>       IntConstant 2
  /// CHECK:          <<ConstNeg2:i\d+>>    IntConstant -2
  /// CHECK-DAG:      <<UShr:i\d+>>         UShr [<<ArgValue>>,<<Const2>>]
  /// CHECK-DAG:      <<Shl:i\d+>>          Shl [<<ArgValue>>,<<ConstNeg2>>]
  /// CHECK:          <<Or:i\d+>>           Or [<<UShr>>,<<Shl>>]
  /// CHECK:                                Return [<<Or>>]

  /// CHECK-START-ARM64: int Main.rotate_int_right_constant_c_negc(int) instruction_simplifier_arm64 (after)
  /// CHECK:          <<ArgValue:i\d+>>     ParameterValue
  /// CHECK:          <<Const2:i\d+>>       IntConstant 2
  /// CHECK:          <<Arm64Ror:i\d+>>     Arm64Ror [<<ArgValue>>,<<Const2>>]
  /// CHECK:                                Return [<<Arm64Ror>>]
  public static int rotate_int_right_constant_c_negc(int value) {
    return (value >>> 2) | (value << -2);
  }


  //  (j >>> #distance) | (j << #-distance)

  /// CHECK-START-ARM64: long Main.rotate_long_right_constant_c_negc(long) instruction_simplifier_arm64 (before)
  /// CHECK:          <<ArgValue:j\d+>>     ParameterValue
  /// CHECK:          <<Const2:i\d+>>       IntConstant 2
  /// CHECK:          <<ConstNeg2:i\d+>>    IntConstant -2
  /// CHECK-DAG:      <<UShr:j\d+>>         UShr [<<ArgValue>>,<<Const2>>]
  /// CHECK-DAG:      <<Shl:j\d+>>          Shl [<<ArgValue>>,<<ConstNeg2>>]
  /// CHECK:          <<Or:j\d+>>           Or [<<UShr>>,<<Shl>>]
  /// CHECK:                                Return [<<Or>>]

  /// CHECK-START-ARM64: long Main.rotate_long_right_constant_c_negc(long) instruction_simplifier_arm64 (after)
  /// CHECK:          <<ArgValue:j\d+>>     ParameterValue
  /// CHECK:          <<Const2:i\d+>>       IntConstant 2
  /// CHECK:          <<Arm64Ror:j\d+>>     Arm64Ror [<<ArgValue>>,<<Const2>>]
  /// CHECK:                                Return [<<Arm64Ror>>]
  public static long rotate_long_right_constant_c_negc(long value) {
    return (value >>> 2) | (value << -2);
  }


  //  (i >>> distance) | (i << (#reg_bits - distance)

  /// CHECK-START-ARM64: int Main.rotate_int_right_reg_v_csubv(int, int) instruction_simplifier_arm64 (before)
  /// CHECK:          <<ArgValue:i\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK:          <<Const32:i\d+>>      IntConstant 32
  /// CHECK-DAG:      <<UShr:i\d+>>         UShr [<<ArgValue>>,<<ArgDistance>>]
  /// CHECK-DAG:      <<Sub:i\d+>>          Sub [<<Const32>>,<<ArgDistance>>]
  /// CHECK-DAG:      <<Shl:i\d+>>          Shl [<<ArgValue>>,<<Sub>>]
  /// CHECK:          <<Or:i\d+>>           Or [<<UShr>>,<<Shl>>]
  /// CHECK:                                Return [<<Or>>]

  /// CHECK-START-ARM64: int Main.rotate_int_right_reg_v_csubv(int, int) instruction_simplifier_arm64 (after)
  /// CHECK:          <<ArgValue:i\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK:          <<Arm64Ror:i\d+>>     Arm64Ror [<<ArgValue>>,<<ArgDistance>>]
  /// CHECK:                                Return [<<Arm64Ror>>]

  /// CHECK-START-ARM64: int Main.rotate_int_right_reg_v_csubv(int, int) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:      UShr
  /// CHECK-NOT:      Shl
  /// CHECK-NOT:      Sub
  public static int rotate_int_right_reg_v_csubv(int value, int distance) {
    return (value >>> distance) | (value << (32 - distance));
  }


  //  (j >>> distance) | (j << (#reg_bits - distance)

  /// CHECK-START-ARM64: long Main.rotate_long_right_reg_v_csubv(long, int) instruction_simplifier_arm64 (before)
  /// CHECK:          <<ArgValue:j\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK:          <<Const64:i\d+>>      IntConstant 64
  /// CHECK-DAG:      <<UShr:j\d+>>         UShr [<<ArgValue>>,<<ArgDistance>>]
  /// CHECK-DAG:      <<Sub:i\d+>>          Sub [<<Const64>>,<<ArgDistance>>]
  /// CHECK-DAG:      <<Shl:j\d+>>          Shl [<<ArgValue>>,<<Sub>>]
  /// CHECK:          <<Or:j\d+>>           Or [<<UShr>>,<<Shl>>]
  /// CHECK:                                Return [<<Or>>]

  /// CHECK-START-ARM64: long Main.rotate_long_right_reg_v_csubv(long, int) instruction_simplifier_arm64 (after)
  /// CHECK:          <<ArgValue:j\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK:          <<Arm64Ror:j\d+>>     Arm64Ror [<<ArgValue>>,<<ArgDistance>>]
  /// CHECK:                                Return [<<Arm64Ror>>]

  /// CHECK-START-ARM64: long Main.rotate_long_right_reg_v_csubv(long, int) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:      UShr
  /// CHECK-NOT:      Shl
  /// CHECK-NOT:      Sub
  public static long rotate_long_right_reg_v_csubv(long value, int distance) {
    return (value >>> distance) | (value << (64 - distance));
  }

  /// CHECK-START-ARM64: long Main.rotate_long_right_reg_v_csubv_0(long, int) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:      Arm64Ror
  public static long rotate_long_right_reg_v_csubv_0(long value, int distance) {
    return (value >>> distance) | (value << (32 - distance));
  }


  //  (i >>> (#reg_bits - distance)) | (i << distance)

  /// CHECK-START-ARM64: int Main.rotate_int_left_reg_csubv_v(int, int) instruction_simplifier_arm64 (before)
  /// CHECK:          <<ArgValue:i\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK:          <<Const32:i\d+>>      IntConstant 32
  /// CHECK-DAG:      <<Sub:i\d+>>          Sub [<<Const32>>,<<ArgDistance>>]
  /// CHECK-DAG:      <<UShr:i\d+>>         UShr [<<ArgValue>>,<<Sub>>]
  /// CHECK-DAG:      <<Shl:i\d+>>          Shl [<<ArgValue>>,<<ArgDistance>>]
  /// CHECK:          <<Or:i\d+>>           Or [<<UShr>>,<<Shl>>]
  /// CHECK:                                Return [<<Or>>]

  /// CHECK-START-ARM64: int Main.rotate_int_left_reg_csubv_v(int, int) instruction_simplifier_arm64 (after)
  /// CHECK:          <<ArgValue:i\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK:          <<Neg:i\d+>>          Neg [<<ArgDistance>>]
  /// CHECK:          <<Arm64Ror:i\d+>>     Arm64Ror [<<ArgValue>>,<<Neg>>]
  /// CHECK:                                Return [<<Arm64Ror>>]
  public static int rotate_int_left_reg_csubv_v(int value, int distance) {
    return (value >>> (32 - distance)) | (value << distance);
  }


  //  (j >>> (#reg_bits - distance)) | (j << distance)

  /// CHECK-START-ARM64: long Main.rotate_long_left_reg_csubv_v(long, int) instruction_simplifier_arm64 (before)
  /// CHECK:          <<ArgValue:j\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK:          <<Const64:i\d+>>      IntConstant 64
  /// CHECK-DAG:      <<Sub:i\d+>>          Sub [<<Const64>>,<<ArgDistance>>]
  /// CHECK-DAG:      <<UShr:j\d+>>         UShr [<<ArgValue>>,<<Sub>>]
  /// CHECK-DAG:      <<Shl:j\d+>>          Shl [<<ArgValue>>,<<ArgDistance>>]
  /// CHECK:          <<Or:j\d+>>           Or [<<UShr>>,<<Shl>>]
  /// CHECK:                                Return [<<Or>>]

  /// CHECK-START-ARM64: long Main.rotate_long_left_reg_csubv_v(long, int) instruction_simplifier_arm64 (after)
  /// CHECK:          <<ArgValue:j\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK:          <<Neg:i\d+>>          Neg [<<ArgDistance>>]
  /// CHECK:          <<Arm64Ror:j\d+>>     Arm64Ror [<<ArgValue>>,<<Neg>>]
  /// CHECK:                                Return [<<Arm64Ror>>]
  public static long rotate_long_left_reg_csubv_v(long value, int distance) {
    return (value >>> (64 - distance)) | (value << distance);
  }

  /// CHECK-START-ARM64: long Main.rotate_long_left_reg_csubv_v_0(long, int) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:      Arm64Ror
  public static long rotate_long_left_reg_csubv_v_0(long value, int distance) {
    return (value >>> (32 - distance)) | (value << distance);
  }


  //  (i >>> distance) | (i << -distance) (i.e. libcore's Integer.rotateRight)

  /// CHECK-START-ARM64: int Main.rotate_int_right_reg_v_negv(int, int) instruction_simplifier_arm64 (before)
  /// CHECK:          <<ArgValue:i\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK-DAG:      <<UShr:i\d+>>         UShr [<<ArgValue>>,<<ArgDistance>>]
  /// CHECK-DAG:      <<Neg:i\d+>>          Neg [<<ArgDistance>>]
  /// CHECK-DAG:      <<Shl:i\d+>>          Shl [<<ArgValue>>,<<Neg>>]
  /// CHECK:          <<Or:i\d+>>           Or [<<UShr>>,<<Shl>>]
  /// CHECK:                                Return [<<Or>>]

  /// CHECK-START-ARM64: int Main.rotate_int_right_reg_v_negv(int, int) instruction_simplifier_arm64 (after)
  /// CHECK:          <<ArgValue:i\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK:          <<Arm64Ror:i\d+>>     Arm64Ror [<<ArgValue>>,<<ArgDistance>>]
  /// CHECK:                                Return [<<Arm64Ror>>]

  /// CHECK-START-ARM64: int Main.rotate_int_right_reg_v_negv(int, int) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:      UShr
  /// CHECK-NOT:      Shl
  /// CHECK-NOT:      Neg
  public static int rotate_int_right_reg_v_negv(int value, int distance) {
    return (value >>> distance) | (value << -distance);
  }


  //  (j >>> distance) | (j << -distance) (i.e. libcore's Long.rotateRight)

  /// CHECK-START-ARM64: long Main.rotate_long_right_reg_v_negv(long, int) instruction_simplifier_arm64 (before)
  /// CHECK:          <<ArgValue:j\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK-DAG:      <<UShr:j\d+>>         UShr [<<ArgValue>>,<<ArgDistance>>]
  /// CHECK-DAG:      <<Neg:i\d+>>          Neg [<<ArgDistance>>]
  /// CHECK-DAG:      <<Shl:j\d+>>          Shl [<<ArgValue>>,<<Neg>>]
  /// CHECK:          <<Or:j\d+>>           Or [<<UShr>>,<<Shl>>]
  /// CHECK:                                Return [<<Or>>]

  /// CHECK-START-ARM64: long Main.rotate_long_right_reg_v_negv(long, int) instruction_simplifier_arm64 (after)
  /// CHECK:          <<ArgValue:j\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK:          <<Arm64Ror:j\d+>>     Arm64Ror [<<ArgValue>>,<<ArgDistance>>]
  /// CHECK:                                Return [<<Arm64Ror>>]

  /// CHECK-START-ARM64: long Main.rotate_long_right_reg_v_negv(long, int) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:      UShr
  /// CHECK-NOT:      Shl
  /// CHECK-NOT:      Neg
  public static long rotate_long_right_reg_v_negv(long value, int distance) {
    return (value >>> distance) | (value << -distance);
  }


  //  (i << distance) | (i >>> -distance) (i.e. libcore's Integer.rotateLeft)

  /// CHECK-START-ARM64: int Main.rotate_int_left_reg_negv_v(int, int) instruction_simplifier_arm64 (before)
  /// CHECK:          <<ArgValue:i\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK-DAG:      <<Neg:i\d+>>          Neg [<<ArgDistance>>]
  /// CHECK-DAG:      <<UShr:i\d+>>         UShr [<<ArgValue>>,<<Neg>>]
  /// CHECK-DAG:      <<Shl:i\d+>>          Shl [<<ArgValue>>,<<ArgDistance>>]
  /// CHECK:          <<Or:i\d+>>           Or [<<Shl>>,<<UShr>>]
  /// CHECK:                                Return [<<Or>>]

  /// CHECK-START-ARM64: int Main.rotate_int_left_reg_negv_v(int, int) instruction_simplifier_arm64 (after)
  /// CHECK:          <<ArgValue:i\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK:          <<Neg:i\d+>>          Neg [<<ArgDistance>>]
  /// CHECK:          <<Arm64Ror:i\d+>>     Arm64Ror [<<ArgValue>>,<<Neg>>]
  /// CHECK:                                Return [<<Arm64Ror>>]
  public static int rotate_int_left_reg_negv_v(int value, int distance) {
    return (value << distance) | (value >>> -distance);
  }


  //  (j << distance) | (j >>> -distance) (i.e. libcore's Long.rotateLeft)

  /// CHECK-START-ARM64: long Main.rotate_long_left_reg_negv_v(long, int) instruction_simplifier_arm64 (before)
  /// CHECK:          <<ArgValue:j\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK-DAG:      <<Neg:i\d+>>          Neg [<<ArgDistance>>]
  /// CHECK-DAG:      <<UShr:j\d+>>         UShr [<<ArgValue>>,<<Neg>>]
  /// CHECK-DAG:      <<Shl:j\d+>>          Shl [<<ArgValue>>,<<ArgDistance>>]
  /// CHECK:          <<Or:j\d+>>           Or [<<Shl>>,<<UShr>>]
  /// CHECK:                                Return [<<Or>>]

  /// CHECK-START-ARM64: long Main.rotate_long_left_reg_negv_v(long, int) instruction_simplifier_arm64 (after)
  /// CHECK:          <<ArgValue:j\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK:          <<Neg:i\d+>>          Neg [<<ArgDistance>>]
  /// CHECK:          <<Arm64Ror:j\d+>>     Arm64Ror [<<ArgValue>>,<<Neg>>]
  /// CHECK:                                Return [<<Arm64Ror>>]
  public static long rotate_long_left_reg_negv_v(long value, int distance) {
    return (value << distance) | (value >>> -distance);
  }


  public static void main(String[] args) {
    assertIntEquals(2, rotate_int_right_constant_c_c(8));
    assertIntEquals(2, rotate_int_right_constant_c_c_0(8));
    assertLongEquals(2L, rotate_long_right_constant_c_c(8L));

    assertIntEquals(2, rotate_int_right_constant_c_negc(8));
    assertLongEquals(2L, rotate_long_right_constant_c_negc(8L));

    assertIntEquals(2, rotate_int_right_reg_v_csubv(8, 2));
    assertLongEquals(2L, rotate_long_right_reg_v_csubv(8L, 2));

    assertIntEquals(32, rotate_int_left_reg_csubv_v(8, 2));
    assertLongEquals(32L, rotate_long_left_reg_csubv_v(8L, 2));

    assertIntEquals(2, rotate_int_right_reg_v_negv(8, 2));
    assertLongEquals(2L, rotate_long_right_reg_v_negv(8L, 2));

    assertIntEquals(32, rotate_int_left_reg_negv_v(8, 2));
    assertLongEquals(32L, rotate_long_left_reg_negv_v(8L, 2));
  }
}
