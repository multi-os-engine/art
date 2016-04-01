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

  // TODO: make something like this work when b/26700769 is done.
  // CHECK-START-X86_64: int Main.bits32(int) disassembly (after)
  // CHECK-DAG: popcnt


  /// CHECK-START: int Main.bitCountBoolean(boolean) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Result:i\d+>>  InvokeStaticOrDirect intrinsic:IntegerBitCount
  /// CHECK-DAG:                      Return [<<Result>>]
  private static int bitCountBoolean(boolean x) {
    return Integer.bitCount(x ? 1 : 0);
  }

  /// CHECK-START: int Main.bitCountByte(byte) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Result:i\d+>>  InvokeStaticOrDirect intrinsic:IntegerBitCount
  /// CHECK-DAG:                      Return [<<Result>>]
  private static int bitCountByte(byte x) {
    return Integer.bitCount(x);
  }

  /// CHECK-START: int Main.bitCountShort(short) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Result:i\d+>>  InvokeStaticOrDirect intrinsic:IntegerBitCount
  /// CHECK-DAG:                      Return [<<Result>>]
  private static int bitCountShort(short x) {
    return Integer.bitCount(x);
  }

  /// CHECK-START: int Main.bitCountChar(char) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Result:i\d+>>  InvokeStaticOrDirect intrinsic:IntegerBitCount
  /// CHECK-DAG:                      Return [<<Result>>]
  private static int bitCountChar(char x) {
    return Integer.bitCount(x);
  }

  /// CHECK-START: int Main.bitCountInt(int) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Result:i\d+>>  InvokeStaticOrDirect intrinsic:IntegerBitCount
  /// CHECK-DAG:                      Return [<<Result>>]
  private static int bitCountInt(int x) {
    return Integer.bitCount(x);
  }

  /// CHECK-START: int Main.bitCountLong(long) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Result:i\d+>>  InvokeStaticOrDirect intrinsic:LongBitCount
  /// CHECK-DAG:                      Return [<<Result>>]
  private static int bitCountLong(long x) {
    return Long.bitCount(x);
  }

  public static void testBitCountBoolean() {
    expectEqualsInt(bitCountBoolean(false), 0);
    expectEqualsInt(bitCountBoolean(true), 1);
  }

  public static void testBitCountByte() {
    // Number of bits in an 32-bit integer representing the sign
    // extension of a byte value widened to an int.
    int signExtensionSize = Integer.SIZE - Byte.SIZE;
    // Sign bit position in a byte.
    int signBit = Byte.SIZE - 1;

    expectEqualsInt(bitCountByte((byte) 0x00), 0);
    expectEqualsInt(bitCountByte((byte) 0x01), 1);
    expectEqualsInt(bitCountByte((byte) 0x10), 1);
    expectEqualsInt(bitCountByte((byte) 0x11), 2);
    expectEqualsInt(bitCountByte((byte) 0x03), 2);
    expectEqualsInt(bitCountByte((byte) 0x70), 3);
    expectEqualsInt(bitCountByte((byte) 0xF0), 4 + signExtensionSize);
    expectEqualsInt(bitCountByte((byte) 0x0F), 4);
    expectEqualsInt(bitCountByte((byte) 0x12), 2);
    expectEqualsInt(bitCountByte((byte) 0x9A), 4 + signExtensionSize);
    expectEqualsInt(bitCountByte((byte) 0xFF), 8 + signExtensionSize);

    for (int i = 0; i < Byte.SIZE; i++) {
      expectEqualsInt(bitCountByte((byte) (1 << i)),
                      (i < signBit) ? 1 : 1 + signExtensionSize);
    }
  }

  public static void testBitCountShort() {
    // Number of bits in an 32-bit integer representing the sign
    // extension of a short value widened to an int.
    int signExtensionSize = Integer.SIZE - Short.SIZE;
    // Sign bit position in a short.
    int signBit = Short.SIZE - 1;

    expectEqualsInt(bitCountShort((short) 0x0000), 0);
    expectEqualsInt(bitCountShort((short) 0x0001), 1);
    expectEqualsInt(bitCountShort((short) 0x1000), 1);
    expectEqualsInt(bitCountShort((short) 0x1001), 2);
    expectEqualsInt(bitCountShort((short) 0x0003), 2);
    expectEqualsInt(bitCountShort((short) 0x7000), 3);
    expectEqualsInt(bitCountShort((short) 0x0F00), 4);
    expectEqualsInt(bitCountShort((short) 0x0011), 2);
    expectEqualsInt(bitCountShort((short) 0x1100), 2);
    expectEqualsInt(bitCountShort((short) 0x1111), 4);
    expectEqualsInt(bitCountShort((short) 0x1234), 5);
    expectEqualsInt(bitCountShort((short) 0x9ABC), 9 + signExtensionSize);
    expectEqualsInt(bitCountShort((short) 0xFFFF), 16 + signExtensionSize);

    for (int i = 0; i < Short.SIZE; i++) {
      expectEqualsInt(bitCountShort((short) (1 << i)),
                      (i < signBit) ? 1 : 1 + signExtensionSize);
    }
  }

  public static void testBitCountChar() {
    expectEqualsInt(bitCountChar((char) 0x0000), 0);
    expectEqualsInt(bitCountChar((char) 0x0001), 1);
    expectEqualsInt(bitCountChar((char) 0x1000), 1);
    expectEqualsInt(bitCountChar((char) 0x1001), 2);
    expectEqualsInt(bitCountChar((char) 0x0003), 2);
    expectEqualsInt(bitCountChar((char) 0x7000), 3);
    expectEqualsInt(bitCountChar((char) 0x0F00), 4);
    expectEqualsInt(bitCountChar((char) 0x0011), 2);
    expectEqualsInt(bitCountChar((char) 0x1100), 2);
    expectEqualsInt(bitCountChar((char) 0x1111), 4);
    expectEqualsInt(bitCountChar((char) 0x1234), 5);
    expectEqualsInt(bitCountChar((char) 0x9ABC), 9);
    expectEqualsInt(bitCountChar((char) 0xFFFF), 16);

    for (int i = 0; i < Character.SIZE; i++) {
      expectEqualsInt(bitCountChar((char) (1 << i)), 1);
    }
  }

  public static void testBitCountInt() {
    expectEqualsInt(bitCountInt(0x00000000), 0);
    expectEqualsInt(bitCountInt(0x00000001), 1);
    expectEqualsInt(bitCountInt(0x10000000), 1);
    expectEqualsInt(bitCountInt(0x10000001), 2);
    expectEqualsInt(bitCountInt(0x00000003), 2);
    expectEqualsInt(bitCountInt(0x70000000), 3);
    expectEqualsInt(bitCountInt(0x000F0000), 4);
    expectEqualsInt(bitCountInt(0x00001111), 4);
    expectEqualsInt(bitCountInt(0x11110000), 4);
    expectEqualsInt(bitCountInt(0x11111111), 8);
    expectEqualsInt(bitCountInt(0x12345678), 13);
    expectEqualsInt(bitCountInt(0x9ABCDEF0), 19);
    expectEqualsInt(bitCountInt(0xFFFFFFFF), 32);

    for (int i = 0; i < Integer.SIZE; i++) {
      expectEqualsInt(bitCountInt(1 << i), 1);
    }
  }

  public static void testBitCountLong() {
    expectEqualsInt(bitCountLong(0x0000000000000000L), 0);
    expectEqualsInt(bitCountLong(0x0000000000000001L), 1);
    expectEqualsInt(bitCountLong(0x1000000000000000L), 1);
    expectEqualsInt(bitCountLong(0x1000000000000001L), 2);
    expectEqualsInt(bitCountLong(0x0000000000000003L), 2);
    expectEqualsInt(bitCountLong(0x7000000000000000L), 3);
    expectEqualsInt(bitCountLong(0x000F000000000000L), 4);
    expectEqualsInt(bitCountLong(0x0000000011111111L), 8);
    expectEqualsInt(bitCountLong(0x1111111100000000L), 8);
    expectEqualsInt(bitCountLong(0x1111111111111111L), 16);
    expectEqualsInt(bitCountLong(0x123456789ABCDEF1L), 33);
    expectEqualsInt(bitCountLong(0xFFFFFFFFFFFFFFFFL), 64);

    for (int i = 0; i < Long.SIZE; i++) {
      expectEqualsInt(bitCountLong(1L << i), 1);
    }
  }

  public static void main(String args[]) {
    testBitCountBoolean();
    testBitCountByte();
    testBitCountShort();
    testBitCountChar();
    testBitCountInt();
    testBitCountLong();

    System.out.println("passed");
  }

  private static void expectEqualsInt(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

}
