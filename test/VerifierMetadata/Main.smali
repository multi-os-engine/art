# Copyright (C) 2016 The Android Open Source Project
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

.class public LMain;
.super Ljava/lang/Object;

.method public static ArgumentType_Resolved(Ljava/lang/IllegalStateException;)V
  .registers 1
  return-void
.end method

.method public static ArgumentType_Unresolved(LUnresolvedClass;)V
  .registers 1
  return-void
.end method

.method public static ReturnType_Resolved()Ljava/lang/IllegalStateException;
  .registers 1
  const/4 v0, 0x0
  return-object v0
.end method

.method public static ReturnType_Unresolved()LUnresolvedClass;
  .registers 1
  const/4 v0, 0x0
  return-object v0
.end method

.method public static ConstClass_Resolved()V
  .registers 1
  const-class v0, Ljava/lang/IllegalStateException;
  return-void
.end method

.method public static ConstClass_Unresolved()V
  .registers 1
  const-class v0, LUnresolvedClass;
  return-void
.end method

.method public static CheckCast_Resolved(Ljava/lang/Object;)V
  .registers 1
  check-cast p0, Ljava/lang/IllegalStateException;
  return-void
.end method

.method public static CheckCast_Unresolved(Ljava/lang/Object;)V
  .registers 1
  check-cast p0, LUnresolvedClass;
  return-void
.end method

.method public static InstanceOf_Resolved(Ljava/lang/Object;)Z
  .registers 1
  instance-of p0, p0, Ljava/lang/IllegalStateException;
  return p0
.end method

.method public static InstanceOf_Unresolved(Ljava/lang/Object;)Z
  .registers 1
  instance-of p0, p0, LUnresolvedClass;
  return p0
.end method

.method public static NewInstance_Resolved()V
  .registers 1
  new-instance v0, Ljava/lang/IllegalStateException;
  return-void
.end method

.method public static NewInstance_Unresolved()V
  .registers 1
  new-instance v0, LUnresolvedClass;
  return-void
.end method

.method public static NewArray_Resolved()V
  .registers 1
  const/4 v0, 0x1
  new-array v0, v0, [Ljava/lang/IllegalStateException;
  return-void
.end method

.method public static NewArray_Unresolved()V
  .registers 2
  const/4 v0, 0x1
  new-array v0, v0, [LUnresolvedClass;
  return-void
.end method

.method public static StaticField_Resolved_DeclaredInReferrer()V
  .registers 1
  sget-object v0, Ljava/lang/System;->out:Ljava/io/PrintStream;
  return-void
.end method

.method public static StaticField_Resolved_DeclaredInSuperclass1()V
  .registers 1
  sget v0, Ljava/util/SimpleTimeZone;->LONG:I
  return-void
.end method

.method public static StaticField_Resolved_DeclaredInSuperclass2()V
  .registers 1
  sget v0, LMySimpleTimeZone;->SHORT:I
  return-void
.end method

.method public static StaticField_Resolved_DeclaredInInterface1()V
  .registers 1
  # Case 1: DOMResult implements Result
  sget-object v0, Ljavax/xml/transform/dom/DOMResult;->PI_ENABLE_OUTPUT_ESCAPING:Ljava/lang/String;
  return-void
.end method

.method public static StaticField_Resolved_DeclaredInInterface2()V
  .registers 1
  # Case 2: MyDOMResult extends DOMResult, DOMResult implements Result
  sget-object v0, LMyDOMResult;->PI_ENABLE_OUTPUT_ESCAPING:Ljava/lang/String;
  return-void
.end method

.method public static StaticField_Resolved_DeclaredInInterface3()V
  .registers 1
  # Case 3: MyResult implements Result
  sget-object v0, LMyResult;->PI_ENABLE_OUTPUT_ESCAPING:Ljava/lang/String;
  return-void
.end method

.method public static StaticField_Resolved_DeclaredInInterface4()V
  .registers 1
  # Case 4: MyDocument implements Document, Document extends Node
  sget-short v0, LMyDocument;->ELEMENT_NODE:S
  return-void
.end method

.method public static InstanceField_Resolved_DeclaredInReferrer(LMySocketTimeoutException;)V
  .registers 1
  iget v0, p0, Ljava/io/InterruptedIOException;->bytesTransferred:I
  return-void
.end method

.method public static InstanceField_Resolved_DeclaredInSuperclass1(Ljava/net/SocketTimeoutException;)V
  .registers 1
  iget v0, p0, Ljava/net/SocketTimeoutException;->bytesTransferred:I
  return-void
.end method

.method public static InstanceField_Resolved_DeclaredInSuperclass2(LMySocketTimeoutException;)V
  .registers 1
  iget v0, p0, LMySocketTimeoutException;->bytesTransferred:I
  return-void
.end method

.method public static MoveException_Resolved()Ljava/lang/Object;
  .registers 1
  :try_start
  invoke-static {}, Ljava/lang/System;->nanoTime()I
  :try_end
  .catch Ljava/io/IOException; {:try_start .. :try_end} :catch_block
  const/4 v0, 0x0
  return-object v0

  :catch_block
  move-exception v0
  return-object v0
.end method

.method public static MoveException_Unresolved()Ljava/lang/Object;
  .registers 1
  :try_start
  invoke-static {}, Ljava/lang/System;->nanoTime()I
  :try_end
  .catch LUnresolvedException; {:try_start .. :try_end} :catch_block
  const/4 v0, 0x0
  return-object v0

  :catch_block
  move-exception v0
  return-object v0
.end method
