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

    public static final boolean DEBUG = false;

    public static void main(String[] args) {
        if (TestFmod() && TestFmodf()) {
            System.out.println("Pass");
        } else {
            System.out.println("Fail");
        }
    }

    // To ensure this method will be invoked, use the $noinline$ tag.
    // Use a long argument list to ensure they are passed to simulator correctly.
    private static final float $noinline$simulate$Fmodf(float x, float y, float i, float j, float k,
        float m, float n, float p, float q) {
        if (x > 2 * y) {
            return $noinline$simulate$Fmodf(x - y, y, i, j, k, m, n, p, q);
        } else if (x < - 2 * y) {
            return $noinline$simulate$Fmodf(x + y, y, i, j, k, m, n, p, q);
        }
        return x % y;
    }

    // To ensure this method will be invoked, use the $noinline$ tag.
    // Use a long argument list to ensure they are passed to simulator correctly.
    private static final double $noinline$simulate$Fmod(double x, double y, long i, long j, long k,
        long m, long n, long p, long q) {
        if (x > 2 * y) {
            return $noinline$simulate$Fmod(x - y, y, i, j, k, m, n, p, q);
        } else if (x < - 2 * y) {
            return $noinline$simulate$Fmod(x + y, y, i, j, k, m, n, p, q);
        }
        return x % y;
    }

    private static boolean TestFmod() {
        Main main = new Main();
        double x = 3.1415926;
        double y = 6.2951413;
        double z = x % y;
        double err = z - $noinline$simulate$Fmod(x, y, 1l, 2l, 3l, 4l, 5l, 6l, 7l);
        if (err > -1e-5 && err < 1e-5) {
            return true;
        }
        if (DEBUG) {
            System.out.printf("double error: %f\n", err);
        }
        return false;
    }

    private static boolean TestFmodf() {
        Main main = new Main();
        float x = 3.1415926f;
        float y = 6.2951413f;
        float z = x % y;
        float err = z - $noinline$simulate$Fmodf(x, y, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f);
        if (err > -1e-5 && err < 1e-5) {
            return true;
        }
        if (DEBUG) {
            System.out.printf("float error: %f\n", err);
        }
        return false;
    }
}
