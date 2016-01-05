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

.class public LIrreducibleLoop;

.super Ljava/lang/Object;

# Test that we support a simple irreducible loop.

## CHECK-START: int IrreducibleLoop.simpleLoop(int) dead_code_elimination (before)
## CHECK: Goto loop:B4 irreducible:true
.method public static simpleLoop(I)I
   .registers 2
   const/16 v0, 42
   if-eq v1, v0, :other_loop_entry
   :loop_entry
   if-ne v1, v0, :exit
   add-int v0, v0, v0
   :other_loop_entry
   add-int v0, v0, v0
   goto :loop_entry
   :exit
   return v0
.end method

# Test that lse does not wrongly optimize loads in irreducible loops. At the
# SSA level, since we create redundant phis for irreducible loop headers, lse
# does not see the relation between the dex register and the phi.

## CHECK-START: int IrreducibleLoop.lse(int, Main) load_store_elimination (after)
## CHECK: InstanceFieldGet
.method public static lse(ILMain;)I
   .registers 4
   const/16 v0, 42
   const/16 v1, 30
   if-eq p0, v0, :other_loop_pre_entry
   iput v0, p1, LMain;->myField:I
   :loop_entry
   if-ne v1, v0, :exit
   :other_loop_entry
   iget v0, p1, LMain;->myField:I
   if-eq v1, v0, :exit
   goto :loop_entry
   :exit
   return v0
   :other_loop_pre_entry
   iput v1, p1, LMain;->myField:I
   goto :other_loop_entry
.end method

# Check that if a irreducible loop entry is dead, the loop can become
# natural.

## CHECK-START: int IrreducibleLoop.dce(int) dead_code_elimination (before)
## CHECK: Goto loop:B4 irreducible:true

## CHECK-START: int IrreducibleLoop.dce(int) dead_code_elimination (after)
## CHECK: Add loop:B2 irreducible:false
.method public static dce(I)I
   .registers 3
   const/16 v0, 42
   const/16 v1, 168
   if-ne v0, v0, :other_loop_pre_entry
   :loop_entry
   if-ne v0, v0, :exit
   add-int v0, v0, v0
   :other_loop_entry
   add-int v0, v0, v0
   if-eq v0, v1, :exit
   goto :loop_entry
   :exit
   return v0
   :other_loop_pre_entry
   add-int v0, v0, v0
   goto :other_loop_entry
.end method


# Check that a dex register only used in the loop header remains live thanks
# to the (redundant) Phi created at the loop header for it.

## CHECK-START: int IrreducibleLoop.liveness(int) liveness (after)
## CHECK-DAG: <<Arg:i\d+>>      ParameterValue liveness:<<ArgLiv:\d+>> ranges:{[<<ArgLiv>>,<<ArgLoopPhiUse:\d+>>)}
## CHECK-DAG: <<LoopPhi:i\d+>>  Phi [<<Arg>>,<<PhiInLoop:i\d+>>] liveness:<<ArgLoopPhiUse>> ranges:{[<<ArgLoopPhiUse>>,<<PhiInLoopUse:\d+>>)}
## CHECK-DAG: <<PhiInLoop>>     Phi [<<Arg>>,<<LoopPhi>>] liveness:<<PhiInLoopUse>> ranges:{[<<PhiInLoopUse>>,<<BackEdgeLifetimeEnd:\d+>>)}
## CHECK:                       Return liveness:<<ReturnLiviness:\d+>>
## CHECK-EVAL:    <<ReturnLiviness>> == <<BackEdgeLifetimeEnd>> + 2
.method public static liveness(I)I
   .registers 2
   const/16 v0, 42
   if-eq p0, v0, :other_loop_entry
   :loop_entry
   add-int v0, v0, p0
   if-ne v1, v0, :exit
   :other_loop_entry
   add-int v0, v0, v0
   goto :loop_entry
   :exit
   return v0
.end method

## CHECK-START: java.lang.Class IrreducibleLoop.gvn() GVN (before)
## CHECK: LoadClass
## CHECK: LoadClass
## CHECK: LoadClass
## CHECK-NOT: LoadClass

## CHECK-START: java.lang.Class IrreducibleLoop.gvn() GVN (before)
## CHECK: LoadClass
## CHECK: LoadClass
## CHECK: LoadClass
## CHECK-NOT: LoadClass

.method public static gvn()Ljava/lang/Class;
  .registers 3
  const/4 v2, 0
  const-class v0, LMain;
  if-ne v0, v2, :other_loop_entry
  :loop_entry
  const-class v0, LMain;
  if-ne v0, v2, :exit
  :other_loop_entry
  const-class v1, LIrreducibleLoop;
  goto :loop_entry
  :exit
  return-object v0
.end method

## CHECK-START: int IrreducibleLoop.licm1(int) licm (after)
## CHECK: Add loop:B3 irreducible:true
.method public static licm1(I)I
  .registers 3
  const/4 v0, 0
  if-ne p0, v0, :other_loop_entry
  :loop_entry
  add-int v0, p0, p0
  if-ne v0, p0, :exit
  :other_loop_entry
  sub-int v1, p0, p0
  goto :loop_entry
  :exit
  sub-int v0, v0, p0
  return v0
