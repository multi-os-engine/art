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
class Bad {
  static int i = 1 / 0;

  static void test() {}
}
class Test {
  public static void test() {
    Bad.test();
  }
}

class Main {
  public static void main(String[] args) {
    Class<?> c;
    try {
      c = Class.forName("Test");
    } catch (Throwable t) {
      System.out.println("Could not load class Test: " + t);
      return;
    }

    java.lang.reflect.Method method = null;
    for (java.lang.reflect.Method m : c.getDeclaredMethods()) {
      if (m.getName().equals("test")) {
        method = m;
        break;
      }
    }
    if (method == null) {
      System.out.println("Could not find method 'test'");
    }

    try {
      method.invoke(null, new Object[] {});
      System.out.println("There is not expected exception");
    } catch (Throwable t) {
    }
  }
}
