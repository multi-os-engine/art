
.class public LMain;
.super Ljava/lang/Object;

# class Main {

.method public constructor <init>()V
    .registers 1
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    return-void
.end method


#   public static void TEST_NAME_A() {
#     System.out.println("Testing for type A");
#     String s = "A";
#     A v = new A();
.method public static TEST_NAME_A()V
    .locals 3
    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v0, "Testing for type A"
    invoke-virtual {v2,v0}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V

    const-string v0, "A"
    new-instance v1, LA;
    invoke-direct {v1}, LA;-><init>()V

    
#     Test_Func_SayHi_A_virtual(s, v);
    invoke-static {v0, v1}, Test_Func_SayHi_A_virtual(Ljava/lang/String;LA;)V

#     Test_Func_SayHi_Greeter_interface(s, v);
    invoke-static {v0, v1}, Test_Func_SayHi_Greeter_interface(Ljava/lang/String;LGreeter;)V

#     Test_Func_SayHiTwice_A_virtual(s, v);
    invoke-static {v0, v1}, Test_Func_SayHiTwice_A_virtual(Ljava/lang/String;LA;)V

#     Test_Func_SayHiTwice_Greeter_interface(s, v);
    invoke-static {v0, v1}, Test_Func_SayHiTwice_Greeter_interface(Ljava/lang/String;LGreeter;)V


    const-string v0, "End testing for type A"
    invoke-virtual {v2,v0}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V
    return-void
.end method
#     System.out.println("End testing for type A");
#   }

#   public static void TEST_NAME_B() {
#     System.out.println("Testing for type B");
#     String s = "B";
#     B v = new B();
.method public static TEST_NAME_B()V
    .locals 3
    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v0, "Testing for type B"
    invoke-virtual {v2,v0}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V

    const-string v0, "B"
    new-instance v1, LB;
    invoke-direct {v1}, LB;-><init>()V

    
#     Test_Func_SayHi_B_virtual(s, v);
    invoke-static {v0, v1}, Test_Func_SayHi_B_virtual(Ljava/lang/String;LB;)V

#     Test_Func_SayHi_Greeter_interface(s, v);
    invoke-static {v0, v1}, Test_Func_SayHi_Greeter_interface(Ljava/lang/String;LGreeter;)V

#     Test_Func_SayHi_Greeter2_interface(s, v);
    invoke-static {v0, v1}, Test_Func_SayHi_Greeter2_interface(Ljava/lang/String;LGreeter2;)V

#     Test_Func_SayHiTwice_B_virtual(s, v);
    invoke-static {v0, v1}, Test_Func_SayHiTwice_B_virtual(Ljava/lang/String;LB;)V

#     Test_Func_SayHiTwice_Greeter_interface(s, v);
    invoke-static {v0, v1}, Test_Func_SayHiTwice_Greeter_interface(Ljava/lang/String;LGreeter;)V

#     Test_Func_SayHiTwice_Greeter2_interface(s, v);
    invoke-static {v0, v1}, Test_Func_SayHiTwice_Greeter2_interface(Ljava/lang/String;LGreeter2;)V


    const-string v0, "End testing for type B"
    invoke-virtual {v2,v0}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V
    return-void
.end method
#     System.out.println("End testing for type B");
#   }

#   public static void TEST_NAME_C() {
#     System.out.println("Testing for type C");
#     String s = "C";
#     C v = new C();
.method public static TEST_NAME_C()V
    .locals 3
    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v0, "Testing for type C"
    invoke-virtual {v2,v0}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V

    const-string v0, "C"
    new-instance v1, LC;
    invoke-direct {v1}, LC;-><init>()V

    
#     Test_Func_SayHi_A_virtual(s, v);
    invoke-static {v0, v1}, Test_Func_SayHi_A_virtual(Ljava/lang/String;LA;)V

#     Test_Func_SayHi_C_virtual(s, v);
    invoke-static {v0, v1}, Test_Func_SayHi_C_virtual(Ljava/lang/String;LC;)V

#     Test_Func_SayHi_Greeter_interface(s, v);
    invoke-static {v0, v1}, Test_Func_SayHi_Greeter_interface(Ljava/lang/String;LGreeter;)V

#     Test_Func_SayHiTwice_A_virtual(s, v);
    invoke-static {v0, v1}, Test_Func_SayHiTwice_A_virtual(Ljava/lang/String;LA;)V

#     Test_Func_SayHiTwice_C_virtual(s, v);
    invoke-static {v0, v1}, Test_Func_SayHiTwice_C_virtual(Ljava/lang/String;LC;)V

#     Test_Func_SayHiTwice_Greeter_interface(s, v);
    invoke-static {v0, v1}, Test_Func_SayHiTwice_Greeter_interface(Ljava/lang/String;LGreeter;)V


    const-string v0, "End testing for type C"
    invoke-virtual {v2,v0}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V
    return-void
.end method
#     System.out.println("End testing for type C");
#   }

#   public static void TEST_NAME_D() {
#     System.out.println("Testing for type D");
#     String s = "D";
#     D v = new D();
.method public static TEST_NAME_D()V
    .locals 3
    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v0, "Testing for type D"
    invoke-virtual {v2,v0}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V

    const-string v0, "D"
    new-instance v1, LD;
    invoke-direct {v1}, LD;-><init>()V

    
#     Test_Func_GetName_D_virtual(s, v);
    invoke-static {v0, v1}, Test_Func_GetName_D_virtual(Ljava/lang/String;LD;)V

#     Test_Func_GetName_Greeter3_interface(s, v);
    invoke-static {v0, v1}, Test_Func_GetName_Greeter3_interface(Ljava/lang/String;LGreeter3;)V

#     Test_Func_SayHi_D_virtual(s, v);
    invoke-static {v0, v1}, Test_Func_SayHi_D_virtual(Ljava/lang/String;LD;)V

#     Test_Func_SayHi_Greeter_interface(s, v);
    invoke-static {v0, v1}, Test_Func_SayHi_Greeter_interface(Ljava/lang/String;LGreeter;)V

#     Test_Func_SayHi_Greeter3_interface(s, v);
    invoke-static {v0, v1}, Test_Func_SayHi_Greeter3_interface(Ljava/lang/String;LGreeter3;)V

