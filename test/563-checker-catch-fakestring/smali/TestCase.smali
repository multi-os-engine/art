# Copyright (C) 2015 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

.class public LTestCase;
.super Ljava/lang/Object;

# Test that GraphBuilder updates all vregs holding new-instance after initialization.

## CHECK-START: java.lang.String TestCase.vregAliasing(byte[]) builder (after)
## CHECK:          <<Invoke:l\d+>> InvokeStaticOrDirect
## CHECK-NEXT:                     StoreLocal [v0,<<Invoke>>]
## CHECK-NEXT:                     StoreLocal [v1,<<Invoke>>]
## CHECK-NEXT:                     StoreLocal [v2,<<Invoke>>]

.method public static vregAliasing([B)Ljava/lang/String;
   .registers 5

   new-instance v0, Ljava/lang/String;
   move-object v1, v0
   move-object v2, v1
   const-string v3, "UTF8"
   invoke-direct {v0, p0, v3}, Ljava/lang/String;-><init>([BLjava/lang/String;)V

   return-object v2

.end method

# Test usage of String new-instance before it is initialized.

## CHECK-START: void TestCase.compareFakeString() register (after)
## CHECK-DAG:     <<Null:l\d+>>   NullConstant
## CHECK-DAG:     <<String:l\d+>> NewInstance
## CHECK-DAG:     <<Cond:z\d+>>   NotEqual [<<String>>,<<Null>>]
## CHECK-DAG:                     If [<<Cond>>]

.method public static compareFakeString()V
   .registers 3

   new-instance v0, Ljava/lang/String;
   if-nez v0, :return

   const v1, 0x0
   const-string v2, "UTF8"
   invoke-direct {v0, v1, v2}, Ljava/lang/String;-><init>([BLjava/lang/String;)V
   return-void

   :return
   return-void

.end method

# Test deoptimization between String's allocation and initialization.

## CHECK-START: int TestCase.deoptimizeFakeString(int[], byte[]) register (after)
## CHECK:         <<String:l\d+>> NewInstance
## CHECK:                         Deoptimize env:[[<<String>>,{{.*]]}}
## CHECK:                         InvokeStaticOrDirect method_name:java.lang.String.<init>

.method public static deoptimizeFakeString([I[B)I
   .registers 6

   new-instance v0, Ljava/lang/String;

   const v2, 0x0
   const v1, 0x1
   aget v1, p0, v1
   add-int/2addr v2, v1

   invoke-static {}, LMain;->assertIsInterpreted()V
   const-string v3, "UTF8"
   invoke-direct {v0, p1, v3}, Ljava/lang/String;-><init>([BLjava/lang/String;)V

   const v1, 0x4
   aget v1, p0, v1
   add-int/2addr v2, v1

   return v2

.end method

# Test throwing and catching an exception between String's allocation and initialization.

## CHECK-START: void TestCase.catchFakeString() register (after)
## CHECK-DAG:     <<Null:l\d+>>   NullConstant
## CHECK-DAG:     <<Zero:i\d+>>   IntConstant 0
## CHECK-DAG:     <<String:l\d+>> NewInstance
## CHECK-DAG:     <<UTF8:l\d+>>   LoadString
## CHECK-DAG:                     InvokeStaticOrDirect [<<Null>>,<<UTF8>>] env:[[<<String>>,<<Zero>>,<<UTF8>>]]
## CHECK-DAG:                     Phi [<<Null>>,<<Null>>,<<String>>] reg:0 is_catch_phi:true

.method public static catchFakeString()V
   .registers 3

   const v0, 0x0
   :try_start
   new-instance v0, Ljava/lang/String;
   const v1, 0x0
   const-string v2, "UTF8"

   invoke-direct {v0, v1, v2}, Ljava/lang/String;-><init>([BLjava/lang/String;)V
   :try_end
   .catchall {:try_start .. :try_end} :catch
   return-void

   :catch
   move-exception v1
   if-eqz v0, :throw
   return-void

   :throw
   throw v1

.end method
