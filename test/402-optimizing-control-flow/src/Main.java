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

// Note that $opt$ is a marker for the optimizing compiler to ensure
// it does compile the method.

public class Main {

  public static void expectEquals(int expected, int value) {
    if (expected != value) {
      throw new Error("Expected: " + expected + ", found: " + value);
    }
  }

  public static void main(String[] args) {
    int result = $opt$testIfEq1();
    expectEquals(42, result);

    result = $opt$testIfEq2();
    expectEquals(7, result);

    result = $opt$testWhileLoop();
    expectEquals(45, result);

    result = $opt$testDoWhileLoop();
    expectEquals(45, result);

    result = $opt$testForLoop();
    expectEquals(44, result);
  }

  static int $opt$testIfEq1() {
    if (42 + 1 == 43) {
      return 42;
    } else {
      return 7;
    }
  }

  static int $opt$testIfEq2() {
    if (42 + 1 == 41) {
      return 42;
    } else {
      return 7;
    }
  }

  static int $opt$testWhileLoop() {
    int a = 42;
    while (a++ != 44) {}
    return a;
  }

  static int $opt$testDoWhileLoop() {
    int a = 42;
    do {
    } while (a++ != 44);
    return a;
  }

  static int $opt$testForLoop() {
    int a = 42;
    for (; a != 44; a++) {}
    return a;
  }
}