#     Test_Func_SayHiTwice_D_virtual(s, v);
    invoke-static {v0, v1}, Test_Func_SayHiTwice_D_virtual(Ljava/lang/String;LD;)V

#     Test_Func_SayHiTwice_Greeter_interface(s, v);
    invoke-static {v0, v1}, Test_Func_SayHiTwice_Greeter_interface(Ljava/lang/String;LGreeter;)V

#     Test_Func_SayHiTwice_Greeter3_interface(s, v);
    invoke-static {v0, v1}, Test_Func_SayHiTwice_Greeter3_interface(Ljava/lang/String;LGreeter3;)V


    const-string v0, "End testing for type D"
    invoke-virtual {v2,v0}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V
    return-void
.end method
#     System.out.println("End testing for type D");
#   }

#   public static void TEST_NAME_E() {
#     System.out.println("Testing for type E");
#     String s = "E";
#     E v = new E();
.method public static TEST_NAME_E()V
    .locals 3
    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v0, "Testing for type E"
    invoke-virtual {v2,v0}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V

    const-string v0, "E"
    new-instance v1, LE;
    invoke-direct {v1}, LE;-><init>()V

    
#     Test_Func_SayHi_A_virtual(s, v);
    invoke-static {v0, v1}, Test_Func_SayHi_A_virtual(Ljava/lang/String;LA;)V

#     Test_Func_SayHi_E_virtual(s, v);
    invoke-static {v0, v1}, Test_Func_SayHi_E_virtual(Ljava/lang/String;LE;)V

#     Test_Func_SayHi_Greeter_interface(s, v);
    invoke-static {v0, v1}, Test_Func_SayHi_Greeter_interface(Ljava/lang/String;LGreeter;)V

#     Test_Func_SayHi_Greeter2_interface(s, v);
    invoke-static {v0, v1}, Test_Func_SayHi_Greeter2_interface(Ljava/lang/String;LGreeter2;)V

#     Test_Func_SayHiTwice_A_virtual(s, v);
    invoke-static {v0, v1}, Test_Func_SayHiTwice_A_virtual(Ljava/lang/String;LA;)V

#     Test_Func_SayHiTwice_E_virtual(s, v);
    invoke-static {v0, v1}, Test_Func_SayHiTwice_E_virtual(Ljava/lang/String;LE;)V

#     Test_Func_SayHiTwice_Greeter_interface(s, v);
    invoke-static {v0, v1}, Test_Func_SayHiTwice_Greeter_interface(Ljava/lang/String;LGreeter;)V

#     Test_Func_SayHiTwice_Greeter2_interface(s, v);
    invoke-static {v0, v1}, Test_Func_SayHiTwice_Greeter2_interface(Ljava/lang/String;LGreeter2;)V


    const-string v0, "End testing for type E"
    invoke-virtual {v2,v0}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V
    return-void
.end method
#     System.out.println("End testing for type E");
#   }

#   public static void TEST_NAME_F() {
#     System.out.println("Testing for type F");
#     String s = "F";
#     F v = new F();
.method public static TEST_NAME_F()V
    .locals 3
    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v0, "Testing for type F"
    invoke-virtual {v2,v0}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V

    const-string v0, "F"
    new-instance v1, LF;
    invoke-direct {v1}, LF;-><init>()V

    
#     Test_Func_GetPlace_Attendant_interface(s, v);
    invoke-static {v0, v1}, Test_Func_GetPlace_Attendant_interface(Ljava/lang/String;LAttendant;)V

#     Test_Func_GetPlace_F_virtual(s, v);
    invoke-static {v0, v1}, Test_Func_GetPlace_F_virtual(Ljava/lang/String;LF;)V

#     Test_Func_SayHi_A_virtual(s, v);
    invoke-static {v0, v1}, Test_Func_SayHi_A_virtual(Ljava/lang/String;LA;)V

#     Test_Func_SayHi_Attendant_interface(s, v);
    invoke-static {v0, v1}, Test_Func_SayHi_Attendant_interface(Ljava/lang/String;LAttendant;)V

#     Test_Func_SayHi_F_virtual(s, v);
    invoke-static {v0, v1}, Test_Func_SayHi_F_virtual(Ljava/lang/String;LF;)V

#     Test_Func_SayHi_Greeter_interface(s, v);
    invoke-static {v0, v1}, Test_Func_SayHi_Greeter_interface(Ljava/lang/String;LGreeter;)V

#     Test_Func_SayHiTwice_A_virtual(s, v);
    invoke-static {v0, v1}, Test_Func_SayHiTwice_A_virtual(Ljava/lang/String;LA;)V

#     Test_Func_SayHiTwice_Attendant_interface(s, v);
    invoke-static {v0, v1}, Test_Func_SayHiTwice_Attendant_interface(Ljava/lang/String;LAttendant;)V

#     Test_Func_SayHiTwice_F_virtual(s, v);
    invoke-static {v0, v1}, Test_Func_SayHiTwice_F_virtual(Ljava/lang/String;LF;)V

#     Test_Func_SayHiTwice_Greeter_interface(s, v);
    invoke-static {v0, v1}, Test_Func_SayHiTwice_Greeter_interface(Ljava/lang/String;LGreeter;)V


    const-string v0, "End testing for type F"
    invoke-virtual {v2,v0}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V
    return-void
.end method
#     System.out.println("End testing for type F");
#   }

#   public static void TEST_NAME_G() {
#     System.out.println("Testing for type G");
#     String s = "G";
#     G v = new G();
.method public static TEST_NAME_G()V
    .locals 3
    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v0, "Testing for type G"
    invoke-virtual {v2,v0}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V

    const-string v0, "G"
    new-instance v1, LG;
    invoke-direct {v1}, LG;-><init>()V

    
#     Test_Func_GetPlace_Attendant_interface(s, v);
    invoke-static {v0, v1}, Test_Func_GetPlace_Attendant_interface(Ljava/lang/String;LAttendant;)V

#     Test_Func_GetPlace_G_virtual(s, v);
    invoke-static {v0, v1}, Test_Func_GetPlace_G_virtual(Ljava/lang/String;LG;)V

#     Test_Func_SayHi_Attendant_interface(s, v);
    invoke-static {v0, v1}, Test_Func_SayHi_Attendant_interface(Ljava/lang/String;LAttendant;)V

