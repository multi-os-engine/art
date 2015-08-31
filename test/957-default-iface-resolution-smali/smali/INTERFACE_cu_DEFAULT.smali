
.class public abstract interface LINTERFACE_cu_DEFAULT;
.super Ljava/lang/Object;
 
.implements LINTERFACE_ad_DEFAULT;

.implements LINTERFACE_x;

.implements LINTERFACE_y_DEFAULT;


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
# public interface INTERFACE_cu_DEFAULT extends   INTERFACE_ad_DEFAULT, INTERFACE_x, INTERFACE_y_DEFAULT {
#   public String CalledClassName();

.method public abstract CalledClassName()Ljava/lang/String;
.end method


#   public default String CalledInterfaceName() {
#     return "[INTERFACE_cu_DEFAULT [INTERFACE_ad_DEFAULT [INTERFACE_ab ] [INTERFACE_y_DEFAULT ]] [INTERFACE_x ] [INTERFACE_y_DEFAULT ]]";
#   }
.method public CalledInterfaceName()Ljava/lang/String;
  .locals 1
  const-string v0, "[INTERFACE_cu_DEFAULT [INTERFACE_ad_DEFAULT [INTERFACE_ab ] [INTERFACE_y_DEFAULT ]] [INTERFACE_x ] [INTERFACE_y_DEFAULT ]]"
  return-object v0
.end method


# }

