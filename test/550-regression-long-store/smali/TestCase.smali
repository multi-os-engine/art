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

.method public static $noinline$throw()V
  .registers 1
  new-instance v0, Ljava/lang/Exception;
  invoke-direct {v0}, Ljava/lang/Exception;-><init>()V
  throw v0
.end method

.method public static invalidateLow(J)I
  .registers 4

  const/4 v1, 0x0

  :try_start
  invoke-static {}, Ljava/lang/System;->nanoTime()J
  move-wide v0, p0
  long-to-int v1, v0
  invoke-static {}, LTestCase;->$noinline$throw()V
  :try_end
  .catchall {:try_start .. :try_end} :catchall

  :catchall
  return v1

.end method
