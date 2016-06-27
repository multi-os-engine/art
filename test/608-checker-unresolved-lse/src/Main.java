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

// We make Main extend an unresolved super class. This will lead to an
// unresolved access to Foo.field, as we won't know if Main can access
// a package private field.
public class Main extends MissingSuperClass {

  /// CHECK-START: void Main.main(java.lang.String[]) inliner (before)
  /// CHECK-NOT:        InstanceFieldSet

  /// CHECK-START: void Main.main(java.lang.String[]) inliner (after)
  /// CHECK:        InstanceFieldSet

  /// CHECK-START: void Main.main(java.lang.String[]) load_store_elimination (after)
  /// CHECK:        InstanceFieldSet
  public static void main(String[] args) {
    Foo f = new Foo();
    if (f.field != 42) {
      throw new Error("Expected 42, got " + f.field);
    }
  }
}

class Foo {
  // field needs to be package-private to make the access in Main.main
  // unresolved.
  int field;

  // Constructor needs to be public to get it resolved in Main.main
  // and therefore inlined.
  public Foo() {
    field = 42;
  }
}
