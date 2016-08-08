/*
 * Copyright (C) 2013 The Android Open Source Project
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
import java.lang.String;

public class Main {
    public static void main(String[] args) {
        //System.loadLibrary("arttest");
        testByteMethod();
        testShortMethod();
        testBooleanMethod();
        testCharMethod();
        testIntMethod();
        testLongMethod();
        testByteShortMethod();
        testByteIntMethod();
        testByteLongMethod();
        testCharIntMethod();
        testCharLongMethod();
        testIntLongMethod();
        testIntStringMethod();
    }
   
    // Test sign-extension for values < 32b

    native static byte byteMethod(byte b1, byte b2, byte b3, byte b4, byte b5, byte b6, byte b7,
        byte b8, byte b9, byte b10);

    private static void testByteMethod() {
      byte returns[] = { 0, 1, 2, 127, -1, -2, -128 };
      byte result = byteMethod((byte)0, (byte)2, (byte)(-3), (byte)4, (byte)(-5), (byte)6,
            (byte)(-7), (byte)8, (byte)(-9), (byte)10);
      if (returns[0] != result) {
          System.out.println("Run with " + returns[0] + " vs " + result);
          throw new AssertionError();
      }
    }

    native static short shortMethod(short s1, short s2, short s3, short s4, short s5, short s6, short s7,
        short s8, short s9, short s10);

    private static void testShortMethod() {
      short returns[] = { 0, 1, 2, 127, 32767, -1, -2, -128, -32768 };
        short result = shortMethod((short)0, (short)2, (short)(-3), (short)4, (short)(-5), (short)6,
            (short)(-7), (short)8, (short)(-9), (short)10);
        if (returns[0] != result) {
          System.out.println("Run with " + returns[0] + " vs " + result);
          throw new AssertionError();
        }
    }

    // Test zero-extension for values < 32b

    native static boolean booleanMethod(boolean b1, boolean b2, boolean b3, boolean b4, boolean b5, boolean b6, boolean b7,
        boolean b8, boolean b9, boolean b10);

    private static void testBooleanMethod() {
      if (booleanMethod(false, true, false, true, false, true, false, true, false, true)) {
        throw new AssertionError();
      }

      if (!booleanMethod(true, true, false, true, false, true, false, true, false, true)) {
        throw new AssertionError();
      }
    }

    native static char charMethod(char c1, char c2, char c3, char c4, char c5, char c6, char c7,
        char c8, char c9, char c10);

    private static void testCharMethod() {
      char returns[] = { (char)0, (char)1, (char)2, (char)127, (char)255, (char)256, (char)15000,
          (char)34000 };
        char result = charMethod((char)0, 'a', 'b', 'c', '0', '1', '2', (char)1234, (char)2345,
            (char)3456);
        if (returns[0] != result) {
          System.out.println("Run with " + (int)returns[0] + " vs " + (int)result);
          throw new AssertionError();
        }
    }
    
    native static int intMethod(int i1,  int i2, int i3, int i4, int i5, int i6, int i7, int i8, int i9, int i10);
    
    private static void testIntMethod() {
        int ret = 0;
        int result = intMethod((int)0, (int)2, (int)(-3), (int)4, (int)(-5), (int)6,
                                 (int) (-7), (int)8, (int)(-30000), (int)30000);
        if (ret != result) {
            System.out.println("Run with " + ret + " vs " + result);
            throw new AssertionError();
        }
    }
    
    native static long longMethod(long l1, long l2, long l3, long l4, long l5, long l6, long l7, long l8, long l9, long l10);
    
    private static void testLongMethod() {
        long ret = 0;
        long result = longMethod((long)0, (long)2, (long)(-3), (long)4, (long)(-5), (long)6,
                                (long) (-7), (long)8, (long)(-100000), (long)100000);
        if (ret != result) {
            System.out.println("Run with " + ret + " vs " + result);
            throw new AssertionError();
        }
    }
    
    native static byte byteshortMethod(byte b1,  byte b2, byte b3, byte b4, byte b5, byte b6, byte b7, short s1, byte b8, short s2);
    
    private static void testByteShortMethod() {
        byte ret = 0;
        byte result = byteshortMethod((byte)0, (byte)2, (byte)(-3), (byte)4, (byte)(-5), (byte)6,
                                 (byte) (-7), (short)8, (byte)(-9), (short)10);
        if (ret != result) {
            System.out.println("Run with " + ret + " vs " + result);
            throw new AssertionError();
        }
    }
    
    native static byte byteintMethod(byte b1,  byte b2, byte b3, byte b4, byte b5, byte b6, byte b7, int i1, byte b8, int i2);
    
    private static void testByteIntMethod() {
        byte ret = 0;
        byte result = byteintMethod((byte)0, (byte)2, (byte)(-3), (byte)4, (byte)(-5), (byte)6,
                                      (byte) (-7), (int)8, (byte)(-9), (int)30000);
        if (ret != result) {
            System.out.println("Run with " + ret + " vs " + result);
            throw new AssertionError();
        }
    }
    
    native static byte bytelongMethod(byte b1,  byte b2, byte b3, byte b4, byte b5, byte b6, byte b7, long l1, byte b8, long l2);
    
    private static void testByteLongMethod() {
        byte ret = 0;
        byte result = bytelongMethod((byte)0, (byte)2, (byte)(-3), (byte)4, (byte)(-5), (byte)6,
                                      (byte) (-7), (long)8, (byte)(-9), (long)100000);
        if (ret != result) {
            System.out.println("Run with " + ret + " vs " + result);
            throw new AssertionError();
        }
    }
    
    native static char charintMethod(char c1,  char c2, char c3, char c4, char c5, char c6, char c7, int i1, char c8, int i2);
    
    private static void testCharIntMethod() {
        char ret = 0;
        char result = charintMethod((char)0, 'a', 'b', 'c', 'd', 'e', 'f', (int)8, 'g',
                                 (int)30000);
        if (ret != result) {
            System.out.println("Run with " + (int)ret + " vs " + (int)result);
            throw new AssertionError();
        }
    }
    
    native static char charlongMethod(char c1,  char c2, char c3, char c4, char c5, char c6, char c7, long l1, char c8, long l2);
    
    private static void testCharLongMethod() {
        char ret = 0;
        char result = charlongMethod((char)0, 'a', 'b', 'c', 'd', 'e', 'f', (long)8, 'g',
                                    (long)100000);
        if (ret != result) {
            System.out.println("Run with " + (int)ret + " vs " + (int)result);
            throw new AssertionError();
        }
    }
    
    native static int intlongMethod(int i1,  int i2, int i3, int i4, int i5, int i6, int i7, long l1, int i8, long l2);
    
    private static void testIntLongMethod() {
        int ret = 0;
        int result = intlongMethod((int)0, (int)2, (int)(-3), (int)4, (int)(-5), (int)6,
                               (int) (-7), (long)8, (int)(-30000), (long)100000);
        if (ret != result) {
            System.out.println("Run with " + ret + " vs " + result);
            throw new AssertionError();
        }
    }
    
    native static int intstringMethod(int i1, int i2, int i3, int i4, int i5, int i6, int i7, String s1,
                                      int i8, String s2);
    
    private static void testIntStringMethod() {
        int ret = 0;
        int result = intstringMethod((int)0, (int)2, (int)(-3), (int)4, (int)(-5), (int)6,
                                   (int) (-7), "abc", (int)(-30000), "def");
        if (ret != result) {
            System.out.println("Run with " + ret + " vs " + result);
            throw new AssertionError();
        }
    }
}