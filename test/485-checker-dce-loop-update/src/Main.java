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

import java.lang.reflect.Method;

public class Main {

  // Workaround for b/18051191.
  class InnerClass {}

  // CHECK-START: int TestCase.testSingleExit(int, boolean) dead_code_elimination_final (before)
  // CHECK-DAG:     [[ArgX:i\d+]]  ParameterValue
  // CHECK-DAG:     [[ArgY:z\d+]]  ParameterValue
  // CHECK-DAG:     [[Cst1:i\d+]]  IntConstant 1
  // CHECK-DAG:     [[Cst5:i\d+]]  IntConstant 5
  // CHECK-DAG:     [[Cst7:i\d+]]  IntConstant 7
  //
  // CHECK-DAG:     [[PhiX:i\d+]]  Phi [ [[ArgX]] [[Add5:i\d+]] [[Add7:i\d+]] ] loop_header:[[HeaderY:B\d+]]
  // CHECK-DAG:                    If [ [[ArgY]] ]                              loop_header:[[HeaderY]]
  // CHECK-DAG:                    If [ [[Cst1]] ]                              loop_header:[[HeaderY]]
  // CHECK-DAG:     [[Add5]]       Add [ [[PhiX]] [[Cst5]] ]                    loop_header:[[HeaderY]]
  // CHECK-DAG:     [[Add7]]       Add [ [[PhiX]] [[Cst7]] ]                    loop_header:[[HeaderY]]
  // CHECK-DAG:                    Return [ [[PhiX]] ]                          loop_header:null

  // CHECK-START: int TestCase.testSingleExit(int, boolean) dead_code_elimination_final (after)
  // CHECK-DAG:     [[ArgX:i\d+]]  ParameterValue
  // CHECK-DAG:     [[ArgY:z\d+]]  ParameterValue
  // CHECK-DAG:     [[Cst7:i\d+]]  IntConstant 7
  //
  // CHECK-DAG:     [[PhiX:i\d+]]  Phi [ [[ArgX]] [[AddX:i\d+]] ]               loop_header:[[HeaderY:B\d+]]
  // CHECK-DAG:                    If [ [[ArgY]] ]                              loop_header:[[HeaderY]]
  // CHECK-DAG:     [[AddX]]       Add [ [[PhiX]] [[Cst7]] ]                    loop_header:[[HeaderY]]
  // CHECK-DAG:                    Return [ [[PhiX]] ]                          loop_header:null


  // CHECK-START: int TestCase.testMultipleExits(int, boolean, boolean) dead_code_elimination_final (before)
  // CHECK-DAG:     [[ArgX:i\d+]]  ParameterValue
  // CHECK-DAG:     [[ArgY:z\d+]]  ParameterValue
  // CHECK-DAG:     [[ArgZ:z\d+]]  ParameterValue
  // CHECK-DAG:     [[Cst1:i\d+]]  IntConstant 1
  // CHECK-DAG:     [[Cst5:i\d+]]  IntConstant 5
  // CHECK-DAG:     [[Cst7:i\d+]]  IntConstant 7
  //
  // CHECK-DAG:     [[PhiX:i\d+]]  Phi [ [[ArgX]] [[Add5:i\d+]] [[Add7:i\d+]] ] loop_header:[[HeaderY:B\d+]]
  // CHECK-DAG:                    If [ [[ArgY]] ]                              loop_header:[[HeaderY]]
  // CHECK-DAG:                    If [ [[ArgZ]] ]                              loop_header:[[HeaderY]]
  // CHECK-DAG:                    If [ [[Cst1]] ]                              loop_header:[[HeaderY]]
  // CHECK-DAG:     [[Add5]]       Add [ [[PhiX]] [[Cst5]] ]                    loop_header:[[HeaderY]]
  // CHECK-DAG:     [[Add7]]       Add [ [[PhiX]] [[Cst7]] ]                    loop_header:[[HeaderY]]
  // CHECK-DAG:                    Return [ [[PhiX]] ]                          loop_header:null

