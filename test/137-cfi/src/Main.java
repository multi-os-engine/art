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
  public static void main(String[] args) throws Exception {
    System.out.println(new Main().foo(true));
  }

  static {
    System.loadLibrary("arttest");
  }

  public boolean foo(boolean b) {
    System.out.println(b);
    if (b) {
      return bar(false);
    }
    return true;
  }

  public boolean bar(boolean b) {
    System.out.println(b);
    if (!b) {
      return cfiBaz(1, true);
    }
    return false;
  }

  public native boolean cfiBaz(int i, boolean b);
}
