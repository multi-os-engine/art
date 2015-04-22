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

  // TODO: Add a test case for when the static method is not
  // "available", making it non inlinable.

  /*
   * Ensure an inline static invoke explicitly triggers the
   * initialization check of the called method's declaring class.
   */

  // CHECK-START: void Main.InvokeStaticInlined() inliner (before)
  // CHECK-DAG:     [[LoadClass:l\d+]]    LoadClass
  // CHECK-DAG:     [[ClinitCheck:l\d+]]  ClinitCheck [ [[LoadClass]] ]
  // CHECK-DAG:     [[Invoke:v\d+]]       InvokeStaticOrDirect [ [[ClinitCheck]] ]

  // CHECK-START: void Main.InvokeStaticInlined() inliner (after)
  // CHECK-DAG:     [[LoadClass:l\d+]]    LoadClass
  // CHECK-DAG:     [[ClinitCheck:l\d+]]  ClinitCheck [ [[LoadClass]] ]

  // CHECK-START: void Main.InvokeStaticInlined() inliner (after)
  // CHECK-NOT:                           InvokeStaticOrDirect

  // TODO: Implement checks after the PrepareForRegisterAllocation
  // pass.  Note that the graph is not dumped after (nor before) this
  // pass, so we can check the graph as it is before the next pass
  // (liveness analysis) instead.

  static void InvokeStaticInlined() {
    ClassWithClinit1.$opt$inline$StaticMethod();
  }

  static class ClassWithClinit1 {
    static {
      Main.classWithClinitInitialized = true;
    }

    static void $opt$inline$StaticMethod() {
    }
  }

  /*
   * Ensure a non-inlined static invoke does not have an explicit
   * initialization check of the called method's declaring class as
   * input.
   */

  // CHECK-START: void Main.InvokeStaticNotInlined() inliner (before)
  // CHECK-DAG:     [[LoadClass:l\d+]]    LoadClass
  // CHECK-DAG:     [[ClinitCheck:l\d+]]  ClinitCheck [ [[LoadClass]] ]
  // CHECK-DAG:     [[Invoke:v\d+]]       InvokeStaticOrDirect [ [[ClinitCheck]] ]

  // CHECK-START: void Main.InvokeStaticNotInlined() inliner (after)
  // CHECK-DAG:     [[LoadClass:l\d+]]    LoadClass
  // CHECK-DAG:     [[ClinitCheck:l\d+]]  ClinitCheck [ [[LoadClass]] ]
  // CHECK-DAG:     [[Invoke:v\d+]]       InvokeStaticOrDirect [ [[ClinitCheck]] ]

  // TODO: Implement checks after the PrepareForRegisterAllocation
  // pass.  Note that the graph is not dumped after (nor before) this
  // pass, so we can check the graph as it is before the next pass
  // (liveness analysis) instead.

  static void InvokeStaticNotInlined() {
    ClassWithClinit2.StaticMethod();
  }

  static class ClassWithClinit2 {
    static {
      Main.classWithClinitInitialized = false;
    }

    static boolean doThrow = false;

    static void StaticMethod() {
      if (doThrow) {
        // Try defeating inlining.
        throw new Error();
      }
    }
  }


  static boolean classWithClinitInitialized = false;

  public static void main(String[] args) {
    InvokeStaticInlined();
    InvokeStaticNotInlined();
  }
}
