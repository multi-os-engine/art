.class public LMain;
.super Ljava/lang/Object;

# public class Main {
#     public static void main(String[] args) {
#         A a = new A();
#         PrintCalls("a", a);
#         System.out.println(a.SayHiTwice())
#
#         B b = new B();
#         PrintCalls("b", b);
#
#         C c = new C();
#         PrintCalls("c", c);
#
#         D d = new D();
#         PrintCalls("d", d);
#
#         E e = new E();
#         PrintCalls("e", e);
#     }
#
#     public static void PrintCalls(String name, Greeter g) {
#         String once = g.SayHi();
#         String twice = g.SayHiTwice();
#         System.out.printf("%s: SayHi()='%s', SayHiTwice()='%s'", name, once, twice);
#     }
# }

.method public constructor <init>()V
    .registers 1
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    return-void
.end method

.method public static PrintCalls(Ljava/lang/String;LGreeter;)V
    .locals 7
    const/4 v0, 3
    new-array v1,v0, [Ljava/lang/Object;
    const/4 v0, 0
    aput-object p0,v1,v0

    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v3, "%s: SayHi()='%s', SayHiTwice()='%s'"

    invoke-interface {p1}, LGreeter;->SayHi()Ljava/lang/String;
    move-result-object v4
    const/4 v0, 1
    aput-object v4, v1, v0

    invoke-interface {p1}, LGreeter;->SayHiTwice()Ljava/lang/String;
    move-result-object v5
    const/4 v0, 2
    aput-object v5, v1, v0

    invoke-static {v3,v1}, Ljava/lang/String;->format(Ljava/lang/String;[Ljava/lang/Object;)Ljava/lang/String;
    move-result-object v6

    invoke-virtual {v2,v6}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V

    return-void
.end method

.method public static main([Ljava/lang/String;)V
    .locals 2

    const-string v0, "A"
    new-instance v1, LA;
    invoke-direct {v1}, LA;-><init>()V
    invoke-static {v0, v1}, PrintCalls(Ljava/lang/String;LGreeter;)V

    const-string v0, "B"
    new-instance v1, LB;
    invoke-direct {v1}, LB;-><init>()V
    invoke-static {v0, v1}, PrintCalls(Ljava/lang/String;LGreeter;)V

    const-string v0, "C"
    new-instance v1, LC;
    invoke-direct {v1}, LC;-><init>()V
    invoke-static {v0, v1}, PrintCalls(Ljava/lang/String;LGreeter;)V

    const-string v0, "D"
    new-instance v1, LD;
    invoke-direct {v1}, LD;-><init>()V
    invoke-static {v0, v1}, PrintCalls(Ljava/lang/String;LGreeter;)V

    const-string v0, "E"
    new-instance v1, LE;
    invoke-direct {v1}, LE;-><init>()V
    invoke-static {v0, v1}, PrintCalls(Ljava/lang/String;LGreeter;)V

    return-void
.end method
