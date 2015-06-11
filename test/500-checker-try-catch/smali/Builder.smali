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

.class public LBuilder;

.super Ljava/lang/Object;

# Test that all catch handlers are linked to the EnterTry/ExitTry blocks.

## CHECK-START: int Builder.testMultipleCatch(int, int) builder (after)
.method public static testMultipleCatch(II)I
    .registers 3

## CHECK:  name         "B0"
## CHECK:  successors   "<<BEnterTry:B\d+>>"
## CHECK:  <<ConstM1:i\d+>>  IntConstant -1
## CHECK:  <<ConstM2:i\d+>>  IntConstant -2
## CHECK:  <<ConstM3:i\d+>>  IntConstant -3

## CHECK:  name         "<<BTry:B\d+>>"
## CHECK:  predecessors "<<BEnterTry>>"
## CHECK:  successors   "<<BExitTry:B\d+>>"
## CHECK:  DivZeroCheck
## CHECK:  Div
    :try_start
    div-int/2addr p0, p1
    :try_end
    .catch Ljava/lang/ArithmeticException; {:try_start .. :try_end} :catch_arith
    .catch Ljava/lang/OutOfMemoryError; {:try_start .. :try_end} :catch_mem
    .catchall {:try_start .. :try_end} :catch_other

## CHECK:  name         "<<BReturn:B\d+>>"
## CHECK:  predecessors "<<BCatch1:B\d+>>" "<<BCatch2:B\d+>>" "<<BCatch3:B\d+>>" "<<BExitTry>>"
## CHECK:  Return
    :goto_ret
    return p0

## CHECK:  name         "<<BCatch1>>"
## CHECK:  predecessors "<<BEnterTry>>" "<<BExitTry>>"
## CHECK:  successors   "<<BReturn>>"
## CHECK:  flags        "catch_block"
## CHECK:  StoreLocal [v1,<<ConstM1>>]
    :catch_arith
    const/4 p0, -0x1
    goto :goto_ret

## CHECK:  name         "<<BCatch2>>"
## CHECK:  predecessors "<<BEnterTry>>" "<<BExitTry>>"
## CHECK:  successors   "<<BReturn>>"
## CHECK:  flags        "catch_block"
## CHECK:  StoreLocal [v1,<<ConstM2>>]
    :catch_mem
    const/4 p0, -0x2
    goto :goto_ret

## CHECK:  name         "<<BCatch3>>"
## CHECK:  predecessors "<<BEnterTry>>" "<<BExitTry>>"
## CHECK:  successors   "<<BReturn>>"
## CHECK:  flags        "catch_block"
## CHECK:  StoreLocal [v1,<<ConstM3>>]
    :catch_other
    const/4 p0, -0x3
    goto :goto_ret

## CHECK:  name         "<<BEnterTry>>"
## CHECK:  predecessors "B0"
## CHECK:  successors   "<<BTry>>"
## CHECK:  xhandlers    "<<BCatch1>>" "<<BCatch2>>" "<<BCatch3>>"
## CHECK:  EnterTry

## CHECK:  name         "<<BExitTry>>"
## CHECK:  predecessors "<<BTry>>"
## CHECK:  successors   "<<BReturn>>"
## CHECK:  xhandlers    "<<BCatch1>>" "<<BCatch2>>" "<<BCatch3>>"
## CHECK:  ExitTry
.end method

# Test that multiple EnterTry blocks are generated if there are multiple entry
# points into the try block.

## CHECK-START: int Builder.testMultipleEntries(int, int, int, int) builder (after)
.method public static testMultipleEntries(IIII)I
    .registers 4

## CHECK:  name         "B0"
## CHECK:  successors   "<<BIf:B\d+>>"
## CHECK:  <<M1:i\d+>>  IntConstant -1

## CHECK:  name         "<<BIf>>"
## CHECK:  predecessors "B0"
## CHECK:  successors   "<<BEnterTry2:B\d+>>" "<<BThen:B\d+>>"
## CHECK:  If
    if-eqz p2, :else

## CHECK:  name         "<<BThen>>"
## CHECK:  predecessors "<<BIf>>"
## CHECK:  successors   "<<BEnterTry1:B\d+>>"
## CHECK:  Div
    div-int/2addr p0, p1

## CHECK:  name         "<<BTry1:B\d+>>"
## CHECK:  predecessors "<<BEnterTry1>>"
## CHECK:  successors   "<<BTry2:B\d+>>"
## CHECK:  Div
    :try_start
    div-int/2addr p0, p2

