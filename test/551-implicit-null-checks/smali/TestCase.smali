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

.field public i1:J

.method public constructor <init>()V
      .registers 1

      invoke-direct {v0}, Ljava/lang/Object;-><init>()V
      return-void
.end method

.method public get()J
      .registers 3

      const v0, 0
      iget-wide v1, v0, LTestCase;->i1:J
      return-wide v1
.end method

.method public put()V
      .registers 3

      const v2, 0
      const-wide v0, 778899112233L
      iput-wide v0, v2, LTestCase;->i1:J
      return-void
.end method
