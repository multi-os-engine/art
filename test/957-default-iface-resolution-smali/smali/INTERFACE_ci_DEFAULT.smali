
.class public abstract interface LINTERFACE_ci_DEFAULT;
.super Ljava/lang/Object;
 
.implements LINTERFACE_ac_DEFAULT;

.implements LINTERFACE_x;


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
# public interface INTERFACE_ci_DEFAULT extends   INTERFACE_ac_DEFAULT, INTERFACE_x {
#   public String CalledClassName();

.method public abstract CalledClassName()Ljava/lang/String;
.end method


#   public default String CalledInterfaceName() {
#     return "[INTERFACE_ci_DEFAULT [INTERFACE_ac_DEFAULT [INTERFACE_aa_DEFAULT ] [INTERFACE_y_DEFAULT ]] [INTERFACE_x ]]";
#   }
.method public CalledInterfaceName()Ljava/lang/String;
  .locals 1
  const-string v0, "[INTERFACE_ci_DEFAULT [INTERFACE_ac_DEFAULT [INTERFACE_aa_DEFAULT ] [INTERFACE_y_DEFAULT ]] [INTERFACE_x ]]"
  return-object v0
.end method


# }