#     Test_Func_SayHi_G_virtual(s, v);
    invoke-static {v0, v1}, Test_Func_SayHi_G_virtual(Ljava/lang/String;LG;)V

#     Test_Func_SayHiTwice_Attendant_interface(s, v);
    invoke-static {v0, v1}, Test_Func_SayHiTwice_Attendant_interface(Ljava/lang/String;LAttendant;)V

#     Test_Func_SayHiTwice_G_virtual(s, v);
    invoke-static {v0, v1}, Test_Func_SayHiTwice_G_virtual(Ljava/lang/String;LG;)V


    const-string v0, "End testing for type G"
    invoke-virtual {v2,v0}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V
    return-void
.end method
#     System.out.println("End testing for type G");
#   }



#   public static void Test_Func_SayHi_A_virtual(String s, A v) {
#     try {
#       System.out.printf("%s-virtual           A.SayHi()='%s'\n", s, v.SayHi());
#       return;
#     } catch (Error e) {
#       System.out.printf("%s-virtual on A: SayHi() threw exception!\n", s);
#       e.printStackTrace(System.out);
#     }
#   }
.method public static Test_Func_SayHi_A_virtual(Ljava/lang/String;LA;)V
    .locals 7
    :call_Test_Func_SayHi_A_virtual_try_start
      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s-virtual           A.SayHi()='%s'\n"

      invoke-virtual {p1}, LA;->SayHi()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_Test_Func_SayHi_A_virtual_try_end
    .catch Ljava/lang/Error; {:call_Test_Func_SayHi_A_virtual_try_start .. :call_Test_Func_SayHi_A_virtual_try_end} :error_Test_Func_SayHi_A_virtual_start
    :error_Test_Func_SayHi_A_virtual_start
      move-exception v3
      const/4 v0, 1
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0, v1, v0
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v4, "%s-virtual on A: SayHi() threw exception!\n"
      invoke-virtual {v2,v4,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void Test_Func_SayHiTwice_A_virtual(String s, A v) {
#     try {
#       System.out.printf("%s-virtual           A.SayHiTwice()='%s'\n", s, v.SayHiTwice());
#       return;
#     } catch (Error e) {
#       System.out.printf("%s-virtual on A: SayHiTwice() threw exception!\n", s);
#       e.printStackTrace(System.out);
#     }
#   }
.method public static Test_Func_SayHiTwice_A_virtual(Ljava/lang/String;LA;)V
    .locals 7
    :call_Test_Func_SayHiTwice_A_virtual_try_start
      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s-virtual           A.SayHiTwice()='%s'\n"

      invoke-virtual {p1}, LA;->SayHiTwice()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_Test_Func_SayHiTwice_A_virtual_try_end
    .catch Ljava/lang/Error; {:call_Test_Func_SayHiTwice_A_virtual_try_start .. :call_Test_Func_SayHiTwice_A_virtual_try_end} :error_Test_Func_SayHiTwice_A_virtual_start
    :error_Test_Func_SayHiTwice_A_virtual_start
      move-exception v3
      const/4 v0, 1
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0, v1, v0
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v4, "%s-virtual on A: SayHiTwice() threw exception!\n"
      invoke-virtual {v2,v4,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void Test_Func_SayHi_C_virtual(String s, C v) {
#     try {
#       System.out.printf("%s-virtual           C.SayHi()='%s'\n", s, v.SayHi());
#       return;
#     } catch (Error e) {
#       System.out.printf("%s-virtual on C: SayHi() threw exception!\n", s);
#       e.printStackTrace(System.out);
#     }
#   }
.method public static Test_Func_SayHi_C_virtual(Ljava/lang/String;LC;)V
    .locals 7
    :call_Test_Func_SayHi_C_virtual_try_start
      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s-virtual           C.SayHi()='%s'\n"

      invoke-virtual {p1}, LC;->SayHi()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_Test_Func_SayHi_C_virtual_try_end
    .catch Ljava/lang/Error; {:call_Test_Func_SayHi_C_virtual_try_start .. :call_Test_Func_SayHi_C_virtual_try_end} :error_Test_Func_SayHi_C_virtual_start
    :error_Test_Func_SayHi_C_virtual_start
      move-exception v3
      const/4 v0, 1
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0, v1, v0
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v4, "%s-virtual on C: SayHi() threw exception!\n"
      invoke-virtual {v2,v4,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void Test_Func_SayHiTwice_C_virtual(String s, C v) {
#     try {
#       System.out.printf("%s-virtual           C.SayHiTwice()='%s'\n", s, v.SayHiTwice());
#       return;
#     } catch (Error e) {
#       System.out.printf("%s-virtual on C: SayHiTwice() threw exception!\n", s);
#       e.printStackTrace(System.out);
#     }
#   }
.method public static Test_Func_SayHiTwice_C_virtual(Ljava/lang/String;LC;)V
    .locals 7
    :call_Test_Func_SayHiTwice_C_virtual_try_start
      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s-virtual           C.SayHiTwice()='%s'\n"

      invoke-virtual {p1}, LC;->SayHiTwice()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_Test_Func_SayHiTwice_C_virtual_try_end
    .catch Ljava/lang/Error; {:call_Test_Func_SayHiTwice_C_virtual_try_start .. :call_Test_Func_SayHiTwice_C_virtual_try_end} :error_Test_Func_SayHiTwice_C_virtual_start
    :error_Test_Func_SayHiTwice_C_virtual_start
      move-exception v3
      const/4 v0, 1
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0, v1, v0
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v4, "%s-virtual on C: SayHiTwice() threw exception!\n"
      invoke-virtual {v2,v4,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void Test_Func_GetPlace_Attendant_interface(String s, Attendant v) {
#     try {
#       System.out.printf("%s-interface Attendant.GetPlace()='%s'\n", s, v.GetPlace());
#       return;
#     } catch (Error e) {
#       System.out.printf("%s-interface on Attendant: GetPlace() threw exception!\n", s);
#       e.printStackTrace(System.out);
#     }
#   }
.method public static Test_Func_GetPlace_Attendant_interface(Ljava/lang/String;LAttendant;)V
    .locals 7
    :call_Test_Func_GetPlace_Attendant_interface_try_start
      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s-interface Attendant.GetPlace()='%s'\n"

      invoke-interface {p1}, LAttendant;->GetPlace()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_Test_Func_GetPlace_Attendant_interface_try_end
    .catch Ljava/lang/Error; {:call_Test_Func_GetPlace_Attendant_interface_try_start .. :call_Test_Func_GetPlace_Attendant_interface_try_end} :error_Test_Func_GetPlace_Attendant_interface_start
    :error_Test_Func_GetPlace_Attendant_interface_start
      move-exception v3
      const/4 v0, 1
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0, v1, v0
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v4, "%s-interface on Attendant: GetPlace() threw exception!\n"
      invoke-virtual {v2,v4,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void Test_Func_SayHi_Attendant_interface(String s, Attendant v) {
#     try {
#       System.out.printf("%s-interface Attendant.SayHi()='%s'\n", s, v.SayHi());
#       return;
#     } catch (Error e) {
#       System.out.printf("%s-interface on Attendant: SayHi() threw exception!\n", s);
#       e.printStackTrace(System.out);
#     }
#   }
.method public static Test_Func_SayHi_Attendant_interface(Ljava/lang/String;LAttendant;)V
    .locals 7
    :call_Test_Func_SayHi_Attendant_interface_try_start
      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s-interface Attendant.SayHi()='%s'\n"

      invoke-interface {p1}, LAttendant;->SayHi()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_Test_Func_SayHi_Attendant_interface_try_end
    .catch Ljava/lang/Error; {:call_Test_Func_SayHi_Attendant_interface_try_start .. :call_Test_Func_SayHi_Attendant_interface_try_end} :error_Test_Func_SayHi_Attendant_interface_start
    :error_Test_Func_SayHi_Attendant_interface_start
      move-exception v3
      const/4 v0, 1
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0, v1, v0
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v4, "%s-interface on Attendant: SayHi() threw exception!\n"
      invoke-virtual {v2,v4,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void Test_Func_GetName_D_virtual(String s, D v) {
#     try {
#       System.out.printf("%s-virtual           D.GetName()='%s'\n", s, v.GetName());
#       return;
#     } catch (Error e) {
#       System.out.printf("%s-virtual on D: GetName() threw exception!\n", s);
#       e.printStackTrace(System.out);
#     }
#   }
.method public static Test_Func_GetName_D_virtual(Ljava/lang/String;LD;)V
    .locals 7
    :call_Test_Func_GetName_D_virtual_try_start
      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s-virtual           D.GetName()='%s'\n"

      invoke-virtual {p1}, LD;->GetName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_Test_Func_GetName_D_virtual_try_end
    .catch Ljava/lang/Error; {:call_Test_Func_GetName_D_virtual_try_start .. :call_Test_Func_GetName_D_virtual_try_end} :error_Test_Func_GetName_D_virtual_start
    :error_Test_Func_GetName_D_virtual_start
      move-exception v3
      const/4 v0, 1
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0, v1, v0
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v4, "%s-virtual on D: GetName() threw exception!\n"
      invoke-virtual {v2,v4,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void Test_Func_SayHi_D_virtual(String s, D v) {
#     try {
#       System.out.printf("%s-virtual           D.SayHi()='%s'\n", s, v.SayHi());
#       return;
#     } catch (Error e) {
#       System.out.printf("%s-virtual on D: SayHi() threw exception!\n", s);
#       e.printStackTrace(System.out);
#     }
#   }
.method public static Test_Func_SayHi_D_virtual(Ljava/lang/String;LD;)V
    .locals 7
    :call_Test_Func_SayHi_D_virtual_try_start
      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s-virtual           D.SayHi()='%s'\n"

      invoke-virtual {p1}, LD;->SayHi()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_Test_Func_SayHi_D_virtual_try_end
    .catch Ljava/lang/Error; {:call_Test_Func_SayHi_D_virtual_try_start .. :call_Test_Func_SayHi_D_virtual_try_end} :error_Test_Func_SayHi_D_virtual_start
    :error_Test_Func_SayHi_D_virtual_start
      move-exception v3
      const/4 v0, 1
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0, v1, v0
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v4, "%s-virtual on D: SayHi() threw exception!\n"
      invoke-virtual {v2,v4,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void Test_Func_SayHiTwice_D_virtual(String s, D v) {
#     try {
#       System.out.printf("%s-virtual           D.SayHiTwice()='%s'\n", s, v.SayHiTwice());
#       return;
#     } catch (Error e) {
#       System.out.printf("%s-virtual on D: SayHiTwice() threw exception!\n", s);
#       e.printStackTrace(System.out);
#     }
#   }
.method public static Test_Func_SayHiTwice_D_virtual(Ljava/lang/String;LD;)V
    .locals 7
    :call_Test_Func_SayHiTwice_D_virtual_try_start
      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s-virtual           D.SayHiTwice()='%s'\n"

      invoke-virtual {p1}, LD;->SayHiTwice()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_Test_Func_SayHiTwice_D_virtual_try_end
    .catch Ljava/lang/Error; {:call_Test_Func_SayHiTwice_D_virtual_try_start .. :call_Test_Func_SayHiTwice_D_virtual_try_end} :error_Test_Func_SayHiTwice_D_virtual_start
    :error_Test_Func_SayHiTwice_D_virtual_start
      move-exception v3
      const/4 v0, 1
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0, v1, v0
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v4, "%s-virtual on D: SayHiTwice() threw exception!\n"
      invoke-virtual {v2,v4,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void Test_Func_SayHiTwice_Attendant_interface(String s, Attendant v) {
#     try {
#       System.out.printf("%s-interface Attendant.SayHiTwice()='%s'\n", s, v.SayHiTwice());
#       return;
#     } catch (Error e) {
#       System.out.printf("%s-interface on Attendant: SayHiTwice() threw exception!\n", s);
#       e.printStackTrace(System.out);
#     }
#   }
.method public static Test_Func_SayHiTwice_Attendant_interface(Ljava/lang/String;LAttendant;)V
    .locals 7
    :call_Test_Func_SayHiTwice_Attendant_interface_try_start
      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s-interface Attendant.SayHiTwice()='%s'\n"

      invoke-interface {p1}, LAttendant;->SayHiTwice()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_Test_Func_SayHiTwice_Attendant_interface_try_end
    .catch Ljava/lang/Error; {:call_Test_Func_SayHiTwice_Attendant_interface_try_start .. :call_Test_Func_SayHiTwice_Attendant_interface_try_end} :error_Test_Func_SayHiTwice_Attendant_interface_start
    :error_Test_Func_SayHiTwice_Attendant_interface_start
      move-exception v3
      const/4 v0, 1
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0, v1, v0
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v4, "%s-interface on Attendant: SayHiTwice() threw exception!\n"
      invoke-virtual {v2,v4,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void Test_Func_SayHi_G_virtual(String s, G v) {
#     try {
#       System.out.printf("%s-virtual           G.SayHi()='%s'\n", s, v.SayHi());
#       return;
#     } catch (Error e) {
#       System.out.printf("%s-virtual on G: SayHi() threw exception!\n", s);
#       e.printStackTrace(System.out);
#     }
#   }
.method public static Test_Func_SayHi_G_virtual(Ljava/lang/String;LG;)V
    .locals 7
    :call_Test_Func_SayHi_G_virtual_try_start
      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s-virtual           G.SayHi()='%s'\n"

      invoke-virtual {p1}, LG;->SayHi()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_Test_Func_SayHi_G_virtual_try_end
    .catch Ljava/lang/Error; {:call_Test_Func_SayHi_G_virtual_try_start .. :call_Test_Func_SayHi_G_virtual_try_end} :error_Test_Func_SayHi_G_virtual_start
    :error_Test_Func_SayHi_G_virtual_start
      move-exception v3
      const/4 v0, 1
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0, v1, v0
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v4, "%s-virtual on G: SayHi() threw exception!\n"
      invoke-virtual {v2,v4,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void Test_Func_SayHi_F_virtual(String s, F v) {
#     try {
#       System.out.printf("%s-virtual           F.SayHi()='%s'\n", s, v.SayHi());
#       return;
#     } catch (Error e) {
#       System.out.printf("%s-virtual on F: SayHi() threw exception!\n", s);
#       e.printStackTrace(System.out);
#     }
#   }
.method public static Test_Func_SayHi_F_virtual(Ljava/lang/String;LF;)V
    .locals 7
    :call_Test_Func_SayHi_F_virtual_try_start
      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s-virtual           F.SayHi()='%s'\n"

      invoke-virtual {p1}, LF;->SayHi()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_Test_Func_SayHi_F_virtual_try_end
    .catch Ljava/lang/Error; {:call_Test_Func_SayHi_F_virtual_try_start .. :call_Test_Func_SayHi_F_virtual_try_end} :error_Test_Func_SayHi_F_virtual_start
    :error_Test_Func_SayHi_F_virtual_start
      move-exception v3
      const/4 v0, 1
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0, v1, v0
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v4, "%s-virtual on F: SayHi() threw exception!\n"
      invoke-virtual {v2,v4,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void Test_Func_SayHiTwice_F_virtual(String s, F v) {
#     try {
#       System.out.printf("%s-virtual           F.SayHiTwice()='%s'\n", s, v.SayHiTwice());
#       return;
#     } catch (Error e) {
#       System.out.printf("%s-virtual on F: SayHiTwice() threw exception!\n", s);
#       e.printStackTrace(System.out);
#     }
#   }
.method public static Test_Func_SayHiTwice_F_virtual(Ljava/lang/String;LF;)V
    .locals 7
    :call_Test_Func_SayHiTwice_F_virtual_try_start
      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s-virtual           F.SayHiTwice()='%s'\n"

      invoke-virtual {p1}, LF;->SayHiTwice()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_Test_Func_SayHiTwice_F_virtual_try_end
    .catch Ljava/lang/Error; {:call_Test_Func_SayHiTwice_F_virtual_try_start .. :call_Test_Func_SayHiTwice_F_virtual_try_end} :error_Test_Func_SayHiTwice_F_virtual_start
    :error_Test_Func_SayHiTwice_F_virtual_start
      move-exception v3
      const/4 v0, 1
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0, v1, v0
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v4, "%s-virtual on F: SayHiTwice() threw exception!\n"
      invoke-virtual {v2,v4,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void Test_Func_GetPlace_F_virtual(String s, F v) {
#     try {
#       System.out.printf("%s-virtual           F.GetPlace()='%s'\n", s, v.GetPlace());
#       return;
#     } catch (Error e) {
#       System.out.printf("%s-virtual on F: GetPlace() threw exception!\n", s);
#       e.printStackTrace(System.out);
#     }
#   }
.method public static Test_Func_GetPlace_F_virtual(Ljava/lang/String;LF;)V
    .locals 7
    :call_Test_Func_GetPlace_F_virtual_try_start
      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s-virtual           F.GetPlace()='%s'\n"

      invoke-virtual {p1}, LF;->GetPlace()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_Test_Func_GetPlace_F_virtual_try_end
    .catch Ljava/lang/Error; {:call_Test_Func_GetPlace_F_virtual_try_start .. :call_Test_Func_GetPlace_F_virtual_try_end} :error_Test_Func_GetPlace_F_virtual_start
    :error_Test_Func_GetPlace_F_virtual_start
      move-exception v3
      const/4 v0, 1
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0, v1, v0
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v4, "%s-virtual on F: GetPlace() threw exception!\n"
      invoke-virtual {v2,v4,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void Test_Func_SayHiTwice_G_virtual(String s, G v) {
#     try {
#       System.out.printf("%s-virtual           G.SayHiTwice()='%s'\n", s, v.SayHiTwice());
#       return;
#     } catch (Error e) {
#       System.out.printf("%s-virtual on G: SayHiTwice() threw exception!\n", s);
#       e.printStackTrace(System.out);
#     }
#   }
.method public static Test_Func_SayHiTwice_G_virtual(Ljava/lang/String;LG;)V
    .locals 7
    :call_Test_Func_SayHiTwice_G_virtual_try_start
      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s-virtual           G.SayHiTwice()='%s'\n"

      invoke-virtual {p1}, LG;->SayHiTwice()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_Test_Func_SayHiTwice_G_virtual_try_end
    .catch Ljava/lang/Error; {:call_Test_Func_SayHiTwice_G_virtual_try_start .. :call_Test_Func_SayHiTwice_G_virtual_try_end} :error_Test_Func_SayHiTwice_G_virtual_start
    :error_Test_Func_SayHiTwice_G_virtual_start
      move-exception v3
      const/4 v0, 1
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0, v1, v0
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v4, "%s-virtual on G: SayHiTwice() threw exception!\n"
      invoke-virtual {v2,v4,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void Test_Func_GetPlace_G_virtual(String s, G v) {
#     try {
#       System.out.printf("%s-virtual           G.GetPlace()='%s'\n", s, v.GetPlace());
#       return;
#     } catch (Error e) {
#       System.out.printf("%s-virtual on G: GetPlace() threw exception!\n", s);
#       e.printStackTrace(System.out);
#     }
#   }
.method public static Test_Func_GetPlace_G_virtual(Ljava/lang/String;LG;)V
    .locals 7
    :call_Test_Func_GetPlace_G_virtual_try_start
      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s-virtual           G.GetPlace()='%s'\n"

      invoke-virtual {p1}, LG;->GetPlace()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_Test_Func_GetPlace_G_virtual_try_end
    .catch Ljava/lang/Error; {:call_Test_Func_GetPlace_G_virtual_try_start .. :call_Test_Func_GetPlace_G_virtual_try_end} :error_Test_Func_GetPlace_G_virtual_start
    :error_Test_Func_GetPlace_G_virtual_start
      move-exception v3
      const/4 v0, 1
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0, v1, v0
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v4, "%s-virtual on G: GetPlace() threw exception!\n"
      invoke-virtual {v2,v4,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void Test_Func_SayHi_E_virtual(String s, E v) {
#     try {
#       System.out.printf("%s-virtual           E.SayHi()='%s'\n", s, v.SayHi());
#       return;
#     } catch (Error e) {
#       System.out.printf("%s-virtual on E: SayHi() threw exception!\n", s);
#       e.printStackTrace(System.out);
#     }
#   }
.method public static Test_Func_SayHi_E_virtual(Ljava/lang/String;LE;)V
    .locals 7
    :call_Test_Func_SayHi_E_virtual_try_start
      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s-virtual           E.SayHi()='%s'\n"

      invoke-virtual {p1}, LE;->SayHi()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_Test_Func_SayHi_E_virtual_try_end
    .catch Ljava/lang/Error; {:call_Test_Func_SayHi_E_virtual_try_start .. :call_Test_Func_SayHi_E_virtual_try_end} :error_Test_Func_SayHi_E_virtual_start
    :error_Test_Func_SayHi_E_virtual_start
      move-exception v3
      const/4 v0, 1
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0, v1, v0
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v4, "%s-virtual on E: SayHi() threw exception!\n"
      invoke-virtual {v2,v4,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void Test_Func_SayHiTwice_E_virtual(String s, E v) {
#     try {
#       System.out.printf("%s-virtual           E.SayHiTwice()='%s'\n", s, v.SayHiTwice());
#       return;
#     } catch (Error e) {
#       System.out.printf("%s-virtual on E: SayHiTwice() threw exception!\n", s);
#       e.printStackTrace(System.out);
#     }
#   }
.method public static Test_Func_SayHiTwice_E_virtual(Ljava/lang/String;LE;)V
    .locals 7
    :call_Test_Func_SayHiTwice_E_virtual_try_start
      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s-virtual           E.SayHiTwice()='%s'\n"

      invoke-virtual {p1}, LE;->SayHiTwice()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_Test_Func_SayHiTwice_E_virtual_try_end
    .catch Ljava/lang/Error; {:call_Test_Func_SayHiTwice_E_virtual_try_start .. :call_Test_Func_SayHiTwice_E_virtual_try_end} :error_Test_Func_SayHiTwice_E_virtual_start
    :error_Test_Func_SayHiTwice_E_virtual_start
      move-exception v3
      const/4 v0, 1
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0, v1, v0
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v4, "%s-virtual on E: SayHiTwice() threw exception!\n"
      invoke-virtual {v2,v4,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void Test_Func_GetName_Greeter3_interface(String s, Greeter3 v) {
#     try {
#       System.out.printf("%s-interface  Greeter3.GetName()='%s'\n", s, v.GetName());
#       return;
#     } catch (Error e) {
#       System.out.printf("%s-interface on Greeter3: GetName() threw exception!\n", s);
#       e.printStackTrace(System.out);
#     }
#   }
.method public static Test_Func_GetName_Greeter3_interface(Ljava/lang/String;LGreeter3;)V
    .locals 7
    :call_Test_Func_GetName_Greeter3_interface_try_start
      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s-interface  Greeter3.GetName()='%s'\n"

      invoke-interface {p1}, LGreeter3;->GetName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_Test_Func_GetName_Greeter3_interface_try_end
    .catch Ljava/lang/Error; {:call_Test_Func_GetName_Greeter3_interface_try_start .. :call_Test_Func_GetName_Greeter3_interface_try_end} :error_Test_Func_GetName_Greeter3_interface_start
    :error_Test_Func_GetName_Greeter3_interface_start
      move-exception v3
      const/4 v0, 1
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0, v1, v0
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v4, "%s-interface on Greeter3: GetName() threw exception!\n"
      invoke-virtual {v2,v4,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void Test_Func_SayHi_Greeter3_interface(String s, Greeter3 v) {
#     try {
#       System.out.printf("%s-interface  Greeter3.SayHi()='%s'\n", s, v.SayHi());
#       return;
#     } catch (Error e) {
#       System.out.printf("%s-interface on Greeter3: SayHi() threw exception!\n", s);
#       e.printStackTrace(System.out);
#     }
#   }
.method public static Test_Func_SayHi_Greeter3_interface(Ljava/lang/String;LGreeter3;)V
    .locals 7
    :call_Test_Func_SayHi_Greeter3_interface_try_start
      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s-interface  Greeter3.SayHi()='%s'\n"

      invoke-interface {p1}, LGreeter3;->SayHi()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_Test_Func_SayHi_Greeter3_interface_try_end
    .catch Ljava/lang/Error; {:call_Test_Func_SayHi_Greeter3_interface_try_start .. :call_Test_Func_SayHi_Greeter3_interface_try_end} :error_Test_Func_SayHi_Greeter3_interface_start
    :error_Test_Func_SayHi_Greeter3_interface_start
      move-exception v3
      const/4 v0, 1
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0, v1, v0
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v4, "%s-interface on Greeter3: SayHi() threw exception!\n"
      invoke-virtual {v2,v4,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void Test_Func_SayHiTwice_Greeter3_interface(String s, Greeter3 v) {
#     try {
#       System.out.printf("%s-interface  Greeter3.SayHiTwice()='%s'\n", s, v.SayHiTwice());
#       return;
#     } catch (Error e) {
#       System.out.printf("%s-interface on Greeter3: SayHiTwice() threw exception!\n", s);
#       e.printStackTrace(System.out);
#     }
#   }
.method public static Test_Func_SayHiTwice_Greeter3_interface(Ljava/lang/String;LGreeter3;)V
    .locals 7
    :call_Test_Func_SayHiTwice_Greeter3_interface_try_start
      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s-interface  Greeter3.SayHiTwice()='%s'\n"

      invoke-interface {p1}, LGreeter3;->SayHiTwice()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_Test_Func_SayHiTwice_Greeter3_interface_try_end
    .catch Ljava/lang/Error; {:call_Test_Func_SayHiTwice_Greeter3_interface_try_start .. :call_Test_Func_SayHiTwice_Greeter3_interface_try_end} :error_Test_Func_SayHiTwice_Greeter3_interface_start
    :error_Test_Func_SayHiTwice_Greeter3_interface_start
      move-exception v3
      const/4 v0, 1
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0, v1, v0
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v4, "%s-interface on Greeter3: SayHiTwice() threw exception!\n"
      invoke-virtual {v2,v4,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void Test_Func_SayHi_Greeter_interface(String s, Greeter v) {
#     try {
#       System.out.printf("%s-interface   Greeter.SayHi()='%s'\n", s, v.SayHi());
#       return;
#     } catch (Error e) {
#       System.out.printf("%s-interface on Greeter: SayHi() threw exception!\n", s);
#       e.printStackTrace(System.out);
#     }
#   }
.method public static Test_Func_SayHi_Greeter_interface(Ljava/lang/String;LGreeter;)V
    .locals 7
    :call_Test_Func_SayHi_Greeter_interface_try_start
      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s-interface   Greeter.SayHi()='%s'\n"

      invoke-interface {p1}, LGreeter;->SayHi()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_Test_Func_SayHi_Greeter_interface_try_end
    .catch Ljava/lang/Error; {:call_Test_Func_SayHi_Greeter_interface_try_start .. :call_Test_Func_SayHi_Greeter_interface_try_end} :error_Test_Func_SayHi_Greeter_interface_start
    :error_Test_Func_SayHi_Greeter_interface_start
      move-exception v3
      const/4 v0, 1
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0, v1, v0
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v4, "%s-interface on Greeter: SayHi() threw exception!\n"
      invoke-virtual {v2,v4,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void Test_Func_SayHiTwice_Greeter_interface(String s, Greeter v) {
#     try {
#       System.out.printf("%s-interface   Greeter.SayHiTwice()='%s'\n", s, v.SayHiTwice());
#       return;
#     } catch (Error e) {
#       System.out.printf("%s-interface on Greeter: SayHiTwice() threw exception!\n", s);
#       e.printStackTrace(System.out);
#     }
#   }
.method public static Test_Func_SayHiTwice_Greeter_interface(Ljava/lang/String;LGreeter;)V
    .locals 7
    :call_Test_Func_SayHiTwice_Greeter_interface_try_start
      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s-interface   Greeter.SayHiTwice()='%s'\n"

      invoke-interface {p1}, LGreeter;->SayHiTwice()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_Test_Func_SayHiTwice_Greeter_interface_try_end
    .catch Ljava/lang/Error; {:call_Test_Func_SayHiTwice_Greeter_interface_try_start .. :call_Test_Func_SayHiTwice_Greeter_interface_try_end} :error_Test_Func_SayHiTwice_Greeter_interface_start
    :error_Test_Func_SayHiTwice_Greeter_interface_start
      move-exception v3
      const/4 v0, 1
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0, v1, v0
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v4, "%s-interface on Greeter: SayHiTwice() threw exception!\n"
      invoke-virtual {v2,v4,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void Test_Func_SayHi_Greeter2_interface(String s, Greeter2 v) {
#     try {
#       System.out.printf("%s-interface  Greeter2.SayHi()='%s'\n", s, v.SayHi());
#       return;
#     } catch (Error e) {
#       System.out.printf("%s-interface on Greeter2: SayHi() threw exception!\n", s);
#       e.printStackTrace(System.out);
#     }
#   }
.method public static Test_Func_SayHi_Greeter2_interface(Ljava/lang/String;LGreeter2;)V
    .locals 7
    :call_Test_Func_SayHi_Greeter2_interface_try_start
      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s-interface  Greeter2.SayHi()='%s'\n"

      invoke-interface {p1}, LGreeter2;->SayHi()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_Test_Func_SayHi_Greeter2_interface_try_end
    .catch Ljava/lang/Error; {:call_Test_Func_SayHi_Greeter2_interface_try_start .. :call_Test_Func_SayHi_Greeter2_interface_try_end} :error_Test_Func_SayHi_Greeter2_interface_start
    :error_Test_Func_SayHi_Greeter2_interface_start
      move-exception v3
      const/4 v0, 1
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0, v1, v0
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v4, "%s-interface on Greeter2: SayHi() threw exception!\n"
      invoke-virtual {v2,v4,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void Test_Func_SayHiTwice_Greeter2_interface(String s, Greeter2 v) {
#     try {
#       System.out.printf("%s-interface  Greeter2.SayHiTwice()='%s'\n", s, v.SayHiTwice());
#       return;
#     } catch (Error e) {
#       System.out.printf("%s-interface on Greeter2: SayHiTwice() threw exception!\n", s);
#       e.printStackTrace(System.out);
#     }
#   }
.method public static Test_Func_SayHiTwice_Greeter2_interface(Ljava/lang/String;LGreeter2;)V
    .locals 7
    :call_Test_Func_SayHiTwice_Greeter2_interface_try_start
      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s-interface  Greeter2.SayHiTwice()='%s'\n"

      invoke-interface {p1}, LGreeter2;->SayHiTwice()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_Test_Func_SayHiTwice_Greeter2_interface_try_end
    .catch Ljava/lang/Error; {:call_Test_Func_SayHiTwice_Greeter2_interface_try_start .. :call_Test_Func_SayHiTwice_Greeter2_interface_try_end} :error_Test_Func_SayHiTwice_Greeter2_interface_start
    :error_Test_Func_SayHiTwice_Greeter2_interface_start
      move-exception v3
      const/4 v0, 1
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0, v1, v0
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v4, "%s-interface on Greeter2: SayHiTwice() threw exception!\n"
      invoke-virtual {v2,v4,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void Test_Func_SayHi_B_virtual(String s, B v) {
#     try {
#       System.out.printf("%s-virtual           B.SayHi()='%s'\n", s, v.SayHi());
#       return;
#     } catch (Error e) {
#       System.out.printf("%s-virtual on B: SayHi() threw exception!\n", s);
#       e.printStackTrace(System.out);
#     }
#   }
.method public static Test_Func_SayHi_B_virtual(Ljava/lang/String;LB;)V
    .locals 7
    :call_Test_Func_SayHi_B_virtual_try_start
      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s-virtual           B.SayHi()='%s'\n"

      invoke-virtual {p1}, LB;->SayHi()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_Test_Func_SayHi_B_virtual_try_end
    .catch Ljava/lang/Error; {:call_Test_Func_SayHi_B_virtual_try_start .. :call_Test_Func_SayHi_B_virtual_try_end} :error_Test_Func_SayHi_B_virtual_start
    :error_Test_Func_SayHi_B_virtual_start
      move-exception v3
      const/4 v0, 1
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0, v1, v0
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v4, "%s-virtual on B: SayHi() threw exception!\n"
      invoke-virtual {v2,v4,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method

#   public static void Test_Func_SayHiTwice_B_virtual(String s, B v) {
#     try {
#       System.out.printf("%s-virtual           B.SayHiTwice()='%s'\n", s, v.SayHiTwice());
#       return;
#     } catch (Error e) {
#       System.out.printf("%s-virtual on B: SayHiTwice() threw exception!\n", s);
#       e.printStackTrace(System.out);
#     }
#   }
.method public static Test_Func_SayHiTwice_B_virtual(Ljava/lang/String;LB;)V
    .locals 7
    :call_Test_Func_SayHiTwice_B_virtual_try_start
      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s-virtual           B.SayHiTwice()='%s'\n"

      invoke-virtual {p1}, LB;->SayHiTwice()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {v2,v3,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_Test_Func_SayHiTwice_B_virtual_try_end
    .catch Ljava/lang/Error; {:call_Test_Func_SayHiTwice_B_virtual_try_start .. :call_Test_Func_SayHiTwice_B_virtual_try_end} :error_Test_Func_SayHiTwice_B_virtual_start
    :error_Test_Func_SayHiTwice_B_virtual_start
      move-exception v3
      const/4 v0, 1
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0, v1, v0
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v4, "%s-virtual on B: SayHiTwice() threw exception!\n"
      invoke-virtual {v2,v4,v1}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      invoke-virtual {v3,v2}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method



#   public static void main(String[] args) {
.method public static main([Ljava/lang/String;)V
    .locals 2
    sget-object v1, Ljava/lang/System;->out:Ljava/io/PrintStream;

    
#     System.out.println("Beginning repetition 0");
    const-string v0, "Beginning repetition 0"
    invoke-virtual {v1,v0}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V

#     TEST_NAME_A();
    invoke-static {}, TEST_NAME_A()V

#     TEST_NAME_B();
    invoke-static {}, TEST_NAME_B()V

#     TEST_NAME_C();
    invoke-static {}, TEST_NAME_C()V

#     TEST_NAME_D();
    invoke-static {}, TEST_NAME_D()V

#     TEST_NAME_E();
    invoke-static {}, TEST_NAME_E()V

#     TEST_NAME_F();
    invoke-static {}, TEST_NAME_F()V

#     TEST_NAME_G();
    invoke-static {}, TEST_NAME_G()V

#     System.out.println("Beginning repetition 1");
    const-string v0, "Beginning repetition 1"
    invoke-virtual {v1,v0}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V

#     TEST_NAME_A();
    invoke-static {}, TEST_NAME_A()V

#     TEST_NAME_B();
    invoke-static {}, TEST_NAME_B()V

#     TEST_NAME_C();
    invoke-static {}, TEST_NAME_C()V

#     TEST_NAME_D();
    invoke-static {}, TEST_NAME_D()V

#     TEST_NAME_E();
    invoke-static {}, TEST_NAME_E()V

#     TEST_NAME_F();
    invoke-static {}, TEST_NAME_F()V

#     TEST_NAME_G();
    invoke-static {}, TEST_NAME_G()V

#     System.out.println("Beginning repetition 2");
    const-string v0, "Beginning repetition 2"
    invoke-virtual {v1,v0}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V

#     TEST_NAME_A();
    invoke-static {}, TEST_NAME_A()V

#     TEST_NAME_B();
    invoke-static {}, TEST_NAME_B()V

#     TEST_NAME_C();
    invoke-static {}, TEST_NAME_C()V

#     TEST_NAME_D();
    invoke-static {}, TEST_NAME_D()V

#     TEST_NAME_E();
    invoke-static {}, TEST_NAME_E()V

#     TEST_NAME_F();
    invoke-static {}, TEST_NAME_F()V

#     TEST_NAME_G();
    invoke-static {}, TEST_NAME_G()V

#     System.out.println("Beginning repetition 3");
    const-string v0, "Beginning repetition 3"
    invoke-virtual {v1,v0}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V

#     TEST_NAME_A();
    invoke-static {}, TEST_NAME_A()V

#     TEST_NAME_B();
    invoke-static {}, TEST_NAME_B()V

#     TEST_NAME_C();
    invoke-static {}, TEST_NAME_C()V

#     TEST_NAME_D();
    invoke-static {}, TEST_NAME_D()V

#     TEST_NAME_E();
    invoke-static {}, TEST_NAME_E()V

#     TEST_NAME_F();
    invoke-static {}, TEST_NAME_F()V

#     TEST_NAME_G();
    invoke-static {}, TEST_NAME_G()V


    return-void
.end method
#   }


# }

