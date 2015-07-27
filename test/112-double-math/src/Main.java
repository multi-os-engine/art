/*
 * Copyright (C) 2007 The Android Open Source Project
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
    public static double cond_neg_double(double value, boolean cond) {
        return cond ? -value : value;
    }

    public static void main(String args[]) {
        double result = cond_neg_double(-1.0d, true);

        if (Double.doubleToRawLongBits(result) == 0x3ff0000000000000L) {
            System.out.println("cond_neg_double PASSED");
        } else {
            System.out.println("cond_neg_double FAILED " + result);
        }

        double d = Double.longBitsToDouble(4638648202453526795L /* approximately 127.15568 */);
        long result_l = Double.doubleToRawLongBits(Math.sqrt(d));
        if (result_l == 4622537629043927147L) {
            System.out.println("sqrt PASSED");
        } else {
            System.out.println("sqrt FAILED " + result_l);
        }
    }
}
