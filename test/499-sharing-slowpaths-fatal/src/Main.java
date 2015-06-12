/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

public class Main {
  public static void main(String[] args) {
    $noinline$OneDivZeroSlowPath(1);
    $noinline$MultipleDivZeroSlowPaths(3);
  }

  // A dummy value to defeat inlining of these routines.
  static boolean doThrow = false;

  public static int $noinline$OneDivZeroSlowPath(int div) {
    // Try defeating inlining.
    if (doThrow) {
      throw new Error();
    }
    return 10 / div;
  }

  public static int $noinline$MultipleDivZeroSlowPaths(int div) {
    // Try defeating inlining.
    if (doThrow) {
      throw new Error();
    }
    int res = 0;
    res += 10 / div;
    div --;
    res += 10 / div;
    div --;
    res += 10 / div;
    div --;
    res += 10 / div;
    return res;
  }
}