## CHECK:  name         "<<BTry2>>"
## CHECK:  predecessors "<<BTry1>>" "<<BEnterTry2>>"
## CHECK:  successors   "<<BExitTry:B\d+>>"
## CHECK:  Div
    :else
    div-int/2addr p0, p3
    :try_end
    .catchall {:try_start .. :try_end} :catch_all

## CHECK:  name         "<<BReturn:B\d+>>"
## CHECK:  predecessors "<<BCatch:B\d+>>" "<<BExitTry>>"
## CHECK:  Return
    :goto_ret
    return p0

## CHECK:  name         "<<BCatch>>"
## CHECK:  predecessors "<<BEnterTry1>>" "<<BEnterTry2>>" "<<BExitTry>>"
## CHECK:  successors   "<<BReturn>>"
## CHECK:  flags        "catch_block"
## CHECK:  StoreLocal [v0,<<M1>>]
    :catch_all
    const/4 p0, -0x1
    goto :goto_ret

## CHECK:  name         "<<BEnterTry1>>"
## CHECK:  predecessors "<<BThen>>"
## CHECK:  successors   "<<BTry1>>"
## CHECK:  xhandlers    "<<BCatch>>"
## CHECK:  EnterTry

## CHECK:  name         "<<BEnterTry2>>"
## CHECK:  predecessors "<<BIf>>"
## CHECK:  successors   "<<BTry2>>"
## CHECK:  xhandlers    "<<BCatch>>"
## CHECK:  EnterTry

## CHECK:  name         "<<BExitTry>>"
## CHECK:  predecessors "<<BTry2>>"
## CHECK:  successors   "<<BReturn>>"
## CHECK:  xhandlers    "<<BCatch>>"
## CHECK:  ExitTry
.end method

# Test that multiple ExitTry blocks are generated if (normal) control flow can
# jump out of the try block at multiple points.

## CHECK-START: int Builder.testMultipleExits(int, int) builder (after)
.method public static testMultipleExits(II)I
    .registers 2

## CHECK:  name         "B0"
## CHECK:  successors   "<<BEnterTry:B\d+>>"
## CHECK:  <<M1:i\d+>>  IntConstant -1
## CHECK:  <<M2:i\d+>>  IntConstant -2

## CHECK:  name         "<<BTry:B\d+>>"
## CHECK:  predecessors "<<BEnterTry>>"
## CHECK:  successors   "<<BExitTry1:B\d+>>" "<<BExitTry2:B\d+>>"
## CHECK:  Div
## CHECK:  If
    :try_start
    div-int/2addr p0, p1
    if-eqz p0, :then
    :try_end
    .catchall {:try_start .. :try_end} :catch_all

## CHECK:  name         "<<BReturn:B\d+>>"
## CHECK:  predecessors "<<BThen:B\d+>>" "<<BCatch:B\d+>>" "<<BExitTry2>>"
## CHECK:  Return
    :goto_ret
    return p0

## CHECK:  name         "<<BThen>>"
## CHECK:  predecessors "<<BExitTry1>>"
## CHECK:  successors   "<<BReturn>>"
## CHECK:  StoreLocal [v0,<<M1>>]
    :then
    const/4 p0, -0x1
    goto :goto_ret

## CHECK:  name         "<<BCatch>>"
## CHECK:  predecessors "<<BEnterTry>>" "<<BExitTry1>>" "<<BExitTry2>>"
## CHECK:  successors   "<<BReturn>>"
## CHECK:  flags        "catch_block"
## CHECK:  StoreLocal [v0,<<M2>>]
    :catch_all
    const/4 p0, -0x2
    goto :goto_ret

## CHECK:  name         "<<BEnterTry>>"
## CHECK:  predecessors "B0"
## CHECK:  successors   "<<BTry>>"
## CHECK:  xhandlers    "<<BCatch>>"
## CHECK:  EnterTry

## CHECK:  name         "<<BExitTry1>>"
## CHECK:  predecessors "<<BTry>>"
## CHECK:  successors   "<<BThen>>"
## CHECK:  xhandlers    "<<BCatch>>"
## CHECK:  ExitTry

## CHECK:  name         "<<BExitTry2>>"
## CHECK:  predecessors "<<BTry>>"
## CHECK:  successors   "<<BReturn>>"
## CHECK:  xhandlers    "<<BCatch>>"
## CHECK:  ExitTry
.end method

