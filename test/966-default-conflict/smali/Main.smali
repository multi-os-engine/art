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
# class Main implements iface, iface2 {
#   public static void main(String[] args) {
#       System.out.println("Create Main instance");
#       Main m = new Main();
#       System.out.println("Calling non-conflicting function");
#       System.out.println(m.Charge());
#       System.out.println("Calling conflicting function");
#       try {
#           System.out.println(m.SayHi());
#           System.out.println("Unexpected no error Thrown");
#       } catch (AbstractMethodError e) {
#           System.out.println("Unexpected AME Thrown");
#       } catch (IncompatibleClassChangeError e) {
#           System.out.println("Expected ICCE Thrown");
#       }
#       System.out.println("Calling non-conflicting function");
#       System.out.println(m.Charge());
#   }
# }

.class public LMain;
.super Ljava/lang/Object;
.implements Liface;
.implements Liface2;

.method public constructor <init>()V
    .registers 1
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    return-void
.end method

.method public static main([Ljava/lang/String;)V
    .locals 3
    sget-object v1, Ljava/lang/System;->out:Ljava/io/PrintStream;

    const-string v0, "Create Main instance"
    invoke-virtual {v1,v0}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V

    new-instance v2, LMain;
    invoke-direct {v2}, LMain;-><init>()V

    const-string v0, "Calling non-conflicting function"
    invoke-virtual {v1,v0}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V

    invoke-virtual {v2}, LMain;->Charge()Ljava/lang/String;
    move-result-object v0
    invoke-virtual {v1,v0}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V

    const-string v0, "Calling conflicting function"
    invoke-virtual {v1,v0}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V
    :try_start
        invoke-virtual {v2}, LMain;->SayHi()Ljava/lang/String;
        move-result-object v0
        invoke-virtual {v1,v0}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V

        const-string v0, "Unexpected no error Thrown"
        invoke-virtual {v1,v0}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V

        goto :error_end
    :try_end
    .catch Ljava/lang/AbstractMethodError; {:try_start .. :try_end} :AME_error_start
    .catch Ljava/lang/IncompatibleClassChangeError; {:try_start .. :try_end} :ICCE_error_start
    :AME_error_start
        const-string v0, "Unexpected AME Thrown"
        invoke-virtual {v1,v0}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V
        goto :error_end
    :ICCE_error_start
        const-string v0, "Expected ICCE Thrown"
        invoke-virtual {v1,v0}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V
        goto :error_end
    :error_end
    const-string v0, "Calling non-conflicting function"
    invoke-virtual {v1,v0}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V

    invoke-virtual {v2}, LMain;->Charge()Ljava/lang/String;
    move-result-object v0
    invoke-virtual {v1,v0}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V

    return-void
.end method
