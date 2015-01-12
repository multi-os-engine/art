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

public class ImplicitNullCheckEliminationTest {

  // CHECK-START: int ImplicitNullCheckEliminationTest.testInvokeAndFieldSet(ImplicitNullCheckEliminationTest, boolean) INCE (before)
  // CHECK-DAG:     NullCheck (needed)
  // CHECK-DAG:     InvokeVirtual (needs_nc)
  // CHECK-DAG:     InvokeVirtual (needs_nc)
  // CHECK-DAG:     InstanceFieldSet (needs_nc)
  // CHECK-DAG:     InstanceFieldGet (needs_nc)
  // CHECK-DAG:     InvokeStaticOrDirect (direct, needs_nc)

  // CHECK-START: int ImplicitNullCheckEliminationTest.testInvokeAndFieldSet(ImplicitNullCheckEliminationTest, boolean) INCE (after)
  // CHECK-DAG:     NullCheck (not_needed)
  // CHECK-DAG:     InvokeVirtual (needs_nc)
  // CHECK-DAG:     InvokeVirtual (skip_nc)
  // CHECK-DAG:     InstanceFieldSet (skip_nc)
  // CHECK-DAG:     InstanceFieldGet (skip_nc)
  // CHECK-DAG:     InvokeStaticOrDirect (direct, skip_nc)

  public int testInvokeAndFieldSet(ImplicitNullCheckEliminationTest t, boolean cond) {
    t.f();
    if (cond) {
      t.f();
    } else {
      t.g();
    }
    t.x = 1;
    return t.x;
  }

  // CHECK-START: void ImplicitNullCheckEliminationTest.testInvokeInterface(TestInterface, boolean) INCE (before)
  // CHECK-DAG:     NullCheck (needed)
  // CHECK-DAG:     InvokeInterface (needs_nc)
  // CHECK-DAG:     InvokeInterface (needs_nc)
  // CHECK-DAG:     InvokeInterface (needs_nc)

  // CHECK-START: void ImplicitNullCheckEliminationTest.testInvokeInterface(TestInterface, boolean) INCE (after)
  // CHECK-DAG:     NullCheck (not_needed)
  // CHECK-DAG:     InvokeInterface (needs_nc)
  // CHECK-DAG:     InvokeInterface (skip_nc)
  // CHECK-DAG:     InvokeInterface (skip_nc)

  public void testInvokeInterface(TestInterface t, boolean cond) {
    t.h();
    if (cond) {
      t.h();
    } else {
      t.k();
    }
  }

  // CHECK-START: void ImplicitNullCheckEliminationTest.testIf(TestInterface, boolean) INCE (before)
  // CHECK-DAG:     NullCheck (needed)
  // CHECK-DAG:     InvokeInterface (needs_nc)
  // CHECK-DAG:     InvokeInterface (needs_nc)

  // CHECK-START: void ImplicitNullCheckEliminationTest.testIf(TestInterface, boolean) INCE (after)
  // CHECK-DAG:     NullCheck (not_needed)
  // CHECK-DAG:     InvokeInterface (needs_nc)
  // CHECK-DAG:     InvokeInterface (needs_nc)
  public void testIf(TestInterface t, boolean cond) {
    if (cond) {
      t.h();
    } else {
      t.k();
    }
  }

  // CHECK-START: void ImplicitNullCheckEliminationTest.testIfDirect(ImplicitNullCheckEliminationTest, boolean) INCE (before)
  // CHECK-DAG:     NullCheck (needed)
  // CHECK-DAG:     InvokeVirtual (needs_nc)
  // CHECK-DAG:     InvokeStaticOrDirect (direct, needs_nc)

  // CHECK-START: void ImplicitNullCheckEliminationTest.testIfDirect(ImplicitNullCheckEliminationTest, boolean) INCE (after)
  // CHECK-DAG:     NullCheck (needed)
  // CHECK-DAG:     InvokeVirtual (needs_nc)
  // CHECK-DAG:     InvokeStaticOrDirect (direct, skip_nc)
  public void testIfDirect(ImplicitNullCheckEliminationTest t, boolean cond) {
    if (cond) {
      t.f();
    } else {
      t.g();
    }
  }

  // CHECK-START: void ImplicitNullCheckEliminationTest.testInvokeDirect(ImplicitNullCheckEliminationTest) INCE (before)
  // CHECK-DAG:     NullCheck (needed)
  // CHECK-DAG:     InvokeStaticOrDirect (direct, needs_nc)
  // CHECK-DAG:     InvokeVirtual (needs_nc)

  // CHECK-START: void ImplicitNullCheckEliminationTest.testInvokeDirect(ImplicitNullCheckEliminationTest) INCE (after)
  // CHECK-DAG:     NullCheck (needed)
  // CHECK-DAG:     InvokeStaticOrDirect (direct, skip_nc)
  // CHECK-DAG:     InvokeVirtual (skip_nc)

  public void testInvokeDirect(ImplicitNullCheckEliminationTest t) {
    t.g();
    t.f();
  }



  // CHECK-START: void ImplicitNullCheckEliminationTest.testArray(TestInterface[]) INCE (before)
  // CHECK-DAG:     NullCheck (needed)
  // CHECK-DAG:     ArrayLength (needs_nc)
  // CHECK-DAG:     ArrayGet (needs_nc)
  // CHECK-DAG:     ArrayGet (needs_nc)
  // CHECK-DAG:     ArraySet (needs_nc)

  // CHECK-START: void ImplicitNullCheckEliminationTest.testArray(TestInterface[]) INCE (after)
  // CHECK-DAG:     NullCheck (not_needed)
  // CHECK-DAG:     ArrayLength (needs_nc)
  // CHECK-DAG:     ArrayGet (skip_nc)
  // CHECK-DAG:     ArrayGet (skip_nc)
  // CHECK-DAG:     ArraySet (skip_nc)

  public void testArray(TestInterface[] ts) {
    if (ts[0] == null) {
      ts[1] = ts[2];
    }
  }

  // CHECK-START: void ImplicitNullCheckEliminationTest.testArrayAndBCE(TestInterface[]) INCE (before)
  // CHECK-DAG:     NullCheck (needed)
  // CHECK-DAG:     ArrayLength (needs_nc)
  // CHECK-DAG:     ArrayGet (needs_nc)
  // CHECK-DAG:     ArraySet (needs_nc)

  // CHECK-START: void ImplicitNullCheckEliminationTest.testArrayAndBCE(TestInterface[]) INCE (after)
  // CHECK-DAG:     NullCheck (not_needed)
  // CHECK-DAG:     ArrayLength (needs_nc)
  // CHECK-DAG:     ArrayGet (skip_nc)
  // CHECK-DAG:     ArraySet (skip_nc)

  public void testArrayAndBCE(TestInterface[] ts) {
    for (int i = 0; i < ts.length; i++) {
      ts[i] = process(ts[i]);
    }
  }

  public int x;

  public void f() {}

  private void g() {
    // avoid inlining
    for (int i = 0; i < x; i++) {
      x--;
    }
  }

  public TestInterface process(TestInterface ti) {
    return ti;
  }

}

interface TestInterface {
  void h();
  void k();
}

