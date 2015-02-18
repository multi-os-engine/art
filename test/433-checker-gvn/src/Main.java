/*
 * Copyright (C) 2014 The Android Open Source Project
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
  public static void main(String[] args) {
    System.out.println(foo());
    if (calc1(9) != 9) {
      System.out.println("calc1 failed!");
    }
    if (calc2(2, 3) != 10) {
      System.out.println("calc2 failed!");
    }
  }

  public static int foo() {
    Main m = new Main();
    int a = m.field;
    if (a == 0) {
      m.field = 42;
      if (m.test) {
        a = 3;
      }
    }
    // The compiler used to GVN this field get with the one line 24,
    // even though the field is updated in the if.
    return m.field;
  }

  static int getConstant0() {
    return 0;
  }

  static int getConstant1() {
    return 1;
  }

  // CHECK-START: int Main.calc1(int) GVN (before)
  // CHECK: Add
  // CHECK: Sub
  // CHECK: Xor
  // CHECK: And
  // CHECK: Or
  // CHECK: Div

  // CHECK-START: int Main.calc1(int) GVN (after)
  // CHECK-NOT: Add
  // CHECK-NOT: Sub
  // CHECK-NOT: Xor
  // CHECK-NOT: And
  // CHECK-NOT: Or
  // CHECK-NOT: Div

  public static int calc1(int i) {
    int j = (getConstant0() ^ (i + getConstant0() - getConstant0())) * getConstant1();
    j = j & 0xffffffff;
    j = 0 | j;
    return j / 1;
  }


  // CHECK-START: int Main.calc2(int, int) GVN (before)
  // CHECK: Add
  // CHECK: Add
  // CHECK: Add

  // CHECK-START: int Main.calc2(int, int) GVN (after)
  // CHECK: Add
  // CHECK: Add
  // CHECK-NOT: Add

  public static int calc2(int x, int y) {
    int sum1 = x + y;
    int sum2 = y + x;
    return sum1 + sum2;
  }

  public int field;
  public boolean test = true;
}
