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

.method public static inlineTrue()Z
  .registers 1
  const/4 v0, 1
  return v0
.end method

.method public static inlineFalse()Z
  .registers 1
  const/4 v0, 0
  return v0
.end method


.method public static testSingleExit(IZ)I
  .registers 3

  # p0 = int X
  # p1 = boolean Y
  # v0 = true

  invoke-static {}, LTestCase;->inlineTrue()Z
  move-result v0

  :loop_start
  if-eqz p1, :loop_body   # cannot be determined statically
  if-nez v0, :loop_end    # will always exit

  # Dead block
  add-int/lit8 p0, p0, 5
  goto :loop_start

  # Live block
  :loop_body
  add-int/lit8 p0, p0, 7
  goto :loop_start

  :loop_end
  return p0
.end method


.method public static testMultipleExits(IZZ)I
  .registers 4

  # p0 = int X
  # p1 = boolean Y
  # p2 = boolean Z
  # v0 = true

  invoke-static {}, LTestCase;->inlineTrue()Z
  move-result v0

  :loop_start
  if-eqz p1, :loop_body   # cannot be determined statically
  if-nez p2, :loop_end    # may exit
  if-nez v0, :loop_end    # will always exit

  # Dead block
  add-int/lit8 p0, p0, 5
  goto :loop_start

  # Live block
  :loop_body
  add-int/lit8 p0, p0, 7
  goto :loop_start

  :loop_end
  return p0
.end method


.method public static testExitPredecessors(IZZ)I
  .registers 4

  # p0 = int X
  # p1 = boolean Y
  # p2 = boolean Z
  # v0 = true

  invoke-static {}, LTestCase;->inlineTrue()Z
  move-result v0

  :loop_start
  if-eqz p1, :loop_body   # cannot be determined statically

  # Additional logic which will end up outside the loop
  if-eqz p2, :skip_if
  mul-int/lit8 p0, p0, 9
  :skip_if

  if-nez v0, :loop_end    # will always take the branch

  # Dead block
  add-int/lit8 p0, p0, 5
  goto :loop_start

  # Live block
  :loop_body
  add-int/lit8 p0, p0, 7
  goto :loop_start

  :loop_end
  return p0
.end method

.method public static testInnerLoop(IZZ)I
  .registers 4

  # p0 = int X
  # p1 = boolean Y
  # p2 = boolean Z
  # v0 = true

  invoke-static {}, LTestCase;->inlineTrue()Z
  move-result v0

  :loop_start
  if-eqz p1, :loop_body   # cannot be determined statically

  # Inner loop which will end up outside its parent
  :inner_loop_start
  xor-int/lit8 p2, p2, 1
  if-eqz p2, :inner_loop_start

  if-nez v0, :loop_end    # will always take the branch

  # Dead block
  add-int/lit8 p0, p0, 5
  goto :loop_start

  # Live block
  :loop_body
  add-int/lit8 p0, p0, 7
  goto :loop_start

  :loop_end
  return p0
.end method
