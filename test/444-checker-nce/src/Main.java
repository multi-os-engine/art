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

  // CHECK-START: Main Main.newInstanceTest() null_check_elimination (before)
  // CHECK-DAG:     NewInstance
  // CHECK-DAG:     NullCheck
  // CHECK-DAG:     InvokeStaticOrDirect
  // CHECK-NOT:     NullCheck
  // CHECK-DAG:     InvokeStaticOrDirect

  // CHECK-START: Main Main.newInstanceTest() null_check_elimination (after)
  // CHECK-DAG:     NewInstance
  // CHECK-NOT:     NullCheck
  // CHECK-DAG:     InvokeStaticOrDirect
  // CHECK-NOT:     NullCheck
  // CHECK-DAG:     InvokeStaticOrDirect
  public Main newInstanceTest() {
    Main m = new Main();
    return m.g();
  }

  // CHECK-START: Main Main.newArrayTest() null_check_elimination (before)
  // CHECK-DAG:     NewArray
  // CHECK-DAG:     NullCheck
  // CHECK-DAG:     ArrayGet

  // CHECK-START: Main Main.newArrayTest() null_check_elimination (after)
  // CHECK-DAG:     NewArray
  // CHECK-NOT:     NullCheck
  // CHECK-DAG:     ArrayGet
  public Main newArrayTest() {
    Main[] ms = new Main[1];
    return ms[0];
  }

  // CHECK-START: Main Main.ifTest(boolean) null_check_elimination (before)
  // CHECK-DAG:     NewInstance
  // CHECK-DAG:     NullCheck

  // CHECK-START: Main Main.ifTest(boolean) null_check_elimination (after)
  // CHECK-DAG:     NewInstance
  // CHECK-NOT:     NullCheck
  public Main ifTest(boolean flag) {
    Main m = null;
    if (flag) {
      m = new Main();
    } else {
      m = new Main(1);
    }
    return m.g();
  }

  // CHECK-START: Main Main.ifKeepCheckTest(boolean) null_check_elimination (before)
  // CHECK-DAG:     NewInstance
  // CHECK-DAG:     NullCheck

  // CHECK-START: Main Main.ifKeepCheckTest(boolean) null_check_elimination (after)
  // CHECK-DAG:     NewInstance
  // CHECK-DAG:     NullCheck
  public Main ifKeepCheckTest(boolean flag) {
    Main m = null;
    if (flag) {
      m = new Main(1);
    }
    return m.g();
  }

  // CHECK-START: Main Main.keepCheckTest(Main) null_check_elimination (before)
  // CHECK:     NullCheck
  // CHECK:     InvokeStaticOrDirect

  // CHECK-START: Main Main.keepCheckTest(Main) null_check_elimination (after)
  // CHECK:     NullCheck
  // CHECK:     InvokeStaticOrDirect
  public Main keepCheckTest(Main m) {
    return m.g();
  }

  public Main() {}
  public Main(int dummy) {}

  private Main g() {
    // avoids inlining
    throw new RuntimeException();
  }


  public static void main(String[] args) {
    new Main();
  }

}
