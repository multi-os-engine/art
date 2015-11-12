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

//
// Test on lambda expressions.
//
public class Main {

  @NeedsLambda
  interface L0 {
    public void run();
  }

  @NeedsLambda
  interface L1 {
    public void run(int a);
  }

  int mField;

  public Main() {
    mField = 4;
  }

  /// CHECK-START: void Main.doL0() ssa_builder (after)
  /// CHECK-DAG: CreateLambda
  /// CHECK-DAG: InvokeLambda
  /// CHECK-DAG: ReturnVoid
  private void doL0() {
    L0 lambda0 = () -> { System.out.println("Lambda0:"); };
    lambda0.run();
  }

  /// CHECK-START: void Main.doL1() ssa_builder (after)
  /// CHECK-DAG: CreateLambda
  /// CHECK-DAG: InvokeLambda
  /// CHECK-DAG: ReturnVoid
  private void doL1() {
    L1 lambda1 = (int a) -> { System.out.println("Lambda1:" + a); };
    lambda1.run(1);
  }

  // TODO: b/25631011 prevents enabling this test
  private void doL1Capture(int parameter) {
    int local = 3;
    // Takes its own parameter and captures parameter, local, and field.
//  L1 lambda1 = (int a) -> { System.out.println("Lambda1:" + a + parameter + local + mField); };
//  lambda1.run(1);
  }

  public static void main(String[] args) {
    Main main = new Main();
    main.doL0();
    main.doL1();
    main.doL1Capture(2);
    System.out.println("passed");
  }
}
