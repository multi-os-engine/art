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

## CHECK-START-DEBUGGABLE: void TestCase.mergeSameType(int, int, int) ssa_builder (after)
## CHECK: {{i\d+}} Phi
.method public static mergeSameType(III)V
  .registers 8
  if-eqz p0, :after
  move p1, p2
  :after
  invoke-static {}, Ljava/lang/System;->nanoTime()J  # create an env use
  return-void
.end method

## CHECK-START-DEBUGGABLE: void TestCase.mergeType_IF(int, float) ssa_builder (after)
## CHECK: {{f\d+}} Phi
.method public static mergeType_IF(IF)V
  .registers 4

  const v0, 0xABCD
  if-eqz p0, :after
  move v0, p1
  :after
  invoke-static {}, Ljava/lang/System;->nanoTime()J  # create an env use
  return-void
.end method

## CHECK-START-DEBUGGABLE: void TestCase.mergeType_IL(int, java.lang.Object) ssa_builder (after)
## CHECK: {{l\d+}} Phi
.method public static mergeType_IL(ILjava/lang/Object;)V
  .registers 4

  const v0, 0x0
  if-eqz p0, :after
  move-object v0, p1
  :after
  invoke-static {}, Ljava/lang/System;->nanoTime()J  # create an env use
  return-void
.end method

## CHECK-START-DEBUGGABLE: void TestCase.mergeType_IJ(int, long) ssa_builder (after)
## CHECK-NOT: Phi
.method public static mergeType_IJ(IJ)V
  .registers 4

  const v0, 0x0
  if-eqz p0, :after
  move-wide v0, p1
  :after
  invoke-static {}, Ljava/lang/System;->nanoTime()J  # create an env use
  return-void
.end method

## CHECK-START-DEBUGGABLE: void TestCase.mergeType_LJ(int, java.lang.Object) ssa_builder (after)
## CHECK-NOT: Phi
.method public static mergeType_LJ(ILjava/lang/Object;)V
  .registers 4

  const-wide v0, 0xABCD
  if-eqz p0, :after
  move-object v0, p1
  :after
  invoke-static {}, Ljava/lang/System;->nanoTime()J  # create an env use
  return-void
.end method

## CHECK-START-DEBUGGABLE: void TestCase.mergeType_FL(int, float, java.lang.Object) ssa_builder (after)
## CHECK-NOT: Phi
.method public static mergeType_FL(IFLjava/lang/Object;)V
  .registers 4

  move v0, p1
  if-eqz p0, :after
  move-object v0, p2
  :after
  invoke-static {}, Ljava/lang/System;->nanoTime()J  # create an env use
  return-void
.end method

## CHECK-START-DEBUGGABLE: void TestCase.mergeType_FL(int, float, java.lang.Object) ssa_builder (after)
## CHECK-NOT: Phi
.method public static mergeType_FL(IFLjava/lang/Object;)V
  .registers 4

  move v0, p1
  if-eqz p0, :after
  move-object v0, p2
  :after
  invoke-static {}, Ljava/lang/System;->nanoTime()J  # create an env use
  return-void
.end method

## CHECK-START-DEBUGGABLE: void TestCase.mergeType_FL(int, float, java.lang.Object) ssa_builder (after)
## CHECK-NOT: Phi
.method public static mergeType_LF(IFLjava/lang/Object;)V
  .registers 4

  move-object v0, p2
  if-eqz p0, :after
  move v0, p1
  :after
  invoke-static {}, Ljava/lang/System;->nanoTime()J  # create an env use
  return-void
.end method
