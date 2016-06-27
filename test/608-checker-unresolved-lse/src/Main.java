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

  public static void main(String[] args) {
    instanceFieldTest();
    staticFieldTest();
  }

  /// CHECK-START: void Main.instanceFieldTest() inliner (before)
  /// CHECK-NOT:    InstanceFieldSet

  /// CHECK-START: void Main.instanceFieldTest() inliner (after)
  /// CHECK:        InstanceFieldSet

  /// CHECK-START: void Main.instanceFieldTest() load_store_elimination (after)
  /// CHECK:        InstanceFieldSet
  public static void instanceFieldTest() {
    Foo f = new Foo();
    if (f.iField != 42) {
      throw new Error("Expected 42, got " + f.iField);
    }
  }

  /// CHECK-START: void Main.staticFieldTest() inliner (before)
  /// CHECK-NOT:    StaticFieldSet

  /// CHECK-START: void Main.staticFieldTest() inliner (after)
  /// CHECK:        StaticFieldSet
  /// CHECK:        StaticFieldSet

  /// CHECK-START: void Main.staticFieldTest() load_store_elimination (after)
  /// CHECK:        StaticFieldSet
  /// CHECK:        StaticFieldSet
  public static void staticFieldTest() {
    // Ensure Foo is initialized.
    Foo f = new Foo();
    f.$inline$StaticSet42();
    if (Foo.sField != 42) {
      throw new Error("Expected 42, got " + Foo.sField);
    }
    f.$inline$StaticSet43();
    if (Foo.sField != 43) {
      throw new Error("Expected 43, got " + Foo.sField);
    }
  }
}

class Foo {
  // field needs to be package-private to make the access in Main.main
  // unresolved.
  int iField;
  static int sField;

  public void $inline$StaticSet42() {
    sField = 42;
  }

  public void $inline$StaticSet43() {
    sField = 43;
  }


  // Constructor needs to be public to get it resolved in Main.main
  // and therefore inlined.
  public Foo() {
    iField = 42;
  }
}