  // CHECK-START: int TestCase.testMultipleExits(int, boolean, boolean) dead_code_elimination_final (after)
  // CHECK-DAG:     [[ArgX:i\d+]]  ParameterValue
  // CHECK-DAG:     [[ArgY:z\d+]]  ParameterValue
  // CHECK-DAG:     [[ArgZ:z\d+]]  ParameterValue
  // CHECK-DAG:     [[Cst7:i\d+]]  IntConstant 7
  //
  // CHECK-DAG:     [[PhiX:i\d+]]  Phi [ [[ArgX]] [[Add7:i\d+]] ]               loop_header:[[HeaderY:B\d+]]
  // CHECK-DAG:                    If [ [[ArgY]] ]                              loop_header:[[HeaderY]]
  // CHECK-DAG:     [[Add7]]       Add [ [[PhiX]] [[Cst7]] ]                    loop_header:[[HeaderY]]
  //
  // CHECK-DAG:                    If [ [[ArgZ]] ]                              loop_header:null
  // CHECK-DAG:                    Return [ [[PhiX]] ]                          loop_header:null


  // CHECK-START: int TestCase.testExitPredecessors(int, boolean, boolean) dead_code_elimination_final (before)
  // CHECK-DAG:     [[ArgX:i\d+]]  ParameterValue
  // CHECK-DAG:     [[ArgY:z\d+]]  ParameterValue
  // CHECK-DAG:     [[ArgZ:z\d+]]  ParameterValue
  // CHECK-DAG:     [[Cst1:i\d+]]  IntConstant 1
  // CHECK-DAG:     [[Cst5:i\d+]]  IntConstant 5
  // CHECK-DAG:     [[Cst7:i\d+]]  IntConstant 7
  // CHECK-DAG:     [[Cst9:i\d+]]  IntConstant 9
  //
  // CHECK-DAG:     [[PhiX1:i\d+]] Phi [ [[ArgX]] [[Add5:i\d+]] [[Add7:i\d+]] ] loop_header:[[HeaderY:B\d+]]
  // CHECK-DAG:                    If [ [[ArgY]] ]                              loop_header:[[HeaderY]]
  // CHECK-DAG:                    If [ [[ArgZ]] ]                              loop_header:[[HeaderY]]
  // CHECK-DAG:     [[Mul9:i\d+]]  Mul [ [[PhiX1]] [[Cst9]] ]                   loop_header:[[HeaderY]]
  // CHECK-DAG:     [[PhiX2:i\d+]] Phi [ [[Mul9]] [[PhiX1]] ]                   loop_header:[[HeaderY]]
  // CHECK-DAG:                    If [ [[Cst1]] ]                              loop_header:[[HeaderY]]
  // CHECK-DAG:     [[Add5]]       Add [ [[PhiX2]] [[Cst5]] ]                   loop_header:[[HeaderY]]
  // CHECK-DAG:     [[Add7]]       Add [ [[PhiX1]] [[Cst7]] ]                   loop_header:[[HeaderY]]
  // CHECK-DAG:                    Return [ [[PhiX2]] ]                         loop_header:null

  // CHECK-START: int TestCase.testExitPredecessors(int, boolean, boolean) dead_code_elimination_final (after)
  // CHECK-DAG:     [[ArgX:i\d+]]  ParameterValue
  // CHECK-DAG:     [[ArgY:z\d+]]  ParameterValue
  // CHECK-DAG:     [[ArgZ:z\d+]]  ParameterValue
  // CHECK-DAG:     [[Cst7:i\d+]]  IntConstant 7
  // CHECK-DAG:     [[Cst9:i\d+]]  IntConstant 9
  //
  // CHECK-DAG:     [[PhiX1:i\d+]] Phi [ [[ArgX]] [[Add7:i\d+]] ]               loop_header:[[HeaderY:B\d+]]
  // CHECK-DAG:                    If [ [[ArgY]] ]                              loop_header:[[HeaderY]]
  // CHECK-DAG:     [[Add7]]       Add [ [[PhiX1]] [[Cst7]] ]                   loop_header:[[HeaderY]]
  //
  // CHECK-DAG:                    If [ [[ArgZ]] ]                              loop_header:null
  // CHECK-DAG:     [[Mul9:i\d+]]  Mul [ [[PhiX1]] [[Cst9]] ]                   loop_header:null
  // CHECK-DAG:     [[PhiX2:i\d+]] Phi [ [[Mul9]] [[PhiX1]] ]                   loop_header:null
  // CHECK-DAG:                    Return [ [[PhiX2]] ]                         loop_header:null


