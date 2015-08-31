
.class public LCLASS_hm;
.super Ljava/lang/Object;
.implements LINTERFACE_hl_DEFAULT;

# /*
#  * Copyright (C) 2015 The Android Open Source Project
#  *
#  * Licensed under the Apache License, Version 2.0 (the "License");
#  * you may not use this file except in compliance with the License.
#  * You may obtain a copy of the License at
#  *
#  *      http://www.apache.org/licenses/LICENSE-2.0
#  *
#  * Unless required by applicable law or agreed to in writing, software
#  * distributed under the License is distributed on an "AS IS" BASIS,
#  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  * See the License for the specific language governing permissions and
#  * limitations under the License.
#  */
#
# public class CLASS_hm implements INTERFACE_hl_DEFAULT {
#   public String CalledClassName() {
#     return "[CLASS_hm [INTERFACE_hl_DEFAULT [INTERFACE_hb_DEFAULT ] [INTERFACE_he ] [INTERFACE_hf_DEFAULT ]]]";
#   }
# }

.method public constructor <init>()V
  .registers 1
  invoke-direct {p0}, Ljava/lang/Object;-><init>()V
  return-void
.end method

.method public CalledClassName()Ljava/lang/String;
  .locals 1
  const-string v0, "[CLASS_hm [INTERFACE_hl_DEFAULT [INTERFACE_hb_DEFAULT ] [INTERFACE_he ] [INTERFACE_hf_DEFAULT ]]]"
  return-object v0
.end method

