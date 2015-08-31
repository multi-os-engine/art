.class public abstract interface LGreeter3;
.super Ljava/lang/Object;
.implements LGreeter;

# public interface Greeter3 extends Greeter {
#     public String GetName();
#
#     public default String SayHi() {
#         return "Hello " + GetName();
#     }
# }

.method public abstract GetName()Ljava/lang/String;
.end method

.method public SayHi()Ljava/lang/String;
    .locals 2
    const-string v0, "Hello "
    invoke-interface {p0}, LGreeter3;->GetName()Ljava/lang/String;
    move-result-object v1
    invoke-virtual {v0, v1}, Ljava/lang/String;->concat(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v0
    return-object v0
.end method
