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

public class Main {
  public static void setNull$inline$(Object[] a) {
    a[0] = null;
  }

  /// CHECK-START: void Main.setNull2(java.lang.Object[]) inliner (before)
  /// CHECK-NOT: ArraySet
  /// CHECK-START: void Main.setNull2(java.lang.Object[]) inliner (after)
  /// CHECK: ArraySet needs_type_check:false {{(env:.*\],\[.*)}}
  public static void setNull2(Object[] a) {
    setNull$inline$(a);
  }

  public static void main(String[] args) {
    Object[] a = new Object[] {1};

    if (a[0] == null) {
      throw new Error("Expected 1");
    }

    setNull2(a);

    if (a[0] != null) {
      throw new Error("Expected null");
    }
  }
}
