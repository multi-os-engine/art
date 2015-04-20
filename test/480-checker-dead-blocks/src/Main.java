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

  /*
   * Note that `javac` flips conditions and hence the true/false blocks
   * are also flipped in the HGraph. 
   */
  
  public static void assertIntEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  // CHECK-START: int Main.atestTrueBranch(int, int) dead_block_elimination (before)
  // CHECK-DAG:     [[ArgX:i\d+]]    ParameterValue
  // CHECK-DAG:     [[ArgY:i\d+]]    ParameterValue
  // CHECK-DAG:     [[Const1:i\d+]]  IntConstant 1
  // CHECK-DAG:                      If [ [[Const1]] ]
  // CHECK-DAG:     [[Add:i\d+]]     Add [ [[ArgX]] [[ArgY]] ]
  // CHECK-DAG:     [[Sub:i\d+]]     Sub [ [[ArgX]] [[ArgY]] ]
  // CHECK-DAG:     [[Phi:i\d+]]     Phi [ [[Add]] [[Sub]] ] 
  // CHECK-DAG:                      Return [ [[Phi]] ] 
  
  // CHECK-START: int Main.atestTrueBranch(int, int) dead_block_elimination (after)
  // CHECK-DAG:     [[ArgX:i\d+]]    ParameterValue
  // CHECK-DAG:     [[ArgY:i\d+]]    ParameterValue
  // CHECK-DAG:     [[Sub:i\d+]]     Sub [ [[ArgX]] [[ArgY]] ]
  // CHECK-DAG:                      Return [ [[Sub]] ] 

  // CHECK-START: int Main.atestTrueBranch(int, int) dead_block_elimination (after)
  // CHECK-NOT:                      Add
  // CHECK-NOT:                      Phi
  
  public static boolean inlineTrue() {
    return true;
  }

  public static int atestTrueBranch(int x, int y) {
    int z;
    if (inlineTrue() != true) {
      z = x + y;
    } else {
      z = x - y;
    }
    return z;
  }

  // CHECK-START: int Main.atestFalseBranch(int, int) dead_block_elimination (before)
  // CHECK-DAG:     [[ArgX:i\d+]]    ParameterValue
  // CHECK-DAG:     [[ArgY:i\d+]]    ParameterValue
  // CHECK-DAG:     [[Const0:i\d+]]  IntConstant 0
  // CHECK-DAG:                      If [ [[Const0]] ]
  // CHECK-DAG:     [[Add:i\d+]]     Add [ [[ArgX]] [[ArgY]] ]
  // CHECK-DAG:     [[Sub:i\d+]]     Sub [ [[ArgX]] [[ArgY]] ]
  // CHECK-DAG:     [[Phi:i\d+]]     Phi [ [[Add]] [[Sub]] ] 
  // CHECK-DAG:                      Return [ [[Phi]] ] 

  // CHECK-START: int Main.atestFalseBranch(int, int) dead_block_elimination (after)
  // CHECK-DAG:     [[ArgX:i\d+]]    ParameterValue
  // CHECK-DAG:     [[ArgY:i\d+]]    ParameterValue
  // CHECK-DAG:     [[Add:i\d+]]     Add [ [[ArgX]] [[ArgY]] ]
  // CHECK-DAG:                      Return [ [[Add]] ] 

  // CHECK-START: int Main.atestFalseBranch(int, int) dead_block_elimination (after)
  // CHECK-NOT:                      Sub
  // CHECK-NOT:                      Phi
  
  public static boolean inlineFalse() {
    return false;
  }
  
  public static int atestFalseBranch(int x, int y) {
    int z;
    if (inlineFalse() == false) {
      z = x + y;
    } else {
      z = x - y;
    }
    return z;
  }
  
  // CHECK-START: int Main.atestInfiniteLoop(int) dead_block_elimination (before)
  // CHECK:                          Exit

  // CHECK-START: int Main.atestInfiniteLoop(int) dead_block_elimination (after)
  // CHECK-NOT:                      Exit
  
  public static int atestInfiniteLoop(int x) {
    while (inlineTrue()) {
      x++;
    }
    return x;
  }
  
  public static void main(String[] args) {
    assertIntEquals(1, atestTrueBranch(4, 3));
    assertIntEquals(7, atestFalseBranch(4, 3));
  }
}