.end method

## CHECK-START: int IrreducibleLoop.licm2(int) licm (after)
## CHECK: LoadClass loop:B3 irreducible:true
.method public static licm2(I)I
  .registers 3
  const/4 v0, 0
  if-ne p0, v0, :other_loop_entry
  :loop_entry
  const-class v1, LIrreducibleLoop;
  if-ne v0, p0, :exit
  :other_loop_entry
  sub-int v1, p0, p0
  goto :loop_entry
  :exit
  sub-int v0, v0, p0
  return v0
.end method

# Check a loop within an irreducible loop
## CHECK-START: void IrreducibleLoop.analyze1(int) ssa_builder (after)
## CHECK-DAG: Goto loop:B3 outer_loop:none irreducible:true
## CHECK-DAG: Goto loop:B5 outer_loop:B3 irreducible:false
.method public static analyze1(I)V
  .registers 1
  if-eq p0, p0, :irreducible_loop_entry
  goto :irreducible_loop_pre_other_entry

  :irreducible_loop_entry
  if-eq p0, p0, :exit
  goto :irreducible_loop_body

  :irreducible_loop_body
  :loop_within_header
  if-eq p0, p0, :irreducible_loop_back_edge
  goto :loop_within_back_edge

  :loop_within_back_edge
  goto :loop_within_header

  :irreducible_loop_back_edge
  goto :irreducible_loop_entry

  :irreducible_loop_pre_other_entry
  goto :irreducible_loop_body

  :exit
  return-void
.end method

# Check than a loop before an irreducible loop is not part of the
# irreducible loop.
## CHECK-START: void IrreducibleLoop.analyze2(int) ssa_builder (after)
## CHECK-DAG: Goto loop:B1 outer_loop:none irreducible:false
## CHECK-DAG: Goto loop:B7 outer_loop:none irreducible:true
.method public static analyze2(I)V
  .registers 1
  :loop_header
  if-eq p0, p0, :irreducible_loop_pre_entry
  goto :loop_header

  :irreducible_loop_pre_entry
  if-eq p0, p0, :irreducible_loop_other_pre_entry
  goto :irreducible_loop_entry

  :irreducible_loop_entry
  if-eq p0, p0, :exit
  goto :irreducible_loop_body

  :irreducible_loop_body
  goto :irreducible_loop_entry

  :irreducible_loop_other_pre_entry
  goto :irreducible_loop_body

  :exit
  return-void
.end method

# Check two irreducible loops, one within another.
## CHECK-START: void IrreducibleLoop.analyze3(int) ssa_builder (after)
## CHECK-DAG: Goto loop:B4 outer_loop:none irreducible:true
## CHECK-DAG: Goto loop:B5 outer_loop:B4 irreducible:true
.method public static analyze3(I)V
  .registers 1
  if-eq p0, p0, :loop2_header
  goto :loop1_header

  :loop1_header
  goto :loop2_body

  :loop2_header
  goto :loop2_body

  :loop2_body
  if-eq p0, p0, :loop2_back_edge
  goto :loop1_body

  :loop2_back_edge
  goto :loop2_header

  :loop1_body
  if-eq p0, p0, :exit
  goto :loop1_header

  :exit
  return-void
.end method

# Check two irreducible loops, one within another.
## CHECK-START: void IrreducibleLoop.analyze4(int) ssa_builder (after)
## CHECK-DAG: Goto loop:B3 outer_loop:none irreducible:true
## CHECK-DAG: Goto loop:B5 outer_loop:B3 irreducible:true
.method public static analyze4(I)V
  .registers 1
  if-eq p0, p0, :loop1_header
  goto :loop2_header

  :loop1_header
  goto :loop2_body

  :loop2_header
  goto :loop2_body

  :loop2_body
  if-eq p0, p0, :loop2_back_edge
  goto :loop1_body

  :loop2_back_edge
  goto :loop2_header

  :loop1_body
  if-eq p0, p0, :exit
  goto :loop1_header

  :exit
  return-void
.end method

# Check two irreducible loops, one within another.
## CHECK-START: void IrreducibleLoop.analyze5(int) ssa_builder (after)
## CHECK-DAG: Goto loop:B3 outer_loop:none irreducible:true
## CHECK-DAG: Goto loop:B5 outer_loop:B3 irreducible:true
.method public static analyze5(I)V
  .registers 1
  if-eq p0, p0, :loop1_header
  goto :loop2_header

  :loop1_header
  goto :loop2_body

  :loop2_header
  goto :loop2_body

  :loop2_body
  goto :loop2_back_edge

  :loop2_back_edge
  if-eq p0, p0, :loop2_header
  goto :loop1_body

  :loop1_body
  if-eq p0, p0, :exit
  goto :loop1_header

  :exit
  return-void
.end method
