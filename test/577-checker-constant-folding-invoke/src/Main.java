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

  public static short GetShortM49() {
    return (short) -49;
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

  public static byte GetByteM42() {
    return (byte) -42;
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

  public static char GetChar51() {
    return (char) 51;
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

  // TODO: Exercise constant folding of MathMinIntInt with short values.
  // TODO: Exercise constant folding of MathMinIntInt with byte values.
  // TODO: Exercise constant folding of MathMinIntInt with char values.

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

  // TODO: Exercise constant folding of MathMaxIntInt with short values.
  // TODO: Exercise constant folding of MathMaxIntInt with byte values.
  // TODO: Exercise constant folding of MathMaxIntInt with char values.


  public static void main(String[] args) throws Exception {
    assertLongEquals(12345678901L, ReturnAbsLongM12345678901());
    assertIntEquals(100, ReturnAbsIntM100());
    assertIntEquals(49, ReturnAbsShortM49());
    assertIntEquals(42, ReturnAbsByteM42());
    assertIntEquals(51, ReturnAbsChar51());

    assertLongEquals(-34567890123L, ReturnMinLongLong23456789012_M34567890123());
    assertIntEquals(-321, ReturnMinIntInt123_M321());

    assertLongEquals(23456789012L, ReturnMaxLongLong23456789012_M34567890123());
    assertIntEquals(123, ReturnMaxIntInt123_M321());
  }
}
