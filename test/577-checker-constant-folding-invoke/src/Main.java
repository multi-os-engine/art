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


  public static short GetShortM49() {
    return (short) -49;
  }

  public static short GetShort52() {
    return (short) 52;
  }

  public static byte GetByteM42() {
    return (byte) -42;
  }

  public static byte GetByte63() {
    return (byte) 63;
  }

  public static char GetChar32() {
    return (char) 32;
  }

  public static char GetChar51() {
    return (char) 51;
  }


  /**
   * Exercise constant folding on invocations of java.lang.Math methods.
   */

  /// CHECK-START: long Main.ReturnAbsLongM12345678901() constant_folding (before)
  /// CHECK-DAG:     <<Const:j\d+>>     LongConstant -12345678901
  /// CHECK-DAG:     <<AbsLong:j\d+>>   InvokeStaticOrDirect [<<Const>>] intrinsic:MathAbsLong
  /// CHECK-DAG:                        Return [<<AbsLong>>]

  /// CHECK-START: long Main.ReturnAbsLongM12345678901() constant_folding (after)
  /// CHECK-DAG:     <<Const:j\d+>>     LongConstant 12345678901
  /// CHECK-DAG:                        Return [<<Const>>]

  /// CHECK-START: long Main.ReturnAbsLongM12345678901() constant_folding (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static long ReturnAbsLongM12345678901() {
    long imm = -12345678901L;
    return Math.abs(imm);
  }

  /// CHECK-START: int Main.ReturnAbsIntM100() constant_folding (before)
  /// CHECK-DAG:     <<Const:i\d+>>     IntConstant -100
  /// CHECK-DAG:     <<AbsInt:i\d+>>    InvokeStaticOrDirect [<<Const>>] intrinsic:MathAbsInt
  /// CHECK-DAG:                        Return [<<AbsInt>>]

  /// CHECK-START: int Main.ReturnAbsIntM100() constant_folding (after)
  /// CHECK-DAG:     <<Const:i\d+>>     IntConstant 100
  /// CHECK-DAG:                        Return [<<Const>>]

  /// CHECK-START: int Main.ReturnAbsIntM100() constant_folding (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static int ReturnAbsIntM100() {
    int imm = -100;
    return Math.abs(imm);
  }

  /// CHECK-START: int Main.ReturnAbsShortM49() inliner (before)
  /// CHECK-DAG:     <<ShortM49:s\d+>>  InvokeStaticOrDirect method_name:Main.GetShortM49
  /// CHECK-DAG:     <<AbsInt:i\d+>>    InvokeStaticOrDirect [<<ShortM49>>] intrinsic:MathAbsInt
  /// CHECK-DAG:                        Return [<<AbsInt>>]

  /// CHECK-START: int Main.ReturnAbsShortM49() inliner (after)
  /// CHECK-DAG:     <<Const:i\d+>>     IntConstant -49
  /// CHECK-DAG:     <<AbsInt:i\d+>>    InvokeStaticOrDirect [<<Const>>] intrinsic:MathAbsInt
  /// CHECK-DAG:                        Return [<<AbsInt>>]

  /// CHECK-START: int Main.ReturnAbsShortM49() constant_folding_after_inlining (after)
  /// CHECK-DAG:     <<Const:i\d+>>     IntConstant 49
  /// CHECK-DAG:                        Return [<<Const>>]

  /// CHECK-START: int Main.ReturnAbsShortM49() constant_folding_after_inlining (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static int ReturnAbsShortM49() {
    return Math.abs(GetShortM49());
  }

  /// CHECK-START: int Main.ReturnAbsByteM42() inliner (before)
  /// CHECK-DAG:     <<ByteM42:b\d+>>   InvokeStaticOrDirect method_name:Main.GetByteM42
  /// CHECK-DAG:     <<AbsInt:i\d+>>    InvokeStaticOrDirect [<<ByteM42>>] intrinsic:MathAbsInt
  /// CHECK-DAG:                        Return [<<AbsInt>>]

  /// CHECK-START: int Main.ReturnAbsByteM42() inliner (after)
  /// CHECK-DAG:     <<Const:i\d+>>     IntConstant -42
  /// CHECK-DAG:     <<AbsInt:i\d+>>    InvokeStaticOrDirect [<<Const>>] intrinsic:MathAbsInt
  /// CHECK-DAG:                        Return [<<AbsInt>>]

  /// CHECK-START: int Main.ReturnAbsByteM42() constant_folding_after_inlining (after)
  /// CHECK-DAG:     <<Const:i\d+>>     IntConstant 42
  /// CHECK-DAG:                        Return [<<Const>>]

  /// CHECK-START: int Main.ReturnAbsByteM42() constant_folding_after_inlining (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static int ReturnAbsByteM42() {
    return Math.abs(GetByteM42());
  }

  /// CHECK-START: int Main.ReturnAbsChar51() inliner (before)
  /// CHECK-DAG:     <<Char51:c\d+>>    InvokeStaticOrDirect method_name:Main.GetChar51
  /// CHECK-DAG:     <<AbsInt:i\d+>>    InvokeStaticOrDirect [<<Char51>>] intrinsic:MathAbsInt
  /// CHECK-DAG:                        Return [<<AbsInt>>]

  /// CHECK-START: int Main.ReturnAbsChar51() inliner (after)
  /// CHECK-DAG:     <<Const:i\d+>>     IntConstant 51
  /// CHECK-DAG:     <<AbsInt:i\d+>>    InvokeStaticOrDirect [<<Const>>] intrinsic:MathAbsInt
  /// CHECK-DAG:                        Return [<<AbsInt>>]

  /// CHECK-START: int Main.ReturnAbsChar51() constant_folding_after_inlining (after)
  /// CHECK-DAG:     <<Const:i\d+>>     IntConstant 51
  /// CHECK-DAG:                        Return [<<Const>>]

  /// CHECK-START: int Main.ReturnAbsChar51() constant_folding_after_inlining (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static int ReturnAbsChar51() {
    return Math.abs(GetChar51());
  }

  /// CHECK-START: long Main.ReturnMinLongLong23456789012_M34567890123() constant_folding (before)
  /// CHECK-DAG:     <<Const1:j\d+>>    LongConstant 23456789012
  /// CHECK-DAG:     <<Const2:j\d+>>    LongConstant -34567890123
  /// CHECK-DAG:     <<MinLong:j\d+>>   InvokeStaticOrDirect [<<Const1>>,<<Const2>>] intrinsic:MathMinLongLong
  /// CHECK-DAG:                        Return [<<MinLong>>]

  /// CHECK-START: long Main.ReturnMinLongLong23456789012_M34567890123() constant_folding (after)
  /// CHECK-DAG:     <<Const:j\d+>>     LongConstant -34567890123
  /// CHECK-DAG:                        Return [<<Const>>]

  /// CHECK-START: long Main.ReturnMinLongLong23456789012_M34567890123() constant_folding (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static long ReturnMinLongLong23456789012_M34567890123() {
    long imm1 = 23456789012L;
    long imm2 = -34567890123L;
    return Math.min(imm1, imm2);
  }

  /// CHECK-START: int Main.ReturnMinIntInt123_M321() constant_folding (before)
  /// CHECK-DAG:     <<Const1:i\d+>>    IntConstant 123
  /// CHECK-DAG:     <<Const2:i\d+>>    IntConstant -321
  /// CHECK-DAG:     <<MinInt:i\d+>>    InvokeStaticOrDirect [<<Const1>>,<<Const2>>] intrinsic:MathMinIntInt
  /// CHECK-DAG:                        Return [<<MinInt>>]

  /// CHECK-START: int Main.ReturnMinIntInt123_M321() constant_folding (after)
  /// CHECK-DAG:     <<Const:i\d+>>     IntConstant -321
  /// CHECK-DAG:                        Return [<<Const>>]

  /// CHECK-START: int Main.ReturnMinIntInt123_M321() constant_folding (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static int ReturnMinIntInt123_M321() {
    int imm1 = 123;
    int imm2 = -321;
    return Math.min(imm1, imm2);
  }

  /// CHECK-START: int Main.ReturnMinShortShortM49_52() inliner (before)
  /// CHECK-DAG:     <<ShortM49:s\d+>>  InvokeStaticOrDirect method_name:Main.GetShortM49
  /// CHECK-DAG:     <<Short52:s\d+>>   InvokeStaticOrDirect method_name:Main.GetShort52
  /// CHECK-DAG:     <<MinInt:i\d+>>    InvokeStaticOrDirect [<<ShortM49>>,<<Short52>>] intrinsic:MathMinIntInt
  /// CHECK-DAG:                        Return [<<MinInt>>]

  /// CHECK-START: int Main.ReturnMinShortShortM49_52() inliner (after)
  /// CHECK-DAG:     <<ConstM49:i\d+>>  IntConstant -49
  /// CHECK-DAG:     <<Const52:i\d+>>   IntConstant 52
  /// CHECK-DAG:     <<MinInt:i\d+>>    InvokeStaticOrDirect [<<ConstM49>>,<<Const52>>] intrinsic:MathMinIntInt
  /// CHECK-DAG:                        Return [<<MinInt>>]

  /// CHECK-START: int Main.ReturnMinShortShortM49_52() constant_folding_after_inlining (after)
  /// CHECK-DAG:     <<ConstM49:i\d+>>  IntConstant -49
  /// CHECK-DAG:     <<Const52:i\d+>>   IntConstant 52
  /// CHECK-DAG:                        Return [<<ConstM49>>]

  /// CHECK-START: int Main.ReturnMinShortShortM49_52() constant_folding_after_inlining (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static int ReturnMinShortShortM49_52() {
    return Math.min(GetShortM49(), GetShort52());
  }

  /// CHECK-START: int Main.ReturnMinByteByteM42_63() inliner (before)
  /// CHECK-DAG:     <<ByteM42:b\d+>>   InvokeStaticOrDirect method_name:Main.GetByteM42
  /// CHECK-DAG:     <<Byte63:b\d+>>    InvokeStaticOrDirect method_name:Main.GetByte63
  /// CHECK-DAG:     <<MinInt:i\d+>>    InvokeStaticOrDirect [<<ByteM42>>,<<Byte63>>] intrinsic:MathMinIntInt
  /// CHECK-DAG:                        Return [<<MinInt>>]

  /// CHECK-START: int Main.ReturnMinByteByteM42_63() inliner (after)
  /// CHECK-DAG:     <<ConstM42:i\d+>>  IntConstant -42
  /// CHECK-DAG:     <<Const63:i\d+>>   IntConstant 63
  /// CHECK-DAG:     <<MinInt:i\d+>>    InvokeStaticOrDirect [<<ConstM42>>,<<Const63>>] intrinsic:MathMinIntInt
  /// CHECK-DAG:                        Return [<<MinInt>>]

  /// CHECK-START: int Main.ReturnMinByteByteM42_63() constant_folding_after_inlining (after)
  /// CHECK-DAG:     <<ConstM42:i\d+>>  IntConstant -42
  /// CHECK-DAG:     <<Const63:i\d+>>   IntConstant 63
  /// CHECK-DAG:                        Return [<<ConstM42>>]

  /// CHECK-START: int Main.ReturnMinByteByteM42_63() constant_folding_after_inlining (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static int ReturnMinByteByteM42_63() {
    return Math.min(GetByteM42(), GetByte63());
  }

  /// CHECK-START: int Main.ReturnMinCharChar51_32() inliner (before)
  /// CHECK-DAG:     <<Char51:c\d+>>    InvokeStaticOrDirect method_name:Main.GetChar51
  /// CHECK-DAG:     <<Char32:c\d+>>    InvokeStaticOrDirect method_name:Main.GetChar32
  /// CHECK-DAG:     <<MinInt:i\d+>>    InvokeStaticOrDirect [<<Char51>>,<<Char32>>] intrinsic:MathMinIntInt
  /// CHECK-DAG:                        Return [<<MinInt>>]

  /// CHECK-START: int Main.ReturnMinCharChar51_32() inliner (after)
  /// CHECK-DAG:     <<Const51:i\d+>>   IntConstant 51
  /// CHECK-DAG:     <<Const32:i\d+>>   IntConstant 32
  /// CHECK-DAG:     <<MinInt:i\d+>>    InvokeStaticOrDirect [<<Const51>>,<<Const32>>] intrinsic:MathMinIntInt
  /// CHECK-DAG:                        Return [<<MinInt>>]

  /// CHECK-START: int Main.ReturnMinCharChar51_32() constant_folding_after_inlining (after)
  /// CHECK-DAG:     <<Const51:i\d+>>   IntConstant 51
  /// CHECK-DAG:     <<Const32:i\d+>>   IntConstant 32
  /// CHECK-DAG:                        Return [<<Const32>>]

  /// CHECK-START: int Main.ReturnMinCharChar51_32() constant_folding_after_inlining (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static int ReturnMinCharChar51_32() {
    return Math.min(GetChar51(), GetChar32());
  }

  /// CHECK-START: long Main.ReturnMaxLongLong23456789012_M34567890123() constant_folding (before)
  /// CHECK-DAG:     <<Const1:j\d+>>    LongConstant 23456789012
  /// CHECK-DAG:     <<Const2:j\d+>>    LongConstant -34567890123
  /// CHECK-DAG:     <<MaxLong:j\d+>>   InvokeStaticOrDirect [<<Const1>>,<<Const2>>] intrinsic:MathMaxLongLong
  /// CHECK-DAG:                        Return [<<MaxLong>>]

  /// CHECK-START: long Main.ReturnMaxLongLong23456789012_M34567890123() constant_folding (after)
  /// CHECK-DAG:     <<Const:j\d+>>     LongConstant 23456789012
  /// CHECK-DAG:                        Return [<<Const>>]

  /// CHECK-START: long Main.ReturnMaxLongLong23456789012_M34567890123() constant_folding (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static long ReturnMaxLongLong23456789012_M34567890123() {
    long imm1 = 23456789012L;
    long imm2 = -34567890123L;
    return Math.max(imm1, imm2);
  }

  /// CHECK-START: int Main.ReturnMaxIntInt123_M321() constant_folding (before)
  /// CHECK-DAG:     <<Const1:i\d+>>    IntConstant 123
  /// CHECK-DAG:     <<Const2:i\d+>>    IntConstant -321
  /// CHECK-DAG:     <<MaxInt:i\d+>>    InvokeStaticOrDirect [<<Const1>>,<<Const2>>] intrinsic:MathMaxIntInt
  /// CHECK-DAG:                        Return [<<MaxInt>>]

  /// CHECK-START: int Main.ReturnMaxIntInt123_M321() constant_folding (after)
  /// CHECK-DAG:     <<Const:i\d+>>     IntConstant 123
  /// CHECK-DAG:                        Return [<<Const>>]

  /// CHECK-START: int Main.ReturnMaxIntInt123_M321() constant_folding (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static int ReturnMaxIntInt123_M321() {
    int imm1 = 123;
    int imm2 = -321;
    return Math.max(imm1, imm2);
  }

  /// CHECK-START: int Main.ReturnMaxShortShortM49_52() inliner (before)
  /// CHECK-DAG:     <<ShortM49:s\d+>>  InvokeStaticOrDirect method_name:Main.GetShortM49
  /// CHECK-DAG:     <<Short52:s\d+>>   InvokeStaticOrDirect method_name:Main.GetShort52
  /// CHECK-DAG:     <<MaxInt:i\d+>>    InvokeStaticOrDirect [<<ShortM49>>,<<Short52>>] intrinsic:MathMaxIntInt
  /// CHECK-DAG:                        Return [<<MaxInt>>]

  /// CHECK-START: int Main.ReturnMaxShortShortM49_52() inliner (after)
  /// CHECK-DAG:     <<ConstM49:i\d+>>  IntConstant -49
  /// CHECK-DAG:     <<Const52:i\d+>>   IntConstant 52
  /// CHECK-DAG:     <<MaxInt:i\d+>>    InvokeStaticOrDirect [<<ConstM49>>,<<Const52>>] intrinsic:MathMaxIntInt
  /// CHECK-DAG:                        Return [<<MaxInt>>]

  /// CHECK-START: int Main.ReturnMaxShortShortM49_52() constant_folding_after_inlining (after)
  /// CHECK-DAG:     <<ConstM49:i\d+>>  IntConstant -49
  /// CHECK-DAG:     <<Const52:i\d+>>   IntConstant 52
  /// CHECK-DAG:                        Return [<<Const52>>]

  /// CHECK-START: int Main.ReturnMaxShortShortM49_52() constant_folding_after_inlining (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static int ReturnMaxShortShortM49_52() {
    return Math.max(GetShortM49(), GetShort52());
  }

  /// CHECK-START: int Main.ReturnMaxByteByteM42_63() inliner (before)
  /// CHECK-DAG:     <<ByteM42:b\d+>>   InvokeStaticOrDirect method_name:Main.GetByteM42
  /// CHECK-DAG:     <<Byte63:b\d+>>    InvokeStaticOrDirect method_name:Main.GetByte63
  /// CHECK-DAG:     <<MaxInt:i\d+>>    InvokeStaticOrDirect [<<ByteM42>>,<<Byte63>>] intrinsic:MathMaxIntInt
  /// CHECK-DAG:                        Return [<<MaxInt>>]

  /// CHECK-START: int Main.ReturnMaxByteByteM42_63() inliner (after)
  /// CHECK-DAG:     <<ConstM42:i\d+>>  IntConstant -42
  /// CHECK-DAG:     <<Const63:i\d+>>   IntConstant 63
  /// CHECK-DAG:     <<MaxInt:i\d+>>    InvokeStaticOrDirect [<<ConstM42>>,<<Const63>>] intrinsic:MathMaxIntInt
  /// CHECK-DAG:                        Return [<<MaxInt>>]

  /// CHECK-START: int Main.ReturnMaxByteByteM42_63() constant_folding_after_inlining (after)
  /// CHECK-DAG:     <<ConstM42:i\d+>>  IntConstant -42
  /// CHECK-DAG:     <<Const63:i\d+>>   IntConstant 63
  /// CHECK-DAG:                        Return [<<Const63>>]

  /// CHECK-START: int Main.ReturnMaxByteByteM42_63() constant_folding_after_inlining (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static int ReturnMaxByteByteM42_63() {
    return Math.max(GetByteM42(), GetByte63());
  }

  /// CHECK-START: int Main.ReturnMaxCharChar51_32() inliner (before)
  /// CHECK-DAG:     <<Char51:c\d+>>    InvokeStaticOrDirect method_name:Main.GetChar51
  /// CHECK-DAG:     <<Char32:c\d+>>    InvokeStaticOrDirect method_name:Main.GetChar32
  /// CHECK-DAG:     <<MaxInt:i\d+>>    InvokeStaticOrDirect [<<Char51>>,<<Char32>>] intrinsic:MathMaxIntInt
  /// CHECK-DAG:                        Return [<<MaxInt>>]

  /// CHECK-START: int Main.ReturnMaxCharChar51_32() inliner (after)
  /// CHECK-DAG:     <<Const51:i\d+>>   IntConstant 51
  /// CHECK-DAG:     <<Const32:i\d+>>   IntConstant 32
  /// CHECK-DAG:     <<MaxInt:i\d+>>    InvokeStaticOrDirect [<<Const51>>,<<Const32>>] intrinsic:MathMaxIntInt
  /// CHECK-DAG:                        Return [<<MaxInt>>]

  /// CHECK-START: int Main.ReturnMaxCharChar51_32() constant_folding_after_inlining (after)
  /// CHECK-DAG:     <<Const51:i\d+>>   IntConstant 51
  /// CHECK-DAG:     <<Const32:i\d+>>   IntConstant 32
  /// CHECK-DAG:                        Return [<<Const51>>]

  /// CHECK-START: int Main.ReturnMaxCharChar51_32() constant_folding_after_inlining (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static int ReturnMaxCharChar51_32() {
    return Math.max(GetChar51(), GetChar32());
  }


  public static void main(String[] args) throws Exception {
    assertLongEquals(12345678901L, ReturnAbsLongM12345678901());
    assertIntEquals(100, ReturnAbsIntM100());
    assertIntEquals(49, ReturnAbsShortM49());
    assertIntEquals(42, ReturnAbsByteM42());
    assertIntEquals(51, ReturnAbsChar51());

    assertLongEquals(-34567890123L, ReturnMinLongLong23456789012_M34567890123());
    assertIntEquals(-321, ReturnMinIntInt123_M321());
    assertIntEquals(-49, ReturnMinShortShortM49_52());
    assertIntEquals(-42, ReturnMinByteByteM42_63());
    assertIntEquals(32, ReturnMinCharChar51_32());

    assertLongEquals(23456789012L, ReturnMaxLongLong23456789012_M34567890123());
    assertIntEquals(123, ReturnMaxIntInt123_M321());
    assertIntEquals(52, ReturnMaxShortShortM49_52());
    assertIntEquals(63, ReturnMaxByteByteM42_63());
    assertIntEquals(51, ReturnMaxCharChar51_32());
  }
}
