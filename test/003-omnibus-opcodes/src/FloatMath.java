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
public class FloatMath {

    static boolean doThrow = false;

    static void $noinline$ConvTest() {
        System.out.println("FloatMath.$noinline$ConvTest");

        if (doThrow) { throw new Error(); }  // Try defeating inlining.

        float f;
        double d;
        int i;
        long l;

        /* float --> int */
        f = 1234.5678f;
        i = (int) f;
        Main.assertTrue(i == 1234);

        f = -1234.5678f;
        i = (int) f;
        Main.assertTrue(i == -1234);

        /* float --> long */
        f = 1238.5678f;
        l = (long) f;
        Main.assertTrue(l == 1238);

        f = -1238.5678f;
        l = (long) f;
        Main.assertTrue(l == -1238);

        /* float --> double */
        f = 1238.5678f;
        d = (double) f;
        Main.assertTrue(d > 1238.567 && d < 1238.568);

        /* double --> int */
        d = 1234.5678;
        i = (int) d;
        Main.assertTrue(i == 1234);

        d = -1234.5678;
        i = (int) d;
        Main.assertTrue(i == -1234);

        /* double --> long */
        d = 5678956789.0123;
        l = (long) d;
        Main.assertTrue(l == 5678956789L);

        d = -5678956789.0123;
        l = (long) d;
        Main.assertTrue(l == -5678956789L);

        /* double --> float */
        d = 1238.5678;
        f = (float) d;
        Main.assertTrue(f > 1238.567 && f < 1238.568);

        /* int --> long */
        i = 7654;
        l = (long) i;
        Main.assertTrue(l == 7654L);

        i = -7654;
        l = (long) i;
        Main.assertTrue(l == -7654L);

        /* int --> float */
        i = 1234;
        f = (float) i;
        Main.assertTrue(f > 1233.9f && f < 1234.1f);

        i = -1234;
        f = (float) i;
        Main.assertTrue(f < -1233.9f && f > -1234.1f);

        /* int --> double */
        i = 1238;
        d = (double) i;
        Main.assertTrue(d > 1237.9f && d < 1238.1f);

        i = -1238;
        d = (double) i;
        Main.assertTrue(d < -1237.9f && d > -1238.1f);

        /* long --> int (with truncation) */
        l = 5678956789L;
        i = (int) l;
        Main.assertTrue(i == 1383989493);

        l = -5678956789L;
        i = (int) l;
        Main.assertTrue(i == -1383989493);

        /* long --> float */
        l = 5678956789L;
        f = (float) l;
        Main.assertTrue(f > 5.6789564E9 && f < 5.6789566E9);

        l = -5678956789L;
        f = (float) l;
        Main.assertTrue(f < -5.6789564E9 && f > -5.6789566E9);

        /* long --> double */
        l = 6678956789L;
        d = (double) l;
        Main.assertTrue(d > 6.6789567E9 && d < 6.6789568E9);

        l = -6678956789L;
        d = (double) l;
        Main.assertTrue(d < -6.6789567E9 && d > -6.6789568E9);
    }

    /*
     * We pass in the arguments and return the results so the compiler
     * doesn't do the math for us.
     */
    static float[] $noinline$FloatOperTest(float x, float y) {
        System.out.println("FloatMath.$noinline$FloatOperTest");

        if (doThrow) { throw new Error(); }  // Try defeating inlining.

        float[] results = new float[9];

        /* this seems to generate "op-float" instructions */
        results[0] = x + y;
        results[1] = x - y;
        results[2] = x * y;
        results[3] = x / y;
        results[4] = x % -y;

        /* this seems to generate "op-float/2addr" instructions */
        results[8] = x + (((((x + y) - y) * y) / y) % y);

        return results;
    }
    static void floatOperCheck(float[] results) {
        Main.assertTrue(results[0] > 69996.99f && results[0] < 69997.01f);
        Main.assertTrue(results[1] > 70002.99f && results[1] < 70003.01f);
        Main.assertTrue(results[2] > -210000.01f && results[2] < -209999.99f);
        Main.assertTrue(results[3] > -23333.34f && results[3] < -23333.32f);
        Main.assertTrue(results[4] > 0.999f && results[4] < 1.001f);
        Main.assertTrue(results[8] > 70000.99f && results[8] < 70001.01f);
    }

