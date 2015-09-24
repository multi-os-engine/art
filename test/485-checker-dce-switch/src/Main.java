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

  public static int $inline$method() {
    return 5;
  }

  /// CHECK-START: int Main.p(int) dead_code_elimination_final (before)
  /// CHECK-DAG:                      PackedSwitch

  /// CHECK-START: int Main.p(int) dead_code_elimination_final (after)
  /// CHECK-DAG:    <<Const100:i\d+>> IntConstant 100
  /// CHECK-NOT:                      PackedSwitch
  /// CHECK-DAG:                      Return [<<Const100>>]
  public static int p(int j) {
    int i = $inline$method();
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

  /// CHECK-START: int Main.q() dead_code_elimination_final (before)
  /// CHECK-DAG:                      PackedSwitch

  /// CHECK-START: int Main.q() dead_code_elimination_final (after)
  /// CHECK-DAG:     <<Const7:i\d+>>  IntConstant 7
  /// CHECK-NOT:                      PackedSwitch
  /// CHECK-DAG:                      Return [<<Const7>>]
  public static int q() {
    int i = $inline$method();
    switch(i) {
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

    return i;
  }

  public static void main(String[] args) throws Exception {
    int ret_val = p(10);
    if (ret_val != 100) {
      throw new Error("Incorrect return value from p:" + ret_val);
    }

    ret_val = q();
    if (ret_val != 7) {
      throw new Error("Incorrect return value from q:" + ret_val);
    }
  }
}
