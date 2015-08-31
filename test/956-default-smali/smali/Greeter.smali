.class public abstract interface LGreeter;
.super Ljava/lang/Object;

# public interface Greeter {
#     public String SayHi();
#
#     public default String SayHiTwice() {
#         return SayHi() + SayHi();
#     }
# }

.method public abstract SayHi()Ljava/lang/String;
.end method

.method public SayHiTwice()Ljava/lang/String;
    .locals 2
    invoke-interface {p0}, LGreeter;->SayHi()Ljava/lang/String;
    move-result-object v0
    invoke-interface {p0}, LGreeter;->SayHi()Ljava/lang/String;
    move-result-object v1
    invoke-virtual {v0, v1}, Ljava/lang/String;->concat(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v0
    return-object v0
.end method
