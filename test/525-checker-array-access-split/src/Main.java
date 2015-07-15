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

  public static void assertIntEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  /**
   * Test that HArrayGet with a constant index is not split.
   */

  /// CHECK-ARM64-START: int Main.constantIndexGet(int[]) instruction_simplifier_arch (before)
  /// CHECK-ARM64:       <<Array:l\d+>>         NullCheck
  /// CHECK-ARM64:       <<Index:i\d+>>         BoundsCheck
  /// CHECK-ARM64:                              ArrayGet [<<Array>>,<<Index>>]

  /// CHECK-ARM64-START: int Main.constantIndexGet(int[]) instruction_simplifier_arch (after)
  /// CHECK-ARM64:       <<Array:l\d+>>         NullCheck
  /// CHECK-ARM64:       <<Index:i\d+>>         BoundsCheck
  /// CHECK-ARM64-NOT:                          Arm64IntermediateAddress
  /// CHECK-ARM64:                              ArrayGet [<<Array>>,<<Index>>]

  public static int constantIndexGet(int array[]) {
    return array[1];
  }

  /**
   * Test that HArraySet with a constant index is not split.
   */

  /// CHECK-ARM64-START: void Main.constantIndexSet(int[]) instruction_simplifier_arch (before)
  /// CHECK-ARM64:       <<Const2:i\d+>>        IntConstant 2
  /// CHECK-ARM64:       <<Array:l\d+>>         NullCheck
  /// CHECK-ARM64:       <<Index:i\d+>>         BoundsCheck
  /// CHECK-ARM64:                              ArraySet [<<Array>>,<<Index>>,<<Const2>>]

  /// CHECK-ARM64-START: void Main.constantIndexSet(int[]) instruction_simplifier_arch (after)
  /// CHECK-ARM64:       <<Const2:i\d+>>        IntConstant 2
  /// CHECK-ARM64:       <<Array:l\d+>>         NullCheck
  /// CHECK-ARM64:       <<Index:i\d+>>         BoundsCheck
  /// CHECK-ARM64-NOT:                          Arm64IntermediateAddress
  /// CHECK-ARM64:                              ArraySet [<<Array>>,<<Index>>,<<Const2>>]


  public static void constantIndexSet(int array[]) {
    array[1] = 2;
  }

  /**
   * Test basic splitting of HArrayGet.
   */

  /// CHECK-ARM64-START: int Main.get(int[], int) instruction_simplifier_arch (before)
  /// CHECK-ARM64:       <<Array:l\d+>>         NullCheck
  /// CHECK-ARM64:       <<Index:i\d+>>         BoundsCheck
  /// CHECK-ARM64:                              ArrayGet [<<Array>>,<<Index>>]

  /// CHECK-ARM64-START: int Main.get(int[], int) instruction_simplifier_arch (after)
  /// CHECK-ARM64:       <<DataOffset:i\d+>>    IntConstant
  /// CHECK-ARM64:       <<Array:l\d+>>         NullCheck
  /// CHECK-ARM64:       <<Index:i\d+>>         BoundsCheck
  /// CHECK-ARM64:       <<Address:l\d+>>       Arm64IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-ARM64-NEXT:                         ArrayGet [<<Address>>,<<Index>>]

  public static int get(int array[], int index) {
    return array[index];
  }

  /**
   * Test basic splitting of HArraySet.
   */

  /// CHECK-ARM64-START: void Main.set(int[], int, int) instruction_simplifier_arch (before)
  /// CHECK-ARM64:                              ParameterValue
  /// CHECK-ARM64:                              ParameterValue
  /// CHECK-ARM64:       <<Arg:i\d+>>           ParameterValue
  /// CHECK-ARM64:       <<Array:l\d+>>         NullCheck
  /// CHECK-ARM64:       <<Index:i\d+>>         BoundsCheck
  /// CHECK-ARM64:                              ArraySet [<<Array>>,<<Index>>,<<Arg>>]

  /// CHECK-ARM64-START: void Main.set(int[], int, int) instruction_simplifier_arch (after)
  /// CHECK-ARM64:                              ParameterValue
  /// CHECK-ARM64:                              ParameterValue
  /// CHECK-ARM64:       <<Arg:i\d+>>           ParameterValue
  /// CHECK-ARM64:       <<DataOffset:i\d+>>    IntConstant
  /// CHECK-ARM64:       <<Array:l\d+>>         NullCheck
  /// CHECK-ARM64:       <<Index:i\d+>>         BoundsCheck
  /// CHECK-ARM64:       <<Address:l\d+>>       Arm64IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-ARM64-NEXT:                         ArraySet [<<Address>>,<<Index>>,<<Arg>>]

  public static void set(int array[], int index, int value) {
    array[index] = value;
  }

  /**
   * Check that the intermediate address can be shared after GVN.
   */

  /// CHECK-ARM64-START: void Main.getSet(int[], int) instruction_simplifier_arch (before)
  /// CHECK-ARM64:       <<Const1:i\d+>>        IntConstant 1
  /// CHECK-ARM64:       <<Array:l\d+>>         NullCheck
  /// CHECK-ARM64:       <<Index:i\d+>>         BoundsCheck
  /// CHECK-ARM64:       <<ArrayGet:i\d+>>      ArrayGet [<<Array>>,<<Index>>]
  /// CHECK-ARM64:       <<Add:i\d+>>           Add [<<ArrayGet>>,<<Const1>>]
  /// CHECK-ARM64:                              ArraySet [<<Array>>,<<Index>>,<<Add>>]

  /// CHECK-ARM64-START: void Main.getSet(int[], int) instruction_simplifier_arch (after)
  /// CHECK-ARM64-DAG:   <<Const1:i\d+>>        IntConstant 1
  /// CHECK-ARM64-DAG:   <<DataOffset:i\d+>>    IntConstant
  /// CHECK-ARM64:       <<Array:l\d+>>         NullCheck
  /// CHECK-ARM64:       <<Index:i\d+>>         BoundsCheck
  /// CHECK-ARM64:       <<Address1:l\d+>>      Arm64IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-ARM64-NEXT:  <<ArrayGet:i\d+>>      ArrayGet [<<Address1>>,<<Index>>]
  /// CHECK-ARM64:       <<Add:i\d+>>           Add [<<ArrayGet>>,<<Const1>>]
  /// CHECK-ARM64:       <<Address2:l\d+>>      Arm64IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-ARM64-NEXT:                         ArraySet [<<Address2>>,<<Index>>,<<Add>>]

  /// CHECK-ARM64-START: void Main.getSet(int[], int) GVN_after_arch (after)
  /// CHECK-ARM64-DAG:   <<Const1:i\d+>>        IntConstant 1
  /// CHECK-ARM64-DAG:   <<DataOffset:i\d+>>    IntConstant
  /// CHECK-ARM64:       <<Array:l\d+>>         NullCheck
  /// CHECK-ARM64:       <<Index:i\d+>>         BoundsCheck
  /// CHECK-ARM64:       <<Address:l\d+>>       Arm64IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-ARM64:       <<ArrayGet:i\d+>>      ArrayGet [<<Address>>,<<Index>>]
  /// CHECK-ARM64:       <<Add:i\d+>>           Add [<<ArrayGet>>,<<Const1>>]
  /// CHECK-ARM64-NOT:                          Arm64IntermediateAddress
  /// CHECK-ARM64:                              ArraySet [<<Address>>,<<Index>>,<<Add>>]

  public static void getSet(int array[], int index) {
    array[index] = array[index] + 1;
  }

  /**
   * Check that the intermediate address computation is not reordered or merged
   * across IRs that can trigger GC.
   */

  /// CHECK-ARM64-START: int[] Main.accrossGC(int[], int) instruction_simplifier_arch (before)
  /// CHECK-ARM64:       <<Const1:i\d+>>        IntConstant 1
  /// CHECK-ARM64:       <<Array:l\d+>>         NullCheck
  /// CHECK-ARM64:       <<Index:i\d+>>         BoundsCheck
  /// CHECK-ARM64:       <<ArrayGet:i\d+>>      ArrayGet [<<Array>>,<<Index>>]
  /// CHECK-ARM64:       <<Add:i\d+>>           Add [<<ArrayGet>>,<<Const1>>]
  /// CHECK-ARM64:                              NewArray
  /// CHECK-ARM64:                              ArraySet [<<Array>>,<<Index>>,<<Add>>]

  /// CHECK-ARM64-START: int[] Main.accrossGC(int[], int) instruction_simplifier_arch (after)
  /// CHECK-ARM64-DAG:   <<Const1:i\d+>>        IntConstant 1
  /// CHECK-ARM64-DAG:   <<DataOffset:i\d+>>    IntConstant
  /// CHECK-ARM64:       <<Array:l\d+>>         NullCheck
  /// CHECK-ARM64:       <<Index:i\d+>>         BoundsCheck
  /// CHECK-ARM64:       <<Address1:l\d+>>      Arm64IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-ARM64-NEXT:  <<ArrayGet:i\d+>>      ArrayGet [<<Address1>>,<<Index>>]
  /// CHECK-ARM64:       <<Add:i\d+>>           Add [<<ArrayGet>>,<<Const1>>]
  /// CHECK-ARM64:                              NewArray
  /// CHECK-ARM64:       <<Address2:l\d+>>      Arm64IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-ARM64-NEXT:                         ArraySet [<<Address2>>,<<Index>>,<<Add>>]

  /// CHECK-ARM64-START: int[] Main.accrossGC(int[], int) GVN_after_arch (after)
  /// CHECK-ARM64-DAG:   <<Const1:i\d+>>        IntConstant 1
  /// CHECK-ARM64-DAG:   <<DataOffset:i\d+>>    IntConstant
  /// CHECK-ARM64:       <<Array:l\d+>>         NullCheck
  /// CHECK-ARM64:       <<Index:i\d+>>         BoundsCheck
  /// CHECK-ARM64:       <<Address1:l\d+>>      Arm64IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-ARM64:       <<ArrayGet:i\d+>>      ArrayGet [<<Address1>>,<<Index>>]
  /// CHECK-ARM64:       <<Add:i\d+>>           Add [<<ArrayGet>>,<<Const1>>]
  /// CHECK-ARM64:                              NewArray
  /// CHECK-ARM64:       <<Address2:l\d+>>      Arm64IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-ARM64:                              ArraySet [<<Address2>>,<<Index>>,<<Add>>]

  public static int[] accrossGC(int array[], int index) {
    int tmp = array[index] + 1;
    int[] new_array = new int[1];
    array[index] = tmp;
    return new_array;
  }

  /**
   * Test that the intermediate address is shared between array accesses after
   * the bounds check have been removed by BCE.
   */

  /// CHECK-ARM64-START: int Main.canMergeAfterBCE1() instruction_simplifier_arch (before)
  /// CHECK-ARM64:       <<Const1:i\d+>>        IntConstant 1
  /// CHECK-ARM64:       <<Array:l\d+>>         NewArray
  /// CHECK-ARM64:       <<Index:i\d+>>         Phi
  /// CHECK-ARM64:                              If
  //  -------------- Loop
  /// CHECK-ARM64:       <<ArrayGet:i\d+>>      ArrayGet [<<Array>>,<<Index>>]
  /// CHECK-ARM64:       <<Add:i\d+>>           Add [<<ArrayGet>>,<<Const1>>]
  /// CHECK-ARM64:                              ArraySet [<<Array>>,<<Index>>,<<Add>>]

  // By the time we reach the architecture-specific instruction simplifier, BCE
  // has removed the bounds checks in the loop.

  // Note that we do not care that the `DataOffset` is `12`. But if we do not
  // specify it and any other `IntConstant` appears before that instruction,
  // checker will match the previous `IntConstant`, and we will thus fail the
  // check.

  /// CHECK-ARM64-START: int Main.canMergeAfterBCE1() instruction_simplifier_arch (after)
  /// CHECK-ARM64-DAG:   <<Const1:i\d+>>        IntConstant 1
  /// CHECK-ARM64-DAG:   <<DataOffset:i\d+>>    IntConstant 12
  /// CHECK-ARM64:       <<Array:l\d+>>         NewArray
  /// CHECK-ARM64:       <<Index:i\d+>>         Phi
  /// CHECK-ARM64:                              If
  //  -------------- Loop
  /// CHECK-ARM64:       <<Address1:l\d+>>      Arm64IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-ARM64-NEXT:  <<ArrayGet:i\d+>>      ArrayGet [<<Address1>>,<<Index>>]
  /// CHECK-ARM64:       <<Add:i\d+>>           Add [<<ArrayGet>>,<<Const1>>]
  /// CHECK-ARM64:       <<Address2:l\d+>>      Arm64IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-ARM64-NEXT:                         ArraySet [<<Address2>>,<<Index>>,<<Add>>]

  /// CHECK-ARM64-START: int Main.canMergeAfterBCE1() GVN_after_arch (after)
  /// CHECK-ARM64-DAG:   <<Const1:i\d+>>        IntConstant 1
  /// CHECK-ARM64-DAG:   <<DataOffset:i\d+>>    IntConstant 12
  /// CHECK-ARM64:       <<Array:l\d+>>         NewArray
  /// CHECK-ARM64:       <<Index:i\d+>>         Phi
  /// CHECK-ARM64:                              If
  //  -------------- Loop
  /// CHECK-ARM64:       <<Address:l\d+>>       Arm64IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-ARM64:       <<ArrayGet:i\d+>>      ArrayGet [<<Address>>,<<Index>>]
  /// CHECK-ARM64:       <<Add:i\d+>>           Add [<<ArrayGet>>,<<Const1>>]
  /// CHECK-ARM64-NOT:                          Arm64IntermediateAddress
  /// CHECK-ARM64:                              ArraySet [<<Address>>,<<Index>>,<<Add>>]

  public static int canMergeAfterBCE1() {
    int[] array = {0, 1, 2, 3};
    for (int i = 0; i < array.length; i++) {
      array[i] = array[i] + 1;
    }
    return array[array.length - 1];
  }

  /**
   * This test case is similar to `canMergeAfterBCE1`, but with different
   * indexes for the accesses.
   */

  /// CHECK-ARM64-START: int Main.canMergeAfterBCE2() instruction_simplifier_arch (before)
  /// CHECK-ARM64:       <<Const1:i\d+>>        IntConstant 1
  /// CHECK-ARM64:       <<Array:l\d+>>         NewArray
  /// CHECK-ARM64:       <<Index:i\d+>>         Phi
  /// CHECK-ARM64:                              If
  //  -------------- Loop
  /// CHECK-ARM64-DAG:   <<Index1:i\d+>>        Add [<<Index>>,<<Const1>>]
  /// CHECK-ARM64-DAG:   <<ArrayGetI:i\d+>>     ArrayGet [<<Array>>,<<Index>>]
  /// CHECK-ARM64-DAG:   <<ArrayGetI1:i\d+>>    ArrayGet [<<Array>>,<<Index1>>]
  /// CHECK-ARM64:       <<Add:i\d+>>           Add [<<ArrayGetI>>,<<ArrayGetI1>>]
  /// CHECK-ARM64:                              ArraySet [<<Array>>,<<Index1>>,<<Add>>]

  // Note that we do not care that the `DataOffset` is `12`. But if we do not
  // specify it and any other `IntConstant` appears before that instruction,
  // checker will match the previous `IntConstant`, and we will thus fail the
  // check.

  // TODO

  /// CHECK-ARM64-START: int Main.canMergeAfterBCE2() instruction_simplifier_arch (after)
  /// CHECK-ARM64-DAG:   <<Const1:i\d+>>        IntConstant 1
  /// CHECK-ARM64-DAG:   <<DataOffset:i\d+>>    IntConstant 12
  /// CHECK-ARM64:       <<Array:l\d+>>         NewArray
  /// CHECK-ARM64:       <<Index:i\d+>>         Phi
  /// CHECK-ARM64:                              If
  //  -------------- Loop
  /// CHECK-ARM64-DAG:   <<Index1:i\d+>>        Add [<<Index>>,<<Const1>>]
  /// CHECK-ARM64-DAG:   <<Address1:l\d+>>      Arm64IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-ARM64-DAG:   <<ArrayGetI:i\d+>>     ArrayGet [<<Address1>>,<<Index>>]
  /// CHECK-ARM64-DAG:   <<Address2:l\d+>>      Arm64IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-ARM64-DAG:   <<ArrayGetI1:i\d+>>    ArrayGet [<<Address2>>,<<Index1>>]
  /// CHECK-ARM64:       <<Add:i\d+>>           Add [<<ArrayGetI>>,<<ArrayGetI1>>]
  /// CHECK-ARM64:       <<Address3:l\d+>>      Arm64IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-ARM64:                              ArraySet [<<Address3>>,<<Index1>>,<<Add>>]

  /// CHECK-ARM64-START: int Main.canMergeAfterBCE2() GVN_after_arch (after)
  /// CHECK-ARM64-DAG:   <<Const1:i\d+>>        IntConstant 1
  /// CHECK-ARM64-DAG:   <<DataOffset:i\d+>>    IntConstant 12
  /// CHECK-ARM64:       <<Array:l\d+>>         NewArray
  /// CHECK-ARM64:       <<Index:i\d+>>         Phi
  /// CHECK-ARM64:                              If
  //  -------------- Loop
  /// CHECK-ARM64-DAG:   <<Index1:i\d+>>        Add [<<Index>>,<<Const1>>]
  /// CHECK-ARM64-DAG:   <<Address:l\d+>>       Arm64IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-ARM64-DAG:   <<ArrayGetI:i\d+>>     ArrayGet [<<Address>>,<<Index>>]
  /// CHECK-ARM64-DAG:   <<ArrayGetI1:i\d+>>    ArrayGet [<<Address>>,<<Index1>>]
  /// CHECK-ARM64:       <<Add:i\d+>>           Add [<<ArrayGetI>>,<<ArrayGetI1>>]
  /// CHECK-ARM64:                              ArraySet [<<Address>>,<<Index1>>,<<Add>>]

  /// CHECK-ARM64-START: int Main.canMergeAfterBCE2() GVN_after_arch (after)
  /// CHECK-ARM64:                              Arm64IntermediateAddress
  /// CHECK-ARM64-NOT:                          Arm64IntermediateAddress

  public static int canMergeAfterBCE2() {
    int[] array = {0, 1, 2, 3};
    for (int i = 0; i < array.length - 1; i++) {
      array[i + 1] = array[i] + array[i + 1];
    }
    return array[array.length - 1];
  }


  public static void main(String[] args) {
    int[] array = {123, 456, 789};

    assertIntEquals(456, constantIndexGet(array));

    constantIndexSet(array);
    assertIntEquals(2, array[1]);

    assertIntEquals(789, get(array, 2));

    set(array, 1, 456);
    assertIntEquals(456, array[1]);

    getSet(array, 0);
    assertIntEquals(124, array[0]);

    accrossGC(array, 0);
    assertIntEquals(125, array[0]);

    assertIntEquals(4, canMergeAfterBCE1());
    assertIntEquals(6, canMergeAfterBCE2());
  }
}
