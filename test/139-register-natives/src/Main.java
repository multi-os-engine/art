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
    registerNatives(TestSub.class);

    try {
      new TestSub().foo();
      System.out.println("TestSub.foo succeeded");
    } catch (Throwable t) {
      System.out.println("TestSub.foo failed");
    }

    try {
      new TestSuper().fooTest();
      System.out.println("TestSuper.foo succeeded");
    } catch (Throwable t) {
      System.out.println("TestSuper.foo failed");
    }

    registerNatives(TestSub2.class);

    try {
      new TestSub2().foo();
      System.out.println("TestSub2.foo succeeded");
    } catch (Throwable t) {
      System.out.println("TestSub2.foo failed");
    }

    try {
      new TestSuper2().foo();
      System.out.println("TestSuper2.foo succeeded");
    } catch (Throwable t) {
      System.out.println("TestSuper2.foo failed");
    }

    try {
      registerNatives(TestSub3.class);
      System.out.println("Expected registration failure.");
    } catch (NoSuchMethodError ignored) {
    }
  }

  static {
    System.loadLibrary("arttest");
  }

  private native static int registerNatives(Class c);

  private native static boolean isDex2OatEnabled();
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