# Test that nested tries are split into non-overlapping blocks and Enter/ExitTry
# blocks are correctly created between them. This also tests code with multiple
# try blocks and shared catch handlers.

## CHECK-START: int Builder.testNestedTry(int, int, int, int) builder (after)
.method public static testNestedTry(IIII)I
    .registers 4

## CHECK:  name         "B0"
## CHECK:  <<M1:i\d+>>  IntConstant -1
## CHECK:  <<M2:i\d+>>  IntConstant -2

## CHECK:  name         "<<BTry1:B\d+>>"
## CHECK:  predecessors "<<BEnterTry1:B\d+>>"
## CHECK:  successors   "<<BExitTry1:B\d+>>"
## CHECK:  Div
    :try_start_1
    div-int/2addr p0, p1

## CHECK:  name         "<<BTry2:B\d+>>"
## CHECK:  predecessors "<<BEnterTry2:B\d+>>"
## CHECK:  successors   "<<BExitTry2:B\d+>>"
## CHECK:  Div
    :try_start_2
    div-int/2addr p0, p2
    :try_end_2
    .catch Ljava/lang/ArithmeticException; {:try_start_2 .. :try_end_2} :catch_arith

## CHECK:  name         "<<BTry3:B\d+>>"
## CHECK:  predecessors "<<BEnterTry3:B\d+>>"
## CHECK:  successors   "<<BExitTry3:B\d+>>"
## CHECK:  Div
    div-int/2addr p0, p3
    :try_end_1
    .catchall {:try_start_1 .. :try_end_1} :catch_all

## CHECK:  name         "<<BReturn:B\d+>>"
## CHECK:  predecessors "<<BCatchArith:B\d+>>" "<<BCatchAll:B\d+>>" "<<BExitTry3>>"
    :goto_ret
    return p0

## CHECK:  name         "<<BCatchArith>>"
## CHECK:  predecessors "<<BEnterTry2>>" "<<BExitTry2>>"
## CHECK:  successors   "<<BReturn>>"
## CHECK:  flags        "catch_block"
## CHECK:  StoreLocal [v0,<<M1>>]
    :catch_arith
    const/4 p0, -0x1
    goto :goto_ret

## CHECK:  name         "<<BCatchAll>>"
## CHECK:  predecessors "<<BEnterTry1>>" "<<BExitTry1>>" "<<BEnterTry2>>" "<<BExitTry2>>" "<<BEnterTry3>>" "<<BExitTry3>>"
## CHECK:  successors   "<<BReturn>>"
## CHECK:  flags        "catch_block"
## CHECK:  StoreLocal [v0,<<M2>>]
    :catch_all
    const/4 p0, -0x2
    goto :goto_ret

## CHECK:  name         "<<BEnterTry1>>"
## CHECK:  predecessors "B0"
## CHECK:  successors   "<<BTry1>>"
## CHECK:  xhandlers    "<<BCatchAll>>"
## CHECK:  EnterTry

## CHECK:  name         "<<BExitTry1>>"
## CHECK:  predecessors "<<BTry1>>"
## CHECK:  successors   "<<BEnterTry2>>"
## CHECK:  xhandlers    "<<BCatchAll>>"
## CHECK:  ExitTry

## CHECK:  name         "<<BEnterTry2>>"
## CHECK:  predecessors "<<BExitTry1>>"
## CHECK:  successors   "<<BTry2>>"
## CHECK:  xhandlers    "<<BCatchArith>>" "<<BCatchAll>>"
## CHECK:  EnterTry

## CHECK:  name         "<<BExitTry2>>"
## CHECK:  predecessors "<<BTry2>>"
## CHECK:  successors   "<<BEnterTry3>>"
## CHECK:  xhandlers    "<<BCatchArith>>" "<<BCatchAll>>"
## CHECK:  ExitTry

## CHECK:  name         "<<BEnterTry3>>"
## CHECK:  predecessors "<<BExitTry2>>"
## CHECK:  successors   "<<BTry3>>"
## CHECK:  xhandlers    "<<BCatchAll>>"
## CHECK:  EnterTry

## CHECK:  name         "<<BExitTry3>>"
## CHECK:  predecessors "<<BTry3>>"
## CHECK:  successors   "<<BReturn>>"
## CHECK:  xhandlers    "<<BCatchAll>>"
## CHECK:  ExitTry
.end method