  // CHECK-START: int TestCase.testInnerLoop(int, boolean, boolean) dead_code_elimination_final (before)
  // CHECK-DAG:     [[ArgX:i\d+]]  ParameterValue
  // CHECK-DAG:     [[ArgY:z\d+]]  ParameterValue
  // CHECK-DAG:     [[ArgZ:z\d+]]  ParameterValue
  // CHECK-DAG:     [[Cst0:i\d+]]  IntConstant 0
  // CHECK-DAG:     [[Cst1:i\d+]]  IntConstant 1
  // CHECK-DAG:     [[Cst5:i\d+]]  IntConstant 5
  // CHECK-DAG:     [[Cst7:i\d+]]  IntConstant 7
  //
  // CHECK-DAG:     [[PhiX:i\d+]]  Phi [ [[ArgX]] [[Add5:i\d+]] [[Add7:i\d+]] ] loop_header:[[HeaderY:B\d+]]
  // CHECK-DAG:     [[PhiZ1:i\d+]] Phi [ [[ArgZ]] [[XorZ:i\d+]] [[PhiZ1]] ]     loop_header:[[HeaderY]]
  // CHECK-DAG:                    If [ [[ArgY]] ]                              loop_header:[[HeaderY]]
  //
  //                               ### Inner loop ###
  // CHECK-DAG:     [[PhiZ2:i\d+]] Phi [ [[PhiZ1]] [[XorZ]] ]                   loop_header:[[HeaderZ:B\d+]]
  // CHECK-DAG:     [[XorZ]]       Xor [ [[PhiZ2]] [[Cst1]] ]                   loop_header:[[HeaderZ]]
  // CHECK-DAG:     [[CondZ:z\d+]] Equal [ [[XorZ]] [[Cst0]] ]                  loop_header:[[HeaderZ]]
  // CHECK-DAG:                    If [ [[CondZ]] ]                             loop_header:[[HeaderZ]]
  //
  // CHECK-DAG:     [[Add5]]       Add [ [[PhiX]] [[Cst5]] ]                    loop_header:[[HeaderY]]
  // CHECK-DAG:     [[Add7]]       Add [ [[PhiX]] [[Cst7]] ]                    loop_header:[[HeaderY]]
  // CHECK-DAG:                    Return [ [[PhiX]] ]                          loop_header:null

  // CHECK-START: int TestCase.testInnerLoop(int, boolean, boolean) dead_code_elimination_final (after)
  // CHECK-DAG:     [[ArgX:i\d+]]  ParameterValue
  // CHECK-DAG:     [[ArgY:z\d+]]  ParameterValue
  // CHECK-DAG:     [[ArgZ:z\d+]]  ParameterValue
  // CHECK-DAG:     [[Cst0:i\d+]]  IntConstant 0
  // CHECK-DAG:     [[Cst1:i\d+]]  IntConstant 1
  // CHECK-DAG:     [[Cst7:i\d+]]  IntConstant 7
  //
  // CHECK-DAG:     [[PhiX:i\d+]]  Phi [ [[ArgX]] [[Add7:i\d+]] ]               loop_header:[[HeaderY:B\d+]]
  // CHECK-DAG:     [[PhiZ1:i\d+]] Phi [ [[ArgZ]] [[PhiZ1]] ]                   loop_header:[[HeaderY]]
  // CHECK-DAG:                    If [ [[ArgY]] ]                              loop_header:[[HeaderY]]
  // CHECK-DAG:     [[Add7]]       Add [ [[PhiX]] [[Cst7]] ]                    loop_header:[[HeaderY]]
  //
  //                               ### Inner loop ###
  // CHECK-DAG:     [[PhiZ2:i\d+]] Phi [ [[PhiZ1]] [[XorZ:i\d+]] ]              loop_header:[[HeaderZ:B\d+]]
  // CHECK-DAG:     [[XorZ]]       Xor [ [[PhiZ2]] [[Cst1]] ]                   loop_header:[[HeaderZ]]
  // CHECK-DAG:     [[CondZ:z\d+]] Equal [ [[XorZ]] [[Cst0]] ]                  loop_header:[[HeaderZ]]
  // CHECK-DAG:                    If [ [[CondZ]] ]                             loop_header:[[HeaderZ]]
  //
  // CHECK-DAG:                    Return [ [[PhiX]] ]                          loop_header:null

  public static void main(String[] args) throws Exception {
    return;
  }
}
