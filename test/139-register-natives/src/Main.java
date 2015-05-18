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
  public static void main(String[] args) {
    testRegistration1();
    testRegistration2();
    testRegistration3();
  }

  static {
    System.loadLibrary("arttest");
  }

  // Test that a subclass' method is registered instead of a superclass' method.
  private static void testRegistration1() {
    registerNatives(TestSub.class);

    try {
      new TestSub().foo();
    } catch (Throwable t) {
      expectNotThrows(t, "TestSub.foo");
    }

    try {
      new TestSuper().fooTest();
      expectThrows("TestSuper.foo");
    } catch (Throwable ignored) {
    }
  }

  // Test that a superclass' method is registered if the subclass doesn't have a matching method.
  private static void testRegistration2() {
    registerNatives(TestSub2.class);

    try {
      new TestSub2().foo();
    } catch (Throwable t) {
      expectNotThrows(t, "TestSub2.foo");
    }

    try {
      new TestSuper2().foo();
    } catch (Throwable t) {
      expectNotThrows(t, "TestSub2.foo");
    }
  }

  // Test that registration fails if the subclass has a matching non-native method.
  private static void testRegistration3() {
    try {
      registerNatives(TestSub3.class);
      expectThrows("TestSub3");
    } catch (NoSuchMethodError ignored) {
    }
  }

  private native static int registerNatives(Class c);

  private static void expectThrows(String s) {
    System.out.println("Expected exception for " + s);
  }

  private static void expectNotThrows(Throwable t, String s) {
    System.out.println("Did not expect an exception for " + s);
    t.printStackTrace(System.out);
  }
}

class TestSuper {
  private native void foo();

  public void fooTest() {
    foo();
  }
}

class TestSub extends TestSuper {
  public native void foo();
}

class TestSuper2 {
  public native void foo();
}

class TestSub2 extends TestSuper2 {
}

class TestSuper3 {
  public native void foo();
}

class TestSub3 extends TestSuper3 {
  public void foo() {
    System.out.println("TestSub3.foo()");
  }
}
