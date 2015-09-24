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

  public static int inline_method() {
    return 5;
  }

  /// CHECK-START: int Main.p(int) dead_code_elimination_final (before)
  /// CHECK-DAG:                      PackedSwitch

  /// CHECK-START: int Main.p(int) dead_code_elimination_final (after)
  /// CHECK-NOT:                      PackedSwitch
  public static int p(int j) {
    int i = inline_method();
    int l = 100;
    if (i > 100) {
      switch(j) {
        case 1:
          i++;
          break;
        case 2:
          i = 99;
          break;
        case 3:
          i = 100;
          break;
        case 4:
          i = -100;
          break;
        case 5:
          i = 7;
          break;
        case 6:
          i = -9;
          break;
      }
      l += i;
    }

    return l;
  }

  public static void main(String[] args) throws Exception {
    p(10);
  }
}
