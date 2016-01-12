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

  /**
   * Method with an outer countable loop and an inner do-while loop.
   * Since all work is done in the header of the inner loop, any invariant hoisting
   * and deopting should be done in its loop header proper, not the true-block
   * of the newly generated taken-test after dynamic BCE. 
   */
  public static int doit(int[][] x, int j) {
    float f = 0;
    int acc = 0;
    for (int i = 0; i < 2; i++) {
      do {
        f++;  // A bit noise to avoid early hoisting.
        acc += x[i][i];
      } while (++j < i);
    }
    return acc;
  }

  public static void main(String args[]) {
    int[][] x = new int[2][2];
    int y;

    x[0][0] = 1;
    x[1][1] = 2;

    expectEquals(8, doit(x, -6));
    expectEquals(7, doit(x, -5));
    expectEquals(6, doit(x, -4));
    expectEquals(5, doit(x, -3));
    expectEquals(4, doit(x, -2));
    expectEquals(3, doit(x, -1));
    expectEquals(3, doit(x,  0));
    expectEquals(3, doit(x,  1));
    expectEquals(3, doit(x, 22));

    System.out.println("passed");
  }

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}
