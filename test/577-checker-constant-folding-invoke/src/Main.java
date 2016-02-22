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

  public static void assertFloatEquals(float expected, float result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void assertDoubleEquals(double expected, double result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void assertApproxFloatEquals(float expected, float result, float maxDelta) {
    boolean aproxEquals = (expected > result)
      ? ((expected - result) < maxDelta)
      : ((result - expected) < maxDelta);
    if (!aproxEquals) {
      throw new Error("Expected: " + expected + ", found: " + result + ", with delta: " + maxDelta);
    }
  }

  public static void assertApproxDoubleEquals(double expected, double result, double maxDelta) {
    boolean aproxEquals = (expected > result)
      ? ((expected - result) < maxDelta)
      : ((result - expected) < maxDelta);
    if (!aproxEquals) {
      throw new Error("Expected: " + expected + ", found: " + result + ", with delta: " + maxDelta);
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

  /// CHECK-START: double Main.ReturnAbsDoubleM1P5() constant_folding (before)
  /// CHECK-DAG:     <<Const:d\d+>>     DoubleConstant -1.5
  /// CHECK-DAG:     <<AbsDouble:d\d+>> InvokeStaticOrDirect [<<Const>>] intrinsic:MathAbsDouble
  /// CHECK-DAG:                        Return [<<AbsDouble>>]

  /// CHECK-START: double Main.ReturnAbsDoubleM1P5() constant_folding (after)
  /// CHECK-DAG:     <<Const:d\d+>>     DoubleConstant 1.5
  /// CHECK-DAG:                        Return [<<Const>>]

  /// CHECK-START: double Main.ReturnAbsDoubleM1P5() constant_folding (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static double ReturnAbsDoubleM1P5() {
    double imm = -1.5D;
    return Math.abs(imm);
  }

  /// CHECK-START: float Main.ReturnAbsFloatM2P5() constant_folding (before)
  /// CHECK-DAG:     <<Const:f\d+>>     FloatConstant -2.5
  /// CHECK-DAG:     <<AbsFloat:f\d+>>  InvokeStaticOrDirect [<<Const>>] intrinsic:MathAbsFloat
  /// CHECK-DAG:                        Return [<<AbsFloat>>]

  /// CHECK-START: float Main.ReturnAbsFloatM2P5() constant_folding (after)
  /// CHECK-DAG:     <<Const:f\d+>>     FloatConstant 2.5
  /// CHECK-DAG:                        Return [<<Const>>]

  /// CHECK-START: float Main.ReturnAbsFloatM2P5() constant_folding (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static float ReturnAbsFloatM2P5() {
    float imm = -2.5F;
    return Math.abs(imm);
  }

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

  /// CHECK-START: double Main.ReturnMinDoubleDoubleM3P5_4P5() constant_folding (before)
  /// CHECK-DAG:     <<Const1:d\d+>>    DoubleConstant -3.5
  /// CHECK-DAG:     <<Const2:d\d+>>    DoubleConstant 4.5
  /// CHECK-DAG:     <<MinDouble:d\d+>> InvokeStaticOrDirect [<<Const1>>,<<Const2>>] intrinsic:MathMinDoubleDouble
  /// CHECK-DAG:                        Return [<<MinDouble>>]

  /// CHECK-START: double Main.ReturnMinDoubleDoubleM3P5_4P5() constant_folding (after)
  /// CHECK-DAG:     <<Const:d\d+>>     DoubleConstant -3.5
  /// CHECK-DAG:                        Return [<<Const>>]

  /// CHECK-START: double Main.ReturnMinDoubleDoubleM3P5_4P5() constant_folding (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static double ReturnMinDoubleDoubleM3P5_4P5() {
    double imm1 = -3.5D;
    double imm2 = 4.5D;
    return Math.min(imm1, imm2);
  }

  /// CHECK-START: float Main.ReturnMinFloatFloatM5P5_6P5() constant_folding (before)
  /// CHECK-DAG:     <<Const1:f\d+>>    FloatConstant -5.5
  /// CHECK-DAG:     <<Const2:f\d+>>    FloatConstant 6.5
  /// CHECK-DAG:     <<MinFloat:f\d+>>  InvokeStaticOrDirect [<<Const1>>,<<Const2>>] intrinsic:MathMinFloatFloat
  /// CHECK-DAG:                        Return [<<MinFloat>>]

  /// CHECK-START: float Main.ReturnMinFloatFloatM5P5_6P5() constant_folding (after)
  /// CHECK-DAG:     <<Const:f\d+>>     FloatConstant -5.5
  /// CHECK-DAG:                        Return [<<Const>>]

  /// CHECK-START: float Main.ReturnMinFloatFloatM5P5_6P5() constant_folding (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static float ReturnMinFloatFloatM5P5_6P5() {
    float imm1 = -5.5F;
    float imm2 = 6.5F;
    return Math.min(imm1, imm2);
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

  /// CHECK-START: double Main.ReturnMaxDoubleDoubleM3P5_4P5() constant_folding (before)
  /// CHECK-DAG:     <<Const1:d\d+>>    DoubleConstant -3.5
  /// CHECK-DAG:     <<Const2:d\d+>>    DoubleConstant 4.5
  /// CHECK-DAG:     <<MaxDouble:d\d+>> InvokeStaticOrDirect [<<Const1>>,<<Const2>>] intrinsic:MathMaxDoubleDouble
  /// CHECK-DAG:                        Return [<<MaxDouble>>]

  /// CHECK-START: double Main.ReturnMaxDoubleDoubleM3P5_4P5() constant_folding (after)
  /// CHECK-DAG:     <<Const:d\d+>>     DoubleConstant 4.5
  /// CHECK-DAG:                        Return [<<Const>>]

  /// CHECK-START: double Main.ReturnMaxDoubleDoubleM3P5_4P5() constant_folding (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static double ReturnMaxDoubleDoubleM3P5_4P5() {
    double imm1 = -3.5D;
    double imm2 = 4.5D;
    return Math.max(imm1, imm2);
  }

  /// CHECK-START: float Main.ReturnMaxFloatFloatM5P5_6P5() constant_folding (before)
  /// CHECK-DAG:     <<Const1:f\d+>>    FloatConstant -5.5
  /// CHECK-DAG:     <<Const2:f\d+>>    FloatConstant 6.5
  /// CHECK-DAG:     <<MaxFloat:f\d+>>  InvokeStaticOrDirect [<<Const1>>,<<Const2>>] intrinsic:MathMaxFloatFloat
  /// CHECK-DAG:                        Return [<<MaxFloat>>]

  /// CHECK-START: float Main.ReturnMaxFloatFloatM5P5_6P5() constant_folding (after)
  /// CHECK-DAG:     <<Const:f\d+>>     FloatConstant 6.5
  /// CHECK-DAG:                        Return [<<Const>>]

  /// CHECK-START: float Main.ReturnMaxFloatFloatM5P5_6P5() constant_folding (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static float ReturnMaxFloatFloatM5P5_6P5() {
    float imm1 = -5.5F;
    float imm2 = 6.5F;
    return Math.max(imm1, imm2);
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

  /// CHECK-START: double Main.ReturnCosPiDiv4() constant_folding (before)
  /// CHECK-DAG:     <<Const:d\d+>>     DoubleConstant <<PiDiv4:\d+(\.\d*)?>>
  /// CHECK-EVAL:    abs(<<PiDiv4>> - 0.785398163) < 0.00001
  /// CHECK-DAG:     <<Cos:d\d+>>       InvokeStaticOrDirect [<<Const>>] intrinsic:MathCos
  /// CHECK-DAG:                        Return [<<Cos>>]

  // There are two DoubleConstant instructions after constant folding:
  // the first one is the `pi / 4` value passed to the Math.cos
  // function, while the second one is the result of the static
  // evaluation of `cos(pi / 4)` operation (`sqrt(2) / 2`).  To make
  // sure `<<Const>>` matches the latter, try to match the Return
  // instruction first.

  /// CHECK-START: double Main.ReturnCosPiDiv4() constant_folding (after)
  /// CHECK-DAG:                        Return [<<Const:d\d+>>]
  /// CHECK-DAG:     <<Const>>          DoubleConstant <<Sqrt2Div2:\d+(\.\d*)?>>
  /// CHECK-EVAL:    abs(<<Sqrt2Div2>> - 0.707106781) < 0.00001

  /// CHECK-START: double Main.ReturnCosPiDiv4() constant_folding (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static double ReturnCosPiDiv4() {
    double imm = Math.PI / 4;
    return Math.cos(imm);
  }

  /// CHECK-START: double Main.ReturnSinPiDiv4() constant_folding (before)
  /// CHECK-DAG:     <<Const:d\d+>>     DoubleConstant <<PiDiv4:\d+(\.\d*)?>>
  /// CHECK-EVAL:    abs(<<PiDiv4>> - 0.785398163) < 0.00001
  /// CHECK-DAG:     <<Sin:d\d+>>       InvokeStaticOrDirect [<<Const>>] intrinsic:MathSin
  /// CHECK-DAG:                        Return [<<Sin>>]

  // There are two DoubleConstant instructions after constant folding:
  // the first one is the `pi / 4` value passed to the Math.sin
  // function, while the second one is the result of the static
  // evaluation of `sin(pi / 4)` operation (`sqrt(2) / 2`).  To make
  // sure `<<Const>>` matches the latter, try to match the Return
  // instruction first.

  /// CHECK-START: double Main.ReturnSinPiDiv4() constant_folding (after)
  /// CHECK-DAG:                        Return [<<Const:d\d+>>]
  /// CHECK-DAG:     <<Const>>          DoubleConstant <<Sqrt2Div2:\d+(\.\d*)?>>
  /// CHECK-EVAL:    abs(<<Sqrt2Div2>> - 0.707106781) < 0.00001

  /// CHECK-START: double Main.ReturnSinPiDiv4() constant_folding (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static double ReturnSinPiDiv4() {
    double imm = Math.PI / 4;
    return Math.sin(imm);
  }

  /// CHECK-START: double Main.ReturnAcosSqrt2Div2() constant_folding (before)
  /// CHECK-DAG:     <<Const2:d\d+>>    DoubleConstant 2
  /// CHECK-DAG:     <<Sqrt:d\d+>>      InvokeStaticOrDirect [<<Const2>>] intrinsic:MathSqrt
  /// CHECK-DAG:     <<Div:d\d+>>       Div [<<Sqrt>>,<<Const2>>]
  /// CHECK-DAG:     <<Acos:d\d+>>      InvokeStaticOrDirect [<<Div>>] intrinsic:MathAcos
  /// CHECK-DAG:                        Return [<<Acos>>]

  // There are two DoubleConstant instructions after constant folding:
  // the first one is the `sqrt(2) / 2` value passed to the Math.acos
  // function, while the second one is the result of the static
  // evaluation of `acos(sqrt(2) / 2)` operation (`pi / 4`).  To make
  // sure `<<Const>>` matches the latter, try to match the Return
  // instruction first.

  /// CHECK-START: double Main.ReturnAcosSqrt2Div2() constant_folding (after)
  /// CHECK-DAG:                        Return [<<Const:d\d+>>]
  /// CHECK-DAG:     <<Const>>          DoubleConstant <<PiDiv4:\d+(\.\d*)?>>
  /// CHECK-EVAL:    abs(<<PiDiv4>> - 0.785398163) < 0.00001

  /// CHECK-START: double Main.ReturnAcosSqrt2Div2() constant_folding_after_inlining (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static double ReturnAcosSqrt2Div2() {
    double imm = Math.sqrt(2) / 2;
    return Math.acos(imm);
  }

  /// CHECK-START: double Main.ReturnAsinSqrt2Div2() constant_folding (before)
  /// CHECK-DAG:     <<Const2:d\d+>>    DoubleConstant 2
  /// CHECK-DAG:     <<Sqrt:d\d+>>      InvokeStaticOrDirect [<<Const2>>] intrinsic:MathSqrt
  /// CHECK-DAG:     <<Div:d\d+>>       Div [<<Sqrt>>,<<Const2>>]
  /// CHECK-DAG:     <<Asin:d\d+>>      InvokeStaticOrDirect [<<Div>>] intrinsic:MathAsin
  /// CHECK-DAG:                        Return [<<Asin>>]

  // There are two DoubleConstant instructions after constant folding:
  // the first one is the `sqrt(2) / 2` value passed to the Math.asin
  // function, while the second one is the result of the static
  // evaluation of `asin(sqrt(2) / 2)` operation (`pi / 4`).  To make
  // sure `<<Const>>` matches the latter, try to match the Return
  // instruction first.

  /// CHECK-START: double Main.ReturnAsinSqrt2Div2() constant_folding (after)
  /// CHECK-DAG:                        Return [<<Const:d\d+>>]
  /// CHECK-DAG:     <<Const>>          DoubleConstant <<PiDiv4:\d+(\.\d*)?>>
  /// CHECK-EVAL:    abs(<<PiDiv4>> - 0.785398163) < 0.00001

  /// CHECK-START: double Main.ReturnAsinSqrt2Div2() constant_folding_after_inlining (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static double ReturnAsinSqrt2Div2() {
    double imm = Math.sqrt(2) / 2;
    return Math.asin(imm);
  }

  /// CHECK-START: double Main.ReturnAtanOne() constant_folding (before)
  /// CHECK-DAG:     <<Const:d\d+>>     DoubleConstant 1
  /// CHECK-DAG:     <<Atan:d\d+>>      InvokeStaticOrDirect [<<Const>>] intrinsic:MathAtan
  /// CHECK-DAG:                        Return [<<Atan>>]

  // There are two DoubleConstant instructions after constant folding:
  // the first one is the `1` value passed to the Math.atan
  // function, while the second one is the result of the static
  // evaluation of `atan(1)` operation (`pi / 4`).  To make
  // sure `<<Const>>` matches the latter, try to match the Return
  // instruction first.

  /// CHECK-START: double Main.ReturnAtanOne() constant_folding (after)
  /// CHECK-DAG:                        Return [<<Const:d\d+>>]
  /// CHECK-DAG:     <<Const>>          DoubleConstant <<PiDiv4:\d+(\.\d*)?>>
  /// CHECK-EVAL:    abs(<<PiDiv4>> - 0.785398163) < 0.00001

  /// CHECK-START: double Main.ReturnAtanOne() constant_folding (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static double ReturnAtanOne() {
    double imm = 1;
    return Math.atan(imm);
  }

  /// CHECK-START: double Main.ReturnAtan2MinusOneDivOne() constant_folding (before)
  /// CHECK-DAG:     <<ConstM1:d\d+>>   DoubleConstant -1
  /// CHECK-DAG:     <<Const1:d\d+>>    DoubleConstant 1
  /// CHECK-DAG:     <<Atan2:d\d+>>     InvokeStaticOrDirect [<<ConstM1>>,<<Const1>>] intrinsic:MathAtan2
  /// CHECK-DAG:                        Return [<<Atan2>>]

  // There are two DoubleConstant instructions after constant folding:
  // the first one is the `1` value passed to the Math.atan2
  // function, while the second one is the result of the static
  // evaluation of `atan2(1, -1)` operation (`-pi / 4`).  To make
  // sure `<<Const>>` matches the latter, try to match the Return
  // instruction first.

  /// CHECK-START: double Main.ReturnAtan2MinusOneDivOne() constant_folding (after)
  /// CHECK-DAG:                        Return [<<Const:d\d+>>]
  /// CHECK-DAG:     <<Const>>          DoubleConstant <<MinusPiDiv4:-\d+(\.\d*)?>>
  /// CHECK-EVAL:    abs(<<MinusPiDiv4>> - -0.785398163) < 0.00001

  /// CHECK-START: double Main.ReturnAtan2MinusOneDivOne() constant_folding (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static double ReturnAtan2MinusOneDivOne() {
    double imm1 = -1;
    double imm2 = 1;
    return Math.atan2(imm1, imm2);
  }

  /// CHECK-START: double Main.ReturnCbrthEight() constant_folding (before)
  /// CHECK-DAG:     <<Const:d\d+>>     DoubleConstant 8
  /// CHECK-DAG:     <<Cbrt:d\d+>>      InvokeStaticOrDirect [<<Const>>] intrinsic:MathCbrt
  /// CHECK-DAG:                        Return [<<Cbrt>>]

  /// CHECK-START: double Main.ReturnCbrthEight() constant_folding (after)
  /// CHECK-DAG:     <<Const:d\d+>>     DoubleConstant 2
  /// CHECK-DAG:                        Return [<<Const>>]

  /// CHECK-START: double Main.ReturnCbrthEight() constant_folding (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static double ReturnCbrthEight() {
    double imm = 8;
    return Math.cbrt(imm);
  }

  /// CHECK-START: double Main.ReturnCoshOne() constant_folding (before)
  /// CHECK-DAG:     <<Const:d\d+>>     DoubleConstant 1
  /// CHECK-DAG:     <<Cosh:d\d+>>      InvokeStaticOrDirect [<<Const>>] intrinsic:MathCosh
  /// CHECK-DAG:                        Return [<<Cosh>>]

  // There are two DoubleConstant instructions after constant folding:
  // the first one is the `1` value passed to the Math.cosh function,
  // while the second one is the result of the static evaluation of
  // `cosh(1)` operation.  To make sure `<<Const>>` matches the latter,
  // try to match the Return instruction first.

  /// CHECK-START: double Main.ReturnCoshOne() constant_folding (after)
  /// CHECK-DAG:                        Return [<<Const:d\d+>>]
  /// CHECK-DAG:     <<Const>>          DoubleConstant <<One:\d+(\.\d*)?>>
  /// CHECK-EVAL:    abs(<<One>> - 1.543080635) < 0.00001

  /// CHECK-START: double Main.ReturnCoshOne() constant_folding (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static double ReturnCoshOne() {
    double imm = 1;
    return Math.cosh(imm);
  }

  /// CHECK-START: double Main.ReturnExpOne() constant_folding (before)
  /// CHECK-DAG:     <<Const:d\d+>>     DoubleConstant 1
  /// CHECK-DAG:     <<Exp:d\d+>>       InvokeStaticOrDirect [<<Const>>] intrinsic:MathExp
  /// CHECK-DAG:                        Return [<<Exp>>]

  // There are two DoubleConstant instructions after constant folding:
  // the first one is the `1` value passed to the Math.exp function,
  // while the second one is the result of the static evaluation of
  // `exp(1)` operation.  To make sure `<<Const>>` matches the latter,
  // try to match the Return instruction first.

  /// CHECK-START: double Main.ReturnExpOne() constant_folding (after)
  /// CHECK-DAG:                        Return [<<Const:d\d+>>]
  /// CHECK-DAG:     <<Const>>          DoubleConstant <<One:\d+(\.\d*)?>>
  /// CHECK-EVAL:    abs(<<One>> - 2.718281828) < 0.00001

  /// CHECK-START: double Main.ReturnExpOne() constant_folding (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static double ReturnExpOne() {
    double imm = 1;
    return Math.exp(imm);
  }

  /// CHECK-START: double Main.ReturnExpm1One() constant_folding (before)
  /// CHECK-DAG:     <<Const:d\d+>>     DoubleConstant 1
  /// CHECK-DAG:     <<Expm1:d\d+>>     InvokeStaticOrDirect [<<Const>>] intrinsic:MathExpm1
  /// CHECK-DAG:                        Return [<<Expm1>>]

  // There are two DoubleConstant instructions after constant folding:
  // the first one is the `1` value passed to the Math.expm1 function,
  // while the second one is the result of the static evaluation of
  // `expm1(1)` operation (`e^1 - 1`).  To make sure `<<Const>>`
  // matches the latter, try to match the Return instruction first.

  /// CHECK-START: double Main.ReturnExpm1One() constant_folding (after)
  /// CHECK-DAG:                        Return [<<Const:d\d+>>]
  /// CHECK-DAG:     <<Const>>          DoubleConstant <<One:\d+(\.\d*)?>>
  /// CHECK-EVAL:    abs(<<One>> - 1.718281828) < 0.00001

  /// CHECK-START: double Main.ReturnExpm1One() constant_folding (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static double ReturnExpm1One() {
    double imm = 1;
    return Math.expm1(imm);
  }

  /// CHECK-START: double Main.ReturnHypot3_4() constant_folding (before)
  /// CHECK-DAG:     <<Const3:d\d+>>    DoubleConstant 3
  /// CHECK-DAG:     <<Const4:d\d+>>    DoubleConstant 4
  /// CHECK-DAG:     <<Hypot:d\d+>>     InvokeStaticOrDirect [<<Const3>>,<<Const4>>] intrinsic:MathHypot
  /// CHECK-DAG:                        Return [<<Hypot>>]

  /// CHECK-START: double Main.ReturnHypot3_4() constant_folding (after)
  /// CHECK-DAG:     <<Const:d\d+>>     DoubleConstant 5
  /// CHECK-DAG:                        Return [<<Const>>]

  /// CHECK-START: double Main.ReturnHypot3_4() constant_folding (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static double ReturnHypot3_4() {
    double imm1 = 3D;
    double imm2 = 4D;
    return Math.hypot(imm1, imm2);
  }

  /// CHECK-START: double Main.ReturnLogOne() constant_folding (before)
  /// CHECK-DAG:     <<Const:d\d+>>     DoubleConstant 1
  /// CHECK-DAG:     <<Log:d\d+>>       InvokeStaticOrDirect [<<Const>>] intrinsic:MathLog
  /// CHECK-DAG:                        Return [<<Log>>]

  /// CHECK-START: double Main.ReturnLogOne() constant_folding (after)
  /// CHECK-DAG:     <<Const:d\d+>>     DoubleConstant 0
  /// CHECK-DAG:                        Return [<<Const>>]

  /// CHECK-START: double Main.ReturnLogOne() constant_folding (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static double ReturnLogOne() {
    double imm = 1;
    return Math.log(imm);
  }

  /// CHECK-START: double Main.ReturnLog10OneHundred() constant_folding (before)
  /// CHECK-DAG:     <<Const:d\d+>>     DoubleConstant 100
  /// CHECK-DAG:     <<Log10:d\d+>>     InvokeStaticOrDirect [<<Const>>] intrinsic:MathLog10
  /// CHECK-DAG:                        Return [<<Log10>>]

  /// CHECK-START: double Main.ReturnLog10OneHundred() constant_folding (after)
  /// CHECK-DAG:     <<Const:d\d+>>     DoubleConstant 2
  /// CHECK-DAG:                        Return [<<Const>>]

  /// CHECK-START: double Main.ReturnLog10OneHundred() constant_folding (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static double ReturnLog10OneHundred() {
    double imm = 100;
    return Math.log10(imm);
  }

  // There are two DoubleConstant instructions before constant
  // folding: the first one is the `Double.MAX_VALUE` value passed as
  // first argument to the Math.nextAfter function, while the second
  // one is the `Double.POSITIVE_INFINITY` value passed as second
  // argument to that function. We can reliably match the latter (as
  // `inf`), but not the latter, as it might be precision-dependent.
  // To make sure `<<ConstMax>>` matches the first of these constants
  // (`Double.MAX_VALUE`), try to match the try to match the second
  // constant (`Double.POSITIVE_INFINITY`) first (and capture it as
  // `<<ConstInf>>`); only then try to match the first constant.

  /// CHECK-START: double Main.ReturnNexAfterMaxDouble() constant_folding (before)
  /// CHECK-DAG:     <<ConstInf:d\d+>>  DoubleConstant inf
  /// CHECK-DAG:     <<ConstMax:d\d+>>  DoubleConstant <<MaxDouble:\d+(\.\d*)?([eE]\+\d+)?>>
  /// CHECK-EVAL:    abs(<<MaxDouble>> - 1.79769e+308) < 0.00001
  /// CHECK-DAG:     <<Nextafter:d\d+>> InvokeStaticOrDirect [<<ConstMax>>,<<ConstInf>>] intrinsic:MathNextAfter
  /// CHECK-DAG:                        Return [<<Nextafter>>]

  /// CHECK-START: double Main.ReturnNexAfterMaxDouble() constant_folding (after)
  /// CHECK-DAG:     <<Const:d\d+>>     DoubleConstant inf
  /// CHECK-DAG:                        Return [<<Const>>]

  /// CHECK-START: double Main.ReturnNexAfterMaxDouble() constant_folding (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static double ReturnNexAfterMaxDouble() {
    double imm_start = Double.MAX_VALUE;
    double imm_direction = Double.POSITIVE_INFINITY;
    return Math.nextAfter(imm_start, imm_direction);
  }

  /// CHECK-START: double Main.ReturnSinhOne() constant_folding (before)
  /// CHECK-DAG:     <<Const:d\d+>>     DoubleConstant 1
  /// CHECK-DAG:     <<Sinh:d\d+>>      InvokeStaticOrDirect [<<Const>>] intrinsic:MathSinh
  /// CHECK-DAG:                        Return [<<Sinh>>]

  // There are two DoubleConstant instructions after constant folding:
  // the first one is the `1` value passed to the Math.sinh function,
  // while the second one is the result of the static evaluation of
  // `sinh(1)` operation.  To make sure `<<Const>>` matches the latter,
  // try to match the Return instruction first.

  /// CHECK-START: double Main.ReturnSinhOne() constant_folding (after)
  /// CHECK-DAG:                        Return [<<Const:d\d+>>]
  /// CHECK-DAG:     <<Const>>          DoubleConstant <<One:\d+(\.\d*)?>>
  /// CHECK-EVAL:    abs(<<One>> - 1.175201194) < 0.00001

  /// CHECK-START: double Main.ReturnSinhOne() constant_folding (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static double ReturnSinhOne() {
    double imm = 1;
    return Math.sinh(imm);
  }

  /// CHECK-START: double Main.ReturnTanPiDiv4() constant_folding (before)
  /// CHECK-DAG:     <<Const:d\d+>>     DoubleConstant <<PiDiv4:\d+(\.\d*)?>>
  /// CHECK-EVAL:    abs(<<PiDiv4>> - 0.785398163) < 0.00001
  /// CHECK-DAG:     <<Tan:d\d+>>       InvokeStaticOrDirect [<<Const>>] intrinsic:MathTan
  /// CHECK-DAG:                        Return [<<Tan>>]

  // There are two DoubleConstant instructions after constant folding:
  // the first one is the `pi / 4` value passed to the Math.tan
  // function, while the second one is the result of the static
  // evaluation of `tan(pi / 4)` operation (`1`).  To make
  // sure `<<Const>>` matches the latter, try to match the Return
  // instruction first.

  /// CHECK-START: double Main.ReturnTanPiDiv4() constant_folding (after)
  /// CHECK-DAG:                        Return [<<Const:d\d+>>]
  /// CHECK-DAG:     <<Const>>          DoubleConstant <<One:\d+(\.\d*)?>>
  /// CHECK-EVAL:    abs(<<One>> - 1) < 0.00001

  /// CHECK-START: double Main.ReturnTanPiDiv4() constant_folding (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static double ReturnTanPiDiv4() {
    double imm = Math.PI / 4;
    return Math.tan(imm);
  }

  /// CHECK-START: double Main.ReturnTanhOne() constant_folding (before)
  /// CHECK-DAG:     <<Const:d\d+>>     DoubleConstant 1
  /// CHECK-DAG:     <<Tanh:d\d+>>      InvokeStaticOrDirect [<<Const>>] intrinsic:MathTanh
  /// CHECK-DAG:                        Return [<<Tanh>>]

  // There are two DoubleConstant instructions after constant folding:
  // the first one is the `1` value passed to the Math.tanh function,
  // while the second one is the result of the static evaluation of
  // `tanh(1)` operation.  To make sure `<<Const>>` matches the latter,
  // try to match the Return instruction first.

  /// CHECK-START: double Main.ReturnTanhOne() constant_folding (after)
  /// CHECK-DAG:                        Return [<<Const:d\d+>>]
  /// CHECK-DAG:     <<Const>>          DoubleConstant <<One:\d+(\.\d*)?>>
  /// CHECK-EVAL:    abs(<<One>> - 0.761594156) < 0.00001

  /// CHECK-START: double Main.ReturnTanhOne() constant_folding (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static double ReturnTanhOne() {
    double imm = 1;
    return Math.tanh(imm);
  }

  /// CHECK-START: double Main.ReturnSqrtNine() constant_folding (before)
  /// CHECK-DAG:     <<Const:d\d+>>     DoubleConstant 9
  /// CHECK-DAG:     <<Sqrt:d\d+>>      InvokeStaticOrDirect [<<Const>>] intrinsic:MathSqrt
  /// CHECK-DAG:                        Return [<<Sqrt>>]

  /// CHECK-START: double Main.ReturnSqrtNine() constant_folding (after)
  /// CHECK-DAG:     <<Const:d\d+>>     DoubleConstant 3
  /// CHECK-DAG:                        Return [<<Const>>]

  /// CHECK-START: double Main.ReturnSqrtNine() constant_folding (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static double ReturnSqrtNine() {
    double imm = 9;
    return Math.sqrt(imm);
  }

  /// CHECK-START: double Main.ReturnCeil10P5() constant_folding (before)
  /// CHECK-DAG:     <<Const:d\d+>>     DoubleConstant 10.5
  /// CHECK-DAG:     <<Ceil:d\d+>>      InvokeStaticOrDirect [<<Const>>] intrinsic:MathCeil
  /// CHECK-DAG:                        Return [<<Ceil>>]

  /// CHECK-START: double Main.ReturnCeil10P5() constant_folding (after)
  /// CHECK-DAG:     <<Const:d\d+>>     DoubleConstant 11
  /// CHECK-DAG:                        Return [<<Const>>]

  /// CHECK-START: double Main.ReturnCeil10P5() constant_folding (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static double ReturnCeil10P5() {
    double imm = 10.5;
    return Math.ceil(imm);
  }

  /// CHECK-START: double Main.ReturnFloor12P5() constant_folding (before)
  /// CHECK-DAG:     <<Const:d\d+>>     DoubleConstant 12.5
  /// CHECK-DAG:     <<Floor:d\d+>>     InvokeStaticOrDirect [<<Const>>] intrinsic:MathFloor
  /// CHECK-DAG:                        Return [<<Floor>>]

  /// CHECK-START: double Main.ReturnFloor12P5() constant_folding (after)
  /// CHECK-DAG:     <<Const:d\d+>>     DoubleConstant 12
  /// CHECK-DAG:                        Return [<<Const>>]

  /// CHECK-START: double Main.ReturnFloor12P5() constant_folding (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static double ReturnFloor12P5() {
    double imm = 12.5;
    return Math.floor(imm);
  }

  /// CHECK-START: double Main.ReturnRintM3P2() constant_folding (before)
  /// CHECK-DAG:     <<Const:d\d+>>     DoubleConstant -3.2
  /// CHECK-DAG:     <<Rint:d\d+>>      InvokeStaticOrDirect [<<Const>>] intrinsic:MathRint
  /// CHECK-DAG:                        Return [<<Rint>>]

  /// CHECK-START: double Main.ReturnRintM3P2() constant_folding (after)
  /// CHECK-DAG:     <<Const:d\d+>>     DoubleConstant -3
  /// CHECK-DAG:                        Return [<<Const>>]

  /// CHECK-START: double Main.ReturnRintM3P2() constant_folding (after)
  /// CHECK-NOT:                        Invoke{{.*}}

  public static double ReturnRintM3P2() {
    double imm = -3.2;
    return Math.rint(imm);
  }


  public static void main(String[] args) throws Exception {
    assertDoubleEquals(1.5D, ReturnAbsDoubleM1P5());
    assertFloatEquals(2.5F, ReturnAbsFloatM2P5());
    assertLongEquals(12345678901L, ReturnAbsLongM12345678901());
    assertIntEquals(100, ReturnAbsIntM100());
    assertIntEquals(49, ReturnAbsShortM49());
    assertIntEquals(42, ReturnAbsByteM42());
    assertIntEquals(51, ReturnAbsChar51());

    assertDoubleEquals(-3.5D, ReturnMinDoubleDoubleM3P5_4P5());
    assertFloatEquals(-5.5F, ReturnMinFloatFloatM5P5_6P5());
    assertLongEquals(-34567890123L, ReturnMinLongLong23456789012_M34567890123());
    assertIntEquals(-321, ReturnMinIntInt123_M321());
    assertIntEquals(-49, ReturnMinShortShortM49_52());
    assertIntEquals(-42, ReturnMinByteByteM42_63());
    assertIntEquals(32, ReturnMinCharChar51_32());

    assertDoubleEquals(4.5D, ReturnMaxDoubleDoubleM3P5_4P5());
    assertFloatEquals(6.5F, ReturnMaxFloatFloatM5P5_6P5());
    assertLongEquals(23456789012L, ReturnMaxLongLong23456789012_M34567890123());
    assertIntEquals(123, ReturnMaxIntInt123_M321());
    assertIntEquals(52, ReturnMaxShortShortM49_52());
    assertIntEquals(63, ReturnMaxByteByteM42_63());
    assertIntEquals(51, ReturnMaxCharChar51_32());

    assertApproxDoubleEquals(Math.sqrt(2) / 2, ReturnCosPiDiv4(), 0.00001D);
    assertApproxDoubleEquals(Math.sqrt(2) / 2, ReturnSinPiDiv4(), 0.00001D);
    assertApproxDoubleEquals(Math.PI / 4, ReturnAcosSqrt2Div2(), 0.00001D);
    assertApproxDoubleEquals(Math.PI / 4, ReturnAsinSqrt2Div2(), 0.00001D);
    assertApproxDoubleEquals(Math.PI / 4, ReturnAtanOne(), 0.00001D);
    assertApproxDoubleEquals(-Math.PI / 4, ReturnAtan2MinusOneDivOne(), 0.00001D);
    assertDoubleEquals(2D, ReturnCbrthEight());
    assertApproxDoubleEquals(1.543080635D, ReturnCoshOne(), 0.00001D);
    assertApproxDoubleEquals(Math.E, ReturnExpOne(), 0.00001D);
    assertApproxDoubleEquals(Math.E - 1, ReturnExpm1One(), 0.00001D);
    assertDoubleEquals(5D, ReturnHypot3_4());
    assertDoubleEquals(0D, ReturnLogOne());
    assertDoubleEquals(2D, ReturnLog10OneHundred());
    assertDoubleEquals(Double.POSITIVE_INFINITY, ReturnNexAfterMaxDouble());
    assertApproxDoubleEquals(1.175201194D, ReturnSinhOne(), 0.00001D);
    assertApproxDoubleEquals(1D, ReturnTanPiDiv4(), 0.00001D);
    assertApproxDoubleEquals(0.761594156D, ReturnTanhOne(), 0.00001D);
    assertDoubleEquals(3D, ReturnSqrtNine());
    assertDoubleEquals(11D, ReturnCeil10P5());
    assertDoubleEquals(12, ReturnFloor12P5());
    assertDoubleEquals(-3, ReturnRintM3P2());
  }
}
