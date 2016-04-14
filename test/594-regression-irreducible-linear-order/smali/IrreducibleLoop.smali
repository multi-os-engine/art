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

.method public static liveness(ZZZ)I
   .registers 10
   const/16 v0, 42

   if-eqz p0, :header

   :pre_entry
   invoke-static {v0}, Ljava/lang/System;->exit(I)V
   goto :body1

   :header
   if-eqz p1, :body2

   :body1
   goto :body_merge

   :body2
   invoke-static {v0}, Ljava/lang/System;->exit(I)V
   goto :body_merge

   :body_merge
   if-eqz p2, :exit
   goto :header

   :exit
   return v0

.end method
