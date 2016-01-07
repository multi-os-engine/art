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

interface IfaceA {
  default void $noinline$f() { throw new RuntimeException(); }
}

interface IfaceB extends IfaceA {}

class ClassA implements IfaceA {
  // Test that we optimize a interface-super to a direct call.
  /// CHECK-START: void ClassA.testSuperInvoke() sharpening (before)
  /// CHECK:                    InvokeSuperInterface
  /// CHECK-START: void ClassA.testSuperInvoke() sharpening (after)
  /// CHECK:                    InvokeStaticOrDirect
  public void testSuperInvoke() {
    IfaceA.super.$noinline$f();
  }
}

class ClassB extends ClassA {
  // Test that nothing happens with regular invoke-super.
  /// CHECK-START: void ClassB.testRegularSuperInvoke() sharpening (before)
  /// CHECK:                    InvokeStaticOrDirect
  /// CHECK-START: void ClassB.testRegularSuperInvoke() sharpening (after)
  /// CHECK:                    InvokeStaticOrDirect
  public void testRegularSuperInvoke() {
    super.$noinline$f();
  }
}

class ClassC implements IfaceB {
  // Test that it works even it a supertype of the interface implements the function
  /// CHECK-START: void ClassC.testSuperInvoke() sharpening (before)
  /// CHECK:                    InvokeSuperInterface
  /// CHECK-START: void ClassC.testSuperInvoke() sharpening (after)
  /// CHECK:                    InvokeStaticOrDirect
  public void testSuperInvoke() {
    IfaceB.super.$noinline$f();
  }
}

public class Main {
  public static void main(String[] args) {
  }
}
