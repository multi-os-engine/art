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

public final class Main implements Interface {

  static void methodWithInvokeInterface(Interface interf) {
    interf.$nonline$doCall();
  }

  public void $noinline$doCall() {
    if (doThrow) throw new Error("");
  }

  public static void main(String[] args) {
    test1();
    test2();
  }

  /// CHECK-START: void Main.test1() inliner (before)
  /// CHECK:                          InvokeStaticOrDirect

  /// CHECK-START: void Main.test1() inliner (before)
  /// CHECK-NOT:                      InvokeInterface

  /// CHECK-START: void Main.test1() inliner (after)
  /// CHECK:                          InvokeInterface

  /// CHECK-START: void Main.test1() inliner (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect
  public static void test1() {
    methodWithInvokeInterface(itf);
  }

  /// CHECK-START: void Main.test2() inliner (before)
  /// CHECK:                          InvokeStaticOrDirect

  /// CHECK-START: void Main.test2() inliner (before)
  /// CHECK-NOT:                      InvokeInterface

  /// CHECK-START: void Main.test2() inliner (after)
  /// CHECK:                          InvokeVirtual

  /// CHECK-START: void Main.test2() inliner (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect
  /// CHECK-NOT:                      InvokeInterface
  public static void test2() {
    methodWithInvokeInterface(m);
  }

  static Interface itf = new Main();
  static Main m = new Main();
  static boolean doThrow = false;
}

interface Interface {
  public void $noinline$doCall();
}