    /*
     * We pass in the arguments and return the results so the compiler
     * doesn't do the math for us.
     */
    static double[] $noinline$DoubleOperTest(double x, double y) {
        System.out.println("FloatMath.$noinline$DoubleOperTest");

        if (doThrow) { throw new Error(); }  // Try defeating inlining.

        double[] results = new double[9];

        /* this seems to generate "op-double" instructions */
        results[0] = x + y;
        results[1] = x - y;
        results[2] = x * y;
        results[3] = x / y;
        results[4] = x % -y;

        /* this seems to generate "op-double/2addr" instructions */
        results[8] = x + (((((x + y) - y) * y) / y) % y);

        return results;
    }
    static void doubleOperCheck(double[] results) {
        Main.assertTrue(results[0] > 69996.99 && results[0] < 69997.01);
        Main.assertTrue(results[1] > 70002.99 && results[1] < 70003.01);
        Main.assertTrue(results[2] > -210000.01 && results[2] < -209999.99);
        Main.assertTrue(results[3] > -23333.34 && results[3] < -23333.32);
        Main.assertTrue(results[4] > 0.999 && results[4] < 1.001);
        Main.assertTrue(results[8] > 70000.99 && results[8] < 70001.01);
    }

    /*
     * Try to cause some unary operations.
     */
    static float $noinline$UnopTest(float f) {
        if (doThrow) { throw new Error(); }  // Try defeating inlining.
        f = -f;
        return f;
    }

    static int[] $noinline$ConvI(long l, float f, double d, float zero) {
        if (doThrow) { throw new Error(); }  // Try defeating inlining.
        int[] results = new int[6];
        results[0] = (int) l;
        results[1] = (int) f;
        results[2] = (int) d;
        results[3] = (int) (1.0f / zero);       // +inf
        results[4] = (int) (-1.0f / zero);      // -inf
        results[5] = (int) ((1.0f / zero) / (1.0f / zero)); // NaN
        return results;
    }
    static void checkConvI(int[] results) {
        System.out.println("FloatMath.checkConvI");
        Main.assertTrue(results[0] == 0x44332211);
        Main.assertTrue(results[1] == 123);
        Main.assertTrue(results[2] == -3);
        Main.assertTrue(results[3] == 0x7fffffff);
        Main.assertTrue(results[4] == 0x80000000);
        Main.assertTrue(results[5] == 0);
    }

    static long[] $noinline$ConvL(int i, float f, double d, double zero) {
        if (doThrow) { throw new Error(); }  // Try defeating inlining.
        long[] results = new long[6];
        results[0] = (long) i;
        results[1] = (long) f;
        results[2] = (long) d;
        results[3] = (long) (1.0 / zero);       // +inf
        results[4] = (long) (-1.0 / zero);      // -inf
        results[5] = (long) ((1.0 / zero) / (1.0 / zero));  // NaN
        return results;
    }
    static void checkConvL(long[] results) {
        System.out.println("FloatMath.checkConvL");
        Main.assertTrue(results[0] == 0xFFFFFFFF88776655L);
        Main.assertTrue(results[1] == 123);
        Main.assertTrue(results[2] == -3);
        Main.assertTrue(results[3] == 0x7fffffffffffffffL);
        Main.assertTrue(results[4] == 0x8000000000000000L);
        Main.assertTrue(results[5] == 0);
    }

    static float[] $noinline$ConvF(int i, long l, double d) {
        if (doThrow) { throw new Error(); }  // Try defeating inlining.
        float[] results = new float[3];
        results[0] = (float) i;
        results[1] = (float) l;
        results[2] = (float) d;
        return results;
    }
    static void checkConvF(float[] results) {
        System.out.println("FloatMath.checkConvF");
        // TODO: Main.assertTrue values
        for (int i = 0; i < results.length; i++)
            System.out.println(" " + i + ": " + results[i]);
        System.out.println("-2.0054409E9, -8.6133031E18, -3.1415927");
    }

