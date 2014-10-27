/*
 * Copyright (C) 2014 The Android Open Source Project
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

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class Main {
  // Workaround for b/18051191.
  static class Nested {
    public void foo() {
      System.out.println("Hello");
    }
  }

  public static void main(String[] args) throws Exception {
    testSmaliFilledNewArray();
  }

  public static Object[] invoke(Method m) throws Exception {
    Object[] args = {new Integer(2), new Integer(2)};
    // Without write barriers in the filled-new-array method invoked
    // below, the arguments won't be seen by the GC at the point we're forcing
    // the GC (in testSmaliFilledNewArray).
    return (Object[])m.invoke(null, args);
  }

  public static void forceGC() {
    Runtime.getRuntime().gc();
    Runtime.getRuntime().gc();
    Runtime.getRuntime().gc();
    Runtime.getRuntime().gc();
    Runtime.getRuntime().gc();
    Runtime.getRuntime().gc();
  }  

  public static void testSmaliFilledNewArray() throws Exception {
    Class<?> c = Class.forName("FilledNewArray");
    Method m = c.getMethod("newRef", Integer.class, Integer.class);
    Object[] res = invoke(m);
    forceGC();

    // Create lots of objects to hope the address of the objects are re-used.
    for (int i = 0; i < 1000; i++) {
      new Object();
    }
    if (res[0].getClass() != Integer.class || res[1].GetClass() != Integer.class) {
      throw new Error("Unexpected object in array");
    }
  }
}
