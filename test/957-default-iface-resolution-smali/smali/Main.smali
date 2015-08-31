
.class public LMain;
.super Ljava/lang/Object;

# class Main {

.method public constructor <init>()V
    .registers 1
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    return-void
.end method


#   public static void TEST_FUNC_CLASS_aab() {
#     CLASS_aab v = new CLASS_aab();
#     System.out.printf("%s calls default method on %s\n",
#                       v.CalledClassName(),
#                       v.CalledInterfaceName());
#     return;
#   }
.method public static TEST_FUNC_CLASS_aab()V
    .locals 7
    new-instance v6, LCLASS_aab;
    invoke-direct {v6}, LCLASS_aab;-><init>()V

    const/4 v0, 2
    new-array v1,v0, [Ljava/lang/Object;
    const/4 v0, 0
    invoke-virtual {v6}, LCLASS_aab;->CalledClassName()Ljava/lang/String;
    move-result-object v4
    aput-object v4,v1,v0

    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v3, "%s calls default method on %s\n"

    invoke-virtual {v6}, LCLASS_aab;->CalledInterfaceName()Ljava/lang/String;
    move-result-object v4
    const/4 v0, 1
    aput-object v4, v1, v0

    invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
    return-void
.end method

#   public static void TEST_FUNC_CLASS_aaf() {
#     CLASS_aaf v = new CLASS_aaf();
#     System.out.printf("%s calls default method on %s\n",
#                       v.CalledClassName(),
#                       v.CalledInterfaceName());
#     return;
#   }
.method public static TEST_FUNC_CLASS_aaf()V
    .locals 7
    new-instance v6, LCLASS_aaf;
    invoke-direct {v6}, LCLASS_aaf;-><init>()V

    const/4 v0, 2
    new-array v1,v0, [Ljava/lang/Object;
    const/4 v0, 0
    invoke-virtual {v6}, LCLASS_aaf;->CalledClassName()Ljava/lang/String;
    move-result-object v4
    aput-object v4,v1,v0

    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v3, "%s calls default method on %s\n"

    invoke-virtual {v6}, LCLASS_aaf;->CalledInterfaceName()Ljava/lang/String;
    move-result-object v4
    const/4 v0, 1
    aput-object v4, v1, v0

    invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
    return-void
.end method

#   public static void TEST_FUNC_CLASS_aaj() {
#     CLASS_aaj v = new CLASS_aaj();
#     System.out.printf("%s calls default method on %s\n",
#                       v.CalledClassName(),
#                       v.CalledInterfaceName());
#     return;
#   }
.method public static TEST_FUNC_CLASS_aaj()V
    .locals 7
    new-instance v6, LCLASS_aaj;
    invoke-direct {v6}, LCLASS_aaj;-><init>()V

    const/4 v0, 2
    new-array v1,v0, [Ljava/lang/Object;
    const/4 v0, 0
    invoke-virtual {v6}, LCLASS_aaj;->CalledClassName()Ljava/lang/String;
    move-result-object v4
    aput-object v4,v1,v0

    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v3, "%s calls default method on %s\n"

    invoke-virtual {v6}, LCLASS_aaj;->CalledInterfaceName()Ljava/lang/String;
    move-result-object v4
    const/4 v0, 1
    aput-object v4, v1, v0

    invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
    return-void
.end method

#   public static void TEST_FUNC_CLASS_aan() {
#     CLASS_aan v = new CLASS_aan();
#     System.out.printf("%s calls default method on %s\n",
#                       v.CalledClassName(),
#                       v.CalledInterfaceName());
#     return;
#   }
.method public static TEST_FUNC_CLASS_aan()V
    .locals 7
    new-instance v6, LCLASS_aan;
    invoke-direct {v6}, LCLASS_aan;-><init>()V

    const/4 v0, 2
    new-array v1,v0, [Ljava/lang/Object;
    const/4 v0, 0
    invoke-virtual {v6}, LCLASS_aan;->CalledClassName()Ljava/lang/String;
    move-result-object v4
    aput-object v4,v1,v0

    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v3, "%s calls default method on %s\n"

    invoke-virtual {v6}, LCLASS_aan;->CalledInterfaceName()Ljava/lang/String;
    move-result-object v4
    const/4 v0, 1
    aput-object v4, v1, v0

    invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
    return-void
.end method

#   public static void TEST_FUNC_CLASS_aat() {
#     CLASS_aat v = new CLASS_aat();
#     System.out.printf("%s calls default method on %s\n",
#                       v.CalledClassName(),
#                       v.CalledInterfaceName());
#     return;
#   }
.method public static TEST_FUNC_CLASS_aat()V
    .locals 7
    new-instance v6, LCLASS_aat;
    invoke-direct {v6}, LCLASS_aat;-><init>()V

    const/4 v0, 2
    new-array v1,v0, [Ljava/lang/Object;
    const/4 v0, 0
    invoke-virtual {v6}, LCLASS_aat;->CalledClassName()Ljava/lang/String;
    move-result-object v4
    aput-object v4,v1,v0

    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v3, "%s calls default method on %s\n"

    invoke-virtual {v6}, LCLASS_aat;->CalledInterfaceName()Ljava/lang/String;
    move-result-object v4
    const/4 v0, 1
    aput-object v4, v1, v0

    invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
    return-void
.end method

#   public static void TEST_FUNC_CLASS_aav() {
#     CLASS_aav v = new CLASS_aav();
#     System.out.printf("%s calls default method on %s\n",
#                       v.CalledClassName(),
#                       v.CalledInterfaceName());
#     return;
#   }
.method public static TEST_FUNC_CLASS_aav()V
    .locals 7
    new-instance v6, LCLASS_aav;
    invoke-direct {v6}, LCLASS_aav;-><init>()V

    const/4 v0, 2
    new-array v1,v0, [Ljava/lang/Object;
    const/4 v0, 0
    invoke-virtual {v6}, LCLASS_aav;->CalledClassName()Ljava/lang/String;
    move-result-object v4
    aput-object v4,v1,v0

    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v3, "%s calls default method on %s\n"

    invoke-virtual {v6}, LCLASS_aav;->CalledInterfaceName()Ljava/lang/String;
    move-result-object v4
    const/4 v0, 1
    aput-object v4, v1, v0

    invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
    return-void
.end method

#   public static void TEST_FUNC_CLASS_aax() {
#     CLASS_aax v = new CLASS_aax();
#     System.out.printf("%s calls default method on %s\n",
#                       v.CalledClassName(),
#                       v.CalledInterfaceName());
#     return;
#   }
.method public static TEST_FUNC_CLASS_aax()V
    .locals 7
    new-instance v6, LCLASS_aax;
    invoke-direct {v6}, LCLASS_aax;-><init>()V

    const/4 v0, 2
    new-array v1,v0, [Ljava/lang/Object;
    const/4 v0, 0
    invoke-virtual {v6}, LCLASS_aax;->CalledClassName()Ljava/lang/String;
    move-result-object v4
    aput-object v4,v1,v0

    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v3, "%s calls default method on %s\n"

    invoke-virtual {v6}, LCLASS_aax;->CalledInterfaceName()Ljava/lang/String;
    move-result-object v4
    const/4 v0, 1
    aput-object v4, v1, v0

    invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
    return-void
.end method

#   public static void TEST_FUNC_CLASS_aaz() {
#     CLASS_aaz v = new CLASS_aaz();
#     System.out.printf("%s calls default method on %s\n",
#                       v.CalledClassName(),
#                       v.CalledInterfaceName());
#     return;
#   }
.method public static TEST_FUNC_CLASS_aaz()V
    .locals 7
    new-instance v6, LCLASS_aaz;
    invoke-direct {v6}, LCLASS_aaz;-><init>()V

    const/4 v0, 2
    new-array v1,v0, [Ljava/lang/Object;
    const/4 v0, 0
    invoke-virtual {v6}, LCLASS_aaz;->CalledClassName()Ljava/lang/String;
    move-result-object v4
    aput-object v4,v1,v0

    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v3, "%s calls default method on %s\n"

    invoke-virtual {v6}, LCLASS_aaz;->CalledInterfaceName()Ljava/lang/String;
    move-result-object v4
    const/4 v0, 1
    aput-object v4, v1, v0

    invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
    return-void
.end method

#   public static void TEST_FUNC_CLASS_abf() {
#     CLASS_abf v = new CLASS_abf();
#     System.out.printf("%s calls default method on %s\n",
#                       v.CalledClassName(),
#                       v.CalledInterfaceName());
#     return;
#   }
.method public static TEST_FUNC_CLASS_abf()V
    .locals 7
    new-instance v6, LCLASS_abf;
    invoke-direct {v6}, LCLASS_abf;-><init>()V

    const/4 v0, 2
    new-array v1,v0, [Ljava/lang/Object;
    const/4 v0, 0
    invoke-virtual {v6}, LCLASS_abf;->CalledClassName()Ljava/lang/String;
    move-result-object v4
    aput-object v4,v1,v0

    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v3, "%s calls default method on %s\n"

    invoke-virtual {v6}, LCLASS_abf;->CalledInterfaceName()Ljava/lang/String;
    move-result-object v4
    const/4 v0, 1
    aput-object v4, v1, v0

    invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
    return-void
.end method

#   public static void TEST_FUNC_CLASS_abh() {
#     CLASS_abh v = new CLASS_abh();
#     System.out.printf("%s calls default method on %s\n",
#                       v.CalledClassName(),
#                       v.CalledInterfaceName());
#     return;
#   }
.method public static TEST_FUNC_CLASS_abh()V
    .locals 7
    new-instance v6, LCLASS_abh;
    invoke-direct {v6}, LCLASS_abh;-><init>()V

    const/4 v0, 2
    new-array v1,v0, [Ljava/lang/Object;
    const/4 v0, 0
    invoke-virtual {v6}, LCLASS_abh;->CalledClassName()Ljava/lang/String;
    move-result-object v4
    aput-object v4,v1,v0

    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v3, "%s calls default method on %s\n"

    invoke-virtual {v6}, LCLASS_abh;->CalledInterfaceName()Ljava/lang/String;
    move-result-object v4
    const/4 v0, 1
    aput-object v4, v1, v0

    invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
    return-void
.end method

#   public static void TEST_FUNC_CLASS_abj() {
#     CLASS_abj v = new CLASS_abj();
#     System.out.printf("%s calls default method on %s\n",
#                       v.CalledClassName(),
#                       v.CalledInterfaceName());
#     return;
#   }
.method public static TEST_FUNC_CLASS_abj()V
    .locals 7
    new-instance v6, LCLASS_abj;
    invoke-direct {v6}, LCLASS_abj;-><init>()V

    const/4 v0, 2
    new-array v1,v0, [Ljava/lang/Object;
    const/4 v0, 0
    invoke-virtual {v6}, LCLASS_abj;->CalledClassName()Ljava/lang/String;
    move-result-object v4
    aput-object v4,v1,v0

    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v3, "%s calls default method on %s\n"

    invoke-virtual {v6}, LCLASS_abj;->CalledInterfaceName()Ljava/lang/String;
    move-result-object v4
    const/4 v0, 1
    aput-object v4, v1, v0

    invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
    return-void
.end method

#   public static void TEST_FUNC_CLASS_abl() {
#     CLASS_abl v = new CLASS_abl();
#     System.out.printf("%s calls default method on %s\n",
#                       v.CalledClassName(),
#                       v.CalledInterfaceName());
#     return;
#   }
.method public static TEST_FUNC_CLASS_abl()V
    .locals 7
    new-instance v6, LCLASS_abl;
    invoke-direct {v6}, LCLASS_abl;-><init>()V

    const/4 v0, 2
    new-array v1,v0, [Ljava/lang/Object;
    const/4 v0, 0
    invoke-virtual {v6}, LCLASS_abl;->CalledClassName()Ljava/lang/String;
    move-result-object v4
    aput-object v4,v1,v0

    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v3, "%s calls default method on %s\n"

    invoke-virtual {v6}, LCLASS_abl;->CalledInterfaceName()Ljava/lang/String;
    move-result-object v4
    const/4 v0, 1
    aput-object v4, v1, v0

    invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
    return-void
.end method

#   public static void TEST_FUNC_CLASS_abx() {
#     CLASS_abx v = new CLASS_abx();
#     System.out.printf("%s calls default method on %s\n",
#                       v.CalledClassName(),
#                       v.CalledInterfaceName());
#     return;
#   }
.method public static TEST_FUNC_CLASS_abx()V
    .locals 7
    new-instance v6, LCLASS_abx;
    invoke-direct {v6}, LCLASS_abx;-><init>()V

    const/4 v0, 2
    new-array v1,v0, [Ljava/lang/Object;
    const/4 v0, 0
    invoke-virtual {v6}, LCLASS_abx;->CalledClassName()Ljava/lang/String;
    move-result-object v4
    aput-object v4,v1,v0

    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v3, "%s calls default method on %s\n"

    invoke-virtual {v6}, LCLASS_abx;->CalledInterfaceName()Ljava/lang/String;
    move-result-object v4
    const/4 v0, 1
    aput-object v4, v1, v0

    invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
    return-void
.end method

#   public static void TEST_FUNC_CLASS_abz() {
#     CLASS_abz v = new CLASS_abz();
#     System.out.printf("%s calls default method on %s\n",
#                       v.CalledClassName(),
#                       v.CalledInterfaceName());
#     return;
#   }
.method public static TEST_FUNC_CLASS_abz()V
    .locals 7
    new-instance v6, LCLASS_abz;
    invoke-direct {v6}, LCLASS_abz;-><init>()V

    const/4 v0, 2
    new-array v1,v0, [Ljava/lang/Object;
    const/4 v0, 0
    invoke-virtual {v6}, LCLASS_abz;->CalledClassName()Ljava/lang/String;
    move-result-object v4
    aput-object v4,v1,v0

    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v3, "%s calls default method on %s\n"

    invoke-virtual {v6}, LCLASS_abz;->CalledInterfaceName()Ljava/lang/String;
    move-result-object v4
    const/4 v0, 1
    aput-object v4, v1, v0

    invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
    return-void
.end method

#   public static void TEST_FUNC_CLASS_acb() {
#     CLASS_acb v = new CLASS_acb();
#     System.out.printf("%s calls default method on %s\n",
#                       v.CalledClassName(),
#                       v.CalledInterfaceName());
#     return;
#   }
.method public static TEST_FUNC_CLASS_acb()V
    .locals 7
    new-instance v6, LCLASS_acb;
    invoke-direct {v6}, LCLASS_acb;-><init>()V

    const/4 v0, 2
    new-array v1,v0, [Ljava/lang/Object;
    const/4 v0, 0
    invoke-virtual {v6}, LCLASS_acb;->CalledClassName()Ljava/lang/String;
    move-result-object v4
    aput-object v4,v1,v0

    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v3, "%s calls default method on %s\n"

    invoke-virtual {v6}, LCLASS_acb;->CalledInterfaceName()Ljava/lang/String;
    move-result-object v4
    const/4 v0, 1
    aput-object v4, v1, v0

    invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
    return-void
.end method

#   public static void TEST_FUNC_CLASS_acd() {
#     CLASS_acd v = new CLASS_acd();
#     System.out.printf("%s calls default method on %s\n",
#                       v.CalledClassName(),
#                       v.CalledInterfaceName());
#     return;
#   }
.method public static TEST_FUNC_CLASS_acd()V
    .locals 7
    new-instance v6, LCLASS_acd;
    invoke-direct {v6}, LCLASS_acd;-><init>()V

    const/4 v0, 2
    new-array v1,v0, [Ljava/lang/Object;
    const/4 v0, 0
    invoke-virtual {v6}, LCLASS_acd;->CalledClassName()Ljava/lang/String;
    move-result-object v4
    aput-object v4,v1,v0

    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v3, "%s calls default method on %s\n"

    invoke-virtual {v6}, LCLASS_acd;->CalledInterfaceName()Ljava/lang/String;
    move-result-object v4
    const/4 v0, 1
    aput-object v4, v1, v0

    invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
    return-void
.end method

#   public static void TEST_FUNC_CLASS_acf() {
#     CLASS_acf v = new CLASS_acf();
#     System.out.printf("%s calls default method on %s\n",
#                       v.CalledClassName(),
#                       v.CalledInterfaceName());
#     return;
#   }
.method public static TEST_FUNC_CLASS_acf()V
    .locals 7
    new-instance v6, LCLASS_acf;
    invoke-direct {v6}, LCLASS_acf;-><init>()V

    const/4 v0, 2
    new-array v1,v0, [Ljava/lang/Object;
    const/4 v0, 0
    invoke-virtual {v6}, LCLASS_acf;->CalledClassName()Ljava/lang/String;
    move-result-object v4
    aput-object v4,v1,v0

    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v3, "%s calls default method on %s\n"

    invoke-virtual {v6}, LCLASS_acf;->CalledInterfaceName()Ljava/lang/String;
    move-result-object v4
    const/4 v0, 1
    aput-object v4, v1, v0

    invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
    return-void
.end method

#   public static void TEST_FUNC_CLASS_ach() {
#     CLASS_ach v = new CLASS_ach();
#     System.out.printf("%s calls default method on %s\n",
#                       v.CalledClassName(),
#                       v.CalledInterfaceName());
#     return;
#   }
.method public static TEST_FUNC_CLASS_ach()V
    .locals 7
    new-instance v6, LCLASS_ach;
    invoke-direct {v6}, LCLASS_ach;-><init>()V

    const/4 v0, 2
    new-array v1,v0, [Ljava/lang/Object;
    const/4 v0, 0
    invoke-virtual {v6}, LCLASS_ach;->CalledClassName()Ljava/lang/String;
    move-result-object v4
    aput-object v4,v1,v0

    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v3, "%s calls default method on %s\n"

    invoke-virtual {v6}, LCLASS_ach;->CalledInterfaceName()Ljava/lang/String;
    move-result-object v4
    const/4 v0, 1
    aput-object v4, v1, v0

    invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
    return-void
.end method

#   public static void TEST_FUNC_CLASS_acj() {
#     CLASS_acj v = new CLASS_acj();
#     System.out.printf("%s calls default method on %s\n",
#                       v.CalledClassName(),
#                       v.CalledInterfaceName());
#     return;
#   }
.method public static TEST_FUNC_CLASS_acj()V
    .locals 7
    new-instance v6, LCLASS_acj;
    invoke-direct {v6}, LCLASS_acj;-><init>()V

    const/4 v0, 2
    new-array v1,v0, [Ljava/lang/Object;
    const/4 v0, 0
    invoke-virtual {v6}, LCLASS_acj;->CalledClassName()Ljava/lang/String;
    move-result-object v4
    aput-object v4,v1,v0

    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v3, "%s calls default method on %s\n"

    invoke-virtual {v6}, LCLASS_acj;->CalledInterfaceName()Ljava/lang/String;
    move-result-object v4
    const/4 v0, 1
    aput-object v4, v1, v0

    invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
    return-void
.end method

#   public static void TEST_FUNC_CLASS_acl() {
#     CLASS_acl v = new CLASS_acl();
#     System.out.printf("%s calls default method on %s\n",
#                       v.CalledClassName(),
#                       v.CalledInterfaceName());
#     return;
#   }
.method public static TEST_FUNC_CLASS_acl()V
    .locals 7
    new-instance v6, LCLASS_acl;
    invoke-direct {v6}, LCLASS_acl;-><init>()V

    const/4 v0, 2
    new-array v1,v0, [Ljava/lang/Object;
    const/4 v0, 0
    invoke-virtual {v6}, LCLASS_acl;->CalledClassName()Ljava/lang/String;
    move-result-object v4
    aput-object v4,v1,v0

    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v3, "%s calls default method on %s\n"

    invoke-virtual {v6}, LCLASS_acl;->CalledInterfaceName()Ljava/lang/String;
    move-result-object v4
    const/4 v0, 1
    aput-object v4, v1, v0

    invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
    return-void
.end method

#   public static void TEST_FUNC_CLASS_acn() {
#     CLASS_acn v = new CLASS_acn();
#     System.out.printf("%s calls default method on %s\n",
#                       v.CalledClassName(),
#                       v.CalledInterfaceName());
#     return;
#   }
.method public static TEST_FUNC_CLASS_acn()V
    .locals 7
    new-instance v6, LCLASS_acn;
    invoke-direct {v6}, LCLASS_acn;-><init>()V

    const/4 v0, 2
    new-array v1,v0, [Ljava/lang/Object;
    const/4 v0, 0
    invoke-virtual {v6}, LCLASS_acn;->CalledClassName()Ljava/lang/String;
    move-result-object v4
    aput-object v4,v1,v0

    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v3, "%s calls default method on %s\n"

    invoke-virtual {v6}, LCLASS_acn;->CalledInterfaceName()Ljava/lang/String;
    move-result-object v4
    const/4 v0, 1
    aput-object v4, v1, v0

    invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
    return-void
.end method

#   public static void TEST_FUNC_CLASS_acp() {
#     CLASS_acp v = new CLASS_acp();
#     System.out.printf("%s calls default method on %s\n",
#                       v.CalledClassName(),
#                       v.CalledInterfaceName());
#     return;
#   }
.method public static TEST_FUNC_CLASS_acp()V
    .locals 7
    new-instance v6, LCLASS_acp;
    invoke-direct {v6}, LCLASS_acp;-><init>()V

    const/4 v0, 2
    new-array v1,v0, [Ljava/lang/Object;
    const/4 v0, 0
    invoke-virtual {v6}, LCLASS_acp;->CalledClassName()Ljava/lang/String;
    move-result-object v4
    aput-object v4,v1,v0

    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v3, "%s calls default method on %s\n"

    invoke-virtual {v6}, LCLASS_acp;->CalledInterfaceName()Ljava/lang/String;
    move-result-object v4
    const/4 v0, 1
    aput-object v4, v1, v0

    invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
    return-void
.end method

#   public static void TEST_FUNC_CLASS_acr() {
#     CLASS_acr v = new CLASS_acr();
#     System.out.printf("%s calls default method on %s\n",
#                       v.CalledClassName(),
#                       v.CalledInterfaceName());
#     return;
#   }
.method public static TEST_FUNC_CLASS_acr()V
    .locals 7
    new-instance v6, LCLASS_acr;
    invoke-direct {v6}, LCLASS_acr;-><init>()V

    const/4 v0, 2
    new-array v1,v0, [Ljava/lang/Object;
    const/4 v0, 0
    invoke-virtual {v6}, LCLASS_acr;->CalledClassName()Ljava/lang/String;
    move-result-object v4
    aput-object v4,v1,v0

    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v3, "%s calls default method on %s\n"

    invoke-virtual {v6}, LCLASS_acr;->CalledInterfaceName()Ljava/lang/String;
    move-result-object v4
    const/4 v0, 1
    aput-object v4, v1, v0

    invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
    return-void
.end method

#   public static void TEST_FUNC_CLASS_act() {
#     CLASS_act v = new CLASS_act();
#     System.out.printf("%s calls default method on %s\n",
#                       v.CalledClassName(),
#                       v.CalledInterfaceName());
#     return;
#   }
.method public static TEST_FUNC_CLASS_act()V
    .locals 7
    new-instance v6, LCLASS_act;
    invoke-direct {v6}, LCLASS_act;-><init>()V

    const/4 v0, 2
    new-array v1,v0, [Ljava/lang/Object;
    const/4 v0, 0
    invoke-virtual {v6}, LCLASS_act;->CalledClassName()Ljava/lang/String;
    move-result-object v4
    aput-object v4,v1,v0

    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v3, "%s calls default method on %s\n"

    invoke-virtual {v6}, LCLASS_act;->CalledInterfaceName()Ljava/lang/String;
    move-result-object v4
    const/4 v0, 1
    aput-object v4, v1, v0

    invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
    return-void
.end method

#   public static void TEST_FUNC_CLASS_acy() {
#     CLASS_acy v = new CLASS_acy();
#     System.out.printf("%s calls default method on %s\n",
#                       v.CalledClassName(),
#                       v.CalledInterfaceName());
#     return;
#   }
.method public static TEST_FUNC_CLASS_acy()V
    .locals 7
    new-instance v6, LCLASS_acy;
    invoke-direct {v6}, LCLASS_acy;-><init>()V

    const/4 v0, 2
    new-array v1,v0, [Ljava/lang/Object;
    const/4 v0, 0
    invoke-virtual {v6}, LCLASS_acy;->CalledClassName()Ljava/lang/String;
    move-result-object v4
    aput-object v4,v1,v0

    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v3, "%s calls default method on %s\n"

    invoke-virtual {v6}, LCLASS_acy;->CalledInterfaceName()Ljava/lang/String;
    move-result-object v4
    const/4 v0, 1
    aput-object v4, v1, v0

    invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
    return-void
.end method

#   public static void TEST_FUNC_CLASS_add() {
#     CLASS_add v = new CLASS_add();
#     System.out.printf("%s calls default method on %s\n",
#                       v.CalledClassName(),
#                       v.CalledInterfaceName());
#     return;
#   }
.method public static TEST_FUNC_CLASS_add()V
    .locals 7
    new-instance v6, LCLASS_add;
    invoke-direct {v6}, LCLASS_add;-><init>()V

    const/4 v0, 2
    new-array v1,v0, [Ljava/lang/Object;
    const/4 v0, 0
    invoke-virtual {v6}, LCLASS_add;->CalledClassName()Ljava/lang/String;
    move-result-object v4
    aput-object v4,v1,v0

    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v3, "%s calls default method on %s\n"

    invoke-virtual {v6}, LCLASS_add;->CalledInterfaceName()Ljava/lang/String;
    move-result-object v4
    const/4 v0, 1
    aput-object v4, v1, v0

    invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
    return-void
.end method

#   public static void TEST_FUNC_CLASS_adi() {
#     CLASS_adi v = new CLASS_adi();
#     System.out.printf("%s calls default method on %s\n",
#                       v.CalledClassName(),
#                       v.CalledInterfaceName());
#     return;
#   }
.method public static TEST_FUNC_CLASS_adi()V
    .locals 7
    new-instance v6, LCLASS_adi;
    invoke-direct {v6}, LCLASS_adi;-><init>()V

    const/4 v0, 2
    new-array v1,v0, [Ljava/lang/Object;
    const/4 v0, 0
    invoke-virtual {v6}, LCLASS_adi;->CalledClassName()Ljava/lang/String;
    move-result-object v4
    aput-object v4,v1,v0

    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v3, "%s calls default method on %s\n"

    invoke-virtual {v6}, LCLASS_adi;->CalledInterfaceName()Ljava/lang/String;
    move-result-object v4
    const/4 v0, 1
    aput-object v4, v1, v0

    invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
    return-void
.end method

#   public static void TEST_FUNC_CLASS_adn() {
#     CLASS_adn v = new CLASS_adn();
#     System.out.printf("%s calls default method on %s\n",
#                       v.CalledClassName(),
#                       v.CalledInterfaceName());
#     return;
#   }
.method public static TEST_FUNC_CLASS_adn()V
    .locals 7
    new-instance v6, LCLASS_adn;
    invoke-direct {v6}, LCLASS_adn;-><init>()V

    const/4 v0, 2
    new-array v1,v0, [Ljava/lang/Object;
    const/4 v0, 0
    invoke-virtual {v6}, LCLASS_adn;->CalledClassName()Ljava/lang/String;
    move-result-object v4
    aput-object v4,v1,v0

    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v3, "%s calls default method on %s\n"

    invoke-virtual {v6}, LCLASS_adn;->CalledInterfaceName()Ljava/lang/String;
    move-result-object v4
    const/4 v0, 1
    aput-object v4, v1, v0

    invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
    return-void
.end method



#   public static void main(String[] args) {
.method public static main([Ljava/lang/String;)V
    .locals 2
    sget-object v1, Ljava/lang/System;->out:Ljava/io/PrintStream;

    
#     TEST_FUNC_CLASS_aab();
    invoke-static {}, TEST_FUNC_CLASS_aab()V

#     TEST_FUNC_CLASS_aaf();
    invoke-static {}, TEST_FUNC_CLASS_aaf()V

#     TEST_FUNC_CLASS_aaj();
    invoke-static {}, TEST_FUNC_CLASS_aaj()V

#     TEST_FUNC_CLASS_aan();
    invoke-static {}, TEST_FUNC_CLASS_aan()V

#     TEST_FUNC_CLASS_aat();
    invoke-static {}, TEST_FUNC_CLASS_aat()V

#     TEST_FUNC_CLASS_aav();
    invoke-static {}, TEST_FUNC_CLASS_aav()V

#     TEST_FUNC_CLASS_aax();
    invoke-static {}, TEST_FUNC_CLASS_aax()V

#     TEST_FUNC_CLASS_aaz();
    invoke-static {}, TEST_FUNC_CLASS_aaz()V

#     TEST_FUNC_CLASS_abf();
    invoke-static {}, TEST_FUNC_CLASS_abf()V

#     TEST_FUNC_CLASS_abh();
    invoke-static {}, TEST_FUNC_CLASS_abh()V

#     TEST_FUNC_CLASS_abj();
    invoke-static {}, TEST_FUNC_CLASS_abj()V

#     TEST_FUNC_CLASS_abl();
    invoke-static {}, TEST_FUNC_CLASS_abl()V

#     TEST_FUNC_CLASS_abx();
    invoke-static {}, TEST_FUNC_CLASS_abx()V

#     TEST_FUNC_CLASS_abz();
    invoke-static {}, TEST_FUNC_CLASS_abz()V

#     TEST_FUNC_CLASS_acb();
    invoke-static {}, TEST_FUNC_CLASS_acb()V

#     TEST_FUNC_CLASS_acd();
    invoke-static {}, TEST_FUNC_CLASS_acd()V

#     TEST_FUNC_CLASS_acf();
    invoke-static {}, TEST_FUNC_CLASS_acf()V

#     TEST_FUNC_CLASS_ach();
    invoke-static {}, TEST_FUNC_CLASS_ach()V

#     TEST_FUNC_CLASS_acj();
    invoke-static {}, TEST_FUNC_CLASS_acj()V

#     TEST_FUNC_CLASS_acl();
    invoke-static {}, TEST_FUNC_CLASS_acl()V

#     TEST_FUNC_CLASS_acn();
    invoke-static {}, TEST_FUNC_CLASS_acn()V

#     TEST_FUNC_CLASS_acp();
    invoke-static {}, TEST_FUNC_CLASS_acp()V

#     TEST_FUNC_CLASS_acr();
    invoke-static {}, TEST_FUNC_CLASS_acr()V

#     TEST_FUNC_CLASS_act();
    invoke-static {}, TEST_FUNC_CLASS_act()V

#     TEST_FUNC_CLASS_acy();
    invoke-static {}, TEST_FUNC_CLASS_acy()V

#     TEST_FUNC_CLASS_add();
    invoke-static {}, TEST_FUNC_CLASS_add()V

#     TEST_FUNC_CLASS_adi();
    invoke-static {}, TEST_FUNC_CLASS_adi()V

#     TEST_FUNC_CLASS_adn();
    invoke-static {}, TEST_FUNC_CLASS_adn()V


    return-void
.end method
#   }


# }

