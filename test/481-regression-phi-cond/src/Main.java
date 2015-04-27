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
  public static void assertBooleanEquals(boolean expected, boolean result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static boolean InlinePhi(boolean x, boolean y, boolean z) {
    boolean phi;
    if (z) {
      phi = x;
    } else {
      phi = y;
    }
    return phi;
  }

  public static boolean DontUseParam(boolean x) {
    return false;
  }

  public static boolean TestCase(boolean x, boolean y, boolean z) {
    boolean phi = InlinePhi(x, y, z);
    return DontUseParam(phi == false ? false : true);
  }

  public static void main(String[] args) {
    assertBooleanEquals(false, TestCase(true, true, true));
  }
}
