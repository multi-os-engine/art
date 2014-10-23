/*
 * Copyright (C) 2006 The Android Open Source Project
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

/**
 * Test arithmetic operations.
 */
public class Main {

    static void shiftTest1()
    {
        final int[] mBytes = {
            0x11, 0x22, 0x33, 0x44, 0x88, 0x99, 0xaa, 0xbb
        };
        long l;
        int i1, i2;

        i1 = mBytes[0] | mBytes[1] << 8 | mBytes[2] << 16 | mBytes[3] << 24;
        i2 = mBytes[4] | mBytes[5] << 8 | mBytes[6] << 16 | mBytes[7] << 24;
        l = i1 | ((long)i2 << 32);

	System.out.println("values are " + Integer.toHexString(i1)
	    + " and " + Integer.toHexString(i2));

        System.out.println("First l is " + Long.toHexString(l));

        l = (long)mBytes[0]
            | (long)mBytes[1] << 8
            | (long)mBytes[2] << 16
            | (long)mBytes[3] << 24
            | (long)mBytes[4] << 32
            | (long)mBytes[5] << 40
            | (long)mBytes[6] << 48
            | (long)mBytes[7] << 56;

        System.out.println("Second l is " + Long.toHexString(l));
    }

    static void shiftTest2()
    {
        long    a = 0x11;
        long    b = 0x22;
        long    c = 0x33;
        long    d = 0x44;
        long    e = 0x55;
        long    f = 0x66;
        long    g = 0x77;
        long    h = 0x88;

        long    result = ((a << 56) | (b << 48) | (c << 40) | (d << 32) |
                         (e << 24) | (f << 16) | (g << 8) | h);

        System.out.println("shiftTest2 l is " + Long.toHexString(result));
    }

    static void convTest()
    {
        float f;
        double d;
        int i;
        long l;

        /* float --> int */
        f = 1234.5678f;
        i = (int) f;
        System.out.println("f=" + f + " --> i=" + i);

        f = -1234.5678f;
        i = (int) f;
        System.out.println("f=" + f + " --> i=" + i);

        /* double --> int */
        d = 1234.5678;
        i = (int) d;
        System.out.println("d=" + d + " --> i=" + i);

        d = -1234.5678;
        i = (int) d;
        System.out.println("d=" + d + " --> i=" + i);

        /* double --> long */
        d = 5678956789.0123;
        l = (long) d;
        System.out.println("d=" + d + " --> l=" + l);

        d = -5678956789.0123;
        l = (long) d;
        System.out.println("d=" + d + " --> l=" + l);

        /* int --> long */
        i = 7654;
        l = (long) i;
        System.out.println("i=" + i + " --> l=" + l);

        i = -7654;
        l = (long) i;
        System.out.println("i=" + i + " --> l=" + l);

        /* long --> int (with truncation) */
        l = 5678956789L;
        i = (int) l;
        System.out.println("l=" + l + " --> i=" + i);

        l = -5678956789L;
        i = (int) l;
        System.out.println("l=" + l + " --> i=" + i);

        /* int --> float */
        i = 1234;
        f = (float) i;
        System.out.println("i=" + i + " --> f=" + f);

        i = -1234;
        f = (float) i;
        System.out.println("i=" + i + " --> f=" + f);
    }

    static void unsignedShiftTest()
    {
        byte b = -4;
        short s = -4;
        char c = 0xfffc;
        int i = -4;

        b >>>= 4;
        s >>>= 4;
        c >>>= 4;
        i >>>= 4;

        System.out.println("b=" + b + ", s=" + s + ", c=" + (int)c + ", i=" +i);
        System.out.println("b=0x" + Integer.toHexString((int)b)
            + ", s=0x" + Integer.toHexString((int)s)
            + ", c=0x" + Integer.toHexString((int)c)
            + ", i=0x" + Integer.toHexString(i));
    }

    public static void expectEquals(long expected, long result) {
        if (expected != result) {
            throw new Error("Expected: " + expected + ", found: " + result);
        }
    }

    public static void expectEquals(int expected, int result) {
        if (expected != result) {
            throw new Error("Expected: " + expected + ", found: " + result);
        }
    }

    static void divDoubleTest() {
        double d1 = 0x1.0p1023;
        double d2 = -2.0;
        double d3 = 0.0;
        double d4 = Double.MIN_NORMAL;
        double d5 = Double.POSITIVE_INFINITY;
        double d6 = Double.NEGATIVE_INFINITY;
        double d7 = -0.0;
        double d8 = Double.MAX_VALUE;
        double d9 = Double.MIN_VALUE;
        double d0 = Double.NaN;
        double pi = Math.PI;

        expectEquals(Double.doubleToRawLongBits(pi/d1), 0x1921fb54442d18L);
        expectEquals(Double.doubleToRawLongBits(pi/d2), 0xbff921fb54442d18L);
        expectEquals(Double.doubleToRawLongBits(pi/d3), 0x7ff0000000000000L);
        expectEquals(Double.doubleToRawLongBits(pi/d4), 0x7fe921fb54442d18L);
        expectEquals(Double.doubleToRawLongBits(pi/d5), 0x0L);
        expectEquals(Double.doubleToRawLongBits(pi/d6), 0x8000000000000000L);
        expectEquals(Double.doubleToRawLongBits(pi/d7), 0xfff0000000000000L);

        expectEquals(Double.doubleToRawLongBits(pi/d8), 0xc90fdaa22168cL);
        expectEquals(Double.doubleToRawLongBits(pi/d9), 0x7ff0000000000000L);
        expectEquals(Double.doubleToRawLongBits(pi/d0), 0x7ff8000000000000L);
    }

    static void divFloatTest() {
        float f1 = 0x1.0p127f;
        float f2 = -2.0f;
        float f3 = 0.0f;
        float f4 = Float.MIN_NORMAL;
        float f5 = Float.POSITIVE_INFINITY;
        float f6 = Float.NEGATIVE_INFINITY;
        float f7 = -0.0f;
        float f8 = Float.MAX_VALUE;
        float f9 = Float.MIN_VALUE;
        float f0 = Float.NaN;
        float pi = (float)Math.PI;

        expectEquals(Float.floatToRawIntBits(pi/f1), 0xc90fdb);
        expectEquals(Float.floatToRawIntBits(pi/f2), 0xbfc90fdb);
        expectEquals(Float.floatToRawIntBits(pi/f3), 0x7f800000);
        expectEquals(Float.floatToRawIntBits(pi/f4), 0x7f490fdb);
        expectEquals(Float.floatToRawIntBits(pi/f5), 0x0);
        expectEquals(Float.floatToRawIntBits(pi/f6), 0x80000000);
        expectEquals(Float.floatToRawIntBits(pi/f7), 0xff800000);

        expectEquals(Float.floatToRawIntBits(pi/f8), 0x6487ee);
        expectEquals(Float.floatToRawIntBits(pi/f9), 0x7f800000);
        expectEquals(Float.floatToRawIntBits(pi/f0), 0x7fc00000);
    }

    public static void main(String[] args) {
        convTest();
        shiftTest1();
        shiftTest2();
        unsignedShiftTest();
        divDoubleTest();
        divFloatTest();
    }
}
