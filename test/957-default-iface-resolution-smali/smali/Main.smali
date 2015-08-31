
.class public LMain;
.super Ljava/lang/Object;

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
# class Main {

.method public constructor <init>()V
    .registers 1
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    return-void
.end method


#   public static void TEST_FUNC_CLASS_an() {
#     try {
#       CLASS_an v = new CLASS_an();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_an()V
    .locals 7
    :call_TEST_FUNC_CLASS_an_try_start
      new-instance v6, LCLASS_an;
      invoke-direct {v6}, LCLASS_an;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_an;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_an;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_an_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_an_try_start .. :call_TEST_FUNC_CLASS_an_try_end} :error_TEST_FUNC_CLASS_an_start
    :error_TEST_FUNC_CLASS_an_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_ap() {
#     try {
#       CLASS_ap v = new CLASS_ap();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_ap()V
    .locals 7
    :call_TEST_FUNC_CLASS_ap_try_start
      new-instance v6, LCLASS_ap;
      invoke-direct {v6}, LCLASS_ap;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_ap;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_ap;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_ap_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_ap_try_start .. :call_TEST_FUNC_CLASS_ap_try_end} :error_TEST_FUNC_CLASS_ap_start
    :error_TEST_FUNC_CLASS_ap_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_ar() {
#     try {
#       CLASS_ar v = new CLASS_ar();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_ar()V
    .locals 7
    :call_TEST_FUNC_CLASS_ar_try_start
      new-instance v6, LCLASS_ar;
      invoke-direct {v6}, LCLASS_ar;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_ar;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_ar;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_ar_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_ar_try_start .. :call_TEST_FUNC_CLASS_ar_try_end} :error_TEST_FUNC_CLASS_ar_start
    :error_TEST_FUNC_CLASS_ar_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_at() {
#     try {
#       CLASS_at v = new CLASS_at();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_at()V
    .locals 7
    :call_TEST_FUNC_CLASS_at_try_start
      new-instance v6, LCLASS_at;
      invoke-direct {v6}, LCLASS_at;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_at;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_at;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_at_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_at_try_start .. :call_TEST_FUNC_CLASS_at_try_end} :error_TEST_FUNC_CLASS_at_start
    :error_TEST_FUNC_CLASS_at_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_av() {
#     try {
#       CLASS_av v = new CLASS_av();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_av()V
    .locals 7
    :call_TEST_FUNC_CLASS_av_try_start
      new-instance v6, LCLASS_av;
      invoke-direct {v6}, LCLASS_av;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_av;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_av;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_av_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_av_try_start .. :call_TEST_FUNC_CLASS_av_try_end} :error_TEST_FUNC_CLASS_av_start
    :error_TEST_FUNC_CLASS_av_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_ax() {
#     try {
#       CLASS_ax v = new CLASS_ax();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_ax()V
    .locals 7
    :call_TEST_FUNC_CLASS_ax_try_start
      new-instance v6, LCLASS_ax;
      invoke-direct {v6}, LCLASS_ax;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_ax;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_ax;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_ax_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_ax_try_start .. :call_TEST_FUNC_CLASS_ax_try_end} :error_TEST_FUNC_CLASS_ax_start
    :error_TEST_FUNC_CLASS_ax_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_az() {
#     try {
#       CLASS_az v = new CLASS_az();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_az()V
    .locals 7
    :call_TEST_FUNC_CLASS_az_try_start
      new-instance v6, LCLASS_az;
      invoke-direct {v6}, LCLASS_az;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_az;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_az;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_az_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_az_try_start .. :call_TEST_FUNC_CLASS_az_try_end} :error_TEST_FUNC_CLASS_az_start
    :error_TEST_FUNC_CLASS_az_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_b() {
#     try {
#       CLASS_b v = new CLASS_b();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_b()V
    .locals 7
    :call_TEST_FUNC_CLASS_b_try_start
      new-instance v6, LCLASS_b;
      invoke-direct {v6}, LCLASS_b;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_b;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_b;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_b_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_b_try_start .. :call_TEST_FUNC_CLASS_b_try_end} :error_TEST_FUNC_CLASS_b_start
    :error_TEST_FUNC_CLASS_b_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_bb() {
#     try {
#       CLASS_bb v = new CLASS_bb();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_bb()V
    .locals 7
    :call_TEST_FUNC_CLASS_bb_try_start
      new-instance v6, LCLASS_bb;
      invoke-direct {v6}, LCLASS_bb;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_bb;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_bb;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_bb_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_bb_try_start .. :call_TEST_FUNC_CLASS_bb_try_end} :error_TEST_FUNC_CLASS_bb_start
    :error_TEST_FUNC_CLASS_bb_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_bd() {
#     try {
#       CLASS_bd v = new CLASS_bd();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_bd()V
    .locals 7
    :call_TEST_FUNC_CLASS_bd_try_start
      new-instance v6, LCLASS_bd;
      invoke-direct {v6}, LCLASS_bd;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_bd;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_bd;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_bd_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_bd_try_start .. :call_TEST_FUNC_CLASS_bd_try_end} :error_TEST_FUNC_CLASS_bd_start
    :error_TEST_FUNC_CLASS_bd_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_bf() {
#     try {
#       CLASS_bf v = new CLASS_bf();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_bf()V
    .locals 7
    :call_TEST_FUNC_CLASS_bf_try_start
      new-instance v6, LCLASS_bf;
      invoke-direct {v6}, LCLASS_bf;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_bf;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_bf;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_bf_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_bf_try_start .. :call_TEST_FUNC_CLASS_bf_try_end} :error_TEST_FUNC_CLASS_bf_start
    :error_TEST_FUNC_CLASS_bf_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_bh() {
#     try {
#       CLASS_bh v = new CLASS_bh();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_bh()V
    .locals 7
    :call_TEST_FUNC_CLASS_bh_try_start
      new-instance v6, LCLASS_bh;
      invoke-direct {v6}, LCLASS_bh;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_bh;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_bh;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_bh_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_bh_try_start .. :call_TEST_FUNC_CLASS_bh_try_end} :error_TEST_FUNC_CLASS_bh_start
    :error_TEST_FUNC_CLASS_bh_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_bj() {
#     try {
#       CLASS_bj v = new CLASS_bj();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_bj()V
    .locals 7
    :call_TEST_FUNC_CLASS_bj_try_start
      new-instance v6, LCLASS_bj;
      invoke-direct {v6}, LCLASS_bj;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_bj;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_bj;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_bj_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_bj_try_start .. :call_TEST_FUNC_CLASS_bj_try_end} :error_TEST_FUNC_CLASS_bj_start
    :error_TEST_FUNC_CLASS_bj_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_bl() {
#     try {
#       CLASS_bl v = new CLASS_bl();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_bl()V
    .locals 7
    :call_TEST_FUNC_CLASS_bl_try_start
      new-instance v6, LCLASS_bl;
      invoke-direct {v6}, LCLASS_bl;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_bl;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_bl;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_bl_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_bl_try_start .. :call_TEST_FUNC_CLASS_bl_try_end} :error_TEST_FUNC_CLASS_bl_start
    :error_TEST_FUNC_CLASS_bl_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_bn() {
#     try {
#       CLASS_bn v = new CLASS_bn();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_bn()V
    .locals 7
    :call_TEST_FUNC_CLASS_bn_try_start
      new-instance v6, LCLASS_bn;
      invoke-direct {v6}, LCLASS_bn;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_bn;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_bn;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_bn_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_bn_try_start .. :call_TEST_FUNC_CLASS_bn_try_end} :error_TEST_FUNC_CLASS_bn_start
    :error_TEST_FUNC_CLASS_bn_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_bp() {
#     try {
#       CLASS_bp v = new CLASS_bp();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_bp()V
    .locals 7
    :call_TEST_FUNC_CLASS_bp_try_start
      new-instance v6, LCLASS_bp;
      invoke-direct {v6}, LCLASS_bp;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_bp;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_bp;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_bp_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_bp_try_start .. :call_TEST_FUNC_CLASS_bp_try_end} :error_TEST_FUNC_CLASS_bp_start
    :error_TEST_FUNC_CLASS_bp_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_br() {
#     try {
#       CLASS_br v = new CLASS_br();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_br()V
    .locals 7
    :call_TEST_FUNC_CLASS_br_try_start
      new-instance v6, LCLASS_br;
      invoke-direct {v6}, LCLASS_br;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_br;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_br;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_br_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_br_try_start .. :call_TEST_FUNC_CLASS_br_try_end} :error_TEST_FUNC_CLASS_br_start
    :error_TEST_FUNC_CLASS_br_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_bt() {
#     try {
#       CLASS_bt v = new CLASS_bt();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_bt()V
    .locals 7
    :call_TEST_FUNC_CLASS_bt_try_start
      new-instance v6, LCLASS_bt;
      invoke-direct {v6}, LCLASS_bt;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_bt;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_bt;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_bt_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_bt_try_start .. :call_TEST_FUNC_CLASS_bt_try_end} :error_TEST_FUNC_CLASS_bt_start
    :error_TEST_FUNC_CLASS_bt_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_bv() {
#     try {
#       CLASS_bv v = new CLASS_bv();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_bv()V
    .locals 7
    :call_TEST_FUNC_CLASS_bv_try_start
      new-instance v6, LCLASS_bv;
      invoke-direct {v6}, LCLASS_bv;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_bv;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_bv;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_bv_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_bv_try_start .. :call_TEST_FUNC_CLASS_bv_try_end} :error_TEST_FUNC_CLASS_bv_start
    :error_TEST_FUNC_CLASS_bv_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_bx() {
#     try {
#       CLASS_bx v = new CLASS_bx();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_bx()V
    .locals 7
    :call_TEST_FUNC_CLASS_bx_try_start
      new-instance v6, LCLASS_bx;
      invoke-direct {v6}, LCLASS_bx;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_bx;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_bx;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_bx_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_bx_try_start .. :call_TEST_FUNC_CLASS_bx_try_end} :error_TEST_FUNC_CLASS_bx_start
    :error_TEST_FUNC_CLASS_bx_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_bz() {
#     try {
#       CLASS_bz v = new CLASS_bz();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_bz()V
    .locals 7
    :call_TEST_FUNC_CLASS_bz_try_start
      new-instance v6, LCLASS_bz;
      invoke-direct {v6}, LCLASS_bz;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_bz;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_bz;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_bz_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_bz_try_start .. :call_TEST_FUNC_CLASS_bz_try_end} :error_TEST_FUNC_CLASS_bz_start
    :error_TEST_FUNC_CLASS_bz_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_cb() {
#     try {
#       CLASS_cb v = new CLASS_cb();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_cb()V
    .locals 7
    :call_TEST_FUNC_CLASS_cb_try_start
      new-instance v6, LCLASS_cb;
      invoke-direct {v6}, LCLASS_cb;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_cb;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_cb;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_cb_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_cb_try_start .. :call_TEST_FUNC_CLASS_cb_try_end} :error_TEST_FUNC_CLASS_cb_start
    :error_TEST_FUNC_CLASS_cb_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_cd() {
#     try {
#       CLASS_cd v = new CLASS_cd();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_cd()V
    .locals 7
    :call_TEST_FUNC_CLASS_cd_try_start
      new-instance v6, LCLASS_cd;
      invoke-direct {v6}, LCLASS_cd;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_cd;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_cd;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_cd_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_cd_try_start .. :call_TEST_FUNC_CLASS_cd_try_end} :error_TEST_FUNC_CLASS_cd_start
    :error_TEST_FUNC_CLASS_cd_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_cf() {
#     try {
#       CLASS_cf v = new CLASS_cf();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_cf()V
    .locals 7
    :call_TEST_FUNC_CLASS_cf_try_start
      new-instance v6, LCLASS_cf;
      invoke-direct {v6}, LCLASS_cf;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_cf;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_cf;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_cf_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_cf_try_start .. :call_TEST_FUNC_CLASS_cf_try_end} :error_TEST_FUNC_CLASS_cf_start
    :error_TEST_FUNC_CLASS_cf_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_ch() {
#     try {
#       CLASS_ch v = new CLASS_ch();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_ch()V
    .locals 7
    :call_TEST_FUNC_CLASS_ch_try_start
      new-instance v6, LCLASS_ch;
      invoke-direct {v6}, LCLASS_ch;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_ch;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_ch;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_ch_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_ch_try_start .. :call_TEST_FUNC_CLASS_ch_try_end} :error_TEST_FUNC_CLASS_ch_start
    :error_TEST_FUNC_CLASS_ch_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_cj() {
#     try {
#       CLASS_cj v = new CLASS_cj();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_cj()V
    .locals 7
    :call_TEST_FUNC_CLASS_cj_try_start
      new-instance v6, LCLASS_cj;
      invoke-direct {v6}, LCLASS_cj;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_cj;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_cj;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_cj_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_cj_try_start .. :call_TEST_FUNC_CLASS_cj_try_end} :error_TEST_FUNC_CLASS_cj_start
    :error_TEST_FUNC_CLASS_cj_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_cl() {
#     try {
#       CLASS_cl v = new CLASS_cl();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_cl()V
    .locals 7
    :call_TEST_FUNC_CLASS_cl_try_start
      new-instance v6, LCLASS_cl;
      invoke-direct {v6}, LCLASS_cl;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_cl;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_cl;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_cl_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_cl_try_start .. :call_TEST_FUNC_CLASS_cl_try_end} :error_TEST_FUNC_CLASS_cl_start
    :error_TEST_FUNC_CLASS_cl_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_cn() {
#     try {
#       CLASS_cn v = new CLASS_cn();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_cn()V
    .locals 7
    :call_TEST_FUNC_CLASS_cn_try_start
      new-instance v6, LCLASS_cn;
      invoke-direct {v6}, LCLASS_cn;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_cn;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_cn;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_cn_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_cn_try_start .. :call_TEST_FUNC_CLASS_cn_try_end} :error_TEST_FUNC_CLASS_cn_start
    :error_TEST_FUNC_CLASS_cn_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_cp() {
#     try {
#       CLASS_cp v = new CLASS_cp();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_cp()V
    .locals 7
    :call_TEST_FUNC_CLASS_cp_try_start
      new-instance v6, LCLASS_cp;
      invoke-direct {v6}, LCLASS_cp;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_cp;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_cp;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_cp_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_cp_try_start .. :call_TEST_FUNC_CLASS_cp_try_end} :error_TEST_FUNC_CLASS_cp_start
    :error_TEST_FUNC_CLASS_cp_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_cr() {
#     try {
#       CLASS_cr v = new CLASS_cr();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_cr()V
    .locals 7
    :call_TEST_FUNC_CLASS_cr_try_start
      new-instance v6, LCLASS_cr;
      invoke-direct {v6}, LCLASS_cr;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_cr;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_cr;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_cr_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_cr_try_start .. :call_TEST_FUNC_CLASS_cr_try_end} :error_TEST_FUNC_CLASS_cr_start
    :error_TEST_FUNC_CLASS_cr_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_ct() {
#     try {
#       CLASS_ct v = new CLASS_ct();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_ct()V
    .locals 7
    :call_TEST_FUNC_CLASS_ct_try_start
      new-instance v6, LCLASS_ct;
      invoke-direct {v6}, LCLASS_ct;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_ct;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_ct;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_ct_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_ct_try_start .. :call_TEST_FUNC_CLASS_ct_try_end} :error_TEST_FUNC_CLASS_ct_start
    :error_TEST_FUNC_CLASS_ct_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_cv() {
#     try {
#       CLASS_cv v = new CLASS_cv();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_cv()V
    .locals 7
    :call_TEST_FUNC_CLASS_cv_try_start
      new-instance v6, LCLASS_cv;
      invoke-direct {v6}, LCLASS_cv;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_cv;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_cv;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_cv_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_cv_try_start .. :call_TEST_FUNC_CLASS_cv_try_end} :error_TEST_FUNC_CLASS_cv_start
    :error_TEST_FUNC_CLASS_cv_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_cx() {
#     try {
#       CLASS_cx v = new CLASS_cx();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_cx()V
    .locals 7
    :call_TEST_FUNC_CLASS_cx_try_start
      new-instance v6, LCLASS_cx;
      invoke-direct {v6}, LCLASS_cx;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_cx;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_cx;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_cx_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_cx_try_start .. :call_TEST_FUNC_CLASS_cx_try_end} :error_TEST_FUNC_CLASS_cx_start
    :error_TEST_FUNC_CLASS_cx_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_cz() {
#     try {
#       CLASS_cz v = new CLASS_cz();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_cz()V
    .locals 7
    :call_TEST_FUNC_CLASS_cz_try_start
      new-instance v6, LCLASS_cz;
      invoke-direct {v6}, LCLASS_cz;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_cz;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_cz;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_cz_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_cz_try_start .. :call_TEST_FUNC_CLASS_cz_try_end} :error_TEST_FUNC_CLASS_cz_start
    :error_TEST_FUNC_CLASS_cz_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_db() {
#     try {
#       CLASS_db v = new CLASS_db();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_db()V
    .locals 7
    :call_TEST_FUNC_CLASS_db_try_start
      new-instance v6, LCLASS_db;
      invoke-direct {v6}, LCLASS_db;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_db;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_db;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_db_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_db_try_start .. :call_TEST_FUNC_CLASS_db_try_end} :error_TEST_FUNC_CLASS_db_start
    :error_TEST_FUNC_CLASS_db_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_dd() {
#     try {
#       CLASS_dd v = new CLASS_dd();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_dd()V
    .locals 7
    :call_TEST_FUNC_CLASS_dd_try_start
      new-instance v6, LCLASS_dd;
      invoke-direct {v6}, LCLASS_dd;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_dd;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_dd;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_dd_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_dd_try_start .. :call_TEST_FUNC_CLASS_dd_try_end} :error_TEST_FUNC_CLASS_dd_start
    :error_TEST_FUNC_CLASS_dd_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_df() {
#     try {
#       CLASS_df v = new CLASS_df();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_df()V
    .locals 7
    :call_TEST_FUNC_CLASS_df_try_start
      new-instance v6, LCLASS_df;
      invoke-direct {v6}, LCLASS_df;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_df;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_df;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_df_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_df_try_start .. :call_TEST_FUNC_CLASS_df_try_end} :error_TEST_FUNC_CLASS_df_start
    :error_TEST_FUNC_CLASS_df_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_dh() {
#     try {
#       CLASS_dh v = new CLASS_dh();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_dh()V
    .locals 7
    :call_TEST_FUNC_CLASS_dh_try_start
      new-instance v6, LCLASS_dh;
      invoke-direct {v6}, LCLASS_dh;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_dh;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_dh;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_dh_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_dh_try_start .. :call_TEST_FUNC_CLASS_dh_try_end} :error_TEST_FUNC_CLASS_dh_start
    :error_TEST_FUNC_CLASS_dh_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_dj() {
#     try {
#       CLASS_dj v = new CLASS_dj();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_dj()V
    .locals 7
    :call_TEST_FUNC_CLASS_dj_try_start
      new-instance v6, LCLASS_dj;
      invoke-direct {v6}, LCLASS_dj;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_dj;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_dj;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_dj_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_dj_try_start .. :call_TEST_FUNC_CLASS_dj_try_end} :error_TEST_FUNC_CLASS_dj_start
    :error_TEST_FUNC_CLASS_dj_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_dl() {
#     try {
#       CLASS_dl v = new CLASS_dl();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_dl()V
    .locals 7
    :call_TEST_FUNC_CLASS_dl_try_start
      new-instance v6, LCLASS_dl;
      invoke-direct {v6}, LCLASS_dl;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_dl;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_dl;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_dl_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_dl_try_start .. :call_TEST_FUNC_CLASS_dl_try_end} :error_TEST_FUNC_CLASS_dl_start
    :error_TEST_FUNC_CLASS_dl_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_dn() {
#     try {
#       CLASS_dn v = new CLASS_dn();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_dn()V
    .locals 7
    :call_TEST_FUNC_CLASS_dn_try_start
      new-instance v6, LCLASS_dn;
      invoke-direct {v6}, LCLASS_dn;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_dn;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_dn;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_dn_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_dn_try_start .. :call_TEST_FUNC_CLASS_dn_try_end} :error_TEST_FUNC_CLASS_dn_start
    :error_TEST_FUNC_CLASS_dn_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_dp() {
#     try {
#       CLASS_dp v = new CLASS_dp();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_dp()V
    .locals 7
    :call_TEST_FUNC_CLASS_dp_try_start
      new-instance v6, LCLASS_dp;
      invoke-direct {v6}, LCLASS_dp;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_dp;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_dp;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_dp_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_dp_try_start .. :call_TEST_FUNC_CLASS_dp_try_end} :error_TEST_FUNC_CLASS_dp_start
    :error_TEST_FUNC_CLASS_dp_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_dr() {
#     try {
#       CLASS_dr v = new CLASS_dr();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_dr()V
    .locals 7
    :call_TEST_FUNC_CLASS_dr_try_start
      new-instance v6, LCLASS_dr;
      invoke-direct {v6}, LCLASS_dr;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_dr;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_dr;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_dr_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_dr_try_start .. :call_TEST_FUNC_CLASS_dr_try_end} :error_TEST_FUNC_CLASS_dr_start
    :error_TEST_FUNC_CLASS_dr_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_dt() {
#     try {
#       CLASS_dt v = new CLASS_dt();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_dt()V
    .locals 7
    :call_TEST_FUNC_CLASS_dt_try_start
      new-instance v6, LCLASS_dt;
      invoke-direct {v6}, LCLASS_dt;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_dt;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_dt;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_dt_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_dt_try_start .. :call_TEST_FUNC_CLASS_dt_try_end} :error_TEST_FUNC_CLASS_dt_start
    :error_TEST_FUNC_CLASS_dt_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_dv() {
#     try {
#       CLASS_dv v = new CLASS_dv();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_dv()V
    .locals 7
    :call_TEST_FUNC_CLASS_dv_try_start
      new-instance v6, LCLASS_dv;
      invoke-direct {v6}, LCLASS_dv;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_dv;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_dv;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_dv_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_dv_try_start .. :call_TEST_FUNC_CLASS_dv_try_end} :error_TEST_FUNC_CLASS_dv_start
    :error_TEST_FUNC_CLASS_dv_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_dx() {
#     try {
#       CLASS_dx v = new CLASS_dx();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_dx()V
    .locals 7
    :call_TEST_FUNC_CLASS_dx_try_start
      new-instance v6, LCLASS_dx;
      invoke-direct {v6}, LCLASS_dx;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_dx;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_dx;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_dx_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_dx_try_start .. :call_TEST_FUNC_CLASS_dx_try_end} :error_TEST_FUNC_CLASS_dx_start
    :error_TEST_FUNC_CLASS_dx_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_dz() {
#     try {
#       CLASS_dz v = new CLASS_dz();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_dz()V
    .locals 7
    :call_TEST_FUNC_CLASS_dz_try_start
      new-instance v6, LCLASS_dz;
      invoke-direct {v6}, LCLASS_dz;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_dz;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_dz;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_dz_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_dz_try_start .. :call_TEST_FUNC_CLASS_dz_try_end} :error_TEST_FUNC_CLASS_dz_start
    :error_TEST_FUNC_CLASS_dz_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_eb() {
#     try {
#       CLASS_eb v = new CLASS_eb();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_eb()V
    .locals 7
    :call_TEST_FUNC_CLASS_eb_try_start
      new-instance v6, LCLASS_eb;
      invoke-direct {v6}, LCLASS_eb;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_eb;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_eb;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_eb_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_eb_try_start .. :call_TEST_FUNC_CLASS_eb_try_end} :error_TEST_FUNC_CLASS_eb_start
    :error_TEST_FUNC_CLASS_eb_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_ed() {
#     try {
#       CLASS_ed v = new CLASS_ed();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_ed()V
    .locals 7
    :call_TEST_FUNC_CLASS_ed_try_start
      new-instance v6, LCLASS_ed;
      invoke-direct {v6}, LCLASS_ed;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_ed;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_ed;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_ed_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_ed_try_start .. :call_TEST_FUNC_CLASS_ed_try_end} :error_TEST_FUNC_CLASS_ed_start
    :error_TEST_FUNC_CLASS_ed_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_ej() {
#     try {
#       CLASS_ej v = new CLASS_ej();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_ej()V
    .locals 7
    :call_TEST_FUNC_CLASS_ej_try_start
      new-instance v6, LCLASS_ej;
      invoke-direct {v6}, LCLASS_ej;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_ej;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_ej;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_ej_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_ej_try_start .. :call_TEST_FUNC_CLASS_ej_try_end} :error_TEST_FUNC_CLASS_ej_start
    :error_TEST_FUNC_CLASS_ej_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_el() {
#     try {
#       CLASS_el v = new CLASS_el();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_el()V
    .locals 7
    :call_TEST_FUNC_CLASS_el_try_start
      new-instance v6, LCLASS_el;
      invoke-direct {v6}, LCLASS_el;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_el;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_el;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_el_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_el_try_start .. :call_TEST_FUNC_CLASS_el_try_end} :error_TEST_FUNC_CLASS_el_start
    :error_TEST_FUNC_CLASS_el_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_en() {
#     try {
#       CLASS_en v = new CLASS_en();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_en()V
    .locals 7
    :call_TEST_FUNC_CLASS_en_try_start
      new-instance v6, LCLASS_en;
      invoke-direct {v6}, LCLASS_en;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_en;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_en;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_en_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_en_try_start .. :call_TEST_FUNC_CLASS_en_try_end} :error_TEST_FUNC_CLASS_en_start
    :error_TEST_FUNC_CLASS_en_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_ep() {
#     try {
#       CLASS_ep v = new CLASS_ep();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_ep()V
    .locals 7
    :call_TEST_FUNC_CLASS_ep_try_start
      new-instance v6, LCLASS_ep;
      invoke-direct {v6}, LCLASS_ep;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_ep;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_ep;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_ep_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_ep_try_start .. :call_TEST_FUNC_CLASS_ep_try_end} :error_TEST_FUNC_CLASS_ep_start
    :error_TEST_FUNC_CLASS_ep_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_fg() {
#     try {
#       CLASS_fg v = new CLASS_fg();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_fg()V
    .locals 7
    :call_TEST_FUNC_CLASS_fg_try_start
      new-instance v6, LCLASS_fg;
      invoke-direct {v6}, LCLASS_fg;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_fg;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_fg;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_fg_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_fg_try_start .. :call_TEST_FUNC_CLASS_fg_try_end} :error_TEST_FUNC_CLASS_fg_start
    :error_TEST_FUNC_CLASS_fg_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_fi() {
#     try {
#       CLASS_fi v = new CLASS_fi();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_fi()V
    .locals 7
    :call_TEST_FUNC_CLASS_fi_try_start
      new-instance v6, LCLASS_fi;
      invoke-direct {v6}, LCLASS_fi;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_fi;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_fi;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_fi_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_fi_try_start .. :call_TEST_FUNC_CLASS_fi_try_end} :error_TEST_FUNC_CLASS_fi_start
    :error_TEST_FUNC_CLASS_fi_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_fk() {
#     try {
#       CLASS_fk v = new CLASS_fk();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_fk()V
    .locals 7
    :call_TEST_FUNC_CLASS_fk_try_start
      new-instance v6, LCLASS_fk;
      invoke-direct {v6}, LCLASS_fk;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_fk;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_fk;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_fk_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_fk_try_start .. :call_TEST_FUNC_CLASS_fk_try_end} :error_TEST_FUNC_CLASS_fk_start
    :error_TEST_FUNC_CLASS_fk_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_fm() {
#     try {
#       CLASS_fm v = new CLASS_fm();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_fm()V
    .locals 7
    :call_TEST_FUNC_CLASS_fm_try_start
      new-instance v6, LCLASS_fm;
      invoke-direct {v6}, LCLASS_fm;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_fm;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_fm;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_fm_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_fm_try_start .. :call_TEST_FUNC_CLASS_fm_try_end} :error_TEST_FUNC_CLASS_fm_start
    :error_TEST_FUNC_CLASS_fm_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_fo() {
#     try {
#       CLASS_fo v = new CLASS_fo();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_fo()V
    .locals 7
    :call_TEST_FUNC_CLASS_fo_try_start
      new-instance v6, LCLASS_fo;
      invoke-direct {v6}, LCLASS_fo;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_fo;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_fo;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_fo_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_fo_try_start .. :call_TEST_FUNC_CLASS_fo_try_end} :error_TEST_FUNC_CLASS_fo_start
    :error_TEST_FUNC_CLASS_fo_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_fq() {
#     try {
#       CLASS_fq v = new CLASS_fq();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_fq()V
    .locals 7
    :call_TEST_FUNC_CLASS_fq_try_start
      new-instance v6, LCLASS_fq;
      invoke-direct {v6}, LCLASS_fq;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_fq;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_fq;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_fq_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_fq_try_start .. :call_TEST_FUNC_CLASS_fq_try_end} :error_TEST_FUNC_CLASS_fq_start
    :error_TEST_FUNC_CLASS_fq_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_fs() {
#     try {
#       CLASS_fs v = new CLASS_fs();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_fs()V
    .locals 7
    :call_TEST_FUNC_CLASS_fs_try_start
      new-instance v6, LCLASS_fs;
      invoke-direct {v6}, LCLASS_fs;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_fs;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_fs;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_fs_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_fs_try_start .. :call_TEST_FUNC_CLASS_fs_try_end} :error_TEST_FUNC_CLASS_fs_start
    :error_TEST_FUNC_CLASS_fs_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_fu() {
#     try {
#       CLASS_fu v = new CLASS_fu();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_fu()V
    .locals 7
    :call_TEST_FUNC_CLASS_fu_try_start
      new-instance v6, LCLASS_fu;
      invoke-direct {v6}, LCLASS_fu;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_fu;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_fu;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_fu_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_fu_try_start .. :call_TEST_FUNC_CLASS_fu_try_end} :error_TEST_FUNC_CLASS_fu_start
    :error_TEST_FUNC_CLASS_fu_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_fw() {
#     try {
#       CLASS_fw v = new CLASS_fw();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_fw()V
    .locals 7
    :call_TEST_FUNC_CLASS_fw_try_start
      new-instance v6, LCLASS_fw;
      invoke-direct {v6}, LCLASS_fw;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_fw;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_fw;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_fw_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_fw_try_start .. :call_TEST_FUNC_CLASS_fw_try_end} :error_TEST_FUNC_CLASS_fw_start
    :error_TEST_FUNC_CLASS_fw_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_fy() {
#     try {
#       CLASS_fy v = new CLASS_fy();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_fy()V
    .locals 7
    :call_TEST_FUNC_CLASS_fy_try_start
      new-instance v6, LCLASS_fy;
      invoke-direct {v6}, LCLASS_fy;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_fy;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_fy;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_fy_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_fy_try_start .. :call_TEST_FUNC_CLASS_fy_try_end} :error_TEST_FUNC_CLASS_fy_start
    :error_TEST_FUNC_CLASS_fy_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_ga() {
#     try {
#       CLASS_ga v = new CLASS_ga();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_ga()V
    .locals 7
    :call_TEST_FUNC_CLASS_ga_try_start
      new-instance v6, LCLASS_ga;
      invoke-direct {v6}, LCLASS_ga;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_ga;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_ga;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_ga_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_ga_try_start .. :call_TEST_FUNC_CLASS_ga_try_end} :error_TEST_FUNC_CLASS_ga_start
    :error_TEST_FUNC_CLASS_ga_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_gc() {
#     try {
#       CLASS_gc v = new CLASS_gc();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_gc()V
    .locals 7
    :call_TEST_FUNC_CLASS_gc_try_start
      new-instance v6, LCLASS_gc;
      invoke-direct {v6}, LCLASS_gc;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_gc;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_gc;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_gc_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_gc_try_start .. :call_TEST_FUNC_CLASS_gc_try_end} :error_TEST_FUNC_CLASS_gc_start
    :error_TEST_FUNC_CLASS_gc_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_ge() {
#     try {
#       CLASS_ge v = new CLASS_ge();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_ge()V
    .locals 7
    :call_TEST_FUNC_CLASS_ge_try_start
      new-instance v6, LCLASS_ge;
      invoke-direct {v6}, LCLASS_ge;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_ge;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_ge;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_ge_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_ge_try_start .. :call_TEST_FUNC_CLASS_ge_try_end} :error_TEST_FUNC_CLASS_ge_start
    :error_TEST_FUNC_CLASS_ge_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_gg() {
#     try {
#       CLASS_gg v = new CLASS_gg();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_gg()V
    .locals 7
    :call_TEST_FUNC_CLASS_gg_try_start
      new-instance v6, LCLASS_gg;
      invoke-direct {v6}, LCLASS_gg;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_gg;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_gg;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_gg_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_gg_try_start .. :call_TEST_FUNC_CLASS_gg_try_end} :error_TEST_FUNC_CLASS_gg_start
    :error_TEST_FUNC_CLASS_gg_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_gi() {
#     try {
#       CLASS_gi v = new CLASS_gi();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_gi()V
    .locals 7
    :call_TEST_FUNC_CLASS_gi_try_start
      new-instance v6, LCLASS_gi;
      invoke-direct {v6}, LCLASS_gi;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_gi;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_gi;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_gi_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_gi_try_start .. :call_TEST_FUNC_CLASS_gi_try_end} :error_TEST_FUNC_CLASS_gi_start
    :error_TEST_FUNC_CLASS_gi_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_gk() {
#     try {
#       CLASS_gk v = new CLASS_gk();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_gk()V
    .locals 7
    :call_TEST_FUNC_CLASS_gk_try_start
      new-instance v6, LCLASS_gk;
      invoke-direct {v6}, LCLASS_gk;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_gk;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_gk;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_gk_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_gk_try_start .. :call_TEST_FUNC_CLASS_gk_try_end} :error_TEST_FUNC_CLASS_gk_start
    :error_TEST_FUNC_CLASS_gk_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_gm() {
#     try {
#       CLASS_gm v = new CLASS_gm();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_gm()V
    .locals 7
    :call_TEST_FUNC_CLASS_gm_try_start
      new-instance v6, LCLASS_gm;
      invoke-direct {v6}, LCLASS_gm;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_gm;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_gm;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_gm_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_gm_try_start .. :call_TEST_FUNC_CLASS_gm_try_end} :error_TEST_FUNC_CLASS_gm_start
    :error_TEST_FUNC_CLASS_gm_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_go() {
#     try {
#       CLASS_go v = new CLASS_go();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_go()V
    .locals 7
    :call_TEST_FUNC_CLASS_go_try_start
      new-instance v6, LCLASS_go;
      invoke-direct {v6}, LCLASS_go;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_go;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_go;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_go_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_go_try_start .. :call_TEST_FUNC_CLASS_go_try_end} :error_TEST_FUNC_CLASS_go_start
    :error_TEST_FUNC_CLASS_go_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_gq() {
#     try {
#       CLASS_gq v = new CLASS_gq();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_gq()V
    .locals 7
    :call_TEST_FUNC_CLASS_gq_try_start
      new-instance v6, LCLASS_gq;
      invoke-direct {v6}, LCLASS_gq;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_gq;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_gq;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_gq_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_gq_try_start .. :call_TEST_FUNC_CLASS_gq_try_end} :error_TEST_FUNC_CLASS_gq_start
    :error_TEST_FUNC_CLASS_gq_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_gs() {
#     try {
#       CLASS_gs v = new CLASS_gs();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_gs()V
    .locals 7
    :call_TEST_FUNC_CLASS_gs_try_start
      new-instance v6, LCLASS_gs;
      invoke-direct {v6}, LCLASS_gs;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_gs;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_gs;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_gs_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_gs_try_start .. :call_TEST_FUNC_CLASS_gs_try_end} :error_TEST_FUNC_CLASS_gs_start
    :error_TEST_FUNC_CLASS_gs_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_gu() {
#     try {
#       CLASS_gu v = new CLASS_gu();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_gu()V
    .locals 7
    :call_TEST_FUNC_CLASS_gu_try_start
      new-instance v6, LCLASS_gu;
      invoke-direct {v6}, LCLASS_gu;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_gu;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_gu;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_gu_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_gu_try_start .. :call_TEST_FUNC_CLASS_gu_try_end} :error_TEST_FUNC_CLASS_gu_start
    :error_TEST_FUNC_CLASS_gu_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_gw() {
#     try {
#       CLASS_gw v = new CLASS_gw();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_gw()V
    .locals 7
    :call_TEST_FUNC_CLASS_gw_try_start
      new-instance v6, LCLASS_gw;
      invoke-direct {v6}, LCLASS_gw;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_gw;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_gw;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_gw_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_gw_try_start .. :call_TEST_FUNC_CLASS_gw_try_end} :error_TEST_FUNC_CLASS_gw_start
    :error_TEST_FUNC_CLASS_gw_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_gy() {
#     try {
#       CLASS_gy v = new CLASS_gy();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_gy()V
    .locals 7
    :call_TEST_FUNC_CLASS_gy_try_start
      new-instance v6, LCLASS_gy;
      invoke-direct {v6}, LCLASS_gy;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_gy;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_gy;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_gy_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_gy_try_start .. :call_TEST_FUNC_CLASS_gy_try_end} :error_TEST_FUNC_CLASS_gy_start
    :error_TEST_FUNC_CLASS_gy_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_h() {
#     try {
#       CLASS_h v = new CLASS_h();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_h()V
    .locals 7
    :call_TEST_FUNC_CLASS_h_try_start
      new-instance v6, LCLASS_h;
      invoke-direct {v6}, LCLASS_h;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_h;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_h;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_h_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_h_try_start .. :call_TEST_FUNC_CLASS_h_try_end} :error_TEST_FUNC_CLASS_h_start
    :error_TEST_FUNC_CLASS_h_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_ha() {
#     try {
#       CLASS_ha v = new CLASS_ha();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_ha()V
    .locals 7
    :call_TEST_FUNC_CLASS_ha_try_start
      new-instance v6, LCLASS_ha;
      invoke-direct {v6}, LCLASS_ha;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_ha;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_ha;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_ha_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_ha_try_start .. :call_TEST_FUNC_CLASS_ha_try_end} :error_TEST_FUNC_CLASS_ha_start
    :error_TEST_FUNC_CLASS_ha_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_hi() {
#     try {
#       CLASS_hi v = new CLASS_hi();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_hi()V
    .locals 7
    :call_TEST_FUNC_CLASS_hi_try_start
      new-instance v6, LCLASS_hi;
      invoke-direct {v6}, LCLASS_hi;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_hi;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_hi;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_hi_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_hi_try_start .. :call_TEST_FUNC_CLASS_hi_try_end} :error_TEST_FUNC_CLASS_hi_start
    :error_TEST_FUNC_CLASS_hi_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_hk() {
#     try {
#       CLASS_hk v = new CLASS_hk();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_hk()V
    .locals 7
    :call_TEST_FUNC_CLASS_hk_try_start
      new-instance v6, LCLASS_hk;
      invoke-direct {v6}, LCLASS_hk;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_hk;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_hk;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_hk_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_hk_try_start .. :call_TEST_FUNC_CLASS_hk_try_end} :error_TEST_FUNC_CLASS_hk_start
    :error_TEST_FUNC_CLASS_hk_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_hm() {
#     try {
#       CLASS_hm v = new CLASS_hm();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_hm()V
    .locals 7
    :call_TEST_FUNC_CLASS_hm_try_start
      new-instance v6, LCLASS_hm;
      invoke-direct {v6}, LCLASS_hm;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_hm;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_hm;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_hm_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_hm_try_start .. :call_TEST_FUNC_CLASS_hm_try_end} :error_TEST_FUNC_CLASS_hm_start
    :error_TEST_FUNC_CLASS_hm_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_ho() {
#     try {
#       CLASS_ho v = new CLASS_ho();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_ho()V
    .locals 7
    :call_TEST_FUNC_CLASS_ho_try_start
      new-instance v6, LCLASS_ho;
      invoke-direct {v6}, LCLASS_ho;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_ho;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_ho;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_ho_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_ho_try_start .. :call_TEST_FUNC_CLASS_ho_try_end} :error_TEST_FUNC_CLASS_ho_start
    :error_TEST_FUNC_CLASS_ho_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_hq() {
#     try {
#       CLASS_hq v = new CLASS_hq();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_hq()V
    .locals 7
    :call_TEST_FUNC_CLASS_hq_try_start
      new-instance v6, LCLASS_hq;
      invoke-direct {v6}, LCLASS_hq;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_hq;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_hq;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_hq_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_hq_try_start .. :call_TEST_FUNC_CLASS_hq_try_end} :error_TEST_FUNC_CLASS_hq_start
    :error_TEST_FUNC_CLASS_hq_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_hs() {
#     try {
#       CLASS_hs v = new CLASS_hs();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_hs()V
    .locals 7
    :call_TEST_FUNC_CLASS_hs_try_start
      new-instance v6, LCLASS_hs;
      invoke-direct {v6}, LCLASS_hs;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_hs;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_hs;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_hs_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_hs_try_start .. :call_TEST_FUNC_CLASS_hs_try_end} :error_TEST_FUNC_CLASS_hs_start
    :error_TEST_FUNC_CLASS_hs_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_hu() {
#     try {
#       CLASS_hu v = new CLASS_hu();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_hu()V
    .locals 7
    :call_TEST_FUNC_CLASS_hu_try_start
      new-instance v6, LCLASS_hu;
      invoke-direct {v6}, LCLASS_hu;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_hu;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_hu;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_hu_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_hu_try_start .. :call_TEST_FUNC_CLASS_hu_try_end} :error_TEST_FUNC_CLASS_hu_start
    :error_TEST_FUNC_CLASS_hu_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_hw() {
#     try {
#       CLASS_hw v = new CLASS_hw();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_hw()V
    .locals 7
    :call_TEST_FUNC_CLASS_hw_try_start
      new-instance v6, LCLASS_hw;
      invoke-direct {v6}, LCLASS_hw;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_hw;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_hw;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_hw_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_hw_try_start .. :call_TEST_FUNC_CLASS_hw_try_end} :error_TEST_FUNC_CLASS_hw_start
    :error_TEST_FUNC_CLASS_hw_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_ib() {
#     try {
#       CLASS_ib v = new CLASS_ib();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_ib()V
    .locals 7
    :call_TEST_FUNC_CLASS_ib_try_start
      new-instance v6, LCLASS_ib;
      invoke-direct {v6}, LCLASS_ib;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_ib;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_ib;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_ib_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_ib_try_start .. :call_TEST_FUNC_CLASS_ib_try_end} :error_TEST_FUNC_CLASS_ib_start
    :error_TEST_FUNC_CLASS_ib_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_ig() {
#     try {
#       CLASS_ig v = new CLASS_ig();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_ig()V
    .locals 7
    :call_TEST_FUNC_CLASS_ig_try_start
      new-instance v6, LCLASS_ig;
      invoke-direct {v6}, LCLASS_ig;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_ig;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_ig;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_ig_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_ig_try_start .. :call_TEST_FUNC_CLASS_ig_try_end} :error_TEST_FUNC_CLASS_ig_start
    :error_TEST_FUNC_CLASS_ig_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_il() {
#     try {
#       CLASS_il v = new CLASS_il();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_il()V
    .locals 7
    :call_TEST_FUNC_CLASS_il_try_start
      new-instance v6, LCLASS_il;
      invoke-direct {v6}, LCLASS_il;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_il;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_il;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_il_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_il_try_start .. :call_TEST_FUNC_CLASS_il_try_end} :error_TEST_FUNC_CLASS_il_start
    :error_TEST_FUNC_CLASS_il_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_j() {
#     try {
#       CLASS_j v = new CLASS_j();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_j()V
    .locals 7
    :call_TEST_FUNC_CLASS_j_try_start
      new-instance v6, LCLASS_j;
      invoke-direct {v6}, LCLASS_j;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_j;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_j;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_j_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_j_try_start .. :call_TEST_FUNC_CLASS_j_try_end} :error_TEST_FUNC_CLASS_j_start
    :error_TEST_FUNC_CLASS_j_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_l() {
#     try {
#       CLASS_l v = new CLASS_l();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_l()V
    .locals 7
    :call_TEST_FUNC_CLASS_l_try_start
      new-instance v6, LCLASS_l;
      invoke-direct {v6}, LCLASS_l;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_l;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_l;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_l_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_l_try_start .. :call_TEST_FUNC_CLASS_l_try_end} :error_TEST_FUNC_CLASS_l_start
    :error_TEST_FUNC_CLASS_l_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_n() {
#     try {
#       CLASS_n v = new CLASS_n();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_n()V
    .locals 7
    :call_TEST_FUNC_CLASS_n_try_start
      new-instance v6, LCLASS_n;
      invoke-direct {v6}, LCLASS_n;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_n;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_n;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_n_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_n_try_start .. :call_TEST_FUNC_CLASS_n_try_end} :error_TEST_FUNC_CLASS_n_start
    :error_TEST_FUNC_CLASS_n_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_r() {
#     try {
#       CLASS_r v = new CLASS_r();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_r()V
    .locals 7
    :call_TEST_FUNC_CLASS_r_try_start
      new-instance v6, LCLASS_r;
      invoke-direct {v6}, LCLASS_r;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_r;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_r;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_r_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_r_try_start .. :call_TEST_FUNC_CLASS_r_try_end} :error_TEST_FUNC_CLASS_r_start
    :error_TEST_FUNC_CLASS_r_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void TEST_FUNC_CLASS_v() {
#     try {
#       CLASS_v v = new CLASS_v();
#       System.out.printf("%s calls default method on %s\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     } catch (Error e) {
#       e.printStackTrace(System.out);
#       return;
#     }
#   }
.method public static TEST_FUNC_CLASS_v()V
    .locals 7
    :call_TEST_FUNC_CLASS_v_try_start
      new-instance v6, LCLASS_v;
      invoke-direct {v6}, LCLASS_v;-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {v6}, LCLASS_v;->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\n"

      invoke-virtual {v6}, LCLASS_v;->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_TEST_FUNC_CLASS_v_try_end
    .catch Ljava/lang/Error; {:call_TEST_FUNC_CLASS_v_try_start .. :call_TEST_FUNC_CLASS_v_try_end} :error_TEST_FUNC_CLASS_v_start
    :error_TEST_FUNC_CLASS_v_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method



#   public static void main(String[] args) {
.method public static main([Ljava/lang/String;)V
    .locals 2
    sget-object v1, Ljava/lang/System;->out:Ljava/io/PrintStream;

    
#     TEST_FUNC_CLASS_an();
    invoke-static {}, TEST_FUNC_CLASS_an()V

#     TEST_FUNC_CLASS_ap();
    invoke-static {}, TEST_FUNC_CLASS_ap()V

#     TEST_FUNC_CLASS_ar();
    invoke-static {}, TEST_FUNC_CLASS_ar()V

#     TEST_FUNC_CLASS_at();
    invoke-static {}, TEST_FUNC_CLASS_at()V

#     TEST_FUNC_CLASS_av();
    invoke-static {}, TEST_FUNC_CLASS_av()V

#     TEST_FUNC_CLASS_ax();
    invoke-static {}, TEST_FUNC_CLASS_ax()V

#     TEST_FUNC_CLASS_az();
    invoke-static {}, TEST_FUNC_CLASS_az()V

#     TEST_FUNC_CLASS_b();
    invoke-static {}, TEST_FUNC_CLASS_b()V

#     TEST_FUNC_CLASS_bb();
    invoke-static {}, TEST_FUNC_CLASS_bb()V

#     TEST_FUNC_CLASS_bd();
    invoke-static {}, TEST_FUNC_CLASS_bd()V

#     TEST_FUNC_CLASS_bf();
    invoke-static {}, TEST_FUNC_CLASS_bf()V

#     TEST_FUNC_CLASS_bh();
    invoke-static {}, TEST_FUNC_CLASS_bh()V

#     TEST_FUNC_CLASS_bj();
    invoke-static {}, TEST_FUNC_CLASS_bj()V

#     TEST_FUNC_CLASS_bl();
    invoke-static {}, TEST_FUNC_CLASS_bl()V

#     TEST_FUNC_CLASS_bn();
    invoke-static {}, TEST_FUNC_CLASS_bn()V

#     TEST_FUNC_CLASS_bp();
    invoke-static {}, TEST_FUNC_CLASS_bp()V

#     TEST_FUNC_CLASS_br();
    invoke-static {}, TEST_FUNC_CLASS_br()V

#     TEST_FUNC_CLASS_bt();
    invoke-static {}, TEST_FUNC_CLASS_bt()V

#     TEST_FUNC_CLASS_bv();
    invoke-static {}, TEST_FUNC_CLASS_bv()V

#     TEST_FUNC_CLASS_bx();
    invoke-static {}, TEST_FUNC_CLASS_bx()V

#     TEST_FUNC_CLASS_bz();
    invoke-static {}, TEST_FUNC_CLASS_bz()V

#     TEST_FUNC_CLASS_cb();
    invoke-static {}, TEST_FUNC_CLASS_cb()V

#     TEST_FUNC_CLASS_cd();
    invoke-static {}, TEST_FUNC_CLASS_cd()V

#     TEST_FUNC_CLASS_cf();
    invoke-static {}, TEST_FUNC_CLASS_cf()V

#     TEST_FUNC_CLASS_ch();
    invoke-static {}, TEST_FUNC_CLASS_ch()V

#     TEST_FUNC_CLASS_cj();
    invoke-static {}, TEST_FUNC_CLASS_cj()V

#     TEST_FUNC_CLASS_cl();
    invoke-static {}, TEST_FUNC_CLASS_cl()V

#     TEST_FUNC_CLASS_cn();
    invoke-static {}, TEST_FUNC_CLASS_cn()V

#     TEST_FUNC_CLASS_cp();
    invoke-static {}, TEST_FUNC_CLASS_cp()V

#     TEST_FUNC_CLASS_cr();
    invoke-static {}, TEST_FUNC_CLASS_cr()V

#     TEST_FUNC_CLASS_ct();
    invoke-static {}, TEST_FUNC_CLASS_ct()V

#     TEST_FUNC_CLASS_cv();
    invoke-static {}, TEST_FUNC_CLASS_cv()V

#     TEST_FUNC_CLASS_cx();
    invoke-static {}, TEST_FUNC_CLASS_cx()V

#     TEST_FUNC_CLASS_cz();
    invoke-static {}, TEST_FUNC_CLASS_cz()V

#     TEST_FUNC_CLASS_db();
    invoke-static {}, TEST_FUNC_CLASS_db()V

#     TEST_FUNC_CLASS_dd();
    invoke-static {}, TEST_FUNC_CLASS_dd()V

#     TEST_FUNC_CLASS_df();
    invoke-static {}, TEST_FUNC_CLASS_df()V

#     TEST_FUNC_CLASS_dh();
    invoke-static {}, TEST_FUNC_CLASS_dh()V

#     TEST_FUNC_CLASS_dj();
    invoke-static {}, TEST_FUNC_CLASS_dj()V

#     TEST_FUNC_CLASS_dl();
    invoke-static {}, TEST_FUNC_CLASS_dl()V

#     TEST_FUNC_CLASS_dn();
    invoke-static {}, TEST_FUNC_CLASS_dn()V

#     TEST_FUNC_CLASS_dp();
    invoke-static {}, TEST_FUNC_CLASS_dp()V

#     TEST_FUNC_CLASS_dr();
    invoke-static {}, TEST_FUNC_CLASS_dr()V

#     TEST_FUNC_CLASS_dt();
    invoke-static {}, TEST_FUNC_CLASS_dt()V

#     TEST_FUNC_CLASS_dv();
    invoke-static {}, TEST_FUNC_CLASS_dv()V

#     TEST_FUNC_CLASS_dx();
    invoke-static {}, TEST_FUNC_CLASS_dx()V

#     TEST_FUNC_CLASS_dz();
    invoke-static {}, TEST_FUNC_CLASS_dz()V

#     TEST_FUNC_CLASS_eb();
    invoke-static {}, TEST_FUNC_CLASS_eb()V

#     TEST_FUNC_CLASS_ed();
    invoke-static {}, TEST_FUNC_CLASS_ed()V

#     TEST_FUNC_CLASS_ej();
    invoke-static {}, TEST_FUNC_CLASS_ej()V

#     TEST_FUNC_CLASS_el();
    invoke-static {}, TEST_FUNC_CLASS_el()V

#     TEST_FUNC_CLASS_en();
    invoke-static {}, TEST_FUNC_CLASS_en()V

#     TEST_FUNC_CLASS_ep();
    invoke-static {}, TEST_FUNC_CLASS_ep()V

#     TEST_FUNC_CLASS_fg();
    invoke-static {}, TEST_FUNC_CLASS_fg()V

#     TEST_FUNC_CLASS_fi();
    invoke-static {}, TEST_FUNC_CLASS_fi()V

#     TEST_FUNC_CLASS_fk();
    invoke-static {}, TEST_FUNC_CLASS_fk()V

#     TEST_FUNC_CLASS_fm();
    invoke-static {}, TEST_FUNC_CLASS_fm()V

#     TEST_FUNC_CLASS_fo();
    invoke-static {}, TEST_FUNC_CLASS_fo()V

#     TEST_FUNC_CLASS_fq();
    invoke-static {}, TEST_FUNC_CLASS_fq()V

#     TEST_FUNC_CLASS_fs();
    invoke-static {}, TEST_FUNC_CLASS_fs()V

#     TEST_FUNC_CLASS_fu();
    invoke-static {}, TEST_FUNC_CLASS_fu()V

#     TEST_FUNC_CLASS_fw();
    invoke-static {}, TEST_FUNC_CLASS_fw()V

#     TEST_FUNC_CLASS_fy();
    invoke-static {}, TEST_FUNC_CLASS_fy()V

#     TEST_FUNC_CLASS_ga();
    invoke-static {}, TEST_FUNC_CLASS_ga()V

#     TEST_FUNC_CLASS_gc();
    invoke-static {}, TEST_FUNC_CLASS_gc()V

#     TEST_FUNC_CLASS_ge();
    invoke-static {}, TEST_FUNC_CLASS_ge()V

#     TEST_FUNC_CLASS_gg();
    invoke-static {}, TEST_FUNC_CLASS_gg()V

#     TEST_FUNC_CLASS_gi();
    invoke-static {}, TEST_FUNC_CLASS_gi()V

#     TEST_FUNC_CLASS_gk();
    invoke-static {}, TEST_FUNC_CLASS_gk()V

#     TEST_FUNC_CLASS_gm();
    invoke-static {}, TEST_FUNC_CLASS_gm()V

#     TEST_FUNC_CLASS_go();
    invoke-static {}, TEST_FUNC_CLASS_go()V

#     TEST_FUNC_CLASS_gq();
    invoke-static {}, TEST_FUNC_CLASS_gq()V

#     TEST_FUNC_CLASS_gs();
    invoke-static {}, TEST_FUNC_CLASS_gs()V

#     TEST_FUNC_CLASS_gu();
    invoke-static {}, TEST_FUNC_CLASS_gu()V

#     TEST_FUNC_CLASS_gw();
    invoke-static {}, TEST_FUNC_CLASS_gw()V

#     TEST_FUNC_CLASS_gy();
    invoke-static {}, TEST_FUNC_CLASS_gy()V

#     TEST_FUNC_CLASS_h();
    invoke-static {}, TEST_FUNC_CLASS_h()V

#     TEST_FUNC_CLASS_ha();
    invoke-static {}, TEST_FUNC_CLASS_ha()V

#     TEST_FUNC_CLASS_hi();
    invoke-static {}, TEST_FUNC_CLASS_hi()V

#     TEST_FUNC_CLASS_hk();
    invoke-static {}, TEST_FUNC_CLASS_hk()V

#     TEST_FUNC_CLASS_hm();
    invoke-static {}, TEST_FUNC_CLASS_hm()V

#     TEST_FUNC_CLASS_ho();
    invoke-static {}, TEST_FUNC_CLASS_ho()V

#     TEST_FUNC_CLASS_hq();
    invoke-static {}, TEST_FUNC_CLASS_hq()V

#     TEST_FUNC_CLASS_hs();
    invoke-static {}, TEST_FUNC_CLASS_hs()V

#     TEST_FUNC_CLASS_hu();
    invoke-static {}, TEST_FUNC_CLASS_hu()V

#     TEST_FUNC_CLASS_hw();
    invoke-static {}, TEST_FUNC_CLASS_hw()V

#     TEST_FUNC_CLASS_ib();
    invoke-static {}, TEST_FUNC_CLASS_ib()V

#     TEST_FUNC_CLASS_ig();
    invoke-static {}, TEST_FUNC_CLASS_ig()V

#     TEST_FUNC_CLASS_il();
    invoke-static {}, TEST_FUNC_CLASS_il()V

#     TEST_FUNC_CLASS_j();
    invoke-static {}, TEST_FUNC_CLASS_j()V

#     TEST_FUNC_CLASS_l();
    invoke-static {}, TEST_FUNC_CLASS_l()V

#     TEST_FUNC_CLASS_n();
    invoke-static {}, TEST_FUNC_CLASS_n()V

#     TEST_FUNC_CLASS_r();
    invoke-static {}, TEST_FUNC_CLASS_r()V

#     TEST_FUNC_CLASS_v();
    invoke-static {}, TEST_FUNC_CLASS_v()V


    return-void
.end method
#   }


# }

