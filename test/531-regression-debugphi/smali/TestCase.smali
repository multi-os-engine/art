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

# The following method has two phis: PhiA and PhiB. They are both dead but
# have environment uses. When compiled with --debugging, these need to be kept
# live. b/24129675 caused the SSAChecker to crash because two kPrimNot
# equivalents were kept for PhiA.

.method public static testCase(IILjava/lang/Object;)V
  .registers 5

  # v0 - Phi A
  # v1 - Phi B
  # p0 - int arg1
  # p1 - int arg2
  # p2 - ref arg3

  if-nez p0, :else1
  :then1
    if-nez p1, :else2
    :then2
      const/4 v1, 0
      goto :merge2

    :else2
      move-object v1, p2
      goto :merge2

    :merge2
      # PhiA [null, arg3]
      move-object v0, v1
      invoke-static {}, Ljava/lang/System;->nanoTime()J  # env use of PhiA
      goto :merge1

  :else1
    move-object v0, p2
    goto :merge1

  :merge1
    # PhiB [PhiA, arg3]
    invoke-static {}, Ljava/lang/System;->nanoTime()J  # env use of PhiB

  return-void
.end method