    static double[] $noinline$ConvD(int i, long l, float f) {
        if (doThrow) { throw new Error(); }  // Try defeating inlining.
        double[] results = new double[3];
        results[0] = (double) i;
        results[1] = (double) l;
        results[2] = (double) f;
        return results;
    }
    static void checkConvD(double[] results) {
        System.out.println("FloatMath.checkConvD");
        // TODO: Main.assertTrue values
        for (int i = 0; i < results.length; i++)
            System.out.println(" " + i + ": " + results[i]);
        System.out.println("-2.005440939E9, -8.6133032459203287E18, 123.4560012817382");
    }

    static void $noinline$CheckConsts() {
        System.out.println("FloatMath.$noinline$CheckConsts");

        if (doThrow) { throw new Error(); }  // Try defeating inlining.

        float f = 10.0f;        // const/special
        Main.assertTrue(f > 9.9 && f < 10.1);

        double d = 10.0;        // const-wide/special
        Main.assertTrue(d > 9.9 && d < 10.1);
    }

    /*
     * Determine if two floating point numbers are approximately equal.
     *
     * (Assumes that floating point is generally working, so we can't use
     * this for the first set of tests.)
     */
    static boolean approxEqual(float a, float b, float maxDelta) {
        if (a > b)
            return (a - b) < maxDelta;
        else
            return (b - a) < maxDelta;
    }
    static boolean approxEqual(double a, double b, double maxDelta) {
        if (a > b)
            return (a - b) < maxDelta;
        else
            return (b - a) < maxDelta;
    }

    /*
     * Test some java.lang.Math functions.
     *
     * The method arguments are positive values.
     */
    static void jlmTests(float ff, double dd) {
        System.out.println("FloatMath.jlmTests");

        Main.assertTrue(approxEqual(Math.abs(ff), ff, 0.001f));
        Main.assertTrue(approxEqual(Math.abs(-ff), ff, 0.001f));
        Main.assertTrue(approxEqual(Math.min(ff, -5.0f), -5.0f, 0.001f));
        Main.assertTrue(approxEqual(Math.max(ff, -5.0f), ff, 0.001f));

        Main.assertTrue(approxEqual(Math.abs(dd), dd, 0.001));
        Main.assertTrue(approxEqual(Math.abs(-dd), dd, 0.001));
        Main.assertTrue(approxEqual(Math.min(dd, -5.0), -5.0, 0.001));
        Main.assertTrue(approxEqual(Math.max(dd, -5.0), dd, 0.001));

        double sq = Math.sqrt(dd);
        Main.assertTrue(approxEqual(sq*sq, dd, 0.001));

        Main.assertTrue(approxEqual(0.5403023058681398, Math.cos(1.0), 0.00000001));
        Main.assertTrue(approxEqual(0.8414709848078965, Math.sin(1.0), 0.00000001));
    }

    public static void run() {
        $noinline$ConvTest();

        float[] floatResults;
        double[] doubleResults;
        int[] intResults;
        long[] longResults;

        floatResults = $noinline$FloatOperTest(70000.0f, -3.0f);
        floatOperCheck(floatResults);
        doubleResults = $noinline$DoubleOperTest(70000.0, -3.0);
        doubleOperCheck(doubleResults);

        intResults = $noinline$ConvI(0x8877665544332211L, 123.456f, -3.1415926535, 0.0f);
        checkConvI(intResults);
        longResults = $noinline$ConvL(0x88776655, 123.456f, -3.1415926535, 0.0);
        checkConvL(longResults);
        floatResults = $noinline$ConvF(0x88776655, 0x8877665544332211L, -3.1415926535);
        checkConvF(floatResults);
        doubleResults = $noinline$ConvD(0x88776655, 0x8877665544332211L, 123.456f);
        checkConvD(doubleResults);

        $noinline$UnopTest(123.456f);

        $noinline$CheckConsts();

        jlmTests(3.14159f, 123456.78987654321);
    }
}
