.class public abstract interface LGreeter2;
.super Ljava/lang/Object;
.implements LGreeter;

# public interface Greeter2 extends Greeter {
#     public default String SayHiTwice() {
#         return "I say " + SayHi() + SayHi();
#     }
# }

.method public SayHiTwice()Ljava/lang/String;
    .locals 3
    const-string v0, "I say "
    invoke-interface {p0}, LGreeter;->SayHi()Ljava/lang/String;
    move-result-object v1
    invoke-virtual {v0, v1}, Ljava/lang/String;->concat(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v0
    invoke-interface {p0}, LGreeter;->SayHi()Ljava/lang/String;
    move-result-object v1
    invoke-virtual {v0, v1}, Ljava/lang/String;->concat(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v0
    return-object v0
.end method
