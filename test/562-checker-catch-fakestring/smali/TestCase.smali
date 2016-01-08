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

## CHECK-START: void TestCase.method() instruction_simplifier (before)
## CHECK-DAG:     <<NullCst:l\d+>> NullConstant
## CHECK-DAG:     <<ZeroCst:i\d+>> IntConstant 0
## CHECK-DAG:     <<FakeStr:l\d+>> FakeString
## CHECK-DAG:     <<UTF8Str:l\d+>> LoadString
## CHECK-DAG:                      InvokeStaticOrDirect [<<NullCst>>,<<UTF8Str>>,<<FakeStr>>] env:[[<<FakeStr>>,<<ZeroCst>>,<<UTF8Str>>]]
## CHECK-DAG:                      Phi [<<NullCst>>,<<NullCst>>,<<NullCst>>,<<FakeStr>>] reg:0 is_catch_phi:true

## CHECK-START: void TestCase.method() instruction_simplifier (after)
## CHECK-DAG:     <<NullCst:l\d+>> NullConstant
## CHECK-DAG:     <<ZeroCst:i\d+>> IntConstant 0
## CHECK-DAG:     <<UTF8Str:l\d+>> LoadString
## CHECK-DAG:     <<RealStr:l\d+>> InvokeStaticOrDirect [<<NullCst>>,<<UTF8Str>>] env:[[<<NullCst>>,<<ZeroCst>>,<<UTF8Str>>]]
## CHECK-DAG:                      Phi [<<NullCst>>,<<NullCst>>,<<NullCst>>,<<RealStr>>] reg:0 is_catch_phi:true

.method public static method()V
   .registers 3

   const v0, 0x0
   :try_start
   invoke-static {}, Ljava/lang/System;->nanoTime()J

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
