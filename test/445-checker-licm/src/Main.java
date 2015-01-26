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

  // CHECK-START: int Main.div() licm (before)
  // CHECK-DAG: Div [ {{i\d+}} {{i\d+}} ] (loop_header: {{B\d+}})

  // CHECK-START: int Main.div() licm (after)
  // CHECK-NOT: Div [ {{i\d+}} {{i\d+}} ] (loop_header: {{B\d+}})
  // CHECK-DAG: Div [ {{i\d+}} {{i\d+}} ] (loop_header: null)

  public static int div() {
    int b = staticField;
    int result = 0;
    for (int i = 0; i < 10; ++i) {
      result += staticField / 42;
    }
    return result;
  }

  // CHECK-START: int Main.innerDiv() licm (before)
  // CHECK-DAG: Div [ {{i\d+}} {{i\d+}} ] (loop_header: {{B\d+}})

  // CHECK-START: int Main.innerDiv() licm (after)
  // CHECK-NOT: Div [ {{i\d+}} {{i\d+}} ] (loop_header: {{B\d+}})
  // CHECK-DAG: Div [ {{i\d+}} {{i\d+}} ] (loop_header: null)

  public static int innerDiv() {
    int b = staticField;
    int result = 0;
    for (int i = 0; i < 10; ++i) {
      for (int j = 0; j < 10; ++j) {
        result += staticField / 42;
      }
    }
    return result;
  }


  // CHECK-START: int Main.arrayLength(int[]) licm (before)
  // CHECK-DAG: [[NullCheck:l\d+]] NullCheck [ {{l\d+}} ] (loop_header: {{B\d+}})
  // CHECK-DAG:                    ArrayLength [ [[NullCheck]] ] (loop_header: {{B\d+}})

  // CHECK-START: int Main.arrayLength(int[]) licm (after)
  // CHECK-NOT:                    NullCheck [ {{l\d+}} ] (loop_header: {{B\d+}})
  // CHECK-NOT:                    ArrayLength [ {{l\d+}} ] (loop_header: {{B\d+}})
  // CHECK-DAG: [[NullCheck:l\d+]] NullCheck [ {{l\d+}} ] (loop_header: null)
  // CHECK-DAG:                    ArrayLength [ [[NullCheck]] ] (loop_header: null)

  public static int arrayLength(int[] array) {
    int result = 0;
    for (int i = 0; i < array.length; ++i) {
      result += array[i];
    }
    return result;
  }

  public static int staticField = 42;

  public static void assertEquals(int expected, int actual) {
    if (expected != actual) {
      throw new Error("Expected " + expected + ", got " + actual);
    }
  }

  public static void main(String[] args) {
    assertEquals(10, div());
    assertEquals(100, innerDiv());
    assertEquals(12, arrayLength(new int[] { 4, 8 }));
  }
}
