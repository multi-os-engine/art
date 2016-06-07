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

  /// CHECK-START: int Main.foo(int, int) prepare_for_register_allocation (before)
  /// CHECK:         <<Cond:z\d+>> NotEqual
  /// CHECK-NEXT:    <<Add:i\d+>>  Add
  /// CHECK-NEXT:                  Select [{{i\d+}},<<Add>>,<<Cond>>]

  /// CHECK-START: int Main.foo(int, int) prepare_for_register_allocation (after)
  /// CHECK:         <<Add:i\d+>>  Add
  /// CHECK-NEXT:    <<Cond:z\d+>> NotEqual
  /// CHECK-NEXT:                  Select [{{i\d+}},<<Add>>,<<Cond>>]

  public static int foo(int x, int y) {
    return (x == y) ? x : x + y;
  }

  public static void assertEquals(int x, int y) {
    if (x != y) {
      throw new Error("Assertion failed: " + x + " != " + "y");
    }
  }

  public static void main(String[] args) {
    assertEquals(foo(4, 4), 4);
    assertEquals(foo(3, 4), 7);
  }
}
