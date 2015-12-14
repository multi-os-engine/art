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

  /// CHECK-START: long Main.$noinline$longSelect(long) register (before)
  /// CHECK:         <<Cond:z\d+>> LessThanOrEqual [{{j\d+}},{{j\d+}}]
  /// CHECK-NEXT:                  Select [{{j\d+}},{{j\d+}},<<Cond>>]

  public long $noinline$longSelect(long param) {
    if (doThrow) { throw new Error(); }
    long val_true = longB;
    long val_false = longC;
    return (param > longA) ? val_true : val_false;
  }

  public static void main(String[] args) {
    Main m = new Main();
    assertLongEquals(5, m.$noinline$longSelect(4));
    assertLongEquals(7, m.$noinline$longSelect(2));
  }

  public static void assertLongEquals(long expected, long actual) {
    if (expected != actual) {
      throw new Error(expected + " != " + actual);
    }
  }

  public boolean doThrow = false;

  public long longA = 3;
  public long longB = 5;
  public long longC = 7;
}