# Test case when control flow leaves the try block and later returns.

## CHECK-START: int Builder.testIncontinuousTry(int, int, int, int) builder (after)
.method public static testIncontinuousTry(IIII)I
    .registers 4

## CHECK:  name         "B0"

## CHECK:  name         "<<BTry1:B\d+>>"
## CHECK:  predecessors "<<BEnterTry1:B\d+>>"
## CHECK:  successors   "<<BExitTry1:B\d+>>"
## CHECK:  Div
    :try_start
    div-int/2addr p0, p1
    goto :outside

## CHECK:  name         "<<BTry2:B\d+>>"
## CHECK:  predecessors "<<BEnterTry2:B\d+>>"
## CHECK:  successors   "<<BExitTry2:B\d+>>"
## CHECK:  Div
    :inside
    div-int/2addr p0, p3
    :try_end
    .catchall {:try_start .. :try_end} :catch_all

## CHECK:  name         "<<BReturn:B\d+>>"
## CHECK:  predecessors "<<BCatch:B\d+>>" "<<BExitTry2>>"
    :goto_ret
    return p0

## CHECK:  name         "<<BOutside:B\d+>>"
## CHECK:  predecessors "<<BExitTry1>>"
## CHECK:  successors   "<<BEnterTry2>>"
## CHECK:  Div
    :outside
    div-int/2addr p0, p2
    goto :inside

## CHECK:  name         "<<BCatch>>"
## CHECK:  predecessors "<<BEnterTry1>>" "<<BExitTry1>>" "<<BEnterTry2>>" "<<BExitTry2>>"
## CHECK:  successors   "<<BReturn>>"
## CHECK:  flags        "catch_block"
## CHECK:  StoreLocal [v0,{{i\d+}}]
    :catch_all
    const/4 p0, -0x1
    goto :goto_ret

## CHECK:  name         "<<BEnterTry1>>"
## CHECK:  predecessors "B0"
## CHECK:  successors   "<<BTry1>>"
## CHECK:  xhandlers    "<<BCatch>>"
## CHECK:  EnterTry

## CHECK:  name         "<<BExitTry1>>"
## CHECK:  predecessors "<<BTry1>>"
## CHECK:  successors   "<<BOutside>>"
## CHECK:  xhandlers    "<<BCatch>>"
## CHECK:  ExitTry

## CHECK:  name         "<<BEnterTry2>>"
## CHECK:  predecessors "<<BOutside>>"
## CHECK:  successors   "<<BTry2>>"
## CHECK:  xhandlers    "<<BCatch>>"
## CHECK:  EnterTry

## CHECK:  name         "<<BExitTry2>>"
## CHECK:  predecessors "<<BTry2>>"
## CHECK:  successors   "<<BReturn>>"
## CHECK:  xhandlers    "<<BCatch>>"
## CHECK:  ExitTry
.end method

# Test a graph with a loop from throwing and catching an exception.

## CHECK-START: int Builder.testCatchLoop(int, int, int) builder (after)
.method public static testCatchLoop(III)I
    .registers 4

## CHECK:  name         "B0"

## CHECK:  name         "<<BTry:B\d+>>"
## CHECK:  predecessors "<<BEnterTry:B\d+>>"
## CHECK:  successors   "<<BCatch:B\d+>>"
## CHECK:  Div
    :try_start
    div-int/2addr p0, p1

## CHECK:  name         "<<BCatch>>"
## CHECK:  predecessors "<<BTry>>" "<<BEnterTry>>" "<<BExitTry:B\d+>>"
## CHECK:  successors   "<<BExitTry>>"
## CHECK:  Div
    :catch_all
    div-int/2addr p0, p2
    :try_end
    .catchall {:try_start .. :try_end} :catch_all

## CHECK:  name         "<<BReturn:B\d+>>"
## CHECK:  predecessors "<<BExitTry>>"
    :goto_ret
    return p0

## CHECK:  name         "<<BEnterTry>>"
## CHECK:  predecessors "B0"
## CHECK:  successors   "<<BTry>>"
## CHECK:  xhandlers    "<<BCatch>>"
## CHECK:  EnterTry

## CHECK:  name         "<<BExitTry>>"
## CHECK:  predecessors "<<BCatch>>"
## CHECK:  successors   "<<BReturn>>"
## CHECK:  xhandlers    "<<BCatch>>"
## CHECK:  ExitTry
.end method
